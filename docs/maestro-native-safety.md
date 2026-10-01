# Restricted projection filesystem: E01a / E01b

These are native safety slices, **not a qualified engine isolation boundary yet**.
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

The directory must be application-owned and inaccessible to other principals. Writable opens
use an **unpublished session directory**, supplied by the application; this API does not infer
publication state from a pathname. Revalidation catches deterministic replacement; it is not
an atomic compare-and-open against arbitrary same-principal external tools. Such tools
bypassing the ownership boundary are unsupported.

## Live operations and closed seams

| Operation | E01a / E01b disposition | Callers / remaining slice |
| --- | --- | --- |
| Local `openFile` read/write/create/truncate | Unix held-dirfd `fstatat`/`openat`; regular single-link identity checked before content access; truncation delayed until validation and lock acquisition; newly created names directory-synced | `storage/file_handle.cpp`, shadow, WAL, checkpointer |
| Startup and `fileOrPathExists` | Rooted no-follow regular-file probe; unsafe objects/errors are not absence; every later open rechecks | Database startup, WAL, checkpoint lock discovery; static legacy `fileExists` remains deferred to query refusals |
| Native read/write locks | `fcntl` on the validated descriptor; rooted writable startup acquires and retains the database write lock before companion adoption or eager spill cleanup, then transfers that same descriptor into the data FileHandle after shadow replay | File handles and checkpoint locks; refused second writers preserve live bytes/dev/ino |
| Root / remembered leaf replacement | Held ancestor identities and observed child identities revalidated; external disappearance/replacement refused | Supported rename/unlink explicitly transfer or revoke identity ownership |
| Rename, including base `FileSystem` | Same held root/name pair in base, local and virtual FS; owned database/exact-companion source; atomic no-overwrite publication; replacement only of an already owned, identity-validated exact companion | WAL rotation; graph/partition rename remains refused by ownership |
| `removeFileIfExists` | Exact owned companion or registered generated temp; anchored single-entry `unlinkat`, never recursive removal or extension-directory escape; missing exact companions are idempotent | WAL/shadow/checkpoint locks, spiller and bulk PK validation |
| Spill and bulk-insert temp | Writable spill activation/reset under the held root; generated PK temps allocated create-new, registered by identity and unlinked through the same rooted removal | Forced-pressure spill/reload; operator's real no-index PK validator; see loader limitation below |
| Checkpoint and writable WAL/shadow recovery | Enabled on Unix; adoption is under the startup write lock; recoverable shadows remain until every checkpoint target's data-file sync completes and the checkpoint WAL is durably removed; read-only startup never adopts or creates sidecars | Checkpoint/rollback, held-handle WAL/shadow replay and cleanup crash cuts |
| File / directory durability | File I/O/sync uses validated descriptors; every supported name creation/rename/unlink syncs the held directory, revalidating ancestors before and after sync; WAL replay uses that handle, with no pathname reopen or ambient `.` fallback | Durability/identity errors throw, not success; directory-sync failure permanently poisons the capability |
| Copy/overwrite, createDir, glob, expandPath | Still refused in restricted mode; no ambient fallback. These flows need no directory-creation primitive | Later E01 slices |
| Alternative VFS registration/dispatch | Still refused; only local rooted handles supported | Later E01 dispatch slice |
| Windows capability / restricted construction | Unsupported, fails closed before database I/O | E02 |

### Companion ownership and publication

The fixed companion set is derived from `StorageUtils::getCompanionFilePaths` and its existing
name helpers: `<db>.wal`, `.wal.checkpoint`, `.shadow`, `.tmp`, `.checkpoint.intent.lock` and
`.checkpoint.apply.lock`. No wildcard, graph stem, prefix or extension-directory permission
participates in rooted deletion. `.lock`, `.checkpoint` and unrelated/tagged stems are not
implicitly added to this set. Graph/partition children remain unsupported for rooted name
mutation; a request outside the session's supported names refuses.

Rename sources must be the selected database basename or one of its six exact companions,
as well as owned. An owned graph child, another database stem, or another arbitrary newly
created name is not a source permission. This keeps graph/partition child mutations refused.

A probe or an open of an existing file alone does not grant mutation ownership. A newly
created rooted file is owned by that capability. Writable recovery explicitly adopts only the
fixed companion names, under the database write lock and after no-follow
regular/single-link/remembered-identity checks and an owner match with the held root. Rename validates both endpoints immediately before the
anchored operation, then explicitly updates the identity and ownership maps. Replacement
is allowed only for an exact companion already owned by that capability. Publication into
any other existing destination refuses, even if that destination is owned.

Absent-destination rename uses Linux `renameat2(RENAME_NOREPLACE)` or macOS
`renameatx_np(RENAME_EXCL)`. **Kernel and filesystem support is required.** Unsupported
`EINVAL`, `ENOSYS`, `ENOTSUP`, or other syscall failures raise `IOException`; there is no
check-then-rename fallback. Directory sync failures likewise return an error; callers must
not treat that operation as a successful durable publication.

A capability has one shared directory-durability state: `UNSYNCED`, `SYNCED`, or `POISONED`.
It starts unsynced. Its first durable file sync establishes held-directory durability first;
successful name-change directory sync also establishes that state. Later file syncs do not
repeat the initial directory sync. Any directory-sync error, including identity failure
before/after that sync, permanently poisons the capability. Fault removal is not a retry
permission: every later rooted name/probe/open operation and every LocalFileSystem file sync,
including already-held WAL handles, refuses. File sync checks the shared capability before
I/O and again before returning success, so poison arriving during that sync is refused too.
Recovery requires a fresh capability, which must independently establish directory durability
before its first durable file sync. State is not copied per file handle.

### Rooted startup and checkpoint ordering

Writable rooted startup opens and locks the database before adopting companions or constructing
BufferManager (whose eager Spiller removes a stale `.tmp`). Shadow recovery uses that locked
FileInfo; FileHandle initialization takes ownership only after replay, so its page count reflects
the recovered file. Reinitialization retains that handle. A second writer's lock refusal occurs
before adoption or deletion. Unrestricted startup retains upstream's adoption-free, buffer-manager-
before-data-lock ordering; unrestricted shadow replay and partition ordering are unchanged.

Rooted checkpoint application keeps the durable shadow intact while its committed WAL marker can
require replay. After all target data-file syncs, `postCheckpointCleanup` removes and directory-syncs
the checkpoint WAL before resetting/truncating/unlinking the shadow. Thus either side of the shadow
unlink, including an unlink EIO, is recoverable with a fresh capability. Unrestricted checkpoint
and partition-child cleanup retain upstream ordering.

### Generated bulk temp files

The name helper lives in `StorageUtils`: `<db>.pk_validator.<counter>.tmp`. Only the engine's
allocator generates these names, creates them with anchored `O_CREAT|O_EXCL`, checks their
identity, and registers them for this capability. Existing entries are never opened/adopted
by the allocator: `EEXIST` advances the counter, with 16 attempts per allocation before a
typed refusal. A normal rooted open or a pattern match does not register a generated temp.
Removal requires its registration and remembered identity; successful unlink revokes both.

**Crash-left generated temps leak safely.** A later session never adopts or automatically
removes them. Receipt-backed anchored cleanup belongs to E10. The six fixed companions have
the distinct, explicit writable-recovery adoption contract above.

## Caller audit and remaining direct access

Run `bash scripts/audit-native-filesystem.sh` on the candidate revision. The diagnostic scan
includes direct Unix rename-family/syscall calls; new hits require operation/caller review.
The scan has **630 hits**, versus 629 at the E01b baseline and 616 at the E01a baseline.
The new hit is the rooted startup database lock open; all scans exit 0. Full candidate output
and the exact command are recorded in the E01b fix report. This scan does not qualify deferred
rows.

- WAL replay's former direct directory `open` moved behind `FileSystem::syncDirectoryForFile`.
  Its unrestricted implementation retains legacy behavior; rooted calls select the held
  directory before that branch. No restricted remove/rename path reopens a parent pathname.
- The tests drive the actual `NodeBatchInsert` no-index PK validator through its internal
  operator constructor, using real chunks and a one-byte spill threshold. The factory is in
  the internal `batch_insert` namespace, not a new exported/public API or a test-only branch.
  Prepared ordinary writes map to `Insert`; `NodeBatchInsert` is reached by the COPY planner.
  Rooted COPY's glob remains refused. **E07b must prove the prepared bound loader end to end**;
  this operator-level bulk test is not that qualification.
- Extension dynamic loads/recursive uninstall, COPY/import/export, ATTACH/DETACH and direct
  NPY mapping still require E01c's query refusals. Do not expose these queries in an interim
  deployment. Static `LocalFileSystem::fileExists` remains unrestricted for the legacy binder.
- Copy/overwrite, directory creation, query refusals, alternative-VFS dispatch, Windows safety
  and `build.rs` are unchanged by E01b.

## Tests

Configure `tests/native_safety` into `target/native-safety`, build with `--parallel 3`, and run
CTest with `--verbose --output-on-failure`. The three executables cover E01a opens/locks and
closed seams, E01b rename/remove/adoption/durability operations, and the full native flows.
Outside sentinel bytes and identity are checked. Tests exercise ancestor swaps, links,
existing destinations, aliases, unrelated stems, directories, replaced/disappeared identities,
atomic destination races, syscall/durability failures and generated-temp collisions/cleanup.

Flow tests exercise forced-memory-pressure spill/reload/reset/close, actual bulk PK spill and
duplicate-run failure cleanup, rollback, repeated/automatic/default-close checkpoints,
read-only WAL replay without writable adoption, and `SIGKILL`/fresh-capability reopen after
WAL commit, WAL rename, checkpoint-record sync (before shadow application), and completed
checkpoint, plus immediately before and after shadow unlink. The crash oracle blocks on a
ready pipe (never SIGSTOP) before the parent kills the child; it records remaining WAL/shadow sizes
without asserting a particular layout. Shadow-unlink EIO is followed by writable recovery,
another checkpoint and read-only reopen. A refused second writer preserves a real live spill and
all file snapshots; database bytes are read through the held handle because opening/closing another
fd for that inode would release POSIX record locks. Tests also require a single data-file open
through shadow recovery and retained FileHandle initialization. Read-only opens preserve entries
and create no sidecars. The existing `CALL spill_to_disk`
setter already refuses enabling spill for read-only databases. The native buffer-manager
reset additionally retains the constructor's read-only mode: non-empty rooted read-only
reset refuses before constructing a spiller, empty disable is allowed, and writable rooted
reset remains live. Unrestricted reset behavior remains upstream. Tests force read-only
memory exhaustion after refusal and assert that no directory entry appeared.

Only the native CMake harness compiles deterministic interleave/syscall-fault/crash callbacks;
ordinary bundled builds do not include them. Reused native archives leave callbacks inert
unless the harness installs them. Rust constructor tests additionally exercise writable
recovery, rollback, checkpoint and read-only reopen using the same source-built archive.
On Linux, `maestro_rooted_sidecars` uses `-Wl,--wrap=fsync` to observe the actual directory-sync
call, verify its descriptor's dev/ino, return EIO there (the pre-call hook succeeds), and prove
permanent refusal including an already-held WAL handle. The three-OS workflow runs the native
and Rust suites; Windows still proves refusal.
