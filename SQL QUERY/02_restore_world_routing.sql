-- OPTIONAL: Replace the placeholder with your ACTUAL GameServer
-- network address before running. For public players, ensure the IP
-- is reachable by them and the game port is allowed through the firewall.
USE `zq`;

-- MANUAL CONFIGURATION:
-- UPDATE `servers`
-- SET `IP` = 'YOUR_GAME_SERVER_IP', `Port` = 5816
-- WHERE `Name` = 'CoPrivate';

-- This report is safe to run without any edits.
SELECT `Name`, `IP`, `Port`
FROM `servers`
WHERE `Name` = 'CoPrivate';
