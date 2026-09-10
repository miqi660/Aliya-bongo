param(
    [ValidateRange(1, 2000)][int]$ResizeCycles = 500,
    [ValidateRange(1, 1000)][int]$VisibilityCycles = 100
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $repo 'target/release/examples/native_window_lifecycle.exe'
$model = Join-Path $repo 'src-tauri/assets/models/standard/cat.model3.json'
$output = Join-Path $repo 'target/phase-03-release'
New-Item -ItemType Directory -Force $output | Out-Null
$stdout = Join-Path $output 'stdout.log'
$stderr = Join-Path $output 'stderr.log'
$process = Start-Process -FilePath $exe -ArgumentList @(
    ('"' + $model + '"'), $ResizeCycles.ToString(), $VisibilityCycles.ToString()
) -WorkingDirectory $repo -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
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
if ($process.ExitCode -ne 0) { throw "Phase 3 验收程序失败，退出码 $($process.ExitCode)，见 $output" }
if ($samples.Count -lt 2) { throw '采样不足' }
$first = $samples[0]
$last = $samples[$samples.Count - 1]
$result = [pscustomobject]@{
    scenario = 'Phase 3 Native Window Lifecycle；包含 Show/Hide、Move、Resize、Runtime Destroy'
    resize_cycles = $ResizeCycles
    visibility_cycles = $VisibilityCycles
    sample_count = $samples.Count
    duration_seconds = $last.seconds - $first.seconds
    logical_processors = [Environment]::ProcessorCount
    cpu_avg_percent = 100 * ($last.cpu_ms - $first.cpu_ms) / (($last.seconds - $first.seconds) * 1000 * [Environment]::ProcessorCount)
    working_set_start_mb = $first.working_set_mb
    working_set_peak_mb = ($samples.working_set_mb | Measure-Object -Maximum).Maximum
    working_set_end_mb = $last.working_set_mb
    private_start_mb = $first.private_mb
    private_peak_mb = ($samples.private_mb | Measure-Object -Maximum).Maximum
    private_end_mb = $last.private_mb
    samples = $samples
}
$result | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $output 'metrics.json') -Encoding utf8
$result | Select-Object -ExcludeProperty samples | Format-List
