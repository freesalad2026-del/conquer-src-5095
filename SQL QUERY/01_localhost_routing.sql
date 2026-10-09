-- OPTIONAL: Only run when Conquer client, C++ AccountServer, and
-- GameServer are all on the SAME Windows/Linux computer.
-- Do not use 127.0.0.1 for external players.
-- Take a database backup before making configuration changes.
USE `zq`;

-- See current values first.
SELECT `Name`, `IP`, `Port`
FROM `servers`
WHERE `Name` = 'CoPrivate';

-- Existing zq.sql already creates this server row.
-- This updates its destination; it never deletes accounts or characters.
UPDATE `servers`
SET `IP` = '127.0.0.1', `Port` = 5816
WHERE `Name` = 'CoPrivate';

SELECT `Name`, `IP`, `Port`
FROM `servers`
WHERE `Name` = 'CoPrivate';
