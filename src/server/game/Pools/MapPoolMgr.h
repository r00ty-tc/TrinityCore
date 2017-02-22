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

enum PoolDynamicGroups
{
    POOL_CREATURE_GROUP = 5,
    POOL_GAMEOBJECT_GROUP = 5
};

// Forward declare all the things
struct MapPoolCreatureTemplate;
struct MapPoolGameObjectTemplate;
struct MapPoolCreatureSpawn;
struct MapPoolGameObjectSpawn;
struct MapPoolCreatureInfo;
struct MapPoolGameObjectInfo;
struct MapPoolCreatureData;
struct MapPoolGameObjectData;
struct MapPoolAssignedCreature;
struct MapPoolAssignedGameObject;
struct RespawnInfo;
typedef std::vector<MapPoolCreatureSpawn*> MapPoolCreatureSpawnList;
typedef std::vector<MapPoolGameObjectSpawn*> MapPoolGameObjectSpawnList;
typedef std::vector<MapPoolCreatureInfo*> MapPoolCreatureInfoList;
typedef std::vector<MapPoolGameObjectInfo*> MapPoolGameObjectInfoList;
typedef std::unordered_map<uint32, MapPoolCreatureData*> MapPoolCreatureMap;
typedef std::unordered_map<uint32, MapPoolGameObjectData*> MapPoolGameObjectMap;
typedef std::unordered_map<Creature const*, MapPoolAssignedCreature*> MapPoolActiveCreatureMap;
typedef std::unordered_map<GameObject const*, MapPoolAssignedGameObject*> MapPoolActiveGameObjectMap;
typedef std::pair<uint32, uint32> RespawnPoolSpawnPointPair;
typedef std::unordered_map<RespawnPoolSpawnPointPair, RespawnInfo*> respawnPoolInfoMap;

// Other forward declarations
struct CreatureData;

struct MapPoolCreatureTemplate
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
    uint32 corpsetimeSecsLoot;
    uint32 corpsetimeSecsNoLoot;
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
    uint32 spawntimeSecsMin;
    uint32 spawntimeSecsMax;
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
    uint32 gridId;
    uint8 movementTypeOverride;
    float spawnDistOverride;
    uint32 spawntimeSecsMinOverride;
    uint32 spawntimeSecsMaxOverride;
    uint32 corpsetimeSecsLootOverride;
    uint32 corpsetimeSecsNoLootOverride;
    uint32 AINameOverrideEntry;
    std::string AINameOverride;
    uint32 ScriptNameOverrideEntry;
    std::string ScriptNameOverride;
};

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
    uint32 gridId;
    uint32 spawntimeSecsMinOverride;
    uint32 spawntimeSecsMaxOverride;
    uint32 AINameOverrideEntry;
    std::string AINameOverride;
    uint32 ScriptNameOverrideEntry;
    std::string ScriptNameOverride;
};

struct MapPoolCreatureInfo
{
    uint32 mapId;
    uint32 poolId;
    uint32 creatureId;
    uint32 creatureQualifier;
    float chance;
    uint32 modelId;
    int8 equipmentId;
    uint32 currentWaypoint;
    uint32 curHealth;
    uint32 curMana;
    uint32 npcFlag;
    uint32 unitFlags;
    uint32 dynamicFlags;
    uint8 movementTypeOverride;
    float spawnDistOverride;
    uint32 spawntimeSecsMinOverride;
    uint32 spawntimeSecsMaxOverride;
    uint32 corpsetimeSecsLootOverride;
    uint32 corpsetimeSecsNoLootOverride;
    std::string AINameOverride;
    std::string ScriptNameOverride;
};

struct MapPoolGameObjectInfo
{
    uint32 mapId;
    uint32 poolId;
    uint32 gameobjectId;
    uint32 gameobjectQualifier;
    float chance;
    uint32 spawntimeSecsMinOverride;
    uint32 spawntimeSecsMaxOverride;
    uint8 animProgress;
    uint8 state;
    std::string AINameOverride;
    std::string ScriptNameOverride;
};

struct MapPoolCreatureData
{
    MapPoolCreatureTemplate* poolTemplate;
    MapPoolCreatureSpawnList* spawnList;
    MapPoolCreatureInfoList* infoList;
    MapPoolCreatureData* parentPool;
    std::vector<MapPoolCreatureData*> childPools;
};

struct MapPoolGameObjectData
{
    MapPoolGameObjectTemplate* poolTemplate;
    MapPoolGameObjectSpawnList* spawnList;
    MapPoolGameObjectInfoList* infoList;
    MapPoolGameObjectData* parentPool;
    std::vector<MapPoolGameObjectData*> childPools;
};

struct MapPoolAssignedCreature
{
    Creature* creature;
    uint32 poolId;
    MapPoolCreatureSpawn* spawnPoint;
    MapPoolCreatureInfo* info;
};

struct MapPoolAssignedGameObject
{
    GameObject* gameObject;
    uint32 poolId;
    MapPoolGameObjectSpawn* spawnPoint;
    MapPoolGameObjectInfo* info;
};

class TC_GAME_API MapPoolMgr
{
    friend class Map;

private:
    // Local respawn functions
    respawnInfoMultiMap _creatureRespawnTimesByGridId;
    respawnInfoMultiMap _creatureRespawnTimesByCellAreaZoneId;
    respawnPoolInfoMap  _creatureRespawnTimesByPoolSpawnPoint;
    respawnInfoMultiMap _gameObjectRespawnTimesByGridId;
    respawnInfoMultiMap _gameObjectRespawnTimesByCellAreaZoneId;
    respawnPoolInfoMap  _gameObjectRespawnTimesByPoolSpawnPoint;

    std::unordered_map<uint32, uint32> _cellAreaZoneLastRespawnedCreatureMap;
    std::unordered_map<uint32, uint32> _cellAreaZoneLastRespawnedGameObjectMap;

    // Pool active/inactive spawn lists
    std::vector<MapPoolCreatureSpawn*> _inactiveCreatureSpawnList;
    std::unordered_map<MapPoolCreatureSpawn*, Creature*> _activeCreatureSpawnList;
    std::vector<MapPoolGameObjectSpawn*> _inactiveGameObjectSpawnList;
    std::unordered_map<MapPoolGameObjectSpawn*, GameObject*> _activeGameObjectSpawnList;

    Map* ownerMap;
    uint32 ownerMapId;
    uint32 ownerInstanceId;
    MapPoolCreatureMap poolCreatureMap;
    MapPoolGameObjectMap poolGameObjectMap;
    MapPoolActiveCreatureMap poolActiveCreatureMap;
    MapPoolActiveGameObjectMap poolActiveGameObjectMap;
    MapPoolCreatureData* createPoolCreatureData();
    MapPoolGameObjectData* createPoolGameObjectData();
    MapPoolCreatureData* getCreaturePool(uint32 poolId);
    MapPoolGameObjectData* getGameObjectPool(uint32 poolId);
    MapPoolCreatureSpawn* getCreatureSpawnPoint(uint32 poolId, uint32 spawnPoint);
    MapPoolGameObjectSpawn* getGameObjectSpawnPoint(uint32 poolId, uint32 spawnPoint);
    MapPoolCreatureInfo* getCreatureSpawnInfo(uint32 poolId, uint32 spawnPoint);
    MapPoolGameObjectInfo* getGameObjectSpawnInfo(uint32 poolId, uint32 spawnPoint);
    bool checkHierarchy(MapPoolCreatureData const* childPool, MapPoolCreatureData const* thisPool);
    bool checkHierarchy(MapPoolGameObjectData const* childPool, MapPoolGameObjectData const* thisPool);

    // Respawn functions
    void RespawnCellAreaZoneCreature(uint32 cellZoneAreaId);
    void RespawnCellAreaZoneGameObject(uint32 cellZoneAreaId);

    void addRespawnInfo(respawnInfoMultiMap& gridList, respawnInfoMultiMap& cellAreaZoneList, respawnPoolInfoMap& spawnList, RespawnInfo& Info, bool replace = false);
    bool getRespawnInfo(respawnInfoMultiMap const& gridList, respawnInfoMultiMap const& cellAreaZoneList, respawnPoolInfoMap const& spawnList, RespawnVector& RespawnData, uint32 poolId, uint32 spawnPointId, uint32 gridId = 0, uint32 cellAreaZoneId = 0, bool onlyDue = true);
    void deleteRespawnInfo(respawnInfoMultiMap& gridList, respawnInfoMultiMap& cellAreaZoneList, respawnPoolInfoMap& spawnList, uint32 poolId, uint32 spawnPointId, uint32 gridId = 0, uint32 cellAreaZoneId = 0, bool onlyDue = true);

    void RespawnCellAreaZone(uint32 cellId, uint32 zoneId, uint32 areaId);
    void RespawnCreatureList(RespawnVector const& RespawnData, bool force = false);
    void RespawnGameObjectList(RespawnVector const& RespawnData, bool force = false);

    void AddCreatureRespawnInfo(RespawnInfo& Info, bool replace = false)
    {
        addRespawnInfo(_creatureRespawnTimesByGridId, _creatureRespawnTimesByCellAreaZoneId, _creatureRespawnTimesByPoolSpawnPoint, Info, replace);
    }
    bool GetCreatureRespawnInfo(RespawnVector& RespawnData, uint32 poolId, uint32 spawnPointId, uint32 gridId = 0, uint32 cellAreaZoneId = 0, bool onlyDue = true)
    {
        return getRespawnInfo(_creatureRespawnTimesByGridId, _creatureRespawnTimesByCellAreaZoneId, _creatureRespawnTimesByPoolSpawnPoint, RespawnData, poolId, spawnPointId, gridId, cellAreaZoneId, onlyDue);
    }
    void DeleteCreatureRespawnInfo(uint32 poolId, uint32 spawnPointId, uint32 gridId = 0, uint32 cellAreaZoneId = 0, bool onlyDue = true)
    {
        deleteRespawnInfo(_creatureRespawnTimesByGridId, _creatureRespawnTimesByCellAreaZoneId, _creatureRespawnTimesByPoolSpawnPoint, poolId, spawnPointId, gridId, cellAreaZoneId, onlyDue);
    }

    void AddGameObjectRespawnInfo(RespawnInfo& Info, bool replace = false)
    {
        addRespawnInfo(_gameObjectRespawnTimesByGridId, _gameObjectRespawnTimesByCellAreaZoneId, _gameObjectRespawnTimesByPoolSpawnPoint, Info, replace);
    }

    bool GetGameObjectRespawnInfo(RespawnVector& RespawnData, uint32 poolId, uint32 spawnPointId, uint32 gridId = 0, uint32 cellAreaZoneId = 0, bool onlyDue = true)
    {
        return getRespawnInfo(_gameObjectRespawnTimesByGridId, _gameObjectRespawnTimesByCellAreaZoneId, _gameObjectRespawnTimesByPoolSpawnPoint, RespawnData, poolId, spawnPointId, gridId, cellAreaZoneId, onlyDue);
    }

    void DeleteGameObjectRespawnInfo(uint32 poolId, uint32 spawnPointId, uint32 gridId = 0, uint32 cellAreaZoneId = 0, bool onlyDue = true)
    {
        deleteRespawnInfo(_gameObjectRespawnTimesByGridId, _gameObjectRespawnTimesByCellAreaZoneId, _gameObjectRespawnTimesByPoolSpawnPoint, poolId, spawnPointId, gridId, cellAreaZoneId, onlyDue);
    }

    void buildCreatureData(CreatureData& cdata, MapPoolCreatureTemplate const* pool, MapPoolCreatureInfo const* info, MapPoolCreatureSpawn const* spawn);
    void buildGameObjectData(GameObjectData& cdata, MapPoolGameObjectTemplate const* pool, MapPoolGameObjectInfo const* info, MapPoolGameObjectSpawn const* spawn);

public:
    MapPoolMgr(Map* map);
    ~MapPoolMgr();
    void LoadMapPools();
    MapPoolCreatureData const* GetCreaturePool(uint32 poolId);
    MapPoolGameObjectData const* GetGameObjectPool(uint32 poolId);
    void LoadPoolRespawns();
    void SaveCreaturePoolRespawnTime(uint32 poolId, uint32 entry, uint32 spawnPointId, time_t respawnTime, uint32 cellAreaZoneId = 0, uint32 gridId = 0, bool WriteDB = true, bool replace = false, SQLTransaction respawntrans = nullptr);
    void SaveCreaturePoolRespawnTimeDB(uint32 poolId, uint32 spawnPointId, time_t respawnTime, SQLTransaction respawntrans = nullptr);
    void SaveGameObjectPoolRespawnTime(uint32 poolId, uint32 entry, uint32 spawnPointId, time_t respawnTime, uint32 cellAreaZoneId = 0, uint32 gridId = 0, bool WriteDB = true, bool replace = false, SQLTransaction respawntrans = nullptr);
    void SaveGameObjectPoolRespawnTimeDB(uint32 poolId, uint32 spawnPointId, time_t respawnTime, SQLTransaction respawntrans = nullptr);
    MapPoolAssignedCreature const* GetPoolForObject(Creature const* creature);
    MapPoolAssignedGameObject const* GetPoolForObject(GameObject const* gameobject);
    bool isPoolObject(Creature const* creature) { return (GetPoolForObject(creature) != nullptr); }
    bool isPoolObject(GameObject const* gameobject) { return (GetPoolForObject(gameobject) != nullptr); }

    // Respawn functions
    bool GetRespawnData(RespawnVector& results, Map::RespawnObjectType type, bool onlyDue = false, uint32 poolId = 0, uint32 spawnPointId = 0, uint32 grid = 0, bool allMap = true, float x = 0.0f, float y = 0.0f, float z = 0.0f);
    void DeleteRespawnTimes();

    void RemoveCreatureRespawnTime(uint32 poolId = 0, uint32 spawnPointId = 0, uint32 cellAreaZoneId = 0, uint32 gridId = 0, bool respawnCreature = false, SQLTransaction respawntrans = nullptr);
    void RemoveGORespawnTime(uint32 poolId = 0, uint32 spawnPointId = 0, uint32 cellAreaZoneId = 0, uint32 gridId = 0, bool respawnObject = false, SQLTransaction respawntrans = nullptr);
    static void DeleteRespawnTimesInDB(uint16 mapId, uint32 instanceId);
};

#endif
