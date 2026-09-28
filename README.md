# lbug, patched for Maestro

This repository carries [`lbug`](https://crates.io/crates/lbug), the Rust
binding of [LadybugDB](https://github.com/LadybugDB/ladybug), with one small
patch that Maestro needs. The code is upstream's, under its MIT licence
(`LICENSE`); bundled third-party sources keep their own licences under
`lbug-src/third_party`.

- Upstream binding: <https://github.com/LadybugDB/ladybug-rust>
- Upstream engine: <https://github.com/LadybugDB/ladybug>
- Consumer: `Orchestration-Maestro/maestro-core`, through
  `[patch.crates-io] lbug = { git = "...", rev = "<commit>" }`

## Branch layout

Branch `patched-<version>` holds two commits:

1. the published crate, unmodified: the contents of `lbug-<version>.crate`
   from crates.io, plus the binding's `LICENSE`, which the crate leaves out;
2. the patch below.

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

## Moving to a new upstream version

1. Download `lbug-<new>.crate` from crates.io and record its sha256.
2. Create `patched-<new>` from an empty tree, extract the crate into it, add
   `LICENSE` from the binding commit named in `.cargo_vcs_info.json`, and
   commit that as the import.
3. Cherry-pick the patch commit from the previous branch and resolve any
   conflict in the six files it touches.
4. In maestro-core, update the `lbug` version and the `rev` of the patch.

If upstream ships an OpenSSL-free option, drop this repository and use the
crates.io release directly.
