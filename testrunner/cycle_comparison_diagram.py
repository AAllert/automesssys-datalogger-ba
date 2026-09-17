import os
import statistics
import matplotlib.pyplot as plt

from analyse_log import LogAnalyser
from help_functions import REPO_ROOT

RPI_LOG_FILE = os.path.join(REPO_ROOT, "testlogs", "raw_2025-05-22_21-38-02.txt")
ESP32_LOG_FILE = os.path.join(REPO_ROOT, "testlogs", "raw_ID3_2026-09-16_14-06-36_comp.log")

label_size = 14

analyser = LogAnalyser(os.path.join(REPO_ROOT, "testrunner", "results"))

log_rpi = analyser.parse_logfile(RPI_LOG_FILE)
log_esp32 = analyser.parse_logfile(ESP32_LOG_FILE)

cycles_rpi, durations_rpi = analyser.analyse_cycles(log_rpi)
cycles_esp32, durations_esp32 = analyser.analyse_cycles(log_esp32)

_, _, _, signals_rpi = analyser.analyse_signals(log_rpi)
_, _, _, signals_esp32 = analyser.analyse_signals(log_esp32)

median_rpi = statistics.median(durations_rpi)
median_esp32 = statistics.median(durations_esp32)
speedup_percent = (median_rpi - median_esp32) / median_rpi * 100

plt.figure(figsize=(10, 5))

plt.scatter(cycles_rpi, durations_rpi, marker="x", color="green", alpha=0.6, label="RPI Datalogger")
plt.scatter(cycles_esp32, durations_esp32, marker="x", color="orange", alpha=0.6, label="ESP32 Datalogger")
plt.axhline(y=0.5, color="red", linestyle="--", linewidth=1.5, label="Zielwert 500 ms")

plt.xlabel("Zyklus", fontsize=label_size)
plt.ylabel("Dauer [s]", fontsize=label_size)
plt.xlim(left=0)
plt.ylim(bottom=0)
plt.grid(True)
plt.legend(fontsize=label_size)

plt.tight_layout()
plt.savefig(analyser.output_folder / "cycle_diagram_rpi_vs_esp32.png", dpi=300)
plt.show()
