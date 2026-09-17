import os
import sys
import threading
from queue import Empty, Queue

_REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, _REPO_ROOT)

from testrunner import output_formatter as fmt
from testrunner.esp_flasher import EspFlasher
from testrunner.gcov_runner import GcovRunner
from testrunner.serial_bridge import SerialBridge
from testrunner.unity_parser import UnityParser
from testrunner.help_functions import get_args, start_vecu, stop_process, find_esp_port

# ------------------------------------------------------------------
# Tag helpers
# ------------------------------------------------------------------

# Canonical list of all test tags in the project — used when no --tags are specified (run-all mode). Update this list when new tagged test groups are added.
_ALL_KNOWN_TAGS = ["can_ascii", "can_backend",  "config_parser", "csv_log_manager", "datalogger_service", "interpolator", "log_manager", "nvs_storage", "sdcard", "storage", "uds", "uds_config", "uds_decoder"]

# Tags that require an active CAN backend (USB/vECU or hardware TWAI). 
_CAN_REQUIRED_TAGS = {"can_backend", "uds", "datalogger_service"}


def _determine_backend(tags: list[str], vecu: bool) -> str:
    """Return the BACKEND string to send to the ESP.
    """
    if vecu:
        return "usb"
    if not tags:
        # No tag filter -> run everything -> assume CAN may be needed
        return "can"
    clean = {t.strip("[]") for t in tags}
    if clean & _CAN_REQUIRED_TAGS:
        return "can"
    return "none"


def _format_tags_for_esp(tags: list[str]) -> str:
    """Normalise user-supplied tag strings and join for the TAGS: protocol line.
    """
    source = tags if tags else _ALL_KNOWN_TAGS
    result = []
    for t in source:
        t = t.strip()
        if not t.startswith("["):
            t = f"[{t}"
        if not t.endswith("]"):
            t = f"{t}]"
        result.append(t)
    return ",".join(result)


# ------------------------------------------------------------------
# Entry point
# ------------------------------------------------------------------

def main() -> None:
    args = get_args()
    port = args.port or find_esp_port()
    baud = args.baud

    # 1. Build & Flash
    flasher = EspFlasher(os.path.join("datalogger", "test"))
    built = flasher.build_if_needed(args.rebuild, args.coverage)
    flasher.flash_if_needed(port, built, args.force_flash, args.no_flash)

    vecu_proc = None
    bridge    = None
    gcov      = None
    coverage_percent = None

    try:
        # 1. start openOCD for --coverage
        if args.coverage:
            gcov = GcovRunner(flasher.build_dir, os.path.join(flasher.build_dir, "build"))
            gcov.start()

        # 2. start vECU
        if args.vecu:
            fmt.phase("RUNNER", "Starte vECU (stdin-Modus) ...")
            vecu_proc = start_vecu(args.vehicle, "always_on", args.ambient_temp, args.update_rate)
        else:
            fmt.phase("RUNNER", "Kein --vecu — vECU wird nicht gestartet (Echtfahrzeug-Modus)")

        # 3. open serial bridge
        fmt.phase("RUNNER", f"Öffne Serial Bridge auf {port} ...")
        bridge = SerialBridge(port, baud)
        bridge.start(vecu_proc)
        fmt.phase("RUNNER", "Bridge läuft. Warte auf Tests ...")

        # 4 start log- collectors
        event_queue: Queue = Queue()

        def _esp_feeder():
            while True:
                item = bridge.esp_log_queue.get()
                event_queue.put(("ESP_EOF" if item is None else "ESP_LOG", item))
                if item is None:
                    break

        def _vecu_feeder():
            while True:
                item = bridge.vecu_log_queue.get()
                event_queue.put(("VECU_EOF" if item is None else "VECU_LOG", item))
                if item is None:
                    break

        threading.Thread(target=_esp_feeder, daemon=True).start()
        if vecu_proc is not None:
            threading.Thread(target=_vecu_feeder, daemon=True).start()

        # 5. prepare config values for handshake
        backend_str = _determine_backend(args.tags or [], args.vecu)
        tags_str = _format_tags_for_esp(args.tags or [])

        # 6. Haupt-Eventloop 
        unity   = UnityParser()
        results = []
        timeout = 120  # timeout in seconds after which the tests must be finished

        while True:
            try:
                event_type, data = event_queue.get(timeout=timeout)
            except Empty:
                fmt.phase("RUNNER", f"[TIMEOUT] Keine Aktivität für {timeout}s — Abbruch.")
                break

            if event_type == "ESP_LOG":
                fmt.esp_log(data)

                if data.strip() == "HELLO":
                    coverage_str = "1" if args.coverage else "0"
                    bridge.send_to_esp("READY\n")
                    bridge.send_to_esp(f"BACKEND:{backend_str}\n")
                    bridge.send_to_esp(f"TAGS:{tags_str}\n")
                    bridge.send_to_esp(f"COVERAGE:{coverage_str}\n")
                    fmt.phase("RUNNER",
                              f"HELLO -> READY + BACKEND:{backend_str} + TAGS:{tags_str} + COVERAGE:{coverage_str}")

                if data.strip() == "CONFIG_ACK":
                    fmt.phase("RUNNER", "CONFIG_ACK empfangen — ESP startet Tests")

                result = unity.feed(data)
                if result:
                    results.append(result)

                if unity.done:
                    break

            elif event_type == "VECU_LOG":
                fmt.vecu_log(data)

            elif event_type in ("ESP_EOF", "VECU_EOF"):
                fmt.phase("RUNNER", f"[WARN] {event_type} — Verbindung unterbrochen")
                break

        # 6b. Coverage-Dump after running tests
        if args.coverage and unity.done:
            gcov.dump()
            coverage_percent = gcov.build_report()

    finally:
        if bridge:
            bridge.stop()
        if vecu_proc:
            stop_process(vecu_proc)
        if gcov:
            gcov.stop()

    # 7. print results
    fmt.print_results(results)
    if args.coverage:
        fmt.print_coverage_table(gcov.files)
        fmt.print_coverage(coverage_percent)

if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    main()
