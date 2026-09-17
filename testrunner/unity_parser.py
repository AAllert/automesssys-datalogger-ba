import re
from typing import Optional

class UnityParser:
    """
    Parses Unity test output lines streamed from the ESP.
    """

    def __init__(self):
        self.current_tag = "default"
        self.done        = False

        self._result_re = re.compile(r":(?P<name>[^:]+):(PASS|FAIL|IGNORE)(?::\s*(?P<msg>.*))?$")
        self._tag_re  = re.compile(r"tests matching '\[([^\]]+)\]'")
        self._done_re = re.compile(r"\bTests\b.*\bFailures\b")

    def feed(self, line: str) -> Optional[dict]:
        """
        Process one log line.

        Returns a result dict {"name", "tag", "status", "message"} when a PASS/FAIL/IGNORE line is detected, otherwise None.
        Sets self.done = True when the Unity summary line is seen.
        """
        tag_m = self._tag_re.search(line)
        if tag_m:
            self.current_tag = tag_m.group(1)

        if self._done_re.search(line):
            self.done = True
            return None

        if ".c:" in line and any(s in line for s in ("PASS", "FAIL", "IGNORE")):
            return self._parse(line)

        return None

    def _parse(self, line: str) -> Optional[dict]:
        m = self._result_re.search(line)
        if not m:
            return None

        if "PASS" in line:
            status = "PASS"
        elif "FAIL" in line:
            status = "FAIL"
        else:
            status = "IGNORED"

        return {
            "name":    m.group("name").strip(),
            "tag":     self.current_tag,
            "status":  status,
            "message": (m.group("msg") or "").strip(),
        }
