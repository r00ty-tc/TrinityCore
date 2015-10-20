DELETE FROM `rbac_permissions` WHERE `id` BETWEEN 853 AND 857;
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(853, 'Command: npc spawngroup'),
(854, 'Command: npc despawngroup'),
(855, 'Command: gobject spawngroup'),
(856, 'Command: gobject despawngroup'),
(857, 'Command: list respawns');

DELETE FROM `rbac_linked_permissions` WHERE `id` = 197 AND `linkedId` BETWEEN 853 AND 857;
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(197, 853),
(197, 854),
(197, 855),
(197, 856),
(197, 857);
