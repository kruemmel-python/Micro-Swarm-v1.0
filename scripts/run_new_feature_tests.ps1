param(
    [string]$Exe = "",
    [switch]$SkipGpu,
    [int]$TimeoutSeconds = 120
)

function Resolve-Exe {
    param([string]$PathOverride)
    if ($PathOverride -and (Test-Path $PathOverride)) { return (Resolve-Path $PathOverride).Path }
    $candidates = @(
        ".\\build\\Release\\micro_swarm.exe",
        ".\\build\\micro_swarm.exe",
        ".\\build\\Debug\\micro_swarm.exe",
        ".\\micro_swarm.exe"
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { return (Resolve-Path $c).Path }
    }
    throw "micro_swarm.exe nicht gefunden. Bitte -Exe angeben."
}

$exePath = Resolve-Exe -PathOverride $Exe
Write-Host "Using executable: $exePath"

$pass = 0
$fail = 0

function Run-Test {
    param(
        [string]$Name,
        [string[]]$CliArgs,
        [int]$ExpectExit = 0,
        [string[]]$MustContain = @(),
        [string[]]$MustNotContain = @(),
        [string]$FileToCheck = "",
        [string[]]$FileMustContain = @()
    )

    Write-Host "[TEST] $Name"
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exePath
    $psi.Arguments = ($CliArgs -join ' ')
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true

    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $psi
    $null = $proc.Start()
    $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
    $stderrTask = $proc.StandardError.ReadToEndAsync()

    if (-not $proc.WaitForExit($TimeoutSeconds * 1000)) {
        $proc.Kill()
        Write-Host "  FAIL: Timeout"
        $script:fail++
        return
    }

    $proc.WaitForExit()
    $out = $stdoutTask.Result + $stderrTask.Result
    $code = $proc.ExitCode

    $ok = $true
    if ($code -ne $ExpectExit) { $ok = $false }
    foreach ($m in $MustContain) {
        if ($out -notmatch [regex]::Escape($m)) { $ok = $false }
    }
    foreach ($m in $MustNotContain) {
        if ($out -match [regex]::Escape($m)) { $ok = $false }
    }

    $fileNote = ""
    if ($ok -and $FileToCheck) {
        if (-not (Test-Path $FileToCheck)) {
            $ok = $false
            $fileNote = "  Missing file: $FileToCheck"
        } else {
            $fileText = (Get-Content -Path $FileToCheck -Raw)
            foreach ($m in $FileMustContain) {
                if ($fileText -notmatch [regex]::Escape($m)) {
                    $ok = $false
                    $fileNote = "  Missing in file: $m"
                    break
                }
            }
        }
    }

    if ($ok) {
        Write-Host "  PASS"
        $script:pass++
    } else {
        Write-Host "  FAIL (exit=$code)"
        $script:fail++
        Write-Host "  Output (trimmed):"
        Write-Host ($out.Substring(0, [Math]::Min(800, $out.Length)))
        if ($fileNote) { Write-Host $fileNote }
    }
}

# 1) Help shows new flags
Run-Test -Name "Help shows toxic flags" -CliArgs @("--help") -ExpectExit 1 -MustContain @(
    "--toxic-enable",
    "--toxic-disable",
    "--toxic-max-frac",
    "--toxic-stride-min",
    "--toxic-stride-max",
    "--toxic-iters-min",
    "--toxic-iters-max",
    "--toxic-max-frac-quadrant",
    "--toxic-max-frac-species",
    "--dna-export",
    "--logic-mode",
    "--logic-inputs",
    "--logic-output",
    "--logic-pulse-period",
    "--logic-pulse-strength",
    "--log-verbosity"
)

# 2) Invalid value rejects
Run-Test -Name "Invalid toxic-max-frac rejects" -CliArgs @("--toxic-max-frac", "2.0") -ExpectExit 1

# 3) Basic run with new params (CPU)
Run-Test -Name "CPU run with toxic params" -CliArgs @(
    "--steps", "2",
    "--toxic-enable",
    "--toxic-max-frac", "0.5",
    "--toxic-stride-min", "1",
    "--toxic-stride-max", "32",
    "--toxic-iters-min", "0",
    "--toxic-iters-max", "64",
    "--toxic-max-frac-quadrant", "0", "0.25",
    "--toxic-max-frac-species", "2", "0.1",
    "--log-verbosity", "0"
) -ExpectExit 0

# 4) Logic mode run (CPU)
Run-Test -Name "CPU run with logic XOR" -CliArgs @(
    "--steps", "40",
    "--evo-enable",
    "--logic-mode", "XOR",
    "--logic-inputs", "4", "4", "4", "28",
    "--logic-output", "28", "16",
    "--logic-pulse-period", "10",
    "--logic-pulse-strength", "10"
) -ExpectExit 0

# 5) DNA export includes semantics columns
$dnaCsv = Join-Path $env:TEMP "micro_swarm_dna_test.csv"
if (Test-Path $dnaCsv) { Remove-Item $dnaCsv -Force }
Run-Test -Name "DNA export includes semantics columns" -CliArgs @(
    "--steps", "5",
    "--evo-enable",
    "--dna-export", "`"$dnaCsv`""
) -ExpectExit 0 -FileToCheck $dnaCsv -FileMustContain @("response0", "response2", "emit3")

# 6) Info-metabolism flag parsing
Run-Test -Name "Info-cost parsing" -CliArgs @(
    "--steps", "2",
    "--info-cost", "0.02"
) -ExpectExit 0

if (-not $SkipGpu) {
    # 7) GPU run (optional). Uses more steps to trigger evolution logs.
    Run-Test -Name "GPU run (optional)" -CliArgs @(
        "--steps", "510",
        "--evo-enable",
        "--ocl-enable",
        "--log-verbosity", "1"
    ) -ExpectExit 0 -MustContain @("Toxic-Hist")
}

Write-Host "\nSummary: $pass passed, $fail failed"
if ($fail -gt 0) { exit 1 }
