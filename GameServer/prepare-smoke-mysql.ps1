# Start a disposable MySQL server on a Windows GitHub Actions runner.
# The MySQL instance is bound to localhost and its data is discarded with the runner.
$ErrorActionPreference = 'Stop'
$mysqlCmd = Get-Command mysql -ErrorAction Stop
$mysqldCmd = Get-Command mysqld -ErrorAction SilentlyContinue
if (-not $mysqldCmd) {
    $mysqldPath = Join-Path (Split-Path $mysqlCmd.Source -Parent) 'mysqld.exe'
    if (Test-Path $mysqldPath) {
        $mysqldCmd = @{ Source = $mysqldPath }
    }
}
if (-not $mysqldCmd) { throw "MySQL client found at $($mysqlCmd.Source) but MySQL server (mysqld.exe) is missing" }
Write-Host ("MySQL client: " + $mysqlCmd.Source)
Write-Host ("MySQL daemon: " + $mysqldCmd.Source)
$baseDir = Split-Path (Split-Path $mysqldCmd.Source -Parent) -Parent
$dataDir = Join-Path $env:RUNNER_TEMP 'conquer-mysql'
New-Item -ItemType Directory -Force -Path $dataDir | Out-Null

Write-Host 'Initializing temporary local MySQL database...'
& $mysqldCmd.Source --no-defaults --initialize-insecure "--basedir=$baseDir" "--datadir=$dataDir" --console 2>&1 | ForEach-Object { if ($_ -notmatch 'password') { Write-Host $_ } }
if ($LASTEXITCODE -ne 0) { throw "MySQL initialization failed: exit $LASTEXITCODE" }

$stdout = Join-Path $env:RUNNER_TEMP 'mysqld.stdout.log'
$stderr = Join-Path $env:RUNNER_TEMP 'mysqld.stderr.log'
$mysqlProc = Start-Process -FilePath $mysqldCmd.Source -ArgumentList @('--no-defaults',"--basedir=$baseDir","--datadir=$dataDir",'--port=3306','--bind-address=127.0.0.1','--console') -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
Write-Host ("MySQL daemon started (PID " + $mysqlProc.Id + ")")
$healthy = $false
for ($i = 0; $i -lt 35; $i++) {
    Start-Sleep -Milliseconds 600
    $mysqlProc.Refresh()
    if ($mysqlProc.HasExited) { break }
    try {
        $tcp = [Net.Sockets.TcpClient]::new()
        $task = $tcp.BeginConnect('127.0.0.1', 3306, $null, $null)
        $connected = $task.AsyncWaitHandle.WaitOne(500)
        if ($connected) {
            try { $tcp.EndConnect($task); $healthy = $true } catch {}
        }
        $tcp.Dispose()
        if ($healthy) { break }
    } catch {}
}
if (-not $healthy) {
    Write-Host 'MySQL daemon failed to listen. Recent output:'
    if (Test-Path $stderr) { Get-Content $stderr -Tail 25 }
    throw 'MySQL port 3306 did not open'
}
Write-Host 'MySQL listening on 127.0.0.1:3306'

# GameServer source expects the sample root password already committed to the repository.
# AccountServer sample config differs, so only its generated CI output config is aligned.
& $mysqlCmd.Source --host=127.0.0.1 --protocol=tcp --user=root -e "ALTER USER 'root'@'localhost' IDENTIFIED BY 'Higor123*';"
if ($LASTEXITCODE -ne 0) { throw "Cannot set temporary root credentials" }
$env:MYSQL_PWD = 'Higor123*'
& $mysqlCmd.Source --host=127.0.0.1 --protocol=tcp --user=root -e "CREATE DATABASE zq CHARACTER SET utf8mb4;"
if ($LASTEXITCODE -ne 0) { throw "Cannot create zq database" }
Write-Host 'Importing repository zq.sql into the temporary database...'
Get-Content -Path (Join-Path (Resolve-Path '.').Path 'zq.sql') -Raw | & $mysqlCmd.Source --host=127.0.0.1 --protocol=tcp --user=root zq
if ($LASTEXITCODE -ne 0) { throw "zq.sql import failed: exit $LASTEXITCODE" }
Write-Host 'MySQL database imported successfully'
Remove-Item Env:MYSQL_PWD -ErrorAction SilentlyContinue

$accountConfig = Join-Path (Resolve-Path '.').Path 'AccServer\bin\Release\AccServer.exe.config'
if (-not (Test-Path $accountConfig)) { throw "Missing generated AccountServer config: $accountConfig" }
[xml]$configXml = Get-Content $accountConfig -Raw
$entry = $configXml.configuration.connectionStrings.add | Where-Object { $_.name -eq 'Conquer_Server' }
if (-not $entry) { throw "Conquer_Server connection string not found" }
$entry.connectionString = [regex]::Replace($entry.connectionString,'(?i)Password=[^;]*', 'Password=Higor123*')
$configXml.Save($accountConfig)
Write-Host 'Temporarily aligned Account Server SQL credentials for localhost test'
