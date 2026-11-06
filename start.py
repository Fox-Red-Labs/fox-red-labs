#!/usr/bin/env python3
"""One command from a clone to a checked result, on macOS, Linux or Windows.

  macOS, Linux:  ./start.sh mspm0l1306 blink          (start.sh runs this file)
  Windows:       py start.py mspm0l1306 blink         (PowerShell or Command Prompt)

A sparse clone (the one README.md gives) holds only the top-level files. This adds the tool and the
board's notes from the same clone, folders of this repository and no new software, then hands every
argument to tools/frl/start.py (`py start.py --help` lists them). Standard library only, Python 3.9+.
"""
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
TOOL = HERE / "tools" / "frl" / "start.py"
DIRS = ["tools/frl", "tools/benchmark", "data", "site/data"]


def git(*args):
    return subprocess.run(["git", "-C", str(HERE), *args], capture_output=True, text=True)


def add_folders(argv):
    """Fill a sparse clone; return an exit status, or None when the tool is ready."""
    try:
        sparse = git("sparse-checkout", "list").returncode == 0
    except OSError:
        sparse = False
    if not sparse:
        print(f"tools/frl is missing from {HERE}. Clone the whole repository, or copy tools/frl beside this file.")
        return 2
    dirs = list(DIRS)
    board = argv[0] if argv and not argv[0].startswith("-") else ""
    if board and git("cat-file", "-e", f"HEAD:{board}/tools").returncode == 0:
        dirs.append(f"{board}/tools")
    print("This copy has only the top-level files. Adding the folders the tool needs from this repository")
    print("(no new software, about 25 MB): " + " ".join(dirs))
    r = subprocess.run(["git", "-C", str(HERE), "sparse-checkout", "add", *dirs])
    if r.returncode != 0:
        print("Could not add them. Run: git sparse-checkout add " + " ".join(dirs))
        return 2
    print()
    return None


def main(argv):
    if sys.version_info < (3, 9):
        print(f"This needs Python 3.9 or later; this is {sys.version.split()[0]}. Install a newer one "
              "(docs/install-routes.md, \"Your computer\").")
        return 2
    if not TOOL.is_file():
        status = add_folders(argv)
        if status is not None:
            return status
    # A child process, not exec: on Windows os.exec* detaches from the console and loses the exit status.
    return subprocess.call([sys.executable, str(TOOL), *argv])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
