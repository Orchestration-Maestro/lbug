# Restricted projection filesystem: E01a

This is the first native safety slice, **not a qualified engine isolation boundary yet**.
Maestro must not adopt restricted mode until every E01 slice and E02 have landed and the
combined candidate has passed E07a qualification. The existing path constructor remains
unrestricted and is not suitable for projection isolation.

## Root contract

`RootDirectory::open(existing_absolute_root)` canonicalizes the caller's existing root once.
Links in the root's own path are resolved then; that is the caller's trust decision, including
macOS `/tmp` and `/var` and symlinked homes. The canonical path is walked component by component
with no-follow, and every ancestor is held with its identity recorded.

After open, replacement of any held ancestor above or at the root is detected by identity
revalidation before an operation, which fails closed. Below the root, a child is one validated
plain component: no separators, `.` or `..`, empty string or NUL. Links at any depth,
unexpected object kinds and hard-link aliases are refused. There is no recursive directory
adoption. Invalid names are refused before filesystem calls.

`Database::new_rooted(&root, basename, config)` uses the held root and the plain basename;
it does not expand home/search paths or reopen the ambient database path. Capabilities may
outlive the caller's root wrapper. Handle I/O stays handle-based.

The directory must be application-owned and inaccessible to other principals. Revalidation
catches deterministic replacement; it is not an atomic compare-and-open against arbitrary
same-principal external tools. Such tools bypassing the ownership boundary are unsupported.

## Live operations and closed seams

| Operation | E01a disposition | Callers / next slice |
| --- | --- | --- |
| Local `openFile` read/write/create/truncate | Unix held-dirfd `fstatat`/`openat`; regular single-link identity checked before content access; truncation delayed until validation and lock acquisition | `storage/file_handle.cpp`, `storage/shadow_file.cpp`, `storage/wal/{wal,wal_replayer}.cpp`, `storage/checkpointer.cpp` |
| Startup database-directory probe | Restricted regular-file probe refuses directory/link/alias; absence allowed only for writable create | `main/database.cpp`, `validatePathInReadOnly`; narrow `fileOrPathExists` is also used by startup/WAL/lock discovery |
| Native read/write locks | `fcntl` on the safely opened descriptor, with no path reopen; requested truncation occurs only after lock acquisition | `storage/file_handle.cpp`, `storage/checkpointer.cpp`, `storage/wal/wal_replayer.cpp` |
| Root / remembered leaf replacement | Held ancestor identities and observed child identities revalidated; disappearance/replacement refused | Later supported name mutations must update the capability's identity ownership explicitly |
| Rename (including base fallback), copy/overwrite, createDir, remove, glob, expandPath | Refused in restricted mode, naming the unsupported operation; no ambient fallback | Later E01 slices |
| Alternative VFS registration/dispatch | Refused in restricted mode; only local rooted handles supported | Later E01 dispatch slice |
| Spill activation and writable WAL/shadow recovery | Eager spiller creation disabled only in rooted mode; explicit spill activation and writable recovery with pending sidecars refused | Next E01 spill/checkpoint/delete/rename slice |
| Windows root capability / restricted construction | Unsupported, fails closed before database I/O | E02 |

Canonicalization is only root acquisition. The regular-file probe is not an authorization
for a later pathname open: every open checks again under the held directory.

## Caller audit and remaining direct access

Run `bash scripts/audit-native-filesystem.sh` on the candidate revision. It inventories native
operation calls and direct filesystem/stream calls throughout `lbug-src/src`; review new hits
against this table. This diagnostic scan is not a claim that deferred rows are safe.

- WAL directory durability's direct `open` in `storage/wal/wal_replayer.cpp` remains a later row;
  current restricted remove/rename failure prevents the implemented path from reaching it.
- Spill/checkpoint/delete/rename are the next E01 slice. Rooted mode disables eager spill setup;
  `BufferManager::resetSpiller` refuses activation explicitly without a temp file. Existing
  unrestricted spill behavior is unchanged. Rooted `removeFileIfExists` always throws, so
  successful startup is the behavioral oracle that it did not call deletion (no production
  counting hook). Tests snapshot sibling entries and the root parent's names/sizes/mtimes;
  the application-owned root itself is intentionally excluded because its contents change.
- E01a exercises an empty database create/read-only reopen and sidecar handle opens, plus a
  tiny SQL write with checkpoint disabled and a read-only WAL reopen seeing that row. Pending
  WAL/shadow writable recovery is refused at startup; explicit checkpoint reaches a closed
  unsupported primitive. Full checkpoint/crash recovery, companion deletion/enumeration,
  immutable publication and directory durability remain unqualified later slices.
- Extension dynamic loads/recursive uninstall, COPY/import/export, ATTACH/DETACH and direct NPY
  mapping still require their dedicated restricted refusals. Do not expose those queries in an
  interim deployment. Static `LocalFileSystem::fileExists` remains an unrestricted API for the
  legacy binder; the later query-refusal slice must prevent that path in restricted mode.
- Existing file read/write/seek/truncate/size/sync methods use the validated handle. Full WAL
  directory sync and publication durability are deferred, not inferred from these file checks.

## Tests

`cmake -S tests/native_safety -B target/native-safety -DCMAKE_BUILD_TYPE=Release`,
`cmake --build target/native-safety --parallel 3`, and
`ctest --test-dir target/native-safety --output-on-failure` exercise the native operations.
Rust constructor tests run with `cargo test --no-default-features native_safety_tests` using
bundled source (`LBUG_BUILD_FROM_SOURCE=1`). The three-OS workflow runs native tests, including
Windows refusal. Source-default and cache qualification belong to E03/E03b; `build.rs` is
unchanged in E01a.
