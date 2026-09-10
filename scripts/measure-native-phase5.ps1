param(
[ValidateRange(1, 1000)][int]$Repeats = 50
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $repo 'target/release/examples/native_dirty_sleep.exe'
$output = Join-Path $repo 'target/phase-05-release'
if (-not (Test-Path $exe)) {
    throw "找不到 Release 验收程序：$exe，请先构建 native_dirty_sleep example"
}
New-Item -ItemType Directory -Force $output | Out-Null
$stdout = Join-Path $output 'stdout.log'
$stderr = Join-Path $output 'stderr.log'
$process = Start-Process -FilePath $exe -ArgumentList $Repeats.ToString() `
    -WorkingDirectory $repo -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr
$watch = [System.Diagnostics.Stopwatch]::StartNew()
$samples = [System.Collections.Generic.List[object]]::new()
while (-not $process.HasExited) {
    $process.Refresh()
    try {
        if ($process.HasExited) { break }
        $samples.Add([pscustomobject]@{
            seconds = $watch.Elapsed.TotalSeconds
            cpu_ms = $process.TotalProcessorTime.TotalMilliseconds
            working_set_mb = $process.WorkingSet64 / 1MB
            private_mb = $process.PrivateMemorySize64 / 1MB
        })
    } catch { if (-not $process.HasExited) { throw } }
    Start-Sleep -Milliseconds 100
}
$process.WaitForExit()
$process.Refresh()
$exitCode = $process.ExitCode
$stdoutText = Get-Content -Raw $stdout
if ($null -eq $exitCode -and $stdoutText -match 'Phase 5 Dirty Rendering') { $exitCode = 0 }
if ($exitCode -ne 0 -or $stdoutText -notmatch 'Phase 5 Dirty Rendering') {
    throw "Phase 5 验收程序失败，退出码 $exitCode，见 $output"
}
$result = [pscustomobject]@{
    scenario = 'Phase 5 Dirty Rendering；静态 0 FPS、condition_variable 阻塞、dirty/wake 来源'
    repeats = $Repeats
    sample_count = $samples.Count
    duration_seconds = $watch.Elapsed.TotalSeconds
    exit_code = $exitCode
    logical_processors = [Environment]::ProcessorCount
    working_set_start_mb = if ($samples.Count) { $samples[0].working_set_mb } else { $null }
    working_set_peak_mb = if ($samples.Count) { ($samples.working_set_mb | Measure-Object -Maximum).Maximum } else { $null }
    working_set_end_mb = if ($samples.Count) { $samples[-1].working_set_mb } else { $null }
    private_start_mb = if ($samples.Count) { $samples[0].private_mb } else { $null }
    private_peak_mb = if ($samples.Count) { ($samples.private_mb | Measure-Object -Maximum).Maximum } else { $null }
    private_end_mb = if ($samples.Count) { $samples[-1].private_mb } else { $null }
    samples = $samples
}
$result | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $output 'metrics.json') -Encoding utf8
$result | Select-Object -ExcludeProperty samples | Format-List
