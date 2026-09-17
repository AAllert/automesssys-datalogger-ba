import argparse
import subprocess
import sys
import os
import serial.tools.list_ports

import output_formatter as fmt

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def get_args():
    p = argparse.ArgumentParser(description="HIL Test Runner")

    # Hardware
    p.add_argument("--port", help="Serieller Port des ESP (z.B. COM6)")
    p.add_argument("--baud", type=int, default=115200, help="Baudrate (default: 115200)")

    # Test selection
    p.add_argument(
        "--tags", nargs="*",
        metavar="TAG",
        help="Test-Tags die ausgeführt werden sollen, ohne Angabe werden alle Tests ausgeführt.",
    )

    # vECU
    p.add_argument(
        "--vecu", action="store_true",
        help="Startet die vECU und nutzt diese zur Testausführung",
    )

    # vECU parameters (only relevant when --vecu is given)
    p.add_argument("--vehicle", default="vw_id3")
    p.add_argument("--scenario", default="city_drive")
    p.add_argument("--ambient-temp", type=float, default=20.0)
    p.add_argument("--update-rate", type=int,   default=50)

    # Build control
    p.add_argument(
        "--rebuild", action="store_true",
        help="Immer neu bauen, auch wenn sich der Quellcode nicht geändert hat.",
    )

    # Coverage
    p.add_argument(
        "--coverage", action="store_true",
        help="Erstellt zusätzlich zur Ausführung der Tests einen coverage report.",
    )

    # Flash control
    flash_group = p.add_mutually_exclusive_group()
    flash_group.add_argument(
        "--force-flash", action="store_true",
        help="Immer flashen, auch wenn kein Neubau stattgefunden hat.",
    )
    flash_group.add_argument(
        "--no-flash", action="store_true",
        help="Niemals flashen, Firmware wird als aktuell angenommen.",
    )

    return p.parse_args()

def start_vecu(vehicle: str, scenario: str, ambient_temp: str, update_rate: str) -> subprocess.Popen:
    scenario_path = "./scenarios/" + scenario + ".yaml"
    cmd = [
        sys.executable, "-u", "-m", "src.main",
        "--transport",    "stdin",
        "--vehicle",      str(vehicle),
        "--scenario",     scenario_path,
        "--ambient-temp", str(ambient_temp),
        "--update-rate",  str(update_rate),
    ]
    return subprocess.Popen(
        cmd,
        cwd=os.path.join(REPO_ROOT, "vecu"),
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        bufsize=1,
    )


def stop_process(proc: subprocess.Popen) -> None:
    if proc and proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()


def find_esp_port() -> str:
    ports = serial.tools.list_ports.comports()
    if not ports:
        raise RuntimeError("Keine seriellen Ports gefunden")

    candidates = []
    for p in ports:
        desc = (p.description or "").lower()
        hwid = (p.hwid or "").lower()
        fmt.phase("PORT", f"{p.device} | {p.description} | {p.hwid}")
        if any(x in desc for x in ("cp210", "ch340", "usb serial", "ftdi")):
            candidates.append(p.device)
        elif any(x in hwid for x in ("cp210", "ch340", "usb")):
            candidates.append(p.device)

    if len(candidates) == 1:
        fmt.phase("PORT", f"Verwende automatisch: {candidates[0]}")
        return candidates[0]
    if len(candidates) > 1:
        raise RuntimeError(f"Mehrere ESP-Ports gefunden: {candidates} — bitte --port angeben")
    raise RuntimeError("ESP nicht gefunden — bitte --port angeben")