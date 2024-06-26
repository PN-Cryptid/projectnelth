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

#include "BattlegroundNA.h"
#include "Language.h"
#include "Object.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "WorldPacket.h"

BattlegroundNA::BattlegroundNA()
{
    doordelete = 0;

    BgObjects.resize(BG_NA_OBJECT_MAX);

    BgCreatures.resize(1);

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

BattlegroundNA::~BattlegroundNA()
{

}

void BattlegroundNA::StartingEventCloseDoors()
{
    for (uint32 i = BG_NA_OBJECT_DOOR_1; i <= BG_NA_OBJECT_DOOR_4; ++i)
        SpawnBGObject(i, RESPAWN_IMMEDIATELY);
}

void BattlegroundNA::StartingEventOpenDoors()
{
    for (uint32 i = BG_NA_OBJECT_DOOR_1; i <= BG_NA_OBJECT_DOOR_2; ++i)
        DoorOpen(i);

    doordelete = 2 * IN_MILLISECONDS;

    for (uint32 i = BG_NA_OBJECT_BUFF_1; i <= BG_NA_OBJECT_BUFF_2; ++i)
        SpawnBGObject(i, SHADOW_SIGHT_SPAWN_TIME);

    for (uint32 i = BG_NA_OBJECT_READYMARKER_1; i <= BG_NA_OBJECT_READYMARKER_2; ++i)
        DelObject(i);
}

void BattlegroundNA::AddPlayer(Player* player)
{
    Battleground::AddPlayer(player);
    BattlegroundScore* sc = new BattlegroundScore;
    sc->BgTeam = player->GetTeam();
    sc->TalentTree = player->GetPrimaryTalentTree(player->GetActiveSpec());
    PlayerScores[player->GetGUID()] = sc;
    UpdateArenaWorldState();
}

void BattlegroundNA::RemovePlayer(Player* /*player*/, uint64 /*guid*/, uint32 /*team*/)
{
    if (GetStatus() == STATUS_WAIT_LEAVE)
        return;

    UpdateArenaWorldState();
    CheckArenaWinConditions();
}

bool BattlegroundNA::PreUpdateImpl(uint32 diff)
{
    if (doordelete)
    {
        if (doordelete <= diff)
        {
            for (uint32 i = BG_NA_OBJECT_DOOR_1; i <= BG_NA_OBJECT_DOOR_2; ++i)
                DelObject(i);
            doordelete = 0;
        }
        else
            doordelete -= diff;
    }
    return true;
}

void BattlegroundNA::HandleKillPlayer(Player* player, Player* killer)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    if (killer)
        if (player)
            Battleground::HandleKillPlayer(player, killer);
        else TC_LOG_ERROR("bg.battleground", "BattlegroundNA::HandleKillPlayer: Killer player not found");
    else TC_LOG_ERROR("bg.battleground", "BattlegroundNA::HandleKillPlayer: Victim player not found");


    UpdateArenaWorldState();
    CheckArenaWinConditions();
}

bool BattlegroundNA::HandlePlayerUnderMap(Player* player)
{
    player->TeleportTo(GetMapId(), 4055.504395f, 2919.660645f, 13.611241f, player->GetOrientation());
    return true;
}

void BattlegroundNA::HandleAreaTrigger(Player* player, uint32 trigger)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    switch (trigger)
    {
        case 4536:                                          // buff trigger?
        case 4537:                                          // buff trigger?
            break;
        default:
            Battleground::HandleAreaTrigger(player, trigger);
            break;
    }
}

void BattlegroundNA::FillInitialWorldStates(WorldPacket &data)
{
    data << uint32(0xa11) << uint32(1);           // 9
    UpdateArenaWorldState();
}

void BattlegroundNA::Reset()
{
    //call parent's class reset
    Battleground::Reset();
}

bool BattlegroundNA::SetupBattleground()
{
    // gates
    if (!AddObject(BG_NA_OBJECT_DOOR_1, BG_NA_OBJECT_TYPE_DOOR_1, 4031.854f, 2966.833f, 12.6462f, -2.648788f, 0, 0, 0.9697962f, -0.2439165f, RESPAWN_IMMEDIATELY)
        || !AddObject(BG_NA_OBJECT_DOOR_2, BG_NA_OBJECT_TYPE_DOOR_2, 4081.179f, 2874.97f, 12.39171f, 0.4928045f, 0, 0, 0.2439165f, 0.9697962f, RESPAWN_IMMEDIATELY)
        || !AddObject(BG_NA_OBJECT_DOOR_3, BG_NA_OBJECT_TYPE_DOOR_3, 4023.709f, 2981.777f, 10.70117f, -2.648788f, 0, 0, 0.9697962f, -0.2439165f, RESPAWN_IMMEDIATELY)
        || !AddObject(BG_NA_OBJECT_DOOR_4, BG_NA_OBJECT_TYPE_DOOR_4, 4090.064f, 2858.438f, 10.23631f, 0.4928045f, 0, 0, 0.2439165f, 0.9697962f, RESPAWN_IMMEDIATELY)
    // buffs
        || !AddObject(BG_NA_OBJECT_BUFF_1, BG_NA_OBJECT_TYPE_BUFF_1, 4009.189941f, 2895.250000f, 13.052700f, -1.448624f, 0, 0, 0.6626201f, -0.7489557f, 120)
        || !AddObject(BG_NA_OBJECT_BUFF_2, BG_NA_OBJECT_TYPE_BUFF_2, 4103.330078f, 2946.350098f, 13.051300f, -0.06981307f, 0, 0, 0.03489945f, -0.9993908f, 120))
    {
        TC_LOG_ERROR("sql.sql", "BatteGroundNA: Failed to spawn some object!");
        return false;
    }

    if (HasActiveHolidayEvent(EVENT_ARENAS_ACTIVATE_TORNADO))
    if (!AddCreature(1337026, 0, TEAM_NEUTRAL, 4058.952637f, 2922.107178f, 13.512731f, 0.463424f, urand(15, 20)))
    {
        TC_LOG_ERROR("sql.sql", "BatteGroundNA: Failed to spawn tornado!");
        return false;
    }
    // readymarkers
    if (sWorld->getBoolConfig(CONFIG_ARENA_READYMARK_ENABLED))
    {
        AddObject(BG_NA_OBJECT_READYMARKER_1, BG_NA_OBJECT_READYMARKER, 4090.634766f, 2875.984863f, 12.173015f, 3.6f, 0, 0, 0, 0, RESPAWN_IMMEDIATELY);
        AddObject(BG_NA_OBJECT_READYMARKER_2, BG_NA_OBJECT_READYMARKER, 4022.504395f, 2965.815918f, 12.197223f, 0.5f, 0, 0, 0, 0, RESPAWN_IMMEDIATELY);
    }

    return true;
}
