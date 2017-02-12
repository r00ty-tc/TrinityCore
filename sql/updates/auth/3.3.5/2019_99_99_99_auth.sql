-- Add RBAC for Pooling
DELETE FROM `rbac_permissions`
WHERE `id` BETWEEN 875 AND 880;

INSERT INTO `rbac_permissions`
(`id`, `name`)
VALUES
(880, 'Command: pool'),
(881, 'Command: pool dump'),
(882, 'Command: pool respawn'),
(883, 'Command: pool despawn'),
(884, 'Command: pool reseed'),
(885, 'Command: pool list');

DELETE FROM `rbac_linked_permissions`
WHERE `linkedId` BETWEEN 875 AND 880;

-- Gamemasters can list/dump admins can spawn/despawn/reseed
INSERT INTO `rbac_linked_permissions`
(`id`, `linkedId`)
VALUES
(197, 880),
(197, 881),
(196, 882),
(196, 883),
(196, 884),
(197, 885);