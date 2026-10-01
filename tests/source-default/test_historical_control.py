"""The historical locked consumer must reach (and trip) the network trap."""

import argparse
import subprocess
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--cargo-runner", type=Path)
    args = parser.parse_args()
    script = Path(__file__).resolve().parents[2] / "scripts/test_source_default.py"
    command = [sys.executable, str(script), "--repository", str(args.repository), "--evidence", str(args.evidence)]
    if args.cargo_runner:
        command += ["--cargo-runner", str(args.cargo_runner)]
    result = subprocess.run(command, check=False)
    assert result.returncode != 0, "historical downloader unexpectedly passed"
    counter = args.evidence / "attempt-count.txt"
    assert counter.is_file(), "historical lock failed before reaching the network trap"
    assert int(counter.read_text()) > 0, "historical downloader never reached the network trap"
    print("PASS: historical locked consumer reached and tripped the network trap")


if __name__ == "__main__":
    main()
