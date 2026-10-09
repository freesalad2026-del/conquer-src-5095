# Native C++ AccountServer (Conquer 5095)

This is a standalone **C++17 replacement** for `AccServer/` (the original C#/.NET Framework 4.8 program). It does **not** change or migrate `GameServer/`, and it does not require .NET to run.

The code follows the legacy source's wire format:

* TCP auth listener **9958**, 276-byte client packet **1086**
* Legacy auth stream transform and password RC5-12 decryption
* MySQL `zq.accounts` (`Username`, `Password`, `State`, `EntityID`)
* Lookup `zq.servers` by `Name` to get GameServer's IP and port
* Encrypted 32-byte packet **1055** response, UID at offset 4, account state or error at offset 8, server IP at offset 12, port at offset 28
* Five unsuccessful login attempts per IP within 30 seconds trigger a temporary 10-second block; connections time out after six seconds

The old C# `Authentication.Deserialize` **ignores the client-provided server name** and forces `CoPrivate`. For drop-in compatibility, the C++ version defaults to that same world, controlled by `AUTH_WORLD`.

## Windows / Visual Studio 2026

1. Install Visual Studio's **Desktop development with C++** workload, including CMake tools and Windows SDK.
2. Install **MariaDB Connector/C (64-bit)** (or a compatible MySQL C client) and remember the installation directory.
3. Open the **AccServerCpp** directory (the one containing `CMakeLists.txt`) in Visual Studio.
4. Set the CMake cache variable `MYSQL_ROOT` to the Connector/C installation directory. For a command-line build:

   ```powershell
   cmake -S AccServerCpp -B AccServerCpp/build -A x64 -DMYSQL_ROOT="C:/Program Files/MariaDB/MariaDB Connector C"
   cmake --build AccServerCpp/build --config Release
   ctest --test-dir AccServerCpp/build -C Release --output-on-failure
   ```

5. Place the Connector/C runtime DLL (`libmariadb.dll` or compatible) on your executable's DLL search path; it is not shipped with this repository.

## Linux

Install a C++ compiler, CMake and MariaDB Connector/C development files (Ubuntu/Debian: `sudo apt install build-essential cmake libmariadb-dev`).

```bash
cmake -S AccServerCpp -B AccServerCpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build AccServerCpp/build -j
ctest --test-dir AccServerCpp/build --output-on-failure
```

This standalone AccountServer can run on Linux; it **does not make the existing Windows/.NET Framework GameServer Linux-native**.

## Database and runtime configuration

Import the existing `zq.sql` to MySQL first. Configure environment variables; no password is compiled into the binary or read from the old insecure `AccServer/app.config`.

| Environment variable | Default | Meaning |
| --- | --- | --- |
| `DB_HOST` | `127.0.0.1` | MySQL host |
| `DB_PORT` | `3306` | MySQL port |
| `DB_NAME` | `zq` | Existing schema |
| `DB_USER` | `root` | MySQL user |
| `DB_PASSWORD` | empty | MySQL password; set this explicitly |
| `AUTH_BIND` | `0.0.0.0` | Local IPv4 interface |
| `AUTH_PORT` | `9958` | Client authentication TCP port |
| `AUTH_WORLD` | `CoPrivate` | Server entry name in `zq.servers` |

Example in PowerShell, **using your own credentials**:

```powershell
$env:DB_HOST="127.0.0.1"
$env:DB_NAME="zq"
$env:DB_USER="root"
$env:DB_PASSWORD="YOUR_MYSQL_PASSWORD"
$env:AUTH_WORLD="CoPrivate"
.\AccServerCpp\build\Release\AccServerCpp.exe
```

For *local testing* with the GameServer on the **same PC**:

```sql
UPDATE servers SET IP = '127.0.0.1', Port = 5816 WHERE Name = 'CoPrivate';
```

For remote players, set `servers.IP` to a reachable LAN/public address instead of `127.0.0.1`. Start the GameServer separately on that address/port. Configure the Conquer client's **authentication endpoint** to the machine running this AccountServer, TCP 9958. Do not run the C# and C++ account servers on the same address/port simultaneously.

## Important limitations

* This preserves **legacy plaintext database passwords** and the legacy **weak wire cipher** for the existing unmodified 5095 client. It is not a modern secure login design; run only on networks you trust until the client and GameServer can be upgraded together.
* Only IPv4 client authentication is supported, matching the old login packet's 16-byte address field.
* Accounts with missing/zero/out-of-range `EntityID` are denied rather than inventing a new UID. The supplied `zq.sql` defines `EntityID` as AUTO_INCREMENT, so newly created accounts should already get an ID. Fix broken rows in the database.
* `servers` routing is loaded at startup. Restart to pick up IP/port changes.
* Included automated tests cover packet structures and cipher test vectors. **Full live-client/GameServer integration has not been verified**; test locally before replacing production service.
