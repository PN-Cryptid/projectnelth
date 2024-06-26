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

#include "Common.h"
#include "ObjectMgr.h"
#include "ArenaTeamMgr.h"
#include "World.h"
#include "WorldPacket.h"

#include "ArenaTeam.h"
#include "BattlegroundMgr.h"
#include "BattlegroundAV.h"
#include "BattlegroundAB.h"
#include "BattlegroundEY.h"
#include "BattlegroundEYR.h"
#include "BattlegroundWS.h"
#include "BattlegroundNA.h"
#include "BattlegroundBE.h"
#include "BattlegroundRL.h"
#include "BattlegroundSA.h"
#include "BattlegroundDS.h"
#include "BattlegroundRV.h"
#include "BattlegroundIC.h"
#include "BattlegroundTP.h"
#include "BattlegroundBFG.h"
#include "Chat.h"
#include "Map.h"
#include "MapInstanced.h"
#include "MapManager.h"
#include "Player.h"
#include "GameEventMgr.h"
#include "SharedDefines.h"
#include "Formulas.h"
#include "DisableMgr.h"
#include "Opcodes.h"

/*********************************************************/
/***            BATTLEGROUND MANAGER                   ***/
/*********************************************************/

BattlegroundMgr::BattlegroundMgr() :
    m_NextRatedArenaUpdate(sWorld->getIntConfig(CONFIG_ARENA_RATED_UPDATE_TIMER)),
    m_ArenaTesting(false), m_Testing(false)
{ }

BattlegroundMgr::~BattlegroundMgr()
{
    DeleteAllBattlegrounds();
}

void BattlegroundMgr::DeleteAllBattlegrounds()
{
    for (BattlegroundDataContainer::iterator itr1 = bgDataStore.begin(); itr1 != bgDataStore.end(); ++itr1)
    {
        BattlegroundData& data = itr1->second;

        while (!data.m_Battlegrounds.empty())
            delete data.m_Battlegrounds.begin()->second;
        data.m_Battlegrounds.clear();

        while (!data.BGFreeSlotQueue.empty())
            delete data.BGFreeSlotQueue.front();
    }

    bgDataStore.clear();
}

// used to update running battlegrounds, and delete finished ones
void BattlegroundMgr::Update(uint32 diff)
{
    for (BattlegroundDataContainer::iterator itr1 = bgDataStore.begin(); itr1 != bgDataStore.end(); ++itr1)
    {
        BattlegroundContainer& bgs = itr1->second.m_Battlegrounds;
        BattlegroundContainer::iterator itrDelete = bgs.begin();
        // first one is template and should not be deleted
        for (BattlegroundContainer::iterator itr = ++itrDelete; itr != bgs.end();)
        {
            itrDelete = itr++;
            Battleground* bg = itrDelete->second;

            bg->Update(diff);
            if (bg->ToBeDeleted())
            {
                itrDelete->second = NULL;
                bgs.erase(itrDelete);
                BattlegroundClientIdsContainer& clients = itr1->second.m_ClientBattlegroundIds[bg->GetBracketId()];
                if (!clients.empty())
                     clients.erase(bg->GetClientInstanceID());

                delete bg;
            }
        }
    }

    // update events timer
    for (int qtype = BATTLEGROUND_QUEUE_NONE; qtype < MAX_BATTLEGROUND_QUEUE_TYPES; ++qtype)
        m_BattlegroundQueues[qtype].UpdateEvents(diff);

    // update scheduled queues
    if (!m_QueueUpdateScheduler.empty())
    {
        std::vector<uint64> scheduled;
        std::swap(scheduled, m_QueueUpdateScheduler);

        for (uint8 i = 0; i < scheduled.size(); i++)
        {
            uint32 arenaMMRating = scheduled[i] >> 32;
            uint8 arenaType = scheduled[i] >> 24 & 255;
            BattlegroundQueueTypeId bgQueueTypeId = BattlegroundQueueTypeId(scheduled[i] >> 16 & 255);
            BattlegroundTypeId bgTypeId = BattlegroundTypeId((scheduled[i] >> 8) & 255);
            BattlegroundBracketId bracket_id = BattlegroundBracketId(scheduled[i] & 255);
            m_BattlegroundQueues[bgQueueTypeId].BattlegroundQueueUpdate(diff, bgTypeId, bracket_id, arenaType, arenaMMRating > 0, arenaMMRating);
        }
    }

    // if rating difference counts, maybe force-update queues
    if (sWorld->getIntConfig(CONFIG_ARENA_MAX_RATING_DIFFERENCE) && sWorld->getIntConfig(CONFIG_ARENA_RATED_UPDATE_TIMER))
    {
        // it's time to force update
        if (m_NextRatedArenaUpdate < diff)
        {
            // forced update for rated arenas (scan all, but skipped non rated)
            TC_LOG_TRACE("bg.arena", "BattlegroundMgr: UPDATING ARENA QUEUES");
            for (int qtype = BATTLEGROUND_QUEUE_2v2; qtype <= BATTLEGROUND_QUEUE_1v1_SOLO; ++qtype)
                for (int bracket = BG_BRACKET_ID_FIRST; bracket < MAX_BATTLEGROUND_BRACKETS; ++bracket)
                    m_BattlegroundQueues[qtype].BattlegroundQueueUpdate(diff,
                        BATTLEGROUND_AA, BattlegroundBracketId(bracket),
                        BattlegroundMgr::BGArenaType(BattlegroundQueueTypeId(qtype)), true, 0);

            // forced update for rated battlegrounds
            TC_LOG_TRACE("bg.battleground", "BattlegroundMgr: UPDATING RATED BG QUEUES");
            for (int bracket = BG_BRACKET_ID_FIRST; bracket < MAX_BATTLEGROUND_BRACKETS; ++bracket)
                m_BattlegroundQueues[BATTLEGROUND_QUEUE_RBG_10_VS_10].BattlegroundQueueUpdate(diff,
                BATTLEGROUND_RATED_10_VS_10, BattlegroundBracketId(bracket), 0, true, 0);

            m_NextRatedArenaUpdate = sWorld->getIntConfig(CONFIG_ARENA_RATED_UPDATE_TIMER);
        }
        else
            m_NextRatedArenaUpdate -= diff;
    }
}

void BattlegroundMgr::BuildBattlegroundStatusPacket(WorldPacket* data, Battleground* bg, Player* player, uint8 QueueSlot, uint8 StatusID, uint32 Time1, uint32 Time2, uint8 arenatype, bool battlegroundEnd)
{
    ObjectGuid playerGuid = player->GetGUID();
    ObjectGuid bgGuid;

    if (bg)
        bgGuid = bg->GetGUID();
    else
        StatusID = STATUS_NONE;

    switch (StatusID)
    {
        case STATUS_NONE:
        {
            data->Initialize(SMSG_BATTLEFIELD_STATUS);

            data->WriteBit(playerGuid[0]);
            data->WriteBit(playerGuid[4]);
            data->WriteBit(playerGuid[7]);
            data->WriteBit(playerGuid[1]);
            data->WriteBit(playerGuid[6]);
            data->WriteBit(playerGuid[3]);
            data->WriteBit(playerGuid[5]);
            data->WriteBit(playerGuid[2]);

            data->WriteByteSeq(playerGuid[5]);
            data->WriteByteSeq(playerGuid[6]);
            data->WriteByteSeq(playerGuid[7]);
            data->WriteByteSeq(playerGuid[2]);
            *data << uint32(arenatype ? arenatype : 1); // Player count, 1 for bgs, 2-3-5 for arena (2v2, 3v3, 5v5)
            data->WriteByteSeq(playerGuid[3]);
            data->WriteByteSeq(playerGuid[1]);
            *data << uint32(QueueSlot);                 // Queue slot
            *data << uint32(Time1);                     // Join Time
            data->WriteByteSeq(playerGuid[0]);
            data->WriteByteSeq(playerGuid[4]);
            break;
        }
        case STATUS_WAIT_QUEUE:
        {
            data->Initialize(SMSG_BATTLEFIELD_STATUS_QUEUED);

            data->WriteBit(playerGuid[3]);
            data->WriteBit(playerGuid[0]);
            data->WriteBit(bgGuid[3]);
            data->WriteBit(playerGuid[2]);
            data->WriteBit(1);                          // Eligible In Queue
            data->WriteBit(0);                          // Join Failed, 1 when it's arena ...
            data->WriteBit(bgGuid[2]);
            data->WriteBit(playerGuid[1]);
            data->WriteBit(bgGuid[0]);
            data->WriteBit(bgGuid[6]);
            data->WriteBit(bgGuid[4]);
            data->WriteBit(playerGuid[6]);
            data->WriteBit(playerGuid[7]);
            data->WriteBit(bgGuid[7]);
            data->WriteBit(bgGuid[5]);
            data->WriteBit(playerGuid[4]);
            data->WriteBit(playerGuid[5]);
            data->WriteBit(bg->isRated());              // Is Rated
            data->WriteBit(0);                          // Waiting On Other Activity
            data->WriteBit(bgGuid[1]);

            data->FlushBits();

            data->WriteByteSeq(playerGuid[0]);
            *data << uint32(bg->isArena() ? arenatype : 1); // Player count, 1 for bgs, 2-3-5 for arena (2v2, 3v3, 5v5)
            data->WriteByteSeq(bgGuid[5]);
            data->WriteByteSeq(playerGuid[3]);
            *data << uint32(Time1);                     // Estimated Wait Time
            data->WriteByteSeq(bgGuid[7]);
            data->WriteByteSeq(bgGuid[1]);
            data->WriteByteSeq(bgGuid[2]);
            *data << uint8(0);                          // unk
            data->WriteByteSeq(bgGuid[4]);
            data->WriteByteSeq(playerGuid[2]);
            *data << uint8(0);                          // unk
            data->WriteByteSeq(bgGuid[6]);
            data->WriteByteSeq(playerGuid[7]);
            data->WriteByteSeq(bgGuid[3]);
            data->WriteByteSeq(playerGuid[6]);
            data->WriteByteSeq(bgGuid[0]);
            *data << uint32(Time2);                     // Join Time
            *data << uint32(QueueSlot);                 // Queue slot
            *data << uint8(bg->GetMinLevel());          // Min Level
            *data << uint32(GetMSTimeDiffToNow(Time2)); // Time since joined
            data->WriteByteSeq(playerGuid[1]);
            data->WriteByteSeq(playerGuid[5]);
            *data << uint32(bg->GetClientInstanceID()); // Client Instance ID
            data->WriteByteSeq(playerGuid[4]);
            break;
        }
        case STATUS_WAIT_JOIN:
        {
            data->Initialize(SMSG_BATTLEFIELD_STATUS_NEEDCONFIRMATION);

            *data << uint32(bg->GetClientInstanceID()); // Client Instance ID
            *data << uint32(Time1);                     // Time until closed
            *data << uint8(bg->isRatedBattleground());  // bool rated match
            *data << uint32(QueueSlot);                 // Queue slot
            *data << uint32(Time2);                     // Join Time
            *data << uint8(bg->GetMinLevel());          // Min Level
            *data << uint32(bg->isArena() ? arenatype : 1); // Player count, 1 for bgs, 2-3-5 for arena (2v2, 3v3, 5v5)
            *data << uint32((bg->isArena() || bg->GetTypeID(true) == BATTLEGROUND_RB) ? -1 : bg->GetMapId()); // Map Id
            *data << uint8(bg->isRatedBattleground() ? 10 : 0); // rated match player count (10 for 10vs10 for example)

            data->WriteBit(playerGuid[5]);
            data->WriteBit(playerGuid[2]);
            data->WriteBit(playerGuid[1]);
            data->WriteBit(bgGuid[2]);
            data->WriteBit(playerGuid[4]);
            data->WriteBit(bgGuid[6]);
            data->WriteBit(bgGuid[3]);
            data->WriteBit(bg->isRated());              // Is Rated
            data->WriteBit(playerGuid[7]);
            data->WriteBit(playerGuid[3]);
            data->WriteBit(bgGuid[7]);
            data->WriteBit(bgGuid[0]);
            data->WriteBit(bgGuid[4]);
            data->WriteBit(playerGuid[6]);
            data->WriteBit(bgGuid[1]);
            data->WriteBit(bgGuid[5]);
            data->WriteBit(playerGuid[0]);

            data->WriteByteSeq(bgGuid[6]);
            data->WriteByteSeq(bgGuid[5]);
            data->WriteByteSeq(bgGuid[7]);
            data->WriteByteSeq(bgGuid[2]);
            data->WriteByteSeq(playerGuid[0]);
            data->WriteByteSeq(playerGuid[7]);
            data->WriteByteSeq(bgGuid[4]);
            data->WriteByteSeq(playerGuid[1]);
            data->WriteByteSeq(bgGuid[0]);
            data->WriteByteSeq(playerGuid[4]);
            data->WriteByteSeq(bgGuid[1]);
            data->WriteByteSeq(playerGuid[5]);
            data->WriteByteSeq(bgGuid[3]);
            data->WriteByteSeq(playerGuid[6]);
            data->WriteByteSeq(playerGuid[2]);
            data->WriteByteSeq(playerGuid[3]);
            break;
        }
        case STATUS_IN_PROGRESS:
        {
            data->Initialize(SMSG_BATTLEFIELD_STATUS_ACTIVE);

            data->WriteBit(playerGuid[2]);
            data->WriteBit(playerGuid[7]);
            data->WriteBit(bgGuid[7]);
            data->WriteBit(bgGuid[1]);
            data->WriteBit(playerGuid[5]);
            data->WriteBit(player->GetTeam() == HORDE ? 0 : 1);
            data->WriteBit(bgGuid[0]);
            data->WriteBit(playerGuid[1]);
            data->WriteBit(bgGuid[3]);
            data->WriteBit(playerGuid[6]);
            data->WriteBit(bgGuid[5]);
            data->WriteBit(bg->isRated());              // Is Rated
            data->WriteBit(playerGuid[4]);
            data->WriteBit(bgGuid[6]);
            data->WriteBit(bgGuid[4]);
            data->WriteBit(bgGuid[2]);
            data->WriteBit(playerGuid[3]);
            data->WriteBit(playerGuid[0]);

            data->FlushBits();

            data->WriteByteSeq(bgGuid[4]);
            data->WriteByteSeq(bgGuid[5]);
            data->WriteByteSeq(playerGuid[5]);
            data->WriteByteSeq(bgGuid[1]);
            data->WriteByteSeq(bgGuid[6]);
            data->WriteByteSeq(bgGuid[3]);
            data->WriteByteSeq(bgGuid[7]);
            data->WriteByteSeq(playerGuid[6]);

            *data << uint32(Time1);                     // Join Time
            *data << uint8(0);                          // unk

            data->WriteByteSeq(playerGuid[4]);
            data->WriteByteSeq(playerGuid[1]);

            *data << uint32(QueueSlot);                 // Queue slot
            *data << uint8(0);                          // unk
            *data << uint32(bg->isArena() ? arenatype : 1); // Player count, 1 for bgs, 2-3-5 for arena (2v2, 3v3, 5v5)
            *data << uint32(bg->GetMapId());            // Map Id
            *data << uint8(bg->GetMinLevel());          // Min Level
            *data << uint32(Time2);                     // Elapsed Time

            data->WriteByteSeq(playerGuid[2]);
            *data << uint32(battlegroundEnd ? bg->GetRemainingTime() : bg->GetPrematureCountDownTimer());    // Remaining Time
            data->WriteByteSeq(playerGuid[0]);
            data->WriteByteSeq(playerGuid[3]);
            data->WriteByteSeq(bgGuid[2]);

            *data << uint32(bg->GetClientInstanceID()); // Client Instance ID or faction ?

            data->WriteByteSeq(bgGuid[0]);
            data->WriteByteSeq(playerGuid[7]);
            break;
        }
        case STATUS_WAIT_LEAVE:
            break;
    }
}

void BattlegroundMgr::BuildPvpLogDataPacket(WorldPacket* data, Battleground* bg)
{
    ByteBuffer buff;
    uint8 isArena = (bg->isArena() ? 1 : 0);    // Arena names
    uint8 isRatedArena = ((isArena && bg->isRated()) ? 1 : 0);

    data->Initialize(SMSG_PVP_LOG_DATA, (1+1+4+40*bg->GetPlayerScoresSize()));
    data->WriteBit(isArena);
    data->WriteBit(isRatedArena);

    if (isArena)
    {
        for (int8 i = 0; i < BG_TEAMS_COUNT; ++i)
        {
            if (ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(bg->GetArenaTeamIdByIndex(i)))
                data->WriteBits(at->GetName().length(), 8);
            else
                data->WriteBits(0, 8);
        }
    }

    data->WriteBits(bg->GetPlayerScoresSize(), 21);

    if (bg->GetPlayerScoresSize())
    for (auto itr = bg->GetPlayerScoresBegin(); itr != bg->GetPlayerScoresEnd(); ++itr)
        if (auto current_guid = (itr->first))
            if (auto current_score = (itr->second))
            {
                    if (!isArena && !bg->IsPlayerInBattleground(current_guid))
                    {
                        TC_LOG_ERROR("bg.battleground", "Player " UI64FMTD " has scoreboard entry for battleground %u but is not in battleground!", current_guid, bg->GetTypeID(true));
                        continue;
                    }

                    // Rated BG rating stuff.
                    ObjectGuid playerGUID = current_guid;
                    uint8 hasRatedBGRating = bg->isRatedBattleground() ? 1 : 0;
                    uint32 RatedBGRating = 0;

                    if (Player* player = ObjectAccessor::FindPlayer(playerGUID)) // Get the rating if player is online.
                        RatedBGRating = (bg->GetWinner() == bg->GetPlayerTeam(playerGUID) ? (player->GetRBGPersonalRating() - 20) :
                        (player->GetRBGPersonalRating() > 1500 ? (player->GetRBGPersonalRating() + 20) : player->GetRBGPersonalRating()));

                    int32 RatedBGRatingChange = 0;

                    if (Player* player = ObjectAccessor::FindPlayer(playerGUID)) // Get the rating change if player is online.
                        RatedBGRatingChange = (bg->GetWinner() == bg->GetPlayerTeam(playerGUID) ? 20 : (player->GetRBGPersonalRating() > 1500 ? -20 : 0));

                    data->WriteBit(0);                // Unk 1
                    data->WriteBit(hasRatedBGRating); // hasPersonalBGRating
                    data->WriteBit(playerGUID[2]);
                    data->WriteBit(!isArena);         // isBattleground
                    data->WriteBit(0);                // hasPreMatchMMR  - After 4.2 update is always zero (number).
                    data->WriteBit(isRatedArena);     // hasBGRatingChange
                    data->WriteBit(0);                // hasMMRChange  - After 4.2 update is always zero (number).
                    data->WriteBit(playerGUID[3]);
                    data->WriteBit(playerGUID[0]);
                    data->WriteBit(playerGUID[5]);
                    data->WriteBit(playerGUID[1]);
                    data->WriteBit(playerGUID[6]);
                    data->WriteBit(current_score->BgTeam == HORDE ? 0 : 1);
                    data->WriteBit(playerGUID[7]);

                    buff << uint32(current_score->HealingDone);             // healing done
                    buff << uint32(current_score->DamageDone);              // damage done

                    if (!isArena)
                    {
                        buff << uint32(current_score->BonusHonor / 100);
                        buff << uint32(current_score->Deaths);
                        buff << uint32(current_score->HonorableKills);
                    }

                    buff.WriteByteSeq(playerGUID[4]);
                    buff << uint32(current_score->KillingBlows);

                    //breaks scoreboard
                    if (isRatedArena/* || (bg->isRatedBattleground() && bg->GetStatus() == STATUS_WAIT_LEAVE)*/)
                    {
                        int32 rating_change = bg->GetArenaTeamRatingChangeForTeam(current_score->BgTeam);
                        buff << uint32(hasRatedBGRating ? RatedBGRatingChange : rating_change);
                    }
       
                    buff.WriteByteSeq(playerGUID[5]);

                    // if (hasMMRChange)
                    // << uint32(mmrChange) - 0 since 4.2.

                    if (hasRatedBGRating)
                        buff << uint32(RatedBGRating);

                    buff.WriteByteSeq(playerGUID[1]);
                    buff.WriteByteSeq(playerGUID[6]);


                    buff << int32(current_score->TalentTree);

                    switch (bg->GetTypeID(true))                             // Custom values
                    {
                        /* Building scoreboard for ppl who request it:
                        TP: 726, zone 5031
                        WsG: 489, zone 3277
                        ArB: 529, zone 3358
                        BfG: 761, zone 5449
                        */
                        case BATTLEGROUND_RB:
                        case BATTLEGROUND_RATED_10_VS_10:
                            switch (bg->GetMapId())
                            {
                                case 489:
                                    data->WriteBits(0x00000002, 24);
                                    buff << uint32(((BattlegroundWGScore*)current_score)->FlagCaptures);        // flag captures
                                    buff << uint32(((BattlegroundWGScore*)current_score)->FlagReturns);         // flag returns
                                    break;
                                case 566:
                                {
                                    if (bg->isRatedBattleground())
                                    {
                                        data->WriteBits(0x00000003, 24);
                                        buff << uint32(((BattlegroundEYRScore*)current_score)->FlagCaptures);        // flag captures
                                        buff << uint32(((BattlegroundEYRScore*)current_score)->BasesAssaulted);       // bases assaulted
                                        buff << uint32(((BattlegroundEYRScore*)current_score)->BasesDefended);        // bases defended
                                    }
                                    else
                                    {
                                        data->WriteBits(0x00000001, 24);
                                        buff << uint32(((BattlegroundEYScore*)current_score)->FlagCaptures);        // flag captures
                                    }
                                    break;
                                }
                                case 968:
                                    data->WriteBits(0x00000003, 24);
                                    buff << uint32(((BattlegroundEYRScore*)current_score)->FlagCaptures);        // flag captures
                                    buff << uint32(((BattlegroundEYRScore*)current_score)->BasesAssaulted);       // bases assaulted
                                    buff << uint32(((BattlegroundEYRScore*)current_score)->BasesDefended);        // bases defended
                                    break;
                                case 529:
                                    data->WriteBits(0x00000002, 24);
                                    buff << uint32(((BattlegroundABScore*)current_score)->BasesAssaulted);      // bases assaulted
                                    buff << uint32(((BattlegroundABScore*)current_score)->BasesDefended);       // bases defended
                                    break;
                                case 30:
                                    data->WriteBits(0x00000005, 24);
                                    buff << uint32(((BattlegroundAVScore*)current_score)->GraveyardsAssaulted); // GraveyardsAssaulted
                                    buff << uint32(((BattlegroundAVScore*)current_score)->GraveyardsDefended);  // GraveyardsDefended
                                    buff << uint32(((BattlegroundAVScore*)current_score)->TowersAssaulted);     // TowersAssaulted
                                    buff << uint32(((BattlegroundAVScore*)current_score)->TowersDefended);      // TowersDefended
                                    buff << uint32(((BattlegroundAVScore*)current_score)->MinesCaptured);       // MinesCaptured
                                    break;
                                case 607:
                                    data->WriteBits(0x00000002, 24);
                                    buff << uint32(((BattlegroundSAScore*)current_score)->demolishers_destroyed);
                                    buff << uint32(((BattlegroundSAScore*)current_score)->gates_destroyed);
                                    break;
                                case 628:                                                                      // IC
                                    data->WriteBits(0x00000002, 24);
                                    buff << uint32(((BattlegroundICScore*)current_score)->BasesAssaulted);       // bases assaulted
                                    buff << uint32(((BattlegroundICScore*)current_score)->BasesDefended);        // bases defended
                                    break;
                                case 726:
                                    data->WriteBits(0x00000002, 24);
                                    buff << uint32(((BattlegroundTPScore*)current_score)->FlagCaptures);         // flag captures
                                    buff << uint32(((BattlegroundTPScore*)current_score)->FlagReturns);          // flag returns
                                    break;
                                case 761:
                                    data->WriteBits(0x00000002, 24);
                                    buff << uint32(((BattlegroundBFGScore*)current_score)->BasesAssaulted);      // bases assaulted
                                    buff << uint32(((BattlegroundBFGScore*)current_score)->BasesDefended);       // bases defended
                                    break;
                                default:
                                    data->WriteBits(0, 24);
                                    break;
                            }
                            break;
                        case BATTLEGROUND_AV:
                            data->WriteBits(0x00000005, 24);
                            buff << uint32(((BattlegroundAVScore*)current_score)->GraveyardsAssaulted); // GraveyardsAssaulted
                            buff << uint32(((BattlegroundAVScore*)current_score)->GraveyardsDefended);  // GraveyardsDefended
                            buff << uint32(((BattlegroundAVScore*)current_score)->TowersAssaulted);     // TowersAssaulted
                            buff << uint32(((BattlegroundAVScore*)current_score)->TowersDefended);      // TowersDefended
                            buff << uint32(((BattlegroundAVScore*)current_score)->MinesCaptured);       // MinesCaptured
                            break;
                        case BATTLEGROUND_WS:
                            data->WriteBits(0x00000002, 24);
                            buff << uint32(((BattlegroundWGScore*)current_score)->FlagCaptures);        // flag captures
                            buff << uint32(((BattlegroundWGScore*)current_score)->FlagReturns);         // flag returns
                            break;
                        case BATTLEGROUND_AB:
                            data->WriteBits(0x00000002, 24);
                            buff << uint32(((BattlegroundABScore*)current_score)->BasesAssaulted);      // bases assaulted
                            buff << uint32(((BattlegroundABScore*)current_score)->BasesDefended);       // bases defended
                            break;
                        case BATTLEGROUND_EY_RATED:
                            data->WriteBits(0x00000003, 24);
                            buff << uint32(((BattlegroundEYRScore*)current_score)->FlagCaptures);        // flag captures
                            buff << uint32(((BattlegroundEYRScore*)current_score)->BasesAssaulted);       // bases assaulted
                            buff << uint32(((BattlegroundEYRScore*)current_score)->BasesDefended);        // bases defended
                            break;
                        case BATTLEGROUND_EY:
                            data->WriteBits(0x00000001, 24);
                            buff << uint32(((BattlegroundEYScore*)current_score)->FlagCaptures);        // flag captures
                            break;
                        case BATTLEGROUND_SA:
                            data->WriteBits(0x00000002, 24);
                            buff << uint32(((BattlegroundSAScore*)current_score)->demolishers_destroyed);
                            buff << uint32(((BattlegroundSAScore*)current_score)->gates_destroyed);
                            break;
                        case BATTLEGROUND_IC:
                            data->WriteBits(0x00000002, 24);
                            buff << uint32(((BattlegroundICScore*)current_score)->BasesAssaulted);       // bases assaulted
                            buff << uint32(((BattlegroundICScore*)current_score)->BasesDefended);        // bases defended
                            break;
                        case BATTLEGROUND_TP:
                            data->WriteBits(0x00000002, 24);
                            buff << uint32(((BattlegroundTPScore*)current_score)->FlagCaptures);         // flag captures
                            buff << uint32(((BattlegroundTPScore*)current_score)->FlagReturns);          // flag returns
                            break;
                        case BATTLEGROUND_BFG:
                            data->WriteBits(0x00000002, 24);
                            buff << uint32(((BattlegroundBFGScore*)current_score)->BasesAssaulted);      // bases assaulted
                            buff << uint32(((BattlegroundBFGScore*)current_score)->BasesDefended);       // bases defended
                            break;
                        case BATTLEGROUND_NA:
                        case BATTLEGROUND_BE:
                        case BATTLEGROUND_AA:
                        case BATTLEGROUND_RL:
                        case BATTLEGROUND_DS:
                        case BATTLEGROUND_RV:
                            data->WriteBits(0, 24);
                            break;
                        default:
                            data->WriteBits(0, 24);
                            break;
                    }

                    data->WriteBit(playerGUID[4]);

                    buff.WriteByteSeq(playerGUID[0]);
                    buff.WriteByteSeq(playerGUID[3]);

                    // if (hasPreMatchMMR)
                    // << uint32(preMatchMMR)

                    buff.WriteByteSeq(playerGUID[7]);
                    buff.WriteByteSeq(playerGUID[2]);
            }
            else TC_LOG_ERROR("sql.sql", "else current_score for %u", current_guid);
            else TC_LOG_ERROR("sql.sql", "else current_guid");
            

    data->WriteBit(bg->GetStatus() == STATUS_WAIT_LEAVE);    // If Ended

    if (isRatedArena || bg->isRatedBattleground())                                             // arena
    {
        // it seems this must be according to BG_WINNER_A/H and _NOT_ BG_TEAM_A/H
        if (isArena)
        {
            for (int8 i = 1; i >= 0; --i)
            {
                int32 rating_change = bg->GetArenaTeamRatingChangeByIndex(i);

                uint32 pointsLost = rating_change < 0 ? -rating_change : 0;
                uint32 pointsGained = rating_change > 0 ? rating_change : 0;
                uint32 MatchmakerRating = bg->GetArenaMatchmakerRatingByIndex(i);

                *data << uint32(MatchmakerRating);              // Matchmaking Value
                *data << uint32(pointsLost);                    // Rating Lost
                *data << uint32(pointsGained);                  // Rating gained
                TC_LOG_DEBUG("bg.battleground", "rating change: %d", rating_change);
            }
        }
        else // Rated BG shit here, same as for arena but need to get the exact order and the unk values meaning of this sending to complete it.
        {
            //*data << uint32(100);              // Horde Team Matchmaking Value ?
            //*data << uint32(0);                // Unk
            //*data << uint32(0);                // Rating gained
        }
    }

    data->FlushBits();
    data->append(buff);

    if (isArena)
        for (int8 i = 0; i < BG_TEAMS_COUNT; ++i)
            if (ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(bg->GetArenaTeamIdByIndex(i)))
                data->WriteString(at->GetName());

    *data << uint8(bg->GetPlayersCountByTeam(HORDE));

    if (bg->GetStatus() == STATUS_WAIT_LEAVE)
        *data << uint8(bg->GetWinner());

    *data << uint8(bg->GetPlayersCountByTeam(ALLIANCE));
}

void BattlegroundMgr::BuildStatusFailedPacket(WorldPacket* data, Battleground* bg, Player* player, uint8 QueueSlot, GroupJoinBattlegroundResult result)
{
    ObjectGuid guidBytes1 = player->GetGUID(); // player who caused the error
    ObjectGuid guidBytes2 = bg->GetGUID();
    ObjectGuid unkGuid3 = 0;

    data->Initialize(SMSG_BATTLEFIELD_STATUS_FAILED);

    data->WriteBit(guidBytes2[3]);
    data->WriteBit(unkGuid3[3]);
    data->WriteBit(guidBytes1[3]);
    data->WriteBit(unkGuid3[0]);
    data->WriteBit(guidBytes2[6]);
    data->WriteBit(guidBytes1[5]);
    data->WriteBit(guidBytes1[6]);
    data->WriteBit(guidBytes1[4]);

    data->WriteBit(guidBytes1[2]);
    data->WriteBit(unkGuid3[1]);
    data->WriteBit(guidBytes2[1]);
    data->WriteBit(unkGuid3[5]);
    data->WriteBit(unkGuid3[6]);
    data->WriteBit(guidBytes1[1]);
    data->WriteBit(guidBytes2[7]);
    data->WriteBit(unkGuid3[4]);

    data->WriteBit(guidBytes2[2]);
    data->WriteBit(guidBytes2[5]);
    data->WriteBit(unkGuid3[7]);
    data->WriteBit(guidBytes2[4]);
    data->WriteBit(guidBytes2[0]);
    data->WriteBit(guidBytes1[0]);
    data->WriteBit(unkGuid3[2]);
    data->WriteBit(guidBytes1[7]);

    data->WriteByteSeq(guidBytes2[1]);

    *data << uint32(1);                         // Unk, always 1
    *data << uint32(QueueSlot);                 // Queue slot

    data->WriteByteSeq(guidBytes1[6]);
    data->WriteByteSeq(guidBytes1[3]);
    data->WriteByteSeq(guidBytes1[7]);
    data->WriteByteSeq(guidBytes1[4]);
    data->WriteByteSeq(guidBytes2[0]);
    data->WriteByteSeq(guidBytes1[5]);
    data->WriteByteSeq(guidBytes2[7]);
    data->WriteByteSeq(guidBytes2[6]);
    data->WriteByteSeq(guidBytes2[2]);
    data->WriteByteSeq(unkGuid3[6]);
    data->WriteByteSeq(unkGuid3[3]);
    data->WriteByteSeq(guidBytes1[1]);
    data->WriteByteSeq(guidBytes2[3]);
    data->WriteByteSeq(unkGuid3[0]);
    data->WriteByteSeq(unkGuid3[1]);
    data->WriteByteSeq(unkGuid3[4]);
    data->WriteByteSeq(guidBytes1[0]);
    data->WriteByteSeq(guidBytes2[5]);
    data->WriteByteSeq(unkGuid3[7]);
    data->WriteByteSeq(guidBytes2[4]);
    data->WriteByteSeq(guidBytes1[2]);

    *data << uint32(result);                    // Result

    data->WriteByteSeq(unkGuid3[2]);

    *data << uint32(player->GetBattlegroundQueueJoinTime(bg->GetTypeID())); // Join Time

    data->WriteByteSeq(unkGuid3[5]);
}

void BattlegroundMgr::BuildUpdateWorldStatePacket(WorldPacket* data, uint32 field, uint32 value)
{
    data->Initialize(SMSG_UPDATE_WORLD_STATE, 4 + 4 + 1);
    *data << uint32(field);
    *data << uint32(value);
    *data << uint8(0);
}

void BattlegroundMgr::BuildPlaySoundPacket(WorldPacket* data, uint32 soundid)
{
    data->Initialize(SMSG_PLAY_SOUND, 4);
    *data << uint32(soundid);
}

void BattlegroundMgr::BuildPlayerLeftBattlegroundPacket(WorldPacket* data, uint64 guid)
{
    ObjectGuid guidBytes = guid;

    data->Initialize(SMSG_BATTLEGROUND_PLAYER_LEFT, 8);

    data->WriteBit(guidBytes[7]);
    data->WriteBit(guidBytes[6]);
    data->WriteBit(guidBytes[2]);
    data->WriteBit(guidBytes[4]);
    data->WriteBit(guidBytes[5]);
    data->WriteBit(guidBytes[1]);
    data->WriteBit(guidBytes[3]);
    data->WriteBit(guidBytes[0]);

    data->WriteByteSeq(guidBytes[4]);
    data->WriteByteSeq(guidBytes[2]);
    data->WriteByteSeq(guidBytes[5]);
    data->WriteByteSeq(guidBytes[7]);
    data->WriteByteSeq(guidBytes[0]);
    data->WriteByteSeq(guidBytes[6]);
    data->WriteByteSeq(guidBytes[1]);
    data->WriteByteSeq(guidBytes[3]);
}

void BattlegroundMgr::BuildPlayerJoinedBattlegroundPacket(WorldPacket* data, uint64 guid)
{
    ObjectGuid guidBytes = guid;

    data->Initialize(SMSG_BATTLEGROUND_PLAYER_JOINED, 8);

    data->WriteBit(guidBytes[0]);
    data->WriteBit(guidBytes[4]);
    data->WriteBit(guidBytes[3]);
    data->WriteBit(guidBytes[5]);
    data->WriteBit(guidBytes[7]);
    data->WriteBit(guidBytes[6]);
    data->WriteBit(guidBytes[2]);
    data->WriteBit(guidBytes[1]);

    data->WriteByteSeq(guidBytes[1]);
    data->WriteByteSeq(guidBytes[5]);
    data->WriteByteSeq(guidBytes[3]);
    data->WriteByteSeq(guidBytes[2]);
    data->WriteByteSeq(guidBytes[0]);
    data->WriteByteSeq(guidBytes[7]);
    data->WriteByteSeq(guidBytes[4]);
    data->WriteByteSeq(guidBytes[6]);
}

Battleground* BattlegroundMgr::GetBattlegroundThroughClientInstance(uint32 instanceId, BattlegroundTypeId bgTypeId)
{
    //cause at HandleBattlegroundJoinOpcode the clients sends the instanceid he gets from
    //SMSG_BATTLEFIELD_LIST we need to find the battleground with this clientinstance-id
    Battleground* bg = GetBattlegroundTemplate(bgTypeId);
    if (!bg)
        return NULL;

    if (bg->isArena())
        return GetBattleground(instanceId, bgTypeId);

    BattlegroundDataContainer::const_iterator it = bgDataStore.find(bgTypeId);
    if (it == bgDataStore.end())
        return NULL;

    for (BattlegroundContainer::const_iterator itr = it->second.m_Battlegrounds.begin(); itr != it->second.m_Battlegrounds.end(); ++itr)
    {
        if (itr->second->GetClientInstanceID() == instanceId)
            return itr->second;
    }

    return NULL;
}

Battleground* BattlegroundMgr::GetBattleground(uint32 instanceId, BattlegroundTypeId bgTypeId)
{
    if (!instanceId)
        return NULL;

    BattlegroundDataContainer::const_iterator begin, end;

    if (bgTypeId == BATTLEGROUND_TYPE_NONE)
    {
        begin = bgDataStore.begin();
        end = bgDataStore.end();
    }
    else
    {
        end = bgDataStore.find(bgTypeId);
        if (end == bgDataStore.end())
            return NULL;
        begin = end++;
    }

    for (BattlegroundDataContainer::const_iterator it = begin; it != end; ++it)
    {
        BattlegroundContainer const& bgs = it->second.m_Battlegrounds;
        BattlegroundContainer::const_iterator itr = bgs.find(instanceId);
        if (itr != bgs.end())
           return itr->second;
    }

    return NULL;
}

Battleground* BattlegroundMgr::GetBattlegroundTemplate(BattlegroundTypeId bgTypeId)
{
    BattlegroundDataContainer::const_iterator itr = bgDataStore.find(bgTypeId);
    if (itr == bgDataStore.end())
        return NULL;

    BattlegroundContainer const& bgs = itr->second.m_Battlegrounds;
    // map is sorted and we can be sure that lowest instance id has only BG template
    return bgs.empty() ? NULL : bgs.begin()->second;
}

uint32 BattlegroundMgr::CreateClientVisibleInstanceId(BattlegroundTypeId bgTypeId, BattlegroundBracketId bracket_id)
{
    if (IsArenaType(bgTypeId))
        return 0;                                           //arenas don't have client-instanceids

    // we create here an instanceid, which is just for
    // displaying this to the client and without any other use..
    // the client-instanceIds are unique for each battleground-type
    // the instance-id just needs to be as low as possible, beginning with 1
    // the following works, because std::set is default ordered with "<"
    // the optimalization would be to use as bitmask std::vector<uint32> - but that would only make code unreadable

    BattlegroundClientIdsContainer& clientIds = bgDataStore[bgTypeId].m_ClientBattlegroundIds[bracket_id];
    uint32 lastId = 0;
    for (BattlegroundClientIdsContainer::const_iterator itr = clientIds.begin(); itr != clientIds.end();)
    {
        if ((++lastId) != *itr)                             //if there is a gap between the ids, we will break..
            break;
        lastId = *itr;
    }

    clientIds.insert(++lastId);
    return lastId;
}

// create a new battleground that will really be used to play
Battleground* BattlegroundMgr::CreateNewBattleground(BattlegroundTypeId originalBgTypeId, PvPDifficultyEntry const* bracketEntry, uint8 arenaType, bool isRated, BattlegroundTypeId bgTypeIdRandom, bool isWarGame)
{
    BattlegroundTypeId bgTypeId = originalBgTypeId;
    BattlegroundTypeId bgTypeIdRated = BATTLEGROUND_TYPE_NONE;
    bool isRandom = false;
    bool isRandomArena = false;
    bool isRatedBg = false;

    switch (originalBgTypeId)
    {
        case BATTLEGROUND_RB:
            isRandom = true;
            bgTypeId = bgTypeIdRandom;
            break;
        case BATTLEGROUND_AA:
            bgTypeId = sWorld->GetForcedBG() != BATTLEGROUND_TYPE_NONE ? sWorld->GetForcedBG() : GetRandomBG(originalBgTypeId);
            isRandomArena = true;
            break;
        case BATTLEGROUND_RATED_10_VS_10:
        case BATTLEGROUND_RATED_15_VS_15:
        case BATTLEGROUND_RATED_25_VS_25:
            isRatedBg = true;
            bgTypeIdRated = bgTypeId;
            bgTypeId = sWorld->GetForcedBG() != BATTLEGROUND_TYPE_NONE ? sWorld->GetForcedBG() : GetRandomBG(originalBgTypeId);
            break;
        default:
            break;
    }

    // get the template BG
    Battleground* bg_template = GetBattlegroundTemplate(bgTypeId);

    if (!bg_template)
    {
        TC_LOG_ERROR("bg.battleground", "Battleground: CreateNewBattleground - bg template not found for %u", bgTypeId);
        return NULL;
    }

    Battleground* bg = NULL;
    // create a copy of the BG template
    switch (bgTypeId)
    {
        case BATTLEGROUND_AV:
            bg = new BattlegroundAV(*(BattlegroundAV*)bg_template);
            break;
        case BATTLEGROUND_WS:
            bg = new BattlegroundWS(*(BattlegroundWS*)bg_template);
            break;
        case BATTLEGROUND_AB:
            bg = new BattlegroundAB(*(BattlegroundAB*)bg_template);
            break;
        case BATTLEGROUND_NA:
            bg = new BattlegroundNA(*(BattlegroundNA*)bg_template);
            break;
        case BATTLEGROUND_BE:
            bg = new BattlegroundBE(*(BattlegroundBE*)bg_template);
            break;
        case BATTLEGROUND_EY:
            bg = new BattlegroundEY(*(BattlegroundEY*)bg_template);
            break;
        case BATTLEGROUND_EY_RATED:
            bg = new BattlegroundEYR(*(BattlegroundEYR*)bg_template);
            break;
        case BATTLEGROUND_RL:
            bg = new BattlegroundRL(*(BattlegroundRL*)bg_template);
            break;
        case BATTLEGROUND_SA:
            bg = new BattlegroundSA(*(BattlegroundSA*)bg_template);
            break;
        case BATTLEGROUND_DS:
            bg = new BattlegroundDS(*(BattlegroundDS*)bg_template);
            break;
        case BATTLEGROUND_RV:
            bg = new BattlegroundRV(*(BattlegroundRV*)bg_template);
            break;
        case BATTLEGROUND_IC:
            bg = new BattlegroundIC(*(BattlegroundIC*)bg_template);
            break;
        case BATTLEGROUND_TP:
            bg = new BattlegroundTP(*(BattlegroundTP*)bg_template);
            break;
        case BATTLEGROUND_BFG:
            bg = new BattlegroundBFG(*(BattlegroundBFG*)bg_template);
            break;
        case BATTLEGROUND_RB:
        case BATTLEGROUND_AA:
            bg = new Battleground(*bg_template);
            break;
        default:
            return NULL;
    }

    bg->SetBracket(bracketEntry);
    bg->SetInstanceID(sMapMgr->GenerateInstanceId());
    if (isRatedBg)
    {
        bg->SetClientInstanceID(CreateClientVisibleInstanceId(bgTypeIdRated, bracketEntry->GetBracketId()));
        bg->SetRatedBattleground(true);
    }
    else
        bg->SetClientInstanceID(CreateClientVisibleInstanceId(isRandom ? BATTLEGROUND_RB : (isRandomArena ? BATTLEGROUND_AA : bgTypeId), bracketEntry->GetBracketId()));
    bg->Reset();                     // reset the new bg (set status to status_wait_queue from status_none)
    bg->SetStatus(STATUS_WAIT_JOIN); // start the joining of the bg
    bg->SetArenaType(arenaType);
    bg->SetTypeID(originalBgTypeId);
    bg->SetRandomTypeID(bgTypeId);
    bg->SetRated(isRated);
    bg->SetWarGame(isWarGame);
    bg->SetRandom(isRandom);
    bg->SetGuid(MAKE_NEW_GUID(bg->isRatedBattleground() ? (uint64(bgTypeIdRated) | UI64LIT(0x100000)) : (isRandom ? BATTLEGROUND_RB : (isRandomArena ? BATTLEGROUND_AA : bgTypeId)), arenaType, HIGHGUID_BATTLEGROUND));

    // Set up correct min/max player counts for scoreboards
    if (bg->isArena())
    {
        uint32 maxPlayersPerTeam = 0;
        switch (arenaType)
        {
            case ARENA_TYPE_1v1_SOLO:
                maxPlayersPerTeam = 1;
                break;
            case ARENA_TYPE_2v2:
                maxPlayersPerTeam = 2;
                break;
            case ARENA_TYPE_3v3:
            case ARENA_TYPE_3v3_SOLO:
                maxPlayersPerTeam = 3;
                break;
            case ARENA_TYPE_5v5:
                maxPlayersPerTeam = 5;
                break;
        }

        bg->SetMaxPlayersPerTeam(maxPlayersPerTeam);
        bg->SetMaxPlayers(maxPlayersPerTeam * 2);
    }

    return bg;
}

// used to create the BG templates
bool BattlegroundMgr::CreateBattleground(CreateBattlegroundData& data)
{
    // Create the BG
    Battleground* bg = NULL;
    switch (data.bgTypeId)
    {
        case BATTLEGROUND_AV:
            bg = new BattlegroundAV;
            break;
        case BATTLEGROUND_WS:
            bg = new BattlegroundWS;
            break;
        case BATTLEGROUND_AB:
            bg = new BattlegroundAB;
            break;
        case BATTLEGROUND_NA:
            bg = new BattlegroundNA;
            break;
        case BATTLEGROUND_BE:
            bg = new BattlegroundBE;
            break;
        case BATTLEGROUND_EY:
            bg = new BattlegroundEY;
            break;
        case BATTLEGROUND_EY_RATED:
            bg = new BattlegroundEYR;
            break;
        case BATTLEGROUND_RL:
            bg = new BattlegroundRL;
            break;
        case BATTLEGROUND_SA:
            bg = new BattlegroundSA;
            break;
        case BATTLEGROUND_DS:
            bg = new BattlegroundDS;
            break;
        case BATTLEGROUND_RV:
            bg = new BattlegroundRV;
            break;
        case BATTLEGROUND_IC:
            bg = new BattlegroundIC;
            break;
        case BATTLEGROUND_AA:
            bg = new Battleground;
            break;
        case BATTLEGROUND_TP:
            bg = new BattlegroundTP;
            break;
        case BATTLEGROUND_BFG:
            bg = new BattlegroundBFG;
            break;
        case BATTLEGROUND_RB:
            bg = new Battleground;
            bg->SetRandom(true);
            break;
        case BATTLEGROUND_RATED_10_VS_10:
        case BATTLEGROUND_RATED_15_VS_15:
        case BATTLEGROUND_RATED_25_VS_25:
            bg = new Battleground;
            break;
        default:
            return false;
    }

    bg->SetMapId(data.MapID);
    bg->SetTypeID(data.bgTypeId);
    bg->SetInstanceID(0);
    bg->SetArenaorBGType(data.IsArena);
    bg->SetMinPlayersPerTeam(data.MinPlayersPerTeam);
    bg->SetMaxPlayersPerTeam(data.MaxPlayersPerTeam);
    bg->SetMinPlayers(data.MinPlayersPerTeam * 2);
    bg->SetMaxPlayers(data.MaxPlayersPerTeam * 2);
    bg->SetName(data.BattlegroundName);
    bg->SetTeamStartLoc(ALLIANCE, data.Team1StartLocX, data.Team1StartLocY, data.Team1StartLocZ, data.Team1StartLocO);
    bg->SetTeamStartLoc(HORDE,    data.Team2StartLocX, data.Team2StartLocY, data.Team2StartLocZ, data.Team2StartLocO);
    bg->SetStartMaxDist(data.StartMaxDist);
    bg->SetActiveEventMask(data.active_event_mask);
    bg->SetLevelRange(data.LevelMin, data.LevelMax);
    bg->SetScriptId(data.scriptId);
    bg->SetGuid(MAKE_NEW_GUID(data.bgTypeId, 0, HIGHGUID_BATTLEGROUND));

    AddBattleground(bg);

    return true;
}

void BattlegroundMgr::CreateInitialBattlegrounds()
{
    uint32 oldMSTime = getMSTime();
    //                                               0   1                  2                  3       4       5                 6               7              8            9             10      11
    QueryResult result = WorldDatabase.Query("SELECT id, MinPlayersPerTeam, MaxPlayersPerTeam, MinLvl, MaxLvl, AllianceStartLoc, AllianceStartO, HordeStartLoc, HordeStartO, StartMaxDist, Weight, ScriptName, eventFlags FROM battleground_template");

    if (!result)
    {
        TC_LOG_ERROR("server.loading", ">> Loaded 0 battlegrounds. DB table `battleground_template` is empty.");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        uint32 bgTypeId = fields[0].GetUInt32();
        if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, bgTypeId, NULL))
            continue;

        // can be overwrite by values from DB
        BattlemasterListEntry const* bl = sBattlemasterListStore.LookupEntry(bgTypeId);
        if (!bl)
        {
            TC_LOG_ERROR("bg.battleground", "Battleground ID %u not found in BattlemasterList.dbc. Battleground not created.", bgTypeId);
            continue;
        }

        CreateBattlegroundData data;
        data.bgTypeId = BattlegroundTypeId(bgTypeId);
        data.IsArena = (bl->type == TYPE_ARENA);
        data.MinPlayersPerTeam = fields[1].GetUInt16();
        data.MaxPlayersPerTeam = fields[2].GetUInt16();
        data.LevelMin = fields[3].GetUInt8();
        data.LevelMax = fields[4].GetUInt8();
        float dist = fields[9].GetFloat();
        data.StartMaxDist = dist * dist;
        data.active_event_mask = fields[12].GetUInt32();

        data.scriptId = sObjectMgr->GetScriptId(fields[11].GetCString());
        data.BattlegroundName = bl->name;
        data.MapID = bl->mapid[0];

        if (data.MaxPlayersPerTeam == 0 || data.MinPlayersPerTeam > data.MaxPlayersPerTeam)
        {
            TC_LOG_ERROR("sql.sql", "Table `battleground_template` for id %u has bad values for MinPlayersPerTeam (%u) and MaxPlayersPerTeam(%u)",
                data.bgTypeId, data.MinPlayersPerTeam, data.MaxPlayersPerTeam);
            continue;
        }

        if (data.LevelMin == 0 || data.LevelMax == 0 || data.LevelMin > data.LevelMax)
        {
            TC_LOG_ERROR("sql.sql", "Table `battleground_template` for id %u has bad values for LevelMin (%u) and LevelMax(%u)",
                data.bgTypeId, data.LevelMin, data.LevelMax);
            continue;
        }

        if (data.bgTypeId == BATTLEGROUND_AA || data.bgTypeId == BATTLEGROUND_RB
            || data.bgTypeId == BATTLEGROUND_RATED_10_VS_10 || data.bgTypeId == BATTLEGROUND_RATED_15_VS_15
            || data.bgTypeId == BATTLEGROUND_RATED_25_VS_25)
        {
            data.Team1StartLocX = 0;
            data.Team1StartLocY = 0;
            data.Team1StartLocZ = 0;
            data.Team1StartLocO = fields[6].GetFloat();
            data.Team2StartLocX = 0;
            data.Team2StartLocY = 0;
            data.Team2StartLocZ = 0;
            data.Team2StartLocO = fields[8].GetFloat();
        }
        else
        {
            uint32 startId = fields[5].GetUInt32();
            if (WorldSafeLocsEntry const* start = sWorldSafeLocsStore.LookupEntry(startId))
            {
                data.Team1StartLocX = start->x;
                data.Team1StartLocY = start->y;
                data.Team1StartLocZ = start->z;
                data.Team1StartLocO = fields[6].GetFloat();
            }
            else
            {
                TC_LOG_ERROR("sql.sql", "Table `battleground_template` for id %u have non-existed WorldSafeLocs.dbc id %u in field `AllianceStartLoc`. BG not created.", data.bgTypeId, startId);
                continue;
            }

            startId = fields[7].GetUInt32();
            if (WorldSafeLocsEntry const* start = sWorldSafeLocsStore.LookupEntry(startId))
            {
                data.Team2StartLocX = start->x;
                data.Team2StartLocY = start->y;
                data.Team2StartLocZ = start->z;
                data.Team2StartLocO = fields[8].GetFloat();
            }
            else
            {
                TC_LOG_ERROR("sql.sql", "Table `battleground_template` for id %u have non-existed WorldSafeLocs.dbc id %u in field `HordeStartLoc`. BG not created.", data.bgTypeId, startId);
                continue;
            }
        }

        if (!CreateBattleground(data))
            continue;

        switch (data.bgTypeId)
        {
            case BATTLEGROUND_RL: // Ruins of Lordaernon
            case BATTLEGROUND_DS: // Dalaran Sewers
            case BATTLEGROUND_RV: // Ring of Valor
            case BATTLEGROUND_BE: // Blade's Edge Arena
            case BATTLEGROUND_NA: // Nagrand Arena
                m_ArenaSelectionWeights[data.bgTypeId] = fields[10].GetUInt8();
                break;
            case BATTLEGROUND_AV: // Alterac Valley
            case BATTLEGROUND_WS: // Warsong Gulch
            case BATTLEGROUND_AB: // Arathi Basin
            case BATTLEGROUND_EY: // Eye of the Storm
            case BATTLEGROUND_SA: // Strand of the Ancients
            case BATTLEGROUND_IC: // Isle of Conquest
            case BATTLEGROUND_TP: // Twin Peaks
            case BATTLEGROUND_BFG: // Battle For Gilneas
                m_BGSelectionWeights[data.bgTypeId] = fields[10].GetUInt8();
                break;
            default:
                break;
        }

        if (data.bgTypeId == BATTLEGROUND_WS || data.bgTypeId == BATTLEGROUND_AB || data.bgTypeId == BATTLEGROUND_EY_RATED
            || data.bgTypeId == BATTLEGROUND_BFG || data.bgTypeId == BATTLEGROUND_TP)
            m_RatedBGSelectionWeights[data.bgTypeId] = fields[10].GetUInt8();

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u battlegrounds in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void BattlegroundMgr::BuildBattlegroundListPacket(WorldPacket* data, uint64 guid, Player* player, BattlegroundTypeId bgTypeId)
{
    if (!player)
        return;

    BattlegroundDataContainer::iterator it = bgDataStore.find(bgTypeId);
    if (it == bgDataStore.end())
        return;

    PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(it->second.m_Battlegrounds.begin()->second->GetMapId(), player->getLevel());
    if (!bracketEntry)
        return;

    uint32 winner_conquest = (player->IsRandomDailyBgRewarded() ? BG_REWARD_WINNER_CONQUEST_LAST : BG_REWARD_WINNER_CONQUEST_FIRST) / CURRENCY_PRECISION;
    uint32 winner_honor = (player->IsRandomDailyBgRewarded() ? BG_REWARD_WINNER_HONOR_LAST : BG_REWARD_WINNER_HONOR_FIRST) / CURRENCY_PRECISION;
    uint32 loser_honor = (player->IsRandomDailyBgRewarded() ? BG_REWARD_LOSER_HONOR_LAST : BG_REWARD_LOSER_HONOR_FIRST) / CURRENCY_PRECISION;

    ObjectGuid guidBytes = guid;

    data->Initialize(SMSG_BATTLEFIELD_LIST);
    *data << uint32(winner_conquest)                        // Winner Conquest Reward or Random Winner Conquest Reward
          << uint32(winner_conquest)                        // Winner Conquest Reward or Random Winner Conquest Reward
          << uint32(loser_honor)                            // Loser Honor Reward or Random Loser Honor Reward
          << uint32(bgTypeId)                               // battleground id
          << uint32(loser_honor)                            // Loser Honor Reward or Random Loser Honor Reward
          << uint32(winner_honor)                           // Winner Honor Reward or Random Winner Honor Reward
          << uint32(winner_honor)                           // Winner Honor Reward or Random Winner Honor Reward
          << uint8(bracketEntry->maxLevel)                  // max level
          << uint8(bracketEntry->minLevel);                 // min level

    data->WriteBit(guidBytes[0]);
    data->WriteBit(guidBytes[1]);
    data->WriteBit(guidBytes[7]);
    data->WriteBit(0);                                      // unk
    data->WriteBit(0);                                      // unk

    size_t count_pos = data->bitwpos();
    data->WriteBits(0, 24);                                 // placeholder

    data->WriteBit(guidBytes[6]);
    data->WriteBit(guidBytes[4]);
    data->WriteBit(guidBytes[2]);
    data->WriteBit(guidBytes[3]);
    data->WriteBit(0);                                      // unk
    data->WriteBit(guidBytes[5]);
    data->WriteBit(1);                                      // hide battleground list window

    data->FlushBits();

    data->WriteByteSeq(guidBytes[6]);
    data->WriteByteSeq(guidBytes[1]);
    data->WriteByteSeq(guidBytes[7]);
    data->WriteByteSeq(guidBytes[5]);

    uint32 count = 0;
    BattlegroundBracketId bracketId = bracketEntry->GetBracketId();
    BattlegroundClientIdsContainer& clientIds = it->second.m_ClientBattlegroundIds[bracketId];
    for (BattlegroundClientIdsContainer::const_iterator itr = clientIds.begin(); itr != clientIds.end(); ++itr)
    {
        *data << uint32(*itr);
        ++count;
    }
    data->PutBits(count_pos, count, 24);                    // bg instance count

    data->WriteByteSeq(guidBytes[0]);
    data->WriteByteSeq(guidBytes[2]);
    data->WriteByteSeq(guidBytes[4]);
    data->WriteByteSeq(guidBytes[3]);
}

void BattlegroundMgr::SendToBattleground(Player* player, uint32 instanceId, BattlegroundTypeId bgTypeId)
{
    if (Battleground* bg = GetBattleground(instanceId, bgTypeId))
    {
        float x, y, z, O;
        uint32 mapid = bg->GetMapId();
        uint32 team = player->GetTeam();

        bg->GetTeamStartLoc(team, x, y, z, O);
        TC_LOG_DEBUG("bg.battleground", "BattlegroundMgr::SendToBattleground: Sending %s to map %u, X %f, Y %f, Z %f, O %f (bgType %u)", player->GetName().c_str(), mapid, x, y, z, O, bgTypeId);
        player->TeleportTo(mapid, x, y, z, O);
    }
    else
        TC_LOG_ERROR("bg.battleground", "BattlegroundMgr::SendToBattleground: Instance %u (bgType %u) not found while trying to teleport player %s", instanceId, bgTypeId, player->GetName().c_str());
}

void BattlegroundMgr::SendAreaSpiritHealerQueryOpcode(Player* player, Battleground* bg, uint64 guid)
{
    WorldPacket data(SMSG_AREA_SPIRIT_HEALER_TIME, 12);
    uint32 time_ = bg->getRespawnTime() - bg->GetLastResurrectTime();      // resurrect every 30 OR 45 seconds
    if (time_ == uint32(-1))
        time_ = 0;
    data << guid << time_;
    player->GetSession()->SendPacket(&data);
}

bool BattlegroundMgr::IsArenaType(BattlegroundTypeId bgTypeId)
{
    return bgTypeId == BATTLEGROUND_AA
            || bgTypeId == BATTLEGROUND_BE
            || bgTypeId == BATTLEGROUND_NA
            || bgTypeId == BATTLEGROUND_DS
            || bgTypeId == BATTLEGROUND_RV
            || bgTypeId == BATTLEGROUND_RL;
}

BattlegroundQueueTypeId BattlegroundMgr::BGQueueTypeId(BattlegroundTypeId bgTypeId, uint8 arenaType)
{
    switch (bgTypeId)
    {
        case BATTLEGROUND_AB:
            return BATTLEGROUND_QUEUE_AB;
        case BATTLEGROUND_AV:
            return BATTLEGROUND_QUEUE_AV;
        case BATTLEGROUND_EY:
            return BATTLEGROUND_QUEUE_EY;
        case BATTLEGROUND_IC:
            return BATTLEGROUND_QUEUE_IC;
        case BATTLEGROUND_TP:
            return BATTLEGROUND_QUEUE_TP;
        case BATTLEGROUND_BFG:
            return BATTLEGROUND_QUEUE_BFG;
        case BATTLEGROUND_RB:
            return BATTLEGROUND_QUEUE_RB;
        case BATTLEGROUND_SA:
            return BATTLEGROUND_QUEUE_SA;
        case BATTLEGROUND_WS:
            return BATTLEGROUND_QUEUE_WS;
        case BATTLEGROUND_RATED_10_VS_10:
            return BATTLEGROUND_QUEUE_RBG_10_VS_10;
        case BATTLEGROUND_RATED_15_VS_15:
            return BATTLEGROUND_QUEUE_RBG_15_VS_15;
        case BATTLEGROUND_RATED_25_VS_25:
            return BATTLEGROUND_QUEUE_RBG_25_VS_25;
        case BATTLEGROUND_AA:
        case BATTLEGROUND_BE:
        case BATTLEGROUND_DS:
        case BATTLEGROUND_NA:
        case BATTLEGROUND_RL:
        case BATTLEGROUND_RV:
            switch (arenaType)
            {
                case ARENA_TYPE_1v1_SOLO:
                    return BATTLEGROUND_QUEUE_1v1_SOLO;
                case ARENA_TYPE_2v2:
                    return BATTLEGROUND_QUEUE_2v2;
                case ARENA_TYPE_3v3:
                    return BATTLEGROUND_QUEUE_3v3;
                case ARENA_TYPE_3v3_SOLO:
                    return BATTLEGROUND_QUEUE_3v3_SOLO;
                case ARENA_TYPE_5v5:
                    return BATTLEGROUND_QUEUE_5v5;
                default:
                    return BATTLEGROUND_QUEUE_NONE;
            }
        default:
            return BATTLEGROUND_QUEUE_NONE;
    }
}

BattlegroundTypeId BattlegroundMgr::BGTemplateId(BattlegroundQueueTypeId bgQueueTypeId)
{
    switch (bgQueueTypeId)
    {
        case BATTLEGROUND_QUEUE_WS:
            return BATTLEGROUND_WS;
        case BATTLEGROUND_QUEUE_AB:
            return BATTLEGROUND_AB;
        case BATTLEGROUND_QUEUE_AV:
            return BATTLEGROUND_AV;
        case BATTLEGROUND_QUEUE_EY:
            return BATTLEGROUND_EY;
        case BATTLEGROUND_QUEUE_SA:
            return BATTLEGROUND_SA;
        case BATTLEGROUND_QUEUE_IC:
            return BATTLEGROUND_IC;
        case BATTLEGROUND_QUEUE_TP:
            return BATTLEGROUND_TP;
        case BATTLEGROUND_QUEUE_BFG:
            return BATTLEGROUND_BFG;
        case BATTLEGROUND_QUEUE_RB:
            return BATTLEGROUND_RB;
        case BATTLEGROUND_QUEUE_RBG_10_VS_10:
            return BATTLEGROUND_RATED_10_VS_10;
        case BATTLEGROUND_QUEUE_RBG_15_VS_15:
            return BATTLEGROUND_RATED_15_VS_15;
        case BATTLEGROUND_QUEUE_RBG_25_VS_25:
            return BATTLEGROUND_RATED_25_VS_25;
        case BATTLEGROUND_QUEUE_1v1_SOLO:
        case BATTLEGROUND_QUEUE_2v2:
        case BATTLEGROUND_QUEUE_3v3:
        case BATTLEGROUND_QUEUE_3v3_SOLO:
        case BATTLEGROUND_QUEUE_5v5:
            return BATTLEGROUND_AA;
        default:
            return BattlegroundTypeId(0);                   // used for unknown template (it existed and do nothing)
    }
}

uint8 BattlegroundMgr::BGArenaType(BattlegroundQueueTypeId bgQueueTypeId)
{
    switch (bgQueueTypeId)
    {
        case BATTLEGROUND_QUEUE_1v1_SOLO:
            return ARENA_TYPE_1v1_SOLO;
        case BATTLEGROUND_QUEUE_2v2:
            return ARENA_TYPE_2v2;
        case BATTLEGROUND_QUEUE_3v3:
            return ARENA_TYPE_3v3;
        case BATTLEGROUND_QUEUE_3v3_SOLO:
            return ARENA_TYPE_3v3_SOLO;
        case BATTLEGROUND_QUEUE_5v5:
            return ARENA_TYPE_5v5;
        default:
            return 0;
    }
}

void BattlegroundMgr::ToggleTesting()
{
    m_Testing = !m_Testing;
    sWorld->SendWorldText(m_Testing ? LANG_DEBUG_BG_ON : LANG_DEBUG_BG_OFF);
}

void BattlegroundMgr::ToggleArenaTesting()
{
    m_ArenaTesting = !m_ArenaTesting;
    sWorld->SendWorldText(m_ArenaTesting ? LANG_DEBUG_ARENA_ON : LANG_DEBUG_ARENA_OFF);
}

void BattlegroundMgr::SetHolidayWeekends(uint32 mask)
{
    for (uint32 bgtype = 1; bgtype < MAX_BATTLEGROUND_TYPE_ID; ++bgtype)
    {
        if (Battleground* bg = GetBattlegroundTemplate(BattlegroundTypeId(bgtype)))
        {
            bg->SetHoliday(mask & (1 << bgtype));
        }
    }
}

void BattlegroundMgr::ScheduleQueueUpdate(uint32 arenaMatchmakerRating, uint8 arenaType, BattlegroundQueueTypeId bgQueueTypeId, BattlegroundTypeId bgTypeId, BattlegroundBracketId bracket_id)
{
    //This method must be atomic, TODO add mutex
    //we will use only 1 number created of bgTypeId and bracket_id
    uint64 const scheduleId = ((uint64)arenaMatchmakerRating << 32) | (arenaType << 24) | (bgQueueTypeId << 16) | (bgTypeId << 8) | bracket_id;
    if (std::find(m_QueueUpdateScheduler.begin(), m_QueueUpdateScheduler.end(), scheduleId) == m_QueueUpdateScheduler.end())
        m_QueueUpdateScheduler.push_back(scheduleId);
}

uint32 BattlegroundMgr::GetMaxRatingDifference() const
{
    // this is for stupid people who can't use brain and set max rating difference to 0
    uint32 diff = sWorld->getIntConfig(CONFIG_ARENA_MAX_RATING_DIFFERENCE);
    if (diff == 0)
        diff = 5000;
    return diff;
}

uint32 BattlegroundMgr::GetRatingDiscardTimer() const
{
    return sWorld->getIntConfig(CONFIG_ARENA_RATING_DISCARD_TIMER);
}

uint32 BattlegroundMgr::GetPrematureFinishTime() const
{
    return sWorld->getIntConfig(CONFIG_BATTLEGROUND_PREMATURE_FINISH_TIMER);
}

void BattlegroundMgr::LoadBattleMastersEntry()
{
    uint32 oldMSTime = getMSTime();

    mBattleMastersMap.clear();                                  // need for reload case

    QueryResult result = WorldDatabase.Query("SELECT entry, bg_template FROM battlemaster_entry");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 battlemaster entries. DB table `battlemaster_entry` is empty!");
        return;
    }

    uint32 count = 0;

    do
    {
        ++count;

        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();
        uint32 bgTypeId  = fields[1].GetUInt32();
        if (!sBattlemasterListStore.LookupEntry(bgTypeId))
        {
            TC_LOG_ERROR("sql.sql", "Table `battlemaster_entry` contain entry %u for not existed battleground type %u, ignored.", entry, bgTypeId);
            continue;
        }

        mBattleMastersMap[entry] = BattlegroundTypeId(bgTypeId);
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u battlemaster entries in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

HolidayIds BattlegroundMgr::BGTypeToWeekendHolidayId(BattlegroundTypeId bgTypeId)
{
    switch (bgTypeId)
    {
        case BATTLEGROUND_AV: return HOLIDAY_CALL_TO_ARMS_AV;
        case BATTLEGROUND_EY: return HOLIDAY_CALL_TO_ARMS_EY;
        case BATTLEGROUND_WS: return HOLIDAY_CALL_TO_ARMS_WS;
        case BATTLEGROUND_SA: return HOLIDAY_CALL_TO_ARMS_SA;
        case BATTLEGROUND_AB: return HOLIDAY_CALL_TO_ARMS_AB;
        case BATTLEGROUND_IC: return HOLIDAY_CALL_TO_ARMS_IC;
        case BATTLEGROUND_TP: return HOLIDAY_CALL_TO_ARMS_TP;
        case BATTLEGROUND_BFG: return HOLIDAY_CALL_TO_ARMS_BFG;
        default: return HOLIDAY_NONE;
    }
}

BattlegroundTypeId BattlegroundMgr::WeekendHolidayIdToBGType(HolidayIds holiday)
{
    switch (holiday)
    {
        case HOLIDAY_CALL_TO_ARMS_AV: return BATTLEGROUND_AV;
        case HOLIDAY_CALL_TO_ARMS_EY: return BATTLEGROUND_EY;
        case HOLIDAY_CALL_TO_ARMS_WS: return BATTLEGROUND_WS;
        case HOLIDAY_CALL_TO_ARMS_SA: return BATTLEGROUND_SA;
        case HOLIDAY_CALL_TO_ARMS_AB: return BATTLEGROUND_AB;
        case HOLIDAY_CALL_TO_ARMS_IC: return BATTLEGROUND_IC;
        case HOLIDAY_CALL_TO_ARMS_TP: return BATTLEGROUND_TP;
        case HOLIDAY_CALL_TO_ARMS_BFG: return BATTLEGROUND_BFG;
        default: return BATTLEGROUND_TYPE_NONE;
    }
}

bool BattlegroundMgr::IsBGWeekend(BattlegroundTypeId bgTypeId)
{
    return IsHolidayActive(BGTypeToWeekendHolidayId(bgTypeId));
}

BattlegroundTypeId BattlegroundMgr::GetRandomBG(BattlegroundTypeId bgTypeId, BattlegroundTypeId currentRandomBg)
{
    uint32 weight = 0;
    BattlegroundTypeId returnBgTypeId = BATTLEGROUND_TYPE_NONE;
    BattlegroundSelectionWeightMap selectionWeights;

    if (bgTypeId == BATTLEGROUND_AA)
    {
        for (BattlegroundSelectionWeightMap::const_iterator it = m_ArenaSelectionWeights.begin(); it != m_ArenaSelectionWeights.end(); ++it)
        {
            if (it->second)
            {
                weight += it->second;
                selectionWeights[it->first] = it->second;
            }
        }
    }
    else if (bgTypeId == BATTLEGROUND_RB)
    {
        for (BattlegroundSelectionWeightMap::const_iterator it = m_BGSelectionWeights.begin(); it != m_BGSelectionWeights.end(); ++it)
        {
            if (it->second)
            {
                if (it->first == BATTLEGROUND_AV || it->first == BATTLEGROUND_IC)
                {
                    time_t now = time(NULL);
                    struct tm *now_tm = localtime(&now);
                    uint32 hour = static_cast<uint32>(now_tm->tm_hour);

                    if (hour < sWorld->getIntConfig(CONFIG_BATTLEGROUND_EXCEPTION_START) || hour >= sWorld->getIntConfig(CONFIG_BATTLEGROUND_EXCEPTION_END))
                        continue;
                }

                if (it->first == currentRandomBg)
                    continue;

                weight += it->second;
                selectionWeights[it->first] = it->second;
            }
        }
    }
    else if (bgTypeId == BATTLEGROUND_RATED_10_VS_10 || bgTypeId == BATTLEGROUND_RATED_15_VS_15 || bgTypeId == BATTLEGROUND_RATED_25_VS_25)
    {
        for (BattlegroundSelectionWeightMap::const_iterator it = m_RatedBGSelectionWeights.begin(); it != m_RatedBGSelectionWeights.end(); ++it)
        {
            if (it->second)
            {
                weight += it->second;
                selectionWeights[it->first] = it->second;
            }
        }
    }

    if (weight)
    {
        // Select a random value
        uint32 selectedWeight = urand(0, weight - 1);
        // Select the correct bg (if we have in DB A(10), B(20), C(10), D(15) --> [0---A---9|10---B---29|30---C---39|40---D---54])
        weight = 0;
        for (BattlegroundSelectionWeightMap::const_iterator it = selectionWeights.begin(); it != selectionWeights.end(); ++it)
        {
            weight += it->second;
            if (selectedWeight < weight)
            {
                returnBgTypeId = it->first;
                break;
            }
        }
    }

    return returnBgTypeId;
}

BGFreeSlotQueueContainer& BattlegroundMgr::GetBGFreeSlotQueueStore(BattlegroundTypeId bgTypeId)
{
    return bgDataStore[bgTypeId].BGFreeSlotQueue;
}

void BattlegroundMgr::AddToBGFreeSlotQueue(BattlegroundTypeId bgTypeId, Battleground* bg)
{
    bgDataStore[bgTypeId].BGFreeSlotQueue.push_front(bg);
}

void BattlegroundMgr::RemoveFromBGFreeSlotQueue(BattlegroundTypeId bgTypeId, uint32 instanceId)
{
    BGFreeSlotQueueContainer& queues = bgDataStore[bgTypeId].BGFreeSlotQueue;
    for (BGFreeSlotQueueContainer::iterator itr = queues.begin(); itr != queues.end(); ++itr)
        if ((*itr)->GetInstanceID() == instanceId)
        {
            queues.erase(itr);
            return;
        }
}

void BattlegroundMgr::AddBattleground(Battleground* bg)
{
    if (bg)
        bgDataStore[bg->GetTypeID()].m_Battlegrounds[bg->GetInstanceID()] = bg;
}

void BattlegroundMgr::RemoveBattleground(BattlegroundTypeId bgTypeId, uint32 instanceId)
{
    bgDataStore[bgTypeId].m_Battlegrounds.erase(instanceId);
}