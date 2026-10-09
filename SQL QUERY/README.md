# SQL QUERY — Native AccountServer

Run these queries using a MySQL client or phpMyAdmin **after** importing the root `zq.sql` database dump.

- `00_auth_compatibility_check.sql` — read-only checks for tables, columns, world routing and account IDs.
- `01_localhost_routing.sql` — optional change for running both services on one computer, routing clients to 127.0.0.1:5816.
- `02_restore_world_routing.sql` — example query to set the world back to **your own actual LAN or public GameServer IP**.

The native account server reads from `zq.accounts` and `zq.servers`; both tables already exist in the provided `zq.sql`. **No new table is necessary and no accounts need migration.** Queries here do not delete or reset player data.

**Security**: The old repository includes example credentials. Change reused/compromised passwords and set the AccountServer's `DB_PASSWORD` environment variable on the running host rather than adding credentials to SQL scripts. Do not expose MySQL port 3306 to the internet.

Verify `servers.Name` matches `AUTH_WORLD` (default `CoPrivate`) and that the GameServer actually listens on the configured port. `127.0.0.1` redirects only to the connecting player's own PC and is suitable for local testing, **not public players**.
