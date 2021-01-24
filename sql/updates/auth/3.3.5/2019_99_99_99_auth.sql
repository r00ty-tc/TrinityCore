-- Add RBAC for Pooling
DELETE FROM `rbac_permissions`
WHERE `id` BETWEEN 884 AND 889;

INSERT INTO `rbac_permissions`
(`id`, `name`)
VALUES
(884, 'Command: pool'),
(885, 'Command: pool dump'),
(886, 'Command: pool respawn'),
(887, 'Command: pool despawn'),
(888, 'Command: pool reseed'),
(889, 'Command: pool list');

DELETE FROM `rbac_linked_permissions`
WHERE `linkedId` BETWEEN 875 AND 880;

-- Gamemasters can list/dump admins can spawn/despawn/reseed
INSERT INTO `rbac_linked_permissions`
(`id`, `linkedId`)
VALUES
(197, 884),
(197, 885),
(196, 886),
(196, 887),
(196, 888),
(197, 889);