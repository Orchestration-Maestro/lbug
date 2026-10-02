# Restricted projection filesystem: E01a / E01b / E01c / E02a / E02b / E02c

These are native safety slices, **not a qualified engine isolation boundary yet**.
Maestro must not adopt restricted mode until every E01 slice and E02 have landed and the
combined candidate has passed E07a qualification. The existing path constructor remains
unrestricted and is not suitable for projection isolation.

## Root contract

`RootDirectory::open(existing_absolute_root)` canonicalizes the caller's existing root once.
Links in the root's own path are resolved then; that is the caller's trust decision, including
macOS `/tmp` and `/var` and symlinked homes. The canonical path is walked component by component
with no-follow, and every ancestor is held with its identity recorded.

On Unix, every rooted open (read-only or writable) requires the held root's own `fstat`
metadata to identify a directory owned by the current effective user (`st_uid == geteuid()`)
with neither group nor other write permission (`(st_mode & (S_IWGRP | S_IWOTH)) == 0`).
`RootDirectory::open` throws `IOException` naming this rule before any child is opened,
created or probed. Modes 0700 and 0755 are accepted. Ancestors above the root retain only
the held-identity rules; they need not be private. Unrooted opens are unchanged, and
native Windows supports read-only rooted construction as described below.
Callers create the root themselves with mode 0700 and never rely on the umask.

After open, replacement of any held ancestor above or at the root is detected by identity
revalidation before an operation, which fails closed. Below the root, a child is one validated
plain component: no separators, `.` or `..`, empty string or NUL. Links at any depth,
unexpected object kinds and hard-link aliases are refused. There is no recursive directory
adoption. Invalid names are refused before filesystem calls.

`Database::new_rooted(&root, basename, config)` uses the held root and the plain basename;
it does not expand home/search paths or reopen the ambient database path. Capabilities may
outlive the caller's root wrapper. Handle I/O stays handle-based.

The directory must be application-owned and not writable by other principals. Writable opens
use an **unpublished session directory**, supplied by the application; this API does not infer
publication state from a pathname. Revalidation catches deterministic replacement; it is not
an atomic compare-and-open against arbitrary same-principal external tools. Such tools
bypassing the ownership boundary are unsupported.

## Live operations and closed seams

| Operation | E01a / E01b / E01c disposition | Callers / remaining slice |
| --- | --- | --- |
| Local `openFile` read/write/create/truncate | Unix held-dirfd `fstatat`/`openat`; regular single-link identity checked before content access; truncation delayed until validation and lock acquisition; newly created names directory-synced | `storage/file_handle.cpp`, shadow, WAL, checkpointer |
| Startup and `fileOrPathExists` | Rooted no-follow regular-file probe; unsafe objects/errors are not absence; every later open rechecks | Database startup, WAL, checkpoint lock discovery; static legacy `fileExists` is unreachable from rooted file-scan binding |
| Native read/write locks | `fcntl` on the validated descriptor; rooted writable startup acquires and retains the database write lock before companion adoption or eager spill cleanup, then transfers that same descriptor into the data FileHandle after shadow replay | File handles and checkpoint locks; refused second writers preserve live bytes/dev/ino |
| Root / remembered leaf replacement | Held ancestor identities and observed child identities revalidated; external disappearance/replacement refused | Supported rename/unlink explicitly transfer or revoke identity ownership |
| Rename, including base `FileSystem` | Same held root/name pair in base, local and virtual FS; owned database/exact-companion source; atomic no-overwrite publication; replacement only of an already owned, identity-validated exact companion | WAL rotation; graph/partition rename remains refused by ownership |
| `removeFileIfExists` | Exact owned companion or registered generated temp; anchored single-entry `unlinkat`, never recursive removal or extension-directory escape; missing exact companions are idempotent | WAL/shadow/checkpoint locks, spiller and bulk PK validation |
| Spill and bulk-insert temp | Writable spill activation/reset under the held root; generated PK temps allocated create-new, registered by identity and unlinked through the same rooted removal | Forced-pressure spill/reload; operator's real no-index PK validator; see loader limitation below |
| Checkpoint and writable WAL/shadow recovery | Enabled on Unix; adoption is under the startup write lock; recoverable shadows remain until every checkpoint target's data-file sync completes and the checkpoint WAL is durably removed; read-only startup never adopts or creates sidecars | Checkpoint/rollback, held-handle WAL/shadow replay and cleanup crash cuts |
| File / directory durability | File I/O/sync uses validated descriptors; every supported name creation/rename/unlink syncs the held directory, revalidating ancestors before and after sync; WAL replay uses that handle, with no pathname reopen or ambient `.` fallback | Durability/identity errors throw, not success; directory-sync failure permanently poisons the capability |
| Copy/overwrite, createDir, glob, expandPath | Native E01a refusals retained, no ambient fallback; E01c refuses file queries before these primitives | COPY/import/export, extension install/load, file-scan binding |
| Alternative VFS registration/dispatch | Native registration refused; E01c refuses file-scan sources before glob, existence checks or function dispatch | Only local rooted handles supported |
| External query features | E01c typed structural bind-time refusals; contextual runtime/recovery helpers share the same table | Extension install/load/uninstall, COPY FROM/TO, import/export, ATTACH/DETACH, CSV/NPY/Parquet scans and external Parquet storage |
| Windows capability / restricted construction | Private local NTFS roots held by non-delete-shared ancestor handles; metadata and read-only handles opened relative to the held root; writable construction refuses before database I/O | E02c; writable support remains closed |

### Native Windows read-only boundary (E02c)

Windows resolves the caller's existing absolute root once, with strict UTF-8 conversion,
then walks the normalized volume-GUID path with held, non-inheritable directory handles.
Every held ancestor excludes `FILE_SHARE_DELETE`. Before and after child access, those
handles and directory-relative reopenings must retain their volume serial and 128-bit file
IDs. Only fixed local NTFS volumes with the required SDK metadata semantics are supported.
Trusted initial junctions, Unicode names and long roots are accepted; missing roots are never created.

Acquisition and every rooted child access validate `maestro-private-root/1` through held
handles. `NtCreateFile(FILE_OPEN)` opens one counted UTF-16 child relative to its held parent,
with `FILE_OPEN_REPARSE_POINT`; kind, reparse attributes, delete-pending state, single link,
normalized long spelling, volume and observed identity are checked before content access.
ADS, invalid UTF-8, DOS devices, separators, controls, wildcards, trailing dot/space and
case/short-name aliases refuse. Only an unremembered object-name-not-found is absence.

`new_rooted` supports clean read-only databases and active-WAL read replay, without adoption,
spill creation, mutation or namespace barriers. Rooted Windows opens force strict WAL replay:
any WAL replay failure propagates to the constructor caller rather than skipping records.
Rooted Unix opens, both read-only and writable, also force strict WAL replay, regardless of
caller configuration. Unrooted opens retain their configured WAL replay behavior.
Windows writable rooted constructors refuse before startup probing; low-level writes,
truncation, adoption, rename, removal, generated temps and directory/file sync remain closed.
No directory-sync no-op is used. Read/seek/size errors propagate rather than returning fabricated success.
This remains an intermediate fork slice, not deployment or engine-isolation qualification.

The focused Windows workflow runs mandatory root and Rust reader cases as a verified
standard user. The full native workflow runs the remaining native/Rust cases; it excludes
only that separately executed standard-user CTest case. E02c hand mutants live exclusively
on a disposable `test/` branch, never in the delivered production history.

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
The E01b new hit is the rooted startup database lock open. E01c keeps the count at 630:
query refusals add no native filesystem operation. Candidate commands and output are recorded
in the E01c report. This scan does not qualify deferred rows.

- WAL replay's former direct directory `open` moved behind `FileSystem::syncDirectoryForFile`.
  Its unrestricted implementation retains legacy behavior; rooted calls select the held
  directory before that branch. No restricted remove/rename path reopens a parent pathname.
- The tests drive the actual `NodeBatchInsert` no-index PK validator through its internal
  operator constructor, using real chunks and a one-byte spill threshold. The factory is in
  the internal `batch_insert` namespace, not a new exported/public API or a test-only branch.
  Prepared ordinary writes map to `Insert`; `NodeBatchInsert` is reached by the COPY planner.
  E01c refuses rooted COPY before its glob. **E07b must prove the prepared bound loader end to end**;
  this operator-level bulk test is not that qualification.
- E01c closes extension dynamic loads/recursive uninstall, COPY/import/export, ATTACH/DETACH,
  direct NPY mapping and remaining external file-scan routes. Static
  `LocalFileSystem::fileExists` remains unrestricted for unrooted binding, but rooted file
  sources refuse before it. No native copy/overwrite or directory-creation primitive was enabled.
- E01c itself does not change Windows safety or `build.rs`; full E01/E02/E07 qualification remains required.

### Query refusal policy

`binder/rooted_query_refusal.cpp` holds one table of 15 structural keys: statement types,
extension actions, file-scan source kind, resolved built-in reader function names, and
`StorageFormat::ICEBUG_DISK`. Binders check before any feature-specific filesystem work;
EXPLAIN and PROFILE recurse through that same binding. There is no SQL text matching.
Every refusal is a `BinderException` with the platform-independent message
`Rooted mode refuses <feature>.` Unrooted calls return from the policy without changing
upstream behavior. On a read-only database, transaction validation can refuse a write
statement first, with its own `ConnectionException`, before any file access.

Contextual non-binder boundaries share the table: WAL extension replay's manager load,
extension installation and recursive uninstall, attached-database construction/operators,
import/export operators and export directory planning, CSV/Parquet export initialization,
CSV/NPY/Parquet reader binding/planning and raw contextual CSV/Parquet readers. External
Parquet node/rel DDL refuses at bind time; the corresponding table constructors refuse
before path resolution during startup or WAL catalog recreation. Anonymous buffer-manager
mapping and Arrow in-memory storage are unchanged. No query-only guard is relied on for
persisted extension or external-table reconstruction.

## E02a Windows namespace-barrier feasibility

**Verdict: NO. Writable rooted Windows remains closed; E02c enables read-only access.** This diagnostic slice
neither links the engine nor enables any rooted operation. An API returning success is
not a documented parent-directory power-loss ordering guarantee.

Configure `tests/native_safety` with `-DMAESTRO_WINDOWS_PROBE_ONLY=ON` to select only
`maestro_windows_capability_probe`, before adding the engine. The focused
`maestro-windows-safety.yml` workflow runs on PRs and manual dispatch. On an elevated
runner it creates a temporary Users-only account, runs the probe in that account and
removes the account/profile afterward. Before opening any tested handle, the child
requires a non-elevated medium-integrity token, no Administrators SID (even deny-only),
and no backup/restore privileges; it logs the user, groups and every privilege.

On local NTFS the probe retains non-delete-shared ancestor handles and records volume
serials and 128-bit file IDs. It separately observes `FlushFileBuffers` and dynamically
resolved `NtFlushBuffersFileEx(handle, 0, nullptr, 0, &iosb)` on the same held directory,
including create/file-flush, no-replace rename, and POSIX-disposition deletion with live
leaf handles. Win32 `FileRenameInfo` with non-NULL `RootDirectory` is observed unsupported
on the tested build (`ERROR_INVALID_PARAMETER`); the probe asserts unchanged
source/destination snapshots without a path fallback. A separately authorized diagnostic resolves `NtSetInformationFile`
and uses the SDK rename layout with the published `FileRenameInformation` class to test
held-directory-relative collision refusal and publication. This is not a production API
adoption. A denied writable-directory open is a negative capability result, not a
passing writable fixture; read-only directory calls are explicitly diagnostic only.
It records the SDK, compiler, exact OS build, source SHA, filesystem, access mask,
Win32 errors, NTSTATUS and IO_STATUS_BLOCK. Mandatory mutation/token fixtures must
execute or fail the workflow. Named red-first controls and five isolated hand-mutants
prove that skipped calls, ignored failures, file-handle substitution, unsupported
results and fabricated NT success cannot qualify. The final restored CTest runs in
the same non-admin account.

The documentation gate is unresolved, independently of runtime results:
[FlushFileBuffers](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers)
requires `GENERIC_WRITE` and documents administrative privileges for a volume-wide
flush, not a non-admin parent-entry ordering guarantee.
[NtFlushBuffersFileEx](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntflushbuffersfileex)
documents flags-zero file data/metadata/storage-cache flushing and a kernel-driver
calling context; it does not establish the required user-mode directory create/rename/
delete power-loss ordering. [FILE_RENAME_INFO](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_rename_info)
permits a directory handle in `RootDirectory` for a relative `FileName`; error 87 is
an observed limitation on the tested build, not a documented prohibition. The
diagnostic NT alternative follows the published
[NtSetInformationFile](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntsetinformationfile)
ABI and [rename layout](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_rename_information).
Therefore even observed non-admin success cannot yield YES.
No process-kill or hosted-runner test substitutes for those semantics. A future YES
requires both applicable documented semantics and successful non-admin ordering probes;
it would still need the remaining E02 production and qualification slices.

SHA-bound observations, red/green transcripts, mutant diffs and restoration evidence
are uploaded as `windows-capability-probe-<sha>`; the E02a ledger report binds these to
the PR and native three-OS regression runs. No best-effort namespace barrier or weaker
immutable-snapshot durability policy is adopted here.

## Tests

Configure `tests/native_safety` into `target/native-safety`, build with `--parallel 3`, and run
CTest with `--verbose --output-on-failure`. The four executables cover E01a opens/locks and
closed seams, E01b rename/remove/adoption/durability operations, full native flows and E01c
query/helper refusals. Query probes assert typed early errors, unchanged sentinel bytes,
dev/ino and recursive directory listings; Linux linker wrapping additionally observes
ambient open/probe/mapping/load calls. Persisted external-table startup allows rooted
header reads but no external probe/open. Unrooted controls exercise real COPY/import/export,
load/uninstall, attach/detach, reader binding and external-table creation/scanning; extension
install prepares without execution because the harness disables its network installer.
Windows additionally executes the mandatory E02c read-only root suite in the focused
standard-user job; the full Windows query-refusal matrix remains E02g qualification work.
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
and Rust suites; Windows proves read-only success and writable-before-I/O refusal.

## Windows owner/DACL boundary (E02b)

`maestro-private-root/1` is an integrity predicate, not a grant of access or a
Windows rooted-open implementation by itself. E02c wires these validators into read-only
acquisition and child access; writable Windows construction remains closed.
The private native validators query held handles without modifying permissions.
They require the process primary TokenUser owner, reject thread impersonation and
all security/token/control/ACE-query failures, require a present non-null DACL,
and protect the application root from inherited ACL propagation (`D:P`). Each
child's owner and actual ACL are checked against the validated root; ancestors
above that root need held identity/no-delete-sharing protection, not private ACLs.

Only ordinary allow/deny ACEs with OI/CI/NP/IO/ID flags and known file/generic masks
are supported. Mutating grants, including inherit-only grants, may name only the
user, SYSTEM or Administrators. Deny entries do not cancel an unsafe allow.
CREATOR OWNER allow entries are permitted only inherit-only on directories with
an inheritance target. Nonmutating reads for other principals and empty DACLs
pass this predicate; an empty DACL still need not permit ordinary I/O. No ACL is
repaired. Trusted-principal ACL changes and same-user external mutation remain
outside the integrity boundary.

The shared dependency-free fixture is
`tests/native_safety/windows-private-root-v1.tsv`: **190 SDDL cases**, SHA-256
`fc2cc9f0f21bf26df6f56beadb9e6e80f8712489b26545d74c13f93010a4b46d`.
V26 expands all ten mutation masks over four untrusted principals and three roles;
V23/V24 cross allow/deny object/callback types with effective/inherit-only flags.
Every row checks decoded ACE types/flags/masks before invoking the production
bounded descriptor evaluator; failed SDDL setup never counts as refusal. The
Windows-only seam is compiled solely into `maestro_windows_security`, not the
engine. Non-SDDL invalid buffers/API results and synthetic foreign owners are
separate from standard-user live `GetSecurityInfo` acceptance/refusal fixtures.
The latter check protected-root inheritance against a changed broad parent,
unsafe writes, unreadable handles, root protection and real impersonation, with
outside bytes/file ID/volume/link and directory-listing invariants.

The focused Windows workflow runs all six security groups (including real token sizing
with `ERROR_BAD_LENGTH`) and four isolated
owner/null-DACL/writable-ACE/query-failure mutants, rebuilds after each restore,
and executes both probe and security CTest entries as its verified standard-user
child. The full-engine/source-default jobs also exercise C++ and Rust constructors; the
focused job is authoritative for standard-user ACL/root fixtures.
E02c implements capability ancestry/no-follow integration. E02a's namespace-durability
verdict remains NO; this boundary does not authorize writable Windows support.
