"""
CAN transport abstractions.

Provides a common CanFrame type and Transport interface so that ISO-TP handling
and UDS processing are independent of the physical medium (CAN bus, UART, stdin).
"""

import re
import sys
import threading
import time
from abc import ABC, abstractmethod
from dataclasses import dataclass
from queue import Empty, Queue
from typing import Callable, Optional


@dataclass
class CanFrame:
    can_id: int
    data: bytes


class Transport(ABC):
    """Abstract transport — reads and writes CanFrames."""

    @property
    @abstractmethod
    def in_waiting(self) -> int:
        """Number of CAN frames available for immediate reading."""

    @abstractmethod
    def read_frame(self) -> Optional[CanFrame]:
        """Non-blocking: return next available CAN frame, or None."""

    @abstractmethod
    def read_frame_blocking(self, timeout_s: float) -> Optional[CanFrame]:
        """Blocking: wait up to timeout_s for a CAN frame, return None on timeout."""

    @abstractmethod
    def write_frame(self, frame: CanFrame) -> None: ...

    @abstractmethod
    def close(self) -> None: ...


class TextCanTransport(Transport):
    """
    Shared base for text-format CAN transports (UART and stdin).
    Wire format: "CAN <hex_id> <dlc> <data_bytes>\\n"
    Example:     "CAN 7DF 3 22 2C 96\\n"
    """

    _PATTERN = re.compile(
        r'^CAN\s+([0-9A-Fa-f]+)\s+(\d+)\s+((?:[0-9A-Fa-f]{2}\s*)+)$'
    )

    def __init__(self) -> None:
        self.on_unknown_line: Optional[Callable[[str], None]] = None
        self.frames_received: int = 0
        self.frames_sent: int = 0

    def _parse_line(self, line: str) -> Optional[CanFrame]:
        m = self._PATTERN.match(line.strip())
        if not m:
            return None
        can_id = int(m.group(1), 16)
        dlc = int(m.group(2))
        data = bytes.fromhex(m.group(3).replace(" ", ""))[:dlc]
        return CanFrame(can_id, data)

    def _format_frame(self, frame: CanFrame) -> str:
        dlc = len(frame.data)
        padded = frame.data + bytes(max(0, 8 - dlc))
        hex_str = " ".join(f"{b:02X}" for b in padded)
        return f"CAN {frame.can_id:03X} {dlc} {hex_str}\n"

    def _dispatch(self, line: str) -> Optional[CanFrame]:
        """Parse a line; fire on_unknown_line callback for non-CAN content."""
        frame = self._parse_line(line)
        if frame is not None:
            self.frames_received += 1
            return frame
        if line and self.on_unknown_line:
            self.on_unknown_line(line)
        return None


class StdinTransport(TextCanTransport):
    """
    Text-format CAN transport over stdin/stdout.
    Reads CAN frames from stdin in a background thread.
    Writes CAN responses to _can_out (the original stdout, saved before redirection).
    """

    def __init__(self, can_out=None) -> None:
        super().__init__()
        self._can_out = can_out or sys.stdout
        self._queue: Queue[bytes] = Queue()
        t = threading.Thread(target=self._reader, daemon=True)
        t.start()

    def _reader(self) -> None:
        for line in sys.stdin:
            self._queue.put(line.encode("utf-8"))
        self._queue.put(b"")  # EOF sentinel

    @property
    def in_waiting(self) -> int:
        return self._queue.qsize()

    def _dequeue(self) -> Optional[str]:
        try:
            raw = self._queue.get_nowait()
        except Empty:
            return None
        if not raw:
            return None
        return raw.decode("utf-8", errors="replace").strip()

    def read_frame(self) -> Optional[CanFrame]:
        line = self._dequeue()
        if line is None:
            return None
        return self._dispatch(line)

    def read_frame_blocking(self, timeout_s: float) -> Optional[CanFrame]:
        deadline = time.time() + timeout_s
        while time.time() < deadline:
            remaining = max(0.0, deadline - time.time())
            try:
                raw = self._queue.get(timeout=min(0.05, remaining))
            except Empty:
                continue
            if not raw:
                return None
            line = raw.decode("utf-8", errors="replace").strip()
            frame = self._dispatch(line)
            if frame is not None:
                return frame
        return None

    def write_frame(self, frame: CanFrame) -> None:
        self._can_out.write(self._format_frame(frame))
        self._can_out.flush()
        self.frames_sent += 1

    def close(self) -> None:
        pass


class NativeCanTransport(Transport):
    """
    CAN transport using python-can (socketcan on Linux, virtual bus elsewhere).
    python-can is imported lazily so that UART/stdin modes don't require it.
    """

    def __init__(self, config) -> None:
        import can
        import platform

        if platform.system() == "Linux":
            interface, channel = "socketcan", "vcan0"
        else:
            interface, channel = "virtual", "vecu-dev"

        self.interface_name = interface
        self.channel_name = channel
        self.backend_description = f"{interface}:{channel}"

        bus_kwargs: dict = {"channel": channel, "bustype": interface}
        if interface == "virtual":
            bus_kwargs["receive_own_messages"] = True
        self._bus = can.interface.Bus(**bus_kwargs)

        self._valid_rx = {rxid for rxid, _ in config.get_unique_isotp_addresses()}
        self._queue: Queue[CanFrame] = Queue()
        self.frames_received: int = 0
        self.frames_sent: int = 0

    def _to_frame(self, msg) -> Optional[CanFrame]:
        if msg and msg.arbitration_id in self._valid_rx:
            self.frames_received += 1
            return CanFrame(msg.arbitration_id, bytes(msg.data))
        return None

    def _fill_queue(self) -> None:
        """Drain all immediately available bus messages into the internal queue."""
        while True:
            msg = self._bus.recv(timeout=0)
            if msg is None:
                break
            frame = self._to_frame(msg)
            if frame is not None:
                self._queue.put(frame)

    @property
    def in_waiting(self) -> int:
        self._fill_queue()
        return self._queue.qsize()

    def read_frame(self) -> Optional[CanFrame]:
        self._fill_queue()
        try:
            return self._queue.get_nowait()
        except Empty:
            return None

    def read_frame_blocking(self, timeout_s: float) -> Optional[CanFrame]:
        # Drain the queue first in case in_waiting was called before us.
        try:
            return self._queue.get_nowait()
        except Empty:
            pass
        deadline = time.time() + timeout_s
        while time.time() < deadline:
            msg = self._bus.recv(timeout=min(0.1, deadline - time.time()))
            frame = self._to_frame(msg)
            if frame is not None:
                return frame
        return None

    def write_frame(self, frame: CanFrame) -> None:
        import can
        msg = can.Message(
            arbitration_id=frame.can_id,
            data=frame.data,
            is_extended_id=frame.can_id > 0x7FF,
        )
        self._bus.send(msg)
        self.frames_sent += 1

    def close(self) -> None:
        self._bus.shutdown()
