DROP TABLE IF EXISTS `mappool_template`;
CREATE TABLE `mappool_template`
(
  `map` smallint(5) unsigned NOT NULL DEFAULT '0',
  `poolId` mediumint(8) unsigned NOT NULL,
  `poolType` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `phaseMask` int(10) unsigned NOT NULL DEFAULT '1',
  `spawnMask` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `minLimit` int(10) unsigned NOT NULL DEFAULT '1',
  `maxLimit` int(10) unsigned NOT NULL DEFAULT '1',
  `MovementType` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `spawnDist` float NOT NULL DEFAULT '0',
  `spawntimeSecsMin` int(10) unsigned NOT NULL DEFAULT '0',
  `spawntimeSecsMax` int(10) unsigned NOT NULL DEFAULT '0',
  `spawntimeSecsFast` int(10) unsigned NOT NULL DEFAULT '0',
  `corpsetimeSecsLoot` int(10) unsigned NOT NULL DEFAULT '0',
  `corpsetimeSecsNoLoot` int(10) unsigned NOT NULL DEFAULT '0',
  `description` varchar(255) NULL,
  PRIMARY KEY (`map`, `poolId`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='Pool Template';

DROP TABLE IF EXISTS `mappool_hierarchy`;
CREATE TABLE `mappool_hierarchy`
(
  `map` smallint(5) unsigned NOT NULL DEFAULT '0',
  `poolId` mediumint(8) unsigned NOT NULL,
  `childPoolId` mediumint(8) unsigned NOT NULL,
  PRIMARY KEY (`map`, `poolId`, `childPoolId`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='Pool Hierarchy';

DROP TABLE IF EXISTS `mappool_spawnpoints`;
CREATE TABLE `mappool_spawnpoints`
(
  `map` smallint(5) unsigned NOT NULL DEFAULT '0',
  `pointId` int(10) unsigned NOT NULL,
  `zoneId` smallint(5) unsigned NOT NULL DEFAULT '0',
  `areaId` smallint(5) unsigned NOT NULL DEFAULT '0',
  `gridId` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `positionX` float NOT NULL DEFAULT '0',
  `positionY` float NOT NULL DEFAULT '0',
  `positionZ` float NOT NULL DEFAULT '0',
  `orientation` float NOT NULL DEFAULT '0',
  `rotation0` float NOT NULL DEFAULT '0',
  `rotation1` float NOT NULL DEFAULT '0',
  `rotation2` float NOT NULL DEFAULT '0',
  `rotation3` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`map`, `pointId`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='Pool Spawn Points';

DROP TABLE IF EXISTS `mappool_spawns`;
CREATE TABLE `mappool_spawns`
(
  `map` smallint(5) unsigned NOT NULL DEFAULT '0',
  `poolId` mediumint(8) unsigned NOT NULL,
  `pointId` int(10) unsigned NOT NULL,
  PRIMARY KEY (`map`, `poolId`, `pointId`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='Pool Spawn Mapping';

DROP TABLE IF EXISTS `mappool_creature`;
CREATE TABLE `mappool_creature`
(
  `map` smallint(5) unsigned NOT NULL DEFAULT '0',
  `poolId` mediumint(8) unsigned NOT NULL,
  `entry` mediumint(8) unsigned NOT NULL,
  `chance` float NOT NULL DEFAULT '0',
  `modelId` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `equipmentId` tinyint(3) NOT NULL DEFAULT '0',
  `currentWaypoint` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `curHealth` int(10) unsigned NOT NULL DEFAULT '1',
  `curMana` int(10) unsigned NOT NULL DEFAULT '0',
  `npcFlag` int(10) unsigned NOT NULL DEFAULT '0',
  `unitFlags` int(10) unsigned NOT NULL DEFAULT '0',
  `dynamicFlags` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`map`, `poolId`, `entry`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='Pool Creature Data';

DROP TABLE IF EXISTS `mappool_gameobject`;
CREATE TABLE `mappool_gameobject`
(
  `map` smallint(5) unsigned NOT NULL DEFAULT '0',
  `poolId` mediumint(8) unsigned NOT NULL,
  `entry` mediumint(8) unsigned NOT NULL,
  `chance` float NOT NULL DEFAULT '0',
  `animProgress` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `state` tinyint(3) unsigned NOT NULL DEFAULT '1',
  PRIMARY KEY (`map`, `poolId`, `entry`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='Pool GameObject Data';

DROP TABLE IF EXISTS `mappool_gameobject_override`;
CREATE TABLE `mappool_gameobject_override`
(
  `map` smallint(5) unsigned NOT NULL DEFAULT '0',
  `poolId` mediumint(8) unsigned NOT NULL,
  `entry` mediumint(8) unsigned NOT NULL,
  `pointId` int(10) unsigned NOT NULL,
  `animProgress` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `state` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `spawntimeSecsMin` int(10) unsigned NOT NULL DEFAULT '0',
  `spawntimeSecsMax` int(10) unsigned NOT NULL DEFAULT '0',
  `AIName` varchar(64) NOT NULL DEFAULT '',
  `ScriptName` varchar(64) NOT NULL DEFAULT '',
  PRIMARY KEY (`map`, `poolId`, `entry`, `pointId`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='Pool GameObject Override Data';

DROP TABLE IF EXISTS `mappool_creature_override`;
CREATE TABLE `mappool_creature_override`
(
  `map` smallint(5) unsigned NOT NULL DEFAULT '0',
  `poolId` mediumint(8) unsigned NOT NULL,
  `entry` mediumint(8) unsigned NOT NULL,
  `pointId` int(10) unsigned NOT NULL,
  `modelId` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `equipmentId` tinyint(3) NOT NULL DEFAULT '0',
  `currentWaypoint` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `curHealth` int(10) unsigned NOT NULL DEFAULT '1',
  `curMana` int(10) unsigned NOT NULL DEFAULT '0',
  `npcFlag` int(10) unsigned NOT NULL DEFAULT '0',
  `unitFlags` int(10) unsigned NOT NULL DEFAULT '0',
  `dynamicFlags` int(10) unsigned NOT NULL DEFAULT '0',
  `MovementType` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `spawnDist` float NOT NULL DEFAULT '0',
  `spawntimeSecsMin` int(10) unsigned NOT NULL DEFAULT '0',
  `spawntimeSecsMax` int(10) unsigned NOT NULL DEFAULT '0',
  `corpsetimeSecsLoot` int(10) unsigned NOT NULL DEFAULT '0',
  `corpsetimeSecsNoLoot` int(10) unsigned NOT NULL DEFAULT '0',
  `AIName` varchar(64) NOT NULL DEFAULT '',
  `ScriptName` varchar(64) NOT NULL DEFAULT '',
  PRIMARY KEY (`map`, `poolId`, `entry`, `pointId`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='Pool Creature Override Data';
