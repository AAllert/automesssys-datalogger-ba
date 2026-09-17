import matplotlib.pyplot as plt
import statistics
import re
from collections import defaultdict
from pathlib import Path
from dataclasses import dataclass

@dataclass
class LogEntry:
    timestamp: float
    signal_name: str
    value: bytearray

class LogAnalyser:

    def __init__(self, output_folder: str) -> None:
        self.pattern = re.compile(r"^(?P<timestamp>\S+)\s*-\s*(?P<signal>\S+)\s*-\s*b'(?P<value>[^']*)'$")
        self.output_folder = Path(output_folder)
        self.output_folder.mkdir(parents=True, exist_ok=True)
        self.log_prefix = ""

    def parse_logfile(self, logfile_path) -> list[LogEntry]:
        self.log_prefix = Path(logfile_path).stem
        log = []

        with open(logfile_path) as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue

                match = self.pattern.match(line)
                if match is None:
                    continue

                value = bytearray(
                    int(x, 16)
                    for x in match["value"].split(",")
                    if x
                )

                log.append(
                    LogEntry(
                        timestamp=float(match["timestamp"]),
                        signal_name=match["signal"],
                        value=value,
                    )
                )

        return log
    
    def count_values(self, log: list[LogEntry]) -> int:
        if not log:
            return 0
        
        start_signal = log[0].signal_name
        value_count = 1
        for entry in log[1:]:
            value_count += 1
            if entry.signal_name == start_signal:
                break
        return value_count
    
    def count_cycles(self, log: list[LogEntry]) -> int:
        if not log:
            return 0
        
        start_signal = log[0].signal_name
        cycle_count = 0
        for entry in log:
            if entry.signal_name == start_signal:
                cycle_count += 1

        return cycle_count
    
    def analyse_cycles(self, log: list[LogEntry]) -> tuple[list[float], list[int]]:
        if not log:
            return
        
        start_signal = log[0].signal_name
        cycle_durations = []
        cycle_numbers = []

        last_cycle_start = None
        cycle = 0

        for entry in log:
            if entry.signal_name == start_signal:
                timestamp = entry.timestamp
                if last_cycle_start is not None:
                    duration = timestamp - last_cycle_start
                    cycle_durations.append(duration)
                    cycle_numbers.append(cycle)

                last_cycle_start = timestamp
                cycle += 1
        
        return cycle_numbers, cycle_durations

    def build_cycle_diagram(self, log: list[LogEntry]) -> None:
        if not log:
            return
        cycle_numbers, cycle_durations = self.analyse_cycles(log)
        value_count = self.count_values(log)

        plt.figure(figsize=(10, 3))
        plt.scatter(cycle_numbers, cycle_durations, marker="x")
        plt.xlabel("Zyklus", fontsize=14)
        plt.ylabel("Dauer [s]", fontsize=14)
        #plt.ylim(bottom=1.2, top=1.7)
        plt.xlim(left=0)
        plt.title(f"Zyklusdauer ({value_count} Werte)")
        plt.grid(True)
        plt.tight_layout()
        plt.savefig(self.output_folder / f"{self.log_prefix}_cycle_diagram.png", dpi=300)
        plt.show()

    def analyse_signals(self, log: list[LogEntry]) -> tuple[list[float], list[float], list[float], list[str]]:
        durations = defaultdict(list)

        for current, nxt in zip(log[:-1], log[1:]):
            dt = (nxt.timestamp - current.timestamp) * 1000.0
            if dt > 1000:
                print(f"{current.signal_name}: {dt}")
            durations[current.signal_name].append(dt)

        signal_names = list(durations.keys())

        mins = [min(durations[s]) for s in signal_names]
        medians = [statistics.median(durations[s]) for s in signal_names]
        maxs = [max(durations[s]) for s in signal_names]
        
        return mins, medians, maxs, signal_names

    def build_signal_diagram(self, log: list[LogEntry]) -> None:
        mins, medians, maxs, signal_names = self.analyse_signals(log)
        cycle_count = self.count_cycles(log)
        value_count = self.count_values(log)
        signal_numbers: list[int] = range(len(signal_names))

        plt.figure(figsize=(12,5))
        plt.scatter(signal_numbers, maxs, color="red", marker="x", label="Maximum")
        plt.scatter(signal_numbers, medians, color="blue", marker="x", label="Median")
        plt.scatter(signal_numbers, mins, color="green", marker="x", label="Minimum")

        plt.xlabel("Signalnummer")
        if (value_count < 100):
            plt.xticks(range(len(signal_names)), signal_names, rotation=90)
        plt.ylabel("Bearbeitungszeit [ms]")
        plt.xlim(0, len(signal_names) - 0.5)
        plt.ylim(bottom=0, top=75)
        #plt.yticks(list(range(0, 100, 10)))
        plt.title(f"Antwortzeiten ({cycle_count} Zyklen)")
        plt.grid(True)
        plt.legend(loc="upper left")

        plt.tight_layout()
        plt.savefig(self.output_folder / f"{self.log_prefix}_signal_diagram.png", dpi=300)
        plt.show()


import os
import argparse
from help_functions import REPO_ROOT

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Analysiert eine Logdatei.")
    parser.add_argument("logfile", help="Pfad zur Logdatei")
    args = parser.parse_args()

    analyser = LogAnalyser(os.path.join(REPO_ROOT, "testrunner", "results"))
    log = analyser.parse_logfile(args.logfile)
    analyser.build_cycle_diagram(log)
    analyser.build_signal_diagram(log)
