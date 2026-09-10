param(
    [ValidateRange(1, 10000)][int]$Repeats = 100
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $repo 'target/release/examples/native_instrumentation.exe'
$output = Join-Path $repo 'target/phase-08-release'
if (-not (Test-Path $exe)) {
    throw "找不到 Release 验收程序：$exe，请先构建 native_instrumentation example"
}
New-Item -ItemType Directory -Force $output | Out-Null
$stdout = Join-Path $output 'stdout.log'
$stderr = Join-Path $output 'stderr.log'
$process = Start-Process -FilePath $exe -ArgumentList $Repeats.ToString() `
    -WorkingDirectory $repo -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr
$null = $process.Handle
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
if ($null -eq $exitCode -and $stdoutText -match 'Phase 8 Instrumentation') { $exitCode = 0 }
if ($exitCode -ne 0 -or $stdoutText -notmatch 'Phase 8 Instrumentation \+ Release 调优验收通过') {
    throw "Phase 8 验收程序失败，退出码 $exitCode，见 $output"
}
if ($samples.Count -lt 2) { throw 'Phase 8 进程采样不足，无法记录资源指标' }
$first = $samples[0]
$last = $samples[$samples.Count - 1]
$result = [pscustomobject]@{
    scenario = 'Phase 8 Instrumentation；ACTIVE 60/30、IDLE 10、DEEP_IDLE 5、SLEEP 0、Input/Sleep/Wake aggregate'
    repeats = $Repeats
    sample_count = $samples.Count
    duration_seconds = $last.seconds - $first.seconds
    exit_code = $exitCode
    logical_processors = [Environment]::ProcessorCount
    cpu_avg_percent = 100 * ($last.cpu_ms - $first.cpu_ms) / (($last.seconds - $first.seconds) * 1000 * [Environment]::ProcessorCount)
    memory_start_working_set_mb = $first.working_set_mb
    memory_peak_working_set_mb = ($samples.working_set_mb | Measure-Object -Maximum).Maximum
    memory_end_working_set_mb = $last.working_set_mb
    memory_start_private_mb = $first.private_mb
    memory_peak_private_mb = ($samples.private_mb | Measure-Object -Maximum).Maximum
    memory_end_private_mb = $last.private_mb
    samples = $samples
}
$result | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $output 'metrics.json') -Encoding utf8
$result | Select-Object -ExcludeProperty samples | Format-List
