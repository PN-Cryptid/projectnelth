/*
 * Copyright (C) 2008-2013 TrinityCore <http://www.trinitycore.org/>
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

/*
 * Interaction between core and LFGScripts
 */

#include "Common.h"
#include "SharedDefines.h"
#include "Player.h"
#include "Group.h"
#include "LFGScripts.h"
#include "LFGMgr.h"
#include "ScriptMgr.h"
#include "ObjectAccessor.h"
#include "WorldSession.h"
#include "InfoMgr.h"

namespace lfg
{

LFGPlayerScript::LFGPlayerScript() : PlayerScript("LFGPlayerScript")
{
}

void LFGPlayerScript::OnLevelChanged(Player* player, uint8 /*oldLevel*/)
{
    if (!sLFGMgr->isOptionEnabled(LFG_OPTION_ENABLE_DUNGEON_FINDER | LFG_OPTION_ENABLE_RAID_BROWSER))
        return;

    sLFGMgr->InitializeLockedDungeons(player);
}

void LFGPlayerScript::OnLogout(Player* player)
{
    if (!sLFGMgr->isOptionEnabled(LFG_OPTION_ENABLE_DUNGEON_FINDER | LFG_OPTION_ENABLE_RAID_BROWSER))
        return;

    if (!player->GetGroup())
    {
        player->GetSession()->SendLfgLfrList(false);
        sLFGMgr->LeaveLfg(player->GetGUID());
    }
    else if (player->GetSession()->PlayerDisconnected())
        sLFGMgr->LeaveLfg(player->GetGUID(), true);
}

void LFGPlayerScript::OnLogin(Player* player)
{
    if (!sLFGMgr->isOptionEnabled(LFG_OPTION_ENABLE_DUNGEON_FINDER | LFG_OPTION_ENABLE_RAID_BROWSER))
        return;

    // Temporal: Trying to determine when group data and LFG data gets desynched
    uint64 guid = player->GetGUID();
    uint64 gguid = sLFGMgr->GetGroup(guid);

    if (Group const* group = player->GetGroup())
    {
        uint64 gguid2 = group->GetGUID();
        if (gguid != gguid2)
        {
            TC_LOG_ERROR("lfg", "%s on group %u but LFG has group %u saved... Fixing.",
                player->GetSession()->GetPlayerInfo().c_str(), GUID_LOPART(gguid2), GUID_LOPART(gguid));
            sLFGMgr->SetupGroupMember(guid, group->GetGUID());
        }
    }

    sLFGMgr->InitializeLockedDungeons(player);
    sLFGMgr->SetTeam(player->GetGUID(), player->GetTeam());
    /// @todo - Restore LfgPlayerData and send proper status to player if it was in a group
}

void LFGPlayerScript::OnBindToInstance(Player* player, Difficulty difficulty, uint32 mapId, bool /*permanent*/, bool /*isLfg*/)
{
    MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
    if (mapEntry->IsDungeon() && difficulty > DUNGEON_DIFFICULTY_NORMAL)
        sLFGMgr->InitializeLockedDungeons(player);
}

void LFGPlayerScript::OnMapChanged(Player* player)
{
    Map const* map = player->GetMap();

    if (sLFGMgr->inLfgDungeonMap(player->GetGUID(), map->GetId(), map->GetDifficulty()))
    {
        Group* group = player->GetGroup();
        // This function is also called when players log in
        // if for some reason the LFG system recognises the player as being in a LFG dungeon,
        // but the player was loaded without a valid group, we'll teleport to homebind to prevent
        // crashes or other undefined behaviour
        if (!group)
        {
            sLFGMgr->LeaveLfg(player->GetGUID());
            player->RemoveAurasDueToSpell(LFG_SPELL_LUCK_OF_THE_DRAW);
            player->TeleportTo(player->m_homebindMapId, player->m_homebindX, player->m_homebindY, player->m_homebindZ, 0.0f);
            TC_LOG_ERROR("lfg", "LFGPlayerScript::OnMapChanged, Player %s (%u) is in LFG dungeon map but does not have a valid group! "
                "Teleporting to homebind.", player->GetName().c_str(), player->GetGUIDLow());
            return;
        }

        for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
            if (Player* member = itr->getSource())
                player->GetSession()->SendNameQueryOpcode(member->GetGUID());


        uint8 StackCount = 0;
        for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            if (Player* plrg = itr->getSource())
            {
                if(sLFGMgr->GetIsPremade(plrg->GetGUID()) == false)
                    StackCount++;
            }
        }

        if (StackCount > 3)
            StackCount = 3;

        if (StackCount > 0)
        {
            if (player)
            {
                player->CastSpell(player, LFG_SPELL_LUCK_OF_THE_DRAW, true);
                if (auto luck = player->GetAura(LFG_SPELL_LUCK_OF_THE_DRAW))
                    luck->SetStackAmount(StackCount);
            }
        }         
    }
    else
        player->RemoveAurasDueToSpell(LFG_SPELL_LUCK_OF_THE_DRAW);
}

void LFGPlayerScript::OnItemEquip(Player* player, Item* /*pItem*/, bool /*equip*/)
{
    if (!player)
        return;

    sLFGMgr->InitializeLockedDungeons(player);
    player->GetSession()->SendLfgPlayerLockInfo();
}

LFGGroupScript::LFGGroupScript() : GroupScript("LFGGroupScript")
{
}

void LFGGroupScript::OnAddMember(Group* group, uint64 guid)
{
    if (!sLFGMgr->isOptionEnabled(LFG_OPTION_ENABLE_DUNGEON_FINDER | LFG_OPTION_ENABLE_RAID_BROWSER))
        return;

    uint64 gguid = group->GetGUID();
    uint64 leader = group->GetLeaderGUID();

    if (leader == guid)
    {
        TC_LOG_DEBUG("lfg", "LFGScripts::OnAddMember [" UI64FMTD "]: added [" UI64FMTD "] leader " UI64FMTD "]", gguid, guid, leader);
        sLFGMgr->SetLeader(gguid, guid);
    }
    else
    {
        LfgState gstate = sLFGMgr->GetState(gguid);
        LfgState state = sLFGMgr->GetState(guid);
        TC_LOG_DEBUG("lfg", "LFGScripts::OnAddMember [" UI64FMTD "]: added [" UI64FMTD "] leader " UI64FMTD "] gstate: %u, state: %u", gguid, guid, leader, gstate, state);

        if (state == LFG_STATE_QUEUED)
            sLFGMgr->LeaveLfg(guid);

        if (gstate == LFG_STATE_QUEUED)
            sLFGMgr->LeaveLfg(gguid);
    }

    sLFGMgr->SetGroup(guid, gguid);
    sLFGMgr->AddPlayerToGroup(gguid, guid);
}

void LFGGroupScript::OnRemoveMember(Group* group, uint64 guid, RemoveMethod method, uint64 kicker, char const* reason)
{
    if (!sLFGMgr->isOptionEnabled(LFG_OPTION_ENABLE_DUNGEON_FINDER | LFG_OPTION_ENABLE_RAID_BROWSER))
        return;

    uint64 gguid = group->GetGUID();
    TC_LOG_DEBUG("lfg", "LFGScripts::OnRemoveMember [" UI64FMTD "]: remove [" UI64FMTD "] Method: %d Kicker: [" UI64FMTD "] Reason: %s", gguid, guid, method, kicker, (reason ? reason : ""));

    bool isLFG = group->isLFGGroup();

    if (isLFG && method == GROUP_REMOVEMETHOD_KICK)        // Player have been kicked
    {
        if (strlen(reason) != 0)
        {
            std::string str_reason = std::string(reason);
            sLFGMgr->InitBoot(gguid, kicker, guid, str_reason);
        }
        else if(Player* pKicker = ObjectAccessor::FindPlayer(kicker))
        {
            std::string playerName = "";
            InfoCharEntry info;
            if (sInfoMgr->GetCharInfo(guid, info))
                playerName = info.Name;
            else // should never happen
            {
                PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_NAME);
                stmt->setUInt32(0, GUID_LOPART(guid));
                PreparedQueryResult result = CharacterDatabase.Query(stmt);
                if (!result)
                    return;
                playerName = (*result)[0].GetString();
            }
            pKicker->GetSession()->SendPartyResult(PARTY_OP_UNINVITE, playerName, ERR_VOTE_KICK_REASON_NEEDED, 0, guid);
        }
        return;
    }

    LfgState state = sLFGMgr->GetState(gguid);

    // If group is being formed after proposal success do nothing more
    if (state == LFG_STATE_PROPOSAL && method == GROUP_REMOVEMETHOD_DEFAULT)
    {
        // LfgData: Remove player from group
        sLFGMgr->SetGroup(guid, 0);
        sLFGMgr->RemovePlayerFromGroup(gguid, guid);
        return;
    }

    sLFGMgr->LeaveLfg(guid);
    sLFGMgr->SetGroup(guid, 0);
    uint8 players = sLFGMgr->RemovePlayerFromGroup(gguid, guid);

    if (Player* player = ObjectAccessor::FindPlayer(guid))
    {
        if (method == GROUP_REMOVEMETHOD_LEAVE && state == LFG_STATE_DUNGEON &&
            players >= LFG_GROUP_KICK_VOTES_NEEDED)
			if (sWorld->getBoolConfig(CONFIG_DUNGEON_CAST_DESERTER))
            player->CastSpell(player, LFG_SPELL_DUNGEON_DESERTER, true);

        player->GetSession()->SendLfgUpdateStatus(LfgUpdateData(LFG_UPDATETYPE_LEADER_UNK1), true);
        if (isLFG && player->GetMap()->IsDungeon())            // Teleport player out the dungeon
            sLFGMgr->TeleportPlayer(player, true);
    }

    if (isLFG && state != LFG_STATE_FINISHED_DUNGEON) // Need more players to finish the dungeon
        if (Player* leader = ObjectAccessor::FindPlayer(sLFGMgr->GetLeader(gguid)))
            leader->GetSession()->SendLfgOfferContinue(sLFGMgr->GetDungeon(gguid, false));
}

void LFGGroupScript::OnDisband(Group* group)
{
    if (!sLFGMgr->isOptionEnabled(LFG_OPTION_ENABLE_DUNGEON_FINDER | LFG_OPTION_ENABLE_RAID_BROWSER))
        return;

    uint64 gguid = group->GetGUID();
    TC_LOG_DEBUG("lfg", "LFGScripts::OnDisband [" UI64FMTD "]", gguid);

    sLFGMgr->RemoveGroupData(gguid);
}

void LFGGroupScript::OnChangeLeader(Group* group, uint64 newLeaderGuid, uint64 oldLeaderGuid)
{
    if (!sLFGMgr->isOptionEnabled(LFG_OPTION_ENABLE_DUNGEON_FINDER | LFG_OPTION_ENABLE_RAID_BROWSER))
        return;

    uint64 gguid = group->GetGUID();

    TC_LOG_DEBUG("lfg", "LFGScripts::OnChangeLeader [" UI64FMTD "]: old [" UI64FMTD "] new [" UI64FMTD "]", gguid, newLeaderGuid, oldLeaderGuid);
    sLFGMgr->SetLeader(gguid, newLeaderGuid);
}

void LFGGroupScript::OnInviteMember(Group* group, uint64 guid)
{
    if (!sLFGMgr->isOptionEnabled(LFG_OPTION_ENABLE_DUNGEON_FINDER | LFG_OPTION_ENABLE_RAID_BROWSER))
        return;

    uint64 gguid = group->GetGUID();
    uint64 leader = group->GetLeaderGUID();
    TC_LOG_DEBUG("lfg", "LFGScripts::OnInviteMember [" UI64FMTD "]: invite [" UI64FMTD "] leader [" UI64FMTD "]", gguid, guid, leader);
    // No gguid ==  new group being formed
    // No leader == after group creation first invite is new leader
    // leader and no gguid == first invite after leader is added to new group (this is the real invite)
    if (leader && !gguid)
        sLFGMgr->LeaveLfg(leader);
}

} // namespace lfg
