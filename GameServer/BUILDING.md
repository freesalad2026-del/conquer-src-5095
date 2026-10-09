# GameServer with Visual Studio 2026

**No direct modification of TQHandle.dll is required for the project to compile.** This repository already includes `GameServer/bin/Debug/TQHandle.dll`, `ManagedOpenSsl.dll`, `MySql.Data.dll` and `System.Web.Entity.dll`. The original project referenced DLL paths on another developer's PC. Those hint paths are corrected in this branch.

## Build and run (Windows)

1. In the Visual Studio Installer install the **.NET desktop development** workload, the **.NET Framework 4.8 targeting pack**, and the **.NET Framework 4.8 SDK**.
2. Open `GameServer/COServer.sln` and build **Debug | x86**.
3. Run `COServer.exe` from `GameServer/bin/Debug` **with that directory as the current working directory**. The game loads `shell.ini`, `Database5103`, and native libraries relative to the working directory.
4. Keep the bundled `libeay32.dll` alongside the executable (it is a native dependency); do not replace the DLLs with random downloads. Keep the `TQGuard` folder, but note that **TQGuard is a settings/log directory, not TQHandle.dll**.
5. Install/configure MySQL and import `zq.sql` into a database named `zq`. Update the two hardcoded game DB settings in `GameServer/Database/Mysql/DataHolderTable.cs` and `GameServer/Database/ServerDatabase.cs` and the account DB settings in `AccServer/app.config`. All must reference the same database. Do not commit passwords.

## Connect the Account Server to the GameServer

- Account Server listens on **9958**.
- GameServer listens on `Game_Port` in `GameServer/bin/Debug/shell.ini`, normally **5816**.
- `Program.cs` also starts `TQHandle.Network.MsgServer` on **Game_Port + 1000** (**6816** by default).
- The account server reads a record from the `zq.servers` table and tells the *client* what GameServer endpoint to connect to. Its name must match `ServerName` in `shell.ini` (`CoPrivate` by default). The `Servers.IP` column needs the client-reachable IP of the GameServer; `shell.ini`'s `AddresIP` is the IP the GameServer attempts to bind locally.

For **same-computer testing only**, put this in the existing `GameServer/bin/Debug/shell.ini`:

```ini
[ServerInfo]
AddresIP=127.0.0.1
Game_Port = 5816
ServerName = CoPrivate
```

Keep the other existing `shell.ini` sections, especially `[Database]`. Then run the following against your MySQL `zq` database:

```sql
UPDATE servers SET IP = '127.0.0.1', Port = 5816 WHERE Name = 'CoPrivate';
```

Set the **game client's login** endpoint to `127.0.0.1:9958`, start MySQL, start `COServer.exe`, then start `AccServer.exe`. The account server responds to successful logins with the destination from the `servers` table. For connections from another PC, replace `127.0.0.1` in the **SQL destination** with the GameServer's reachable LAN/public IP and bind the game to an IP actually assigned to its host. Make sure Windows Firewall allows the needed TCP ports. Don't publish MySQL port 3306 to the internet.

The repository currently has three **different, old IPs** in `shell.ini`, `Database5103/client_config.ini`, and `zq.sql`; they do not describe a working local server setup. `client_config.ini` is a separate transfer/inter-server list; its port 9920 is not the main 5816 game port.

## If the server still cannot open

- A **missing DLL** or **BadImageFormatException**: confirm the bundled DLLs and `libeay32.dll` exist alongside `COServer.exe` and use the correct process architecture.
- A **socket/address-not-available exception**: check `shell.ini` `AddresIP`. Binding to an old public IP that is not assigned to your PC won't work.
- A **MySQL error**: confirm MySQL is running, the `zq` schema is imported, and both server connection strings use valid credentials.
- A **TQHandle/TQGuard exception**: record the **exact exception message and stack trace** before attempting to modify the binary; the DLL appears to provide a separate network listener, and a crash may be due to a missing dependency, port conflict or runtime mismatch.

The build test checks compilation, **not** a full running server, client login or DLL runtime behavior.
