"""Prove prefetch precedes the offline freshness tests in an empty Cargo home."""

import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--cargo-runner", type=Path)
    args = parser.parse_args()
    script = Path(__file__).with_name("test_build_environment.py")
    with tempfile.TemporaryDirectory(prefix="lbug-cold-cargo-home-") as home:
        env = os.environ.copy()
        env["CARGO_HOME"] = home
        command = [sys.executable, str(script), "--evidence", str(args.evidence)]
        if args.cargo_runner:
            command += ["--cargo-runner", str(args.cargo_runner)]
        result = subprocess.run(command, env=env, check=False)
        assert result.returncode == 0, "cold-home offline freshness checks ran without a successful locked prefetch"
    prefetch = (args.evidence / "prefetch.log").read_text()
    assert "'fetch', '--locked'" in prefetch, "locked prefetch evidence missing"
    for name in ("first", "changed-library-directory", "unchanged"):
        assert "'check', '--locked', '--offline'" in (args.evidence / f"{name}.log").read_text(), "freshness check lost offline/locked mode"
    print("PASS: empty Cargo home prefetched before locked offline freshness checks")


if __name__ == "__main__":
    main()
