-- Read-only native 5095 AccountServer compatibility report.
-- Import the root zq.sql before running this query.
USE `zq`;

-- Both should exist already; this does not modify the schema.
SELECT TABLE_NAME, ENGINE
FROM information_schema.TABLES
WHERE TABLE_SCHEMA = DATABASE()
  AND TABLE_NAME IN ('accounts', 'servers')
ORDER BY TABLE_NAME;

-- Verify the required login columns and data types.
SELECT TABLE_NAME, COLUMN_NAME, DATA_TYPE, COLUMN_TYPE
FROM information_schema.COLUMNS
WHERE TABLE_SCHEMA = DATABASE()
  AND (
    (TABLE_NAME = 'accounts'
     AND COLUMN_NAME IN ('Username', 'Password', 'State', 'EntityID'))
    OR
    (TABLE_NAME = 'servers'
     AND COLUMN_NAME IN ('Name', 'IP', 'Port'))
  )
ORDER BY TABLE_NAME, ORDINAL_POSITION;

-- Confirm login redirects target the correct GameServer endpoint.
SELECT `Name`, `IP`, `Port`
FROM `servers`
WHERE `Name` = 'CoPrivate';

-- Accounts with unusable IDs cannot be forwarded to GameServer.
-- No passwords or sensitive account data are selected.
SELECT COUNT(*) AS AccountsWithInvalidEntityID
FROM `accounts`
WHERE `EntityID` = 0 OR `EntityID` > 4294967295;

-- Expected: account state=1 is banned in the original C# source.
SELECT `State`, COUNT(*) AS AccountCount
FROM `accounts`
GROUP BY `State`
ORDER BY `State`;
