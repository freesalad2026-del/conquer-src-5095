# Private Server in C#

- Based on version 5517
- Written in C#
- Includes AccServer (authentication) and GameServer (game logic)
- Packed with fixes, optimizations, and stability improvements
- Automatic event system, shop, PvP, guilds, and more
- MySQL database included in `.zq` format

## Project Structure

```
AccServer/     - Authentication server (login, database connection)
GameServer/    - All game logic, events, and player management
Database/      - .zq file with the MySQL structure for direct use
```

## How to Run

1. Clone this repository.
2. In `AccServer`, edit the connection string:
   - Example: `Database=zq;Uid=root;Password=123456789`
3. In `GameServer`, change the password `Higor123*` to match the database password.
4. Import the `.zq` database into MySQL (Navicat is recommended).
5. Build the projects in Visual Studio.
6. Run `AccServer.exe` and `GameServer.exe`.

## Implemented Features

- **Offline Market** – Trading system that works even while the character is disconnected.
- **Offline Miner** – Automatic mining while the player is offline.
- **Discord Integration (Discord API)** – Updates and interactions connected to the Discord server.
- **Online Points** – Points system based on time spent online.
- **VIP System** – Exclusive benefits for VIP players.
- **Socket System** – Complete item socket system.
- And many more features!
