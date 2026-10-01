"""E01b regression: a changed external library directory must rerun build.rs."""

import argparse
import json
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--cargo-runner", type=Path)
    args = parser.parse_args()
    repository = Path(__file__).resolve().parents[2]
    evidence = args.evidence.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    cargo = ([str(args.cargo_runner)] if args.cargo_runner else []) + ["cargo"]
    source = (repository / "Cargo.toml").read_text()
    dependencies = source[source.index("[build-dependencies.cc]"):source.index("[lints.clippy]")]
    with tempfile.TemporaryDirectory(prefix="lbug-build-environment-") as temporary:
        root = Path(temporary)
        fixture = root / "fixture"
        (fixture / "src").mkdir(parents=True)
        (fixture / "src/lib.rs").write_text("// No-link environment/freshness fixture.\n")
        (fixture / "Cargo.toml").write_text(
            '[package]\nname = "native-cache-fixture"\nversion = "0.0.0"\nedition = "2021"\nbuild = '
            + json.dumps((repository / "build.rs").as_posix())
            + '\n[workspace]\n[features]\narrow = []\nextension_installer = []\n'
            + dependencies
        )
        shutil.copyfile(repository / "tests/native-cache/Cargo.lock", fixture / "Cargo.lock")
        env = os.environ.copy()
        env.update(DOCS_RS="1", CARGO_BUILD_JOBS="3", CARGO_TARGET_DIR=str(root / "target"))

        def run(name):
            command = cargo + ["check", "--locked", "--offline", "-vv", "--manifest-path", str(fixture / "Cargo.toml")]
            with (evidence / f"{name}.log").open("w") as log:
                log.write("Command: " + repr(command) + "\n")
                log.flush()
                status = subprocess.call(command, env=env, stdout=log, stderr=subprocess.STDOUT)
            text = (evidence / f"{name}.log").read_text()
            assert status == 0, f"{name} failed: {status}; see transcript"
            runs = sum(bool(re.search(r"Running `.*[/\\]native-cache-fixture-[^/\\]+[/\\]build-script-build(?:\.exe)?`$", line)) for line in text.splitlines())
            print(f"{name}: exit={status}, build-script invocations={runs}", flush=True)
            return text, runs

        env["LBUG_LIBRARY_DIR"] = str(root / "library-a")
        first, first_runs = run("first")
        assert first_runs == 1, "cold build did not execute the real build script"
        env["LBUG_LIBRARY_DIR"] = str(root / "library-b")
        changed, changed_runs = run("changed-library-directory")
        assert changed_runs == 1, "E01b: changing LBUG_LIBRARY_DIR did not rerun build.rs"
        _, unchanged_runs = run("unchanged")
        assert unchanged_runs == 0, "unchanged environment reran build.rs"
        emitted = set(re.findall(r"cargo:rerun-if-env-changed=([^\s]+)", changed))
        reads = set(re.findall(r"native-build env-read: ([^\s]+)", changed))
        assert emitted == reads, f"emitted/read sets differ: emitted-only={emitted - reads}, read-only={reads - emitted}"
        required = {"LBUG_LIBRARY_DIR", "LBUG_INCLUDE_DIR", "LBUG_SOURCE_DIR", "LBUG_NATIVE_CACHE_DIR", "LBUG_SHARED", "LBUG_REUSE_CMAKE_BUILD", "DOCS_RS", "CC", "CXX", "CFLAGS", "CXXFLAGS", "CMAKE_TOOLCHAIN_FILE"}
        assert required <= emitted, f"missing tracked variables: {required - emitted}"
        (evidence / "emitted-read-set.json").write_text(json.dumps(sorted(emitted), indent=2) + "\n")
        # All project-owned environment reads must use the checked declaration
        # list; bypassing its wrapper would evade the emitted/read-set check.
        declarations = (repository / "build_support/build_env.rs").read_text()
        declared = set(re.findall(r'\("([A-Z][A-Z0-9_]*)", Kind::', declarations))
        for path in [repository / "build.rs", *sorted((repository / "build_support").glob("*.rs"))]:
            if path.name == "build_env.rs":
                continue
            text = path.read_text()
            literal_reads = set(re.findall(r'(?:env::(?:var|var_os)|target_env|env!)\s*\(\s*"([A-Z][A-Z0-9_]*)"', text))
            for literals in re.findall(r'for var in\s*\[([^]]+)\]', text):
                literal_reads.update(re.findall(r'"([A-Z][A-Z0-9_]*)"', literals))
            assert literal_reads <= declared, f"undeclared literal reads in {path.name}: {literal_reads - declared}"
            assert not re.search(r"std::env::(?:var|var_os|vars|vars_os)\s*\(", text), f"unchecked environment read in {path.name}"
            assert not re.search(r"use\s+std::(?:\{[^;]*\benv\b|env\b)", text), f"unchecked environment import in {path.name}"
        print("PASS: changed library directory reruns build.rs; unchanged inputs stay fresh; emitted set equals read set", flush=True)


if __name__ == "__main__":
    main()
