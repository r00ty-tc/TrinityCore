/*
* Copyright (C) 2008-2017 TrinityCore <http://www.trinitycore.org/>
* Copyright (C) 2005-2009 MaNGOS <http://www.mangosproject.org/>
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TRINITY_MAPPOOLHANDLER_H
#define TRINITY_MAPPOOLHANDLER_H
#include "Define.h"
#include "Map.h"

struct MapPoolCreatureTemplate
{
    uint32 mapId;
    uint32 poolId;
    uint32 phaseMask;
    uint8 spawnMask;
    uint32 minLimit;
    uint32 maxLimit;
    std::string description;
};

struct MapPoolGameObjectTemplate
{
    uint32 mapId;
    uint32 poolId;
    uint32 phaseMask;
    uint8 spawnMask;
    uint32 minLimit;
    uint32 maxLimit;
    std::string description;
};

struct MapPoolCreatureSpawn
{
    uint32 mapId;
    uint32 poolId;
    uint32 pointId;
    uint16 zoneId;
    uint16 areaId;
    float positionX;
    float positionY;
    float positionZ;
    float positionO;
    uint32 AINameOverrideEntry;
    std::string AINameOverride;
    uint32 ScriptNameOverrideEntry;
    std::string ScriptNameOverride;
};

typedef std::vector<MapPoolCreatureSpawn*> MapPoolCreatureSpawnList;

struct MapPoolGameObjectSpawn
{
    uint32 mapId;
    uint32 poolId;
    uint32 pointId;
    uint16 zoneId;
    uint16 areaId;
    float positionX;
    float positionY;
    float positionZ;
    float positionO;
    float rotation0;
    float rotation1;
    float rotation2;
    float rotation3;
    uint32 AINameOverrideEntry;
    std::string AINameOverride;
    uint32 ScriptNameOverrideEntry;
    std::string ScriptNameOverride;
};

typedef std::vector<MapPoolGameObjectSpawn*> MapPoolGameObjectSpawnList;

struct MapPoolCreatureInfo
{
    uint32 mapId;
    uint32 poolId;
    uint32 creatureId;
    uint32 creatureQualifier;
    float chance;
    uint32 modelId;
    int8 equipmentId;
    uint32 spawntimeSecs;
    uint32 corpsetimeSecsLoot;
    uint32 corpsetimeSecsNoLoot;
    float spawnDist;
    uint32 currentWaypoint;
    uint32 curHealth;
    uint32 curMana;
    uint8 movementType;
    uint32 npcFlag;
    uint32 unitFlags;
    uint32 dynamicFlags;
    std::string AINameOverride;
    std::string ScriptNameOverride;
};

typedef std::vector<MapPoolCreatureInfo*> MapPoolCreatureInfoList;

struct MapPoolGameObjectInfo
{
    uint32 mapId;
    uint32 poolId;
    uint32 gameobjectId;
    uint32 gameobjectQualifier;
    float chance;
    uint8 animProgress;
    uint8 state;
    std::string AINameOverride;
    std::string ScriptNameOverride;
};

typedef std::vector<MapPoolGameObjectInfo*> MapPoolGameObjectInfoList;

struct MapPoolCreatureData
{
    MapPoolCreatureTemplate* poolTemplate;
    MapPoolCreatureSpawnList* spawnList;
    MapPoolCreatureInfoList* infoList;
};

struct MapPoolGameObjectData
{
    MapPoolGameObjectTemplate* poolTemplate;
    MapPoolGameObjectSpawnList* spawnList;
    MapPoolGameObjectInfoList* infoList;
};

typedef std::unordered_map<uint32, MapPoolCreatureData*> MapPoolCreatureMap;
typedef std::unordered_map<uint32, MapPoolGameObjectData*> MapPoolGameObjectMap;

class TC_GAME_API MapPoolMgr
{

private:
    Map* ownerMap;
    uint32 ownerMapId;
    MapPoolCreatureMap poolCreatureMap;
    MapPoolGameObjectMap poolGameObjectMap;
    MapPoolCreatureData* createPoolCreatureData();
    MapPoolGameObjectData* createPoolGameObjectData();

public:
    MapPoolMgr(Map* map);
    ~MapPoolMgr();
    void LoadMapPools();

};

#endif
