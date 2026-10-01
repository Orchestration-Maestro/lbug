#!/usr/bin/env bash
# Diagnostic inventory, not a security gate: new hits need operation/caller review.
set -euo pipefail
cd "$(dirname "$0")/.."
grep -RnE '(openFile|fileOrPathExists|fileExists|renameFile|copyFile|overwriteFile|createDir|removeFileIfExists|glob|expandPath|openat|fstatat|mkdirat|unlinkat|renameat|renameat2|renameatx_np|syscall|open|fopen|freopen|stat|lstat|fstat|access|read|write|pread|pwrite|lseek|truncate|ftruncate|fsync|fdatasync|fcntl|mmap|dlopen|CreateFile[A-W]*|LoadLibrary[A-W]*|LockFileEx|std::filesystem::[a-z_]+)[[:space:]]*\(|std::[iof]+stream[[:space:](]' lbug-src/src --include='*.cpp' --include='*.h'
