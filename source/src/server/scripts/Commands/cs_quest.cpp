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

/* ScriptData
Name: quest_commandscript
%Complete: 100
Comment: All quest related commands
Category: commandscripts
EndScriptData */

#include "Chat.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ReputationMgr.h"
#include "ScriptMgr.h"

class quest_commandscript : public CommandScript
{
public:
    quest_commandscript() : CommandScript("quest_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> questAutoCompleteTable =
        {
            { "add",            SEC_CONSOLE,  false, &HandleQuestAutoSet,                  "" },
            { "reset",          SEC_CONSOLE,  false, &HandleQuestAutoUnset,                 "" },
        };

        static std::vector<ChatCommand> questCommandTable =
        {
            { "add",            SEC_CONSOLE,  false, &HandleQuestAdd,                    "" },
            { "complete",       SEC_CONSOLE,  false, &HandleQuestComplete,               "" },
            { "remove",         SEC_CONSOLE,  false, &HandleQuestRemove,                 "" },
            { "reward",         SEC_CONSOLE,  false, &HandleQuestReward,                 "" },
            { "autocomplete",   SEC_CONSOLE,  false, &HandleQuestReward,                 "", questAutoCompleteTable },
        };

        static std::vector<ChatCommand> commandTable =
        {
            { "quest",          SEC_CONSOLE,  false, NULL,                  "", questCommandTable },
        };
        return commandTable;
    }

    static bool HandleQuestAdd(ChatHandler* handler, const char* args)
    {
        Player* player = handler->getSelectedPlayer();
        if (!player)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // .addquest #entry'
        // number or [name] Shift-click form |color|Hquest:quest_id:quest_level|h[name]|h|r
        char* cId = handler->extractKeyFromLink((char*)args, "Hquest");
        if (!cId)
            return false;

        uint32 entry = atol(cId);

        Quest const* quest = sObjectMgr->GetQuestTemplate(entry);

        if (!quest)
        {
            handler->PSendSysMessage(LANG_COMMAND_QUEST_NOTFOUND, entry);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // check item starting quest (it can work incorrectly if added without item in inventory)
        ItemTemplateContainer const* itc = sObjectMgr->GetItemTemplateStore();
        ItemTemplateContainer::const_iterator result = find_if(itc->begin(), itc->end(), Finder<uint32, ItemTemplate>(entry, &ItemTemplate::StartQuest));

        if (result != itc->end())
        {
            handler->PSendSysMessage(LANG_COMMAND_QUEST_STARTFROMITEM, entry, result->second.ItemId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // ok, normal (creature/GO starting) quest
        if (player->CanAddQuest(quest, true))
        {
            player->AddQuest(quest, NULL);
            handler->PSendSysMessage(LANG_COMMAND_QUEST_ADD_NO_LOOKUP, entry, player->GetName().c_str());

            if (player->CanCompleteQuest(entry))
                player->CompleteQuest(entry);
        }

        return true;
    }

    static bool HandleQuestRemove(ChatHandler* handler, const char* args)
    {
        Player* player = handler->getSelectedPlayer();
        if (!player)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // .removequest #entry'
        // number or [name] Shift-click form |color|Hquest:quest_id:quest_level|h[name]|h|r
        char* cId = handler->extractKeyFromLink((char*)args, "Hquest");
        if (!cId)
            return false;

        uint32 entry = atol(cId);

        Quest const* quest = sObjectMgr->GetQuestTemplate(entry);

        if (!quest)
        {
            handler->PSendSysMessage(LANG_COMMAND_QUEST_NOTFOUND, entry);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // remove all quest entries for 'entry' from quest log
        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            uint32 logQuest = player->GetQuestSlotQuestId(slot);
            if (logQuest == entry)
            {
                player->SetQuestSlot(slot, 0);

                // we ignore unequippable quest items in this case, its' still be equipped
                player->TakeQuestSourceItem(logQuest, false);
            }
        }

        player->RemoveActiveQuest(entry);
        player->RemoveRewardedQuest(entry);

        handler->PSendSysMessage(LANG_COMMAND_QUEST_REMOVED, entry, player->GetName().c_str());
        return true;
    }

    static bool HandleQuestComplete(ChatHandler* handler, const char* args)
    {
        Player* player = handler->getSelectedPlayer();
        if (!player)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // .quest complete #entry
        // number or [name] Shift-click form |color|Hquest:quest_id:quest_level|h[name]|h|r
        char* cId = handler->extractKeyFromLink((char*)args, "Hquest");
        if (!cId)
            return false;

        uint32 entry = atol(cId);

        Quest const* quest = sObjectMgr->GetQuestTemplate(entry);

        // If player doesn't have the quest
        if (!quest || player->GetQuestStatus(entry) == QUEST_STATUS_NONE)
        {
            handler->PSendSysMessage(LANG_COMMAND_QUEST_NOTFOUND, entry);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // Add quest items for quests that require items
        for (uint8 x = 0; x < QUEST_ITEM_OBJECTIVES_COUNT; ++x)
        {
            uint32 id = quest->RequiredItemId[x];
            uint32 count = quest->RequiredItemCount[x];
            if (!id || !count)
                continue;

            uint32 curItemCount = player->GetItemCount(id, true);

            ItemPosCountVec dest;
            uint8 msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, id, count - curItemCount);
            if (msg == EQUIP_ERR_OK)
            {
                Item* item = player->StoreNewItem(dest, id, true);
                player->SendNewItem(item, count - curItemCount, true, false);
            }
        }

        // All creature/GO slain/casted (not required, but otherwise it will display "Creature slain 0/10")
        for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        {
            int32 creature = quest->RequiredNpcOrGo[i];
            uint32 creaturecount = quest->RequiredNpcOrGoCount[i];

            if (uint32 spell_id = quest->RequiredSpellCast[i])
            {
                for (uint16 z = 0; z < creaturecount; ++z)
                    player->CastedCreatureOrGO(creature, 0, spell_id);
            }
            else if (creature > 0)
            {
                if (CreatureTemplate const* cInfo = sObjectMgr->GetCreatureTemplate(creature))
                    for (uint16 z = 0; z < creaturecount; ++z)
                        player->KilledMonster(cInfo, 0);
            }
            else if (creature < 0)
            {
                for (uint16 z = 0; z < creaturecount; ++z)
                    player->CastedCreatureOrGO(creature, 0, 0);
            }
        }

        // If the quest requires reputation to complete
        if (uint32 repFaction = quest->GetRepObjectiveFaction())
        {
            uint32 repValue = quest->GetRepObjectiveValue();
            uint32 curRep = player->GetReputationMgr().GetReputation(repFaction);
            if (curRep < repValue)
                if (FactionEntry const* factionEntry = sFactionStore.LookupEntry(repFaction))
                    player->GetReputationMgr().SetReputation(factionEntry, repValue);
        }

        // If the quest requires a SECOND reputation to complete
        if (uint32 repFaction = quest->GetRepObjectiveFaction2())
        {
            uint32 repValue2 = quest->GetRepObjectiveValue2();
            uint32 curRep = player->GetReputationMgr().GetReputation(repFaction);
            if (curRep < repValue2)
                if (FactionEntry const* factionEntry = sFactionStore.LookupEntry(repFaction))
                    player->GetReputationMgr().SetReputation(factionEntry, repValue2);
        }

        // If the quest requires money
        int32 ReqOrRewMoney = quest->GetRewOrReqMoney();
        if (ReqOrRewMoney < 0)
            player->ModifyMoney(-ReqOrRewMoney);

        player->CompleteQuest(entry);
        handler->PSendSysMessage(LANG_COMMAND_QUEST_COMPLETE_NO_LOOKUP, entry, player->GetName().c_str());
        return true;
    }

    static bool HandleQuestReward(ChatHandler* handler, char const* args)
    {
        Player* player = handler->getSelectedPlayer();
        if (!player)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // .quest reward #entry
        // number or [name] Shift-click form |color|Hquest:quest_id:quest_level|h[name]|h|r
        char* cId = handler->extractKeyFromLink((char*)args, "Hquest");
        if (!cId)
            return false;

        uint32 entry = atol(cId);

        Quest const* quest = sObjectMgr->GetQuestTemplate(entry);

        // If player doesn't have the quest
        if (!quest || player->GetQuestStatus(entry) != QUEST_STATUS_COMPLETE)
        {
            handler->PSendSysMessage(LANG_COMMAND_QUEST_NOTFOUND, entry);
            handler->SetSentErrorMessage(true);
            return false;
        }

        player->RewardQuest(quest, 0, player);
        handler->PSendSysMessage(LANG_COMMAND_QUEST_REWARDED_NO_LOOKUP, entry, player->GetName().c_str());
        return true;
    }

    static bool HandleQuestAutoSet(ChatHandler* handler, const char* args)
    {
        char* cId = handler->extractKeyFromLink((char*)args, "Hquest");
        if (!cId)
            return false;

        uint32 entry = atol(cId);

        Quest const* quest = sObjectMgr->GetQuestTemplate(entry);

        if (!quest)
        {
            handler->PSendSysMessage(LANG_COMMAND_QUEST_NOTFOUND, entry);
            handler->SetSentErrorMessage(true);
            return false;
        }

        WorldDatabase.PExecute("UPDATE quest_template SET `Method` = 0 WHERE ID IN (%u);", entry);
        sObjectMgr->LoadQuests();
        handler->PSendSysMessage("Quest %u has been set to automatic completion, quest template has been reloaded.", entry);

        return true;
    }

    static bool HandleQuestAutoUnset(ChatHandler* handler, const char* args)
    {
        char* cId = handler->extractKeyFromLink((char*)args, "Hquest");
        if (!cId)
            return false;

        uint32 entry = atol(cId);

        Quest const* quest = sObjectMgr->GetQuestTemplate(entry);

        if (!quest)
        {
            handler->PSendSysMessage(LANG_COMMAND_QUEST_NOTFOUND, entry);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (auto result = WorldDatabase.PQuery("SELECT `Method` FROM quest_template_perma WHERE ID IN (%u)", entry))
            if (auto fields2 = result->Fetch())
            {
                uint32 method{ fields2[0].GetUInt32() };
                WorldDatabase.PExecute("UPDATE quest_template SET `Method` = (%u) WHERE ID IN (%u);", method, entry);
                sObjectMgr->LoadQuests();
                handler->PSendSysMessage("Quest %u has been set to Method %u, (0 is auto complete), quest template has been reloaded.", entry, method);
                return true;
            }
            else
            {
                handler->PSendSysMessage("Database data could not find permenant template for %u", entry);
                handler->SetSentErrorMessage(true);
                return false;
            }
        else
        {
            handler->PSendSysMessage("Database data could not find permenant template for %u", entry);
            handler->SetSentErrorMessage(true);
            return false;
        }
        return false;
    }
};

void AddSC_quest_commandscript()
{
    new quest_commandscript();
}
