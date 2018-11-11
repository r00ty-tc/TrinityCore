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

// Checks if poolId is anywhere in the hierarchy already
bool MapPoolEntry::CheckHierarchy(uint32 poolId, bool callingSelf) const
{
    if (poolData.poolId == poolId)
        return true;

    if (parentPool != nullptr && !callingSelf)
    {
        // If this pool isn't the top pool, seek poolid, starting at the top
        return GetRootPool()->CheckHierarchy(poolId, true);
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

uint32 MapPoolEntry::_GetSpawnable(bool minimum, int currentLimit) const
{
    uint32 poolLimit = minimum ? poolData.minLimit : poolData.maxLimit;

    // Am I a leaf node? If so, return the lowest of the current limit, or this leaf node's requirement
    if (childPools.size() == 0)
    {
        return std::max(std::min(currentLimit, static_cast<int32>(poolLimit - spawnsThisPool)), 0);
    }
    else
    {
        // This is not a mistake, we only want to limit the leaf node to minimum
        currentLimit = std::max(std::min(currentLimit, static_cast<int32>(poolData.maxLimit - spawnsThisPool)), 0);

        // Shortcut out, if we're already having nothing to spawn at this point
        if (currentLimit <= 0)
            return 0;

        int32 maxChildLimit = 0;
        for (auto thisPool : childPools)
        {
            maxChildLimit += thisPool->_GetSpawnable(minimum, currentLimit);
        }

        return std::max(std::min(currentLimit, maxChildLimit), 0);
    }
}

uint32 MapPoolEntry::GetSpawnable(bool minimum) const
{
    return _GetSpawnable(minimum, minimum ? poolData.minLimit : poolData.maxLimit);
}

void MapPoolEntry::AdjustSpawned(int adjust, bool onlyAggregate)
{
    if (!onlyAggregate && int32(spawnsThisPool + adjust) >= 0)
        spawnsThisPool += adjust;

    if (parentPool)
        parentPool->AdjustSpawned(adjust, false);
}

void MapPoolEntry::UpdateMaxSpawnable(uint32& minSpawns, uint32& maxSpawns, uint32& minNeeded, uint32& maxAllowed) const
{
    int32 spawnsAggregate = rootPool ? rootPool->spawnsThisPool : 0;
    if (minSpawns == 0 || this->poolData.minLimit > minSpawns)
        minSpawns = this->poolData.minLimit;

    if (maxSpawns == 0 || this->poolData.maxLimit < minSpawns)
        maxSpawns = this->poolData.maxLimit;

    int32 minNeededThisLevel = minSpawns - spawnsAggregate;
    if (minNeededThisLevel > 0 && minNeededThisLevel > static_cast<int32>(minNeeded))
        minNeeded = minNeededThisLevel;

    int32 maxAllowedThisLevel = maxSpawns - spawnsAggregate;
    if (maxAllowedThisLevel > 0 && (maxAllowedThisLevel < static_cast<int32>(maxAllowed) || maxAllowed == 0))
        maxAllowed = maxAllowedThisLevel;

    if (parentPool)
        parentPool->UpdateMaxSpawnable(minSpawns, maxSpawns, minNeeded, maxAllowed);
}

bool MapPoolEntry::SpawnSingle(bool minimum)
{
    if (!CanSpawn(minimum))
        return false;

    // Handle leaf node
    if (!childPools.size())
    {
        std::vector<MapPoolSpawnPoint*> spawns;
        GetSpawnList(spawns);
        if (spawns.size() > 0 && PerformSpawn(spawns))
            return true;

        return false;
    }

    std::vector<MapPoolEntry*> includedPools;
    float chanceTotal = 0.0f;
    for (MapPoolEntry* childPool : childPools)
    {
        if (childPool->_GetSpawnable(false, childPool->poolData.maxLimit) > 0)
        {
            includedPools.push_back(childPool);
            chanceTotal += std::max(childPool->chance, 1.0f);
        }
    }

    float choice = frand(1.0f, chanceTotal);
    chanceTotal = 0.0f;

    // Handle child pools
    for (MapPoolEntry* childPool : includedPools)
    {
        chanceTotal += std::max(childPool->chance, 1.0f);
        if (chanceTotal >= choice)
        {
            // If we made a spawn, exit
            if (childPool->SpawnSingle(minimum))
                return true;
        }
    }

    // Nothing more to spawn
    return false;
}

bool MapPoolEntry::PerformSpawn(std::vector<MapPoolSpawnPoint*>& spawns)
{
    // Build spawnpoint shortlist
    if (spawns.size() == 0)
        GetSpawnList(spawns);

    // Get out if there's nothing to spawn, or nowhere to spawn it
    if (spawns.size() == 0 || itemList.size() == 0)
        return false;

    uint32 spawnChoice = urand(0, spawns.size() - 1);
    MapPoolSpawnPoint* point = spawns[spawnChoice];

    float chanceTotal = 0.0f;
    for (MapPoolItem* item : itemList)
        chanceTotal += std::max(item->chance, 1.0f);

    float choice = frand(1.0f, chanceTotal);

    chanceTotal = 0.0f;

    for (MapPoolItem* item : itemList)
    {
        chanceTotal += std::max(item->chance, 1.0f);
        if (chanceTotal >= choice)
        {
            if (item->ToCreatureItem())
                return ownerManager->SpawnCreature(poolData.poolId, item->entry, point->pointId);
            else
                return ownerManager->SpawnGameObject(poolData.poolId, item->entry, point->pointId);
        }
    }
    return false;
}

void MapPoolEntry::GetSpawnList(std::vector<MapPoolSpawnPoint*>& pointList, bool onlyFree)
{
    for (MapPoolSpawnPoint* point : spawnList)
        if (!onlyFree || !point->currentItem)
            pointList.push_back(point);

    if (parentPool)
        parentPool->GetSpawnList(pointList, onlyFree);
}

bool MapPoolEntry::CanSpawn(bool minimum) const
{
    return (GetSpawnable(minimum));
}

uint32 MapPoolEntry::GetSpawnCount() const
{
    return spawnsThisPool;
}