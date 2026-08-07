"""Prints the next release version: YYYY.M.N, N counting that month's releases.

Called only by release.yml.  This is not the API version in
include/receipet.h - see tools/api_version.py.

N is the highest existing N plus one, not the count of tags: a deleted tag
would otherwise reissue a number, and npm accepts a version exactly once.  The
month is the runner's local one, so the workflow sets TZ.
"""

import re
import subprocess
from datetime import datetime


def main():
    now = datetime.now()
    prefix = f"{now.year}.{now.month}."
    tags = subprocess.run(["git", "tag", "--list", f"v{prefix}*"], capture_output=True, text=True, check=True).stdout.split()
    pattern = re.compile(rf"^v{re.escape(prefix)}(\d+)$")
    used = [int(m.group(1)) for tag in tags if (m := pattern.match(tag))]
    print(f"{prefix}{max(used, default=0) + 1}")


if __name__ == "__main__":
    main()
