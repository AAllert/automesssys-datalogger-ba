import os
import sys
import threading
from queue import Empty, Queue

_REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, _REPO_ROOT)

from testrunner.esp_flasher import EspFlasher
from testrunner import output_formatter as fmt
from testrunner.serial_bridge import SerialBridge
from testrunner.help_functions import get_args, find_esp_port, start_vecu, stop_process

def main() -> None:
    args = get_args()
    port = args.port or find_esp_port()
    baud = args.baud
    
    flasher: EspFlasher = EspFlasher("datalogger/main_vecu")
    built = flasher.build_if_needed(args.rebuild)
    flasher.flash_if_needed(port, built, args.force_flash, args.no_flash)

    vecu_proc = None
    bridge    = None

    try: 
        fmt.phase("RUNNER", "Starte vECU (stdin-Modus) ...")
        vecu_proc = start_vecu(args.vehicle, args.scenario, args.ambient_temp, args.update_rate)

        bridge = SerialBridge(port, baud)
        bridge.start(vecu_proc)
        fmt.phase("RUNNER", f"Serial Bridge geöffnet auf {port} ...")

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

        fmt.phase("RUNNER", "System-Test läuft. Strg+C zum Beenden ...")

        esp_alive  = True
        vecu_alive = vecu_proc is not None

        while esp_alive or vecu_alive:
            try:
                event_type, data = event_queue.get(timeout=0.5)
            except Empty:
                continue

            if event_type == "ESP_LOG":
                fmt.esp_log(data)
            elif event_type == "VECU_LOG":
                fmt.vecu_log(data)
            elif event_type == "ESP_EOF":
                fmt.phase("RUNNER", "[WARN] ESP-Verbindung beendet")
                esp_alive = False
            elif event_type == "VECU_EOF":
                fmt.phase("RUNNER", "[WARN] vECU-Verbindung beendet")
                vecu_alive = False

    except KeyboardInterrupt:
        fmt.phase("RUNNER", "Abbruch durch Benutzer (Strg+C)")

    finally:
        if bridge:
            bridge.stop()
        if vecu_proc:
            stop_process(vecu_proc)


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    main()