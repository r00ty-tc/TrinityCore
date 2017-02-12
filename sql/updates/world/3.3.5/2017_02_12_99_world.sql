CREATE TABLE `mappool_creature_template`
(
  `map` smallint(5) unsigned NOT NULL DEFAULT '0',
  `poolId` mediumint(8) unsigned NOT NULL,
  `phaseMask` int(10) unsigned NOT NULL DEFAULT '1',
  `spawnMask` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `minLimit` int(10) unsigned NOT NULL DEFAULT '1',
  `maxLimit` int(10) unsigned NOT NULL DEFAULT '1',
  `description` varchar(255) NULL,
  PRIMARY KEY (`map`, `poolId`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='Pool Creature Template';

CREATE TABLE `mappool_gameobject_template`
(
  `map` smallint(5) unsigned NOT NULL DEFAULT '0',
  `poolId` mediumint(8) unsigned NOT NULL,
  `phaseMask` int(10) unsigned NOT NULL DEFAULT '1',
  `spawnMask` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `minLimit` int(10) unsigned NOT NULL DEFAULT '1',
  `maxLimit` int(10) unsigned NOT NULL DEFAULT '1',
  `description` varchar(255) NULL,
  PRIMARY KEY (`map`, `poolId`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='Pool GameObject Template';

CREATE TABLE `mappool_creature_spawns`
(
  `map` smallint(5) unsigned NOT NULL DEFAULT '0',
  `poolId` mediumint(8) unsigned NOT NULL,
  `pointId` mediumint(8) unsigned NOT NULL,
  `zoneId` smallint(5) unsigned NOT NULL DEFAULT '0',
  `areaId` smallint(5) unsigned NOT NULL DEFAULT '0',
  `positionX` float NOT NULL DEFAULT '0',
  `positionY` float NOT NULL DEFAULT '0',
  `positionZ` float NOT NULL DEFAULT '0',
  `orientation` float NOT NULL DEFAULT '0',
  `AINameOverrideEntry` mediumint(8) unsigned NOT NULL,
  `AINameOverride` varchar(64) NOT NULL DEFAULT '',
  `ScriptNameOverrideEntry` mediumint(8) unsigned NOT NULL,
  `ScriptNameOverride` varchar(64) NOT NULL DEFAULT '',
  PRIMARY KEY (`map`, `poolId`, `pointId`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='Pool Creature Spawn Points';

CREATE TABLE `mappool_gameobject_spawns`
(
  `map` smallint(5) unsigned NOT NULL DEFAULT '0',
  `poolId` mediumint(8) unsigned NOT NULL,
  `pointId` mediumint(8) unsigned NOT NULL,
  `zoneId` smallint(5) unsigned NOT NULL DEFAULT '0',
  `areaId` smallint(5) unsigned NOT NULL DEFAULT '0',
  `positionX` float NOT NULL DEFAULT '0',
  `positionY` float NOT NULL DEFAULT '0',
  `positionZ` float NOT NULL DEFAULT '0',
  `orientation` float NOT NULL DEFAULT '0',
  `rotation0` float NOT NULL DEFAULT '0',
  `rotation1` float NOT NULL DEFAULT '0',
  `rotation2` float NOT NULL DEFAULT '0',
  `rotation3` float NOT NULL DEFAULT '0',
  `AINameOverrideEntry` mediumint(8) unsigned NOT NULL,
  `AINameOverride` varchar(64) NOT NULL DEFAULT '',
  `ScriptNameOverrideEntry` mediumint(8) unsigned NOT NULL,
  `ScriptNameOverride` varchar(64) NOT NULL DEFAULT '',
  PRIMARY KEY (`map`, `poolId`, `pointId`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='Pool Gamobject Spawn Points';


CREATE TABLE `mappool_creature_info`
(
  `map` smallint(5) unsigned NOT NULL DEFAULT '0',
  `poolId` mediumint(8) unsigned NOT NULL,
  `creatureId` mediumint(8) unsigned NOT NULL,
  `creatureQualifier` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `chance` float NOT NULL DEFAULT '0',
  `modelId` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `equipmentId` tinyint(3) NOT NULL DEFAULT '0',
  `spawntimeSecs` int(10) unsigned NOT NULL DEFAULT '0',
  `corpsetimeSecsLoot` int(10) unsigned NOT NULL DEFAULT '0',
  `corpsetimeSecsNoLoot` int(10) unsigned NOT NULL DEFAULT '0',
  `spawnDist` float NOT NULL DEFAULT '0',
  `currentWaypoint` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `curHealth` int(10) unsigned NOT NULL DEFAULT '1',
  `curMana` int(10) unsigned NOT NULL DEFAULT '0',
  `MovementType` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `npcFlag` int(10) unsigned NOT NULL DEFAULT '0',
  `unitFlags` int(10) unsigned NOT NULL DEFAULT '0',
  `dynamicFlags` int(10) unsigned NOT NULL DEFAULT '0',
  `AINameOverride` varchar(64) NOT NULL DEFAULT '',
  `ScriptNameOverride` varchar(64) NOT NULL DEFAULT '',
  PRIMARY KEY (`map`, `poolId`, `creatureId`, `creatureQualifier`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='Pool Creature Data';

CREATE TABLE `mappool_gameobject_info`
(
  `map` smallint(5) unsigned NOT NULL DEFAULT '0',
  `poolId` mediumint(8) unsigned NOT NULL,
  `gameobjectId` mediumint(8) unsigned NOT NULL,
  `gameobjectQualifier` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `chance` float NOT NULL DEFAULT '0',
  `animProgress` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `state` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `AINameOverride` varchar(64) NOT NULL DEFAULT '',
  `ScriptNameOverride` varchar(64) NOT NULL DEFAULT '',
  PRIMARY KEY (`map`, `poolId`, `gameobjectId`, `gameobjectQualifier`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='Pool GameObject Data';
