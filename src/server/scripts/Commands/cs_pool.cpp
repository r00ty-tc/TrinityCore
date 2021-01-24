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
#include "RBAC.h"

using namespace Trinity::ChatCommands;

class pool_commandscript : public CommandScript
{
public:
    pool_commandscript() : CommandScript("pool_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable poolCommandTable =
        {
            { "dump",       HandleDebugPoolDumpCommand,    rbac::RBAC_PERM_COMMAND_POOL_DUMP,    Console::Yes },
            { "respawn",    HandleDebugPoolRespawnCommand, rbac::RBAC_PERM_COMMAND_POOL_RESPAWN, Console::Yes },
            { "despawn",    HandleDebugPoolDespawnCommand, rbac::RBAC_PERM_COMMAND_POOL_DESPAWN, Console::Yes },
            { "reseed",     HandleDebugPoolReseedCommand,  rbac::RBAC_PERM_COMMAND_POOL_RESEED,  Console::Yes },
            { "list",       HandleDebugPoolListCommand,    rbac::RBAC_PERM_COMMAND_POOL_LIST,    Console::Yes },
        };

        static ChatCommandTable commandTable =
        {
            { "pool", poolCommandTable },
        };
        return commandTable;
    }

    static bool HandleDebugPoolRespawnCommand(ChatHandler* handler, Optional<uint32> mapId, Optional<uint32> poolId)
    {
        if (!mapId.has_value())
        {
            handler->PSendSysMessage(LANG_POOL_INVALID_MAP_ID, 0);
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!poolId.has_value())
        {
            handler->PSendSysMessage(LANG_POOL_INVALID_POOL_ID, 0);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (Map* map = sMapMgr->FindBaseNonInstanceMap(mapId.value()))
        {
            if (MapPoolMgr* poolMgr = map->GetMapPoolMgr())
            {
                if (poolMgr->GetPool(poolId.value()) != nullptr)
                {
                    uint32 spawns = poolMgr->RespawnPool(poolId.value());
                    handler->PSendSysMessage(LANG_POOL_RESPAWNED_ENTRIES, poolId.value(), spawns);
                    return true;
                }

                handler->PSendSysMessage(LANG_POOL_NOTFOUND_IN_MAP, poolId.value(), mapId.value());
                handler->SetSentErrorMessage(true);
                return false;
            }
        }
        else
        {
            handler->PSendSysMessage(LANG_POOL_BASEMAP_NOTFOUND, mapId.value());
            handler->SetSentErrorMessage(true);
            return false;
        }
        return false;
    }

    static bool HandleDebugPoolDespawnCommand(ChatHandler* handler, Optional<uint32> mapId, Optional<uint32> poolId)
    {
        if (!mapId.has_value())
        {
            handler->PSendSysMessage(LANG_POOL_INVALID_MAP_ID, 0);
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!poolId.has_value())
        {
            handler->PSendSysMessage(LANG_POOL_INVALID_POOL_ID, 0);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (Map* map = sMapMgr->FindBaseNonInstanceMap(mapId.value()))
        {
            if (MapPoolMgr* poolMgr = map->GetMapPoolMgr())
            {
                if (poolMgr->GetPool(poolId.value()) != nullptr)
                {
                    uint32 spawns = poolMgr->DespawnPool(poolId.value());
                    handler->PSendSysMessage(LANG_POOL_DESPAWNED_ENTRIES, poolId.value(), spawns);
                    return true;
                }

                handler->PSendSysMessage(LANG_POOL_NOTFOUND_IN_MAP, poolId.value(), mapId.value());
                handler->SetSentErrorMessage(true);
                return false;
            }
        }
        else
        {
            handler->PSendSysMessage(LANG_POOL_BASEMAP_NOTFOUND, mapId.value());
            handler->SetSentErrorMessage(true);
            return false;
        }

        return false;
    }

    static bool HandleDebugPoolReseedCommand(ChatHandler* handler, Optional<uint32> mapId, Optional<uint32> poolId)
    {
        if (!mapId.has_value())
        {
            handler->PSendSysMessage(LANG_POOL_INVALID_MAP_ID, 0);
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!poolId.has_value())
        {
            handler->PSendSysMessage(LANG_POOL_INVALID_POOL_ID, 0);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (Map* map = sMapMgr->FindBaseNonInstanceMap(mapId.value()))
        {
            if (MapPoolMgr* poolMgr = map->GetMapPoolMgr())
            {
                if (poolMgr->GetPool(poolId.value()) != nullptr)
                {
                    uint32 spawns = poolMgr->ReseedPool(poolId.value());
                    handler->PSendSysMessage(LANG_POOL_RESEEDED_ENTRIES, poolId.value(), spawns);
                    return true;
                }

                handler->PSendSysMessage(LANG_POOL_NOTFOUND_IN_MAP, poolId.value(), mapId.value());
                handler->SetSentErrorMessage(true);
                return false;
            }
        }
        else
        {
            handler->PSendSysMessage(LANG_POOL_BASEMAP_NOTFOUND, mapId.value());
            handler->SetSentErrorMessage(true);
            return false;
        }
        return false;
    }

    static bool HandleDebugPoolListCommand(ChatHandler* handler, Optional<uint32> mapId)
    {
        if (!mapId.has_value())
        {
            handler->PSendSysMessage(LANG_POOL_INVALID_MAP_ID, 0);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (Map* map = sMapMgr->FindBaseNonInstanceMap(mapId.value()))
        {
            if (MapPoolMgr* poolMgr = map->GetMapPoolMgr())
            {
                std::vector<MapPoolEntry const*> rootPools = poolMgr->GetRootPools();
                handler->PSendSysMessage(LANG_POOL_LIST_ROOT_POOLS_MAP, mapId.value());
                for (MapPoolEntry const* pool : rootPools)
                {
                    MapPoolTemplate const* data = pool->GetPoolData();
                    handler->PSendSysMessage(LANG_POOL_MINMAX_SPAWNED, data->poolId, data->description.c_str(), data->minLimit, data->maxLimit, pool->GetSpawnCount());
                }
                return true;
            }
        }
        else
        {
            handler->PSendSysMessage(LANG_POOL_BASEMAP_NOTFOUND, mapId.value());
            handler->SetSentErrorMessage(true);
            return false;
        }
        return false;
    }

    static bool HandleDebugPoolDumpCommand(ChatHandler* handler, Optional<uint32> mapId, Optional<uint32> poolId)
    {
        if (!mapId.has_value())
        {
            handler->PSendSysMessage(LANG_POOL_INVALID_MAP_ID, 0);
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!poolId.has_value())
        {
            handler->PSendSysMessage(LANG_POOL_INVALID_POOL_ID, 0);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (Map* map = sMapMgr->FindBaseNonInstanceMap(mapId.value()))
        {
            if (MapPoolMgr* poolMgr = map->GetMapPoolMgr())
            {
                if (MapPoolEntry const* pool = poolMgr->GetPool(poolId.value()))
                {
                    MapPoolEntry const* rootPool = pool->GetParentPool() ? pool->GetRootPool() : pool;
                    if (rootPool != pool)
                        handler->PSendSysMessage(LANG_POOL_NOT_ROOT_USING_ROOT_POOL, pool->GetPoolData()->poolId, rootPool->GetPoolData()->poolId);

                    handler->PSendSysMessage(LANG_POOL_DUMPING_DATA, rootPool->GetPoolData()->poolId);
                    DumpPoolRecursive(handler, rootPool);
                    return true;
                }

                handler->PSendSysMessage(LANG_POOL_NOTFOUND_IN_MAP, poolId.value(), mapId.value());
                return true;
            }

            return true;
        }
        handler->PSendSysMessage(LANG_POOL_BASEMAP_NOTFOUND, mapId.value());
        return true;
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

        if (!pool->GetChildPools()->empty())
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
            for (auto* spawn : *pool->GetSpawns())
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
