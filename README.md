# lbug, patched for Maestro

This repository carries [`lbug`](https://crates.io/crates/lbug), the Rust
binding of [LadybugDB](https://github.com/LadybugDB/ladybug), with the
patches that Maestro needs. The code is upstream's, under its MIT licence
(`LICENSE`); bundled third-party sources keep their own licences under
`lbug-src/third_party`.

- Upstream binding: <https://github.com/LadybugDB/ladybug-rust>
- Upstream engine: <https://github.com/LadybugDB/ladybug>
- Consumer: `Orchestration-Maestro/maestro-core`, through
  `[patch.crates-io] lbug = { git = "...", rev = "<commit>" }`

## Branch layout

Branches are named `build/patched-<version>` (the organization's branch-name
rules require a prefix such as `build/`). Each holds:

1. the published crate, unmodified: the contents of `lbug-<version>.crate`
   from crates.io, plus the binding's `LICENSE`, which the crate leaves out;
2. the OpenSSL patch below;
3. the build-reuse commits below.

## The patch

A Cargo feature, `extension_installer`, on by default as upstream behaves.
With default features off:

- the CMake option `LBUG_EXTENSION_INSTALLER=OFF` skips
  `find_package(OpenSSL 3 REQUIRED)` and builds the installer without
  `CPPHTTPLIB_OPENSSL_SUPPORT`;
- `ExtensionInstaller::install` (the `INSTALL` statement) fails with a clear
  error instead of downloading, over HTTPS or plain HTTP;
- `build.rs` no longer links `ssl` and `crypto` (`libssl`/`libcrypto` on
  Windows).

The engine itself never used OpenSSL: only the installer's downloads do. The
approach follows upstream pull requests
[#777](https://github.com/LadybugDB/ladybug/pull/777) and
[#796](https://github.com/LadybugDB/ladybug/pull/796) (optional OpenSSL,
closed unmerged), with an explicit switch instead of detection, so a build
never downgrades to HTTP silently.

For a static link, `build.rs` also sets `BUILD_SHARED_LBUG=OFF`: the shared
library, the largest link of the build, is never used then. This is the
configuration upstream's WebAssembly build already uses.

## Build reuse

The C++ engine takes 13 to 18 minutes to build, and Cargo rebuilds it
whenever the fingerprint of the crate changes: other features, other
`RUSTFLAGS` (coverage, mutation testing), `-p` against `--workspace`.

- With `LBUG_REUSE_CMAKE_BUILD` set, the CMake build lives in
  `<profile>/build/lbug-cmake-<hash>`, the hash naming the source path,
  version, link mode, installer feature, target, profile and C/C++ compiler
  settings. A finished build there is reused without running CMake, which
  also holds after a CI cache restore, where fresh source timestamps would
  make `make` rebuild everything. It then deletes its object files: the
  archives hold them all. Opt-in, for sources that never change in place (a
  registry or git checkout).
- `liblbug` links `-bundle`: a debug archive is 2.6 GB, and bundling copied
  it into every rlib of the crate, with about 7 GB of memory.

## Source-default builds

The fork builds bundled C++ by default, including for external consumers.
Automatic prebuilt and source downloaders have been removed; missing local
sources fail rather than fetch. Existing explicit local source/library
paths and the `DOCS_RS` no-link accommodation remain supported. No-link
lint/docs artifacts are not release or runtime evidence.

Run the cold, external consumer regression with Python 3.12 or newer:

```sh
python scripts/test_source_default.py --evidence /tmp/source-default-evidence
```

The fixture pre-fetches locked Rust dependencies into a fresh Cargo home,
then builds outside this repository with all `LBUG_*`/`DOCS_RS` variables
unset, an empty native target and an HTTP(S) proxy that denies and counts
connections. It requires zero attempts, bundled CMake compilation and a
linked `RETURN 1` query. Transcripts and counters are retained; temporary
sources and outputs are deleted. Fork CI runs a historical-downloader red
control plus this green proof on Linux, macOS and Windows.

## Moving to a new upstream version

1. Download `lbug-<new>.crate` from crates.io and record its sha256.
2. Create `patched-<new>` from an empty tree, extract the crate into it, add
   `LICENSE` from the binding commit named in `.cargo_vcs_info.json`, and
   commit that as the import.
3. Cherry-pick the patch commits from the previous branch and resolve any
   conflict (they touch `build.rs`, the two manifests, two `CMakeLists.txt`
   and `extension_installer.cpp`).
4. In maestro-core, update the `lbug` version and the `rev` of the patch.

If upstream ships an OpenSSL-free option, drop this repository and use the
crates.io release directly.
