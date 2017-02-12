/*
 * Copyright (C) 2008-2019 TrinityCore <https://www.trinitycore.org/>
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

#include "ScriptMgr.h"
#include "Chat.h"
#include "GameObject.h"
#include "Language.h"
#include "Log.h"
#include "MapManager.h"
#include "MapPoolMgr.h"
#include "Player.h"
#include "RBAC.h"

// temporary hack until includes are sorted out (don't want to pull in Windows.h)
#ifdef GetClassName
#undef GetClassName
#endif

class pool_commandscript : public CommandScript
{
public:
    pool_commandscript() : CommandScript("pool_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> poolCommandTable =
        {
            { "dump",          rbac::RBAC_PERM_COMMAND_POOL_DUMP,                    true,  &HandleDebugPoolDumpCommand,    "" },
            { "respawn",       rbac::RBAC_PERM_COMMAND_POOL_RESPAWN,                 true,  &HandleDebugPoolRespawnCommand, "" },
            { "despawn",       rbac::RBAC_PERM_COMMAND_POOL_DESPAWN,                 true,  &HandleDebugPoolDespawnCommand, "" },
            { "reseed",        rbac::RBAC_PERM_COMMAND_POOL_RESEED,                  true,  &HandleDebugPoolReseedCommand,  "" },
            { "list",          rbac::RBAC_PERM_COMMAND_POOL_LIST,                    true,  &HandleDebugPoolListCommand,    "" },
        };

        static std::vector<ChatCommand> commandTable =
        {
            { "pool", rbac::RBAC_PERM_COMMAND_POOL, true, nullptr, "", poolCommandTable },
        };

        return commandTable;
    }

    static bool HandleDebugPoolRespawnCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        char* mapIdStr = args ? strtok((char*)args, " ") : nullptr;
        char* poolIdStr = args ? strtok(nullptr, " ") : nullptr;
        int32 mapId = mapIdStr ? atoi(mapIdStr) : -1;
        if (mapId == -1)
        {
            handler->PSendSysMessage(LANG_POOL_INVALID_MAP_ID, mapIdStr);
            return true;
        }
        int32 poolId = poolIdStr ? atoi(poolIdStr) : -1;
        if (poolId == -1)
        {
            handler->PSendSysMessage(LANG_POOL_INVALID_POOL_ID, poolIdStr);
        }

        if (Map* map = sMapMgr->FindBaseNonInstanceMap(mapId))
        {
            if (MapPoolMgr* poolMgr = map->GetMapPoolMgr())
            {
                uint32 spawns = poolMgr->RespawnPool(poolId);
                handler->PSendSysMessage(LANG_POOL_RESPAWNED_ENTRIES, poolIdStr, spawns);
                return true;
            }
        }
        return false;
    }

    static bool HandleDebugPoolDespawnCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        char* mapIdStr = args ? strtok((char*)args, " ") : nullptr;
        char* poolIdStr = args ? strtok(nullptr, " ") : nullptr;
        int32 mapId = mapIdStr ? atoi(mapIdStr) : -1;
        if (mapId == -1)
        {
            handler->PSendSysMessage(LANG_POOL_INVALID_MAP_ID, mapIdStr);
            return true;
        }
        int32 poolId = poolIdStr ? atoi(poolIdStr) : -1;
        if (poolId == -1)
        {
            handler->PSendSysMessage(LANG_POOL_INVALID_POOL_ID, poolIdStr);
        }

        if (Map* map = sMapMgr->FindBaseNonInstanceMap(mapId))
        {
            if (MapPoolMgr* poolMgr = map->GetMapPoolMgr())
            {
                uint32 spawns = poolMgr->DespawnPool(poolId);
                handler->PSendSysMessage(LANG_POOL_DESPAWNED_ENTRIES, poolIdStr, spawns);
                return true;
            }
        }
        return false;
    }

    static bool HandleDebugPoolReseedCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        char* mapIdStr = args ? strtok((char*)args, " ") : nullptr;
        char* poolIdStr = args ? strtok(nullptr, " ") : nullptr;
        int32 mapId = mapIdStr ? atoi(mapIdStr) : -1;
        if (mapId == -1)
        {
            handler->PSendSysMessage(LANG_POOL_INVALID_MAP_ID, mapIdStr);
            return true;
        }
        int32 poolId = poolIdStr ? atoi(poolIdStr) : -1;
        if (poolId == -1)
        {
            handler->PSendSysMessage(LANG_POOL_INVALID_POOL_ID, poolIdStr);
        }

        if (Map* map = sMapMgr->FindBaseNonInstanceMap(mapId))
        {
            if (MapPoolMgr* poolMgr = map->GetMapPoolMgr())
            {
                uint32 spawns = poolMgr->ReseedPool(poolId);
                handler->PSendSysMessage(LANG_POOL_RESEEDED_ENTRIES, poolIdStr, spawns);
                return true;
            }
        }
        return false;
    }

    static bool HandleDebugPoolListCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        char* mapIdStr = args ? strtok((char*)args, " ") : nullptr;
        int32 mapId = mapIdStr ? atoi(mapIdStr) : -1;
        if (mapId == -1)
        {
            handler->PSendSysMessage(LANG_POOL_INVALID_MAP_ID, mapIdStr);
            return true;
        }

        if (Map* map = sMapMgr->FindBaseNonInstanceMap(mapId))
        {
            if (MapPoolMgr* poolMgr = map->GetMapPoolMgr())
            {
                std::vector<MapPoolEntry const*> rootPools = poolMgr->GetRootPools();
                handler->PSendSysMessage(LANG_POOL_LIST_ROOT_POOLS_MAP, mapIdStr);
                for (MapPoolEntry const* pool : rootPools)
                {
                    MapPoolTemplate const* data = pool->GetPoolData();
                    handler->PSendSysMessage(LANG_POOL_MINMAX_SPAWNED, data->poolId, data->description.c_str(), data->minLimit, data->maxLimit, pool->GetSpawnCount());
                }
                return true;
            }
        }
        return false;
    }

    static bool HandleDebugPoolDumpCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        char* mapIdStr = args ? strtok((char*)args, " ") : nullptr;
        char* poolIdStr = args ? strtok(nullptr, " ") : nullptr;
        int32 mapId = mapIdStr ? atoi(mapIdStr) : -1;
        if (mapId == -1)
        {
            handler->PSendSysMessage(LANG_POOL_INVALID_MAP_ID, mapIdStr);
            return true;
        }
        int32 poolId = poolIdStr ? atoi(poolIdStr) : -1;
        if (poolId == -1)
        {
            handler->PSendSysMessage(LANG_POOL_INVALID_POOL_ID, poolIdStr);
        }

        if (Map* map = sMapMgr->FindBaseNonInstanceMap(mapId))
        {
            if (MapPoolMgr* poolMgr = map->GetMapPoolMgr())
            {
                if (MapPoolEntry const* pool = poolMgr->GetPool(poolId))
                {
                    MapPoolEntry const* rootPool = pool->GetParentPool() ? pool->GetRootPool() : pool;
                    if (rootPool != pool)
                        handler->PSendSysMessage(LANG_POOL_NOT_ROOT_USING_ROOT_POOL, pool->GetPoolData()->poolId, rootPool->GetPoolData()->poolId);

                    handler->PSendSysMessage(LANG_POOL_DUMPING_DATA, rootPool->GetPoolData()->poolId);
                    DumpPoolRecursive(handler, rootPool);
                    return true;
                }
                else
                {
                    handler->PSendSysMessage(LANG_POOL_NOTFOUND_IN_MAP, poolId, mapId);
                    return true;
                }
            }
            else
            {
                return true;
            }

        }
        else
        {
            handler->PSendSysMessage(LANG_POOL_BASEMAP_NOTFOUND, mapId);
            return true;
        }
    }

    static void DumpPoolRecursive(ChatHandler* handler, MapPoolEntry const* pool)
    {
        int32 level = 1;
        MapPoolEntry const* upperPool = pool->GetParentPool();
        while (upperPool)
        {
            level++;
            upperPool = upperPool->GetParentPool();
        }
        std::string indent = "  ";
        for (int count = 1; count < level; count++)
        {
            indent += "  ";
        }

        handler->PSendSysMessage(LANG_POOL_DUMP_HEADER, indent.c_str(), pool->GetPoolData()->poolId,
                    pool->GetPoolData()->description.c_str(), pool->GetPoolData()->minLimit, pool->GetPoolData()->maxLimit, pool->GetChance(), pool->GetSpawnCount(),
                    pool->GetSpawnable(true), pool->GetSpawnable());

        if (pool->GetChildPools()->size() > 0)
        {
            // Not a leaf node, recurse
            for (MapPoolEntry* childPool : *pool->GetChildPools())
            {
                DumpPoolRecursive(handler, childPool);
            }
        }
        else
        {
            // Leaf node, dump used spawn points
            for (auto spawn : *pool->GetSpawns())
            {
                if (WorldObject* obj = spawn->currentObject)
                {
                    if (Creature* creature = obj->ToCreature())
                    {
                        if (creature->GetPoolEntry() == pool)
                            handler->PSendSysMessage(LANG_POOL_POINT_CREATURE, indent.c_str(), spawn->pointId, spawn->positionX, spawn->positionY,
                                    spawn->positionZ, creature->GetGUID().ToString().c_str());
                    }
                    else if (GameObject* go = obj->ToGameObject())
                    {
                        if (go->GetPoolEntry() == pool)
                            handler->PSendSysMessage(LANG_POOL_POINT_GO, indent.c_str(), spawn->pointId, spawn->positionX, spawn->positionY,
                                    spawn->positionZ, go->GetGUID().ToString().c_str());
                    }
                }
                else if (MapPoolItem* item = spawn->currentItem)
                {
                    if (item->poolId == pool->GetPoolData()->poolId)
                    {
                        if (MapPoolCreature* creature = item->ToCreatureItem())
                        {
                            handler->PSendSysMessage(LANG_POOL_POINT_CREATURE_NOGRID, indent.c_str(), spawn->pointId, spawn->positionX,
                                    spawn->positionY, spawn->positionZ, creature->entry);
                        }
                        else if (MapPoolGameObject* go = item->ToGameObjectItem())
                        {
                            handler->PSendSysMessage(LANG_POOL_POINT_GO_NOGRID, indent.c_str(), spawn->pointId, spawn->positionX,
                                    spawn->positionY, spawn->positionZ, go->entry);
                        }
                    }
                }
            }
        }
    }
};

void AddSC_pool_commandscript()
{
    new pool_commandscript();
}
