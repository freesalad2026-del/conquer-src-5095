# Build the Account Server with Visual Studio 2026

The Account Server is a **.NET Framework 4.8** application (not a .NET 8/9/10 or .NET Core application).
Keep its target framework as `v4.8` to preserve compatibility with the existing authentication source.

## One-time setup

1. Open **Visual Studio Installer** -> **Modify** for Visual Studio 2026.
2. Under **Individual components**, install the **.NET Framework 4.8 SDK** and **.NET Framework 4.8 targeting pack** (also called the developer pack). If prompted, install the **.NET desktop development** workload.
3. If you want to run the executable on another computer, install the **.NET Framework 4.8 runtime** there (4.8.1 also supports 4.8 apps).
4. Open `AccServer/AccServer.sln` in Visual Studio 2026.
5. Right-click the solution and choose **Restore NuGet Packages** (or run a solution rebuild; the package is restored automatically by modern Visual Studio).
6. Select **Release | Any CPU**, then **Build -> Rebuild Solution**.
7. The built executable is `AccServer/bin/Release/AccServer.exe`; run it with the matching generated `AccServer.exe.config` and copied NuGet dependencies.

## Database and starting the server

- Before running, import the provided `zq.sql` into MySQL and configure the `Conquer_Server` connection string in `AccServer/app.config` for **your own** database login.
- The Account Server listens on TCP port **9958**, configured in `AccServer/Program.cs`.
- The GameServer is a separate project; building the Account Server does not automatically fix or launch GameServer.
- Do not commit database passwords or other secrets when changing configuration files.

## Troubleshooting

- **MSB3644 or reference assemblies for .NETFramework v4.8 not found:** install the **4.8 developer/targeting pack** via Visual Studio Installer, then reload the project.
- **MySql.Data cannot be found:** restore NuGet packages for the solution and rebuild. The project uses `PackageReference` instead of stale machine-specific DLL hint paths.
- **.NET Framework 4.8 is required when starting AccServer.exe:** install the **.NET Framework 4.8 runtime** on the machine running the executable. A GitHub project change cannot install Windows runtime components.
- **MySQL connection error:** check that MySQL is running, `zq.sql` has been imported and your connection details match. This is separate from framework/build errors.

The project's custom authentication and packet code has not been changed by this build-configuration fix.
