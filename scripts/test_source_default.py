#!/usr/bin/env python3
"""Build an install-like external consumer with a denying, counting proxy.

Run from any directory. Rust inputs are fetched before the trap is enabled.
All generated sources, native outputs and caches live outside the repository.
"""

import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import socketserver
import subprocess
import tempfile
import threading
import time
from typing import cast


class DenyProxy(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, attempt_log):
        self.lock = threading.Lock()
        self.attempt_log = attempt_log
        self.attempts = 0
        super().__init__(("127.0.0.1", 0), DenyRequest)


class DenyRequest(socketserver.BaseRequestHandler):
    def handle(self):
        server = cast(DenyProxy, self.server)
        with server.lock:
            with server.attempt_log.open("a", encoding="utf-8") as log:
                log.write("denied network connection\n")
            server.attempts += 1
        self.request.sendall(b"HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")


def stop_tree(process):
    if os.name == "nt":
        subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"], check=False)
    else:
        os.killpg(process.pid, signal.SIGKILL)
    process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--repository", type=Path, help="Optional historical downloader control checkout")
    args = parser.parse_args()
    evidence = args.evidence.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    fixture_repository = Path(__file__).resolve().parents[1]
    repository = (args.repository or fixture_repository).resolve()
    env = os.environ.copy()
    for name in list(env):
        if name.startswith("LBUG_") or name in ("DOCS_RS", "CARGO_TARGET_DIR", "CARGO_ENCODED_RUSTFLAGS", "RUSTFLAGS"):
            env.pop(name)
    env["CARGO_BUILD_JOBS"] = "3"
    # Do not let a shared compiler cache stand in for this cold native build.
    env["RUSTC_WRAPPER"] = ""
    env["CCACHE_DISABLE"] = "1"
    env["SCCACHE_DISABLE"] = "1"
    env["CARGO_PROFILE_DEV_DEBUG"] = "0"
    with tempfile.TemporaryDirectory(prefix="lbug-source-default-") as temporary:
        root = Path(temporary).resolve()
        assert not root.is_relative_to(repository)
        shutil.copytree(repository, root / "fork", ignore=shutil.ignore_patterns(".git", ".cache", "target", "__pycache__"))
        consumer = root / "consumer"
        shutil.copytree(fixture_repository / "tests/source-default", consumer, ignore=shutil.ignore_patterns("target"))
        manifest = consumer / "Cargo.toml"
        manifest.write_text(manifest.read_text(encoding="utf-8").replace('path = "../.."', 'path = ' + json.dumps((root / "fork").as_posix())), encoding="utf-8")
        env["CARGO_HOME"] = str(root / "cargo-home")
        env["CARGO_TARGET_DIR"] = str(root / "target")
        env["CCACHE_DIR"] = str(root / "ccache")
        env["SCCACHE_DIR"] = str(root / "sccache")
        command = ["cargo", "fetch", "--locked", "--manifest-path", str(manifest)]
        with (evidence / "prefetch.log").open("w", encoding="utf-8") as log:
            log.write("Command: " + repr(command) + "\n")
            log.flush()
            subprocess.run(command, cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
        attempt_log = evidence / "network-attempts.log"
        attempt_log.write_text("", encoding="utf-8")
        with DenyProxy(attempt_log) as proxy:
            thread = threading.Thread(target=proxy.serve_forever, daemon=True)
            thread.start()
            address = f"http://127.0.0.1:{proxy.server_address[1]}"
            for name in ("HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY", "http_proxy", "https_proxy", "all_proxy"):
                env[name] = address
            env["NO_PROXY"] = env["no_proxy"] = ""
            env["CARGO_NET_OFFLINE"] = "true"
            command = ["cargo", "run", "--offline", "--locked", "-vv", "--manifest-path", str(manifest)]
            with (evidence / "build.log").open("w", encoding="utf-8") as log:
                log.write("Command: " + repr(command) + "\n")
                log.write("cwd outside repository; all LBUG_* and DOCS_RS unset; empty CARGO_HOME and target before prefetch\n")
                log.flush()
                process = subprocess.Popen(command, cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=os.name != "nt")
                deadline = time.monotonic() + 3600
                while process.poll() is None:
                    if proxy.attempts or time.monotonic() > deadline:
                        stop_tree(process)
                        break
                    time.sleep(0.1)
                status = process.wait()
            proxy.shutdown()
            thread.join()
            count = proxy.attempts
        (evidence / "attempt-count.txt").write_text(f"{count}\n", encoding="utf-8")
        assert count == 0, f"automatic fetch attempted: {count}; see {attempt_log}"
        assert status == 0, f"external consumer failed: {status}; see build.log"
        caches = list((root / "target").glob("debug/build/lbug-*/out/build/CMakeCache.txt"))
        assert caches, "no bundled CMake compilation evidence"
        assert any(f"CMAKE_HOME_DIRECTORY:INTERNAL={(root / 'fork/lbug-src').as_posix()}" in cache.read_text(encoding="utf-8") for cache in caches), "CMake did not build the bundled source"
        for index, cache in enumerate(caches):
            shutil.copyfile(cache, evidence / f"CMakeCache-{index}.txt")
        assert "bundled source linked; RETURN 1 succeeded" in (evidence / "build.log").read_text(encoding="utf-8"), "missing native runtime proof"
        print("PASS: external consumer compiled bundled C++, linked and queried; network attempts = 0")


if __name__ == "__main__":
    main()
