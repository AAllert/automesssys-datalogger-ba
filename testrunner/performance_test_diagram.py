import os
from dataclasses import dataclass
import matplotlib.pyplot as plt

from analyse_log import LogAnalyser
from help_functions import REPO_ROOT

analyser = LogAnalyser(os.path.join(REPO_ROOT, "testrunner", "results"))

label_size = 14

log_wlan_debug_on = analyser.parse_logfile(os.path.join(REPO_ROOT, "testlogs", "raw_2026-07-21_test_wlan_on_debug_on.log"))
log_wlan_on = analyser.parse_logfile(os.path.join(REPO_ROOT, "testlogs", "raw_2026-07-21_test_wlan_on_debug_off.log"))
log_debug_on = analyser.parse_logfile(os.path.join(REPO_ROOT, "testlogs", "raw_2026-07-21_test_wlan_off_debug_on.log"))
log_all_off = analyser.parse_logfile(os.path.join(REPO_ROOT, "testlogs", "raw_2026-07-21_test_wlan_off_debug_off.log"))

_, medians_1, _, signal_names = analyser.analyse_signals(log_wlan_debug_on)
_, medians_2, _, _ = analyser.analyse_signals(log_wlan_on)
_, medians_3, _, _ = analyser.analyse_signals(log_debug_on)
_, medians_4, _, _ = analyser.analyse_signals(log_all_off)



value_count = analyser.count_values(log_wlan_on)
signal_numbers: list[int] = range(len(signal_names))

plt.figure(figsize=(12,5))
#plt.scatter(signal_numbers, medians_1, color="red", marker="o", alpha=0.5, label="Wifi & Debug an")   # complete overlap with medians 3
plt.scatter(signal_numbers, medians_2, color="blue", marker="x", label="Wifi an")
plt.scatter(signal_numbers, medians_3, color="green", marker="x", label="Debug an")
plt.scatter(signal_numbers, medians_4, color="orange", marker="x", label="beides aus")

plt.xlabel("Signalnummer in der UDS_config.csv", fontsize=label_size)
plt.ylabel("Antwortzeit (Median) [ms]", fontsize=label_size)
plt.xlim(-0.5, len(signal_names) - 0.5)
plt.ylim(-0.5, 45)
plt.grid(True)
plt.legend(fontsize=label_size)

plt.tight_layout()
plt.savefig(os.path.join(REPO_ROOT, "testrunner", "results", "performance_analyis.png"), dpi=300)
plt.show()