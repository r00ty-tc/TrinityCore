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

#include "MapPoolEntry.h"
#include "Define.h"
#include "Map.h"

// Forward declare all the things
struct MapPoolCreatureOverride;
struct MapPoolCreature;
struct MapPoolGameObjectOverride;
struct MapPoolGameObject;
struct MapPoolSpawn;
struct GameObjectData;
struct CreatureData;

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

    MapPoolCreature()
    {
        type = POOLTYPE_CREATURE;
    }
};

struct MapPoolGameObject : MapPoolItem
{
    uint8 animProgress;
    uint8 state;
    MapPoolGameObjectOverride* overrideData;

    MapPoolGameObject()
    {
        type = POOLTYPE_GAMEOBJECT;
    }
};

struct MapPoolOverride
{
    PoolType type;
    uint32 spawntimeSecsMin;
    uint32 spawntimeSecsMax;
    uint32 phaseMask;
    uint8 spawnMask;
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
    float wander_distance;
    uint32 corpsetimeSecsLoot;
    uint32 corpsetimeSecsNoLoot;
    uint32 pathId;
    uint32 mount;
    uint32 bytes1;
    uint32 bytes2;
    uint32 emote;
    std::vector<uint32> auras;

    MapPoolCreatureOverride()
    {
        type = POOLTYPE_CREATURE;
    }
};

struct MapPoolGameObjectOverride : MapPoolOverride
{
    uint8 animProgress;
    uint8 state;
    float parentRotation0;
    float parentRotation1;
    float parentRotation2;
    float parentRotation3;
    uint8 invisibilityType;
    uint32 invisibilityValue;

    MapPoolGameObjectOverride()
    {
        type = POOLTYPE_GAMEOBJECT;
    }
};

struct MapPoolSpawn
{
    uint32 mapId;
    uint32 poolId;
    uint32 pointId;
    MapPoolSpawnPoint* pointData;
};

class TC_GAME_API MapPoolMgr
{
    friend MapPoolEntry;
    typedef std::pair<uint32, uint32> PointEntryPair;
private:
    Map* ownerMap;
    uint32 ownerMapId;
    std::unordered_map<uint32, MapPoolEntry> _poolMap;
    std::unordered_map<PointEntryPair, MapPoolCreatureOverride*> _poolCreatureOverrideMap;
    std::unordered_map<PointEntryPair, MapPoolGameObjectOverride*> _poolGameObjectOverrideMap;
    std::unordered_map<ObjectGuid, CreatureData*> _poolCreatureDataMap;
    std::unordered_map<ObjectGuid, GameObjectData*> _poolGameObjectDataMap;
    void UpdatePoolDefaults(MapPoolEntry* pool);

protected:
    MapPoolEntry* _getPool(uint32 poolId);
    static MapPoolSpawnPoint* _getSpawnPoint(MapPoolEntry const* pool, uint32 pointId);
    static MapPoolCreature* _getSpawnCreature(MapPoolEntry const* pool, uint32 entry);
    static MapPoolGameObject* _getSpawnGameObject(MapPoolEntry* pool, uint32 entry);
    MapPoolCreatureOverride* _getCreatureOverride(uint32 pointId, uint32 entry);
    MapPoolGameObjectOverride* _getGameObjectOverride(uint32 pointId, uint32 entry);
    CreatureData* _getCreatureData(ObjectGuid guid);
    GameObjectData* _getGameObjectData(ObjectGuid guid);
    bool GenerateData(MapPoolEntry* pool, MapPoolCreature* cEntry, MapPoolSpawnPoint* point, CreatureData* data);
    bool GenerateData(MapPoolEntry* pool, MapPoolGameObject* goEntry, MapPoolSpawnPoint* point, GameObjectData* data);
    void GenerateData(MapPoolItem* goEntry, MapPoolSpawnPoint* point, SpawnData* data) const;
    bool SpawnCreature(uint32 poolId, uint32 entry, uint32 pointId);
    bool SpawnGameObject(uint32 poolId, uint32 entry, uint32 pointId);
    uint32 SpawnPool(MapPoolEntry* pool, uint32 items = 0) const;
    void SetActive(MapPoolEntry* pool, bool active);
    void ClearRespawnTimes(MapPoolEntry* pool) const;

public:
    MapPoolMgr(Map* map);
    ~MapPoolMgr();
    void LoadMapPools();
    void SpawnMap();
    MapPoolEntry const* GetPool(uint32 poolId);
    MapPoolEntry const* GetRootPool(uint32 poolId);
    std::vector<MapPoolEntry const*> GetRootPools();
    uint32 GetRootPoolId(uint32 poolId);
    MapPoolCreatureOverride const* GetCreatureOverride(uint32 pointId, uint32 entry);
    MapPoolGameObjectOverride const* GetGameObjectOverride(uint32 pointId, uint32 entry);
    CreatureData const* GetCreatureData(ObjectGuid guid);
    GameObjectData const* GetGameObjectData(ObjectGuid guid);
    bool SpawnCreatureManual(uint32 poolId, uint32 entry, uint32 pointId) { return (SpawnCreature(poolId, entry ,pointId)); }
    void HandleDespawn(WorldObject* obj, bool unloadingGrid = false);
    void HandleDeath(Creature* obj, bool unloadingGrid = false);
    static MapPoolCreature const* GetSpawnCreature(MapPoolEntry const* pool, uint32 entry) { return _getSpawnCreature(pool, entry); }
    static MapPoolGameObject const* GetSpawnGameObject(MapPoolEntry* pool, uint32 entry) { return _getSpawnGameObject(pool, entry); }
    time_t GenerateRespawnTime(WorldObject* obj);
    uint32 SpawnPool(uint32 poolId, uint32 items = 0);
    uint32 DespawnPool(uint32 poolId, bool includeCorpse = false);
    uint32 RespawnPool(uint32 poolId);
    uint32 ReseedPool(uint32 poolId);
    bool SpawnPendingPoint(MapPoolSpawnPoint* pointId);
    uint32 GetRespawnCounter(uint32 poolId);
    void RegisterRespawn(uint32 poolId);
    void SetActive(uint32 poolId, bool active);
    void ClearRespawnTimes(uint32 poolId);
};

#endif
