param(
    [ValidateRange(1, 500)][int]$LoadCycles = 100,
    [ValidateRange(1, 2000)][int]$ShowHideCycles = 500,
    [ValidateRange(1, 2000)][int]$ResizeCycles = 500,
    [ValidateRange(1, 5000)][int]$MotionCycles = 1000
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $repo 'target/release/examples/native_resource_lifecycle.exe'
$model = Join-Path $repo 'src-tauri/assets/models/standard/cat.model3.json'
$output = Join-Path $repo 'target/phase-07-release'
if (-not (Test-Path $exe)) {
    throw "找不到 Release 验收程序：$exe，请先构建 native_resource_lifecycle example"
}
New-Item -ItemType Directory -Force $output | Out-Null
$stdout = Join-Path $output 'stdout.log'
$stderr = Join-Path $output 'stderr.log'
$process = Start-Process -FilePath $exe -ArgumentList @(
    ('"' + $model + '"'), $LoadCycles.ToString(), $ShowHideCycles.ToString(),
    $ResizeCycles.ToString(), $MotionCycles.ToString()
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
$process.Refresh()
$exitCode = $process.ExitCode
$stdoutText = Get-Content -Raw $stdout
if ($null -eq $exitCode -and $stdoutText -match 'Phase 7 Native Resource Lifecycle') { $exitCode = 0 }
if ($exitCode -ne 0 -or $stdoutText -notmatch 'Phase 7 Native Resource Lifecycle') {
    throw "Phase 7 验收程序失败，退出码 $exitCode，见 $output"
}
if ($samples.Count -lt 2) { throw 'Phase 7 进程采样不足，无法记录 Memory Start/Peak/End' }
$first = $samples[0]
$last = $samples[$samples.Count - 1]
$result = [pscustomobject]@{
    scenario = 'Phase 7 Native/OpenGL/Cubism Resource Lifecycle；Load/Unload、Show/Hide、Resize、Motion 压力'
    load_cycles = $LoadCycles
    show_hide_cycles = $ShowHideCycles
    resize_cycles = $ResizeCycles
    motion_cycles = $MotionCycles
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
