# Disposable hosted S1-S5 harness; every production mutation is restored independently.
function Build-FixProbe($label) {
  # Recreate the archive so incremental MSVC bundling cannot duplicate members.
  Remove-Item target/native-safety/src/Release/lbug.lib -ErrorAction SilentlyContinue
  cmake --build target/native-safety --config Release --target maestro_windows_root --parallel 3 | Tee-Object (Join-Path $evidence "$label-build.txt") | Out-Host
  $code = $LASTEXITCODE
  "command=cmake --build target/native-safety --config Release --target maestro_windows_root --parallel 3 exit=$code" | Set-Content (Join-Path $evidence "$label-build.exit.txt")
  if ($code -ne 0) { throw "UNVIABLE build $label" }
  Copy-Item target/native-safety/Release/maestro_windows_root.exe $exe -Force
}
function Fix-Mutant($label, $source, $old, $new, $test, $assertion) {
  try {
    $text = (Get-Content -Raw $source).Replace("`r`n", "`n")
    if ([regex]::Matches($text, [regex]::Escape($old)).Count -ne 1) { throw "UNVIABLE substitution $label" }
    [IO.File]::WriteAllText((Join-Path $pwd $source), $text.Replace($old, $new))
    git diff -- $source | Set-Content (Join-Path $evidence "$label.diff")
    Build-FixProbe $label
    $result = Run-RootChild $exe @($root, $test) $label
    if ($result[-1] -ne 1 -or !(Select-String -Path (Join-Path $evidence "$label.txt") -SimpleMatch "FAIL ${test}: $assertion" -Quiet)) {
      throw "SURVIVED_OR_WRONG_FAILURE $label"
    }
    'KILLED: exit 1 at exact named assertion' | Set-Content (Join-Path $evidence "$label-result.txt")
  } finally {
    git checkout -- $source
    if ($LASTEXITCODE -ne 0) { throw "Restore failed $label" }
    git diff --exit-code -- $source
    if ($LASTEXITCODE -ne 0) { throw "Restore diff failed $label" }
    "command=git checkout -- $source; git diff --exit-code -- $source exit=0" | Set-Content (Join-Path $evidence "$label-restore.exit.txt")
    Build-FixProbe "$label-restored"
    $restored = Run-RootChild $exe @($root) "$label-restored-green"
    if ($restored[-1] -ne 0 -or !(Select-String -Path (Join-Path $evidence "$label-restored-green.txt") -SimpleMatch 'windows_root groups=21 failures=0' -Quiet)) {
      throw "Restored full suite failed $label"
    }
  }
}
Fix-Mutant 'S1' 'lbug-src/src/main/database.cpp' 'dbConfig->throwOnWalReplayFailure = true;' 'dbConfig->throwOnWalReplayFailure = false;' 'active_wal_read_only_replay_has_no_mutations' 'transient WAL read failure silently accepted'
Fix-Mutant 'S2' 'lbug-src/src/common/file_system/root_directory_windows.cpp' 'if (standard.DeletePending || (standard.Directory != FALSE) != directory ||' 'if ((standard.Directory != FALSE) != directory ||' 'delete_pending_is_required' 'delete-pending file accepted'
Fix-Mutant 'S3' 'lbug-src/src/common/file_system/root_directory_windows.cpp' 'if (status == nameNotFound && missing) return {};' 'if ((status == nameNotFound || status == static_cast<NTSTATUS>(0xC0000022UL)) && missing) return {};' 'metadata_open_status_is_not_absence' 'access-denied metadata reported absent'
Fix-Mutant 'S4' 'lbug-src/src/common/file_system/local_file_system.cpp' 'if (root) throw IOException("Restricted writable handle I/O is unsupported on Windows.");' 'if (false) throw IOException("Restricted writable handle I/O is unsupported on Windows.");' 'read_only_mutators_remain_closed' 'rooted write accepted'
Fix-Mutant 'S5' 'lbug-src/src/common/file_system/local_file_system.cpp' 'if (root) throw IOException("Restricted writable truncate is unsupported on Windows.");' 'if (false) throw IOException("Restricted writable truncate is unsupported on Windows.");' 'read_only_mutators_remain_closed' 'rooted truncate accepted'
'S1-S5 killed=5 restored=5 groups=21 failures=0' | Set-Content (Join-Path $evidence 'fix-mutant-summary.txt')
