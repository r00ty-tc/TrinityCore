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


#ifndef TRINITY_MAPPOOLENTRY_H
#define TRINITY_MAPPOOLENTRY_H

#include "Define.h"
#include "Map.h"

// Forward declare all the things
struct MapPoolTemplate;

enum PoolType
{
    POOLTYPE_CREATURE,
    POOLTYPE_GAMEOBJECT,
    POOLTYPE_MAX
};

enum PoolFlags
{
    POOL_FLAG_NONE           = 0x00,
    POOL_FLAG_MANUAL_SPAWN   = 0x01,
    POOL_FLAGS_ALL = (POOL_FLAG_MANUAL_SPAWN)
};

struct MapPoolTemplate
{
    uint32 mapId;
    uint32 poolId;
    uint32 phaseMask;
    uint8 spawnMask;
    uint32 minLimit;
    uint32 maxLimit;
    uint8 movementType;
    float spawnDist;
    uint32 spawntimeSecsMin;
    uint32 spawntimeSecsMax;
    uint32 spawntimeSecsFast;
    uint32 corpsetimeSecsLoot;
    uint32 corpsetimeSecsNoLoot;
    std::string description;
    PoolFlags flags;
};

class MapPoolEntry
{
    friend MapPoolMgr;
private:
    MapPoolMgr* ownerManager;
    void UpdateMaxSpawnable(uint32& minSpawns, uint32& maxSpawns, uint32& minNeeded, uint32& maxAllowed) const;
    bool PerformSpawn(std::vector<MapPoolSpawnPoint*>& spawns);
    void GetSpawnList(std::vector<MapPoolSpawnPoint*>& pointList, bool onlyFree = true);
    MapPoolTemplate poolData;
    void GetSpawnsRecursive(std::vector<MapPoolSpawnPoint*>& items);
    bool activePool;

protected:
    uint32 _GetSpawnable(bool minimum, int currentLimit) const;
    PoolType type;
    MapPoolEntry* parentPool;
    MapPoolEntry* rootPool;
    uint32 spawnsThisPool;
    uint32 respawnCounter;
    float chance;
    std::vector<MapPoolEntry*> childPools;
    std::vector<MapPoolSpawnPoint*> spawnList;
    std::vector<MapPoolItem*> itemList;

public:

    MapPoolEntry()
    {
        type = POOLTYPE_CREATURE;
        rootPool = nullptr;
        parentPool = nullptr;
        ownerManager = nullptr;
        spawnsThisPool = 0;
        chance = 0.0f;
        respawnCounter = 1;
        activePool = false;
    }

    // Checks if poolId is anywhere in the hierarchy already
    bool CheckHierarchy(uint32 poolId, bool callingSelf = false) const;

    MapPoolEntry const* GetRootPool() const { return parentPool == nullptr ? this : parentPool->GetRootPool(); }
    uint32 GetRootPoolId() const { return parentPool == nullptr ? poolData.poolId : parentPool->GetRootPool()->poolData.poolId; }
    uint32 GetSpawnable(bool minimum = false) const;
    void AdjustSpawned(int adjust, bool onlyAggregate = false);
    bool SpawnSingle(bool minimum = false);
    bool CanSpawn(bool minimum = false) const;
    uint32 GetSpawnCount() const;
    void SetOwnerPoolMgr(MapPoolMgr* poolMgr) { ownerManager = poolMgr; }
    uint32 GetRespawnCounter() { return respawnCounter++; }
    MapPoolTemplate const* GetPoolData() const { return &poolData; }
    MapPoolEntry const* GetParentPool() const { return parentPool; }
    std::vector<MapPoolEntry*> const* GetChildPools() const { return &childPools; }
    std::vector<MapPoolSpawnPoint*> const* GetSpawns() const { return &spawnList; }
    std::vector<MapPoolSpawnPoint*> GetSpawnsRecursive();
    std::vector<MapPoolItem*> const* GetItems() const { return &itemList; }
    PoolType GetPoolType() const { return type; }
    float GetChance() const;
    void SetActive(bool active);
    bool isActive() { return activePool; }
};
#endif
