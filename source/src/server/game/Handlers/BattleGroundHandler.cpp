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
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "ArenaTeamMgr.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include "ArenaTeam.h"
#include "BattlegroundMgr.h"
#include "Battleground.h"
#include "Chat.h"
#include "Language.h"
#include "Log.h"
#include "Player.h"
#include "Object.h"
#include "Opcodes.h"
#include "DisableMgr.h"
#include "Group.h"
#include "BattlegroundCrossFaction.h"
#include "InfoMgr.h"

void WorldSession::HandleBattlemasterHelloOpcode(WorldPacket& recvData)
{
    uint64 guid;
    recvData >> guid;
    TC_LOG_DEBUG("network.opcode", "WORLD: Recvd CMSG_BATTLEMASTER_HELLO Message from (GUID: %u TypeId:%u)", GUID_LOPART(guid), GuidHigh2TypeId(GUID_HIPART(guid)));

    Creature* unit = GetPlayer()->GetMap()->GetCreature(guid);
    if (!unit)
        return;

    if (!unit->isBattleMaster())                             // it's not battlemaster
        return;

    // Stop the npc if moving
    unit->StopMoving();

    BattlegroundTypeId bgTypeId = sBattlegroundMgr->GetBattleMasterBG(unit->GetEntry());

    if (!_player->GetBGAccessByLevel(bgTypeId))
    {
                                                            // temp, must be gossip message...
        SendNotification(LANG_YOUR_BG_LEVEL_REQ_ERROR);
        return;
    }

    SendBattleGroundList(guid, bgTypeId);
}

void WorldSession::SendBattleGroundList(uint64 guid, BattlegroundTypeId bgTypeId)
{
    WorldPacket data;
    sBattlegroundMgr->BuildBattlegroundListPacket(&data, guid, _player, bgTypeId);
    SendPacket(&data);
}

void WorldSession::HandleBattlemasterJoinOpcode(WorldPacket& recvData)
{
    uint32 bgTypeId_;
    uint32 instanceId;
    uint8 asGroup;
    bool isPremade = false;
    Group* grp = NULL;
    ObjectGuid guid;

    recvData >> instanceId;                 // Instance Id
    guid[2] = recvData.ReadBit();
    guid[0] = recvData.ReadBit();
    guid[3] = recvData.ReadBit();
    guid[1] = recvData.ReadBit();
    guid[5] = recvData.ReadBit();
    asGroup = recvData.ReadBit();           // As Group
    guid[4] = recvData.ReadBit();
    guid[6] = recvData.ReadBit();
    guid[7] = recvData.ReadBit();

    recvData.ReadByteSeq(guid[2]);
    recvData.ReadByteSeq(guid[6]);
    recvData.ReadByteSeq(guid[4]);
    recvData.ReadByteSeq(guid[3]);
    recvData.ReadByteSeq(guid[7]);
    recvData.ReadByteSeq(guid[0]);
    recvData.ReadByteSeq(guid[5]);
    recvData.ReadByteSeq(guid[1]);

    //extract from guid
    bgTypeId_ = GUID_LOPART(guid);

    if (!sBattlemasterListStore.LookupEntry(bgTypeId_))
    {
        TC_LOG_ERROR("network.opcode", "Battleground: invalid bgtype (%u) received. possible cheater? player guid %u", bgTypeId_, _player->GetGUIDLow());
        return;
    }

    if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, bgTypeId_, NULL))
    {
        ChatHandler(this).PSendSysMessage(LANG_BG_DISABLED);
        return;
    }
    BattlegroundTypeId bgTypeId = BattlegroundTypeId(bgTypeId_);

    //TC_LOG_DEBUG("network.opcode", "WORLD: Recvd CMSG_BATTLEMASTER_JOIN Message from (GUID:" UI64FMTD " TypeId:%u)", guid, bgTypeId_);

    // can do this, since it's battleground, not arena
    BattlegroundQueueTypeId bgQueueTypeId = BattlegroundMgr::BGQueueTypeId(bgTypeId, 0);
    BattlegroundQueueTypeId bgQueueTypeIdRandom = BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_RB, 0);

    // ignore if player is already in BG
    if (_player->InBattleground())
        return;

    // get bg instance or bg template if instance not found
    Battleground* bg = NULL;
    if (instanceId)
        bg = sBattlegroundMgr->GetBattlegroundThroughClientInstance(instanceId, bgTypeId);

    if (!bg)
        bg = sBattlegroundMgr->GetBattlegroundTemplate(bgTypeId);
    if (!bg)
        return;

    // expected bracket entry
    PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bg->GetMapId(), _player->getLevel());
    if (!bracketEntry)
        return;

    GroupJoinBattlegroundResult err = ERR_BATTLEGROUND_NONE;

    // check queue conditions
    if (!asGroup)
    {
        if (GetPlayer()->isUsingLfg())
        {
            WorldPacket data;
            sBattlegroundMgr->BuildStatusFailedPacket(&data, bg, _player, 0, ERR_LFG_CANT_USE_BATTLEGROUND);
            GetPlayer()->GetSession()->SendPacket(&data);
            return;
        }

        // check Deserter debuff
        if (!_player->CanJoinToBattleground(bg))
        {
            WorldPacket data;
            sBattlegroundMgr->BuildStatusFailedPacket(&data, bg, _player, 0, ERR_GROUP_JOIN_BATTLEGROUND_DESERTERS);
            _player->GetSession()->SendPacket(&data);
            return;
        }

        if (_player->GetBattlegroundQueueIndex(bgQueueTypeIdRandom) < PLAYER_MAX_BATTLEGROUND_QUEUES)
        {
            // player is already in random queue
            WorldPacket data;
            sBattlegroundMgr->BuildStatusFailedPacket(&data, bg, _player, 0, ERR_IN_RANDOM_BG);
            _player->GetSession()->SendPacket(&data);
            return;
        }

        if (_player->InBattlegroundQueue() && bgTypeId == BATTLEGROUND_RB)
        {
            // player is already in queue, can't start random queue
            WorldPacket data;
            sBattlegroundMgr->BuildStatusFailedPacket(&data, bg, _player, 0, ERR_IN_NON_RANDOM_BG);
            _player->GetSession()->SendPacket(&data);
            return;
        }

        // check if already in queue
        if (_player->GetBattlegroundQueueIndex(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES)
            // player is already in this queue
            return;

        // check if has free queue slots
        if (!_player->HasFreeBattlegroundQueueId())
        {
            WorldPacket data;
            sBattlegroundMgr->BuildStatusFailedPacket(&data, bg, _player, 0, ERR_BATTLEGROUND_TOO_MANY_QUEUES);
            _player->GetSession()->SendPacket(&data);
            return;
        }

        BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);
        GroupQueueInfo* ginfo = bgQueue.AddGroup(_player, NULL, bgTypeId, bracketEntry, 0, false, isPremade, 0, 0);

        uint32 avgTime = bgQueue.GetAverageQueueWaitTime(ginfo, bracketEntry->GetBracketId());
        uint32 queueSlot = _player->AddBattlegroundQueueId(bgQueueTypeId);

        // add joined time data
        _player->AddBattlegroundQueueJoinTime(bgTypeId, ginfo->JoinTime);

        WorldPacket data; // send status packet (in queue)
        sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, bg, _player, queueSlot, STATUS_WAIT_QUEUE, avgTime, ginfo->JoinTime, ginfo->ArenaType);
        SendPacket(&data);

        TC_LOG_DEBUG("bg.battleground", "Battleground: player joined queue for bg queue type %u bg type %u: GUID %u, NAME %s",
                       bgQueueTypeId, bgTypeId, _player->GetGUIDLow(), _player->GetName().c_str());
    }
    else
    {
        grp = _player->GetGroup();

        if (!grp)
            return;

        if (grp->GetLeaderGUID() != _player->GetGUID())
            return;

        err = grp->CanJoinBattlegroundQueue(bg, bgQueueTypeId, 0, bg->GetMaxPlayersPerTeam(), false, 0);
        isPremade = (grp->GetMembersCount() >= bg->GetMinPlayersPerTeam());

        BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);
        GroupQueueInfo* ginfo = NULL;
        uint32 avgTime = 0;

        if (!err)
        {
            TC_LOG_DEBUG("bg.battleground", "Battleground: the following players are joining as group:");
            ginfo = bgQueue.AddGroup(_player, grp, bgTypeId, bracketEntry, 0, false, isPremade, 0, 0);
            avgTime = bgQueue.GetAverageQueueWaitTime(ginfo, bracketEntry->GetBracketId());
        }

        for (GroupReference* itr = grp->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            Player* member = itr->getSource();
            if (!member)
                continue;   // this should never happen

            if (err)
            {
                WorldPacket data;
                sBattlegroundMgr->BuildStatusFailedPacket(&data, bg, _player, 0, err);
                member->GetSession()->SendPacket(&data);
                continue;
            }

            // add to queue
            uint32 queueSlot = member->AddBattlegroundQueueId(bgQueueTypeId);

            // add joined time data
            member->AddBattlegroundQueueJoinTime(bgTypeId, ginfo->JoinTime);

            WorldPacket data; // send status packet (in queue)
            sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, bg, member, queueSlot, STATUS_WAIT_QUEUE, avgTime, ginfo->JoinTime, ginfo->ArenaType);
            member->GetSession()->SendPacket(&data);
            TC_LOG_DEBUG("bg.battleground", "Battleground: player joined queue for bg queue type %u bg type %u: GUID %u, NAME %s",
                bgQueueTypeId, bgTypeId, member->GetGUIDLow(), member->GetName().c_str());
        }
        TC_LOG_DEBUG("bg.battleground", "Battleground: group end");
    }

    sBattlegroundMgr->ScheduleQueueUpdate(0, 0, bgQueueTypeId, bgTypeId, bracketEntry->GetBracketId());
}

void WorldSession::HandleBattlemasterJoinRatedOpcode(WorldPacket& /*recvData*/)
{
    Group* grp = NULL;
    uint32 matchmakerRating = 0;

    BattlegroundTypeId bgTypeId = BattlegroundTypeId(BATTLEGROUND_RATED_10_VS_10);
    BattlegroundQueueTypeId bgQueueTypeId = BattlegroundMgr::BGQueueTypeId(bgTypeId, 0);

    if (_player->InBattleground())
        return;

    Battleground* bg = sBattlegroundMgr->GetBattlegroundTemplate(bgTypeId);

    if (!bg)
        return;

    bg->SetRatedBattleground(true);

    // expected bracket entry
    PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bg->GetMapId(), _player->getLevel());
    if (!bracketEntry)
        return;

    GroupJoinBattlegroundResult err = ERR_BATTLEGROUND_NONE;

    grp = _player->GetGroup();

    if (!grp)
        return;

    if (grp->GetLeaderGUID() != _player->GetGUID())
        return;

    err = grp->CanJoinBattlegroundQueue(bg, bgQueueTypeId, 0, bg->GetMaxPlayersPerTeam(), true, 0);

    BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);
    GroupQueueInfo* ginfo = NULL;
    uint32 avgTime = 0;

    if (!err || sBattlegroundMgr->isTesting())
    {
        for (GroupReference* itr = grp->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            Player* member = itr->getSource();
            if (!member)
                continue;   // this should never happen

            InfoCharEntry info;
            if (sInfoMgr->GetCharInfo(GUID_LOPART(member->GetGUID()), info))
            {
                if (!info.MMR[3])
                {
                    matchmakerRating += 1500;
                    sInfoMgr->UpdateCharMMR(info.Guid, 3, 1500);
                    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_CHARACTER_PVP_STATS);
                    stmt->setUInt32(0, GUID_LOPART(info.Guid));
                    stmt->setUInt8(1, 3);
                    stmt->setUInt16(2, 1500);
                    CharacterDatabase.Execute(stmt);
                }
                else
                    matchmakerRating += info.MMR[3];
            }
            else
                matchmakerRating += 1500;
        }
        matchmakerRating /= 10;
        TC_LOG_DEBUG("bg.battleground", "Battleground: the following players are joining as group on Rated Battleground:");
        ginfo = bgQueue.AddGroup(_player, grp, bgTypeId, bracketEntry, 0, true, false, 0, matchmakerRating, _player->GetGUIDLow());
        avgTime = bgQueue.GetAverageQueueWaitTime(ginfo, bracketEntry->GetBracketId());
    }

    for (GroupReference* itr = grp->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* member = itr->getSource();
        if (!member)
            continue;   // this should never happen

        if (err)
        {
            WorldPacket data;
            sBattlegroundMgr->BuildStatusFailedPacket(&data, bg, _player, 0, err);
            member->GetSession()->SendPacket(&data);
            continue;
        }

        // add to queue
        uint32 queueSlot = member->AddBattlegroundQueueId(bgQueueTypeId);

        // add joined time data
        member->AddBattlegroundQueueJoinTime(bgTypeId, ginfo->JoinTime);

        WorldPacket data; // send status packet (in queue)
        sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, bg, member, queueSlot, STATUS_WAIT_QUEUE, avgTime, ginfo->JoinTime, ginfo->ArenaType);
        member->GetSession()->SendPacket(&data);
        TC_LOG_DEBUG("bg.battleground", "Battleground: player joined queue for rated bg queue type %u bg type %u: GUID %u, NAME %s",
                       bgQueueTypeId, bgTypeId, member->GetGUIDLow(), member->GetName().c_str());
    }
    TC_LOG_DEBUG("bg.battleground", "Battleground: group end");

    sBattlegroundMgr->ScheduleQueueUpdate(0, 0, bgQueueTypeId, bgTypeId, bracketEntry->GetBracketId());
}

void WorldSession::HandleBattlegroundPlayerPositionsOpcode(WorldPacket& /*recvData*/)
{
    TC_LOG_DEBUG("network.opcode", "WORLD: Recvd CMSG_BATTLEGROUND_PLAYER_POSITIONS Message");

    Battleground* bg = _player->GetBattleground();
    if (!bg)                                                 // can't be received if player not in battleground
        return;

    uint32 playerCount = 0;
    Player* aplr = NULL;
    Player* hplr = NULL;

    if (uint64 guid = bg->GetFlagPickerGUID(TEAM_ALLIANCE))
    {
        aplr = ObjectAccessor::FindPlayer(guid);
        if (aplr)
            ++playerCount;
    }

    if (uint64 guid = bg->GetFlagPickerGUID(TEAM_HORDE))
    {
        hplr = ObjectAccessor::FindPlayer(guid);
        if (hplr)
            ++playerCount;
    }

    ObjectGuid aguid = aplr ? aplr->GetGUID() : 0;
    ObjectGuid hguid = hplr ? hplr->GetGUID() : 0;
    WorldPacket data(SMSG_BATTLEFIELD_PLAYER_POSITIONS);

    data.WriteBits(0, 22); // Always 0 in sniffs
    data.WriteBits(playerCount, 22);

    if (aplr)
    {
        data.WriteBit(aguid[6]);
        data.WriteBit(aguid[5]);
        data.WriteBit(aguid[4]);
        data.WriteBit(aguid[7]);
        data.WriteBit(aguid[2]);
        data.WriteBit(aguid[1]);
        data.WriteBit(aguid[0]);
        data.WriteBit(aguid[3]);
    }

    if (hplr)
    {
        data.WriteBit(hguid[6]);
        data.WriteBit(hguid[5]);
        data.WriteBit(hguid[4]);
        data.WriteBit(hguid[7]);
        data.WriteBit(hguid[2]);
        data.WriteBit(hguid[1]);
        data.WriteBit(hguid[0]);
        data.WriteBit(hguid[3]);
    }

    data.FlushBits();

    if (aplr)
    {
        data.WriteByteSeq(aguid[2]);
        data.WriteByteSeq(aguid[1]);
        data << float(aplr->GetPositionY());
        data.WriteByteSeq(aguid[5]);
        data.WriteByteSeq(aguid[4]);
        data.WriteByteSeq(aguid[7]);
        data.WriteByteSeq(aguid[0]);
        data.WriteByteSeq(aguid[6]);
        data.WriteByteSeq(aguid[3]);
        data << float(aplr->GetPositionX());
    }

    if (hplr)
    {
        data.WriteByteSeq(hguid[2]);
        data.WriteByteSeq(hguid[1]);
        data << float(hplr->GetPositionY());
        data.WriteByteSeq(hguid[5]);
        data.WriteByteSeq(hguid[4]);
        data.WriteByteSeq(hguid[7]);
        data.WriteByteSeq(hguid[0]);
        data.WriteByteSeq(hguid[6]);
        data.WriteByteSeq(hguid[3]);
        data << float(hplr->GetPositionX());
    }

    SendPacket(&data);
}

void WorldSession::HandlePVPLogDataOpcode(WorldPacket & /*recvData*/)
{
    TC_LOG_DEBUG("network.opcode", "WORLD: Recvd CMSG_PVP_LOG_DATA Message");

    Battleground* bg = NULL;
    if ((_player->isGameMaster()) || (!_player->isGMVisible()))//This is a gamemaster asking for a scoreboard
    {
        Player* target = _player->GetSelectedPlayer();
        if (target)
        {
            bg = target->GetBattleground();
        }
    }
    else
    {
        bg = _player->GetBattleground();
    }
    if (!bg)
        return;

    // Prevent players from sending BuildPvpLogDataPacket in an arena except for when sent in BattleGround::EndBattleGround.
    if (bg->isArena())
        return;

    WorldPacket data;
    sBattlegroundMgr->BuildPvpLogDataPacket(&data, bg);
    SendPacket(&data);

    TC_LOG_DEBUG("network.opcode", "WORLD: Sent SMSG_PVP_LOG_DATA Message");
}

void WorldSession::HandleBattlefieldListOpcode(WorldPacket& recvData)
{
    TC_LOG_DEBUG("network.opcode", "WORLD: Recvd CMSG_BATTLEFIELD_LIST Message");

    uint32 bgTypeId;
    recvData >> bgTypeId;                                  // id from DBC

    BattlemasterListEntry const* bl = sBattlemasterListStore.LookupEntry(bgTypeId);
    if (!bl)
    {
        TC_LOG_DEBUG("bg.battleground", "BattlegroundHandler: invalid bgtype (%u) with player (Name: %s, GUID: %u) received.", bgTypeId, _player->GetName().c_str(), _player->GetGUIDLow());
        return;
    }

    WorldPacket data;
    sBattlegroundMgr->BuildBattlegroundListPacket(&data, 0, _player, BattlegroundTypeId(bgTypeId));
    SendPacket(&data);
    recvData.rfinish();
}

void WorldSession::HandleBattleFieldPortOpcode(WorldPacket &recvData)
{
    TC_LOG_DEBUG("network.opcode", "WORLD: Recvd CMSG_BATTLEFIELD_PORT Message");

    uint32 time;
    uint32 queueSlot;
    uint32 bgtype;
    uint8 action;                       // enter battle 0x1, leave queue 0x0
    ObjectGuid guid;

    recvData >> time;
    recvData >> queueSlot;
    recvData >> bgtype;

    guid[0] = recvData.ReadBit();
    guid[1] = recvData.ReadBit();
    guid[5] = recvData.ReadBit();
    guid[6] = recvData.ReadBit();
    guid[7] = recvData.ReadBit();
    guid[4] = recvData.ReadBit();
    guid[3] = recvData.ReadBit();
    guid[2] = recvData.ReadBit();

    action = recvData.ReadBit();

    recvData.ReadByteSeq(guid[1]);
    recvData.ReadByteSeq(guid[3]);
    recvData.ReadByteSeq(guid[5]);
    recvData.ReadByteSeq(guid[7]);
    recvData.ReadByteSeq(guid[0]);
    recvData.ReadByteSeq(guid[2]);
    recvData.ReadByteSeq(guid[6]);
    recvData.ReadByteSeq(guid[4]);

    if (!_player->InBattlegroundQueue())
    {
        TC_LOG_DEBUG("bg.battleground", "CMSG_BATTLEFIELD_PORT %s Slot: %u, Unk: %u, Time: %u, Action: %u. Player not in queue!",
            GetPlayerInfo().c_str(), queueSlot, bgtype, time, action);
        recvData.rfinish();
        return;
    }

    BattlegroundQueueTypeId bgQueueTypeId = _player->GetBattlegroundQueueTypeId(queueSlot);
    if (bgQueueTypeId == BATTLEGROUND_QUEUE_NONE)
    {
        TC_LOG_DEBUG("bg.battleground", "CMSG_BATTLEFIELD_PORT %s Slot: %u, Unk: %u, Time: %u, Action: %u. Invalid queueSlot!",
            GetPlayerInfo().c_str(), queueSlot, bgtype, time, action);
        recvData.rfinish();
        return;
    }

    BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);

    //we must use temporary variable, because GroupQueueInfo pointer can be deleted in BattlegroundQueue::RemovePlayer() function
    GroupQueueInfo ginfo;

    Battleground* bg;
    BattlegroundTypeId bgTypeId;

    // WarGame
    WarGameGroupQueueInfo wginfo;
    uint8 _team = 0;
    uint16 team = 0;
    bool isWarGame = false;
    if (bgQueue.GetInvitedPlayerWarGameGroupInfoData(_player->GetGUID(), wginfo, _team))
    {
        isWarGame = true;
        team = _team == 0 ? 67 : 469;

        if (!wginfo.IsInvitedToBGInstanceGUID && action == 1)
        {
            TC_LOG_DEBUG("bg.battleground", "CMSG_BATTLEFIELD_PORT %s Slot: %u, Unk: %u, Time: %u, Action: %u. Player is not invited to any bg!",
                GetPlayerInfo().c_str(), queueSlot, bgtype, time, action);
            recvData.rfinish();
            return;
        }

        bgTypeId = BattlegroundMgr::BGTemplateId(bgQueueTypeId);

        bg = sBattlegroundMgr->GetBattleground(wginfo.IsInvitedToBGInstanceGUID, bgTypeId == BATTLEGROUND_AA ? BATTLEGROUND_TYPE_NONE : bgTypeId);
        if (!bg)
        {
            if (action)
            {
                TC_LOG_DEBUG("bg.battleground", "CMSG_BATTLEFIELD_PORT %s Slot: %u, Unk: %u, Time: %u, Action: %u. Cant find BG with id %u!",
                    GetPlayerInfo().c_str(), queueSlot, bgtype, time, action, wginfo.IsInvitedToBGInstanceGUID);
                recvData.rfinish();
                return;
            }

            bg = sBattlegroundMgr->GetBattlegroundTemplate(bgTypeId);
            if (!bg)
            {
                TC_LOG_ERROR("network.opcode", "BattlegroundHandler: bg_template not found for type id %u.", bgTypeId);
                recvData.rfinish();
                return;
            }
        }

        TC_LOG_DEBUG("bg.battleground", "CMSG_BATTLEFIELD_PORT %s Slot: %u, Unk: %u, Time: %u, Action: %u.",
            GetPlayerInfo().c_str(), queueSlot, bgtype, time, action);

        // get real bg type
        bgTypeId = bg->GetTypeID();

        // expected bracket entry
        PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bg->GetMapId(), _player->getLevel());
        if (!bracketEntry)
        {
            recvData.rfinish();
            return;
        }

        //some checks if player isn't cheating - it is not exactly cheating, but we cannot allow it
        if (action == 1)
        {
            //if player don't match battleground max level, then do not allow him to enter! (this might happen when player leveled up during his waiting in queue
            if (_player->getLevel() > bg->GetMaxLevel())
            {
                TC_LOG_DEBUG("network.opcode", "Player %s (%u) has level (%u) higher than maxlevel (%u) of battleground (%u)! Do not port him to battleground!",
                    _player->GetName().c_str(), _player->GetGUIDLow(), _player->getLevel(), bg->GetMaxLevel(), bg->GetTypeID());
                action = 0;
            }
        }

        recvData.rfinish();
        goto bottom;
    }

    if (!bgQueue.GetPlayerGroupInfoData(_player->GetGUID(), &ginfo))
    {
        TC_LOG_DEBUG("bg.battleground", "CMSG_BATTLEFIELD_PORT %s Slot: %u, Unk: %u, Time: %u, Action: %u. Player not in queue (No player Group Info)!",
            GetPlayerInfo().c_str(), queueSlot, bgtype, time, action);

        recvData.rfinish();
        return;
    }
    // if action == 1, then instanceId is required
    if (!ginfo.IsInvitedToBGInstanceGUID && action == 1)
    {
        TC_LOG_DEBUG("bg.battleground", "CMSG_BATTLEFIELD_PORT %s Slot: %u, Unk: %u, Time: %u, Action: %u. Player is not invited to any bg!",
            GetPlayerInfo().c_str(), queueSlot, bgtype, time, action);

        recvData.rfinish();
        return;
    }

    bgTypeId = BattlegroundMgr::BGTemplateId(bgQueueTypeId);
    // BGTemplateId returns BATTLEGROUND_AA when it is arena queue.
    // Do instance id search as there is no AA bg instances.
    bg = sBattlegroundMgr->GetBattleground(ginfo.IsInvitedToBGInstanceGUID, bgTypeId == BATTLEGROUND_AA ? BATTLEGROUND_TYPE_NONE : bgTypeId);
    if (!bg)
    {
        if (action)
        {
            TC_LOG_DEBUG("bg.battleground", "CMSG_BATTLEFIELD_PORT %s Slot: %u, Unk: %u, Time: %u, Action: %u. Cant find BG with id %u!",
                GetPlayerInfo().c_str(), queueSlot, bgtype, time, action, ginfo.IsInvitedToBGInstanceGUID);

            recvData.rfinish();
            return;
        }

        bg = sBattlegroundMgr->GetBattlegroundTemplate(bgTypeId);
        if (!bg)
        {
            TC_LOG_ERROR("network.opcode", "BattlegroundHandler: bg_template not found for type id %u.", bgTypeId);
            recvData.rfinish();
            return;
        }
    }

    TC_LOG_DEBUG("bg.battleground", "CMSG_BATTLEFIELD_PORT %s Slot: %u, Unk: %u, Time: %u, Action: %u.",
        GetPlayerInfo().c_str(), queueSlot, bgtype, time, action);

    // get real bg type
    bgTypeId = bg->GetTypeID();

    // expected bracket entry
    PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bg->GetMapId(), _player->getLevel());
    if (!bracketEntry)
    {

        recvData.rfinish();
        return;
    }

    //some checks if player isn't cheating - it is not exactly cheating, but we cannot allow it
    if (action == 1 && ginfo.ArenaType == 0)
    {
        //if player is trying to enter battleground (not arena!) and he has deserter debuff, we must just remove him from queue
        if (!_player->CanJoinToBattleground(bg))
        {
            //send bg command result to show nice message
            WorldPacket data2;
            sBattlegroundMgr->BuildStatusFailedPacket(&data2, bg, _player, 0, ERR_GROUP_JOIN_BATTLEGROUND_DESERTERS);
            _player->GetSession()->SendPacket(&data2);
            action = 0;
            TC_LOG_DEBUG("bg.battleground", "Player %s (%u) has a deserter debuff, do not port him to battleground!", _player->GetName().c_str(), _player->GetGUIDLow());
        }
        //if player don't match battleground max level, then do not allow him to enter! (this might happen when player leveled up during his waiting in queue
        if (_player->getLevel() > bg->GetMaxLevel())
        {
            TC_LOG_DEBUG("network.opcode", "Player %s (%u) has level (%u) higher than maxlevel (%u) of battleground (%u)! Do not port him to battleground!",
                _player->GetName().c_str(), _player->GetGUIDLow(), _player->getLevel(), bg->GetMaxLevel(), bg->GetTypeID());
            action = 0;
        }
    }

    bottom:
    WorldPacket data;
    if (action)
    {
        if (!_player->IsInvitedForBattlegroundQueueType(bgQueueTypeId))
        {
            recvData.rfinish();
            return;
        }                             // cheating?

        if (!_player->InBattleground())
            _player->SetBattlegroundEntryPoint();

        // resurrect the player
        if (!_player->isAlive())
        {
            _player->ResurrectPlayer(1.0f);
            _player->SpawnCorpseBones();
        }
        // stop taxi flight at port
        if (_player->isInFlight())
        {
            _player->GetMotionMaster()->MovementExpired();
            _player->CleanupAfterTaxiFlight();
        }

        sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, bg, _player, queueSlot, STATUS_IN_PROGRESS, _player->GetBattlegroundQueueJoinTime(bgTypeId), bg->GetElapsedTime(), bg->GetArenaType());
        _player->GetSession()->SendPacket(&data);

        // remove battleground queue status from BGmgr
        bgQueue.RemovePlayer(_player->GetGUID(), false);
        // this is still needed here if battleground "jumping" shouldn't add deserter debuff
        // also this is required to prevent stuck at old battleground after SetBattlegroundId set to new
        if (Battleground* currentBg = _player->GetBattleground())
            currentBg->RemovePlayerAtLeave(_player->GetGUID(), false, true);

        // set the destination instance id
        _player->SetBattlegroundId(bg->GetInstanceID(), bgTypeId);
        // set the destination team
        if (isWarGame)
        {
            _player->SetBGTeam(team);
            sBattlegroundMgr->SendToBattleground(_player, wginfo.IsInvitedToBGInstanceGUID, bgTypeId);
        }
        else
        {
            _player->SetBGTeam(ginfo.Team);
            // bg->HandleBeforeTeleportToBattleground(_player);
            sBattlegroundMgr->SendToBattleground(_player, ginfo.IsInvitedToBGInstanceGUID, bgTypeId);
        }
        // add only in HandleMoveWorldPortAck()
        // bg->AddPlayer(_player, team);
        TC_LOG_DEBUG("bg.battleground", "Battleground: player %s (%u) joined battle for bg %u, bgtype %u, queue type %u.", _player->GetName().c_str(), _player->GetGUIDLow(), bg->GetInstanceID(), bg->GetTypeID(), bgQueueTypeId);
    }
    else // leave queue
    {
        if (isWarGame)
        {
            sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, bg, _player, queueSlot, STATUS_NONE, _player->GetBattlegroundQueueJoinTime(bgTypeId), 0,bg->GetArenaType());
            SendPacket(&data);

            _player->RemoveBattlegroundQueueId(bgQueueTypeId);  // must be called this way, because if you move this call to queue->removeplayer, it causes bugs
            bgQueue.RemovePlayer(_player->GetGUID(), true);

            TC_LOG_DEBUG("bg.battleground", "Battleground: player %s (%u) left queue for bgtype %u, queue type %u.", _player->GetName().c_str(), _player->GetGUIDLow(), bg->GetTypeID(), bgQueueTypeId);

            recvData.rfinish();
            return;
        }

        // if player leaves rated arena match before match start, it is counted as he played but he lost
        if (ginfo.IsRated && ginfo.IsInvitedToBGInstanceGUID)
        {
            uint32 teamId = ginfo.isSoloQueueGroup ? _player->GetArenaTeamIdFromDB(_player->GetGUID(), ARENA_TEAM_5v5) : ginfo.ArenaTeamIdOrRbgLeaderGuid;
            ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(teamId);
            if (at)
            {
                TC_LOG_DEBUG("bg.battleground", "UPDATING memberLost's personal arena rating for %u by opponents rating: %u, because he has left queue!", GUID_LOPART(_player->GetGUID()), ginfo.OpponentsTeamRating);
                
                at->MemberLost(_player, ginfo.OpponentsMatchmakerRating);
                at->SaveToDB();
            }

            if (ginfo.isSoloQueueGroup)
                _player->CastSpell(_player, 26013, true); // Deserter
        }
        sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, bg, _player, queueSlot, STATUS_NONE, _player->GetBattlegroundQueueJoinTime(bgTypeId), 0, ginfo.ArenaType);
        SendPacket(&data);

        _player->RemoveBattlegroundQueueId(bgQueueTypeId);  // must be called this way, because if you move this call to queue->removeplayer, it causes bugs
        bgQueue.RemovePlayer(_player->GetGUID(), true);
        // player left queue, we should update it - do not update Arena Queue
        if (!ginfo.ArenaType)
            sBattlegroundMgr->ScheduleQueueUpdate(ginfo.ArenaMatchmakerRating, ginfo.ArenaType, bgQueueTypeId, bgTypeId, bracketEntry->GetBracketId());

        TC_LOG_DEBUG("bg.battleground", "Battleground: player %s (%u) left queue for bgtype %u, queue type %u.", _player->GetName().c_str(), _player->GetGUIDLow(), bg->GetTypeID(), bgQueueTypeId);
    }

    recvData.rfinish();
    return;
}

void WorldSession::HandleBattlefieldLeaveOpcode(WorldPacket& /*recvData*/)
{
    if (_player)
    {
        if (_player->IsSpectator())
        {
            if (auto bg = _player->GetBattleground())
                bg->RemoveSpectator(_player->GetGUID());
            if (!_player->isSpectateCanceled())
                _player->CancelSpectate();

            _player->TeleportToBGEntryPoint();
        }
        else
        {
            TC_LOG_DEBUG("network.opcode", "WORLD: Recvd CMSG_BATTLEFIELD_LEAVE Message");
            _player->LeaveBattleground();
        }
    }
}

void WorldSession::HandleBattlefieldStatusOpcode(WorldPacket & recvData)
{
    // empty opcode
    TC_LOG_DEBUG("network.opcode", "WORLD: Recvd CMSG_BATTLEFIELD_STATUS Message");

    WorldPacket data;
    // we must update all queues here
    Battleground* bg = NULL;
    for (uint8 i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
    {
        BattlegroundQueueTypeId bgQueueTypeId = _player->GetBattlegroundQueueTypeId(i);
        if (!bgQueueTypeId)
            continue;
        BattlegroundTypeId bgTypeId = BattlegroundMgr::BGTemplateId(bgQueueTypeId);
        uint8 arenaType = BattlegroundMgr::BGArenaType(bgQueueTypeId);
        if (bgTypeId == _player->GetBattlegroundTypeId())
        {
            bg = _player->GetBattleground();
            //i cannot check any variable from player class because player class doesn't know if player is in 2v2 / 3v3 or 5v5 arena
            //so i must use bg pointer to get that information
            if (bg && bg->GetArenaType() == arenaType)
            {
                // this line is checked, i only don't know if GetElapsedTime() is changing itself after bg end!
                // send status in Battleground
                sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, bg, GetPlayer(), i, STATUS_IN_PROGRESS, _player->GetBattlegroundQueueJoinTime(bgTypeId), bg->GetElapsedTime(), arenaType);
                SendPacket(&data);
                continue;
            }
        }

        //we are sending update to player about queue - he can be invited there!
        //get GroupQueueInfo for queue status
        BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);
        GroupQueueInfo ginfo;
        if (!bgQueue.GetPlayerGroupInfoData(_player->GetGUID(), &ginfo))
            continue;
        if (ginfo.IsInvitedToBGInstanceGUID)
        {
            bg = sBattlegroundMgr->GetBattleground(ginfo.IsInvitedToBGInstanceGUID, bgTypeId);
            if (!bg)
                continue;

            // send status invited to Battleground
            sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, bg, GetPlayer(), i, STATUS_WAIT_JOIN, getMSTimeDiff(getMSTime(), ginfo.RemoveInviteTime), _player->GetBattlegroundQueueJoinTime(bgTypeId), arenaType);
            SendPacket(&data);
        }
        else
        {
            bg = sBattlegroundMgr->GetBattlegroundTemplate(bgTypeId);
            if (!bg)
                continue;

            // expected bracket entry
            PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bg->GetMapId(), _player->getLevel());
            if (!bracketEntry)
                continue;

            uint32 avgTime = bgQueue.GetAverageQueueWaitTime(&ginfo, bracketEntry->GetBracketId());
            // send status in Battleground Queue
            sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, bg, GetPlayer(), i, STATUS_WAIT_QUEUE, avgTime, _player->GetBattlegroundQueueJoinTime(bgTypeId), arenaType);
            SendPacket(&data);
        }
    }


    recvData.rfinish();                     // prevent warnings spam
    return;
}

void WorldSession::HandleBattlemasterJoinArena(WorldPacket& recvData)
{
    TC_LOG_DEBUG("network.opcode", "WORLD: CMSG_BATTLEMASTER_JOIN_ARENA");

    uint8 arenaslot;                                        // 2v2, 3v3 or 5v5

    recvData >> arenaslot;

    // ignore if we already in BG or BG queue
    if (_player->InBattleground())
        return;

    uint32 arenaRating = 0;
    uint32 matchmakerRating = 0;

    uint8 arenatype = ArenaTeam::GetTypeBySlot(arenaslot);

    //check existance
    Battleground* bg = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA);
    if (!bg)
    {
        TC_LOG_ERROR("network.opcode", "Battleground: template bg (all arenas) not found");
        return;
    }

    if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, BATTLEGROUND_AA, NULL))
    {
        ChatHandler(this).PSendSysMessage(LANG_ARENA_DISABLED);
        return;
    }

    BattlegroundTypeId bgTypeId = bg->GetTypeID();
    BattlegroundQueueTypeId bgQueueTypeId = BattlegroundMgr::BGQueueTypeId(bgTypeId, arenatype);
    PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bg->GetMapId(), _player->getLevel());
    if (!bracketEntry)
        return;

    GroupJoinBattlegroundResult err = ERR_BATTLEGROUND_NONE;

    Group* grp = _player->GetGroup();
    // no group found, error
    if (!grp)
        return;
    if (grp->GetLeaderGUID() != _player->GetGUID())
        return;

    uint32 ateamId = _player->GetArenaTeamId(arenaslot);
    // check real arenateam existence only here (if it was moved to group->CanJoin .. () then we would ahve to get it twice)
    ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(ateamId);
    if (!at)
    {
        _player->GetSession()->SendNotInArenaTeamPacket(arenatype);
        return;
    }

    // get the team rating for queueing
    arenaRating = at->GetRating();
    matchmakerRating = at->GetAverageMMR(grp);
    // the arenateam id must match for everyone in the group

    if (arenaRating <= 0)
        arenaRating = 1;

    BattlegroundQueue &bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);

    uint32 avgTime = 0;
    GroupQueueInfo* ginfo = NULL;

    err = grp->CanJoinBattlegroundQueue(bg, bgQueueTypeId, arenatype, arenatype, true, arenaslot);
    if (!err)
    {
        TC_LOG_DEBUG("bg.battleground", "Battleground: arena team id %u, leader %s queued with matchmaker rating %u for type %u", _player->GetArenaTeamId(arenaslot), _player->GetName().c_str(), matchmakerRating, arenatype);

        ginfo = bgQueue.AddGroup(_player, grp, bgTypeId, bracketEntry, arenatype, true, false, arenaRating, matchmakerRating, ateamId);
        avgTime = bgQueue.GetAverageQueueWaitTime(ginfo, bracketEntry->GetBracketId());
    }

    for (GroupReference* itr = grp->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* member = itr->getSource();
        if (!member)
            continue;

        if (err)
        {
            WorldPacket data;
            sBattlegroundMgr->BuildStatusFailedPacket(&data, bg, _player, 0, err);
            member->GetSession()->SendPacket(&data);
            continue;
        }

        // add to queue
        uint32 queueSlot = member->AddBattlegroundQueueId(bgQueueTypeId);

        // add joined time data
        member->AddBattlegroundQueueJoinTime(bgTypeId, ginfo->JoinTime);

        WorldPacket data; // send status packet (in queue)
        sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, bg, member, queueSlot, STATUS_WAIT_QUEUE, avgTime, ginfo->JoinTime, arenatype);
        member->GetSession()->SendPacket(&data);

        TC_LOG_DEBUG("bg.battleground", "Battleground: player joined queue for arena as group bg queue type %u bg type %u: GUID %u, NAME %s", bgQueueTypeId, bgTypeId, member->GetGUIDLow(), member->GetName().c_str());
    }

    sBattlegroundMgr->ScheduleQueueUpdate(matchmakerRating, arenatype, bgQueueTypeId, bgTypeId, bracketEntry->GetBracketId());
}

void WorldSession::HandleReportPvPAFK(WorldPacket& recvData)
{
    uint64 playerGuid;
    recvData >> playerGuid;
    Player* reportedPlayer = ObjectAccessor::FindPlayer(playerGuid);

    if (!reportedPlayer)
    {
        TC_LOG_DEBUG("bg.battleground", "WorldSession::HandleReportPvPAFK: player not found");
        return;
    }

    TC_LOG_DEBUG("bg.battleground", "WorldSession::HandleReportPvPAFK: %s reported %s", _player->GetName().c_str(), reportedPlayer->GetName().c_str());

    reportedPlayer->ReportedAfkBy(_player);
}

void WorldSession::HandleRequestRatedBgInfo(WorldPacket & recvData)
{
    TC_LOG_DEBUG("network.opcode", "WORLD: CMSG_REQUEST_RATED_BG_INFO");

    InfoCharEntry info;
    if (!sInfoMgr->GetCharInfo(GUID_LOPART(_player->GetGUID()), info))
        return;

    if (!info.RbgPlayed)
        sInfoMgr->UpdateCharRBGstats(info.Guid, 0, 0, 0);

    uint8 unk;
    recvData >> unk; // BG type ?

    TC_LOG_DEBUG("bg.battleground", "WorldSession::HandleRequestRatedBgInfo: unk = %u", unk);

    WorldPacket data(SMSG_BATTLEFIELD_RATED_INFO, 29);
    data << uint32(400);  // Reward
    data << uint8(unk);   // BG type (10vs10 - 15vs15) ?
    data << uint32(info.RBGRating);  // Rating
    data << uint32(0);  // unk
    data << _player->GetCurrencyWeekCap(CURRENCY_TYPE_CONQUEST_META_RBG, true);
    data << uint32(0);  // unk
    data << uint32(0);  // unk
    data << _player->GetCurrency(CURRENCY_TYPE_CONQUEST_POINTS, true);

    SendPacket(&data);
}

void WorldSession::HandleRequestPvpOptions(WorldPacket& /*recvData*/)
{
    TC_LOG_DEBUG("network.opcode", "WORLD: CMSG_REQUEST_PVP_OPTIONS_ENABLED");

    /// @Todo: perfome research in this case
    WorldPacket data(SMSG_PVP_OPTIONS_ENABLED, 1);
    data.WriteBit(1);
    data.WriteBit(1);       // WargamesEnabled
    data.WriteBit(1);
    data.WriteBit(1);       // RatedBGsEnabled
    data.WriteBit(1);       // RatedArenasEnabled

    data.FlushBits();

    SendPacket(&data);
}

void WorldSession::HandleRequestPvpReward(WorldPacket& /*recvData*/)
{
    TC_LOG_DEBUG("network.opcode", "WORLD: CMSG_REQUEST_PVP_REWARDS");

    _player->SendPvpRewards();
}

void WorldSession::HandleRequestRatedBgStats(WorldPacket& /*recvData*/)
{
    TC_LOG_DEBUG("network.opcode", "WORLD: CMSG_REQUEST_RATED_BG_STATS");

    InfoCharEntry info;
    if (!sInfoMgr->GetCharInfo(GUID_LOPART(_player->GetGUID()), info))
        return;

    if (!info.RbgPlayed)
        sInfoMgr->UpdateCharRBGstats(info.Guid, 0, 0, 0);

    /// There is 9 maps for RBG and 18 fiels so 2 fiels per map (1 for wins - 1 for played)
    /// Or maybe it's anything else ?
    WorldPacket data(SMSG_RATED_BG_STATS, 72);
    data << uint32(0);              // BgSeasonWins20vs20
    data << uint32(0);              // BgSeasonPlayed20vs20
    data << uint32(0);              // BgSeasonPlayed15vs15
    data << uint32(0);
    data << uint32(info.RbgWon);    // BgSeasonWins10vs10
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);              // BgSeasonWins15vs15
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    data << uint32(info.RbgPlayed); // BgSeasonPlayed10vs10
    data << uint32(0);
    data << uint32(0);


    SendPacket(&data);
}

void WorldSession::HandleRequestInspectRatedBgStats(WorldPacket& recvData)
{
    ObjectGuid guid;
    guid[1] = recvData.ReadBit();
    guid[4] = recvData.ReadBit();
    guid[6] = recvData.ReadBit();
    guid[5] = recvData.ReadBit();
    guid[0] = recvData.ReadBit();
    guid[2] = recvData.ReadBit();
    guid[7] = recvData.ReadBit();
    guid[3] = recvData.ReadBit();

    recvData.ReadByteSeq(guid[4]);
    recvData.ReadByteSeq(guid[7]);
    recvData.ReadByteSeq(guid[2]);
    recvData.ReadByteSeq(guid[5]);
    recvData.ReadByteSeq(guid[6]);
    recvData.ReadByteSeq(guid[3]);
    recvData.ReadByteSeq(guid[0]);
    recvData.ReadByteSeq(guid[1]);
    Player* player = ObjectAccessor::FindPlayer(guid);

    if (!player)
    {
        TC_LOG_DEBUG("network.opcode", "CMSG_REQUEST_INSPECT_RATED_BG_STATS: No player found from GUID: " UI64FMTD, (uint64)guid);
        return;
    }

    InfoCharEntry info;
    if (!sInfoMgr->GetCharInfo(GUID_LOPART(player->GetGUID()), info))
        return;

    if (!info.RbgPlayed)
        sInfoMgr->UpdateCharRBGstats(info.Guid, 0, 0, 0);

    ObjectGuid playerGuid = player->GetGUID();
    WorldPacket data(SMSG_INSPECT_RATED_BG_STATS, 8 + 4 + 4 + 4);
    data.WriteBit(playerGuid[6]);
    data.WriteBit(playerGuid[4]);
    data.WriteBit(playerGuid[5]);
    data.WriteBit(playerGuid[1]);
    data.WriteBit(playerGuid[2]);
    data.WriteBit(playerGuid[7]);
    data.WriteBit(playerGuid[0]);
    data.WriteBit(playerGuid[3]);
    data.WriteByteSeq(playerGuid[4]);
    data << uint32(info.RBGRating); // Rating
    data.WriteByteSeq(playerGuid[1]);
    data.WriteByteSeq(playerGuid[7]);
    data.WriteByteSeq(playerGuid[3]);
    data.WriteByteSeq(playerGuid[6]);
    data << uint32(info.RbgWon); // Season Won
    data << uint32(info.RbgPlayed); // Season Played
    data.WriteByteSeq(playerGuid[2]);
    data.WriteByteSeq(playerGuid[5]);
    data.WriteByteSeq(playerGuid[0]);
    SendPacket(&data);
}
