import json
import os
import socket
import subprocess
import threading
import time

from . import output_formatter as fmt

_TELNET_HOST = "localhost"
_TELNET_PORT = 4444
_OPENOCD_BOARD_CFG = "board/esp32c6-builtin.cfg"

# Matches this project's ESP32-C6 (RISC-V) toolchain.
_GCOV_EXECUTABLE = "riscv32-esp-elf-gcov"


class GcovRunner:
    def __init__(self, project_dir: str, build_dir: str):
        self.project_dir = project_dir
        self.build_dir = build_dir
        self.files = []  # per-file breakdown, filled in by build_report()
        self._proc: subprocess.Popen = None

    def start(self, timeout: float = 20.0) -> None:
        fmt.phase("COVERAGE", "Starte OpenOCD ...")
        self._proc = subprocess.Popen(
            ["openocd", "-f", _OPENOCD_BOARD_CFG],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            encoding="utf-8",
            text=True,
            shell=True,
            bufsize=1,
        )
        threading.Thread(target=self._pump_log, daemon=True).start()

        # Resetting is necessary, because on ESP32-C6 OpenOCD must be attached during boot to get max trace block size
        self._wait_ready(timeout)
        fmt.phase("COVERAGE", "OpenOCD bereit")

    def dump(self, timeout: float = 180.0) -> None:
        """Sends 'esp gcov dump' over OpenOCD's telnet console.
        """
        self._clean_gcda()
        fmt.phase("COVERAGE", "Sende 'esp gcov dump' an OpenOCD ...")

        response = self._send_command("esp gcov dump", grace=timeout, idle=5.0, until="Targets disconnected")
        fmt.phase("COVERAGE", f"OpenOCD: {response.strip() or '(keine Ausgabe)'}")
        if "Targets disconnected" not in response:
            fmt.phase("COVERAGE", f"[WARN] Dump evtl. unvollständig — 'Targets disconnected' nicht gesehen (Timeout {timeout}s)")

    def _clean_gcda(self) -> None:
        """Deletes leftover .gcda files from a previous dump before this one.

        This prevents gcov from resusing data from the last run.
        """
        removed = 0
        for root, _dirs, files in os.walk(self.build_dir):
            for name in files:
                if name.endswith(".gcda") and self._safe_remove(os.path.join(root, name)):
                    removed += 1
        if removed:
            fmt.phase("COVERAGE", f"{removed} alte .gcda-Datei(en) entfernt")

    def _remove_empty_gcda(self) -> None:
        removed = []
        for root, _dirs, files in os.walk(self.build_dir):
            for name in files:
                if not name.endswith(".gcda"):
                    continue
                path = os.path.join(root, name)
                try:
                    empty = os.path.getsize(path) == 0
                except OSError:
                    continue
                if empty and self._safe_remove(path):
                    removed.append(name)
        if removed:
            fmt.phase(
                "COVERAGE",
                f"[WARN] {len(removed)} leere .gcda-Datei(en) übersprungen (Dump nicht vollständig?): "
                + ", ".join(removed),
            )

    @staticmethod
    def _safe_remove(path: str) -> bool:
        try:
            os.remove(path)
            return True
        except OSError as exc:
            fmt.phase("COVERAGE", f"[WARN] Konnte {os.path.basename(path)} nicht löschen (noch in Benutzung?): {exc}")
            return False

    def stop(self) -> None:
        if self._proc and self._proc.poll() is None:
            self._proc.terminate()
            try:
                self._proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self._proc.kill()
        self._proc = None

    def build_report(self) -> (float | None):
        """Runs gcovr over the just-dumped .gcda files.

        Writes an HTML report under build/coverage_report/html.

        Returns the overall line-coverage percentage or None if gcda fails.
        """
        report_dir = os.path.join(self.build_dir, "coverage_report")
        html_dir = os.path.join(report_dir, "html")
        os.makedirs(html_dir, exist_ok=True)
        summary_json = os.path.join(report_dir, "summary.json")

        # remove emtpty files from the last run to avoid crashes
        self._remove_empty_gcda()

        fmt.phase("COVERAGE", "Erzeuge gcovr-Report ...")
        try:
            result = subprocess.run(
                [
                    "gcovr",
                    "-r", os.path.dirname(self.project_dir),
                    "--gcov-executable", _GCOV_EXECUTABLE,
                    "--gcov-ignore-errors", "all",
                    "--html-details", os.path.join(html_dir, "index.html"),
                    "--json-summary-pretty",
                    "--json-summary", summary_json,
                ],
                cwd=self.build_dir,
                capture_output=True,
                text=True,
                shell=True,
            )
        except FileNotFoundError:
            fmt.phase("COVERAGE", "[WARN] gcovr nicht gefunden — 'pip install gcovr'")
            return None

        if result.returncode != 0:
            fmt.phase("COVERAGE", f"[WARN] gcovr fehlgeschlagen: {result.stderr.strip()}")
            return None
        if "WARNING" in result.stderr:
            fmt.phase("COVERAGE", f"[WARN] gcovr: {result.stderr.strip()}")

        try:
            with open(summary_json, encoding="utf-8") as f:
                summary = json.load(f)
            files = summary.get("files", [])
            fmt.phase("COVERAGE", f"gcovr: {len(files)} Datei(en), {summary.get('line_total', 0)} Zeilen instrumentiert")
            self.files = sorted(
                (self._file_row(f) for f in files),
                key=lambda r: (r["component"], r["file"]),
            )
            return float(summary["line_percent"])
        except (OSError, KeyError, ValueError, json.JSONDecodeError) as exc:
            fmt.phase("COVERAGE", f"[WARN] Konnte gcovr-Summary nicht lesen: {exc}")
            return None

    @staticmethod
    def _file_row(entry: dict) -> dict:
        """Maps one gcovr JSON-summary file entry to a row with its owning TEST_COMPONENTS name split out for grouping."""
        parts = entry["filename"].replace("\\", "/").split("/")
        component = parts[1] if len(parts) > 1 and parts[0] == "components" else parts[0]
        return {
            "component": component,
            "file": parts[-1],
            "line_total": entry.get("line_total", 0),
            "line_covered": entry.get("line_covered", 0),
            "line_percent": entry.get("line_percent") or 0.0,
        }

    # ------------------------------------------------------------------

    def _pump_log(self) -> None:
        if not self._proc or not self._proc.stdout:
            return
        for line in self._proc.stdout:
            line = line.rstrip()
            if line:
                fmt.phase("OPENOCD", line)

    def _wait_ready(self, timeout: float) -> None:
        deadline = time.time() + timeout
        last_error = None
        while time.time() < deadline:
            if self._proc.poll() is not None:
                raise RuntimeError(f"OpenOCD ist unerwartet beendet (exit {self._proc.returncode})")
            try:
                self._send_command("reset run")
                return
            except OSError as exc:
                last_error = exc
                time.sleep(0.5)
        raise TimeoutError(
            f"OpenOCD Telnet-Server auf Port {_TELNET_PORT} nicht erreichbar (Timeout {timeout}s): {last_error}"
        )

    def _send_command(self, command: str, grace: float = 2.0, idle: float = 1.0, until: str = None) -> str:
        with socket.create_connection((_TELNET_HOST, _TELNET_PORT), timeout=5) as sock:
            sock.settimeout(1.0)
            self._drain(sock)  # discard OpenOCD's telnet banner/prompt
            sock.sendall(f"{command}\r\n".encode())
            return self._drain(sock, grace=grace, idle=idle, until=until)

    @staticmethod
    def _drain(sock: socket.socket, grace: float = 2.0, idle: float = 1.0, until: str = None) -> str:
        """Reads from sock until the 'until'- string is seen in the logs or reaches a timeout.
        """
        chunks = []
        hard_deadline = time.time() + grace
        idle_deadline = time.time() + idle
        while time.time() < hard_deadline and (until or time.time() < idle_deadline):
            try:
                data = sock.recv(4096)
            except socket.timeout:
                continue
            except OSError:
                break
            if not data:
                break
            chunks.append(data.decode(errors="replace"))
            idle_deadline = time.time() + idle
            if until and until in "".join(chunks):
                break
        return "".join(chunks)
