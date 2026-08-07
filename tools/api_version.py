"""Checks that pyproject.toml repeats the API version in include/receipet.h.

That version describes the C API and only a human changes it.  What the
packages ship as is the release version, which is dated and generated per
publish - see .github/scripts/release_version.py.

    make version-check
"""

import re
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HEADER = ROOT / "include" / "receipet.h"
PYPROJECT = ROOT / "pyproject.toml"


def api_version():
    text = HEADER.read_text(encoding="utf-8")
    parts = []
    for part in ("MAJOR", "MINOR", "PATCH"):
        match = re.search(rf"^#define\s+RCPT_VERSION_{part}\s+(\d+)\s*$", text, re.MULTILINE)
        if match is None:
            raise SystemExit(f"receipet.h: RCPT_VERSION_{part} not found")
        parts.append(match.group(1))
    return ".".join(parts)


def main():
    expected = api_version()
    found = tomllib.loads(PYPROJECT.read_text(encoding="utf-8"))["project"]["version"]
    if found != expected:
        print(f"version mismatch - receipet.h says {expected}, pyproject.toml says {found}", file=sys.stderr)
        return 1
    print(f"API version {expected}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
