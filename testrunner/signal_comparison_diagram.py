import csv
import os
import re
import statistics
import matplotlib.pyplot as plt

from analyse_log import LogAnalyser
from help_functions import REPO_ROOT

RPI_LOG_FILE = os.path.join(REPO_ROOT, "testlogs", "raw_2025-05-22_21-38-02.txt")
ESP32_LOG_FILE = os.path.join(REPO_ROOT, "testlogs", "raw_ID3_2026-09-16_14-06-36_comp.log")
CONFIG_FILE = os.path.join(REPO_ROOT, "configs", "UdsConfig_v06_ID3.csv")

ECU_COLORS = ["aqua", "lime"]

label_size = 14


def normalize_label(name: str) -> str:
    return re.sub(r"\d+$", "", name)


def load_uds_config(config_path: str) -> dict[str, dict]:
    """Maps each signal label to its row index, ECU and response length in the configuration file."""
    with open(config_path, newline="", encoding="utf-8-sig") as f:
        return {
            row["strLab"]: {"order": i, "ecu": row["strEcu"], "num_bytes": int(row["numBytes"])}
            for i, row in enumerate(csv.DictReader(f))
        }


def resolve_config(label: str, config: dict[str, dict]) -> dict:
    """Looks up a label in the config, falling back to its normalized form."""
    if label in config:
        return config[label]

    normalized = normalize_label(label)
    for key, entry in config.items():
        if normalize_label(key) == normalized:
            return entry

    raise KeyError(label)


def build_ecu_blocks(labels: list[str], config: dict[str, dict]) -> list[tuple[int, int, str]]:
    blocks = []
    start = 0
    current_ecu = resolve_config(labels[0], config)["ecu"]
    for i, label in enumerate(labels[1:], start=1):
        ecu = resolve_config(label, config)["ecu"]
        if ecu != current_ecu:
            blocks.append((start, i - 1, f"ECU {current_ecu}"))
            start = i
            current_ecu = ecu
    blocks.append((start, len(labels) - 1, f"ECU {current_ecu}"))
    return blocks


def match_labels(names_rpi: list[str], names_esp32: list[str]) -> dict[str, tuple[str, str]]:
    """Maps a canonical label to the matching (rpi_name, esp32_name) pair."""
    remaining_rpi = list(names_rpi)
    remaining_esp32 = list(names_esp32)
    matches: dict[str, tuple[str, str]] = {}

    for name in list(remaining_rpi):
        if name in remaining_esp32:
            matches[name] = (name, name)
            remaining_rpi.remove(name)
            remaining_esp32.remove(name)

    esp32_by_normalized = {normalize_label(name): name for name in remaining_esp32}
    for name in remaining_rpi:
        esp32_name = esp32_by_normalized.get(normalize_label(name))
        if esp32_name is not None:
            matches[name] = (name, esp32_name)

    return matches


analyser = LogAnalyser(os.path.join(REPO_ROOT, "testrunner", "results"))

log_rpi = analyser.parse_logfile(RPI_LOG_FILE)
log_esp32 = analyser.parse_logfile(ESP32_LOG_FILE)

_, medians_rpi, _, signals_rpi = analyser.analyse_signals(log_rpi)
_, medians_esp32, _, signals_esp32 = analyser.analyse_signals(log_esp32)

medians_by_signal_rpi = dict(zip(signals_rpi, medians_rpi))
medians_by_signal_esp32 = dict(zip(signals_esp32, medians_esp32))

config = load_uds_config(CONFIG_FILE)

matches = match_labels(signals_rpi, signals_esp32)
labels = sorted(matches, key=lambda label: resolve_config(label, config)["order"])

medians_rpi = [medians_by_signal_rpi[matches[label][0]] for label in labels]
medians_esp32 = [medians_by_signal_esp32[matches[label][1]] for label in labels]
num_bytes = [resolve_config(label, config)["num_bytes"] for label in labels]
ecu_blocks = build_ecu_blocks(labels, config)

median_rpi = statistics.median(medians_rpi)
median_esp32 = statistics.median(medians_esp32)
speedup_percent = (median_rpi - median_esp32) / median_rpi * 100

signal_numbers = range(len(labels))

fig, ax1 = plt.subplots(figsize=(14, 6))
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

ax1.scatter(signal_numbers, medians_rpi, marker="x", color="green", label="RPI Datalogger")
ax1.scatter(signal_numbers, medians_esp32, marker="x", color="orange", label="ESP32 Datalogger")

ax1.set_xlabel("Signal", fontsize=label_size)
ax1.set_ylabel("Bearbeitungszeit [ms]", fontsize=label_size)
ax1.set_xticks(list(signal_numbers))
ax1.set_xticklabels(labels, rotation=90)
ax1.set_xlim(-0.5, len(labels) - 0.5)
ax1.set_ylim(bottom=0)
ax1.grid(True)

lines1, labels1 = ax1.get_legend_handles_labels()
lines2, labels2 = ax2.get_legend_handles_labels()
ax1.legend(
    lines1 + lines2, labels1 + labels2,
    loc="lower center", bbox_to_anchor=(0.5, 1.02), ncol=4, fontsize=label_size
)

fig.tight_layout()
fig.savefig(analyser.output_folder / "signal_diagram_rpi_vs_esp32.png", dpi=300, bbox_inches="tight")
plt.show()
