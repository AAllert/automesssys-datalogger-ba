"""
UART transport: text-format CAN frames over a serial port.
"""

import time
from typing import Optional

import serial

from src.communication.transport import CanFrame, TextCanTransport


class UartTransport(TextCanTransport):
    """
    Text-format CAN transport over a serial (UART) port.

    Non-CAN lines received from the port are forwarded to on_unknown_line so
    the caller can handle log lines or handshake messages (e.g. "HELLO").
    """

    def __init__(self, port: str, baud_rate: int = 115200) -> None:
        super().__init__()
        self._serial = serial.Serial(port=port, baudrate=baud_rate, timeout=1.0)
        time.sleep(2)  # let hardware initialise after opening the port
        print(f"[OK] UART connected on {port} @ {baud_rate} baud")

    @property
    def in_waiting(self) -> int:
        return self._serial.in_waiting

    def read_frame(self) -> Optional[CanFrame]:
        """Non-blocking: read one line if data is available, return CAN frame or None."""
        if self._serial.in_waiting <= 0:
            return None
        raw = self._serial.readline()
        if not raw:
            return None
        line = raw.decode("utf-8", errors="replace").strip()
        return self._dispatch(line)

    def read_frame_blocking(self, timeout_s: float) -> Optional[CanFrame]:
        """
        Block up to timeout_s waiting for a CAN frame.
        Non-CAN lines are forwarded to on_unknown_line and skipped.
        """
        import time
        deadline = time.time() + timeout_s
        while time.time() < deadline:
            raw = self._serial.readline()  # blocks up to serial.timeout (1.0 s)
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace").strip()
            if not line:
                continue
            frame = self._dispatch(line)
            if frame is not None:
                return frame
        return None

    def write_frame(self, frame: CanFrame) -> None:
        self._serial.write(self._format_frame(frame).encode("utf-8"))
        self.frames_sent += 1

    def close(self) -> None:
        if self._serial.is_open:
            self._serial.close()
            print(
                f"[OK] UART disconnected "
                f"({self.frames_received} RX, {self.frames_sent} TX)"
            )
