import re
import shutil
import textwrap
from itertools import groupby

GREEN  = "\033[92m"
RED    = "\033[91m"
CYAN   = "\033[96m"
YELLOW = "\033[93m"
RESET  = "\033[0m"

_ansi_re = re.compile(r'\x1b\[[0-9;]*m')

# Matches the absolute path prefix up to (but not including) the repo root
_abs_path_re = re.compile(
    r'(?:[A-Za-z]:[/\\]|/)[^\s]*?(?=e-mobility-datalogger-ba)'
)


def _clean(line: str) -> str:
    """Strip absolute path prefixes so only repo-relative paths remain."""
    return _abs_path_re.sub('', line)


def esp_log(line: str) -> None:
    print(f"{YELLOW}[LOGGER]{RESET} {_clean(line)}", flush=True)


def vecu_log(line: str) -> None:
    print(f"{CYAN}[VECU]{RESET} {_clean(line)}", flush=True)


def phase(tag: str, msg: str) -> None:
    print(f"{YELLOW}[{tag}]{RESET} {_clean(msg)}", flush=True)


# ------------------------------------------------------------------
# Progress-line overwrite helpers
# ------------------------------------------------------------------

def _visible_len(text: str) -> int:
    return len(_ansi_re.sub('', text))


def overwrite_line(last: str, new: str) -> str:
    """Erase `last` from the terminal and print `new` in its place."""
    if last:
        term_w = shutil.get_terminal_size().columns
        height = max(1, (_visible_len(last) + term_w - 1) // term_w)
        for i in range(height):
            print("\033[2K", end="")
            if i < height - 1:
                print("\033[1A", end="")
        print("\r", end="")
    print(new, end="", flush=True)
    return new


# ------------------------------------------------------------------
# Final results table
# ------------------------------------------------------------------

def print_results(results: list) -> None:
    status_w, name_w, tag_w, msg_w = 7, 40, 15, 50
    sep = "=" * (status_w + name_w + tag_w + msg_w + 10)

    print("\n" + sep)
    print(f"{'STATUS':<{status_w}} | {'TEST NAME':<{name_w}} | {'TAG':<{tag_w}} | MESSAGE")
    print(sep)

    for r in results:
        name_lines = textwrap.wrap(r["name"],    width=name_w) or [""]
        msg_lines  = textwrap.wrap(r["message"], width=msg_w)  or [""]
        max_lines  = max(len(name_lines), len(msg_lines))

        for i in range(max_lines):
            n = name_lines[i] if i < len(name_lines) else ""
            m = msg_lines[i]  if i < len(msg_lines)  else ""
            if i == 0:
                s = _color_status(f"{r['status']:<{status_w}}")
                print(f"{s} | {n:<{name_w}} | {r['tag']:<{tag_w}} | {m}")
            else:
                print(f"{'':<{status_w}} | {n:<{name_w}} | {'':<{tag_w}} | {m}")

    print(sep)
    total   = len(results)
    failed  = sum(1 for r in results if r["status"] == "FAIL")
    passed  = sum(1 for r in results if r["status"] == "PASS")
    ignored = total - failed - passed
    print(
        f"Total: {total}  |  "
        f"{GREEN}Passed: {passed}{RESET}  |  "
        f"{RED}Failed: {failed}{RESET}  |  "
        f"{YELLOW}Ignored: {ignored}{RESET}"
    )


def _color_pct(percent: float) -> str:
    return GREEN if percent >= 80 else (YELLOW if percent >= 50 else RED)


def print_coverage(percent) -> None:
    if percent is None:
        print(f"{YELLOW}Code Coverage: nicht ermittelbar (siehe [COVERAGE]-Log oben){RESET}")
        return
    print(f"{_color_pct(percent)}Code Coverage: {percent:.1f}%{RESET}")


def print_coverage_table(files: list) -> None:
    """Prints a summary table of the detailed coverage report using only the line coverages 
    """
    if not files:
        return

    name_w, num_w = 45, 8
    header = f"{'COMPONENT / FILE':<{name_w}} | {'LINES':>{num_w}} | {'COVERED':>{num_w}} | {'%':>7}"
    sep = "=" * len(header)

    print("\n" + sep)
    print(header)
    print(sep)

    for component, rows in groupby(files, key=lambda r: r["component"]):
        rows = list(rows)
        comp_total   = sum(r["line_total"] for r in rows)
        comp_covered = sum(r["line_covered"] for r in rows)
        comp_percent = (comp_covered / comp_total * 100) if comp_total else 0.0

        print(f"{component:<{name_w}} | {comp_total:>{num_w}} | {comp_covered:>{num_w}} | "
              f"{_color_pct(comp_percent)}{comp_percent:>6.1f}%{RESET}")
        for r in rows:
            label = f"  {r['file']}"
            print(f"{label:<{name_w}} | {r['line_total']:>{num_w}} | {r['line_covered']:>{num_w}} | "
                  f"{_color_pct(r['line_percent'])}{r['line_percent']:>6.1f}%{RESET}")
        print("-" * len(sep))

    print(sep)


def _color_status(s: str) -> str:
    clean = s.strip()
    if clean == "PASS":
        return f"{GREEN}{s}{RESET}"
    if clean == "FAIL":
        return f"{RED}{s}{RESET}"
    return f"{YELLOW}{s}{RESET}"
