/*
 * Copyright (C) 2008-2013 TrinityCore <http://www.trinitycore.org/>
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

#include "BattlegroundBE.h"
#include "Language.h"
#include "Object.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "WorldPacket.h"

BattlegroundBE::BattlegroundBE()
{
    BgObjects.resize(BG_BE_OBJECT_MAX);

    StartDelayTimes[BG_STARTING_EVENT_FIRST]  = BG_START_DELAY_1M;
    StartDelayTimes[BG_STARTING_EVENT_SECOND] = BG_START_DELAY_30S;
    StartDelayTimes[BG_STARTING_EVENT_THIRD]  = BG_START_DELAY_15S;
    StartDelayTimes[BG_STARTING_EVENT_FOURTH] = BG_START_DELAY_NONE;
    //we must set messageIds
    StartMessageIds[BG_STARTING_EVENT_FIRST]  = LANG_ARENA_ONE_MINUTE;
    StartMessageIds[BG_STARTING_EVENT_SECOND] = LANG_ARENA_THIRTY_SECONDS;
    StartMessageIds[BG_STARTING_EVENT_THIRD]  = LANG_ARENA_FIFTEEN_SECONDS;
    StartMessageIds[BG_STARTING_EVENT_FOURTH] = LANG_ARENA_HAS_BEGUN;
}

BattlegroundBE::~BattlegroundBE()
{

}

void BattlegroundBE::StartingEventCloseDoors()
{
    for (uint32 i = BG_BE_OBJECT_DOOR_1; i <= BG_BE_OBJECT_DOOR_4; ++i)
        SpawnBGObject(i, RESPAWN_IMMEDIATELY);

    for (uint32 i = BG_BE_OBJECT_BUFF_1; i <= BG_BE_OBJECT_BUFF_2; ++i)
        SpawnBGObject(i, RESPAWN_ONE_DAY);
}

void BattlegroundBE::StartingEventOpenDoors()
{
    for (uint32 i = BG_BE_OBJECT_DOOR_1; i <= BG_BE_OBJECT_DOOR_2; ++i)
        DoorOpen(i);

    for (uint32 i = BG_BE_OBJECT_BUFF_1; i <= BG_BE_OBJECT_BUFF_2; ++i)
        SpawnBGObject(i, SHADOW_SIGHT_SPAWN_TIME);

    for (uint32 i = BG_BE_OBJECT_READYMARKER_1; i <= BG_BE_OBJECT_READYMARKER_2; ++i)
        DelObject(i);
}

void BattlegroundBE::AddPlayer(Player* player)
{
    Battleground::AddPlayer(player);
    BattlegroundScore* sc = new BattlegroundScore;
    sc->BgTeam = player->GetTeam();
    sc->TalentTree = player->GetPrimaryTalentTree(player->GetActiveSpec());
    PlayerScores[player->GetGUID()] = sc;
    UpdateArenaWorldState();
}

void BattlegroundBE::RemovePlayer(Player* /*player*/, uint64 /*guid*/, uint32 /*team*/)
{
    if (GetStatus() == STATUS_WAIT_LEAVE)
        return;

    UpdateArenaWorldState();
    CheckArenaWinConditions();
}

void BattlegroundBE::HandleKillPlayer(Player* player, Player* killer)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    if (killer)
        if (player)
            Battleground::HandleKillPlayer(player, killer);
        else TC_LOG_ERROR("bg.battleground", "BattlegroundBE::HandleKillPlayer: Killer player not found");
    else TC_LOG_ERROR("bg.battleground", "BattlegroundBE::HandleKillPlayer: Victim player not found");

    UpdateArenaWorldState();
    CheckArenaWinConditions();
}

bool BattlegroundBE::HandlePlayerUnderMap(Player* player)
{
    player->TeleportTo(GetMapId(), 6238.930176f, 262.963470f, 0.889519f, player->GetOrientation());
    return true;
}

void BattlegroundBE::HandleAreaTrigger(Player* player, uint32 trigger)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    switch (trigger)
    {
        case 4538:                                          // buff trigger?
        case 4539:                                          // buff trigger?
            break;
        default:
            Battleground::HandleAreaTrigger(player, trigger);
            break;
    }
}

void BattlegroundBE::FillInitialWorldStates(WorldPacket &data)
{
    data << uint32(0x9f3) << uint32(1);           // 9
    UpdateArenaWorldState();
}

void BattlegroundBE::Reset()
{
    //call parent's class reset
    Battleground::Reset();
}

bool BattlegroundBE::SetupBattleground()
{
    // gates
    if (!AddObject(BG_BE_OBJECT_DOOR_1, BG_BE_OBJECT_TYPE_DOOR_1, 6287.277f, 282.1877f, 3.810925f, -2.260201f, 0, 0, 0.9044551f, -0.4265689f, RESPAWN_IMMEDIATELY)
        || !AddObject(BG_BE_OBJECT_DOOR_2, BG_BE_OBJECT_TYPE_DOOR_2, 6189.546f, 241.7099f, 3.101481f, 0.8813917f, 0, 0, 0.4265689f, 0.9044551f, RESPAWN_IMMEDIATELY)
        || !AddObject(BG_BE_OBJECT_DOOR_3, BG_BE_OBJECT_TYPE_DOOR_3, 6299.116f, 296.5494f, 3.308032f, 0.8813917f, 0, 0, 0.4265689f, 0.9044551f, RESPAWN_IMMEDIATELY)
        || !AddObject(BG_BE_OBJECT_DOOR_4, BG_BE_OBJECT_TYPE_DOOR_4, 6177.708f, 227.3481f, 3.604374f, -2.260201f, 0, 0, 0.9044551f, -0.4265689f, RESPAWN_IMMEDIATELY)
        // buffs
        || !AddObject(BG_BE_OBJECT_BUFF_1, BG_BE_OBJECT_TYPE_BUFF_1, 6249.042f, 275.3239f, 11.22033f, -1.448624f, 0, 0, 0.6626201f, -0.7489557f, 120)
        || !AddObject(BG_BE_OBJECT_BUFF_2, BG_BE_OBJECT_TYPE_BUFF_2, 6228.26f, 249.566f, 11.21812f, -0.06981307f, 0, 0, 0.03489945f, -0.9993908f, 120))
    {
        TC_LOG_ERROR("sql.sql", "BatteGroundBE: Failed to spawn some object!");
        return false;
    }

    // readymarkers
    if (sWorld->getBoolConfig(CONFIG_ARENA_READYMARK_ENABLED))
    {
        AddObject(BG_BE_OBJECT_READYMARKER_1, BG_BE_OBJECT_READYMARKER, 6288.124512f, 290.558502f, 5.484696f, 5.6f, 0, 0, 0, 0, RESPAWN_IMMEDIATELY);
        AddObject(BG_BE_OBJECT_READYMARKER_2, BG_BE_OBJECT_READYMARKER, 6188.941406f, 233.555664f, 5.501983f, 2.5f, 0, 0, 0, 0, RESPAWN_IMMEDIATELY);
    }

    return true;
}

void BattlegroundBE::UpdatePlayerScore(Player* Source, uint32 type, uint32 value, bool doAddHonor)
{

    BattlegroundScoreMap::iterator itr = PlayerScores.find(Source->GetGUID());
    if (itr == PlayerScores.end())                         // player not found...
        return;

    //there is nothing special in this score
    Battleground::UpdatePlayerScore(Source, type, value, doAddHonor);

}

/*
21:45:46 id:231310 [S2C] SMSG_INIT_WORLD_STATES (706 = 0x02C2) len: 86
0000: 32 02 00 00 76 0e 00 00 00 00 00 00 09 00 f3 09  |  2...v...........
0010: 00 00 01 00 00 00 f1 09 00 00 01 00 00 00 f0 09  |  ................
0020: 00 00 02 00 00 00 d4 08 00 00 00 00 00 00 d8 08  |  ................
0030: 00 00 00 00 00 00 d7 08 00 00 00 00 00 00 d6 08  |  ................
0040: 00 00 00 00 00 00 d5 08 00 00 00 00 00 00 d3 08  |  ................
0050: 00 00 00 00 00 00                                |  ......

spell 32724 - Gold Team
spell 32725 - Green Team
35774 Gold Team
35775 Green Team
*/
