DROP TABLE IF EXISTS `creature_pool_respawn`;
CREATE TABLE `creature_pool_respawn`
(
  `map` smallint(5) unsigned NOT NULL DEFAULT '0',
  `poolId` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `instanceId` int(10) unsigned NOT NULL DEFAULT '0',
  `respawnTime` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`map`, `poolId`, `instanceId`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='Pool Creature Respawn';

DROP TABLE IF EXISTS `gameobject_pool_respawn`;
CREATE TABLE `gameobject_pool_respawn`
(
  `map` smallint(5) unsigned NOT NULL DEFAULT '0',
  `poolId` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `instanceId` int(10) unsigned NOT NULL DEFAULT '0',
  `respawnTime` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`map`, `poolId`, `instanceId`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='Pool Gameobject Respawn';
