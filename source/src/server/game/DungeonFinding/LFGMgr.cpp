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

#include "Common.h"
#include "SharedDefines.h"
#include "DBCStores.h"
#include "DisableMgr.h"
#include "ObjectMgr.h"
#include "SocialMgr.h"
#include "Language.h"
#include "LFGMgr.h"
#include "LFGScripts.h"
#include "LFGGroupData.h"
#include "LFGPlayerData.h"
#include "LFGQueue.h"
#include "Group.h"
#include "Player.h"
#include "GroupMgr.h"
#include "GameEventMgr.h"
#include "WorldSession.h"
#include "SoloQueue.h"

namespace lfg
{

LFGMgr::LFGMgr(): m_QueueTimer(0), m_lfgProposalId(1),
                  m_options(sWorld->getIntConfig(CONFIG_LFG_OPTIONSMASK)), _isCallToArmEligible(sWorld->getBoolConfig(CONFIG_ELIGIBLE_CALL_TO_ARM_LFG)), _callToArmsRoles(PLAYER_ROLE_TANK | PLAYER_ROLE_HEALER)
{
    new LFGPlayerScript();
    new LFGGroupScript();
}

LFGMgr::~LFGMgr()
{
    for (auto itr = RewardMapStore.begin(); itr != RewardMapStore.end(); ++itr)
        delete itr->second;
}

void LFGMgr::_LoadFromDB(Field* fields, uint64 guid)
{
    if (!fields)
        return;

    if (!IS_GROUP_GUID(guid))
        return;

    SetLeader(guid, MAKE_NEW_GUID(fields[0].GetUInt32(), 0, HIGHGUID_PLAYER));

    uint32 dungeon = fields[16].GetUInt32();
    uint8 state = fields[17].GetUInt8();

    if (!dungeon || !state)
        return;

    SetDungeon(guid, dungeon);

    switch (state)
    {
        case LFG_STATE_DUNGEON:
        case LFG_STATE_FINISHED_DUNGEON:
            SetState(guid, (LfgState)state);
            break;
        default:
            break;
    }
}

void LFGMgr::_SaveToDB(uint64 guid, uint32 db_guid)
{
    if (!IS_GROUP_GUID(guid))
        return;

    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_LFG_DATA);

    stmt->setUInt32(0, db_guid);

    CharacterDatabase.Execute(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_LFG_DATA);
    stmt->setUInt32(0, db_guid);

    stmt->setUInt32(1, GetDungeon(guid));
    stmt->setUInt32(2, GetState(guid));

    CharacterDatabase.Execute(stmt);
}

/// Load rewards for completing dungeons
void LFGMgr::LoadRewards()
{
    uint32 oldMSTime = getMSTime();

    for (auto itr = RewardMapStore.begin(); itr != RewardMapStore.end(); ++itr)
        delete itr->second;
    RewardMapStore.clear();

    // ORDER BY is very important for GetRandomDungeonReward!
    QueryResult result = WorldDatabase.Query("SELECT dungeonId, maxLevel, firstQuestId, otherQuestId FROM lfg_dungeon_rewards ORDER BY dungeonId, maxLevel ASC");

    if (!result)
    {
        TC_LOG_ERROR("server.loading", ">> Loaded 0 lfg dungeon rewards. DB table `lfg_dungeon_rewards` is empty!");
        return;
    }

    uint32 count = 0;

    Field* fields = NULL;
    do
    {
        fields = result->Fetch();
        uint32 dungeonId = fields[0].GetUInt32();
        uint32 maxLevel = fields[1].GetUInt8();
        uint32 firstQuestId = fields[2].GetUInt32();
        uint32 otherQuestId = fields[3].GetUInt32();

        if (!GetLFGDungeonEntry(dungeonId))
        {
            TC_LOG_ERROR("sql.sql", "Dungeon %u specified in table `lfg_dungeon_rewards` does not exist!", dungeonId);
            continue;
        }

        if (!maxLevel || maxLevel > sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
        {
            TC_LOG_ERROR("sql.sql", "Level %u specified for dungeon %u in table `lfg_dungeon_rewards` can never be reached!", maxLevel, dungeonId);
            maxLevel = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);
        }

        if (!firstQuestId || !sObjectMgr->GetQuestTemplate(firstQuestId))
        {
            TC_LOG_ERROR("sql.sql", "First quest %u specified for dungeon %u in table `lfg_dungeon_rewards` does not exist!", firstQuestId, dungeonId);
            continue;
        }

        if (otherQuestId && !sObjectMgr->GetQuestTemplate(otherQuestId))
        {
            TC_LOG_ERROR("sql.sql", "Other quest %u specified for dungeon %u in table `lfg_dungeon_rewards` does not exist!", otherQuestId, dungeonId);
            otherQuestId = 0;
        }

        RewardMapStore.insert(LfgRewardContainer::value_type(dungeonId, new LfgReward(maxLevel, firstQuestId, otherQuestId)));
        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u lfg dungeon rewards in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

LFGDungeonData const* LFGMgr::GetLFGDungeon(uint32 id) const
{
    LFGDungeonContainer::const_iterator itr = LfgDungeonStore.find(id);
    if (itr != LfgDungeonStore.end())
        return &(itr->second);

    return NULL;
}

uint32 LFGMgr::GetLfgMapDungeonId(uint32 mapId, uint32 difficulty)
{
    LfgMapToDungeonId::const_iterator itr = MapToDungeonIdStore.find(MAKE_PAIR32(mapId, difficulty));
    return itr != MapToDungeonIdStore.end() ? itr->second : 0;
}

bool LFGMgr::IsCallToArmEligible(uint32 level = 0, uint32 dungeonId = 0)
{
    return level == DEFAULT_MAX_LEVEL && (dungeonId == LFG_RANDOM_HEROIC_CATACLYSM || dungeonId == LFG_RANDOM_HEROIC_CATACLYSM_T2);
}

bool LFGMgr::IsPermanentLFGDungeon(uint64 gguid) const
{
    /// won't use global lfg getters to prevent creating now entries in storage
    LfgGroupDataContainer::const_iterator gItr = GroupsStore.find(gguid);
    if (gItr != GroupsStore.end())
    {
        // GetDungeon(gguid)
        if (LFGDungeonData const* dg = GetLFGDungeon(gItr->second.GetDungeon()))
            if (dg->difficulty == DUNGEON_DIFFICULTY_NORMAL || dg->isLFR)
                return false;

        // GetPlayers(gguid)
        LfgGuidSet const& players = gItr->second.GetPlayers();
        for (auto it = players.begin(); it != players.end(); ++it)
        {
            auto pItr = PlayersStore.find(*it);
            if (pItr != PlayersStore.end())
            {
                // GetSelectedDungeons(*it)
                LfgDungeonSet const& dungeons = pItr->second.GetSelectedDungeons();

                uint32 rDungeonId = 0;
                if (!dungeons.empty())
                    rDungeonId = *dungeons.begin();

                if (LFGDungeonData const* dungeon = GetLFGDungeon(rDungeonId))
                    if (dungeon->type == LFG_TYPE_RANDOM)
                        return false;
            }
        }
    }
    return true;
}

void LFGMgr::LoadLFGDungeons(bool reload /* = false */)
{
    uint32 oldMSTime = getMSTime();

    LfgDungeonStore.clear();

    // Initialize Dungeon map with data from dbcs
    for (uint32 i = 0; i < sLFGDungeonStore.GetNumRows(); ++i)
    {
        LFGDungeonEntry const* dungeon = sLFGDungeonStore.LookupEntry(i);
        if (!dungeon)
            continue;
        
        switch (dungeon->type)
        {
            case LFG_TYPE_DUNGEON:
            case LFG_TYPE_RAID:
            case LFG_TYPE_RANDOM:
                LfgDungeonStore.insert(LFGDungeonContainer::value_type(dungeon->ID, LFGDungeonData(dungeon)));
                break;
        }

        if (dungeon->map > -1)
            MapToDungeonIdStore[MAKE_PAIR32(dungeon->map, dungeon->difficulty)] = dungeon->ID;
    }

    // Fill teleport locations from DB
    QueryResult result = WorldDatabase.Query("SELECT dungeonId, position_x, position_y, position_z, orientation, neededILevel FROM lfg_entrances");

    if (!result)
    {
        TC_LOG_ERROR("server.loading", ">> Loaded 0 lfg entrance positions. DB table `lfg_entrances` is empty!");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();
        uint32 dungeonId = fields[0].GetUInt32();
        auto dungeonItr = LfgDungeonStore.find(dungeonId);
        if (dungeonItr == LfgDungeonStore.end())
        {
            TC_LOG_ERROR("sql.sql", "table `lfg_entrances` contains coordinates for wrong dungeon %u", dungeonId);
            continue;
        }

        LFGDungeonData& data = dungeonItr->second;
        data.x = fields[1].GetFloat();
        data.y = fields[2].GetFloat();
        data.z = fields[3].GetFloat();
        data.o = fields[4].GetFloat();

        data.neededILevel = fields[5].GetUInt16();
        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u lfg entrance positions in %u ms", count, GetMSTimeDiffToNow(oldMSTime));

    // Fill all other teleport coords from areatriggers
    for (auto itr = LfgDungeonStore.begin(); itr != LfgDungeonStore.end(); ++itr)
    {
        LFGDungeonData& dungeon = itr->second;

        // No teleport coords in database, load from areatriggers
        if (dungeon.type != LFG_TYPE_RANDOM && dungeon.x == 0.0f && dungeon.y == 0.0f && dungeon.z == 0.0f)
        {
            AreaTriggerStruct const* at = sObjectMgr->GetMapEntranceTrigger(dungeon.map);
            if (!at)
            {
                TC_LOG_ERROR("lfg", "LFGMgr::LoadLFGDungeons: Failed to load dungeon %s, cant find areatrigger for map %u", dungeon.name.c_str(), dungeon.map);
                continue;
            }

            dungeon.map = at->target_mapId;
            dungeon.x = at->target_X;
            dungeon.y = at->target_Y;
            dungeon.z = at->target_Z;
            dungeon.o = at->target_Orientation;
        }

        if (dungeon.type != LFG_TYPE_RANDOM)
        {
            CachedDungeonMapStore[dungeon.group].insert(dungeon.id);

            // Endtime, Well of Eternity and Hour of Twilight have two group types 12 and 33 we load it here into group 33 because DBC can only have one group type
            if (dungeon.id == 435 || dungeon.id == 437 || dungeon.id == 439)
                CachedDungeonMapStore[33].insert(dungeon.id);
        }
        CachedDungeonMapStore[0].insert(dungeon.id);
    }

    if (reload)
    {
        CachedDungeonMapStore.clear();
        // Recalculate locked dungeons
        for (auto it = PlayersStore.begin(); it != PlayersStore.end(); ++it)
            if (Player* player = ObjectAccessor::FindPlayer(it->first))
                InitializeLockedDungeons(player);
    }
}

void LFGMgr::Update(uint32 diff)
{
    if (!isOptionEnabled(LFG_OPTION_ENABLE_DUNGEON_FINDER | LFG_OPTION_ENABLE_RAID_BROWSER))
        return;

    time_t currTime = time(NULL);

    // Remove obsolete role checks
    for (auto it = RoleChecksStore.begin(); it != RoleChecksStore.end();)
    {
        auto itRoleCheck = it++;
        LfgRoleCheck& roleCheck = itRoleCheck->second;
        if (currTime < roleCheck.cancelTime)
            continue;
        roleCheck.state = LFG_ROLECHECK_MISSING_ROLE;

        for (auto itRoles = roleCheck.roles.begin(); itRoles != roleCheck.roles.end(); ++itRoles)
        {
            uint64 guid = itRoles->first;
            RestoreState(guid, "Remove Obsolete RoleCheck");
            SendLfgRoleCheckUpdate(guid, roleCheck);
            if (guid == roleCheck.leader)
                SendLfgJoinResult(guid, LfgJoinResultData(LFG_JOIN_ROLE_CHECK_FAILED, LFG_ROLECHECK_MISSING_ROLE));
        }

        RestoreState(itRoleCheck->first, "Remove Obsolete RoleCheck");
        RoleChecksStore.erase(itRoleCheck);
    }

    // Remove obsolete proposals
    for (auto it = ProposalsStore.begin(); it != ProposalsStore.end();)
    {
        auto itRemove = it++;
        if (itRemove->second.cancelTime < currTime)
            RemoveProposal(itRemove, LFG_UPDATETYPE_PROPOSAL_FAILED);
    }

    // Remove obsolete kicks
    for (auto it = BootsStore.begin(); it != BootsStore.end();)
    {
        auto itBoot = it++;
        LfgPlayerBoot& boot = itBoot->second;
        if (boot.cancelTime < currTime)
        {
            boot.inProgress = false;
            for (LfgAnswerContainer::const_iterator itVotes = boot.votes.begin(); itVotes != boot.votes.end(); ++itVotes)
            {
                uint64 pguid = itVotes->first;
                if (pguid != boot.victim)
                    SendLfgBootProposalUpdate(pguid, boot);
            }
            SetVoteKick(itBoot->first, false);
            BootsStore.erase(itBoot);
        }
    }

    uint32 lastProposalId = m_lfgProposalId;
    // Check if a proposal can be formed with the new groups being added
    for (auto it = QueuesStore.begin(); it != QueuesStore.end(); ++it)
        if (uint8 newProposals = it->second.FindGroups())
            TC_LOG_DEBUG("lfg", "LFGMgr::Update: Found %u new groups in queue %u", newProposals, it->first);

    if (lastProposalId != m_lfgProposalId)
    {
        // FIXME lastProposalId ? lastProposalId +1 ?
        for (auto itProposal = ProposalsStore.find(m_lfgProposalId); itProposal != ProposalsStore.end(); ++itProposal)
        {
            uint32 proposalId = itProposal->first;
            LfgProposal& proposal = ProposalsStore[proposalId];

            uint64 guid = 0;
            for (auto itPlayers = proposal.players.begin(); itPlayers != proposal.players.end(); ++itPlayers)
            {
                guid = itPlayers->first;
                SetState(guid, LFG_STATE_PROPOSAL);
                if (uint64 gguid = GetGroup(guid))
                {
                    SetState(gguid, LFG_STATE_PROPOSAL);
                    SendLfgUpdateStatus(guid, LfgUpdateData(LFG_UPDATETYPE_PROPOSAL_BEGIN, GetSelectedDungeons(guid), GetComment(guid)), true);
                }
                else
                    SendLfgUpdateStatus(guid, LfgUpdateData(LFG_UPDATETYPE_PROPOSAL_BEGIN, GetSelectedDungeons(guid), GetComment(guid)), false);
                SendLfgUpdateProposal(guid, proposal);
            }

            if (proposal.state == LFG_PROPOSAL_SUCCESS)
                UpdateProposal(proposalId, guid, true);
        }
    }

    // Update all players status queue info
    if (m_QueueTimer > LFG_QUEUEUPDATE_INTERVAL)
    {
        m_QueueTimer = 0;
        uint32 tankCount = 0;
        uint32 healerCount = 0;
        uint32 dpsCount = 0;
        time_t currTime = time(NULL);
        for (auto it = QueuesStore.begin(); it != QueuesStore.end(); ++it)
            it->second.UpdateQueueTimers(it->first, currTime, tankCount, healerCount, dpsCount);

        LFGMgr::RemoveCallToArmsRole(PLAYER_ROLE_HEALER);
        LFGMgr::RemoveCallToArmsRole(PLAYER_ROLE_TANK);
        LFGMgr::RemoveCallToArmsRole(PLAYER_ROLE_DAMAGE);
        if (!tankCount || tankCount < dpsCount)
            LFGMgr::AddCallToArmsRole(PLAYER_ROLE_TANK);
        if (!healerCount || healerCount < dpsCount)
            LFGMgr::AddCallToArmsRole(PLAYER_ROLE_HEALER);
        if (dpsCount && (dpsCount < tankCount || dpsCount < healerCount)) // this will never happen....
            AddCallToArmsRole(PLAYER_ROLE_DAMAGE);
    }
    else
        m_QueueTimer += diff;
}

/**
    Generate the dungeon lock map for a given player

   @param[in]     player Player we need to initialize the lock status map
*/
void LFGMgr::InitializeLockedDungeons(Player* player, uint8 level /* = 0 */)
{
    uint64 guid = player->GetGUID();
    if (!level)
        level = player->getLevel();
    uint8 expansion = player->GetSession()->Expansion();
    LfgDungeonSet const& dungeons = GetDungeonsByRandom(0);
    LfgLockMap lock;

    for (auto it = dungeons.begin(); it != dungeons.end(); ++it)
    {
        LFGDungeonData const* dungeon = GetLFGDungeon(*it);
        if (!dungeon) // should never happen - We provide a list from sLFGDungeonStore
            continue;

        LfgLockStatusType lockData = LFG_LOCKSTATUS_NONE;

        //TC_LOG_INFO("server.loading","%u %u %u %u %u",dungeon->Entry(),dungeon->minlevel,dungeon->maxlevel,level); //Debug Line

        if ((dungeon->neededILevel) && dungeon->neededILevel > player->GetAverageItemLevel())
            lockData = LFG_LOCKSTATUS_TOO_LOW_GEAR_SCORE;
        else if (dungeon->expansion > expansion)
            lockData = LFG_LOCKSTATUS_INSUFFICIENT_EXPANSION;
        else if (DisableMgr::IsDisabledFor(DISABLE_TYPE_MAP, dungeon->map, player, dungeon->difficulty))
            lockData = LFG_LOCKSTATUS_RAID_LOCKED;
        else if (dungeon->difficulty > DUNGEON_DIFFICULTY_NORMAL && player->GetBoundInstance(dungeon->map, Difficulty(dungeon->difficulty), dungeon->isLFR))
            lockData = LFG_LOCKSTATUS_RAID_LOCKED;
        else if (dungeon->minlevel > level)
            lockData = LFG_LOCKSTATUS_TOO_LOW_LEVEL;
        else if (dungeon->maxlevel < level)
            lockData = LFG_LOCKSTATUS_TOO_HIGH_LEVEL;
        else if (dungeon->seasonal && !IsSeasonActive(dungeon->id))
            lockData = LFG_LOCKSTATUS_NOT_IN_SEASON;
        else if (AccessRequirement const* ar = sObjectMgr->GetAccessRequirement(dungeon->map, Difficulty(dungeon->difficulty)))
        {
            if (ar->achievement && !player->HasAchieved(ar->achievement))
                lockData = LFG_LOCKSTATUS_MISSING_ACHIEVEMENT;
            else if (player->GetTeam() == ALLIANCE && ar->quest_A && !player->GetQuestRewardStatus(ar->quest_A))
                lockData = LFG_LOCKSTATUS_QUEST_NOT_COMPLETED;
            else if (player->GetTeam() == HORDE && ar->quest_H && !player->GetQuestRewardStatus(ar->quest_H))
                lockData = LFG_LOCKSTATUS_QUEST_NOT_COMPLETED;
            else
                if (ar->item)
                {
                    if (!player->HasItemCount(ar->item) && (!ar->item2 || !player->HasItemCount(ar->item2)))
                        lockData = LFG_LOCKSTATUS_MISSING_ITEM;
                }
                else if (ar->item2 && !player->HasItemCount(ar->item2))
                    lockData = LFG_LOCKSTATUS_MISSING_ITEM;
        }
        /* TODO VoA closed if WG is not under team control (LFG_LOCKSTATUS_RAID_LOCKED)
         *   lockData = LFG_LOCKSTATUS_ATTUNEMENT_TOO_LOW_LEVEL;
         *   lockData = LFG_LOCKSTATUS_ATTUNEMENT_TOO_HIGH_LEVEL;
         *   lockData = LFG_LOCKSTATUS_TOO_HIGH_GEAR_SCORE;
         */

        if (lockData != LFG_LOCKSTATUS_NONE)
        {
            LockData& dataSet = lock[dungeon->Entry()];
            dataSet.locktyp          = lockData;
            dataSet.neededItemlevel  = dungeon->neededILevel;
            dataSet.averageItemlevel = uint32(player->GetAverageItemLevel());   //Wrong But why :D
        }
    }
    SetLockedDungeons(guid, lock);
}

/**
    Adds the player/group to lfg queue. If player is in a group then it is the leader
    of the group tying to join the group. Join conditions are checked before adding
    to the new queue.

   @param[in]     player Player trying to join (or leader of group trying to join)
   @param[in]     roles Player selected roles
   @param[in]     dungeons Dungeons the player/group is applying for
   @param[in]     comment Player selected comment
*/
void LFGMgr::JoinLfg(Player* player, uint8 roles, LfgDungeonSet& dungeons, const std::string& comment)
{
    if (!player || !player->GetSession() || dungeons.empty())
       return;

    Group* grp = player->GetGroup();
    uint64 guid = player->GetGUID();
    uint64 gguid = grp ? grp->GetGUID() : guid;
    LfgJoinResultData joinData;
    LfgGuidSet players;
    uint32 rDungeonId = 0;
    bool isContinue = grp && grp->isLFGGroup() && GetState(gguid) != LFG_STATE_FINISHED_DUNGEON;

    // Do not allow to change dungeon in the middle of a current dungeon
    if (isContinue)
    {
        dungeons.clear();
        dungeons.insert(GetDungeon(gguid));
    }

    // Already in queue?
    LfgState state = GetState(gguid);
    if (state == LFG_STATE_QUEUED)
    {
        LFGQueue& queue = GetQueue(gguid);
        queue.RemoveFromQueue(gguid);
    }

    // Check player or group member restrictions
    else if (player->InBattleground() || player->InArena() || player->InBattlegroundQueue() || sSoloQueueMgr->IsPlayerInSoloQueue(player))
        joinData.result = LFG_JOIN_USING_BG_SYSTEM;
    else if (player->HasAura(LFG_SPELL_DUNGEON_DESERTER))
        joinData.result = LFG_JOIN_DESERTER;
    else if (player->HasAura(LFG_SPELL_DUNGEON_COOLDOWN))
        joinData.result = LFG_JOIN_RANDOM_COOLDOWN;
    else if (dungeons.empty())
        joinData.result = LFG_JOIN_NOT_MEET_REQS;
    else if (grp)
    {
        for (auto it = dungeons.begin(); it != dungeons.end(); ++it)
            if (LFGDungeonData const* dungeon = sLFGMgr->GetLFGDungeon((*it)))
                if (grp->GetMembersCount() > dungeon->GetMaxGroupSize())
                {
                    joinData.result = LFG_JOIN_TOO_MUCH_MEMBERS;
                    break;
                }

        if (joinData.result == LFG_JOIN_OK)
        {
            uint8 memberCount = 0;
            for (auto itr = grp->GetFirstMember(); itr != NULL && joinData.result == LFG_JOIN_OK; itr = itr->next())
            {
                if (Player* plrg = itr->getSource())
                {
                    if (plrg->HasAura(LFG_SPELL_DUNGEON_DESERTER))
                        joinData.result = LFG_JOIN_PARTY_DESERTER;
                    else if (plrg->HasAura(LFG_SPELL_DUNGEON_COOLDOWN))
                        joinData.result = LFG_JOIN_PARTY_RANDOM_COOLDOWN;
                    else if (plrg->InBattleground() || plrg->InArena() || plrg->InBattlegroundQueue() || sSoloQueueMgr->IsPlayerInSoloQueue(plrg))
                        joinData.result = LFG_JOIN_USING_BG_SYSTEM;
                    ++memberCount;
                    players.insert(plrg->GetGUID());
                }
            }

            if (joinData.result == LFG_JOIN_OK && memberCount != grp->GetMembersCount())
                joinData.result = LFG_JOIN_DISCONNECTED;
        }
    }
    else
        players.insert(player->GetGUID());

    // Check if all dungeons are valid
    bool isRaid = false;
    if (joinData.result == LFG_JOIN_OK)
    {
        bool isDungeon = false;
        for (auto it = dungeons.begin(); it != dungeons.end() && joinData.result == LFG_JOIN_OK; ++it)
        {
            LfgType type = GetDungeonType(*it);
            switch (type)
            {
                case LFG_TYPE_RANDOM:
                {
                    if (dungeons.size() > 1)               // Only allow 1 random dungeon
                        joinData.result = LFG_JOIN_DUNGEON_INVALID;
                    else
                        rDungeonId = (*dungeons.begin());
                }
                    // No break on purpose (Random can only be dungeon or heroic dungeon)
                case LFG_TYPE_DUNGEON:
                    if (isRaid)
                        joinData.result = LFG_JOIN_MIXED_RAID_DUNGEON;
                    isDungeon = true;
                    break;
                case LFG_TYPE_RAID:
                    if (isDungeon)
                        joinData.result = LFG_JOIN_MIXED_RAID_DUNGEON;
                    isRaid = true;
                    break;
                default:
                    TC_LOG_ERROR("lfg", "Wrong dungeon type %u for dungeon %u", type, *it);
                    joinData.result = LFG_JOIN_DUNGEON_INVALID;
                    break;
            }
        }

        // it could be changed
        if (joinData.result == LFG_JOIN_OK)
        {
            // Expand random dungeons and check restrictions
            if (rDungeonId)
                dungeons = GetDungeonsByRandom(rDungeonId);

            // if we have lockmap then there are no compatible dungeons
            GetCompatibleDungeons(dungeons, players, joinData.lockmap, isContinue);
            if (dungeons.empty())
                joinData.result = grp ? LFG_JOIN_INTERNAL_ERROR : LFG_JOIN_NOT_MEET_REQS;
        }
    }

    // Can't join. Send result
    if (joinData.result != LFG_JOIN_OK)
    {
        TC_LOG_DEBUG("lfg", "LFGMgr::Join: [" UI64FMTD "] joining with %u members. result: %u", guid, grp ? grp->GetMembersCount() : 1, joinData.result);
        if (!dungeons.empty())                             // Only should show lockmap when have no dungeons available
            joinData.lockmap.clear();
        player->GetSession()->SendLfgJoinResult(joinData);
        return;
    }

    SetComment(guid, comment);

    if (isRaid)
    {
        TC_LOG_DEBUG("lfg", "LFGMgr::Join: [" UI64FMTD "] trying to join raid browser and it's disabled.", guid);
        return;
    }

    std::string debugNames = "";
    if (grp)                                               // Begin rolecheck
    {
        // Create new rolecheck
        LfgRoleCheck& roleCheck = RoleChecksStore[gguid];
        roleCheck.cancelTime = time_t(time(NULL)) + LFG_TIME_ROLECHECK;
        roleCheck.state = LFG_ROLECHECK_INITIALITING;
        roleCheck.leader = guid;
        roleCheck.dungeons = dungeons;
        roleCheck.rDungeonId = rDungeonId;
        roleCheck.isBeginning = true;

        if (rDungeonId)
        {
            dungeons.clear();
            dungeons.insert(rDungeonId);
        }

        SetState(gguid, LFG_STATE_ROLECHECK);
        // Send update to player
        LfgUpdateData updateData = LfgUpdateData(LFG_UPDATETYPE_JOIN_QUEUE, dungeons, comment);
        for (auto itr = grp->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            if (Player* plrg = itr->getSource())
            {
                uint64 pguid = plrg->GetGUID();
                plrg->GetSession()->SendLfgUpdateStatus(updateData, true);
                SetState(pguid, LFG_STATE_ROLECHECK);
                if (!isContinue)
                    SetSelectedDungeons(pguid, dungeons);
                roleCheck.roles[pguid] = 0;
                if (!debugNames.empty())
                    debugNames.append(", ");
                debugNames.append(plrg->GetName());
            }
        }
        // Update leader role
        UpdateRoleCheck(gguid, guid, roles);
    }
    else                                                   // Add player to queue
    {
        LfgRolesMap rolesMap;
        rolesMap[guid] = roles;
        LFGQueue& queue = GetQueue(guid);
        queue.AddQueueData(guid, time(NULL), dungeons, rolesMap);

        if (!isContinue)
        {
            if (rDungeonId)
            {
                dungeons.clear();
                dungeons.insert(rDungeonId);
            }
            SetSelectedDungeons(guid, dungeons);
        }
        // Send update to player
        player->GetSession()->SendLfgJoinResult(joinData);
        player->GetSession()->SendLfgUpdateStatus(LfgUpdateData(LFG_UPDATETYPE_JOIN_QUEUE, dungeons, comment), false);
        SetState(gguid, LFG_STATE_QUEUED);
        SetRoles(guid, roles);
        debugNames.append(player->GetName());
    }

    if (sLog->ShouldLog("lfg", LOG_LEVEL_DEBUG))
    {
        std::ostringstream o;
        o << "LFGMgr::Join: [" << guid << "] joined (" << (grp ? "group" : "player") << ") Members: " << debugNames.c_str()
          << ". Dungeons (" << uint32(dungeons.size()) << "): " << ConcatenateDungeons(dungeons);
        TC_LOG_DEBUG("lfg", "%s", o.str().c_str());
    }
}

/**
    Leaves Dungeon System. Player/Group is removed from queue, rolechecks, proposals
    or votekicks. Player or group needs to be not NULL and using Dungeon System

   @param[in]     guid Player or group guid
*/
void LFGMgr::LeaveLfg(uint64 guid, bool disconnected)
{
    TC_LOG_DEBUG("lfg", "LFGMgr::LeaveLfg: [" UI64FMTD "]", guid);

    uint64 gguid = IS_GROUP_GUID(guid) ? guid : GetGroup(guid);
    LfgState state = GetState(guid);
    switch (state)
    {
        case LFG_STATE_QUEUED:
            if (gguid)
            {
                LFGQueue& queue = GetQueue(gguid);
                queue.RemoveFromQueue(gguid);
                SetState(gguid, LFG_STATE_NONE);
                const LfgGuidSet& players = GetPlayers(gguid);
                for (auto it = players.begin(); it != players.end(); ++it)
                {
                    SetState(*it, LFG_STATE_NONE);
                    SendLfgUpdateStatus(*it, LfgUpdateData(LFG_UPDATETYPE_REMOVED_FROM_QUEUE), true);
                }
            }
            else
            {
                LFGQueue& queue = GetQueue(guid);
                queue.RemoveFromQueue(guid);
                SendLfgUpdateStatus(guid, LfgUpdateData(LFG_UPDATETYPE_REMOVED_FROM_QUEUE), false);
                SetState(guid, LFG_STATE_NONE);
            }
            break;
        case LFG_STATE_ROLECHECK:
            if (gguid)
                UpdateRoleCheck(gguid);                    // No player to update role = LFG_ROLECHECK_ABORTED
            break;
        case LFG_STATE_PROPOSAL:
        {
            // Remove from Proposals
            auto it = ProposalsStore.begin();
            uint64 pguid = gguid == guid ? GetLeader(gguid) : guid;
            while (it != ProposalsStore.end())
            {
                auto itPlayer = it->second.players.find(pguid);
                if (itPlayer != it->second.players.end())
                {
                    // Mark the player/leader of group who left as didn't accept the proposal
                    itPlayer->second.accept = LFG_ANSWER_DENY;
                    break;
                }
                ++it;
            }

            // Remove from queue - if proposal is found, RemoveProposal will call RemoveFromQueue
            if (it != ProposalsStore.end())
                RemoveProposal(it, LFG_UPDATETYPE_PROPOSAL_DECLINED);
            break;
        }
        case LFG_STATE_NONE:
        case LFG_STATE_RAIDBROWSER:
            break;
        case LFG_STATE_DUNGEON:
        case LFG_STATE_FINISHED_DUNGEON:
            if (guid != gguid && !disconnected) // Player
                SetState(guid, LFG_STATE_NONE);
            break;
    }
}

/**
   Update the Role check info with the player selected role.

   @param[in]     grp Group guid to update rolecheck
   @param[in]     guid Player guid (0 = rolecheck failed)
   @param[in]     roles Player selected roles
*/
void LFGMgr::UpdateRoleCheck(uint64 gguid, uint64 guid /* = 0 */, uint8 roles /* = PLAYER_ROLE_NONE */)
{
    if (!gguid)
        return;

    LfgRolesMap check_roles;
    auto itRoleCheck = RoleChecksStore.find(gguid);
    if (itRoleCheck == RoleChecksStore.end())
        return;

    LfgRoleCheck& roleCheck = itRoleCheck->second;
    bool sendRoleChosen = roleCheck.state != LFG_ROLECHECK_DEFAULT && guid;

    LfgDungeonSet dungeons;
    if (roleCheck.rDungeonId)
        dungeons.insert(roleCheck.rDungeonId);
    else
        dungeons = roleCheck.dungeons;

    LFGDungeonData const* dungeon = nullptr;
    if (!dungeons.empty())
        dungeon = sLFGMgr->GetLFGDungeon(*dungeons.begin());

    if (!guid)
        roleCheck.state = LFG_ROLECHECK_ABORTED;
    else if (roles < PLAYER_ROLE_TANK)                            // Player selected no role.
        roleCheck.state = LFG_ROLECHECK_NO_ROLE;
    else
    {
        roleCheck.roles[guid] = roles;

        // Check if all players have selected a role
        auto itRoles = roleCheck.roles.begin();
        while (itRoles != roleCheck.roles.end() && itRoles->second != PLAYER_ROLE_NONE)
            ++itRoles;

        if (itRoles == roleCheck.roles.end())
        {
            // use temporal var to check roles, CheckGroupRoles modifies the roles
            check_roles = roleCheck.roles;
            roleCheck.state = (dungeon && CheckGroupRoles(check_roles, dungeon)) ? LFG_ROLECHECK_FINISHED : LFG_ROLECHECK_WRONG_ROLES;
        }
    }

    LfgJoinResult joinResult = LFG_JOIN_FAILED;
    if (roleCheck.state == LFG_ROLECHECK_MISSING_ROLE || roleCheck.state == LFG_ROLECHECK_WRONG_ROLES)
        joinResult = LFG_JOIN_ROLE_CHECK_FAILED;

    LfgJoinResultData joinData = LfgJoinResultData(joinResult, roleCheck.state);
    for (auto it = roleCheck.roles.begin(); it != roleCheck.roles.end(); ++it)
    {
        uint64 pguid = it->first;

        if (sendRoleChosen && guid != roleCheck.leader)
            SendLfgRoleChosen(pguid, guid, roles);

        SendLfgRoleCheckUpdate(pguid, roleCheck);

        switch (roleCheck.state)
        {
            case LFG_ROLECHECK_INITIALITING:
                continue;
            case LFG_ROLECHECK_FINISHED:
                SetState(pguid, LFG_STATE_QUEUED);
                SetRoles(pguid, it->second);
                SendLfgUpdateStatus(pguid, LfgUpdateData(LFG_UPDATETYPE_ADDED_TO_QUEUE, dungeons, GetComment(pguid)), true);
                break;
            default:
                if (roleCheck.leader == pguid)
                    SendLfgJoinResult(pguid, joinData);
                SendLfgUpdateStatus(pguid, LfgUpdateData(LFG_UPDATETYPE_ROLECHECK_FAILED), true);
                RestoreState(pguid, "Rolecheck Failed");
                break;
        }
    }

    roleCheck.isBeginning = false;

    if (roleCheck.state == LFG_ROLECHECK_FINISHED)
    {
        SetState(gguid, LFG_STATE_QUEUED);
        LFGQueue& queue = GetQueue(gguid);
        queue.AddQueueData(gguid, time_t(time(NULL)), roleCheck.dungeons, roleCheck.roles);
        RoleChecksStore.erase(itRoleCheck);
    }
    else if (roleCheck.state != LFG_ROLECHECK_INITIALITING)
    {
        RestoreState(gguid, "Rolecheck Failed");
        RoleChecksStore.erase(itRoleCheck);
    }
}

/**
   Given a list of dungeons remove the dungeons players have restrictions.

   @param[in, out] dungeons Dungeons to check restrictions
   @param[in]     players Set of players to check their dungeon restrictions
   @param[out]    lockMap Map of players Lock status info of given dungeons (Empty if dungeons is not empty)
*/
void LFGMgr::GetCompatibleDungeons(LfgDungeonSet& dungeons, LfgGuidSet const& players, LfgLockPartyMap& lockMap, bool isContinue)
{
    LfgDungeonSet dungeonsBackup = dungeons;
    lockMap.clear();

    for (auto it = players.begin(); it != players.end() && !dungeons.empty(); ++it)
    {
        uint64 guid = (*it);
        LfgLockMap const& cachedLockMap = GetLockedDungeons(guid);
        for (LfgLockMap::const_iterator it2 = cachedLockMap.begin(); it2 != cachedLockMap.end() && !dungeons.empty(); ++it2)
        {
            uint32 dungeonId = (it2->first & 0x00FFFFFF); // Compare dungeon ids
            LfgDungeonSet::iterator itDungeon = dungeons.find(dungeonId);
            if (itDungeon != dungeons.end())
            {
                // Don't remove the dungeon if team members are trying to continue a locked instance
                if (!(it2->second.locktyp == LFG_LOCKSTATUS_RAID_LOCKED && isContinue))
                {
                    dungeons.erase(itDungeon);
                    lockMap[guid][dungeonId] = it2->second;
                }
            }
        }
    }

    if (dungeons.empty())
        dungeons = dungeonsBackup; // character is bound to all id's reset all requirements

    lockMap.clear();
}

/**
   Check if a group can be formed with the given group roles

   @param[in]     groles Map of roles to check
   @param[in]     dungeon LFGDungeonData to check
   @return True if roles are compatible
*/
bool LFGMgr::CheckGroupRoles(LfgRolesMap& groles, LFGDungeonData const* dungeon)
{
    ASSERT(dungeon);
    if (groles.empty())
        return false;

    uint8 damage = 0;
    uint8 tank = 0;
    uint8 healer = 0;

    for (LfgRolesMap::iterator it = groles.begin(); it != groles.end(); ++it)
    {
        uint8 role = it->second & ~PLAYER_ROLE_LEADER;
        if (role == PLAYER_ROLE_NONE)
            return false;

        if (role & PLAYER_ROLE_DAMAGE)
        {
            if (role != PLAYER_ROLE_DAMAGE)
            {
                it->second -= PLAYER_ROLE_DAMAGE;
                if (CheckGroupRoles(groles, dungeon))
                    return true;
                it->second += PLAYER_ROLE_DAMAGE;
            }
            else if (damage == dungeon->requiredDamagedealers)
                return false;
            else
                damage++;
        }

        if (role & PLAYER_ROLE_HEALER)
        {
            if (role != PLAYER_ROLE_HEALER)
            {
                it->second -= PLAYER_ROLE_HEALER;
                if (CheckGroupRoles(groles, dungeon))
                    return true;
                it->second += PLAYER_ROLE_HEALER;
            }
            else if (healer == dungeon->requiredHeals)
                return false;
            else
                healer++;
        }

        if (role & PLAYER_ROLE_TANK)
        {
            if (role != PLAYER_ROLE_TANK)
            {
                it->second -= PLAYER_ROLE_TANK;
                if (CheckGroupRoles(groles, dungeon))
                    return true;
                it->second += PLAYER_ROLE_TANK;
            }
            else if (tank == dungeon->requiredTanks)
                return false;
            else
                tank++;
        }
    }
    return (tank + healer + damage) == uint8(groles.size());
}

/**
   Makes a new group given a proposal
   @param[in]     proposal Proposal to get info from
*/
void LFGMgr::MakeNewGroup(LfgProposal const& proposal)
{
    LfgGuidList players;
    LfgGuidList playersToTeleport;

    for (auto it = proposal.players.begin(); it != proposal.players.end(); ++it)
    {
        uint64 guid = it->first;
        if (guid == proposal.leader)
            players.push_front(guid);
        else
            players.push_back(guid);

        if (proposal.isNew || GetGroup(guid) != proposal.group)
            playersToTeleport.push_back(guid);
    }

    // Set the dungeon difficulty
    LFGDungeonData const* dungeon = GetLFGDungeon(proposal.dungeonId);
    ASSERT(dungeon);

    // luck of the draw
    uint32 SetPremadePlayer[5]; 
    uint8 SetPremadePlayerSize = 0;

    Group* grp = proposal.group ? sGroupMgr->GetGroupByGUID(GUID_LOPART(proposal.group)) : NULL;
    for (auto it = players.begin(); it != players.end(); ++it)
    {
        uint64 pguid = (*it);
        Player* player = ObjectAccessor::FindPlayer(pguid);
        if (!player)
            continue;

        Group* group = player->GetGroup();
        if (group && group != grp)
        {           
            // for Luck of the draw 
            if (group->GetMembersCount() == 2) // this because group will be dismantled and we won't have the chance to set it next time
            {
                for (auto itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
                {
                    if (Player* plrg = itr->getSource())
                    {
                        SetIsPremade(plrg->GetGUID(), true);

                        SetPremadePlayer[SetPremadePlayerSize] = plrg->GetGUID();
                        ++SetPremadePlayerSize;
                    }
                }
            }
            else SetIsPremade(player->GetGUID(), true);
                           
            group->RemoveMember(player->GetGUID());
        }
        else
        {
            bool wasSet = false;
            for (uint8 i = 0; i < SetPremadePlayerSize; ++i)
            {
                if (player->GetGUID() == SetPremadePlayer[i])
                {
                    wasSet = true;
                    break;
                }
            }
            if (!wasSet)
                SetIsPremade(player->GetGUID(), false);
        }
           
        if (!grp)
        {
            grp = new Group();
            if (dungeon->isLFR)
                grp->ConvertToLFR();
            else
                grp->ConvertToLFG();
            grp->Create(player);
            uint64 gguid = grp->GetGUID();
            SetState(gguid, LFG_STATE_PROPOSAL);
            sGroupMgr->AddGroup(grp);
        }
        else if (group != grp)
            grp->AddMember(player);

        grp->SetLfgRoles(pguid, proposal.players.find(pguid)->second.role);

        for (auto itr = players.begin(); itr != players.end(); ++itr)
        {
            uint64 temppguid = (*itr);
            player->GetSession()->SendNameQueryOpcode(temppguid);
        }

        // Add the cooldown spell if queued for a random dungeon
        if (dungeon->type == LFG_TYPE_RANDOM)
            player->CastSpell(player, LFG_SPELL_DUNGEON_COOLDOWN, false);
    }

    if (dungeon->isLFR)
        grp->SetRaidDifficulty(Difficulty(dungeon->difficulty));
    else
        grp->SetDungeonDifficulty(Difficulty(dungeon->difficulty));

    uint64 gguid = grp->GetGUID();
    SetDungeon(gguid, dungeon->Entry());
    SetState(gguid, LFG_STATE_DUNGEON);

    _SaveToDB(gguid, grp->GetDbStoreId());

    // Update group info
    grp->SendUpdate();

    // Teleport Player
    for (auto it = playersToTeleport.begin(); it != playersToTeleport.end(); ++it)
        if (Player* player = ObjectAccessor::FindPlayer(*it))
            TeleportPlayer(player, false);
}

uint32 LFGMgr::AddProposal(LfgProposal& proposal)
{
    proposal.id = ++m_lfgProposalId;
    ProposalsStore[m_lfgProposalId] = proposal;
    return m_lfgProposalId;
}

/**
   Update Proposal info with player answer

   @param[in]     proposalId Proposal id to be updated
   @param[in]     guid Player guid to update answer
   @param[in]     accept Player answer
*/
void LFGMgr::UpdateProposal(uint32 proposalId, uint64 guid, bool accept)
{
    // Check if the proposal exists
    auto itProposal = ProposalsStore.find(proposalId);
    if (itProposal == ProposalsStore.end())
        return;

    LfgProposal& proposal = itProposal->second;

    // Check if proposal have the current player
    auto itProposalPlayer = proposal.players.find(guid);
    if (itProposalPlayer == proposal.players.end())
        return;

    LfgProposalPlayer& player = itProposalPlayer->second;
    player.accept = LfgAnswer(accept);

    TC_LOG_DEBUG("lfg", "LFGMgr::UpdateProposal: Player [" UI64FMTD "] of proposal %u selected: %u", guid, proposalId, accept);
    if (!accept)
    {
        RemoveProposal(itProposal, LFG_UPDATETYPE_PROPOSAL_DECLINED);
        return;
    }

    // check if all have answered and reorder players (leader first)
    bool allAnswered = true;
    for (auto itPlayers = proposal.players.begin(); itPlayers != proposal.players.end(); ++itPlayers)
        if (itPlayers->second.accept != LFG_ANSWER_AGREE)   // No answer (-1) or not accepted (0)
            allAnswered = false;

    if (!allAnswered)
    {
        for (auto it = proposal.players.begin(); it != proposal.players.end(); ++it)
            SendLfgUpdateProposal(it->first, proposal);

        return;
    }

    bool sendUpdate = proposal.state != LFG_PROPOSAL_SUCCESS;
    proposal.state = LFG_PROPOSAL_SUCCESS;
    time_t joinTime = time(NULL);

    LFGQueue& queue = GetQueue(guid);
    LfgUpdateData updateData = LfgUpdateData(LFG_UPDATETYPE_GROUP_FOUND);
    for (auto it = proposal.players.begin(); it != proposal.players.end(); ++it)
    {
        uint64 pguid = it->first;
        uint64 gguid = it->second.group;
        uint32 dungeonId = (*GetSelectedDungeons(pguid).begin());
        int32 waitTime = -1;
        if (sendUpdate)
           SendLfgUpdateProposal(pguid, proposal);

        if (gguid)
        {
            waitTime = int32((joinTime - queue.GetJoinTime(gguid)) / IN_MILLISECONDS);
            SendLfgUpdateStatus(pguid, updateData, false);
        }
        else
        {
            waitTime = int32((joinTime - queue.GetJoinTime(pguid)) / IN_MILLISECONDS);
            SendLfgUpdateStatus(pguid, updateData, false);
        }
        updateData.updateType = LFG_UPDATETYPE_REMOVED_FROM_QUEUE;
        SendLfgUpdateStatus(pguid, updateData, true);
        SendLfgUpdateStatus(pguid, updateData, false);

        // Update timers
        uint8 role = GetRoles(pguid);
        role &= ~PLAYER_ROLE_LEADER;
        switch (role)
        {
            case PLAYER_ROLE_DAMAGE:
                queue.UpdateWaitTimeDps(waitTime, dungeonId);
                break;
            case PLAYER_ROLE_HEALER:
                queue.UpdateWaitTimeHealer(waitTime, dungeonId);
                break;
            case PLAYER_ROLE_TANK:
                queue.UpdateWaitTimeTank(waitTime, dungeonId);
                break;
            default:
                queue.UpdateWaitTimeAvg(waitTime, dungeonId);
                break;
        }

        SetState(pguid, LFG_STATE_DUNGEON);
    }

    // Remove players/groups from Queue
    for (auto it = proposal.queues.begin(); it != proposal.queues.end(); ++it)
        queue.RemoveFromQueue(*it);

    MakeNewGroup(proposal);
    ProposalsStore.erase(itProposal);
}

/**
   Remove a proposal from the pool, remove the group that didn't accept (if needed) and readd the other members to the queue

   @param[in]     itProposal Iterator to the proposal to remove
   @param[in]     type Type of removal (LFG_UPDATETYPE_PROPOSAL_FAILED, LFG_UPDATETYPE_PROPOSAL_DECLINED)
*/
void LFGMgr::RemoveProposal(LfgProposalContainer::iterator itProposal, LfgUpdateType type)
{
    LfgProposal& proposal = itProposal->second;
    proposal.state = LFG_PROPOSAL_FAILED;

    TC_LOG_DEBUG("lfg", "LFGMgr::RemoveProposal: Proposal %u, state FAILED, UpdateType %u", itProposal->first, type);
    // Mark all people that didn't answered as no accept
    if (type == LFG_UPDATETYPE_PROPOSAL_FAILED)
        for (auto it = proposal.players.begin(); it != proposal.players.end(); ++it)
            if (it->second.accept == LFG_ANSWER_PENDING)
                it->second.accept = LFG_ANSWER_DENY;

    // Mark players/groups to be removed
    LfgGuidSet toRemove;
    for (auto it = proposal.players.begin(); it != proposal.players.end(); ++it)
    {
        if (it->second.accept == LFG_ANSWER_AGREE)
            continue;

        uint64 guid = it->second.group ? it->second.group : it->first;
        // Player didn't accept or still pending when no secs left
        if (it->second.accept == LFG_ANSWER_DENY || type == LFG_UPDATETYPE_PROPOSAL_FAILED)
        {
            it->second.accept = LFG_ANSWER_DENY;
            toRemove.insert(guid);
            Player* leavingPlayer = ObjectAccessor::FindPlayer(guid);
            if (leavingPlayer)
            {
                leavingPlayer->CastCustomSpell(LFG_SPELL_DUNGEON_DESERTER, SPELLVALUE_DURATION, 180000, leavingPlayer);
            }
        }
    }

    // Notify players
    for (auto it = proposal.players.begin(); it != proposal.players.end(); ++it)
    {
        uint64 guid = it->first;
        uint64 gguid = it->second.group ? it->second.group : guid;

        SendLfgUpdateProposal(guid, proposal);

        if (toRemove.find(gguid) != toRemove.end())         // Didn't accept or in same group that someone that didn't accept
        {
            LfgUpdateData updateData;
            if (it->second.accept == LFG_ANSWER_DENY)
            {
                updateData.updateType = type;
                TC_LOG_DEBUG("lfg", "LFGMgr::RemoveProposal: [" UI64FMTD "] didn't accept. Removing from queue and compatible cache", guid);
            }
            else
            {
                updateData.updateType = LFG_UPDATETYPE_REMOVED_FROM_QUEUE;
                TC_LOG_DEBUG("lfg", "LFGMgr::RemoveProposal: [" UI64FMTD "] in same group that someone that didn't accept. Removing from queue and compatible cache", guid);
            }

            RestoreState(guid, "Proposal Fail (didn't accepted or in group with someone that didn't accept");
            if (gguid != guid)
            {
                RestoreState(it->second.group, "Proposal Fail (someone in group didn't accepted)");
                SendLfgUpdateStatus(guid, updateData, true);
            }
            else
                SendLfgUpdateStatus(guid, updateData, false);
        }
        else
        {
            TC_LOG_DEBUG("lfg", "LFGMgr::RemoveProposal: Readding [" UI64FMTD "] to queue.", guid);
            SetState(guid, LFG_STATE_QUEUED);
            if (gguid != guid)
            {
                SetState(gguid, LFG_STATE_QUEUED);
                SendLfgUpdateStatus(guid, LfgUpdateData(LFG_UPDATETYPE_ADDED_TO_QUEUE, GetSelectedDungeons(guid), GetComment(guid)), true);
            }
            else
                SendLfgUpdateStatus(guid, LfgUpdateData(LFG_UPDATETYPE_ADDED_TO_QUEUE, GetSelectedDungeons(guid), GetComment(guid)), false);
        }
    }

    LFGQueue& queue = GetQueue(proposal.players.begin()->first);
    // Remove players/groups from queue
    for (auto it = toRemove.begin(); it != toRemove.end(); ++it)
    {
        uint64 guid = *it;
        queue.RemoveFromQueue(guid);
        proposal.queues.remove(guid);
    }

    // Readd to queue
    for (auto it = proposal.queues.begin(); it != proposal.queues.end(); ++it)
    {
        uint64 guid = *it;
        queue.AddToQueue(guid);
    }

    ProposalsStore.erase(itProposal);
}

/**
   Initialize a boot kick vote

   @param[in]     gguid Group the vote kicks belongs to
   @param[in]     kicker Kicker guid
   @param[in]     victim Victim guid
   @param[in]     reason Kick reason
*/
void LFGMgr::InitBoot(uint64 gguid, uint64 kicker, uint64 victim, std::string const& reason)
{
    SetVoteKick(gguid, true);

    LfgPlayerBoot& boot = BootsStore[gguid];
    boot.inProgress = true;
    boot.cancelTime = time_t(time(NULL)) + LFG_TIME_BOOT;
    boot.reason = reason;
    boot.victim = victim;

    LfgGuidSet const& players = GetPlayers(gguid);

    // Set votes
    for (auto itr = players.begin(); itr != players.end(); ++itr)
    {
        uint64 guid = (*itr);
        boot.votes[guid] = LFG_ANSWER_PENDING;
    }

    boot.votes[victim] = LFG_ANSWER_DENY;                  // Victim auto vote NO
    boot.votes[kicker] = LFG_ANSWER_AGREE;                 // Kicker auto vote YES

    // Notify players
    for (auto it = players.begin(); it != players.end(); ++it)
        SendLfgBootProposalUpdate(*it, boot);
}

/**
   Update Boot info with player answer

   @param[in]     guid Player who has answered
   @param[in]     player answer
*/
void LFGMgr::UpdateBoot(uint64 guid, bool accept)
{
    uint64 gguid = GetGroup(guid);
    if (!gguid)
        return;

    auto itBoot = BootsStore.find(gguid);
    if (itBoot == BootsStore.end())
        return;

    LfgPlayerBoot& boot = itBoot->second;

    if (boot.votes[guid] != LFG_ANSWER_PENDING)    // Cheat check: Player can't vote twice
        return;

    boot.votes[guid] = LfgAnswer(accept);

    uint8 votesNum = 0;
    uint8 agreeNum = 0;
    for (LfgAnswerContainer::const_iterator itVotes = boot.votes.begin(); itVotes != boot.votes.end(); ++itVotes)
    {
        if (itVotes->second != LFG_ANSWER_PENDING)
        {
            ++votesNum;
            if (itVotes->second == LFG_ANSWER_AGREE)
                ++agreeNum;
        }
    }

    bool isLfrGroup = false;
    if (Group* group = sGroupMgr->GetGroupByGUID(GUID_LOPART(gguid)))
        isLfrGroup = group->isLFRGroup();
            
    // if we don't have enough votes (agree or deny) do nothing
    if (agreeNum < (isLfrGroup ? LFR_GROUP_KICK_VOTES_NEEDED : LFG_GROUP_KICK_VOTES_NEEDED) && (votesNum - agreeNum) < (isLfrGroup ? LFR_GROUP_KICK_VOTES_NEEDED : LFG_GROUP_KICK_VOTES_NEEDED))
        return;

    // Send update info to all players
    boot.inProgress = false;
    for (LfgAnswerContainer::const_iterator itVotes = boot.votes.begin(); itVotes != boot.votes.end(); ++itVotes)
    {
        uint64 pguid = itVotes->first;
        if (pguid != boot.victim)
            SendLfgBootProposalUpdate(pguid, boot);
    }

    SetVoteKick(gguid, false);
    if (agreeNum == (isLfrGroup ? LFR_GROUP_KICK_VOTES_NEEDED : LFG_GROUP_KICK_VOTES_NEEDED))           // Vote passed - Kick player
    {
        if (Group* group = sGroupMgr->GetGroupByGUID(GUID_LOPART(gguid)))
            Player::RemoveFromGroup(group, boot.victim, GROUP_REMOVEMETHOD_KICK_LFG);

        if (!isLfrGroup)
            DecreaseKicksLeft(gguid);

        SetVoteKickCooldown(gguid, (time(NULL) + (15 * MINUTE)));
    }
    BootsStore.erase(itBoot);
}

/**
   Teleports the player in or out the dungeon

   @param[in]     player Player to teleport
   @param[in]     out Teleport out (true) or in (false)
   @param[in]     fromOpcode Function called from opcode handlers? (Default false)
*/
void LFGMgr::TeleportPlayer(Player* player, bool out, bool fromOpcode /*= false*/, bool saveEntryPoint /*= true*/)
{
    LFGDungeonData const* dungeon = NULL;
    Group* group = player->GetGroup();

    if (group && group->isLFGGroup())
        dungeon = GetLFGDungeon(GetDungeon(group->GetGUID()));

    if (!dungeon)
    {
        TC_LOG_DEBUG("lfg", "TeleportPlayer: Player %s not in group/lfggroup or dungeon not found!",
            player->GetName().c_str());
        player->GetSession()->SendLfgTeleportError(uint8(LFG_TELEPORTERROR_INVALID_LOCATION));
        return;
    }

    if (out)
    {
        TC_LOG_DEBUG("lfg", "TeleportPlayer: Player %s is being teleported out. Current Map %u - Expected Map %u",
            player->GetName().c_str(), player->GetMapId(), uint32(dungeon->map));
        if (player->GetMapId() == uint32(dungeon->map))
            player->TeleportToBGEntryPoint();
        return;
    }

    LfgTeleportError error = LFG_TELEPORTERROR_OK;
    if (!player->isAlive())
        error = LFG_TELEPORTERROR_PLAYER_DEAD;
    else if (player->IsFalling() || player->HasUnitState(UNIT_STATE_JUMPING))
        error = LFG_TELEPORTERROR_FALLING;
    else if (player->IsMirrorTimerActive(FATIGUE_TIMER))
        error = LFG_TELEPORTERROR_FATIGUE;
    else if (player->GetVehicle())
        error = LFG_TELEPORTERROR_IN_VEHICLE;
    else if (player->GetCharmGUID())
        error = LFG_TELEPORTERROR_CHARMING;
    else if (player->GetMapId() != uint32(dungeon->map))  // Do not teleport players in dungeon to the entrance
    {
        uint32 mapid = dungeon->map;
        float x = dungeon->x;
        float y = dungeon->y;
        float z = dungeon->z;
        float orientation = dungeon->o;

        if (!fromOpcode && saveEntryPoint)
        {
            // Select a player inside to be teleported to
            for (auto itr = group->GetFirstMember(); itr != NULL && !mapid; itr = itr->next())
            {
                Player* plrg = itr->getSource();
                if (plrg && plrg != player && plrg->GetMapId() == uint32(dungeon->map))
                {
                    mapid = plrg->GetMapId();
                    x = plrg->GetPositionX();
                    y = plrg->GetPositionY();
                    z = plrg->GetPositionZ();
                    orientation = plrg->GetOrientation();
                    break;
                }
            }
        }
        else if (player->HasValidLFGLeavePoint(mapid) && saveEntryPoint)
        {
            Position pos;
            player->GetLFGLeavePoint(&pos);
            x = pos.m_positionX;
            y = pos.m_positionY;
            z = pos.m_positionZ;
            orientation = pos.m_orientation;
        }

        if (!player->GetMap()->IsDungeon() && saveEntryPoint)
            player->SetBattlegroundEntryPoint();

        if (player->isInFlight())
        {
            player->GetMotionMaster()->MovementExpired();
            player->CleanupAfterTaxiFlight();
        }

        if (!player->TeleportTo(mapid, x, y, z, orientation))
            error = LFG_TELEPORTERROR_INVALID_LOCATION;
    }
    else
    {
        if (player->isInCombat())
            error = LFG_TELEPORTERROR_IN_COMBAT;
        else if (player->GetMapId() == uint32(dungeon->map))
        {
            player->SetLFGLeavePoint();
            player->TeleportToBGEntryPoint();
        }
    }

    if (error != LFG_TELEPORTERROR_OK)
        player->GetSession()->SendLfgTeleportError(uint8(error));

    TC_LOG_DEBUG("lfg", "TeleportPlayer: Player %s is being teleported in to map %u "
        "(x: %f, y: %f, z: %f) Result: %u", player->GetName().c_str(), dungeon->map,
        dungeon->x, dungeon->y, dungeon->z, error);
}

/**
   Finish a dungeon and give reward, if any.

   @param[in]     guid Group guid
   @param[in]     dungeonId Dungeonid
*/
void LFGMgr::FinishDungeon(uint64 gguid, const uint32 dungeonId)
{
    uint32 gDungeonId = GetDungeon(gguid);

    bool skipMapDungeonId = false;
    if (gDungeonId == 416 || gDungeonId == 417) // Dragon Soul (LFR) have 3 different dungeon ids we need a extra check here
    {
        const LfgGuidSet& players = GetPlayers(gguid);
        for (auto it = players.begin(); it != players.end(); ++it)
        {
            if (Player* player = ObjectAccessor::FindPlayer((*it)))
            {
                if (player->GetMapId() == 967)
                {
                    skipMapDungeonId = true;
                    break;
                }
            }
        }
    }

    if (!skipMapDungeonId && (gDungeonId != dungeonId))
    {
        TC_LOG_DEBUG("lfg", "LFGMgr::FinishDungeon: [" UI64FMTD "] Finished dungeon %u but group queued for %u. Ignoring", gguid, dungeonId, gDungeonId);
        return;
    }

    if (GetState(gguid) == LFG_STATE_FINISHED_DUNGEON) // Shouldn't happen. Do not reward multiple times
    {
        TC_LOG_DEBUG("lfg", "LFGMgr::FinishDungeon: [" UI64FMTD "] Already rewarded group. Ignoring", gguid);
        return;
    }

    SetState(gguid, LFG_STATE_FINISHED_DUNGEON);

    const LfgGuidSet& players = GetPlayers(gguid);
    for (auto it = players.begin(); it != players.end(); ++it)
    {
        uint64 guid = (*it);
        if (GetState(guid) == LFG_STATE_FINISHED_DUNGEON)
        {
            TC_LOG_DEBUG("lfg", "LFGMgr::FinishDungeon: [" UI64FMTD "] Already rewarded player. Ignoring", guid);
            continue;
        }

        uint32 rDungeonId = 0;
        const LfgDungeonSet& dungeons = GetSelectedDungeons(guid);
        if (!dungeons.empty())
            rDungeonId = (*dungeons.begin());

        SetState(guid, LFG_STATE_FINISHED_DUNGEON);

        // Give rewards only if its a random dungeon
        LFGDungeonData const* dungeon = GetLFGDungeon(rDungeonId);

        if (!dungeon || !skipMapDungeonId && (dungeon->type != LFG_TYPE_RANDOM && !dungeon->seasonal))
        {
            TC_LOG_DEBUG("lfg", "LFGMgr::FinishDungeon: [" UI64FMTD "] dungeon %u is not random or seasonal", guid, rDungeonId);
            continue;
        }

        Player* player = ObjectAccessor::FindPlayer(guid);
        if (!player || !player->IsInWorld())
        {
            TC_LOG_DEBUG("lfg", "LFGMgr::FinishDungeon: [" UI64FMTD "] not found in world", guid);
            continue;
        }

        LFGDungeonData const* dungeonDone = GetLFGDungeon(skipMapDungeonId ? gDungeonId : dungeonId);
        uint32 mapId = dungeonDone ? uint32(dungeonDone->map) : 0;

        if (player->GetMapId() != mapId)
        {
            TC_LOG_DEBUG("lfg", "LFGMgr::FinishDungeon: [" UI64FMTD "] is in map %u and should be in %u to get reward", guid, player->GetMapId(), mapId);
            continue;
        }

        // Update achievements
        if (dungeon->difficulty == DUNGEON_DIFFICULTY_HEROIC)
            player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_USE_LFD_TO_GROUP_WITH_PLAYERS, 1);

        LfgReward const* reward = GetRandomDungeonReward(rDungeonId, player->getLevel());
        if (!reward)
            continue;

        bool done = false;
        Quest const* quest = sObjectMgr->GetQuestTemplate(reward->firstQuest);
        if (!quest)
            continue;

        bool forceValorQuestReward = dungeon->type == LFG_TYPE_RANDOM && dungeon->difficulty == DUNGEON_DIFFICULTY_HEROIC && player->GetCurrencyOnWeek(CURRENCY_TYPE_VALOR_POINTS, true) < 1000;

        // if we can take the quest, means that we haven't done this kind of "run", IE: First Heroic Random of Day.
        if (forceValorQuestReward || player->CanRewardQuest(quest, false))
            player->RewardQuest(quest, 0, NULL, false);
        else
        {
            done = true;
            quest = sObjectMgr->GetQuestTemplate(reward->otherQuest);
            if (!quest)
                continue;
            // we give reward without informing client (retail does this)
            player->RewardQuest(quest, 0, NULL, false);
        }
        uint8 tmpRole = 0;
        if (Group *group = player->GetGroup())
            tmpRole = group->GetLfgRoles(player->GetGUID());

        if (IsCallToArmEligible(player->getLevel(), rDungeonId & 0x00FFFFFF))
            if (player->GetCallToArmsTempRoles() & tmpRole)
            {
                const Quest *q = sObjectMgr->GetQuestTemplate(LFG_CALL_TO_ARMS_QUEST);
                player->RewardQuest(q, 0, NULL, false);
            }
        player->SetTempCallToArmsRoles(0);
        // Give rewards
        TC_LOG_DEBUG("lfg", "LFGMgr::FinishDungeon: [" UI64FMTD "] done dungeon %u, %s previously done.", player->GetGUID(), GetDungeon(gguid), done? " " : " not");
        LfgPlayerRewardData data = LfgPlayerRewardData(dungeon->Entry(), GetDungeon(gguid, false), done, quest);
        player->GetSession()->SendLfgPlayerReward(data);
    }
}

// --------------------------------------------------------------------------//
// Auxiliar Functions
// --------------------------------------------------------------------------//

/**
   Get the dungeon list that can be done given a random dungeon entry.

   @param[in]     randomdungeon Random dungeon id (if value = 0 will return all dungeons)
   @returns Set of dungeons that can be done.
*/
LfgDungeonSet const& LFGMgr::GetDungeonsByRandom(uint32 randomdungeon)
{
    LFGDungeonData const* dungeon = GetLFGDungeon(randomdungeon);
    uint32 group = dungeon ? dungeon->group : 0;
    return CachedDungeonMapStore[group];
}

/**
   Get the reward of a given random dungeon at a certain level

   @param[in]     dungeon dungeon id
   @param[in]     level Player level
   @returns Reward
*/
LfgReward const* LFGMgr::GetRandomDungeonReward(uint32 dungeon, uint8 level)
{
    LfgReward const* rew = NULL;
    LfgRewardContainerBounds bounds = RewardMapStore.equal_range(dungeon & 0x00FFFFFF);
    for (auto itr = bounds.first; itr != bounds.second; ++itr)
    {
        rew = itr->second;
        // ordered properly at loading
        if (itr->second->maxLevel >= level)
            break;
    }

    return rew;
}

/**
   Given a Dungeon id returns the dungeon Type

   @param[in]     dungeon dungeon id
   @returns Dungeon type
*/
LfgType LFGMgr::GetDungeonType(uint32 dungeonId)
{
    LFGDungeonData const* dungeon = GetLFGDungeon(dungeonId);
    if (!dungeon)
        return LFG_TYPE_NONE;

    return LfgType(dungeon->type);
}

LfgState LFGMgr::GetState(uint64 guid)
{
    LfgState state;
    if (IS_GROUP_GUID(guid))
        state = GroupsStore[guid].GetState();
    else
        state = PlayersStore[guid].GetState();

    TC_LOG_TRACE("lfg", "LFGMgr::GetState: [" UI64FMTD "] = %u", guid, state);
    return state;
}

LfgState LFGMgr::GetOldState(uint64 guid)
{
    LfgState state;
    if (IS_GROUP_GUID(guid))
        state = GroupsStore[guid].GetOldState();
    else
        state = PlayersStore[guid].GetOldState();

    TC_LOG_TRACE("lfg", "LFGMgr::GetOldState: [" UI64FMTD "] = %u", guid, state);
    return state;
}

bool LFGMgr::IsVoteKickActive(uint64 gguid)
{
    bool active = GroupsStore[gguid].IsVoteKickActive();
    TC_LOG_TRACE("lfg.data.group.votekick.get", "Group: " UI64FMTD ", Active: %d", gguid, active);

    return active;
}

bool LFGMgr::IsKickCooldownActive(uint64 gguid)
{
    bool active = GroupsStore[gguid].GetVoteKickCooldown() > time(NULL);
    return active;
}

uint32 LFGMgr::GetVoteKickCooldownTimer(uint64 gguid)
{
    return GroupsStore[gguid].GetVoteKickCooldown();
}

void LFGMgr::SetVoteKickCooldown(uint64 guid, uint32 cooldownTime)
{
    GroupsStore[guid].UpdateVoteKickCooldown(cooldownTime);
}

uint32 LFGMgr::GetDungeon(uint64 guid, bool asId /*= true */)
{
    uint32 dungeon = GroupsStore[guid].GetDungeon(asId);
    TC_LOG_TRACE("lfg", "LFGMgr::GetDungeon: [" UI64FMTD "] asId: %u = %u", guid, asId, dungeon);
    return dungeon;
}

uint32 LFGMgr::GetDungeonMapId(uint64 guid)
{
    uint32 dungeonId = GroupsStore[guid].GetDungeon(true);
    uint32 mapId = 0;
    if (dungeonId)
        if (LFGDungeonData const* dungeon = GetLFGDungeon(dungeonId))
            mapId = dungeon->map;

    TC_LOG_TRACE("lfg", "LFGMgr::GetDungeonMapId: [" UI64FMTD "] = %u (DungeonId = %u)", guid, mapId, dungeonId);
    return mapId;
}

uint8 LFGMgr::GetRoles(uint64 guid)
{
    uint8 roles = PlayersStore[guid].GetRoles();
    TC_LOG_TRACE("lfg", "LFGMgr::GetRoles: [" UI64FMTD "] = %u", guid, roles);
    return roles;
}

const std::string& LFGMgr::GetComment(uint64 guid)
{
    TC_LOG_TRACE("lfg", "LFGMgr::GetComment: [" UI64FMTD "] = %s", guid, PlayersStore[guid].GetComment().c_str());
    return PlayersStore[guid].GetComment();
}

LfgDungeonSet const& LFGMgr::GetSelectedDungeons(uint64 guid)
{
    TC_LOG_TRACE("lfg", "LFGMgr::GetSelectedDungeons: [" UI64FMTD "]", guid);
    return PlayersStore[guid].GetSelectedDungeons();
}

LfgLockMap const& LFGMgr::GetLockedDungeons(uint64 guid)
{
    TC_LOG_TRACE("lfg", "LFGMgr::GetLockedDungeons: [" UI64FMTD "]", guid);
    return PlayersStore[guid].GetLockedDungeons();
}

uint8 LFGMgr::GetKicksLeft(uint64 guid)
{
    uint8 kicks = GroupsStore[guid].GetKicksLeft();
    TC_LOG_TRACE("lfg", "LFGMgr::GetKicksLeft: [" UI64FMTD "] = %u", guid, kicks);
    return kicks;
}

void LFGMgr::RestoreState(uint64 guid, char const* debugMsg)
{
    if (IS_GROUP_GUID(guid))
    {
        LfgGroupData& data = GroupsStore[guid];
        if (sLog->ShouldLog("lfg", LOG_LEVEL_DEBUG))
        {
            std::string const& ps = GetStateString(data.GetState());
            std::string const& os = GetStateString(data.GetOldState());
            TC_LOG_TRACE("lfg", "LFGMgr::RestoreState: Group: [" UI64FMTD "] (%s) State: %s, oldState: %s",
                guid, debugMsg, ps.c_str(), os.c_str());
        }

        data.RestoreState();
    }
    else
    {
        LfgPlayerData& data = PlayersStore[guid];
        if (sLog->ShouldLog("lfg", LOG_LEVEL_DEBUG))
        {
            std::string const& ps = GetStateString(data.GetState());
            std::string const& os = GetStateString(data.GetOldState());
            TC_LOG_TRACE("lfg", "LFGMgr::RestoreState: Player: [" UI64FMTD "] (%s) State: %s, oldState: %s",
                guid, debugMsg, ps.c_str(), os.c_str());
        }
        data.RestoreState();
    }
}

void LFGMgr::SetState(uint64 guid, LfgState state)
{
    if (IS_GROUP_GUID(guid))
    {
        LfgGroupData& data = GroupsStore[guid];
        if (sLog->ShouldLog("lfg", LOG_LEVEL_TRACE))
        {
            std::string const& ns = GetStateString(state);
            std::string const& ps = GetStateString(data.GetState());
            std::string const& os = GetStateString(data.GetOldState());
            TC_LOG_TRACE("lfg", "LFGMgr::SetState: Group: [" UI64FMTD "] newState: %s, previous: %s, oldState: %s",
                guid, ns.c_str(), ps.c_str(), os.c_str());
        }
        data.SetState(state);
    }
    else
    {
        LfgPlayerData& data = PlayersStore[guid];
        if (sLog->ShouldLog("lfg", LOG_LEVEL_TRACE))
        {
            std::string const& ns = GetStateString(state);
            std::string const& ps = GetStateString(data.GetState());
            std::string const& os = GetStateString(data.GetOldState());
            TC_LOG_TRACE("lfg", "LFGMgr::SetState: Player: [" UI64FMTD "] newState: %s, previous: %s, oldState: %s",
                guid, ns.c_str(), ps.c_str(), os.c_str());
        }
        data.SetState(state);
    }
}

void LFGMgr::SetVoteKick(uint64 gguid, bool active)
{
    LfgGroupData& data = GroupsStore[gguid];
    TC_LOG_TRACE("lfg.data.group.votekick.set", "Group: " UI64FMTD ", New state: %d, Previous: %d",
        gguid, active, data.IsVoteKickActive());

    data.SetVoteKick(active);
}

void LFGMgr::SetDungeon(uint64 guid, uint32 dungeon)
{
    TC_LOG_TRACE("lfg", "LFGMgr::SetDungeon: [" UI64FMTD "] dungeon %u", guid, dungeon);
    GroupsStore[guid].SetDungeon(dungeon);
}

void LFGMgr::SetRoles(uint64 guid, uint8 roles)
{
    TC_LOG_TRACE("lfg", "LFGMgr::SetRoles: [" UI64FMTD "] roles: %u", guid, roles);
    PlayersStore[guid].SetRoles(roles);
}

void LFGMgr::SetComment(uint64 guid, std::string const& comment)
{
    TC_LOG_TRACE("lfg", "LFGMgr::SetComment: [" UI64FMTD "] comment: %s", guid, comment.c_str());
    PlayersStore[guid].SetComment(comment);
}

void LFGMgr::SetSelectedDungeons(uint64 guid, LfgDungeonSet const& dungeons)
{
    TC_LOG_TRACE("lfg", "LFGMgr::SetSelectedDungeons: [" UI64FMTD "] Dungeons: %s", guid, ConcatenateDungeons(dungeons).c_str());
    PlayersStore[guid].SetSelectedDungeons(dungeons);
}

void LFGMgr::SetLockedDungeons(uint64 guid, LfgLockMap const& lock)
{
    TC_LOG_TRACE("lfg", "LFGMgr::SetLockedDungeons: [" UI64FMTD "]", guid);
    PlayersStore[guid].SetLockedDungeons(lock);
}

void LFGMgr::DecreaseKicksLeft(uint64 guid)
{
    TC_LOG_TRACE("lfg", "LFGMgr::DecreaseKicksLeft: [" UI64FMTD "]", guid);
    GroupsStore[guid].DecreaseKicksLeft();
}

void LFGMgr::RemovePlayerData(uint64 guid)
{
    TC_LOG_TRACE("lfg", "LFGMgr::RemovePlayerData: [" UI64FMTD "]", guid);
    LfgPlayerDataContainer::iterator it = PlayersStore.find(guid);
    if (it != PlayersStore.end())
        PlayersStore.erase(it);
}

void LFGMgr::RemoveGroupData(uint64 guid)
{
    TC_LOG_TRACE("lfg", "LFGMgr::RemoveGroupData: [" UI64FMTD "]", guid);
    LfgGroupDataContainer::iterator it = GroupsStore.find(guid);
    if (it == GroupsStore.end())
        return;

    LfgState state = GetState(guid);
    // If group is being formed after proposal success do nothing more
    LfgGuidSet const& players = it->second.GetPlayers();
    for (auto it = players.begin(); it != players.end(); ++it)
    {
        uint64 guid = (*it);
        SetGroup(*it, 0);
        if (state != LFG_STATE_PROPOSAL)
        {
            SetState(*it, LFG_STATE_NONE);
            SendLfgUpdateStatus(guid, LfgUpdateData(LFG_UPDATETYPE_REMOVED_FROM_QUEUE), true);
        }
    }
    GroupsStore.erase(it);
}

uint8 LFGMgr::GetTeam(uint64 guid)
{
    return PlayersStore[guid].GetTeam();
}

uint8 LFGMgr::RemovePlayerFromGroup(uint64 gguid, uint64 guid)
{
    return GroupsStore[gguid].RemovePlayer(guid);
}

void LFGMgr::AddPlayerToGroup(uint64 gguid, uint64 guid)
{
    GroupsStore[gguid].AddPlayer(guid);
}

void LFGMgr::SetIsPremade(uint64 guid, bool set)
{
    PlayersStore[guid].SetIsPremade(set);
}

bool LFGMgr::GetIsPremade(uint64 guid)
{
    return PlayersStore[guid].GetIsPremade();
}

void LFGMgr::SetLeader(uint64 gguid, uint64 leader)
{
    GroupsStore[gguid].SetLeader(leader);
}

void LFGMgr::SetTeam(uint64 guid, uint8 team)
{
    if (sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GROUP) || sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_LFG))
        team = 0;

    PlayersStore[guid].SetTeam(team);
}

uint64 LFGMgr::GetGroup(uint64 guid)
{
    return PlayersStore[guid].GetGroup();
}

void LFGMgr::SetGroup(uint64 guid, uint64 group)
{
    PlayersStore[guid].SetGroup(group);
}

LfgGuidSet const& LFGMgr::GetPlayers(uint64 guid)
{
    return GroupsStore[guid].GetPlayers();
}

uint8 LFGMgr::GetPlayerCount(uint64 guid)
{
    return GroupsStore[guid].GetPlayerCount();
}

uint64 LFGMgr::GetLeader(uint64 guid)
{
    return GroupsStore[guid].GetLeader();
}

bool LFGMgr::HasIgnore(uint64 guid1, uint64 guid2)
{
    Player* plr1 = ObjectAccessor::FindPlayer(guid1);
    Player* plr2 = ObjectAccessor::FindPlayer(guid2);
    uint32 low1 = GUID_LOPART(guid1);
    uint32 low2 = GUID_LOPART(guid2);
    return plr1 && plr2 && (plr1->GetSocial()->HasIgnore(low2) || plr2->GetSocial()->HasIgnore(low1));
}

void LFGMgr::SendLfgRoleChosen(uint64 guid, uint64 pguid, uint8 roles)
{
    if (Player* player = ObjectAccessor::FindPlayer(guid))
        player->GetSession()->SendLfgRoleChosen(pguid, roles);
}

void LFGMgr::SendLfgRoleCheckUpdate(uint64 guid, LfgRoleCheck const& roleCheck)
{
    if (Player* player = ObjectAccessor::FindPlayer(guid))
        player->GetSession()->SendLfgRoleCheckUpdate(roleCheck);
}

void LFGMgr::SendLfgUpdateStatus(uint64 guid, LfgUpdateData const& data, bool party)
{
    if (Player* player = ObjectAccessor::FindPlayer(guid))
        player->GetSession()->SendLfgUpdateStatus(data, party);
}

void LFGMgr::SendLfgJoinResult(uint64 guid, LfgJoinResultData const& data)
{
    if (Player* player = ObjectAccessor::FindPlayer(guid))
        player->GetSession()->SendLfgJoinResult(data);
}

void LFGMgr::SendLfgBootProposalUpdate(uint64 guid, LfgPlayerBoot const& boot)
{
    if (Player* player = ObjectAccessor::FindPlayer(guid))
        player->GetSession()->SendLfgBootProposalUpdate(boot);
}

void LFGMgr::SendLfgUpdateProposal(uint64 guid, LfgProposal const& proposal)
{
    if (Player* player = ObjectAccessor::FindPlayer(guid))
        player->GetSession()->SendLfgUpdateProposal(proposal);
}

void LFGMgr::SendLfgQueueStatus(uint64 guid, LfgQueueStatusData const& data)
{
    if (Player* player = ObjectAccessor::FindPlayer(guid))
        player->GetSession()->SendLfgQueueStatus(data);
}

bool LFGMgr::IsLfgGroup(uint64 guid)
{
    return guid && IS_GROUP_GUID(guid) && GroupsStore[guid].IsLfgGroup();
}

uint8 LFGMgr::GetQueueId(uint64 guid)
{
    if (IS_GROUP_GUID(guid))
    {
        LfgGuidSet const& players = GetPlayers(guid);
        uint64 pguid = players.empty() ? 0 : (*players.begin());
        if (pguid)
            return GetTeam(pguid);
    }

    return GetTeam(guid);
}

LFGQueue& LFGMgr::GetQueue(uint64 guid)
{
    uint8 queueId = GetQueueId(guid);
    return QueuesStore[queueId];
}

bool LFGMgr::AllQueued(LfgGuidList const& check)
{
    if (check.empty())
        return false;

    for (auto it = check.begin(); it != check.end(); ++it)
        if (GetState(*it) != LFG_STATE_QUEUED)
            return false;
    return true;
}

time_t LFGMgr::GetQueueJoinTime(uint64 guid)
{
    uint8 queueId = GetQueueId(guid);
    auto itr = QueuesStore.find(queueId);
    if (itr != QueuesStore.end())
        return itr->second.GetJoinTime(guid);

    return 0;
}

// Only for debugging purposes
void LFGMgr::Clean()
{
    QueuesStore.clear();
}

bool LFGMgr::isOptionEnabled(uint32 option)
{
    return m_options & option;
}

uint32 LFGMgr::GetOptions()
{
    return m_options;
}

void LFGMgr::SetOptions(uint32 options)
{
    m_options = options;
}

LfgUpdateData LFGMgr::GetLfgStatus(uint64 guid)
{
    LfgPlayerData& playerData = PlayersStore[guid];
    return LfgUpdateData(LFG_UPDATETYPE_UPDATE_STATUS, playerData.GetState(), playerData.GetSelectedDungeons());
}

bool LFGMgr::IsSeasonActive(uint32 dungeonId)
{
    switch (dungeonId)
    {
        case 285: // The Headless Horseman
            return IsHolidayActive(HOLIDAY_HALLOWS_END);
        case 286: // The Frost Lord Ahune
            return IsHolidayActive(HOLIDAY_FIRE_FESTIVAL);
        case 287: // Coren Direbrew
            return IsHolidayActive(HOLIDAY_BREWFEST);
        case 288: // The Crown Chemical Co.
            return IsHolidayActive(HOLIDAY_LOVE_IS_IN_THE_AIR);
    }
    return false;
}

std::string LFGMgr::DumpQueueInfo(bool full)
{
    uint32 size = uint32(QueuesStore.size());
    std::ostringstream o;

    o << "Number of Queues: " << size << "\n";
    for (auto itr = QueuesStore.begin(); itr != QueuesStore.end(); ++itr)
    {
        std::string const& queued = itr->second.DumpQueueInfo();
        std::string const& compatibles = itr->second.DumpCompatibleInfo(full);
        o << queued << compatibles;
    }

    return o.str();
}

void LFGMgr::SetupGroupMember(uint64 guid, uint64 gguid)
{
    LfgDungeonSet dungeons;
    dungeons.insert(GetDungeon(gguid));
    SetSelectedDungeons(guid, dungeons);
    SetState(guid, GetState(gguid));
    SetGroup(guid, gguid);
    AddPlayerToGroup(gguid, guid);
}

bool LFGMgr::selectedRandomLfgDungeon(uint64 guid)
{
    if (GetState(guid) != LFG_STATE_NONE)
    {
        LfgDungeonSet const& dungeons = GetSelectedDungeons(guid);
        if (!dungeons.empty())
        {
             LFGDungeonData const* dungeon = GetLFGDungeon(*dungeons.begin());
             if (dungeon && (dungeon->type == LFG_TYPE_RANDOM || dungeon->seasonal))
                 return true;
        }
    }

    return false;
}

bool LFGMgr::inLfgDungeonMap(uint64 guid, uint32 map, Difficulty difficulty)
{
    if (!IS_GROUP_GUID(guid))
        guid = GetGroup(guid);

    if (uint32 dungeonId = GetDungeon(guid, true))
        if (LFGDungeonData const* dungeon = GetLFGDungeon(dungeonId))
            if (uint32(dungeon->map) == map && dungeon->difficulty == difficulty)
                return true;

    return false;
}

uint32 LFGMgr::GetLFGDungeonEntry(uint32 id)
{
    if (id)
        if (LFGDungeonData const* dungeon = GetLFGDungeon(id))
            return dungeon->Entry();

    return 0;
}

LfgDungeonSet LFGMgr::GetRandomAndSeasonalDungeons(uint8 level, uint8 expansion)
{
    LfgDungeonSet randomDungeons;
    for (auto itr = LfgDungeonStore.begin(); itr != LfgDungeonStore.end(); ++itr)
    {
        lfg::LFGDungeonData const& dungeon = itr->second;
        if ((dungeon.type == lfg::LFG_TYPE_RANDOM || (dungeon.seasonal && sLFGMgr->IsSeasonActive(dungeon.id)) || dungeon.isLFR)
            && dungeon.expansion <= expansion && dungeon.minlevel <= level && level <= dungeon.maxlevel)
            randomDungeons.insert(dungeon.Entry());
    }
    return randomDungeons;
}

} // namespace lfg
