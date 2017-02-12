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

#include "DatabaseEnv.h"
#include "MapPoolMgr.h"
#include "Log.h"
#include "ObjectMgr.h"
#include <combaseapi.h>

MapPoolMgr::MapPoolMgr(Map* map)
{
    ownerMapId = map->GetId();
    ownerMap = map;
}

MapPoolMgr::~MapPoolMgr()
{
    //uint32 creaturePools = 0;
    // First clear all references
    for (std::pair<uint32, MapPoolEntry> mapResult : _poolMap)
    {
        mapResult.second.parentPool = nullptr;
        mapResult.second.topPool = nullptr;

        // Don't iterate childs
        mapResult.second.childPools.clear();

        // Delete all items (Creatures/GOs)
        for (auto item : mapResult.second.itemList)
            delete item;

        // Clear item list
        mapResult.second.itemList.clear();

        // Clear spawn list (actual spawns will be deleted in Map context)
        mapResult.second.spawnList.clear();
    }

    // Finally clear the pool Map itself
    _poolMap.clear();
}

void MapPoolMgr::LoadMapPools()
{
    TC_LOG_INFO("server.loading", "[Map %u] Loading pool data", ownerMapId);

    uint32 pools = 0;

    // Read Pool Templates
    //        0       1          2          3         4         5         6             7          8                 9
    // SELECT poolId, poolType, phaseMask, spawnMask, minLimit, maxLimit, MovementType, spawnDist, spawntimeSecsMin, spawntimeSecsMax, 
    //        10                 11                  12                    13
    //        spawntimeSecsFast, corpsetimeSecsLoot, corpsetimeSecsNoLoot, description FROM mappool_template WHERE map = ?
    PreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_MAPPOOL_TEMPLATE);
    stmt->setUInt32(0, ownerMapId);
    if (PreparedQueryResult result = WorldDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 poolId = fields[0].GetUInt32();

            if (fields[1].GetUInt8() > 1)
            {
                TC_LOG_ERROR("server.loading", "[Map %u] Error loading pool %u, type was %u. Allowed types [0,1]. This pool was ignored", ownerMapId, poolId, fields[1].GetUInt8());
                continue;
            }

            MapPoolEntry* thisPool = getPool(poolId);

            // Check if the pool map exists, create it if not
            if (thisPool == nullptr)
            {
                // Add to master map for pool (an empty pool object)
                _poolMap.emplace(poolId, MapPoolEntry());

                thisPool = getPool(poolId);
                ASSERT(thisPool, "emplaced pool not found FATAL");
            }

            // Get the pool template
            MapPoolTemplate& thisTemplate = thisPool->poolData;

            // Populate values for template
            thisTemplate.mapId = ownerMapId;
            thisTemplate.poolId = poolId;
            thisTemplate.phaseMask = fields[2].GetUInt32();
            thisTemplate.spawnMask = fields[3].GetUInt8();
            thisTemplate.minLimit = fields[4].GetUInt32();
            thisTemplate.maxLimit = fields[5].GetUInt32();
            thisTemplate.movementType = fields[6].GetUInt8();
            thisTemplate.spawnDist = fields[7].GetFloat();
            thisTemplate.spawntimeSecsMin = fields[8].GetUInt32();
            thisTemplate.spawntimeSecsMax = fields[9].GetUInt32();
            thisTemplate.spawntimeSecsFast = fields[10].GetUInt32();
            thisTemplate.corpsetimeSecsLoot = fields[11].GetUInt32();
            thisTemplate.corpsetimeSecsNoLoot = fields[12].GetUInt32();
            thisTemplate.description = fields[13].GetString();

            // Populate pool base values and initialize pool
            thisPool->type = static_cast<PoolType>(fields[1].GetUInt8());
            thisPool->parentPool = nullptr;
            thisPool->topPool = nullptr;
            thisPool->childPools.clear();
            thisPool->spawnList.clear();
            ++pools;
        } while (result->NextRow());
    }

    // Read Pool hierarchy
    //        0       1
    // SELECT poolId, childPoolId FROM mappool_hierarchy WHERE map = ?
    stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_MAPPOOL_HIERARCHY);
    stmt->setUInt32(0, ownerMapId);
    if (PreparedQueryResult result = WorldDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 poolId = fields[0].GetUInt32();
            uint32 childPoolId = fields[1].GetUInt32();

            if (MapPoolEntry* thisPool = getPool(poolId))
            {
                if (thisPool->CheckHierarchy(childPoolId))
                {
                    TC_LOG_ERROR("server.loading", "[Map %u] Child Pool %u is already a part of the hierarchy of pool %u. Skipping this relation", ownerMapId, childPoolId, poolId);
                    continue;
                }

                if (MapPoolEntry* childPool = getPool(childPoolId))
                {
                    thisPool->childPools.push_back(childPool);
                    childPool->parentPool = thisPool;
                }
                else
                {
                    TC_LOG_ERROR("server.loading", "[Map %u] Attempted to add spawn to pool %u with non existent child pool %u", ownerMapId, poolId, childPoolId);
                }
            }
            else
            {
                TC_LOG_ERROR("server.loading", "[Map %u] Attempted to add child pool %u to non existent pool %u", ownerMapId, childPoolId, poolId);
            }

        } while (result->NextRow());

        // Now we need to sort out the top level for all pools
        for (auto poolItr : _poolMap)
            poolItr.second.topPool = poolItr.second.GetTopPool();
    }

    // Read Pool Spawnpoints
    //        0       1
    // SELECT poolId, pointId, FROM mappool_spawns WHERE map = ?
    stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_MAPPOOL_SPAWNS);
    stmt->setUInt32(0, ownerMapId);
    if (PreparedQueryResult result = WorldDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 poolId = fields[0].GetUInt32();
            uint32 pointId = fields[1].GetUInt32();

            if (MapPoolEntry* thisPool = getPool(poolId))
            {
                // Check if point already found (shouldn't be possible)
                for (MapPoolSpawnPoint* pointData : thisPool->spawnList)
                {
                    if (pointData->pointId == pointId)
                    {
                        TC_LOG_ERROR("server.loading", "[Map %u] Spawnpoint %u is already in pool %u, skipping this point", ownerMapId, pointId, poolId);
                        continue;
                    }
                }

                // Add the point if found
                if (MapPoolSpawnPoint* thisPoint = ownerMap->GetSpawnPoint(pointId))
                {
                    thisPool->spawnList.push_back(thisPoint);
                    TC_LOG_ERROR("server.loading", "[Map %u] Spawnpoint %u was not found, unable to add to pool %u", ownerMapId, pointId, poolId);
                }
            }
            else
            {
                TC_LOG_ERROR("server.loading", "[Map %u] Attempted to add spawn id %u to non existent pool %u", ownerMapId, pointId, poolId);
            }

        } while (result->NextRow());
    }

    // Read Creature Pool Info
    //        0       1      2       3        4            5                6          7        8        9          10
    // SELECT poolId, entry, chance, modelId, equipmentId, currentWaypoint, curHealth, curMana, npcFlag, unitFlags, dynamicFlags FROM mappool_creature WHERE map = ?
    stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_MAPPOOL_CREATURE_INFO);
    stmt->setUInt32(0, ownerMapId);
    if (PreparedQueryResult result = WorldDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 poolId = fields[0].GetUInt32();
            uint32 entry = fields[1].GetUInt32();

            if (MapPoolEntry* thisPool = getPool(poolId))
            {
                // Verify this pool is for creatures
                if (thisPool->type != POOLTYPE_CREATURE)
                {
                    TC_LOG_ERROR("server.loading", "[Map %u] Attempted to add creature with entry %u to pool %u, when pool is not for creatures", ownerMapId, entry, poolId);
                    continue;
                }

                bool errorState = false;
                // Check if this entry already exists in the pool
                for (MapPoolItem* item : thisPool->itemList)
                {
                    if (item->entry == entry)
                    {
                        TC_LOG_ERROR("server.loading", "[Map %u] Attempted to add creature with entry %u to pool %u, when it already exists in this pool", ownerMapId, entry, poolId);
                        errorState = true;
                        break;
                    }
                }

                if (errorState)
                    continue;

                // Verify creature is valid entry
                if (sObjectMgr->GetCreatureTemplate(entry) == nullptr)
                {
                    TC_LOG_INFO("server.loading", "[Map %u] Attempted to add creature with entry %u to pool %u. Entry was not a valid creature", ownerMapId, entry, poolId);
                    continue;
                }

                // Create new info/populate
                MapPoolCreature* thisInfo = new MapPoolCreature();
                thisInfo->mapId = ownerMapId;
                thisInfo->poolId = poolId;
                thisInfo->entry = fields[1].GetUInt32();
                thisInfo->chance = fields[2].GetFloat();
                thisInfo->modelId = fields[3].GetUInt32();
                thisInfo->equipmentId = fields[4].GetUInt8();
                thisInfo->currentWaypoint = fields[5].GetUInt32();
                thisInfo->curHealth = fields[6].GetUInt32();
                thisInfo->curMana = fields[7].GetUInt32();
                thisInfo->npcFlag = fields[8].GetUInt32();
                thisInfo->unitFlags = fields[9].GetUInt32();
                thisInfo->dynamicFlags = fields[10].GetUInt32();

                // Add this creature
                thisPool->itemList.push_back(thisInfo);
            }
            else
            {
                TC_LOG_ERROR("server.loading", "[Map %u] Attempted to add Creature info to non existent pool %u", ownerMapId, poolId);
            }
        } while (result->NextRow());
    }

    // Read GameObject Pool Info
    //        0       1      2       3             4
    // SELECT poolId, entry, chance, animProgress, state FROM mappool_gameobject WHERE map = ?
    stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_MAPPOOL_GAMEOBJECT_INFO);
    stmt->setUInt32(0, ownerMapId);
    if (PreparedQueryResult result = WorldDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 poolId = fields[0].GetUInt32();
            uint32 entry = fields[1].GetUInt32();

            if (MapPoolEntry* thisPool = getPool(poolId))
            {
                // Verify this pool is for gameobjects
                if (thisPool->type != POOLTYPE_GAMEOBJECT)
                {
                    TC_LOG_ERROR("server.loading", "[Map %u] Attempted to add gameobject with entry %u to pool %u, when pool is not for gameobjects", ownerMapId, entry, poolId);
                    continue;
                }

                bool errorState = false;
                // Check if this entry already exists in the pool
                for (MapPoolItem* item : thisPool->itemList)
                {
                    if (item->entry == entry)
                    {
                        TC_LOG_ERROR("server.loading", "[Map %u] Attempted to add gameobject with entry %u to pool %u, when it already exists in this pool", ownerMapId, entry, poolId);
                        errorState = true;
                        break;
                    }
                }

                if (errorState)
                    continue;

                // Verify gameobject is valid entry
                if (sObjectMgr->GetGameObjectTemplate(entry) == nullptr)
                {
                    TC_LOG_INFO("server.loading", "[Map %u] Attempted to add gameobject with entry %u to pool %u. Entry was not a valid gameobject", ownerMapId, entry, poolId);
                    continue;
                }

                //        0       1      2       3             4
                // SELECT poolId, entry, chance, animProgress, state FROM mappool_gameobject WHERE map = ?

                // Create new info/populate
                MapPoolGameObject* thisInfo = new MapPoolGameObject();
                thisInfo->mapId = ownerMapId;
                thisInfo->poolId = poolId;
                thisInfo->entry = fields[1].GetUInt32();
                thisInfo->chance = fields[2].GetFloat();
                thisInfo->animProgress = fields[3].GetUInt8();
                thisInfo->state = fields[4].GetUInt8();

                // Add this creature
                thisPool->itemList.push_back(thisInfo);
            }
            else
            {
                TC_LOG_ERROR("server.loading", "[Map %u] Attempted to add GameObject info to non existent pool %u", ownerMapId, poolId);
            }
        } while (result->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> [Map %u] Loaded %u pools", ownerMapId, pools);
}

MapPoolEntry* MapPoolMgr::getPool(uint32 poolId)
{
    auto pool = _poolMap.find(poolId);
    if (pool == _poolMap.end())
        return nullptr;

    return &pool->second;
}

MapPoolEntry const* MapPoolMgr::GetPool(uint32 poolId)
{
    return getPool(poolId);
}

// Checks if poolId is anywhere in the hierarchy already
bool MapPoolEntry::CheckHierarchy(uint32 poolId, bool callingSelf) const
{
    if (poolData.poolId == poolId)
        return true;

    if (parentPool != nullptr && !callingSelf)
    {
        // If this pool isn't the top pool, seek poolid, starting at the top
        return GetTopPool()->CheckHierarchy(poolId, true);
    }
    else
    {
        // Search each child pool
        for (MapPoolEntry const* pool : childPools)
        {
            if (pool->CheckHierarchy(poolId, true))
                return true;
        }
    }
    return false;
}
