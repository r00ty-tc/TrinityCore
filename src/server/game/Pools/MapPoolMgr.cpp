/*
* Copyright (C) 2008-2017 TrinityCore <http://www.trinitycore.org/>
* Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
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

#include "MapPoolMgr.h"
#include "ObjectMgr.h"

MapPoolMgr::MapPoolMgr(Map* map)
{
    ownerMapId = map->GetId();
    ownerMap = map;
}

MapPoolMgr::~MapPoolMgr()
{
    //uint32 creaturePools = 0;
    // Destroy Creature Pool Data
    for (std::pair<uint32, MapPoolCreatureData*> mapResult : poolCreatureMap)
    {
        for (MapPoolCreatureSpawn* spawnList : *mapResult.second->spawnList)
            delete spawnList;

        for (MapPoolCreatureInfo* infoList : *mapResult.second->infoList)
            delete infoList;

        delete mapResult.second->poolTemplate;
        delete mapResult.second;
        //++creaturePools;
    }
    poolCreatureMap.clear();

    //uint32 gameobjectPools = 0;
    // Destroy GO Pool Data
    for (std::pair<uint32, MapPoolGameObjectData*> mapResult : poolGameObjectMap)
    {
        for (MapPoolGameObjectSpawn* spawnList : *mapResult.second->spawnList)
            delete spawnList;

        for (MapPoolGameObjectInfo* infoList : *mapResult.second->infoList)
            delete infoList;

        delete mapResult.second->poolTemplate;
        delete mapResult.second;
        //++gameobjectPools;
    }
    poolGameObjectMap.clear();

    //TC_LOG_INFO("server.loading", "Destroyed %u creature pools and %u gameobject pools for map %u", creaturePools, gameobjectPools, ownerMapId);
}

void MapPoolMgr::LoadMapPools()
{
    TC_LOG_INFO("server.loading", "Loading pool data for map %u", ownerMapId);

    uint32 creaturePools = 0;
    uint32 gameobjectPools = 0;

    // Read Creature Pool Templates
    //        0       1          2          3         4         5
    // SELECT poolId, phaseMask, spawnMask, minLimit, maxLimit, description FROM mappool_creature_template WHERE map = ?
    PreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_MAPPOOL_CREATURE_TEMPLATE);
    stmt->setUInt32(0, ownerMapId);
    if (PreparedQueryResult result = WorldDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 poolId = fields[0].GetUInt32();

            // Check if the pool map exists, create it if not
            if (poolCreatureMap.find(poolId) == poolCreatureMap.end())
            {
                // Create new creature data container/structure
                MapPoolCreatureData* poolCreatureData = createPoolCreatureData();

                // Add to master map for pool
                poolCreatureMap.emplace(poolId, poolCreatureData);
            }

            // Get the pool template
            MapPoolCreatureTemplate* thisTemplate = poolCreatureMap[poolId]->poolTemplate;

            // Populate values
            thisTemplate->mapId = ownerMapId;
            thisTemplate->poolId = poolId;
            thisTemplate->phaseMask = fields[1].GetUInt32();
            thisTemplate->spawnMask = fields[2].GetUInt8();
            thisTemplate->minLimit = fields[3].GetUInt32();
            thisTemplate->maxLimit = fields[4].GetUInt32();
            thisTemplate->description = fields[5].GetString();
            ++creaturePools;
        } while (result->NextRow());
    }

    // Read Creature Pool Spawnpoints
    //        0       1        2       3       4          5          6          7            8                    9               10                       11
    // SELECT poolId, pointId, zoneId, areaId, positionX, positionY, positionZ, orientation, AINameOverrideEntry, AINameOverride, ScriptNameOverrideEntry, ScriptNameOverride FROM mappool_creature_spawns WHERE map = ?
    stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_MAPPOOL_CREATURE_SPAWNS);
    stmt->setUInt32(0, ownerMapId);
    if (PreparedQueryResult result = WorldDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 poolId = fields[0].GetUInt32();

            // Check if the pool map exists, create it if not
            if (poolCreatureMap.find(poolId) == poolCreatureMap.end())
            {
                // Create new creature data container/structure
                MapPoolCreatureData* poolCreatureData = createPoolCreatureData();

                // Add to master map for pool
                poolCreatureMap.emplace(poolId, poolCreatureData);
            }

            // Get the spawnlist
            MapPoolCreatureSpawnList* thisSpawnList = poolCreatureMap[poolId]->spawnList;

            // Create new spawn/populate
            MapPoolCreatureSpawn* thisSpawn = new MapPoolCreatureSpawn();
            thisSpawn->mapId = ownerMapId;
            thisSpawn->poolId = poolId;
            thisSpawn->pointId = fields[1].GetUInt32();
            thisSpawn->zoneId = fields[2].GetUInt16();
            thisSpawn->areaId = fields[3].GetUInt16();
            thisSpawn->positionX = fields[4].GetFloat();
            thisSpawn->positionY = fields[5].GetFloat();
            thisSpawn->positionZ = fields[6].GetFloat();
            thisSpawn->positionO = fields[7].GetFloat();
            thisSpawn->AINameOverrideEntry = fields[8].GetUInt32();
            thisSpawn->AINameOverride = fields[9].GetString();
            thisSpawn->ScriptNameOverrideEntry = fields[10].GetUInt32();
            thisSpawn->ScriptNameOverride = fields[11].GetString();

            // Add this spawn
            thisSpawnList->push_back(thisSpawn);

        } while (result->NextRow());
    }

    // Read Creature Pool Info
    //        0       1           2                  3       4        5            6              7                   8                     9          10
    // SELECT poolId, creatureId, creatureQualifier, chance, modelId, equipmentId, spawntimeSecs, corpsetimeSecsLoot, corpsetimeSecsNoLoot, spawnDist, currentWaypoint,
    // 11         12       13            14       15         16            17              18
    // curHealth, curMana, MovementType, npcFlag, unitFlags, dynamicFlags, AINameOverride, ScriptNameOverride FROM mappool_creature_info WHERE map = ?
    stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_MAPPOOL_CREATURE_INFO);
    stmt->setUInt32(0, ownerMapId);
    if (PreparedQueryResult result = WorldDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 poolId = fields[0].GetUInt32();

            // Check if the pool map exists, create it if not
            if (poolCreatureMap.find(poolId) == poolCreatureMap.end())
            {
                // Create new creature data container/structure
                MapPoolCreatureData* poolCreatureData = createPoolCreatureData();

                // Add to master map for pool
                poolCreatureMap.emplace(poolId, poolCreatureData);
            }

            // Either way, we'll have either an empty or part populated pool master list
            MapPoolCreatureInfoList* thisInfoList = poolCreatureMap[poolId]->infoList;

            // Create new info/populate
            MapPoolCreatureInfo* thisInfo = new MapPoolCreatureInfo();
            thisInfo->mapId = ownerMapId;
            thisInfo->poolId = poolId;
            thisInfo->creatureId = fields[1].GetUInt32();
            thisInfo->creatureQualifier = fields[2].GetUInt32();
            thisInfo->chance = fields[3].GetFloat();
            thisInfo->modelId = fields[4].GetUInt32();
            thisInfo->equipmentId = fields[5].GetUInt8();
            thisInfo->spawntimeSecs = fields[6].GetInt32();
            thisInfo->corpsetimeSecsLoot = fields[7].GetUInt32();
            thisInfo->corpsetimeSecsNoLoot = fields[8].GetUInt32();
            thisInfo->spawnDist = fields[9].GetFloat();
            thisInfo->currentWaypoint = fields[10].GetUInt32();
            thisInfo->curHealth = fields[11].GetUInt32();
            thisInfo->curMana = fields[12].GetUInt32();
            thisInfo->movementType = fields[13].GetUInt8();
            thisInfo->npcFlag = fields[14].GetUInt32();
            thisInfo->unitFlags = fields[15].GetUInt32();
            thisInfo->dynamicFlags = fields[16].GetUInt32();
            thisInfo->AINameOverride = fields[17].GetString();
            thisInfo->ScriptNameOverride = fields[18].GetString();

            // Add this info
            thisInfoList->push_back(thisInfo);
        } while (result->NextRow());
    }


    // Read GameObject Pool Templates
    //        0       1          2          3         4         5
    // SELECT poolId, phaseMask, spawnMask, minLimit, maxLimit, description FROM mappool_gameobject_template WHERE map = ?
    stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_MAPPOOL_GAMEOBJECT_TEMPLATE);
    stmt->setUInt32(0, ownerMapId);
    if (PreparedQueryResult result = WorldDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 poolId = fields[0].GetUInt32();

            // Check if the pool map exists, create it if not
            if (poolGameObjectMap.find(poolId) == poolGameObjectMap.end())
            {
                // Create new gameobject data container/structure
                MapPoolGameObjectData* poolCreatureData = createPoolGameObjectData();

                // Add to master map for pool
                poolGameObjectMap.emplace(poolId, poolCreatureData);
            }

            // Get the pool template
            MapPoolGameObjectTemplate* thisTemplate = poolGameObjectMap[poolId]->poolTemplate;

            // Populate values
            thisTemplate->mapId = ownerMapId;
            thisTemplate->poolId = poolId;
            thisTemplate->phaseMask = fields[1].GetUInt32();
            thisTemplate->spawnMask = fields[2].GetUInt8();
            thisTemplate->minLimit = fields[3].GetUInt32();
            thisTemplate->maxLimit = fields[4].GetUInt32();
            thisTemplate->description = fields[5].GetString();
            ++gameobjectPools;
        } while (result->NextRow());
    }

    // Read GameObject Pool Spawnpoints
    //        0       1        2       3       4          5          6          7            8          9          10         11         12
    // SELECT poolId, pointId, zoneId, areaId, positionX, positionY, positionZ, orientation, rotation0, rotation1, rotation2, rotation3, AINameOverrideEntry,
    // 13              14                       15
    // AINameOverride, ScriptNameOverrideEntry, ScriptNameOverride FROM mappool_gameobject_spawns WHERE map = ?
    stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_MAPPOOL_GAMEOBJECT_SPAWNS);
    stmt->setUInt32(0, ownerMapId);
    if (PreparedQueryResult result = WorldDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 poolId = fields[0].GetUInt32();

            // Check if the pool map exists, create it if not
            if (poolGameObjectMap.find(poolId) == poolGameObjectMap.end())
            {
                // Create new gameobject data container/structure
                MapPoolGameObjectData* poolGameObjectData = createPoolGameObjectData();

                // Add to master map for pool
                poolGameObjectMap.emplace(poolId, poolGameObjectData);
            }

            // Get the spawnlist
            MapPoolGameObjectSpawnList* thisSpawnList = poolGameObjectMap[poolId]->spawnList;

            // Create new spawn/populate
            MapPoolGameObjectSpawn* thisSpawn = new MapPoolGameObjectSpawn();
            thisSpawn->mapId = ownerMapId;
            thisSpawn->poolId = poolId;
            thisSpawn->pointId = fields[1].GetUInt32();
            thisSpawn->zoneId = fields[2].GetUInt16();
            thisSpawn->areaId = fields[3].GetUInt16();
            thisSpawn->positionX = fields[4].GetFloat();
            thisSpawn->positionY = fields[5].GetFloat();
            thisSpawn->positionZ = fields[6].GetFloat();
            thisSpawn->positionO = fields[7].GetFloat();
            thisSpawn->rotation0 = fields[8].GetFloat();
            thisSpawn->rotation1 = fields[9].GetFloat();
            thisSpawn->rotation2 = fields[10].GetFloat();
            thisSpawn->rotation3 = fields[11].GetFloat();
            thisSpawn->AINameOverrideEntry = fields[12].GetUInt32();
            thisSpawn->AINameOverride = fields[13].GetString();
            thisSpawn->ScriptNameOverrideEntry = fields[14].GetUInt32();
            thisSpawn->ScriptNameOverride = fields[15].GetString();

            // Add this spawn
            thisSpawnList->push_back(thisSpawn);

        } while (result->NextRow());
    }

    // Read GameObject Pool Info
    //        0       1             2                    3       4             5      6               7
    // SELECT poolId, gameobjectId, gameobjectQualifier, chance, animProgress, state, AINameOverride, ScriptNameOverride FROM mappool_gameobject_info WHERE map = ?
    stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_MAPPOOL_GAMEOBJECT_INFO);
    stmt->setUInt32(0, ownerMapId);
    if (PreparedQueryResult result = WorldDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 poolId = fields[0].GetUInt32();

            // Check if the pool map exists, create it if not
            if (poolGameObjectMap.find(poolId) == poolGameObjectMap.end())
            {
                // Create new gameobject data container/structure
                MapPoolGameObjectData* poolGameObjectData = createPoolGameObjectData();

                // Add to master map for pool
                poolGameObjectMap.emplace(poolId, poolGameObjectData);
            }

            // Either way, we'll have either an empty or part populated pool master list
            MapPoolGameObjectInfoList* thisInfoList = poolGameObjectMap[poolId]->infoList;

            // Create new info/populate
            MapPoolGameObjectInfo* thisInfo = new MapPoolGameObjectInfo();
            thisInfo->mapId = ownerMapId;
            thisInfo->poolId = poolId;
            thisInfo->gameobjectId = fields[1].GetUInt32();
            thisInfo->gameobjectQualifier = fields[2].GetUInt32();
            thisInfo->chance = fields[3].GetFloat();
            thisInfo->animProgress = fields[4].GetUInt8();
            thisInfo->state = fields[5].GetUInt8();
            thisInfo->AINameOverride = fields[6].GetString();
            thisInfo->ScriptNameOverride = fields[7].GetString();

            // Add this info
            thisInfoList->push_back(thisInfo);
        } while (result->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> Loaded %u creature pools/%u gameobject pools for map %u", creaturePools, gameobjectPools, ownerMapId);
}

MapPoolCreatureData* MapPoolMgr::createPoolCreatureData()
{
    // Create new creature data container
    MapPoolCreatureData* poolCreatureData = new MapPoolCreatureData();

    // Create new constructs inside
    poolCreatureData->poolTemplate = new MapPoolCreatureTemplate();
    poolCreatureData->spawnList = new MapPoolCreatureSpawnList();
    poolCreatureData->infoList = new MapPoolCreatureInfoList();
    return poolCreatureData;
}

MapPoolGameObjectData* MapPoolMgr::createPoolGameObjectData()
{
    // Create new creature data container
    MapPoolGameObjectData* poolCreatureData = new MapPoolGameObjectData();

    // Create new constructs inside
    poolCreatureData->poolTemplate = new MapPoolGameObjectTemplate();
    poolCreatureData->spawnList = new MapPoolGameObjectSpawnList();
    poolCreatureData->infoList = new MapPoolGameObjectInfoList();
    return poolCreatureData;
}
