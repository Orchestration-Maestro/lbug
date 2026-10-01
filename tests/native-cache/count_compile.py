"""CMake compiler launcher: count real compile invocations, not configure probes."""
import os
import subprocess
import sys
from pathlib import Path

with Path(os.environ["COMPILE_LOG"]).open("a", encoding="utf-8") as log:
    log.write("compile\n")
sys.exit(subprocess.call(sys.argv[1:]))
