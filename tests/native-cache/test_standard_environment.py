"""Check the unsanitized runner environment and retain its native flag inputs."""

import argparse
import json
import os
import re
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", type=Path, required=True)
    args = parser.parse_args()
    names = {"CFLAGS", "CXXFLAGS", "CPPFLAGS", "ASMFLAGS", "LDFLAGS", "ARFLAGS", "RANLIBFLAGS", "CC_SHELL_ESCAPED_FLAGS"}
    names.update(name for name in os.environ if re.match(r"(?:(?:HOST|TARGET)_)?(?:C|CXX|CPP|ASM|LD|AR|RANLIB)FLAGS(?:$|_)", name))
    flags = {name: os.environ.get(name) for name in sorted(names)}
    (args.evidence / "normal-flag-inputs.json").write_text(json.dumps(flags, indent=2) + "\n")
    print("Standard runner flag inputs: " + json.dumps(flags))
    text = (args.evidence / "normal-environment.log").read_text()
    assert "native-cache environment:" in text, "missing per-variable presence table"
    assert "native cache disabled by " not in text, "standard runner triggered an arbitrary-file bypass"
    if os.name == "nt":
        assert "cache ownership cannot be verified on this platform; building from source" in text
        assert "native cache hit:" not in text and "native cache published:" not in text
    else:
        assert "native cache hit:" in text and "native cache published:" in text
    print("PASS: standard environment did not bypass; platform cache policy holds")


if __name__ == "__main__":
    main()
