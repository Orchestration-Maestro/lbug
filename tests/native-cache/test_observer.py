"""A real compiler check for engine/bridge/dependency invocation accounting."""

import json
import runpy
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

module_path = Path(__file__).resolve().parents[2] / "scripts/test_native_cache.py"
measurement = runpy.run_path(str(module_path))

with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary)
    log = root / "compile.jsonl"
    compiler = shutil.which("c++")
    assert compiler
    observer = root / "observer.py"
    observer.write_text(measurement["OBSERVER"].replace("INTERPRETER", sys.executable).replace("COMPILER", repr(compiler)).replace("LOG", repr(str(log))))
    for name in ("fork/lbug-src/engine.cpp", "cache/.work-test/build/src/codegen/generated.cpp", "target/cxxbridge/bridge.cc", "registry/dummy.cc", "cmake/CMakeCXXCompilerABI.cpp"):
        source = root / name
        source.parent.mkdir(parents=True, exist_ok=True)
        source.write_text("int observer_test() { return 1; }\n")
        subprocess.run([sys.executable, str(observer), "-c", str(source), "-o", str(source.with_suffix(".o"))], check=True)
    observed = [json.loads(line) for line in log.read_text().splitlines()]
    assert [item["kind"] for item in observed] == ["engine", "engine", "bridge", "dependency"], observed
    assert all(item["status"] == 0 and item["end"] >= item["start"] for item in observed)
print("PASS: engine accounting includes generated sources, separates bridges/dependencies and excludes ABI probes")
