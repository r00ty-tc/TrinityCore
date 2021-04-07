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
#include "MapPoolEntry.h"
#include "Log.h"
#include "ObjectMgr.h"
#include <boost/container/flat_set.hpp>
#include <Creature.h>
#include "CreatureData.h"
#include <GameObject.h>
#include "GameObjectData.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Random.h"

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
        mapResult.second.rootPool = nullptr;

        // Don't iterate child pools, they will be reached in linear fashion
        mapResult.second.childPools.clear();

        // Delete any specific override data
        for (auto itr : _poolCreatureOverrideMap)
            delete itr.second;

        _poolCreatureOverrideMap.clear();

        for (auto itr : _poolGameObjectOverrideMap)
            delete itr.second;

        _poolGameObjectOverrideMap.clear();

        // Delete all items (Creatures/GOs)
        for (auto item : mapResult.second.itemList)
        {
            if (auto creature = item->ToCreatureItem())
            {
                if (creature->overrideData)
                    delete creature->overrideData;
            }
            else if (auto go = item->ToGameObjectItem())
            {
                if (go->overrideData)
                    delete go->overrideData;
            }
            delete item;
        }

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
    //        10                 11                  12                    13         14
    //        spawntimeSecsFast, corpsetimeSecsLoot, corpsetimeSecsNoLoot, poolFlags, description FROM mappool_template WHERE map = ?
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

            MapPoolEntry* thisPool = _getPool(poolId);

            // Check if the pool map exists, create it if not
            if (thisPool == nullptr)
            {
                // Add to master map for pool (an empty pool object)
                _poolMap.emplace(poolId, MapPoolEntry());

                thisPool = _getPool(poolId);
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
            thisTemplate.flags = static_cast<PoolFlags>(fields[13].GetUInt32());
            thisTemplate.description = fields[14].GetString();

            // Populate pool base values and initialize pool
            thisPool->type = static_cast<PoolType>(fields[1].GetUInt8());
            thisPool->parentPool = nullptr;
            thisPool->rootPool = nullptr;
            thisPool->childPools.clear();
            thisPool->spawnList.clear();
            thisPool->activePool = (thisTemplate.flags & PoolFlags::POOL_FLAG_MANUAL_SPAWN) == 0    ;
            thisPool->SetOwnerPoolMgr(this);
            ++pools;
        } while (result->NextRow());
    }

    // Read Pool hierarchy
    //        0       1            2
    // SELECT poolId, childPoolId, chance FROM mappool_hierarchy WHERE map = ?
    stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_MAPPOOL_HIERARCHY);
    stmt->setUInt32(0, ownerMapId);
    if (PreparedQueryResult result = WorldDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 poolId = fields[0].GetUInt32();
            uint32 childPoolId = fields[1].GetUInt32();

            if (MapPoolEntry* thisPool = _getPool(poolId))
            {
                if (thisPool->CheckHierarchy(childPoolId))
                {
                    TC_LOG_ERROR("server.loading", "[Map %u] Child Pool %u is already a part of the hierarchy of pool %u. Skipping this relation", ownerMapId, childPoolId, poolId);
                    continue;
                }

                if (MapPoolEntry* childPool = _getPool(childPoolId))
                {
                    thisPool->childPools.push_back(childPool);
                    childPool->parentPool = thisPool;
                    childPool->chance = fields[2].GetFloat();
                }
                else
                {
                    TC_LOG_ERROR("server.loading", "[Map %u] Attempted to add non-existent child pool %u to pool %u.", ownerMapId, childPoolId, poolId);
                }
            }
            else
            {
                TC_LOG_ERROR("server.loading", "[Map %u] Attempted to add child pool %u to non existent pool %u", ownerMapId, childPoolId, poolId);
            }

        } while (result->NextRow());

        // Now we need to sort out the top level for all pools
        std::set<MapPoolEntry*> rootPools;
        for (auto poolItr = _poolMap.begin(); poolItr != _poolMap.end();)
        {
            auto test = const_cast<MapPoolEntry*>(poolItr->second.GetRootPool());
            poolItr->second.rootPool = test;
            if (rootPools.find(test) == rootPools.end())
                rootPools.emplace(test);
            ++poolItr;
        }

        // Trickle down some values from upper level pools, if unset
        for (auto poolItr = rootPools.begin(); poolItr != rootPools.end();)
        {
            UpdatePoolDefaults(*poolItr);
            ++poolItr;
        }
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

            if (MapPoolEntry* thisPool = _getPool(poolId))
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
                    thisPool->spawnList.push_back(thisPoint);
                else
                    TC_LOG_ERROR("server.loading", "[Map %u] Spawnpoint %u was not found, unable to add to pool %u", ownerMapId, pointId, poolId);
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

            if (MapPoolEntry* thisPool = _getPool(poolId))
            {
                // Fail if adding entries to a pool with children
                // Only botton level pools can have entries, upper/mid level pools are for organization of pools
                if (thisPool->childPools.size() != 0)
                {
                    TC_LOG_FATAL("server.loading", "[Map %u] Attempt to add creature info to pool %u with children", ownerMapId, poolId);
                    ABORT();
                }

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
                thisInfo->overrideData = nullptr;

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

            if (MapPoolEntry* thisPool = _getPool(poolId))
            {
                // Fail if adding entries to a pool with children
                // Only botton level pools can have entries, upper/mid level pools are for organization of pools
                if (thisPool->childPools.size() != 0)
                {
                    TC_LOG_FATAL("server.loading", "[Map %u] Attempt to add gameobject info to pool %u with children", ownerMapId, poolId);
                    ABORT();
                }

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
                thisInfo->overrideData = nullptr;

                // Add this creature
                thisPool->itemList.push_back(thisInfo);
            }
            else
            {
                TC_LOG_ERROR("server.loading", "[Map %u] Attempted to add GameObject info to non existent pool %u", ownerMapId, poolId);
            }
        } while (result->NextRow());
    }

    // Read Creature Override Info
    //        0       1      2        3        4            5                6          7        8        9          10
    // SELECT poolId, entry, pointId, modelId, equipmentId, currentWaypoint, curHealth, curMana, npcFlag, unitFlags, dynamicFlags, 
    //        11            12         13         14         15                16                17                  18
    //        MovementType, spawnDist, phaseMask, spawnMask, spawntimeSecsMin, spawntimeSecsMax, corpsetimeSecsLoot, corpsetimeSecsNoLoot, 
    //        19       20     21      22      23     24     25      26
    //        path_id, mount, bytes1, bytes2, emote, auras, AIName, ScriptName FROM mappool_creature_override WHERE map = ?
    stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_MAPPOOL_CREATURE_OVERRIDE);
    stmt->setUInt32(0, ownerMapId);
    if (PreparedQueryResult result = WorldDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 poolId = fields[0].GetUInt32();
            uint32 entry = fields[1].GetUInt32();
            uint32 pointId = fields[2].GetUInt32();

            // Reject bad combination
            if (entry == 0 && pointId == 0)
            {
                TC_LOG_ERROR("server.loading", "[Map %u] Pool %u has Creature override data with entry AND point set to 0. Now allowed, skipping", ownerMapId, poolId);
                continue;
            }

            // Fetch Pool
            MapPoolEntry* poolData = _getPool(poolId);
            if (!poolData)
            {
                TC_LOG_ERROR("server.loading", "[Map %u] Pool %u has Creature override data for spawn point %u, entry %u. Pool not found, skipping", ownerMapId, poolId, pointId, entry);
                continue;
            }

            MapPoolCreatureOverride* override = new MapPoolCreatureOverride();
            override->modelId = fields[3].GetUInt32();
            override->equipmentId = fields[4].GetUInt8();
            override->currentWaypoint = fields[5].GetUInt32();
            override->curHealth = fields[6].GetUInt32();
            override->curMana = fields[7].GetUInt32();
            override->npcFlag = fields[8].GetUInt32();
            override->unitFlags = fields[9].GetUInt32();
            override->dynamicFlags = fields[10].GetUInt32();
            override->movementType = fields[11].GetUInt8();
            override->spawnDist = fields[12].GetFloat();
            override->spawntimeSecsMin = fields[15].GetUInt32();
            override->spawntimeSecsMax = fields[16].GetUInt32();
            override->corpsetimeSecsLoot = fields[17].GetUInt32();
            override->corpsetimeSecsNoLoot = fields[18].GetUInt32();
            override->pathId = fields[19].GetUInt32();
            override->mount = fields[20].GetUInt32();
            override->bytes1 = fields[21].GetUInt32();
            override->bytes2 = fields[22].GetUInt32();
            override->phaseMask = fields[23].GetUInt32();
            override->spawnMask = fields[24].GetInt8();
            override->emote = fields[23].GetUInt32();
            override->aiName = fields[25].GetString();
            override->scriptName = fields[26].GetString();

            Tokenizer tokens(fields[24].GetString(), ' ');
            uint8 i = 0;
            override->auras.resize(tokens.size());
            for (Tokenizer::const_iterator itr = tokens.begin(); itr != tokens.end(); ++itr)
            {
                SpellInfo const* AdditionalSpellInfo = sSpellMgr->GetSpellInfo(atoul(*itr));
                if (!AdditionalSpellInfo)
                {
                    TC_LOG_ERROR("server.loading", "[Map %u] Creature (Entry: %u) has wrong spell %lu defined for pool %u", ownerMapId, entry, atoul(*itr), poolId);
                    continue;
                }

                if (AdditionalSpellInfo->HasAura(SPELL_AURA_CONTROL_VEHICLE))
                    TC_LOG_ERROR("server.loading", "[Map %u] Creature (Entry: %u) has SPELL_AURA_CONTROL_VEHICLE aura %lu defined for pool %u.", ownerMapId, entry, atoul(*itr), poolId);

                if (std::find(override->auras.begin(), override->auras.end(), atoul(*itr)) != override->auras.end())
                {
                    TC_LOG_ERROR("server.loading", "[Map %u] Creature (Entry: %u) has duplicate aura (spell %lu) in pool %u.", ownerMapId, entry, atoul(*itr), poolId);
                    continue;
                }

                override->auras[i++] = atoul(*itr);
            }


            if (pointId == 0)
            {
                MapPoolCreature* thisItem = nullptr;
                for (auto itr : poolData->itemList)
                {
                    if (itr->entry == entry)
                    {
                        thisItem = itr->ToCreatureItem();
                        break;
                    }
                }
                if (thisItem)
                {
                    thisItem->overrideData = override;
                }
                else
                {
                    TC_LOG_ERROR("server.loading", "[Map %u] Unable to find Creature entry %u in pool %u. Skipping adding override data", ownerMapId, entry, poolId);
                    delete override;
                }
            }
            else if (entry == 0)
            {
                MapPoolSpawnPoint* thisPoint = nullptr;
                for (auto itr : poolData->spawnList)
                {
                    if (itr->pointId == pointId)
                    {
                        thisPoint = itr;
                    }
                }

                if (thisPoint)
                {
                    thisPoint->creatureOverride = override;
                }
                else
                {
                    TC_LOG_ERROR("server.loading", "[Map %u] Unable to find spawn point %u in pool %u. Skipping adding Creature override data", ownerMapId, pointId, poolId);
                    delete override;
                }
            }
            else
            {
                // Check point and entry are valid
                MapPoolSpawnPoint* point = ownerMap->GetSpawnPoint(pointId);
                if (!point)
                {
                    TC_LOG_ERROR("server.loading", "[Map %u] Spawn point %u not found when adding spawn point for Creature entry %u to map", ownerMapId, pointId, entry);
                    delete override;
                    continue;
                }

                CreatureTemplate const* cTemp = sObjectMgr->GetCreatureTemplate(entry);

                if (!cTemp)
                {
                    TC_LOG_ERROR("server.loading", "[Map %u] Creature Template for Creature entry %u not found when adding spawn point for point %u to map", ownerMapId, entry, pointId);
                    delete override;
                    continue;
                }
                _poolCreatureOverrideMap[PointEntryPair(pointId, entry)] = override;
            }
        } while (result->NextRow());
    }

    // Read GameObject Override Info
    //        0       1      2        3             4      5          6          7                 8                 9
    // SELECT poolId, entry, pointId, animProgress, state, phaseMask, spawnMask, spawntimeSecsMin, spawntimeSecsMax, parent_rotation0, 
    //        10                11                12                13                14                 15      16
    //        parent_rotation1, parent_rotation2, parent_rotation3, invisibilityType, invisibilityValue, AIName, ScriptName FROM mappool_gameobject_override WHERE map = ?
    stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_MAPPOOL_GAMEOBJECT_OVERRIDE);
    stmt->setUInt32(0, ownerMapId);
    if (PreparedQueryResult result = WorldDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 poolId = fields[0].GetUInt32();
            uint32 entry = fields[1].GetUInt32();
            uint32 pointId = fields[2].GetUInt32();

            // Reject bad combination
            if (entry == 0 && pointId == 0)
            {
                TC_LOG_ERROR("server.loading", "[Map %u] Pool %u has GameObject override data with entry AND point set to 0. Now allowed, skipping", ownerMapId, poolId);
                continue;
            }

            // Fetch Pool
            MapPoolEntry* poolData = _getPool(poolId);
            if (!poolData)
            {
                TC_LOG_ERROR("server.loading", "[Map %u] Pool %u has GameObject override data for spawn point %u, entry %u. Pool not found, skipping", ownerMapId, poolId, pointId, entry);
                continue;
            }

            MapPoolGameObjectOverride* override = new MapPoolGameObjectOverride();
            override->animProgress = fields[3].GetUInt8();
            override->state = fields[4].GetUInt8();
            override->phaseMask = fields[5].GetUInt32();
            override->spawnMask = fields[6].GetInt8();
            override->spawntimeSecsMin = fields[7].GetUInt32();
            override->spawntimeSecsMax = fields[8].GetUInt32();
            override->parentRotation0 = fields[9].GetFloat();
            override->parentRotation1 = fields[10].GetFloat();
            override->parentRotation2 = fields[11].GetFloat();
            override->parentRotation3 = fields[12].GetFloat();
            override->invisibilityType = fields[13].GetUInt8();
            override->invisibilityValue = fields[14].GetUInt32();
            override->aiName = fields[15].GetString();
            override->scriptName = fields[16].GetString();

            if (entry != 0)
            {
                MapPoolGameObject* thisItem = nullptr;
                for (auto itr : poolData->itemList)
                {
                    if (itr->entry == entry)
                    {
                        thisItem = itr->ToGameObjectItem();
                        break;
                    }
                }
                if (thisItem)
                {
                    thisItem->overrideData = override;
                }
                else
                {
                    TC_LOG_ERROR("server.loading", "[Map %u] Unable to find GameObject entry %u in pool %u. Skipping adding override data", ownerMapId, entry, poolId);
                    delete override;
                }
            }
            else if (pointId != 0)
            {
                MapPoolSpawnPoint* thisPoint = nullptr;
                for (auto itr : poolData->spawnList)
                {
                    if (itr->pointId == pointId)
                    {
                        thisPoint = itr;
                    }
                }

                if (thisPoint)
                {
                    thisPoint->gameObjectOverride = override;
                }
                else
                {
                    TC_LOG_ERROR("server.loading", "[Map %u] Unable to find spawn point %u in pool %u. Skipping adding GameObject override data", ownerMapId, pointId, poolId);
                    delete override;
                }
            }
            else
            {
                // Check point and entry are valid
                MapPoolSpawnPoint* point = ownerMap->GetSpawnPoint(pointId);
                if (!point)
                {
                    TC_LOG_ERROR("server.loading", "[Map %u] Spawn point %u not found when adding spawn point for GameObject entry %u to map", ownerMapId, pointId, entry);
                    delete override;
                    continue;
                }

                GameObjectTemplate const* cTemp = sObjectMgr->GetGameObjectTemplate(entry);

                if (!cTemp)
                {
                    TC_LOG_ERROR("server.loading", "[Map %u] GameObject Template for entry %u not found when adding spawn point for point %u to map", ownerMapId, entry, pointId);
                    delete override;
                    continue;
                }
                _poolGameObjectOverrideMap[PointEntryPair(pointId, entry)] = override;
            }
        } while (result->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> [Map %u] Loaded %u pools", ownerMapId, pools);
}

void MapPoolMgr::SpawnMap()
{
    // Spawn map after initial load
    for (auto itr = _poolMap.begin(); itr != _poolMap.end(); ++itr)
    {
        MapPoolEntry* pool = &itr->second;
        // Only process top level pools
        if (!pool->parentPool)
            SpawnPool(pool);
    }
}

MapPoolEntry* MapPoolMgr::_getPool(uint32 poolId)
{
    auto pool = _poolMap.find(poolId);
    if (pool == _poolMap.end())
        return nullptr;

    return &pool->second;
}

MapPoolSpawnPoint* MapPoolMgr::_getSpawnPoint(MapPoolEntry const* pool, uint32 pointId)
{
    std::vector<MapPoolSpawnPoint*>::const_iterator spawnItr = std::find_if(pool->spawnList.begin(), pool->spawnList.end(), [pointId](std::vector<MapPoolSpawnPoint*>::value_type const& spawns)
    {
        return (spawns->pointId == pointId);
    });

    if (spawnItr == pool->spawnList.end() && !pool->parentPool)
        return nullptr;
    else if (spawnItr == pool->spawnList.end())
        return _getSpawnPoint(pool->parentPool, pointId);

    return *spawnItr;
}

MapPoolCreature* MapPoolMgr::_getSpawnCreature(MapPoolEntry const* pool, uint32 entry)
{
    std::vector<MapPoolItem*>::const_iterator entryItr = std::find_if(pool->itemList.begin(), pool->itemList.end(), [entry](std::vector<MapPoolItem*>::value_type const& item)
    {
        return (item->entry == entry);
    });

    if (entryItr == pool->itemList.end() || (*entryItr)->type != POOLTYPE_CREATURE)
        return nullptr;

    return (*entryItr)->ToCreatureItem();
}

MapPoolGameObject* MapPoolMgr::_getSpawnGameObject(MapPoolEntry* pool, uint32 entry)
{
    std::vector<MapPoolItem*>::const_iterator entryItr = std::find_if(pool->itemList.begin(), pool->itemList.end(), [entry](std::vector<MapPoolItem*>::value_type const& item)
    {
        return (item->entry == entry);
    });

    if (entryItr == pool->itemList.end() || (*entryItr)->type != POOLTYPE_GAMEOBJECT)
        return nullptr;

    return (*entryItr)->ToGameObjectItem();
}

MapPoolEntry const* MapPoolMgr::GetPool(uint32 poolId)
{
    return _getPool(poolId);
}

MapPoolEntry const* MapPoolMgr::GetRootPool(uint32 poolId)
{
    if (auto thisPool = _getPool(poolId))
        return (thisPool->parentPool ? thisPool->rootPool : thisPool);

    return nullptr;
}

std::vector<MapPoolEntry const*> MapPoolMgr::GetRootPools()
{
    std::vector<MapPoolEntry const*> rootPools;
    for (auto pool : _poolMap)
        if (pool.second.parentPool == nullptr)
            rootPools.push_back(_getPool(pool.second.poolData.poolId));

    return rootPools;
}

uint32 MapPoolMgr::GetRootPoolId(uint32 poolId)
{
    if (auto thisPool = GetRootPool(poolId))
        return thisPool->poolData.poolId;

    return 0;
}

MapPoolCreatureOverride* MapPoolMgr::_getCreatureOverride(uint32 pointId, uint32 entry)
{
    auto override = _poolCreatureOverrideMap.find(PointEntryPair(pointId, entry));
    if (override == _poolCreatureOverrideMap.end())
        return nullptr;

    return override->second;
}

MapPoolGameObjectOverride* MapPoolMgr::_getGameObjectOverride(uint32 pointId, uint32 entry)
{
    auto override = _poolGameObjectOverrideMap.find(PointEntryPair(pointId, entry));
    if (override == _poolGameObjectOverrideMap.end())
        return nullptr;

    return override->second;
}

MapPoolCreatureOverride const* MapPoolMgr::GetCreatureOverride(uint32 pointId, uint32 entry)
{
    return _getCreatureOverride(pointId, entry);
}

MapPoolGameObjectOverride const* MapPoolMgr::GetGameObjectOverride(uint32 pointId, uint32 entry)
{
    return _getGameObjectOverride(pointId, entry);
}

CreatureData const* MapPoolMgr::GetCreatureData(ObjectGuid guid)
{
    return _getCreatureData(guid);
}

GameObjectData const* MapPoolMgr::GetGameObjectData(ObjectGuid guid)
{
    return _getGameObjectData(guid);
}

void MapPoolMgr::HandleDespawn(WorldObject* obj, bool unloadingGrid)
{
    if (Creature* creature = obj->ToCreature())
    {
        if (MapPoolEntry const* pool = creature->GetPoolEntry())
        {
            if (MapPoolSpawnPoint const* spawnPoint = creature->GetPoolPoint())
            {
                if (!unloadingGrid && creature->IsAlive())
                    HandleDeath(creature, unloadingGrid);

                // Get real spawnpoint
                MapPoolSpawnPoint* point = _getSpawnPoint(pool, spawnPoint->pointId);
                ObjectGuid const guid = obj->GetGUID();
                std::vector<WorldObject*>::iterator itr = std::find_if(point->oldObjects.begin(), point->oldObjects.end(), [guid](std::vector<WorldObject*>::value_type const& item)
                {
                    return (item->GetGUID() == guid);
                });

                if (itr != point->oldObjects.end())
                    point->oldObjects.erase(itr);

                // In case of instant despawn
                if (point->currentObject && point->currentObject->GetGUID() == guid)
                {
                    point->currentObject = nullptr;
                    if (!unloadingGrid)
                        point->currentItem = nullptr;
                }

                auto dmItr = _poolCreatureDataMap.find(obj->GetGUID());
                if (dmItr != _poolCreatureDataMap.end())
                {
                    delete dmItr->second;
                    _poolCreatureDataMap.erase(dmItr);
                }
            }
        }

        // Clear creature pool info
        creature->SetPoolData(nullptr, nullptr, nullptr);
    }
    else if (GameObject* gameObject = obj->ToGameObject())
    {
        if (MapPoolEntry const* pool = gameObject->GetPoolEntry())
        {
            if (MapPoolSpawnPoint const* spawnPoint = gameObject->GetPoolPoint())
            {
                // Get real spawnpoint
                MapPoolSpawnPoint* point = _getSpawnPoint(pool, spawnPoint->pointId);
                ObjectGuid const guid = obj->GetGUID();

                if (point->currentObject && point->currentObject->GetGUID() == guid)
                {
                    point->currentObject = nullptr;
                    if (!unloadingGrid)
                        point->currentItem = nullptr;
                }

                MapPoolEntry* realPool = _getPool(pool->poolData.poolId);
                realPool->AdjustSpawned(-1);

                auto dmItr = _poolGameObjectDataMap.find(obj->GetGUID());
                if (dmItr != _poolGameObjectDataMap.end())
                {
                    delete dmItr->second;
                    _poolGameObjectDataMap.erase(dmItr);
                }

                if (!unloadingGrid)
                {
                    // Unlink spawn point
                    point->currentItem = nullptr;

                    // Handle expedited spawns
                    MapPoolEntry* rootPool = realPool->parentPool ? realPool->rootPool : realPool;
                    uint32 spawnsNeeded = rootPool->GetSpawnable(true);
                    if (spawnsNeeded > 0)
                    {
                        if (RespawnInfo* info = ownerMap->GetFirstPoolRespawn(rootPool->poolData.poolId))
                        {
                            if (SpawnPool(rootPool, 1))
                                ownerMap->DeleteRespawnInfo(info);
                        }
                    }
                }
            }
        }

        // Clear creature pool info
        gameObject->SetPoolData(nullptr, nullptr, nullptr);
    }
}

void MapPoolMgr::HandleDeath(Creature* obj, bool unloadingGrid)
{
    if (MapPoolEntry const* pool = obj->GetPoolEntry())
    {
        if (MapPoolSpawnPoint const* spawnPoint = obj->GetPoolPoint())
        {
            // Get real spawnpoint
            MapPoolSpawnPoint* point = _getSpawnPoint(pool, spawnPoint->pointId);

            // Check if current object is this one
            if (point->currentObject && point->currentObject->GetGUID() == obj->GetGUID())
            {
                // Add to old objects and unlink this spawnpoint
                std::vector<WorldObject*>::const_iterator itr = std::find_if(point->oldObjects.begin(), point->oldObjects.end(), [obj](std::vector<WorldObject*>::value_type const& item)
                {
                    return obj->GetGUID() == item->GetGUID();
                });

                if (itr == point->oldObjects.end())
                    point->oldObjects.push_back(obj);

                point->currentObject = nullptr;
                if (!unloadingGrid)
                    point->currentItem = nullptr;

                MapPoolEntry* realPool = _getPool(pool->poolData.poolId);
                realPool->AdjustSpawned(-1);

                if (unloadingGrid)
                    return;

                // Handle expedited spawns
                MapPoolEntry* rootPool = realPool->parentPool ? realPool->rootPool : realPool;
                uint32 spawnsNeeded = rootPool->GetSpawnable(true);
                if (spawnsNeeded > 0)
                {
                    if (RespawnInfo* info = ownerMap->GetFirstPoolRespawn(rootPool->poolData.poolId))
                    {
                        if (SpawnPool(rootPool, 1))
                            ownerMap->DeleteRespawnInfo(info);
                    }
                }
            }
        }

    }
}

time_t MapPoolMgr::GenerateRespawnTime(WorldObject* obj)
{
    MapPoolCreature const* cEntry = nullptr;
    MapPoolGameObject const* goEntry = nullptr;
    MapPoolEntry const * pool;
    MapPoolSpawnPoint const * spawnPoint;

    Creature* creature = obj->ToCreature();
    GameObject* go = obj->ToGameObject();

    if (creature)
    {
        cEntry = creature->GetPoolCreature();
        pool = creature->GetPoolEntry();
        spawnPoint = creature->GetPoolPoint();
    }
    else if (go)
    {
        goEntry = go->GetPoolGameObject();
        pool = go->GetPoolEntry();
        spawnPoint = go->GetPoolPoint();
    }
    else
    {
        return time_t(0);
    }


    // If we don't have all these things, it's not a valid pool creature
    if (!pool || !spawnPoint || (!cEntry && !goEntry))
        return time_t(0);

    // Check for specific override for this combination
    if (MapPoolCreatureOverride* override = _getCreatureOverride(spawnPoint->pointId, obj->GetEntry()))
    {
        if (override->spawntimeSecsMin && override->spawntimeSecsMax)
            return static_cast<time_t>(urand(override->spawntimeSecsMin, override->spawntimeSecsMax));
    }

    // Check for creature entry override next
    if (creature)
    {
        if (cEntry->overrideData && cEntry->overrideData->spawntimeSecsMin && cEntry->overrideData->spawntimeSecsMax)
            return static_cast<time_t>(urand(cEntry->overrideData->spawntimeSecsMin, cEntry->overrideData->spawntimeSecsMax));

        // Next check for spawn point override
        if (spawnPoint->creatureOverride && spawnPoint->creatureOverride->spawntimeSecsMin && spawnPoint->creatureOverride->spawntimeSecsMax)
            return static_cast<time_t>(urand(spawnPoint->creatureOverride->spawntimeSecsMin, spawnPoint->creatureOverride->spawntimeSecsMax));
    }
    else
    {
        if (goEntry->overrideData && goEntry->overrideData->spawntimeSecsMin && goEntry->overrideData->spawntimeSecsMax)
            return static_cast<time_t>(urand(goEntry->overrideData->spawntimeSecsMin, goEntry->overrideData->spawntimeSecsMax));

        // Next check for spawn point override
        if (spawnPoint->gameObjectOverride && spawnPoint->gameObjectOverride->spawntimeSecsMin && spawnPoint->gameObjectOverride->spawntimeSecsMax)
            return static_cast<time_t>(urand(spawnPoint->gameObjectOverride->spawntimeSecsMin, spawnPoint->gameObjectOverride->spawntimeSecsMax));
    }

    // Finally check pool
    if (pool->poolData.spawntimeSecsMin && pool->poolData.spawntimeSecsMax)
        return static_cast<time_t>(urand(pool->poolData.spawntimeSecsMin, pool->poolData.spawntimeSecsMax));

    // If we make it here, we don't have anything
    return time_t(0);
}

uint32 MapPoolMgr::SpawnPool(uint32 poolId, uint32 items)
{
    if (MapPoolEntry* pool = _getPool(poolId))
        return SpawnPool(pool, items);

    return 0;
}

uint32 MapPoolMgr::SpawnPool(MapPoolEntry* pool, uint32 items)
{
    if (!pool->isActive())
        return 0;

    uint32 spawned = 0;
    // Always spawn from top level pool
    MapPoolEntry* topPool = pool->parentPool ? pool->rootPool : pool;

    bool minReached = false;

    if (items == 0)
    {
        items = topPool->GetSpawnable();

        // Check for pending respawns
        std::vector<RespawnInfo*> ri;
        TC_LOG_DEBUG("maps.pool", "[Map %u] Spawning pool %u with %u items", ownerMap->GetId(), topPool->poolData.poolId, items);
        if (ownerMap->GetPoolRespawnInfo(topPool->poolData.poolId, ri))
        {
            items -= ri.size();
            TC_LOG_DEBUG("maps.pool", "[Map %u] Adjusting pool %u with %lu pending respawns new count %u", ownerMap->GetId(), topPool->poolData.poolId, ri.size(), items);
        }
    }

    for (uint32 item = 0; item < items; ++item)
    {
        if (!minReached && topPool->SpawnSingle(true))
            ++spawned;
        else
            minReached = true;

        if (minReached && topPool->SpawnSingle())
            ++spawned;
    }
    TC_LOG_DEBUG("maps.pool", "[Map %u] Spawned pool %u with %u items out of %u", ownerMap->GetId(), topPool->poolData.poolId, spawned, items);
    return spawned;
}

uint32 MapPoolMgr::RespawnPool(uint32 poolId)
{
    if (MapPoolEntry* pool = _getPool(poolId))
    {
        // Assure root pool
        if (pool->rootPool)
            pool = pool->rootPool;

        ClearRespawnTimes(pool);
        return SpawnPool(pool->poolData.poolId);
    }
    return 0;
}

void MapPoolMgr::ClearRespawnTimes(uint32 poolId)
{
    if (MapPoolEntry* pool = _getPool(poolId))
        ClearRespawnTimes(pool);
}

void MapPoolMgr::ClearRespawnTimes(MapPoolEntry* pool)
{
    MapPoolEntry* topPool = pool->parentPool ? pool->rootPool : pool;
    std::vector<RespawnInfo*> ri;
    if (ownerMap->GetPoolRespawnInfo(topPool->poolData.poolId, ri))
        ownerMap->RemoveRespawnTime(ri);
}

uint32 MapPoolMgr::DespawnPool(uint32 poolId, bool includeCorpse /* = false */)
{
    uint32 items = 0;
    if (MapPoolEntry* pool = _getPool(poolId))
    {
        // Assure root pool
        if (pool->rootPool)
            pool = pool->rootPool;

        for (MapPoolSpawnPoint* spawn : pool->GetSpawnsRecursive())
        {
            if (WorldObject* obj = spawn->currentObject)
            {
                obj->AddObjectToRemoveList();
                items++;
            }
            if (spawn->currentObject == nullptr)
                spawn->currentItem = nullptr;

            if (includeCorpse)
            {
                // If including corpses, remove them here
                // Need to store in new list, since cleanup from removing object will adjust oldObjects vector
                std::vector<WorldObject*> workObjList = std::vector<WorldObject*>(spawn->oldObjects);
                for (WorldObject* oldObj : workObjList)
                    oldObj->AddObjectToRemoveList();

                spawn->oldObjects.clear();
            }
        }
    }
    return items;
}

uint32 MapPoolMgr::ReseedPool(uint32 poolId)
{
    DespawnPool(poolId);
    return RespawnPool(poolId);
}

CreatureData* MapPoolMgr::_getCreatureData(ObjectGuid guid)
{
    auto itr = _poolCreatureDataMap.find(guid);
    if (itr == _poolCreatureDataMap.end())
        return nullptr;

    return itr->second;
}

GameObjectData* MapPoolMgr::_getGameObjectData(ObjectGuid guid)
{
    auto itr = _poolGameObjectDataMap.find(guid);
    if (itr == _poolGameObjectDataMap.end())
        return nullptr;

    return itr->second;
}

bool MapPoolMgr::SpawnCreature(uint32 poolId, uint32 entry, uint32 pointId)
{
    MapPoolEntry* pool = _getPool(poolId);
    if (!pool)
        return false;

    MapPoolSpawnPoint* spawnPoint = _getSpawnPoint(pool, pointId);
    if (!spawnPoint)
        return false;

    ASSERT(!spawnPoint->currentItem && !spawnPoint->currentObject, "[Map %u]: Attempted to spawn on already used spawnpoint %u", spawnPoint->mapId, spawnPoint->pointId);

    MapPoolCreature* cEntry = _getSpawnCreature(pool, entry);

    GridCoord grid = Trinity::ComputeGridCoord(spawnPoint->positionX, spawnPoint->positionY);
    if (ownerMap->IsGridLoaded(grid.GetId()))
    {
        CreatureData* newData = new CreatureData();
        if (!GenerateData(pool, cEntry, spawnPoint, newData))
        {
            delete newData;
            return false;
        }

        Creature* newCreature = new Creature();
        if (!newCreature->LoadFromDB(0, ownerMap, true, true, newData))
        {
            delete newCreature;
            delete newData;
            return false;
        }
        _poolCreatureDataMap[newCreature->GetGUID()] = newData;
        spawnPoint->currentObject = newCreature;
        newCreature->SetPoolData(pool, spawnPoint, cEntry);
    }

    // Hold Creature data while pooled object is spawned

    // Register current creature to spawnpoint
    spawnPoint->currentItem = cEntry;

    pool->AdjustSpawned(1);
    return true;
}

bool MapPoolMgr::SpawnGameObject(uint32 poolId, uint32 entry, uint32 pointId)
{
    MapPoolEntry* pool = _getPool(poolId);
    if (!pool)
        return false;

    MapPoolSpawnPoint* spawnPoint = _getSpawnPoint(pool, pointId);
    if (!spawnPoint)
        return false;

    ASSERT(!spawnPoint->currentItem && !spawnPoint->currentObject, "[Map %u]: Attempted to spawn on already used spawnpoint %u", spawnPoint->mapId, spawnPoint->pointId);

    MapPoolGameObject* goEntry = _getSpawnGameObject(pool, entry);

    GridCoord grid = Trinity::ComputeGridCoord(spawnPoint->positionX, spawnPoint->positionY);
    if (ownerMap->IsGridLoaded(grid.GetId()))
    {
        GameObjectData* newData = new GameObjectData();
        if (!GenerateData(pool, goEntry, spawnPoint, newData))
        {
            delete newData;
            return false;
        }

        GameObject* newGameObject = new GameObject();
        if (!newGameObject->LoadFromDB(0, ownerMap, true, true, newData))
        {
            delete newGameObject;
            delete newData;
            return false;
        }
        _poolGameObjectDataMap[newGameObject->GetGUID()] = newData;
        spawnPoint->currentObject = newGameObject;
        newGameObject->SetPoolData(pool, spawnPoint, goEntry);
    }

    // Hold Creature data while pooled object is spawned

    // Register current creature to spawnpoint
    spawnPoint->currentItem = goEntry;

    pool->AdjustSpawned(1);
    return true;
}

bool MapPoolMgr::GenerateData(MapPoolEntry* pool, MapPoolCreature* cEntry, MapPoolSpawnPoint* point, CreatureData* data)
{
    if (CreatureTemplate const* cInfo = sObjectMgr->GetCreatureTemplate(cEntry->entry))
    {
        MapPoolCreatureOverride const* odata = GetCreatureOverride(point->pointId, cEntry->entry);
        // This is so messy, but should handle priority specific override, creature override, spawnpoint override, pool data
        data->curhealth = odata && odata->curHealth ? odata->curHealth :
            cEntry->overrideData && cEntry->overrideData->curHealth ? cEntry->overrideData->curHealth :
            point->creatureOverride && point->creatureOverride->curHealth ? point->creatureOverride->curHealth :
            cEntry->curHealth;

        data->curmana = odata && odata->curMana ? odata->curMana :
            cEntry->overrideData && cEntry->overrideData->curMana ? cEntry->overrideData->curMana :
            point->creatureOverride && point->creatureOverride->curMana ? point->creatureOverride->curMana :
            cEntry->curMana;

        data->currentwaypoint = odata && odata->currentWaypoint ? odata->currentWaypoint :
            cEntry->overrideData && cEntry->overrideData->currentWaypoint ? cEntry->overrideData->currentWaypoint :
            point->creatureOverride && point->creatureOverride->currentWaypoint ? point->creatureOverride->currentWaypoint :
            cEntry->currentWaypoint;

        data->displayid = odata && odata->modelId ? odata->modelId :
            cEntry->overrideData && cEntry->overrideData->modelId ? cEntry->overrideData->modelId :
            point->creatureOverride && point->creatureOverride->modelId ? point->creatureOverride->modelId :
            cEntry->modelId;

        data->displayid = sObjectMgr->ChooseDisplayId(cInfo, data);

        data->dynamicflags = odata && odata->dynamicFlags ? odata->dynamicFlags :
            cEntry->overrideData && cEntry->overrideData->dynamicFlags ? cEntry->overrideData->dynamicFlags :
            point->creatureOverride && point->creatureOverride->dynamicFlags ? point->creatureOverride->dynamicFlags :
            cEntry->dynamicFlags;

        data->equipmentId = odata && odata->equipmentId ? odata->equipmentId :
            cEntry->overrideData && cEntry->overrideData->equipmentId ? cEntry->overrideData->equipmentId :
            point->creatureOverride && point->creatureOverride->equipmentId ? point->creatureOverride->equipmentId :
            cEntry->equipmentId;

        data->npcflag = odata && odata->npcFlag ? odata->npcFlag :
            cEntry->overrideData && cEntry->overrideData->npcFlag ? cEntry->overrideData->npcFlag :
            point->creatureOverride && point->creatureOverride->npcFlag ? point->creatureOverride->npcFlag :
            cEntry->npcFlag;

        data->spawndist = odata && odata->spawnDist ? odata->spawnDist :
            cEntry->overrideData && cEntry->overrideData->spawnDist ? cEntry->overrideData->spawnDist :
            point->creatureOverride && point->creatureOverride->spawnDist ? point->creatureOverride->spawnDist :
            pool->poolData.spawnDist;

        data->phaseMask = odata && odata->phaseMask ? odata->phaseMask :
            cEntry->overrideData && cEntry->overrideData->phaseMask ? cEntry->overrideData->phaseMask :
            point->creatureOverride && point->creatureOverride->phaseMask ? point->creatureOverride->phaseMask :
            pool->poolData.phaseMask;

        data->spawnMask = odata && odata->spawnMask ? odata->spawnMask :
            cEntry->overrideData && cEntry->overrideData->spawnMask ? cEntry->overrideData->spawnMask :
            point->creatureOverride && point->creatureOverride->spawnMask ? point->creatureOverride->spawnMask :
            pool->poolData.spawnMask;

        std::string scriptName = odata && odata->scriptName.empty() ? odata->scriptName :
            cEntry->overrideData && cEntry->overrideData->scriptName.empty() ? cEntry->overrideData->scriptName :
            point->creatureOverride && point->creatureOverride->scriptName.empty() ? point->creatureOverride->scriptName : "";
        data->scriptId = sObjectMgr->GetScriptId(scriptName);

        data->spawnPoint = WorldLocation(ownerMapId, point->positionX, point->positionY, point->positionZ, point->positionO);
        data->spawnId = 0;
        data->id = cEntry->entry;
        data->dbData = true;
        data->spawnGroupData = sObjectMgr->GetDefaultSpawnGroup();  // @ToDo: Make a proper default group
        return true;
    }

    return false;
}

bool MapPoolMgr::GenerateData(MapPoolEntry* pool, MapPoolGameObject* goEntry, MapPoolSpawnPoint* point, GameObjectData* data)
{
    if (GameObjectTemplate const* goInfo = sObjectMgr->GetGameObjectTemplate(goEntry->entry))
    {
        MapPoolGameObjectOverride const* odata = GetGameObjectOverride(point->pointId, goEntry->entry);
        // This is so messy, but should handle priority specific override, creature override, spawnpoint override, pool data
        data->animprogress = odata && odata->animProgress ? odata->animProgress :
            goEntry->overrideData && goEntry->overrideData->animProgress ? goEntry->overrideData->animProgress :
            point->gameObjectOverride && point->gameObjectOverride->animProgress ? point->gameObjectOverride->animProgress :
            goEntry->animProgress;

        data->goState = GOState(odata && odata->state ? odata->state :
            goEntry->overrideData && goEntry->overrideData->state ? goEntry->overrideData->state :
            point->gameObjectOverride && point->gameObjectOverride->state ? point->gameObjectOverride->state :
            goEntry->state);

        data->phaseMask = odata && odata->phaseMask ? odata->phaseMask :
            goEntry->overrideData && goEntry->overrideData->phaseMask ? goEntry->overrideData->phaseMask :
            point->gameObjectOverride && point->gameObjectOverride->phaseMask ? point->gameObjectOverride->phaseMask :
            pool->poolData.phaseMask;

        data->spawnMask = odata && odata->spawnMask ? odata->spawnMask :
            goEntry->overrideData && goEntry->overrideData->spawnMask ? goEntry->overrideData->spawnMask :
            point->gameObjectOverride && point->gameObjectOverride->spawnMask ? point->gameObjectOverride->spawnMask :
            pool->poolData.spawnMask;

        std::string scriptName = odata && odata->scriptName.empty() ? odata->scriptName :
            goEntry->overrideData && goEntry->overrideData->scriptName.empty() ? goEntry->overrideData->scriptName :
            point->gameObjectOverride && point->gameObjectOverride->scriptName.empty() ? point->gameObjectOverride->scriptName : "";

        GenerateData(goEntry, point, data);
        data->scriptId = sObjectMgr->GetScriptId(scriptName);
        data->artKit = 0;
        data->goState = GO_STATE_READY;
        data->rotation = QuaternionData(point->rotation0, point->rotation1, point->rotation2, point->rotation3);
        return true;
    }

    return false;
}

void MapPoolMgr::GenerateData(MapPoolItem* objEntry, MapPoolSpawnPoint* point, SpawnData* data)
{
        data->spawnPoint = WorldLocation(ownerMapId, point->positionX, point->positionY, point->positionZ, point->positionO);
        data->spawnId = 0;
        data->id = objEntry->entry;
        data->dbData = true;
        data->spawntimesecs = 300;      // This isn't actually used to respawn
        data->spawnGroupData = sObjectMgr->GetDefaultSpawnGroup();  // @ToDo: Make a proper default group
}

bool MapPoolMgr::SpawnPendingPoint(MapPoolSpawnPoint* spawnPoint)
{
    if (!spawnPoint || !spawnPoint->currentItem)
        return false;

    if (MapPoolEntry* pool = _getPool(spawnPoint->currentItem->poolId))
    {
        if (MapPoolCreature* cEntry = spawnPoint->currentItem->ToCreatureItem())
        {
            CreatureData* newData = new CreatureData();
            if (!GenerateData(pool, cEntry, spawnPoint, newData))
            {
                delete newData;
                return false;
            }

            Creature* newCreature = new Creature();
            if (!newCreature->LoadFromDB(0, ownerMap, true, true, newData))
            {
                delete newCreature;
                delete newData;
                return false;
            }
            _poolCreatureDataMap[newCreature->GetGUID()] = newData;
            spawnPoint->currentObject = newCreature;
            newCreature->SetPoolData(pool, spawnPoint, cEntry);
            return true;
        }
        else if (MapPoolGameObject* goEntry = spawnPoint->currentItem->ToGameObjectItem())
        {
            GameObjectData* newData = new GameObjectData();
            if (!GenerateData(pool, goEntry, spawnPoint, newData))
            {
                delete newData;
                return false;
            }

            GameObject* newGameObject = new GameObject();
            if (!newGameObject->LoadFromDB(0, ownerMap, true, true, newData))
            {
                delete newGameObject;
                delete newData;
                return false;
            }
            _poolGameObjectDataMap[newGameObject->GetGUID()] = newData;
            spawnPoint->currentObject = newGameObject;
            newGameObject->SetPoolData(pool, spawnPoint, goEntry);
            return true;
        }
    }

    return false;
}

uint32 MapPoolMgr::GetRespawnCounter(uint32 poolId)
{
    if (MapPoolEntry* pool = _getPool(poolId))
        return pool->GetRespawnCounter();

    return 0;
}

void MapPoolMgr::RegisterRespawn(uint32 poolId)
{
    // Register a respawn (subtract available spawns
    if (MapPoolEntry* pool = _getPool(poolId))
        pool->AdjustSpawned(1);
}

void MapPoolMgr::UpdatePoolDefaults(MapPoolEntry* pool)
{
    if (pool->parentPool)
    {
        MapPoolTemplate* poolData = &pool->poolData;
        MapPoolTemplate* parentData = &pool->parentPool->poolData;

        // Update data for this level
        if (poolData->spawntimeSecsMin == 0)
            poolData->spawntimeSecsMin = parentData->spawntimeSecsMin;
        if (poolData->spawntimeSecsMax == 0)
            poolData->spawntimeSecsMax = parentData->spawntimeSecsMax;
        if (poolData->corpsetimeSecsLoot == 0)
            poolData->corpsetimeSecsLoot = parentData->corpsetimeSecsLoot;
        if (poolData->corpsetimeSecsNoLoot == 0)
            poolData->corpsetimeSecsNoLoot = parentData->corpsetimeSecsNoLoot;
        if (poolData->phaseMask == 0)
            poolData->phaseMask = parentData->phaseMask;
        if (poolData->spawnMask == 0)
            poolData->spawnMask = parentData->spawnMask;
    }

    if (pool->GetChildPools()->size() == 0)
        return;

    // Handle next level down
    for (MapPoolEntry* nextPool : *pool->GetChildPools())
    {
        UpdatePoolDefaults(nextPool);
    }
}

void MapPoolMgr::SetActive(uint32 poolId, bool active)
{
    if (MapPoolEntry* pool = _getPool(poolId))
        SetActive(pool, active);
}

void MapPoolMgr::SetActive(MapPoolEntry* pool, bool active)
{
    pool->SetActive(active);
}
