import re
import threading
from queue import Queue

import serial

_CAN_RE = re.compile(r'^CAN\s+[0-9A-Fa-f]+\s+\d+(?:\s+[0-9A-Fa-f]{2})+\s*$')

def is_can_frame(line: str) -> bool:
    return bool(_CAN_RE.match(line.strip()))


class SerialBridge:

    def __init__(self, port: str, baud: int):
        self._port    = port
        self._baud    = baud
        self._serial: serial.Serial | None = None
        self._vecu    = None
        self._running = False

        self.esp_log_queue:  Queue[str | None] = Queue()
        self.vecu_log_queue: Queue[str | None] = Queue()

        self._threads: list[threading.Thread] = []

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def start(self, vecu_process) -> None:
        self._vecu    = vecu_process
        self._serial  = serial.Serial(self._port, self._baud, timeout=1.0)
        self._serial.reset_input_buffer()  # discard leftover bytes from previous session
        self._running = True

        targets = [self._esp_to_vecu]
        if self._vecu is not None:
            targets += [self._vecu_stdout_to_esp, self._vecu_stderr_reader]

        for target in targets:
            t = threading.Thread(target=target, daemon=True)
            t.start()
            self._threads.append(t)

    def stop(self) -> None:
        self._running = False
        if self._serial and self._serial.is_open:
            self._serial.close()
        self.esp_log_queue.put(None)

    # ------------------------------------------------------------------
    # Public write helper (used by test runner for HELLO/READY)
    # ------------------------------------------------------------------

    def send_to_esp(self, data: str) -> None:
        if self._serial and self._serial.is_open:
            self._serial.write(data.encode("utf-8"))

    # ------------------------------------------------------------------
    # Internal threads
    # ------------------------------------------------------------------

    def _esp_to_vecu(self) -> None:
        """Read from ESP serial -> route CAN frames to vECU stdin, rest to queue."""
        while self._running:
            try:
                raw = self._serial.readline()
            except Exception:
                break
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace").strip()
            if not line:
                continue

            if is_can_frame(line):
                if self._vecu is not None:
                    try:
                        self._vecu.stdin.write(line + "\n")
                        self._vecu.stdin.flush()
                    except Exception:
                        pass
            else:
                self.esp_log_queue.put(line)

        self.esp_log_queue.put(None)  # signal EOF to consumer

    def _vecu_stdout_to_esp(self) -> None:
        """Read vECU stdout -> forward CAN frames to ESP, non-CAN to log queue."""
        try:
            for raw_line in self._vecu.stdout:
                line = raw_line.rstrip("\r\n")
                if not line:
                    continue
                if is_can_frame(line):
                    try:
                        if self._serial and self._serial.is_open:
                            self._serial.write((line + "\n").encode("utf-8"))
                    except Exception:
                        pass
                else:
                    self.vecu_log_queue.put(line)
        except Exception:
            pass

        self.vecu_log_queue.put(None)  # signal EOF to consumer

    def _vecu_stderr_reader(self) -> None:
        """Read vECU stderr -> forward all lines to log queue."""
        try:
            for raw_line in self._vecu.stderr:
                line = raw_line.rstrip("\r\n")
                if line:
                    self.vecu_log_queue.put(line)
        except Exception:
            pass
