# Temporary Windows startup probe. Does not edit committed settings.
# Run from the repository root with PowerShell 7 on Windows.
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path '.').Path
$gameDir = Join-Path $root 'GameServer\bin\Debug'
$accDir = Join-Path $root 'AccServer\bin\Release'
if (-not (Test-Path $accDir)) { throw "Missing Account Server build: $accDir" }
if (-not (Test-Path (Join-Path $gameDir 'COServer.exe'))) { throw "Missing COServer.exe" }
if (-not (Test-Path (Join-Path $accDir 'AccServer.exe'))) { throw "Missing AccServer.exe" }

$ini = Join-Path $gameDir 'shell.ini'
$iniContent = [IO.File]::ReadAllText($ini)
# Change only the temporary CI checkout, never the public/repository config.
$iniContent = [Text.RegularExpressions.Regex]::Replace($iniContent, '(?m)^(AddresIP\s*=\s*)[^\r\n]+', '${1}127.0.0.1')
[IO.File]::WriteAllText($ini, $iniContent)

Write-Host '==== Windows prerequisites ===='
Write-Host ("OS: " + [Environment]::OSVersion.VersionString)
Write-Host ("Game directory exists: " + (Test-Path (Join-Path $gameDir 'Database5103')))
Write-Host ("TQHandle.dll exists: " + (Test-Path (Join-Path $gameDir 'TQHandle.dll')))
Write-Host ("ManagedOpenSsl.dll exists: " + (Test-Path (Join-Path $gameDir 'ManagedOpenSsl.dll')))
Write-Host ("libeay32.dll exists: " + (Test-Path (Join-Path $gameDir 'libeay32.dll')))
Write-Host ("MySQL client installed: " + [bool](Get-Command mysql -ErrorAction SilentlyContinue))
$mysqlServices = @(Get-Service -ErrorAction SilentlyContinue | Where-Object { $_.Name -match 'mysql|maria' } | Select-Object -ExpandProperty Name)
Write-Host ("MySQL services: " + ($mysqlServices -join ', '))

function Probe-Server([string]$Name, [string]$Directory, [string]$Executable, [int]$Port) {
    $out = Join-Path $env:RUNNER_TEMP ("$Name.stdout.txt")
    $err = Join-Path $env:RUNNER_TEMP ("$Name.stderr.txt")
    Write-Host "==== Starting $Name on 127.0.0.1:$Port ===="
    $process = $null
    try {
        $process = Start-Process -FilePath (Join-Path $Directory $Executable) -WorkingDirectory $Directory -RedirectStandardOutput $out -RedirectStandardError $err -PassThru
        Start-Sleep -Seconds 9
        $process.Refresh()
        $alive = -not $process.HasExited
        $exitCode = if ($alive) { 'still running' } else { "$($process.ExitCode)" }
        $listening = $false
        try {
            $client = [Net.Sockets.TcpClient]::new()
            $result = $client.BeginConnect('127.0.0.1', $Port, $null, $null)
            if ($result.AsyncWaitHandle.WaitOne(1200)) {
                try { $client.EndConnect($result); $listening = $true } catch {}
            }
            $client.Close()
        } catch {}
        Write-Host ("RESULT $Name alive=$alive exit=$exitCode localport=$Port listening=$listening")
        if (Test-Path $out) {
            Write-Host "===== $Name stdout (last 40 lines) ====="
            Get-Content -Path $out -Tail 40 -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_ }
        }
        if (Test-Path $err) {
            Write-Host "===== $Name stderr (last 30 lines) ====="
            Get-Content -Path $err -Tail 30 -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_ }
        }
        return $listening
    } catch {
        Write-Host ("RESULT $Name process start failed: " + $_.Exception.Message)
        return $false
    } finally {
        if ($process) {
            try { if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue } } catch {}
        }
    }
}

$gameOk = Probe-Server 'GameServer' $gameDir 'COServer.exe' 5816
$accOk = Probe-Server 'AccountServer' $accDir 'AccServer.exe' 9958
Write-Host "SMOKE_RESULT game_port_5816=$gameOk account_port_9958=$accOk"
if (-not $gameOk -or -not $accOk) {
    throw 'Startup smoke test did not open both ports; inspect output above for the cause. A running MySQL zq database is required for full login.'
}
