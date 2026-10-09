# GameServer anti-cheat: what is verified

## TQHandle.dll inspection

`GameServer/bin/Debug/TQHandle.dll` is a .NET assembly with visible types under `TQHandle.ClientState`, `TQHandle.Network.MsgServer`, `TQHandle.Network.SocketManager` and related network classes.

Two direct calls were found in the checked-in GameServer source:

- `Program.cs`: starts `TQHandle.Network.MsgServer.Run(GamePort + 1000)`; default supplemental TCP port is **6816**.
- `Client/GameClient.cs`: declares `TQHandle.ClientState TQState`, with no other direct reference in the inspected GameServer source.

**Decompilation is not complete.** ILSpy 9.1 enumerated the types, but failed with `System.BadImageFormatException: Invalid method header` for both `MsgServer` and `ClientState`. This may indicate protections/invalid IL; it is *not* proof the DLL is malicious. Do not silently replace those functions with a stub or remove the DLL and claim equivalent protection.

A successful Windows localhost smoke test on the earlier PR showed `[TQGuard] Activated.` and both game (5816) and account (9958) listener ports opened. This proves basic startup, not that cheating is prevented, client scans work, or the binary is safe.

## New independently auditable GameServer checks

`GameServer/AntiCheat/PacketFloodGuard.cs` adds **additional** server-side network abuse detection:

- Enabled by default for established game sessions, after the Diffie-Hellman handshake.
- Each socket gets an independent one-second packet window; no client executable and no shared player files.
- A single extreme packet spike (more than `PacketFloodHardLimit` packets in the same window) disconnects the client.
- A client exceeding `PacketFloodSoftLimit` in **three consecutive one-second windows** is disconnected.
- Ordinary traffic passes through unmodified. Logs are throttled and contain the packet ID, client IP and rate, **not** packet payloads or passwords.
- Memory for a disconnected session can be reclaimed via `ConditionalWeakTable`.
- Long idle periods reset prior violations, reducing false positives.

The `GameServer/bin/Debug/shell.ini` settings are:

```ini
[AntiCheat]
EnablePacketFloodGuard=1
PacketFloodSoftLimit=1000
PacketFloodHardLimit=2500
```

These are deliberately conservative defaults. Valid soft thresholds are clamped between 100 and 5000, and hard thresholds are clamped above the soft threshold. This is a packet flood/rate guard, **not** movement validation, damage validation, server authoritative speed-hack prevention, or client process scanning.

## Preserved behavior and known limitations

- Keep the original `TQHandle.dll` and `TQGuard` folder until a properly reviewed replacement demonstrates equivalent behavior. The new guard does **not** attempt to impersonate that binary or disable any of its checks.
- No edits to `GameClient.Teleport`, account packet formats, or battle damage calculations.
- Windows .NET Framework 4.8 and localhost MySQL remain required for the current server.
- Runtime startup and port availability are checked in the existing Windows localhost workflow. Full packet-flood integration, Conquer client login, anti-cheat efficacy and false-positive testing under real gameplay remain to be done before production enforcement.

Security note: source-level review of a protected DLL is inconclusive. Run it only on a machine and environment you trust, and verify its provenance.
