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

// Forward declare all the things
struct MapPoolTemplate;
struct MapPoolCreatureOverride;
struct MapPoolCreature;
struct MapPoolGameObjectOverride;
struct MapPoolGameObject;
struct MapPoolSpawn;

enum PoolType
{
    POOLTYPE_CREATURE,
    POOLTYPE_GAMEOBJECT,
    POOLTYPE_MAX
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
};

struct MapPoolItem
{
    uint32 mapId;
    uint32 poolId;
    uint32 entry;
    float chance;
    PoolType type;

    MapPoolCreature* ToCreatureItem() { return type == POOLTYPE_CREATURE ? reinterpret_cast<MapPoolCreature*>(this) : nullptr; }
    MapPoolGameObject* ToGameObjectItem() { return type == POOLTYPE_GAMEOBJECT ? reinterpret_cast<MapPoolGameObject*>(this) : nullptr; }
};

struct MapPoolCreature : MapPoolItem
{
    uint32 modelId;
    int8 equipmentId;
    uint32 currentWaypoint;
    uint32 curHealth;
    uint32 curMana;
    uint32 npcFlag;
    uint32 unitFlags;
    uint32 dynamicFlags;
    MapPoolCreatureOverride* overrideData;
};

struct MapPoolGameObject : MapPoolItem
{
    uint8 animProgress;
    uint8 state;
    MapPoolGameObjectOverride* overrideData;
};

struct MapPoolOverride
{
    PoolType type;
    uint32 spawntimeSecsMin;
    uint32 spawntimeSecsMax;
    std::string aiName;
    std::string scriptName;
    MapPoolCreatureOverride* ToCreatureOverride() { return type == POOLTYPE_CREATURE ? reinterpret_cast<MapPoolCreatureOverride*>(this) : nullptr; }
    MapPoolGameObjectOverride* ToGameObjectOverride() { return type == POOLTYPE_GAMEOBJECT ? reinterpret_cast<MapPoolGameObjectOverride*>(this) : nullptr; }
};

struct MapPoolCreatureOverride : MapPoolOverride
{
    uint32 modelId;
    uint8 equipmentId;
    uint32 currentWaypoint;
    uint32 curHealth;
    uint32 curMana;
    uint32 npcFlag;
    uint32 unitFlags;
    uint32 dynamicFlags;
    uint8 movementType;
    float spawnDist;
    uint32 corpsetimeSecsLoot;
    uint32 corpsetimeSecsNoLoot;
};

struct MapPoolGameObjectOverride : MapPoolOverride
{
    uint8 animProgress;
    uint8 state;
};

struct MapPoolSpawn
{
    uint32 mapId;
    uint32 poolId;
    uint32 pointId;
    MapPoolSpawnPoint* pointData;
};

class MapPoolEntry
{
public:
    MapPoolTemplate poolData;
    PoolType type;
    MapPoolEntry* parentPool;
    MapPoolEntry* topPool;
    std::vector<MapPoolEntry*> childPools;
    std::vector<MapPoolSpawnPoint*> spawnList;
    std::vector<MapPoolItem*> itemList;

    MapPoolEntry()
    {
        type = POOLTYPE_CREATURE;
        topPool = nullptr;
        parentPool = nullptr;
    }

    // Checks if poolId is anywhere in the hierarchy already
    bool CheckHierarchy(uint32 poolId, bool callingSelf = false) const;

    MapPoolEntry* GetTopPool() const { return parentPool == nullptr ? const_cast<MapPoolEntry*>(this) : parentPool->GetTopPool(); }
};

class TC_GAME_API MapPoolMgr
{
    typedef std::pair<uint32, uint32> PointEntryPair;
private:
    Map* ownerMap;
    uint32 ownerMapId;
    std::map<uint32, MapPoolEntry> _poolMap;
    std::map<PointEntryPair, MapPoolCreatureOverride*> _poolCreatureOverrideMap;
    std::map<PointEntryPair, MapPoolGameObjectOverride*> _poolGameObjectOverrideMap;
    MapPoolEntry* _getPool(uint32 poolId);
    MapPoolCreatureOverride* _getCreatureOverride(uint32 pointId, uint32 entry);
    MapPoolGameObjectOverride* _getGameObjectOverride(uint32 pointId, uint32 entry);

public:
    MapPoolMgr(Map* map);
    ~MapPoolMgr();
    void LoadMapPools();
    MapPoolEntry const* GetPool(uint32 poolId);
    MapPoolCreatureOverride const* GetCreatureOverride(uint32 pointId, uint32 entry);
    MapPoolGameObjectOverride const* GetGameObjectOverride(uint32 pointId, uint32 entry);
};

#endif
