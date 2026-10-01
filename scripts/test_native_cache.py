#!/usr/bin/env python3
"""Measure real engine-cache reuse between clean Cargo targets (Unix).

Compiler observers record engine and per-target cxx-bridge compiles separately.
The observers and their configuration live in the copied native source tree;
no repository source or downloaded Cargo source is edited.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

OBSERVER = r'''#!INTERPRETER
import json, os, subprocess, sys, time
from pathlib import Path
args = sys.argv[1:]
source = next((arg for arg in args if arg.endswith((".c", ".cc", ".cpp", ".cxx"))), "")
is_compile = "-c" in args and source and not any(part in source for part in ("CMakeScratch", "CMakeTmp", "CompilerId", "CompilerABI"))
start = time.monotonic()
status = subprocess.call([COMPILER, *args])
if is_compile:
    kind = "bridge" if source.endswith("lbug_rs.cpp") or "cxxbridge" in source else "engine" if any(part in Path(source).as_posix() for part in ("/lbug-src/", "/.work-")) else "dependency"
    with Path(LOG).open("a", encoding="utf-8") as log:
        log.write(json.dumps({"kind": kind, "source": source, "start": start, "end": time.monotonic(), "status": status}) + "\n")
sys.exit(status)
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--cargo-runner", type=Path)
    args = parser.parse_args()
    assert os.name == "posix", "Windows deliberately disables the native cache; run the portable fixture there"
    repository = Path(__file__).resolve().parents[1]
    evidence = args.evidence.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    cargo = ([str(args.cargo_runner)] if args.cargo_runner else []) + ["cargo"]
    env = os.environ.copy()
    for name in list(env):
        if name.startswith("LBUG_") or name in ("DOCS_RS", "CARGO_TARGET_DIR", "RUSTFLAGS", "CARGO_ENCODED_RUSTFLAGS", "CXXFLAGS", "CFLAGS"):
            env.pop(name)
    env.update(CARGO_BUILD_JOBS="3", CARGO_PROFILE_RELEASE_DEBUG="0", SCCACHE_DISABLE="1", CCACHE_DISABLE="1")
    results = {}
    with tempfile.TemporaryDirectory(prefix="lbug-native-cache-") as temporary:
        root = Path(temporary).resolve()
        fork = root / "fork"
        shutil.copytree(repository, fork, ignore=shutil.ignore_patterns(".git", "target", "__pycache__"))
        consumer = root / "consumer"
        shutil.copytree(repository / "tests/source-default", consumer, ignore=shutil.ignore_patterns("target"))
        manifest = consumer / "Cargo.toml"
        manifest.write_text(manifest.read_text().replace('path = "../.."', 'path = ' + json.dumps(fork.as_posix())))
        compile_log = evidence / "compile-invocations.jsonl"
        compile_log.write_text("")
        for variable, compiler in (("CC", "cc"), ("CXX", "c++")):
            executable = shutil.which(compiler)
            assert executable, f"missing {compiler}"
            observer = fork / "lbug-src" / f"cache-observe-{compiler.replace('+', 'x')}.py"
            observer.write_text(OBSERVER.replace("INTERPRETER", sys.executable).replace("COMPILER", repr(executable)).replace("LOG", repr(str(compile_log))))
            observer.chmod(0o700)
            env[variable] = str(observer)
        env["LBUG_NATIVE_CACHE_DIR"] = str(root / "cache")

        def compiles():
            return [json.loads(line) for line in compile_log.read_text().splitlines()]

        def run(name, target):
            env["CARGO_TARGET_DIR"] = str(root / target)
            before = len(compiles())
            command = cargo + ["run", "--release", "--locked", "--offline", "-vv", "--manifest-path", str(manifest)]
            peak = [0]
            done = threading.Event()

            def sample():
                # Read this command's aggregate cgroup peak, not the largest
                # individual compiler's RSS. Retain it before systemd collects.
                while not done.is_set():
                    if Path("/proc").is_dir():
                        for process in Path("/proc").glob("[0-9]*"):
                            try:
                                command_line = (process / "cmdline").read_bytes()
                                if str(manifest).encode() not in command_line:
                                    continue
                                group = (process / "cgroup").read_text().split("0::", 1)[1].strip()
                                value = Path("/sys/fs/cgroup") / group.lstrip("/") / "memory.peak"
                                peak[0] = max(peak[0], int(value.read_text()))
                            except (OSError, IndexError, ValueError):
                                pass
                    done.wait(0.2)

            monitor = threading.Thread(target=sample)
            monitor.start()
            start = time.monotonic()
            with (evidence / f"{name}.log").open("w") as log:
                log.write("Command: " + repr(command) + "\n")
                log.flush()
                status = subprocess.call(command, cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT)
            wall = time.monotonic() - start
            done.set()
            monitor.join()
            observed = compiles()[before:]
            bridges = [item for item in observed if item["kind"] == "bridge"]
            result = {"command": command, "exit": status, "wall_seconds": wall, "engine_compiles": sum(item["kind"] == "engine" for item in observed), "bridge_compiles": len(bridges), "bridge_wall_seconds": max(item["end"] for item in bridges) - min(item["start"] for item in bridges) if bridges else 0, "peak_cgroup_bytes": peak[0] or None}
            results[name] = result
            (evidence / "measurements.json").write_text(json.dumps(results, indent=2) + "\n")
            assert status == 0, f"{name}: failed with {status}; see {name}.log"
            assert "RETURN 1 succeeded" in (evidence / f"{name}.log").read_text()
            print(f"{name}: {json.dumps(result)}", flush=True)
            return result

        command = cargo + ["fetch", "--locked", "--manifest-path", str(manifest)]
        with (evidence / "prefetch.log").open("w") as log:
            log.write("Command: " + repr(command) + "\n")
            log.flush()
            subprocess.run(command, cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
        cold = run("cold", "target-a")
        assert cold["engine_compiles"] > 0
        entries = list((root / "cache").glob("entry-*"))
        assert len(entries) == 1 and (entries[0] / "manifest.sha256").is_file()
        inventory = [path.relative_to(entries[0]).as_posix() for path in entries[0].rglob("*") if path.is_file()]
        assert all(path == "manifest.sha256" or path.startswith("build/src/") for path in inventory)
        assert not any("lbug_rs" in path or "cxxbridge" in path for path in inventory), "bridge outputs leaked into engine cache"
        (evidence / "published-inventory.json").write_text(json.dumps(inventory, indent=2) + "\n")
        warm = run("warm", "target-b")
        assert warm["engine_compiles"] == 0, "warm target rebuilt engine C++"
        assert warm["bridge_compiles"] > 0, "bridge observer missed per-target compiles"
        source = consumer / "src/main.rs"
        source.write_text(source.read_text() + "\n// unrelated Rust-only change\n")
        rust_change = run("rust-only", "target-b")
        (consumer / "README.md").write_text("Unrelated documentation change.\n")
        docs_change = run("docs-only", "target-b")
        for result in (rust_change, docs_change):
            assert result["engine_compiles"] == result["bridge_compiles"] == 0
        results["added_clean_seconds"] = max(0, cold["wall_seconds"] - warm["wall_seconds"])
        (evidence / "measurements.json").write_text(json.dumps(results, indent=2) + "\n")
        assert results["added_clean_seconds"] <= 15 * 60, "added clean-build time exceeds the approved 15-minute ceiling"
        if sys.platform.startswith("linux"):
            assert cold["peak_cgroup_bytes"] and cold["peak_cgroup_bytes"] < 8 * 1024**3, "missing/over-budget aggregate peak memory"
        print("PASS: two clean targets reuse engine output; Rust/docs changes compile neither engine nor bridge", flush=True)


if __name__ == "__main__":
    main()
