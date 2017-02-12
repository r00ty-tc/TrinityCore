-- Add RBAC for Pooling
DELETE FROM `rbac_permissions`
WHERE `id` BETWEEN 875 AND 880;

INSERT INTO `rbac_permissions`
(`id`, `name`)
VALUES
(875, 'Command: pool'),
(876, 'Command: pool dump'),
(877, 'Command: pool respawn'),
(878, 'Command: pool despawn'),
(879, 'Command: pool reseed'),
(880, 'Command: pool list');

DELETE FROM `rbac_linked_permissions`
WHERE `linkedId` BETWEEN 875 AND 880;

-- Gamemasters can list/dump admins can spawn/despawn/reseed
INSERT INTO `rbac_linked_permissions`
(`id`, `linkedId`)
VALUES
(197, 875),
(197, 876),
(196, 877),
(196, 878),
(196, 879),
(197, 880);