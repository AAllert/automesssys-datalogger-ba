import csv
import os
import matplotlib.pyplot as plt

from analyse_log import LogAnalyser
from help_functions import REPO_ROOT

LOG_FILE = os.path.join(REPO_ROOT, "testlogs", "raw_ID3_2026-09-02_14-09-13.log")
CONFIG_FILE = os.path.join(REPO_ROOT, "configs", "UdsConfig_v06_ID3.csv")

ECU_COLORS = ["aqua", "lime"]

label_size = 14

def load_uds_config(config_path: str) -> dict[str, tuple[str, int]]:
    config = {}
    with open(config_path, newline="", encoding="utf-8-sig") as f:
        for row in csv.DictReader(f):
            config[row["strLab"]] = (row["strEcu"], int(row["numBytes"]))
    return config


def build_ecu_blocks(signal_names: list[str], config: dict[str, tuple[str, int]]) -> list[tuple[int, int, str]]:
    blocks = []
    start = 0
    current_ecu, _ = config[signal_names[0]]
    for i, name in enumerate(signal_names[1:], start=1):
        ecu, _ = config[name]
        if ecu != current_ecu:
            blocks.append((start, i - 1, f"ECU {current_ecu}"))
            start = i
            current_ecu = ecu
    blocks.append((start, len(signal_names) - 1, f"ECU {current_ecu}"))
    return blocks


analyser = LogAnalyser(os.path.join(REPO_ROOT, "testrunner", "results"))
log = analyser.parse_logfile(LOG_FILE)
config = load_uds_config(CONFIG_FILE)

mins, medians, maxs, signal_names = analyser.analyse_signals(log)
cycle_count = analyser.count_cycles(log)
num_bytes = [config[name][1] for name in signal_names]
ecu_blocks = build_ecu_blocks(signal_names, config)
signal_numbers: list[int] = range(len(signal_names))

fig, ax1 = plt.subplots(figsize=(12, 5))
ax2 = ax1.twinx()

ax1.set_zorder(ax2.get_zorder() + 1)
ax1.patch.set_visible(False)

# areas for ECUs
for i, (start, end, label) in enumerate(ecu_blocks):
    ax1.axvspan(start - 0.5, end + 0.5, color=ECU_COLORS[i % len(ECU_COLORS)], alpha=0.15, zorder=0)
    ax1.text(
        (start + end) / 2, 0.96, label, ha="center", va="top",
        transform=ax1.get_xaxis_transform(),
        bbox=dict(facecolor="white", alpha=0.6, edgecolor="none", pad=1), fontsize=label_size
    )

# bar chart for numBytes
ax2.bar(signal_numbers, num_bytes, width=1.0, color="gray", alpha=0.25, label="Antwortlänge")
ax2.set_ylabel("Antwortlänge [Byte]", fontsize=label_size)
ax2.set_ylim(bottom=0)

# response times
ax1.scatter(signal_numbers, maxs, color="red", marker="x", label="Maximum")
ax1.scatter(signal_numbers, medians, color="blue", marker="x", label="Median")
ax1.scatter(signal_numbers, mins, color="green", marker="x", label="Minimum")

ax1.set_xlabel("Signalnummer", fontsize=label_size)
ax1.set_ylabel("Bearbeitungszeit [ms]", fontsize=label_size)
ax1.set_xlim(-0.5, len(signal_names) - 0.5)
ax1.set_ylim(bottom=0)
ax1.set_yticks(list(range(0, 100, 10)))
ax1.grid(True)

lines1, labels1 = ax1.get_legend_handles_labels()
lines2, labels2 = ax2.get_legend_handles_labels()
ax1.legend(
    lines1 + lines2, labels1 + labels2,
    loc="upper center", bbox_to_anchor=(0.5, -0.12), ncol=4, fontsize=label_size
)

fig.tight_layout()
fig.savefig(analyser.output_folder / f"{analyser.log_prefix}_ecu_signal_diagram.png", dpi=300, bbox_inches="tight")
plt.show()
