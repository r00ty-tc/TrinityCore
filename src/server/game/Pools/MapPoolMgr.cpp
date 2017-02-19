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
#include "World.h"

MapPoolMgr::MapPoolMgr(Map* map)
{
    ownerMapId = map->GetId();
    ownerInstanceId = map->GetInstanceId();
    ownerMap = map;
}

MapPoolMgr::~MapPoolMgr()
{
    //uint32 creaturePools = 0;
    // First clear all references
    for (std::pair<uint32, MapPoolCreatureData*> mapResult : poolCreatureMap)
    {
        mapResult.second->parentPool = nullptr;
        mapResult.second->childPools.clear();
    }
    for (std::pair<uint32, MapPoolGameObjectData*> mapResult : poolGameObjectMap)
    {
        mapResult.second->parentPool = nullptr;
        mapResult.second->childPools.clear();
    }

    // Destroy Creature Pool Data
    for (std::pair<uint32, MapPoolCreatureData*> mapResult : poolCreatureMap)
    {
        for (MapPoolCreatureSpawn* spawnList : *mapResult.second->spawnList)
            delete spawnList;

        for (MapPoolCreatureInfo* infoList : *mapResult.second->infoList)
            delete infoList;

        delete mapResult.second->poolTemplate;
        mapResult.second->childPools.clear();
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
        mapResult.second->childPools.clear();
        delete mapResult.second;
        //++gameobjectPools;
    }
    poolGameObjectMap.clear();

    //TC_LOG_INFO("server.loading", "Destroyed %u creature pools and %u gameobject pools for map %u", creaturePools, gameobjectPools, ownerMapId);
}

void MapPoolMgr::LoadMapPools()
{
    TC_LOG_INFO("server.loading", "[Map %u] Loading pool data", ownerMapId);

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

            MapPoolCreatureData* thisPool = getCreaturePool(poolId);

            // Check if the pool map exists, create it if not
            if (thisPool == nullptr)
            {
                // Create new creature data container/structure
                thisPool = createPoolCreatureData();

                // Add to master map for pool
                poolCreatureMap.emplace(poolId, thisPool);
            }

            // Get the pool template
            MapPoolCreatureTemplate* thisTemplate = thisPool->poolTemplate;

            // Populate values
            thisTemplate->mapId = ownerMapId;
            thisTemplate->poolId = poolId;
            thisTemplate->phaseMask = fields[1].GetUInt32();
            thisTemplate->spawnMask = fields[2].GetUInt8();
            thisTemplate->minLimit = fields[3].GetUInt32();
            thisTemplate->maxLimit = fields[4].GetUInt32();
            thisTemplate->description = fields[5].GetString();
            thisPool->parentPool = nullptr;
            thisPool->childPools.clear();
            ++creaturePools;
        } while (result->NextRow());
    }

    // Read Creature Pool hierarchy
    //        0       1
    // SELECT poolId, childPoolId FROM mappool_creature_hierarchy WHERE map = ?
    stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_MAPPOOL_CREATURE_HIERARCHY);
    stmt->setUInt32(0, ownerMapId);
    if (PreparedQueryResult result = WorldDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 poolId = fields[0].GetUInt32();
            uint32 childPoolId = fields[1].GetUInt32();

            if (MapPoolCreatureData* thisPool = getCreaturePool(poolId))
            {
                if (MapPoolCreatureData* childPool = getCreaturePool(childPoolId))
                {
                    // Get the templates
                    MapPoolCreatureTemplate* thisTemplate = thisPool->poolTemplate;
                    MapPoolCreatureTemplate* childTemplate = childPool->poolTemplate;

                    // Add references, both ways
                    if (childPool->parentPool == nullptr)
                    {
                        if (!checkHierarchy(childPool, thisPool))
                        {
                            childPool->parentPool = thisPool;
                            thisPool->childPools.push_back(childPool);
                        }
                        else
                        {
                            TC_LOG_ERROR("server.loading", "[Map %u] Child Creature pool %u is a parent pool, preventing circular reference", ownerMapId, childPoolId);
                        }
                    }
                    else
                    {
                        TC_LOG_ERROR("server.loading", "[Map %u] Child Creature pool %u is already a member of parent pool %u", ownerMapId, childPoolId, childPool->parentPool->poolTemplate->poolId);
                    }
                }
                else
                {
                    TC_LOG_ERROR("server.loading", "[Map %u] Attempted to add Creature spawn to pool %u with non existent child pool %u", ownerMapId, poolId, childPoolId);
                }
            }
            else
            {
                TC_LOG_ERROR("server.loading", "[Map %u] Attempted to add Creature spawn to non existent pool %u", ownerMapId, poolId);
            }

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

            if (MapPoolCreatureData* thisPool = getCreaturePool(poolId))
            {
                // Get the spawnlist
                MapPoolCreatureSpawnList* thisSpawnList = thisPool->spawnList;

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
            }
            else
            {
                TC_LOG_ERROR("server.loading", "[Map %u] Attempted to add Creature spawn to non existent pool %u", ownerMapId, poolId);
            }

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

            if (MapPoolCreatureData* thisPool = getCreaturePool(poolId))
            {
                // Get the info list
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
            }
            else
            {
                TC_LOG_ERROR("server.loading", "[Map %u] Attempted to add Creature info to non existent pool %u", ownerMapId, poolId);
            }
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

            MapPoolGameObjectData* thisPool = getGameObjectPool(poolId);
            // Check if the pool map exists, create it if not
            if (thisPool == nullptr)
            {
                // Create new gameobject data container/structure
                thisPool = createPoolGameObjectData();

                // Add to master map for pool
                poolGameObjectMap.emplace(poolId, thisPool);
            }

            // Get the pool template
            MapPoolGameObjectTemplate* thisTemplate = thisPool->poolTemplate;

            // Populate values
            thisTemplate->mapId = ownerMapId;
            thisTemplate->poolId = poolId;
            thisTemplate->phaseMask = fields[1].GetUInt32();
            thisTemplate->spawnMask = fields[2].GetUInt8();
            thisTemplate->minLimit = fields[3].GetUInt32();
            thisTemplate->maxLimit = fields[4].GetUInt32();
            thisTemplate->description = fields[5].GetString();
            thisPool->parentPool = nullptr;
            thisPool->childPools.clear();
            ++gameobjectPools;
        } while (result->NextRow());
    }

    // Read GameObject Pool hierarchy
    //        0       1
    // SELECT poolId, childPoolId FROM mappool_gameobject_hierarchy WHERE map = ?
    stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_MAPPOOL_GAMEOBJECT_HIERARCHY);
    stmt->setUInt32(0, ownerMapId);
    if (PreparedQueryResult result = WorldDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 poolId = fields[0].GetUInt32();
            uint32 childPoolId = fields[1].GetUInt32();

            if (MapPoolGameObjectData* thisPool = getGameObjectPool(poolId))
            {
                if (MapPoolGameObjectData* childPool = getGameObjectPool(childPoolId))
                {
                    // Get the templates
                    MapPoolGameObjectTemplate* thisTemplate = thisPool->poolTemplate;
                    MapPoolGameObjectTemplate* childTemplate = childPool->poolTemplate;

                    // Add references, both ways
                    if (childPool->parentPool == nullptr)
                    {
                        if (checkHierarchy(childPool, thisPool))
                        {
                            childPool->parentPool = thisPool;
                            thisPool->childPools.push_back(childPool);
                        }
                        else
                        {
                            TC_LOG_ERROR("server.loading", "[Map %u] Child Gameobject pool %u is a parent pool, preventing circular reference", ownerMapId, childPoolId);
                        }
                    }
                    else
                    {
                        TC_LOG_ERROR("server.loading", "[Map %u] Child Gameobject pool %u is already a member of parent pool %u", ownerMapId, childPoolId, childPool->parentPool->poolTemplate->poolId);
                    }
                }
                else
                {
                    TC_LOG_ERROR("server.loading", "[Map %u] Attempted to add Gameobject spawn to pool %u with non existent child pool %u", ownerMapId, poolId, childPoolId);
                }
            }
            else
            {
                TC_LOG_ERROR("server.loading", "[Map %u] Attempted to add Gameobject spawn to non existent pool %u", ownerMapId, poolId);
            }

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

            if (MapPoolGameObjectData* thisPool = getGameObjectPool(poolId))
            {
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
            }
            else
            {
                TC_LOG_ERROR("server.loading", "[Map %u] Attempted to add gameobject spawn to non existent pool %u", ownerMapId, poolId);
            }

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

            if (MapPoolGameObjectData* thisPool = getGameObjectPool(poolId))
            {
                // Get the info list
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
            }
            else
            {
                TC_LOG_ERROR("server.loading", "[Map %u] Attempted to add gameobject info to non existent pool %u", ownerMapId, poolId);
            }
        } while (result->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> [Map %u] Loaded %u creature pools/%u gameobject pools", ownerMapId, creaturePools, gameobjectPools);
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

bool MapPoolMgr::checkHierarchy(MapPoolCreatureData const* childPool, MapPoolCreatureData const* thisPool)
{
    MapPoolCreatureData const* currentLevel = thisPool;
    uint32 childPoolId = childPool->poolTemplate->poolId;

    do 
    {
        if (currentLevel->poolTemplate->poolId == childPoolId)
            return true;

        currentLevel = currentLevel->parentPool;

    } while (currentLevel != nullptr);
    return false;
}

bool MapPoolMgr::checkHierarchy(MapPoolGameObjectData const* childPool, MapPoolGameObjectData const* thisPool)
{
    MapPoolGameObjectData const* currentLevel = thisPool;
    uint32 childPoolId = childPool->poolTemplate->poolId;

    do
    {
        if (currentLevel->poolTemplate->poolId == childPoolId)
            return true;

        currentLevel = currentLevel->parentPool;

    } while (currentLevel != nullptr);
    return false;
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

MapPoolCreatureData* MapPoolMgr::getCreaturePool(uint32 poolId)
{
    MapPoolCreatureData* result = nullptr;
    auto itr = poolCreatureMap.find(poolId);
    if (itr != poolCreatureMap.end())
        result = itr->second;

    return result;
}

MapPoolCreatureSpawn* MapPoolMgr::getCreatureSpawnPoint(uint32 poolId, uint32 spawnPoint)
{
    MapPoolCreatureSpawn* result = nullptr;
    if (MapPoolCreatureData* poolData = getCreaturePool(poolId))
    {
        for (auto itr = *poolData->spawnList->begin(); itr != *poolData->spawnList->end(); ++itr)
        {
            if (itr->pointId == spawnPoint)
            {
                result = itr;
                break;
            }
        }
    }
    return result;
}

MapPoolCreatureInfo* MapPoolMgr::getCreatureSpawnInfo(uint32 poolId, uint32 spawnInfoId)
{
    MapPoolCreatureInfo* result = nullptr;
    if (MapPoolCreatureData* poolData = getCreaturePool(poolId))
    {
        for (auto itr = *poolData->infoList->begin(); itr != *poolData->infoList->end(); ++itr)
        {
            if (itr->creatureId == spawnInfoId)
            {
                result = itr;
                break;
            }
        }
    }
    return result;
}

MapPoolGameObjectSpawn* MapPoolMgr::getGameObjectSpawnPoint(uint32 poolId, uint32 spawnPoint)
{
    MapPoolGameObjectSpawn* result = nullptr;
    if (MapPoolGameObjectData* poolData = getGameObjectPool(poolId))
    {
        for (auto itr = *poolData->spawnList->begin(); itr != *poolData->spawnList->end(); ++itr)
        {
            if (itr->pointId == spawnPoint)
            {
                result = itr;
                break;
            }
        }
    }
    return result;
}

MapPoolGameObjectInfo* MapPoolMgr::getGameObjectSpawnInfo(uint32 poolId, uint32 spawnInfoId)
{
    MapPoolGameObjectInfo* result = nullptr;
    if (MapPoolGameObjectData* poolData = getGameObjectPool(poolId))
    {
        for (auto itr = *poolData->infoList->begin(); itr != *poolData->infoList->end(); ++itr)
        {
            if (itr->gameobjectId == spawnInfoId)
            {
                result = itr;
                break;
            }
        }
    }
    return result;
}

MapPoolGameObjectData* MapPoolMgr::getGameObjectPool(uint32 poolId)
{
    MapPoolGameObjectData* result = nullptr;
    auto itr = poolGameObjectMap.find(poolId);
    if (itr != poolGameObjectMap.end())
        result = itr->second;

    return result;
}

MapPoolCreatureData const* MapPoolMgr::GetCreaturePool(uint32 poolId)
{
    return getCreaturePool(poolId);
}

MapPoolGameObjectData const* MapPoolMgr::GetGameObjectPool(uint32 poolId)
{
    return getGameObjectPool(poolId);
}

void MapPoolMgr::LoadPoolRespawns()
{
    // Load creature respawns
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CREATURE_POOL_RESPAWNS);
    stmt->setUInt16(0, ownerMapId);
    stmt->setUInt32(1, ownerInstanceId);
    if (PreparedQueryResult result = CharacterDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 poolId = fields[0].GetUInt32();
            uint32 spawnPointId = fields[1].GetUInt32();
            uint32 respawnTime = fields[2].GetUInt32();

            if (MapPoolCreatureSpawn* spawnPoint = getCreatureSpawnPoint(poolId, spawnPointId))
                SaveCreaturePoolRespawnTime(poolId, 0, spawnPointId, time_t(respawnTime), ownerMap->GetZoneAreaGridId(Map::RespawnObjectType::OBJECT_TYPE_CREATURE, spawnPoint->positionX, spawnPoint->positionY, spawnPoint->positionZ), Trinity::ComputeGridCoord(spawnPoint->positionX, spawnPoint->positionY).GetId(), false);

        } while (result->NextRow());
    }

    // Load gameobject respawns
    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_GO_POOL_RESPAWNS);
    stmt->setUInt16(0, ownerMapId);
    stmt->setUInt32(1, ownerInstanceId);
    if (PreparedQueryResult result = CharacterDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 poolId = fields[0].GetUInt32();
            uint32 spawnPointId = fields[1].GetUInt32();
            uint32 respawnTime = fields[2].GetUInt32();

            if (MapPoolGameObjectSpawn* spawnPoint = getGameObjectSpawnPoint(poolId, spawnPointId))
                SaveCreaturePoolRespawnTime(poolId, 0, spawnPointId, time_t(respawnTime), ownerMap->GetZoneAreaGridId(Map::RespawnObjectType::OBJECT_TYPE_GAMEOBJECT, spawnPoint->positionX, spawnPoint->positionY, spawnPoint->positionZ), Trinity::ComputeGridCoord(spawnPoint->positionX, spawnPoint->positionY).GetId(), false);

        } while (result->NextRow());
    }

}

void MapPoolMgr::RespawnCellAreaZoneCreature(uint32 cellZoneAreaId)
{
    // Check we've not visited this Cell/Area/Zone recently
    auto timeItr = _cellAreaZoneLastRespawnedCreatureMap.find(cellZoneAreaId);
    if (timeItr != _cellAreaZoneLastRespawnedCreatureMap.end() && GetMSTimeDiffToNow(timeItr->second) < sWorld->getIntConfig(CONFIG_RESPAWN_MINCELLCHECKMS))
        return;

    // Store this time, so we don't check so often
    _cellAreaZoneLastRespawnedCreatureMap[cellZoneAreaId] = getMSTime();

    RespawnVector rv;
    switch (sWorld->getIntConfig(CONFIG_RESPAWN_ACTIVITYSCOPEGAMEOBJECT))
    {
        case RESPAWNSCOPE_CELL:
            getRespawnInfo(_creatureRespawnTimesByGridId, _creatureRespawnTimesByCellAreaZoneId, _creatureRespawnTimesByPoolSpawnPoint, rv, 0, 0, cellZoneAreaId, false);
            break;
        case RESPAWNSCOPE_AREA:
            getRespawnInfo(_creatureRespawnTimesByGridId, _creatureRespawnTimesByCellAreaZoneId, _creatureRespawnTimesByPoolSpawnPoint, rv, 0, 0, cellZoneAreaId, false);
            break;
        case RESPAWNSCOPE_ZONE:
            getRespawnInfo(_creatureRespawnTimesByGridId, _creatureRespawnTimesByCellAreaZoneId, _creatureRespawnTimesByPoolSpawnPoint, rv, 0, 0, cellZoneAreaId, false);
            break;
    }

    rv.clear();
    if (GetCreatureRespawnInfo(rv, 0, 0, cellZoneAreaId))
        RespawnCreatureList(rv);
}

void MapPoolMgr::RespawnCellAreaZoneGameObject(uint32 cellZoneAreaId)
{
    // Check we've not visited this Cell/Area/Zone recently
    auto timeItr = _cellAreaZoneLastRespawnedGameObjectMap.find(cellZoneAreaId);
    if (timeItr != _cellAreaZoneLastRespawnedGameObjectMap.end() && GetMSTimeDiffToNow(timeItr->second) < sWorld->getIntConfig(CONFIG_RESPAWN_MINCELLCHECKMS))
        return;

    // Store this time, so we don't check so often
    _cellAreaZoneLastRespawnedGameObjectMap[cellZoneAreaId] = getMSTime();

    RespawnVector rv;
    switch (sWorld->getIntConfig(CONFIG_RESPAWN_ACTIVITYSCOPEGAMEOBJECT))
    {
        case RESPAWNSCOPE_CELL:
            getRespawnInfo(_gameObjectRespawnTimesByGridId, _gameObjectRespawnTimesByCellAreaZoneId, _creatureRespawnTimesByPoolSpawnPoint, rv, 0, 0, cellZoneAreaId, false);
            break;
        case RESPAWNSCOPE_AREA:
            getRespawnInfo(_gameObjectRespawnTimesByGridId, _gameObjectRespawnTimesByCellAreaZoneId, _creatureRespawnTimesByPoolSpawnPoint, rv, 0, 0, cellZoneAreaId, false);
            break;
        case RESPAWNSCOPE_ZONE:
            getRespawnInfo(_gameObjectRespawnTimesByGridId, _gameObjectRespawnTimesByCellAreaZoneId, _creatureRespawnTimesByPoolSpawnPoint, rv, 0, 0, cellZoneAreaId, false);
            break;
    }

    rv.clear();
    if (GetGameObjectRespawnInfo(rv, 0, 0, cellZoneAreaId))
        RespawnGameObjectList(rv);
}

void MapPoolMgr::addRespawnInfo(respawnInfoMultiMap& gridList, respawnInfoMultiMap& cellAreaZoneList, respawnPoolInfoMap& spawnList, RespawnInfo& Info, bool replace)
{
    if (!Info.poolId)
        return;

    // Uniqueness checked on the spawnList as master key
    if (spawnList.find(RespawnPoolSpawnPointPair(Info.poolId, Info.lastPoolSpawnId)) == spawnList.end())
    {
        RespawnInfo* ri = new RespawnInfo();
        *ri = Info;
        spawnList[RespawnPoolSpawnPointPair(Info.poolId, Info.lastPoolSpawnId)] = ri;
        gridList.emplace(Info.gridId, ri);
        cellAreaZoneList.emplace(Info.cellAreaZoneId, ri);
        return;
    }
    else if (!replace)
    {
        // If a spawn is found, and we're not blindly replacing
        // Check if it is before our planned time. Only replace if not
        RespawnInfo* ri = spawnList[RespawnPoolSpawnPointPair(Info.poolId, Info.lastPoolSpawnId)];
        if (ri->originalRespawnTime > Info.originalRespawnTime)
        {
            deleteRespawnInfo(gridList, cellAreaZoneList, spawnList, Info.poolId, Info.lastPoolSpawnId, 0, 0, false);
            addRespawnInfo(gridList, cellAreaZoneList, spawnList, Info, false);
        }
        else
        {
            // Otherwise update respawn entry to this earlier time
            // This is really only so the DB is updated correctly
            Info.originalRespawnTime = ri->originalRespawnTime;
        }
    }

    if (replace)
    {
        // In case of replace, delete this spawn ID everywhere, then self call with no replace
        deleteRespawnInfo(gridList, cellAreaZoneList, spawnList, Info.poolId, Info.lastPoolSpawnId, 0, 0, false);
        addRespawnInfo(gridList, cellAreaZoneList, spawnList, Info, false);
    }
}

bool MapPoolMgr::getRespawnInfo(respawnInfoMultiMap const& gridList, respawnInfoMultiMap const& cellAreaZoneList, respawnPoolInfoMap const& spawnList, RespawnVector& RespawnData, uint32 poolId, uint32 spawnPointId, uint32 gridId, uint32 cellAreaZoneId, bool onlyDue)
{
    // If no criteria passed, then return either due respawns, or all respawns
    if (!poolId && !gridId && !cellAreaZoneId)
    {
        if (spawnList.begin() == spawnList.end())
            return false;

        for (respawnPoolInfoMap::const_iterator iter = spawnList.begin(); iter != spawnList.end();)
        {
            if (!onlyDue || time(NULL) >= iter->second->respawnTime)
                RespawnData.push_back(iter->second);
            ++iter;
        }
        return true;
    }

    if (poolId)
    {
        respawnPoolInfoMap::const_iterator iter = spawnList.find(RespawnPoolSpawnPointPair(poolId, spawnPointId));
        if (iter == spawnList.end())
            return false;

        if (!onlyDue || time(NULL) >= iter->second->respawnTime)
        {
            RespawnData.push_back(iter->second);
            return true;
        }
    }

    if (cellAreaZoneId && gridId)
        return false;

    if (cellAreaZoneId || gridId)
    {
        auto const spawnBounds = cellAreaZoneId ? cellAreaZoneList.equal_range(cellAreaZoneId) : gridList.equal_range(gridId);
        if (spawnBounds.first == spawnBounds.second)
            return false;

        for (respawnInfoMap::const_iterator iter = spawnBounds.first; iter != spawnBounds.second;)
        {
            if (!onlyDue || time(NULL) >= iter->second->respawnTime)
            {
                RespawnData.push_back(iter->second);
            }
            ++iter;
        }
        return true;
    }

    return false;
}

void MapPoolMgr::deleteRespawnInfo(respawnInfoMultiMap& gridList, respawnInfoMultiMap& cellAreaZoneList, respawnPoolInfoMap& spawnList, uint32 poolId, uint32 spawnPointId, uint32 gridId, uint32 cellAreaZoneId, bool onlyDue)
{
    // For delete, a bit more important to ensure only one present (or none to erase all)
    if ((poolId && (gridId || cellAreaZoneId)) || (gridId && (poolId || cellAreaZoneId)) || (cellAreaZoneId && (poolId || gridId)))
        ASSERT("too many active parameters passed to Map::deleteRespawnInfo");

    RespawnVector rv;
    if (getRespawnInfo(gridList, cellAreaZoneList, spawnList, rv, poolId, spawnPointId, gridId, cellAreaZoneId, onlyDue))
    {
        // Iterate through all elements to delete
        for (RespawnInfo* ri : rv)
        {
            // First, find element in grid level and erase it
            auto gridBound = gridList.equal_range(ri->gridId);
            for (auto iter = gridBound.first; iter != gridBound.second;)
            {
                if (iter->second->poolId == ri->poolId && iter->second->lastPoolSpawnId == ri->lastPoolSpawnId)
                    iter = gridList.erase(iter);
                else
                    ++iter;
            }

            // Next, find element in cell/area/zone level and erase it
            auto cellAreaZoneBound = cellAreaZoneList.equal_range(ri->cellAreaZoneId);
            for (auto iter = cellAreaZoneBound.first; iter != cellAreaZoneBound.second;)
            {
                if (iter->second->poolId == ri->poolId && iter->second->lastPoolSpawnId == ri->lastPoolSpawnId)
                    iter = cellAreaZoneList.erase(iter);
                else
                    ++iter;
            }

            // Delete element in spawn level
            spawnList.erase(RespawnPoolSpawnPointPair(ri->poolId, ri->lastPoolSpawnId));

            // Finally delete the pointer storage itself
            delete ri;
        }
    }
}

void MapPoolMgr::RespawnCellAreaZone(uint32 cellId, uint32 zoneId, uint32 areaId)
{
    switch (sWorld->getIntConfig(CONFIG_RESPAWN_ACTIVITYSCOPECREATURE))
    {
        case RESPAWNSCOPE_CELL:
            RespawnCellAreaZoneCreature(cellId);
            break;
        case RESPAWNSCOPE_AREA:
            RespawnCellAreaZoneCreature(areaId);
            break;
        case RESPAWNSCOPE_ZONE:
            RespawnCellAreaZoneCreature(zoneId);
            break;
        default:
            ASSERT("INVALID RESPAWN SCOPE");
    }

    switch (sWorld->getIntConfig(CONFIG_RESPAWN_ACTIVITYSCOPEGAMEOBJECT))
    {
        case RESPAWNSCOPE_CELL:
            RespawnCellAreaZoneGameObject(cellId);
            break;
        case RESPAWNSCOPE_AREA:
            RespawnCellAreaZoneGameObject(areaId);
            break;
        case RESPAWNSCOPE_ZONE:
            RespawnCellAreaZoneGameObject(zoneId);
            break;
        default:
            ASSERT("INVALID RESPAWN SCOPE");
    }
}

void MapPoolMgr::RespawnCreatureList(RespawnVector const& RespawnData, bool force)
{
    // @Todo pool spawn methods
}

void MapPoolMgr::RespawnGameObjectList(RespawnVector const& RespawnData, bool force)
{
    // @Todo pool spawn methods
}

bool MapPoolMgr::GetRespawnData(RespawnVector& results, Map::RespawnObjectType type, bool onlyDue, uint32 poolId, uint32 spawnPointId, uint32 grid, bool allMap, float x, float y, float z)
{
    // Obtain references to appropriate respawn stores
    respawnInfoMultiMap const& gridList = (type == Map::OBJECT_TYPE_CREATURE) ? _creatureRespawnTimesByGridId : _gameObjectRespawnTimesByGridId;
    respawnInfoMultiMap const& scopeList = (type == Map::OBJECT_TYPE_CREATURE) ? _creatureRespawnTimesByCellAreaZoneId : _gameObjectRespawnTimesByCellAreaZoneId;
    respawnPoolInfoMap const& spawnIdList = (type == Map::OBJECT_TYPE_CREATURE) ? _creatureRespawnTimesByPoolSpawnPoint : _gameObjectRespawnTimesByPoolSpawnPoint;

    // Spawn ID supplied
    if (poolId)
    {
        return getRespawnInfo(gridList, scopeList, spawnIdList, results, poolId, spawnPointId, 0, 0, onlyDue);
    }

    // All map, or grid only
    if (grid || allMap)
    {
        return getRespawnInfo(gridList, scopeList, spawnIdList, results, 0, grid, 0, onlyDue);
    }

    // By scope
    uint32 zoneAreaCellId = ownerMap->GetZoneAreaGridId(type, x, y, z);
    return getRespawnInfo(gridList, scopeList, spawnIdList, results, 0, 0, zoneAreaCellId, onlyDue);
}

void MapPoolMgr::DeleteRespawnTimes()
{
    DeleteCreatureRespawnInfo(0, 0, 0, false);
    DeleteGameObjectRespawnInfo(0, 0, 0, false);

    DeleteRespawnTimesInDB(ownerMapId, ownerInstanceId);
}

void MapPoolMgr::DeleteRespawnTimesInDB(uint16 mapId, uint32 instanceId)
{
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CREATURE_POOL_RESPAWN_BY_INSTANCE);
    stmt->setUInt16(0, mapId);
    stmt->setUInt32(1, instanceId);
    CharacterDatabase.Execute(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_GO_POOL_RESPAWN_BY_INSTANCE);
    stmt->setUInt16(0, mapId);
    stmt->setUInt32(1, instanceId);
    CharacterDatabase.Execute(stmt);
}

void MapPoolMgr::RemoveCreatureRespawnTime(uint32 poolId, uint32 spawnPointId, uint32 cellAreaZoneId, uint32 gridId, bool respawnCreature, SQLTransaction respawntrans)
{
    if (!poolId && !cellAreaZoneId && !gridId)
        return;

    RespawnVector rv;
    if (GetCreatureRespawnInfo(rv, poolId, spawnPointId, gridId, cellAreaZoneId, false))
    {
        SQLTransaction trans = respawntrans ? respawntrans : CharacterDatabase.BeginTransaction();

        // Delete all creature respawns for this grid/cell/zone/area. Grid load will handle it the normal way
        for (RespawnInfo* ri : rv)
        {
            PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CREATURE_POOL_RESPAWN);
            stmt->setUInt32(0, ri->poolId);
            stmt->setUInt32(1, ri->lastPoolSpawnId);
            stmt->setUInt16(2, ownerMapId);
            stmt->setUInt32(3, ownerInstanceId);
            trans->Append(stmt);
        }
        if (!respawntrans)
            CharacterDatabase.CommitTransaction(trans);

        // Do respawns if needed
        if (respawnCreature)
            RespawnCreatureList(rv, true);
        else
            DeleteCreatureRespawnInfo(poolId, spawnPointId, gridId, cellAreaZoneId, false);
    }
}

void MapPoolMgr::RemoveGORespawnTime(uint32 poolId, uint32 spawnPointId, uint32 cellAreaZoneId, uint32 gridId, bool respawnObject, SQLTransaction respawntrans)
{
    if (!poolId && !cellAreaZoneId && !gridId)
        return;

    RespawnVector rv;
    if (GetGameObjectRespawnInfo(rv, poolId, spawnPointId, gridId, cellAreaZoneId, false))
    {
        SQLTransaction trans = respawntrans ? respawntrans : CharacterDatabase.BeginTransaction();

        // Delete all gameobject respawns for this grid. Grid load will handle it the normal way
        for (RespawnInfo* ri : rv)
        {
            PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_GO_RESPAWN);
            stmt->setUInt32(0, ri->poolId);
            stmt->setUInt32(1, ri->lastPoolSpawnId);
            stmt->setUInt16(2, ownerMapId);
            stmt->setUInt32(3, ownerInstanceId);
            trans->Append(stmt);
        }
        if (!respawntrans)
            CharacterDatabase.CommitTransaction(trans);

        if (respawnObject)
            RespawnGameObjectList(rv, true);
        else
            DeleteGameObjectRespawnInfo(poolId, spawnPointId, gridId, cellAreaZoneId, false);
    }
}

void MapPoolMgr::SaveCreaturePoolRespawnTime(uint32 poolId, uint32 entry, uint32 spawnPointId, time_t respawnTime, uint32 cellAreaZoneId, uint32 gridId, bool WriteDB, bool replace, SQLTransaction respawntrans)
{
    if (!respawnTime)
    {
        // Delete only
        RemoveCreatureRespawnTime(poolId, spawnPointId, 0, 0, false, respawntrans);
        return;
    }

    time_t const timeNow = time(NULL);
    RespawnInfo ri;
    ri.spawnId = 0;
    ri.entry = entry;
    ri.poolId = poolId;
    ri.lastPoolSpawnId = spawnPointId;
    ri.respawnTime = respawnTime;
    ri.originalRespawnTime = respawnTime;
    ri.gridId = gridId;
    ri.cellAreaZoneId = cellAreaZoneId;
    ri.spawnDelay = respawnTime > timeNow ? respawnTime - timeNow : 0;
    ownerMap->AddCreatureRespawnInfo(ri, replace);

    if (!WriteDB)
        return;

    SaveCreaturePoolRespawnTimeDB(poolId, spawnPointId, ri.originalRespawnTime, respawntrans);
}

void MapPoolMgr::SaveCreaturePoolRespawnTimeDB(uint32 poolId, uint32 spawnPointId, time_t respawnTime, SQLTransaction respawntrans)
{
    // Just here for support of compatibility mode
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_CREATURE_POOL_RESPAWN);
    stmt->setUInt32(0, poolId);
    stmt->setUInt32(1, spawnPointId);
    stmt->setUInt32(2, uint32(respawnTime));
    stmt->setUInt16(3, ownerMapId);
    stmt->setUInt32(4, ownerInstanceId);
    if (respawntrans)
        respawntrans->Append(stmt);
    else
        CharacterDatabase.Execute(stmt);
}

void MapPoolMgr::SaveGameObjectPoolRespawnTime(uint32 poolId, uint32 entry, uint32 spawnPointId, time_t respawnTime, uint32 cellAreaZoneId, uint32 gridId, bool WriteDB, bool replace, SQLTransaction respawntrans)
{
    if (!respawnTime)
    {
        // Delete only
        RemoveGORespawnTime(poolId, spawnPointId, 0, 0, false, respawntrans);
        return;
    }

    time_t const timeNow = time(NULL);
    RespawnInfo ri;
    ri.spawnId = 0;
    ri.entry = entry;
    ri.poolId = poolId;
    ri.lastPoolSpawnId = spawnPointId;
    ri.respawnTime = respawnTime;
    ri.originalRespawnTime = respawnTime;
    ri.gridId = gridId;
    ri.cellAreaZoneId = cellAreaZoneId;
    ri.spawnDelay = respawnTime > timeNow ? respawnTime - timeNow : 0;
    AddGameObjectRespawnInfo(ri, replace);

    if (!WriteDB)
        return;

    SaveGameObjectPoolRespawnTimeDB(poolId, spawnPointId, ri.originalRespawnTime, respawntrans);
}

void MapPoolMgr::SaveGameObjectPoolRespawnTimeDB(uint32 poolId, uint32 spawnPointId, time_t respawnTime, SQLTransaction respawntrans)
{
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_GO_POOL_RESPAWN);
    stmt->setUInt32(0, poolId);
    stmt->setUInt32(1, spawnPointId);
    stmt->setUInt32(2, uint32(respawnTime));     // Note, use from ri, in case it was changed during add process
    stmt->setUInt16(3, ownerMapId);
    stmt->setUInt32(4, ownerInstanceId);
    if (respawntrans)
        respawntrans->Append(stmt);
    else
        CharacterDatabase.Execute(stmt);
}

MapPoolAssignedCreature const* MapPoolMgr::GetPoolForObject(Creature const* creature)
{
    auto itr = poolActiveCreatureMap.find(creature);
    return itr == poolActiveCreatureMap.end() ? nullptr : itr->second;
}

MapPoolAssignedGameObject const* MapPoolMgr::GetPoolForObject(GameObject const* gameobject)
{
    auto itr = poolActiveGameObjectMap.find(gameobject);
    return itr == poolActiveGameObjectMap.end() ? nullptr : itr->second;
}
