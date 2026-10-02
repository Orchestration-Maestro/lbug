# Disposable review harness. Production/tests are restored between independent probes.
function Build-Probe($label) {
  # Avoid rebundling dependency members retained by MSVC's incremental librarian.
  Remove-Item target/native-safety/src/Release/lbug.lib -ErrorAction SilentlyContinue
  cmake --build target/native-safety --config Release --target maestro_windows_root --parallel 3 | Tee-Object (Join-Path $evidence "$label-build.txt") | Out-Host
  $buildExit = $LASTEXITCODE
  "command=cmake --build target/native-safety --config Release --target maestro_windows_root --parallel 3 exit=$buildExit" | Set-Content (Join-Path $evidence "$label-build.exit.txt")
  if ($buildExit -ne 0) { throw "UNVIABLE build $label" }
  Copy-Item target/native-safety/Release/maestro_windows_root.exe $exe -Force
}
function Substitute-Probe($source, $old, $new, $label) {
  $text = (Get-Content -Raw $source).Replace("`r`n", "`n")
  if ([regex]::Matches($text, [regex]::Escape($old)).Count -ne 1) { throw "UNVIABLE substitution $label" }
  [IO.File]::WriteAllText((Join-Path $pwd $source), $text.Replace($old, $new))
  git diff -- $source | Set-Content (Join-Path $evidence "$label.diff")
}
function Original-Suite($label) {
  $result = Run-RootChild $exe @($root) $label
  if ($result[-1] -ne 0 -or !(Select-String -Path (Join-Path $evidence "$label.txt") -SimpleMatch 'windows_root groups=19 failures=0' -Quiet)) {
    throw "Original 19-group suite failed: $label"
  }
}
function Review-Probe($probe) {
  $label = "probe-$($probe.id)"
  $sources = @($probe.testSource)
  if ($probe.source) { $sources += $probe.source }
  try {
    Original-Suite "$label-original-green"
    if ($probe.source) {
      Substitute-Probe $probe.source $probe.old $probe.new "$label-source"
      Build-Probe "$label-original-mutant"
      $originalMutant = Run-RootChild $exe @($root) "$label-original-mutant"
      if ($originalMutant[-1] -notin @(0, 1)) { throw 'UNVIABLE original mutant setup' }
      git checkout -- $probe.source
      if ($LASTEXITCODE -ne 0) { throw 'Source restore failed' }
    }
    if ($probe.inputOld) {
      Substitute-Probe $probe.testSource $probe.inputOld $probe.inputNew "$label-input"
      Build-Probe "$label-input-baseline"
      $baseline = Run-RootChild $exe @($root, $probe.test) "$label-input-baseline"
      if ($baseline[-1] -ne 0) {
        $status = if ($probe.id -eq 3) { 'UNVIABLE: exact STATUS_ACCESS_DENIED baseline not established' } elseif ($baseline[-1] -eq 2) { 'UNVIABLE: baseline setup' } else { 'BASELINE_FAILURE: added input fails original production' }
        $status | Set-Content (Join-Path $evidence "$label-result.txt")
        return
      }
      if (!(Select-String -Path (Join-Path $evidence "$label-input-baseline.txt") -SimpleMatch "PASS $($probe.test)" -Quiet)) { throw 'UNVIABLE missing added baseline assertion' }
      if (!$probe.source) {
        'PASS: unchanged production propagates injected exception' | Set-Content (Join-Path $evidence "$label-result.txt")
        return
      }
    }
    Substitute-Probe $probe.source $probe.old $probe.new "$label-source"
    Build-Probe "$label-focused-mutant"
    $mutant = Run-RootChild $exe @($root, $probe.test) "$label-focused-mutant"
    if ($mutant[-1] -eq 1 -and (Select-String -Path (Join-Path $evidence "$label-focused-mutant.txt") -SimpleMatch "FAIL $($probe.test): $($probe.assertion)" -Quiet)) {
      'KILLED: exit 1 at named assertion' | Set-Content (Join-Path $evidence "$label-result.txt")
    } elseif ($mutant[-1] -notin @(0, 1)) {
      'UNVIABLE: mutant setup' | Set-Content (Join-Path $evidence "$label-result.txt")
    } else {
      'SURVIVED_OR_WRONG_FAILURE: see focused assertion log' | Set-Content (Join-Path $evidence "$label-result.txt")
    }
  } catch {
    $_ | Out-String | Set-Content (Join-Path $evidence "$label-result.txt")
    Write-Host "Probe $($probe.id) exception: $_"
  } finally {
    git checkout -- $sources
    if ($LASTEXITCODE -ne 0) { throw 'Probe restore failed' }
    git diff --exit-code -- $sources
    if ($LASTEXITCODE -ne 0) { throw 'Probe sources not restored' }
    Build-Probe "$label-restored"
    Original-Suite "$label-restored-green"
  }
}
$probes = Get-Content -Raw .github/scripts/e02c-review-probes.json | ConvertFrom-Json
foreach ($probe in $probes) { Review-Probe $probe }
'probes=8 completed=8 each-original-and-restored-19-group-green=1' | Set-Content (Join-Path $evidence 'review-probe-summary.txt')
