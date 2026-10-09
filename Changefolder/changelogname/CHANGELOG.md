# Changelog — Conquer 5095 AccountServer C++

## 2026-10-09 — Native AccountServer initial rework

### Added
- New independent `AccServerCpp/` project written in C++17.
- Native account authentication listener (default TCP port `9958`).
- Legacy Conquer 5095 authentication stream encryption/decryption.
- RC5 password block decoder compatible with the original 276-byte login packet (ID 1086).
- 32-byte login redirect/error response (ID 1055) with account UID, state, server IP and server port.
- Existing MySQL `zq.accounts` credential/ban lookup and `zq.servers` world routing.
- Environment variable configuration instead of compiling credentials into the executable.
- Failed-login rate limiting, bounded concurrent client connections, socket timeouts, and prepared SQL queries.
- Visual Studio 2026 / CMake support and a Linux build workflow.
- Protocol regression tests and getting-started instructions.
- Root `SQL QUERY/` folder with safe database checks and optional localhost routing.

### Kept unchanged
- Original `AccServer/` C# source (.NET Framework 4.8), as a rollback option.
- Existing `GameServer/` source and its network protocol.
- Existing `zq.sql` and player/account records.

### Important compatibility notes
- Original C# server forces the world name `CoPrivate` even when the client supplies another name; the C++ rewrite defaults to the same value via `AUTH_WORLD`.
- The existing SQL schema already contains the required authentication tables. No destructive SQL migration is needed.
- C++ removes the **AccountServer's** .NET dependency only. The separately compiled GameServer still has its own framework/dependency requirements.

### Verification status
- RC5 and auth stream deterministic test vectors are included in `AccServerCpp/tests/protocol_tests.cpp`.
- Native build and unit tests are configured through `.github/workflows/accserver-cpp.yml`.
- Live Conquer-client ↔ C++ AccountServer ↔ existing GameServer end-to-end login has **not been validated**; use local testing before production cutover.

### Future work
- Validate a full account login locally using the actual 5095 client and GameServer.
- Review or replace legacy insecure password storage when updating the client and server protocols together.
