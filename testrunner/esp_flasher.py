import os
import re
import subprocess
import hashlib

from . import output_formatter as fmt

from help_functions import REPO_ROOT


class EspFlasher:
    """Builds and flashes the test firmware onto the ESP via idf.py.
    """

    def __init__(self, build_dir: str = None):
        """
        - param build_dir: Absolute path from the repo root
        """
        if build_dir is None:
            build_dir = os.path.join(REPO_ROOT, "datalogger", "test")
        self.build_dir = os.path.join(REPO_ROOT, build_dir)

        # One hash file per subapp (main_vecu, test, ...) so switching between subapps doesn't get mistaken for "nothing changed".
        subapp = re.sub(
            r"[^A-Za-z0-9_.-]+", "_", os.path.basename(os.path.normpath(self.build_dir))
        ) or "default"
        self._hash_file = os.path.join(
            os.path.dirname(os.path.abspath(__file__)), f".last_build_hash_{subapp}"
        )
        self._coverage_flag_file = os.path.join(
            os.path.dirname(os.path.abspath(__file__)), f".last_coverage_flag_{subapp}"
        )
        self._source_root = os.path.join(REPO_ROOT, "datalogger")

        self._hash_extensions = {".c", ".h", ".cmake", ".yml", ".Kconfig"}
        self._hash_filenames = {"CMakeLists.txt", "Kconfig", "Kconfig.projbuild", "sdkconfig.defaults", "sdkconfig.ci"}

    def build(self, force: bool = False, coverage: bool = False) -> None:
        if force:
            cache_file = os.path.join(self.build_dir, "build", "CMakeCache.txt")
            if os.path.exists(cache_file):
                os.remove(cache_file)
        cmd = ["idf.py", "build"]
        if coverage:
            cmd += ["-D", "TEST_COVERAGE=1"]
        self._run_idf(cmd)

    def flash(self, port: str) -> None:
        self._run_idf(["idf.py", "-p", port, "flash"])

    def _run_idf(self, cmd: list) -> None:
        process = subprocess.Popen(
            cmd,
            cwd=self.build_dir,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            encoding="utf-8",
            text=True,
            shell=True,
            bufsize=0,
        )

        buffer          = ""
        last_output     = ""
        progress_active = False
        current_phase   = "BUILD"
        progress_re     = re.compile(r"\[(\d+)/(\d+)\]\s+([^\[]+)")

        try:
            while True:
                chunk = process.stdout.read(128)
                if not chunk:
                    break
                buffer += chunk

                matches = list(progress_re.finditer(buffer))
                if matches:
                    for m in matches:
                        cur, total, obj = m.groups()
                        line = (
                            f"{fmt.YELLOW}[{current_phase}]"
                            f" [{cur}/{total}] {obj.strip()}{fmt.RESET}"
                        )
                        last_output = fmt.overwrite_line(
                            last_output if progress_active else "", line
                        )
                        progress_active = True
                    buffer = buffer[matches[-1].end():]

                if "\n" in buffer:
                    parts = buffer.split("\n")
                    for line in parts[:-1]:
                        line = line.strip()
                        if not line:
                            continue
                        if (
                            "Executing action: flash" in line
                            or 'Executing "ninja flash"' in line
                        ):
                            current_phase = "FLASH"
                        if progress_active:
                            print()
                            last_output     = ""
                            progress_active = False
                        fmt.phase(current_phase, line)
                    buffer = parts[-1]

            process.wait()
            print()
            if process.returncode != 0:
                raise RuntimeError(
                    f"[{current_phase}] Vorgang fehlgeschlagen (exit {process.returncode})"
                )

        except Exception:
            process.terminate()
            process.wait()
            raise

    def _compute_source_hash(self) -> str:
        h = hashlib.sha256()
        for root, dirs, files in os.walk(self._source_root):
            dirs[:] = sorted(d for d in dirs if d not in ("build", "managed_components"))
            for fname in sorted(files):
                _, ext = os.path.splitext(fname)
                if ext in self._hash_extensions or fname in self._hash_filenames:
                    path = os.path.join(root, fname)
                    h.update(path.encode())
                    with open(path, "rb") as f:
                        h.update(f.read())
        return h.hexdigest()

    def _needs_build(self) -> bool:
        if not os.path.exists(self._hash_file):
            return True
        current = self._compute_source_hash()
        with open(self._hash_file) as f:
            return f.read().strip() != current

    def _save_build_hash(self) -> None:
        with open(self._hash_file, "w") as f:
            f.write(self._compute_source_hash())

    def _coverage_changed(self, coverage: bool) -> bool:
        """Whether --coverage differs from the previous build. 
        
        Toggling it changes the CMake cache variable TEST_COVERAGE, which needs a rebuild.
        """
        if not os.path.exists(self._coverage_flag_file):
            return coverage
        with open(self._coverage_flag_file) as f:
            return f.read().strip() != ("1" if coverage else "0")

    def _save_coverage_flag(self, coverage: bool) -> None:
        with open(self._coverage_flag_file, "w") as f:
            f.write("1" if coverage else "0")

    def build_if_needed(self, rebuild: bool, coverage: bool = False) -> bool:
        """Builds the app if the source hash changed, --coverage was toggled, or --rebuild was given.

        Returns True if a build was actually performed.
        """
        coverage_changed = self._coverage_changed(coverage)

        if rebuild:
            fmt.phase("RUNNER", "--rebuild gesetzt — Firmware wird neu gebaut")
        elif coverage_changed:
            fmt.phase("RUNNER", f"--coverage {'aktiviert' if coverage else 'deaktiviert'} — CMake wird neu konfiguriert")
        elif self._needs_build():
            fmt.phase("RUNNER", "Quellcode geändert — Firmware wird neu gebaut")
        else:
            fmt.phase("RUNNER", "Firmware aktuell (Hash unverändert) — Build übersprungen")
            return False

        self.build(force=(rebuild or coverage_changed), coverage=coverage)
        self._save_build_hash()
        self._save_coverage_flag(coverage)
        return True

    def flash_if_needed(self, port: str, built: bool, force_flash: bool, no_flash: bool) -> None:
        """Flashes the app if it was just built or --force-flash was given."""
        if no_flash:
            fmt.phase("RUNNER", "--no-flash gesetzt — Flash übersprungen")
            return

        if built or force_flash:
            fmt.phase("RUNNER", f"Flashe ESP auf Port {port} ...")
            self.flash(port)
            fmt.phase("RUNNER", "Flash abgeschlossen")
            return

        fmt.phase("RUNNER", "Firmware bereits aktuell geflasht — Flash übersprungen")
