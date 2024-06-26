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
Name: reload_commandscript
%Complete: 100
Comment: All reload related commands
Category: commandscripts
EndScriptData */

#include "AchievementMgr.h"
#include "AuctionHouseMgr.h"
#include "Chat.h"
#include "CreatureTextMgr.h"
#include "DisableMgr.h"
#include "Language.h"
#include "LFGMgr.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "ScriptMgr.h"
#include "SkillDiscovery.h"
#include "SkillExtraItems.h"
#include "SmartAI.h"
#include "SpellMgr.h"
#include "TicketMgr.h"
#include "WardenCheckMgr.h"
#include "WaypointManager.h"
#include "PerformanceLog.h"

class reload_commandscript : public CommandScript
{
public:
    reload_commandscript() : CommandScript("reload_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> reloadAllCommandTable =
        {
            { "achievement", SEC_CONSOLE,  true,  &HandleReloadAllAchievementCommand, "" },
            { "area",       SEC_CONSOLE,  true,  &HandleReloadAllAreaCommand,       "" },
            { "gossips",    SEC_CONSOLE,  true,  &HandleReloadAllGossipsCommand,    "" },
            { "item",       SEC_CONSOLE,  true,  &HandleReloadAllItemCommand,       "" },
            { "locales",    SEC_CONSOLE,  true,  &HandleReloadAllLocalesCommand,    "" },
            { "loot",       SEC_CONSOLE,  true,  &HandleReloadAllLootCommand,       "" },
            { "npc",        SEC_CONSOLE,  true,  &HandleReloadAllNpcCommand,        "" },
            { "quest",      SEC_CONSOLE,  true,  &HandleReloadAllQuestCommand,      "" },
            { "scripts",    SEC_CONSOLE,  true,  &HandleReloadAllScriptsCommand,    "" },
            { "spell",      SEC_CONSOLE,  true,  &HandleReloadAllSpellCommand,      "" },
            { "",           SEC_CONSOLE,  true,  &HandleReloadAllCommand,           "" },
        };

        static std::vector<ChatCommand> reloadCommandTable =
        {
            { "auctions",                     SEC_CONSOLE, true,  &HandleReloadAuctionsCommand,                   "" },
            { "access_requirement",           SEC_CONSOLE, true,  &HandleReloadAccessRequirementCommand,          "" },
            { "achievement_criteria_data",    SEC_CONSOLE, true,  &HandleReloadAchievementCriteriaDataCommand,    "" },
            { "achievement_reward",           SEC_CONSOLE, true,  &HandleReloadAchievementRewardCommand,          "" },
            { "all",                          SEC_CONSOLE, true,  NULL,                          "", reloadAllCommandTable },
            { "areatrigger_involvedrelation", SEC_CONSOLE, true,  &HandleReloadQuestAreaTriggersCommand,          "" },
            { "areatrigger_tavern",           SEC_CONSOLE, true,  &HandleReloadAreaTriggerTavernCommand,          "" },
            { "areatrigger_teleport",         SEC_CONSOLE, true,  &HandleReloadAreaTriggerTeleportCommand,        "" },
            { "autobroadcast",                SEC_CONSOLE, true,  &HandleReloadAutobroadcastCommand,              "" },
            { "broadcast_text",               SEC_CONSOLE, true,  &HandleReloadBroadcastTextCommand,              "" },
            { "command",                      SEC_CONSOLE, true,  &HandleReloadCommandCommand,                    "" },
            { "conditions",                   SEC_CONSOLE, true,  &HandleReloadConditions,                        "" },
            { "config",                       SEC_CONSOLE, true,  &HandleReloadConfigCommand,                     "" },
            { "creature_text",                SEC_CONSOLE, true,  &HandleReloadCreatureText,                      "" },
            { "creature_involvedrelation",    SEC_CONSOLE, true,  &HandleReloadCreatureQuestInvRelationsCommand,  "" },
            { "creature_linked_respawn",      SEC_CONSOLE,    true,  &HandleReloadLinkedRespawnCommand,              "" },
            { "creature_loot_template",       SEC_CONSOLE, true,  &HandleReloadLootTemplatesCreatureCommand,      "" },
            { "creature_onkill_reward",       SEC_CONSOLE, true,  &HandleReloadOnKillRewardCommand,               "" },
            { "creature_questrelation",       SEC_CONSOLE, true,  &HandleReloadCreatureQuestRelationsCommand,     "" },
            { "creature_summon_groups",       SEC_CONSOLE, true,  &HandleReloadCreatureSummonGroupsCommand,       "" },
            { "creature_template",            SEC_CONSOLE, true,  &HandleReloadCreatureTemplateCommand,           "" },
            //{ "db_script_string",             SEC_CONSOLE, true,  &HandleReloadDbScriptStringCommand,            "" },
            { "disables",                     SEC_CONSOLE, true,  &HandleReloadDisablesCommand,                   "" },
            { "disenchant_loot_template",     SEC_CONSOLE, true,  &HandleReloadLootTemplatesDisenchantCommand,    "" },
            { "event_scripts",                SEC_CONSOLE, true,  &HandleReloadEventScriptsCommand,               "" },
            { "fishing_loot_template",        SEC_CONSOLE, true,  &HandleReloadLootTemplatesFishingCommand,       "" },
            { "game_graveyard_zone",          SEC_CONSOLE, true,  &HandleReloadGameGraveyardZoneCommand,          "" },
            { "game_tele",                    SEC_CONSOLE, true,  &HandleReloadGameTeleCommand,                   "" },
            { "gameobject_involvedrelation",  SEC_CONSOLE, true,  &HandleReloadGOQuestInvRelationsCommand,        "" },
            { "gameobject_loot_template",     SEC_CONSOLE, true,  &HandleReloadLootTemplatesGameobjectCommand,    "" },
            { "gameobject_questrelation",     SEC_CONSOLE, true,  &HandleReloadGOQuestRelationsCommand,           "" },
            { "gm_tickets",                   SEC_CONSOLE, true,  &HandleReloadGMTicketsCommand,                  "" },
            { "gossip_menu",                  SEC_CONSOLE, true,  &HandleReloadGossipMenuCommand,                 "" },
            { "gossip_menu_option",           SEC_CONSOLE, true,  &HandleReloadGossipMenuOptionCommand,           "" },
            { "item_enchantment_template",    SEC_CONSOLE, true,  &HandleReloadItemEnchantementsCommand,          "" },
            { "item_loot_template",           SEC_CONSOLE, true,  &HandleReloadLootTemplatesItemCommand,          "" },
            { "lfg_dungeon_rewards",          SEC_CONSOLE, true,  &HandleReloadLfgRewardsCommand,                 "" },
            { "locales_achievement_reward",   SEC_CONSOLE, true,  &HandleReloadLocalesAchievementRewardCommand,   "" },
            { "locales_creature",             SEC_CONSOLE, true,  &HandleReloadLocalesCreatureCommand,            "" },
            { "locales_creature_text",        SEC_CONSOLE, true,  &HandleReloadLocalesCreatureTextCommand,        "" },
            { "locales_gameobject",           SEC_CONSOLE, true,  &HandleReloadLocalesGameobjectCommand,          "" },
            { "locales_gossip_menu_option",   SEC_CONSOLE, true,  &HandleReloadLocalesGossipMenuOptionCommand,    "" },
            { "locales_item",                 SEC_CONSOLE, true,  &HandleReloadLocalesItemCommand,                "" },
            { "locales_npc_text",             SEC_CONSOLE, true,  &HandleReloadLocalesNpcTextCommand,             "" },
            { "locales_page_text",            SEC_CONSOLE, true,  &HandleReloadLocalesPageTextCommand,            "" },
            { "locales_points_of_interest",   SEC_CONSOLE, true,  &HandleReloadLocalesPointsOfInterestCommand,    "" },
            { "locales_quest",                SEC_CONSOLE, true,  &HandleReloadLocalesQuestCommand,               "" },
            { "mail_level_reward",            SEC_CONSOLE, true,  &HandleReloadMailLevelRewardCommand,            "" },
            { "mail_loot_template",           SEC_CONSOLE, true,  &HandleReloadLootTemplatesMailCommand,          "" },
            { "milling_loot_template",        SEC_CONSOLE, true,  &HandleReloadLootTemplatesMillingCommand,       "" },
            { "npc_spellclick_spells",        SEC_CONSOLE, true,  &HandleReloadSpellClickSpellsCommand,           "" },
            { "npc_trainer",                  SEC_CONSOLE, true,  &HandleReloadNpcTrainerCommand,                 "" },
            { "npc_vendor",                   SEC_CONSOLE, true,  &HandleReloadNpcVendorCommand,                  "" },
            { "page_text",                    SEC_CONSOLE, true,  &HandleReloadPageTextsCommand,                  "" },
            { "phasedefinitions",             SEC_CONSOLE, true,  &HandleReloadPhaseDefinitionsCommand,           "" },
            { "pickpocketing_loot_template",  SEC_CONSOLE, true,  &HandleReloadLootTemplatesPickpocketingCommand, "" },
            { "points_of_interest",           SEC_CONSOLE, true,  &HandleReloadPointsOfInterestCommand,           "" },
            { "prospecting_loot_template",    SEC_CONSOLE, true,  &HandleReloadLootTemplatesProspectingCommand,   "" },
            { "quest_poi",                    SEC_CONSOLE, true,  &HandleReloadQuestPOICommand,                   "" },
            { "quest_template",               SEC_CONSOLE, true,  &HandleReloadQuestTemplateCommand,              "" },
            { "reference_loot_template",      SEC_CONSOLE, true,  &HandleReloadLootTemplatesReferenceCommand,     "" },
            { "reserved_name",                SEC_CONSOLE, true,  &HandleReloadReservedNameCommand,               "" },
            { "reputation_reward_rate",       SEC_CONSOLE, true,  &HandleReloadReputationRewardRateCommand,       "" },
            { "reputation_spillover_template", SEC_CONSOLE, true,  &HandleReloadReputationRewardRateCommand,       ""},
            { "skill_discovery_template",     SEC_CONSOLE, true,  &HandleReloadSkillDiscoveryTemplateCommand,     "" },
            { "skill_extra_item_template",    SEC_CONSOLE, true,  &HandleReloadSkillExtraItemTemplateCommand,     "" },
            { "skill_fishing_base_level",     SEC_CONSOLE, true,  &HandleReloadSkillFishingBaseLevelCommand,      "" },
            { "skinning_loot_template",       SEC_CONSOLE, true,  &HandleReloadLootTemplatesSkinningCommand,      "" },
            { "smart_scripts",                SEC_CONSOLE, true,  &HandleReloadSmartScripts,                      "" },
            { "spell_required",               SEC_CONSOLE, true,  &HandleReloadSpellRequiredCommand,              "" },
            { "spell_area",                   SEC_CONSOLE, true,  &HandleReloadSpellAreaCommand,                  "" },
            { "spell_bonus_data",             SEC_CONSOLE, true,  &HandleReloadSpellBonusesCommand,               "" },
            { "spell_group",                  SEC_CONSOLE, true,  &HandleReloadSpellGroupsCommand,                "" },
            { "spell_learn_spell",            SEC_CONSOLE, true,  &HandleReloadSpellLearnSpellCommand,            "" },
            { "spell_loot_template",          SEC_CONSOLE, true,  &HandleReloadLootTemplatesSpellCommand,         "" },
            { "spell_linked_spell",           SEC_CONSOLE, true,  &HandleReloadSpellLinkedSpellCommand,           "" },
            { "spell_pet_auras",              SEC_CONSOLE, true,  &HandleReloadSpellPetAurasCommand,              "" },
            { "spell_proc_event",             SEC_CONSOLE, true,  &HandleReloadSpellProcEventCommand,             "" },
            { "spell_proc",                   SEC_CONSOLE, true,  &HandleReloadSpellProcsCommand,                 "" },
            { "spell_scripts",                SEC_CONSOLE, true,  &HandleReloadSpellScriptsCommand,               "" },
            { "spell_target_position",        SEC_CONSOLE, true,  &HandleReloadSpellTargetPositionCommand,        "" },
            { "spell_threats",                SEC_CONSOLE, true,  &HandleReloadSpellThreatsCommand,               "" },
            { "spell_group_stack_rules",      SEC_CONSOLE, true,  &HandleReloadSpellGroupStackRulesCommand,       "" },
            { "instance_teleporter_dest",     SEC_CONSOLE, true,  &HandleReloadInstanceTeleporterDestCommand,     "" },
            { "trinity_string",               SEC_CONSOLE, true,  &HandleReloadTrinityStringCommand,              "" },
            { "warden_action",                SEC_CONSOLE, true,  &HandleReloadWardenactionCommand,               "" },
            { "waypoint_scripts",             SEC_CONSOLE, true,  &HandleReloadWpScriptsCommand,                  "" },
            { "waypoint_data",                SEC_CONSOLE, true,  &HandleReloadWpCommand,                         "" },
            { "vehicle_accessory",            SEC_CONSOLE, true,  &HandleReloadVehicleAccessoryCommand,           "" },
            { "vehicle_template_accessory",   SEC_CONSOLE, true,  &HandleReloadVehicleTemplateAccessoryCommand,   "" },
            { "performanceconfig",            SEC_CONSOLE, true,  &HandleReloadPerfConfigCommand,                 "" },
            { "template_npc",				  SEC_CONSOLE, true,	&HandleReloadTemplateNpcCommand,				"" },
        };
        static std::vector<ChatCommand> commandTable =
        {
            { "reload",         SEC_CONSOLE,  true,  NULL,                 "", reloadCommandTable },
        };
        return commandTable;
    }

    //reload commands
    static bool HandleReloadGMTicketsCommand(ChatHandler* /*handler*/, const char* /*args*/)
    {
        sTicketMgr->LoadTickets();
        return true;
    }

    static bool HandleReloadAllCommand(ChatHandler* handler, const char* /*args*/)
    {
        HandleReloadSkillFishingBaseLevelCommand(handler, "");

        HandleReloadAllAchievementCommand(handler, "");
        HandleReloadAllAreaCommand(handler, "");
        HandleReloadAllLootCommand(handler, "");
        HandleReloadAllNpcCommand(handler, "");
        HandleReloadAllQuestCommand(handler, "");
        HandleReloadAllSpellCommand(handler, "");
        HandleReloadAllItemCommand(handler, "");
        HandleReloadAllGossipsCommand(handler, "");
        HandleReloadAllLocalesCommand(handler, "");

        HandleReloadAccessRequirementCommand(handler, "");
        HandleReloadMailLevelRewardCommand(handler, "");
        HandleReloadCommandCommand(handler, "");
        HandleReloadReservedNameCommand(handler, "");
        HandleReloadTrinityStringCommand(handler, "");
        HandleReloadGameTeleCommand(handler, "");

        HandleReloadCreatureSummonGroupsCommand(handler, "");

        HandleReloadVehicleAccessoryCommand(handler, "");
        HandleReloadVehicleTemplateAccessoryCommand(handler, "");

        HandleReloadAutobroadcastCommand(handler, "");
        HandleReloadInstanceTeleporterDestCommand(handler, "");

        HandleReloadBroadcastTextCommand(handler, "");
        return true;
    }

    static bool HandleReloadAllAchievementCommand(ChatHandler* handler, const char* /*args*/)
    {
        HandleReloadAchievementCriteriaDataCommand(handler, "");
        HandleReloadAchievementRewardCommand(handler, "");
        return true;
    }

    static bool HandleReloadAllAreaCommand(ChatHandler* handler, const char* /*args*/)
    {
        //HandleReloadQuestAreaTriggersCommand(handler, ""); -- reloaded in HandleReloadAllQuestCommand
        HandleReloadAreaTriggerTeleportCommand(handler, "");
        HandleReloadAreaTriggerTavernCommand(handler, "");
        HandleReloadGameGraveyardZoneCommand(handler, "");
        return true;
    }

    static bool HandleReloadAllLootCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Loot Tables...");
        LoadLootTables();
        handler->SendGlobalGMSysMessage("DB tables `*_loot_template` reloaded.");
        sConditionMgr->LoadConditions(true);
        return true;
    }

    static bool HandleReloadAllNpcCommand(ChatHandler* handler, const char* args)
    {
        if (*args != 'a')                                          // will be reloaded from all_gossips
        HandleReloadNpcTrainerCommand(handler, "a");
        HandleReloadNpcVendorCommand(handler, "a");
        HandleReloadPointsOfInterestCommand(handler, "a");
        HandleReloadSpellClickSpellsCommand(handler, "a");
        return true;
    }

    static bool HandleReloadAllQuestCommand(ChatHandler* handler, const char* /*args*/)
    {
        HandleReloadQuestAreaTriggersCommand(handler, "a");
        HandleReloadQuestPOICommand(handler, "a");
        HandleReloadQuestTemplateCommand(handler, "a");

        TC_LOG_INFO("misc", "Re-Loading Quests Relations...");
        sObjectMgr->LoadQuestRelations();
        handler->SendGlobalGMSysMessage("DB tables `*_questrelation` and `*_involvedrelation` reloaded.");
        return true;
    }

    static bool HandleReloadAllScriptsCommand(ChatHandler* handler, const char* /*args*/)
    {
        if (sScriptMgr->IsScriptScheduled())
        {
            handler->PSendSysMessage("DB scripts used currently, please attempt reload later.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        TC_LOG_INFO("misc", "Re-Loading Scripts...");
        HandleReloadEventScriptsCommand(handler, "a");
        HandleReloadSpellScriptsCommand(handler, "a");
        handler->SendGlobalGMSysMessage("DB tables `*_scripts` reloaded.");
        HandleReloadDbScriptStringCommand(handler, "a");
        HandleReloadWpScriptsCommand(handler, "a");
        HandleReloadWpCommand(handler, "a");
        return true;
    }

    static bool HandleReloadAllSpellCommand(ChatHandler* handler, const char* /*args*/)
    {
        HandleReloadSkillDiscoveryTemplateCommand(handler, "a");
        HandleReloadSkillExtraItemTemplateCommand(handler, "a");
        HandleReloadSpellRequiredCommand(handler, "a");
        HandleReloadSpellAreaCommand(handler, "a");
        HandleReloadSpellGroupsCommand(handler, "a");
        HandleReloadSpellLearnSpellCommand(handler, "a");
        HandleReloadSpellLinkedSpellCommand(handler, "a");
        HandleReloadSpellProcEventCommand(handler, "a");
        HandleReloadSpellProcsCommand(handler, "a");
        HandleReloadSpellBonusesCommand(handler, "a");
        HandleReloadSpellTargetPositionCommand(handler, "a");
        HandleReloadSpellThreatsCommand(handler, "a");
        HandleReloadSpellGroupStackRulesCommand(handler, "a");
        HandleReloadSpellPetAurasCommand(handler, "a");
        return true;
    }

    static bool HandleReloadAllGossipsCommand(ChatHandler* handler, const char* args)
    {
        HandleReloadGossipMenuCommand(handler, "a");
        HandleReloadGossipMenuOptionCommand(handler, "a");
        if (*args != 'a')                                          // already reload from all_scripts
        HandleReloadPointsOfInterestCommand(handler, "a");
        return true;
    }

    static bool HandleReloadAllItemCommand(ChatHandler* handler, const char* /*args*/)
    {
        HandleReloadPageTextsCommand(handler, "a");
        HandleReloadItemEnchantementsCommand(handler, "a");
        return true;
    }

    static bool HandleReloadAllLocalesCommand(ChatHandler* handler, const char* /*args*/)
    {
        HandleReloadLocalesAchievementRewardCommand(handler, "a");
        HandleReloadLocalesCreatureCommand(handler, "a");
        HandleReloadLocalesCreatureTextCommand(handler, "a");
        HandleReloadLocalesGameobjectCommand(handler, "a");
        HandleReloadLocalesGossipMenuOptionCommand(handler, "a");
        HandleReloadLocalesItemCommand(handler, "a");
        HandleReloadLocalesNpcTextCommand(handler, "a");
        HandleReloadLocalesPageTextCommand(handler, "a");
        HandleReloadLocalesPointsOfInterestCommand(handler, "a");
        HandleReloadLocalesQuestCommand(handler, "a");
        return true;
    }

    static bool HandleReloadConfigCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading config settings...");
        sWorld->LoadConfigSettings(true);
        sMapMgr->InitializeVisibilityDistanceInfo();
        handler->SendGlobalGMSysMessage("World config settings reloaded.");
        return true;
    }

    static bool HandleReloadAccessRequirementCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Access Requirement definitions...");
        sObjectMgr->LoadAccessRequirements();
        handler->SendGlobalGMSysMessage("DB table `access_requirement` reloaded.");
        return true;
    }

    static bool HandleReloadAchievementCriteriaDataCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Additional Achievement Criteria Data...");
        sAchievementMgr->LoadAchievementCriteriaData();
        handler->SendGlobalGMSysMessage("DB table `achievement_criteria_data` reloaded.");
        return true;
    }

    static bool HandleReloadAchievementRewardCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Achievement Reward Data...");
        sAchievementMgr->LoadRewards();
        handler->SendGlobalGMSysMessage("DB table `achievement_reward` reloaded.");
        return true;
    }

    static bool HandleReloadAreaTriggerTavernCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Tavern Area Triggers...");
        sObjectMgr->LoadTavernAreaTriggers();
        handler->SendGlobalGMSysMessage("DB table `areatrigger_tavern` reloaded.");
        return true;
    }

    static bool HandleReloadAreaTriggerTeleportCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading AreaTrigger teleport definitions...");
        sObjectMgr->LoadAreaTriggerTeleports();
        handler->SendGlobalGMSysMessage("DB table `areatrigger_teleport` reloaded.");
        return true;
    }

    static bool HandleReloadAutobroadcastCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Autobroadcasts...");
        sWorld->LoadAutobroadcasts();
        handler->SendGlobalGMSysMessage("DB table `autobroadcast` reloaded.");
        return true;
    }

    static bool HandleReloadBroadcastTextCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Broadcast texts...");
        sObjectMgr->LoadBroadcastTexts();
        sObjectMgr->LoadBroadcastTextLocales();
        sObjectMgr->LoadBroadcastTextHelpers();
        sObjectMgr->LoadBroadcastGroups();
        handler->SendGlobalGMSysMessage("DB table `broadcast_text` reloaded.");
        return true;
    }

    static bool HandleReloadCommandCommand(ChatHandler* handler, const char* /*args*/)
    {
        handler->SetLoadCommandTable(true);
        handler->SendGlobalGMSysMessage("DB table `command` will be reloaded at next chat command use.");
        return true;
    }

    static bool HandleReloadOnKillRewardCommand (ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading creature award reputation definitions...");
        sObjectMgr->LoadRewardOnKill();
        handler->SendGlobalGMSysMessage("DB table `creature_onkill_reward` reloaded.");
        return true;
    }

    static bool HandleReloadCreatureSummonGroupsCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Reloading creature summon groups...");
        sObjectMgr->LoadTempSummons();
        handler->SendGlobalGMSysMessage("DB table `creature_summon_groups` reloaded.");
        return true;
    }

    static bool HandleReloadCreatureTemplateCommand(ChatHandler* handler, const char* args)
    {
        if (!*args)
            return false;

        Tokenizer entries(std::string(args), ' ');

        for (Tokenizer::const_iterator itr = entries.begin(); itr != entries.end(); ++itr)
        {
            uint32 entry = uint32(atoi(*itr));

            PreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_CREATURE_TEMPLATE);
            stmt->setUInt32(0, entry);
            PreparedQueryResult result = WorldDatabase.Query(stmt);

            if (!result)
            {
                handler->PSendSysMessage(LANG_COMMAND_CREATURETEMPLATE_NOTFOUND, entry);
                continue;
            }

            CreatureTemplate* cInfo = const_cast<CreatureTemplate*>(sObjectMgr->GetCreatureTemplate(entry));
            if (!cInfo)
            {
                handler->PSendSysMessage(LANG_COMMAND_CREATURESTORAGE_NOTFOUND, entry);
                continue;
            }

            TC_LOG_INFO("misc", "Reloading creature template entry %u", entry);

            Field* fields = result->Fetch();

            cInfo->DifficultyEntry[0] = fields[0].GetUInt32();
            cInfo->DifficultyEntry[1] = fields[1].GetUInt32();
            cInfo->DifficultyEntry[2] = fields[2].GetUInt32();
            cInfo->KillCredit[0]      = fields[3].GetUInt32();
            cInfo->KillCredit[1]      = fields[4].GetUInt32();
            cInfo->Modelid1           = fields[5].GetUInt32();
            cInfo->Modelid2           = fields[6].GetUInt32();
            cInfo->Modelid3           = fields[7].GetUInt32();
            cInfo->Modelid4           = fields[8].GetUInt32();
            cInfo->Name               = fields[9].GetString();
            cInfo->SubName            = fields[10].GetString();
            cInfo->IconName           = fields[11].GetString();
            cInfo->GossipMenuId       = fields[12].GetUInt32();
            cInfo->minlevel           = fields[13].GetUInt8();
            cInfo->maxlevel           = fields[14].GetUInt8();
            cInfo->expansion          = fields[15].GetUInt16();
            cInfo->faction_A          = fields[16].GetUInt16();
            cInfo->faction_H          = fields[17].GetUInt16();
            cInfo->npcflag            = fields[18].GetUInt32();
            cInfo->speed_walk         = fields[19].GetFloat();
            cInfo->speed_run          = fields[20].GetFloat();
            cInfo->speed_swim         = fields[21].GetFloat();
            cInfo->speed_fly          = fields[22].GetFloat();
            cInfo->scale              = fields[23].GetFloat();
            cInfo->rank               = fields[24].GetUInt8();
            cInfo->mindmg             = fields[25].GetFloat();
            cInfo->maxdmg             = fields[26].GetFloat();
            cInfo->dmgschool          = fields[27].GetUInt8();
            cInfo->attackpower        = fields[28].GetUInt32();
            cInfo->dmg_multiplier     = fields[29].GetFloat();
            cInfo->baseattacktime     = fields[30].GetUInt32();
            cInfo->rangeattacktime    = fields[31].GetUInt32();
            cInfo->unit_class         = fields[32].GetUInt8();
            cInfo->unit_flags         = fields[33].GetUInt32();
            cInfo->unit_flags2        = fields[34].GetUInt32();
            cInfo->dynamicflags       = fields[35].GetUInt32();
            cInfo->family             = fields[36].GetUInt8();
            cInfo->trainer_type       = fields[37].GetUInt8();
            cInfo->trainer_class      = fields[38].GetUInt8();
            cInfo->trainer_race       = fields[39].GetUInt8();
            cInfo->minrangedmg        = fields[40].GetFloat();
            cInfo->maxrangedmg        = fields[41].GetFloat();
            cInfo->rangedattackpower  = fields[42].GetUInt16();
            cInfo->type               = fields[43].GetUInt8();
            cInfo->type_flags         = fields[44].GetUInt32();
            cInfo->lootid             = fields[45].GetUInt32();
            cInfo->pickpocketLootId   = fields[46].GetUInt32();
            cInfo->SkinLootId         = fields[47].GetUInt32();

            for (uint8 i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
                cInfo->resistance[i] = fields[48 + i -1].GetUInt16();

            cInfo->spells[0]          = fields[54].GetUInt32();
            cInfo->spells[1]          = fields[55].GetUInt32();
            cInfo->spells[2]          = fields[56].GetUInt32();
            cInfo->spells[3]          = fields[57].GetUInt32();
            cInfo->spells[4]          = fields[58].GetUInt32();
            cInfo->spells[5]          = fields[59].GetUInt32();
            cInfo->spells[6]          = fields[60].GetUInt32();
            cInfo->spells[7]          = fields[61].GetUInt32();
            cInfo->PetSpellDataId     = fields[62].GetUInt32();
            cInfo->VehicleId          = fields[63].GetUInt32();
            cInfo->mingold            = fields[64].GetUInt32();
            cInfo->maxgold            = fields[65].GetUInt32();
            cInfo->AIName             = fields[66].GetString();
            cInfo->MovementType       = fields[67].GetUInt8();
            cInfo->InhabitType        = fields[68].GetUInt8();
            cInfo->HoverHeight        = fields[69].GetFloat();
            cInfo->ModHealth          = fields[70].GetFloat();
            cInfo->ModMana            = fields[71].GetFloat();
            cInfo->ModManaExtra       = fields[72].GetFloat();
            cInfo->ModArmor           = fields[73].GetFloat();
            cInfo->RacialLeader       = fields[74].GetBool();
            cInfo->questItems[0]      = fields[75].GetUInt32();
            cInfo->questItems[1]      = fields[76].GetUInt32();
            cInfo->questItems[2]      = fields[77].GetUInt32();
            cInfo->questItems[3]      = fields[78].GetUInt32();
            cInfo->questItems[4]      = fields[79].GetUInt32();
            cInfo->questItems[5]      = fields[80].GetUInt32();
            cInfo->movementId         = fields[81].GetUInt32();
            cInfo->RegenHealth        = fields[82].GetBool();
            cInfo->equipmentId        = fields[83].GetUInt32();
            cInfo->MechanicImmuneMask = fields[84].GetUInt32();
            cInfo->flags_extra        = fields[85].GetUInt32();
            cInfo->ScriptID           = sObjectMgr->GetScriptId(fields[86].GetCString());

            sObjectMgr->CheckCreatureTemplate(cInfo);
        }

        handler->SendGlobalGMSysMessage("Creature template reloaded.");
        return true;
    }

    static bool HandleReloadCreatureQuestRelationsCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Loading Quests Relations... (`creature_questrelation`)");
        sObjectMgr->LoadCreatureQuestRelations();
        handler->SendGlobalGMSysMessage("DB table `creature_questrelation` (creature quest givers) reloaded.");
        return true;
    }

    static bool HandleReloadLinkedRespawnCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Loading Linked Respawns... (`creature_linked_respawn`)");
        sObjectMgr->LoadLinkedRespawn();
        handler->SendGlobalGMSysMessage("DB table `creature_linked_respawn` (creature linked respawns) reloaded.");
        return true;
    }

    static bool HandleReloadCreatureQuestInvRelationsCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Loading Quests Relations... (`creature_involvedrelation`)");
        sObjectMgr->LoadCreatureInvolvedRelations();
        handler->SendGlobalGMSysMessage("DB table `creature_involvedrelation` (creature quest takers) reloaded.");
        return true;
    }

    static bool HandleReloadGossipMenuCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading `gossip_menu` Table!");
        sObjectMgr->LoadGossipMenu();
        handler->SendGlobalGMSysMessage("DB table `gossip_menu` reloaded.");
        sConditionMgr->LoadConditions(true);
        return true;
    }

    static bool HandleReloadGossipMenuOptionCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading `gossip_menu_option` Table!");
        sObjectMgr->LoadGossipMenuItems();
        handler->SendGlobalGMSysMessage("DB table `gossip_menu_option` reloaded.");
        sConditionMgr->LoadConditions(true);
        return true;
    }

    static bool HandleReloadGOQuestRelationsCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Loading Quests Relations... (`gameobject_questrelation`)");
        sObjectMgr->LoadGameobjectQuestRelations();
        handler->SendGlobalGMSysMessage("DB table `gameobject_questrelation` (gameobject quest givers) reloaded.");
        return true;
    }

    static bool HandleReloadGOQuestInvRelationsCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Loading Quests Relations... (`gameobject_involvedrelation`)");
        sObjectMgr->LoadGameobjectInvolvedRelations();
        handler->SendGlobalGMSysMessage("DB table `gameobject_involvedrelation` (gameobject quest takers) reloaded.");
        return true;
    }

    static bool HandleReloadQuestAreaTriggersCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Quest Area Triggers...");
        sObjectMgr->LoadQuestAreaTriggers();
        handler->SendGlobalGMSysMessage("DB table `areatrigger_involvedrelation` (quest area triggers) reloaded.");
        return true;
    }

    static bool HandleReloadQuestTemplateCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Quest Templates...");
        sObjectMgr->LoadQuests();
        handler->SendGlobalGMSysMessage("DB table `quest_template` (quest definitions) reloaded.");

        /// dependent also from `gameobject` but this table not reloaded anyway
        TC_LOG_INFO("misc", "Re-Loading GameObjects for quests...");
        sObjectMgr->LoadGameObjectForQuests();
        handler->SendGlobalGMSysMessage("Data GameObjects for quests reloaded.");
        return true;
    }

    static bool HandleReloadLootTemplatesCreatureCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Loot Tables... (`creature_loot_template`)");
        LoadLootTemplates_Creature();
        LootTemplates_Creature.CheckLootRefs();
        handler->SendGlobalGMSysMessage("DB table `creature_loot_template` reloaded.");
        sConditionMgr->LoadConditions(true);
        return true;
    }

    static bool HandleReloadLootTemplatesDisenchantCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Loot Tables... (`disenchant_loot_template`)");
        LoadLootTemplates_Disenchant();
        LootTemplates_Disenchant.CheckLootRefs();
        handler->SendGlobalGMSysMessage("DB table `disenchant_loot_template` reloaded.");
        sConditionMgr->LoadConditions(true);
        return true;
    }

    static bool HandleReloadLootTemplatesFishingCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Loot Tables... (`fishing_loot_template`)");
        LoadLootTemplates_Fishing();
        LootTemplates_Fishing.CheckLootRefs();
        handler->SendGlobalGMSysMessage("DB table `fishing_loot_template` reloaded.");
        sConditionMgr->LoadConditions(true);
        return true;
    }

    static bool HandleReloadLootTemplatesGameobjectCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Loot Tables... (`gameobject_loot_template`)");
        LoadLootTemplates_Gameobject();
        LootTemplates_Gameobject.CheckLootRefs();
        handler->SendGlobalGMSysMessage("DB table `gameobject_loot_template` reloaded.");
        sConditionMgr->LoadConditions(true);
        return true;
    }

    static bool HandleReloadLootTemplatesItemCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Loot Tables... (`item_loot_template`)");
        LoadLootTemplates_Item();
        LootTemplates_Item.CheckLootRefs();
        handler->SendGlobalGMSysMessage("DB table `item_loot_template` reloaded.");
        sConditionMgr->LoadConditions(true);
        return true;
    }

    static bool HandleReloadLootTemplatesMillingCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Loot Tables... (`milling_loot_template`)");
        LoadLootTemplates_Milling();
        LootTemplates_Milling.CheckLootRefs();
        handler->SendGlobalGMSysMessage("DB table `milling_loot_template` reloaded.");
        sConditionMgr->LoadConditions(true);
        return true;
    }

    static bool HandleReloadLootTemplatesPickpocketingCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Loot Tables... (`pickpocketing_loot_template`)");
        LoadLootTemplates_Pickpocketing();
        LootTemplates_Pickpocketing.CheckLootRefs();
        handler->SendGlobalGMSysMessage("DB table `pickpocketing_loot_template` reloaded.");
        sConditionMgr->LoadConditions(true);
        return true;
    }

    static bool HandleReloadLootTemplatesProspectingCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Loot Tables... (`prospecting_loot_template`)");
        LoadLootTemplates_Prospecting();
        LootTemplates_Prospecting.CheckLootRefs();
        handler->SendGlobalGMSysMessage("DB table `prospecting_loot_template` reloaded.");
        sConditionMgr->LoadConditions(true);
        return true;
    }

    static bool HandleReloadLootTemplatesMailCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Loot Tables... (`mail_loot_template`)");
        LoadLootTemplates_Mail();
        LootTemplates_Mail.CheckLootRefs();
        handler->SendGlobalGMSysMessage("DB table `mail_loot_template` reloaded.");
        sConditionMgr->LoadConditions(true);
        return true;
    }

    static bool HandleReloadLootTemplatesReferenceCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Loot Tables... (`reference_loot_template`)");
        LoadLootTemplates_Reference();
        handler->SendGlobalGMSysMessage("DB table `reference_loot_template` reloaded.");
        sConditionMgr->LoadConditions(true);
        return true;
    }

    static bool HandleReloadLootTemplatesSkinningCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Loot Tables... (`skinning_loot_template`)");
        LoadLootTemplates_Skinning();
        LootTemplates_Skinning.CheckLootRefs();
        handler->SendGlobalGMSysMessage("DB table `skinning_loot_template` reloaded.");
        sConditionMgr->LoadConditions(true);
        return true;
    }

    static bool HandleReloadLootTemplatesSpellCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Loot Tables... (`spell_loot_template`)");
        LoadLootTemplates_Spell();
        LootTemplates_Spell.CheckLootRefs();
        handler->SendGlobalGMSysMessage("DB table `spell_loot_template` reloaded.");
        sConditionMgr->LoadConditions(true);
        return true;
    }

    static bool HandleReloadTrinityStringCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading trinity_string Table!");
        sObjectMgr->LoadTrinityStrings();
        handler->SendGlobalGMSysMessage("DB table `trinity_string` reloaded.");
        return true;
    }

    static bool HandleReloadWardenactionCommand(ChatHandler* handler, const char* /*args*/)
    {
        if (!sWorld->getBoolConfig(CONFIG_WARDEN_ENABLED))
        {
            handler->SendSysMessage("Warden system disabled by config - reloading warden_action skipped.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        TC_LOG_INFO("misc", "Re-Loading warden_action Table!");
        sWardenCheckMgr->LoadWardenOverrides();
        handler->SendGlobalGMSysMessage("DB table `warden_action` reloaded.");
        return true;
    }

    static bool HandleReloadNpcTrainerCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading `npc_trainer` Table!");
        sObjectMgr->LoadTrainerSpell();
        handler->SendGlobalGMSysMessage("DB table `npc_trainer` reloaded.");
        return true;
    }

    static bool HandleReloadNpcVendorCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading `npc_vendor` Table!");
        sObjectMgr->LoadVendors();
        handler->SendGlobalGMSysMessage("DB table `npc_vendor` reloaded.");
        return true;
    }

    static bool HandleReloadPointsOfInterestCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading `points_of_interest` Table!");
        sObjectMgr->LoadPointsOfInterest();
        handler->SendGlobalGMSysMessage("DB table `points_of_interest` reloaded.");
        return true;
    }

    static bool HandleReloadQuestPOICommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Quest POI ..." );
        sObjectMgr->LoadQuestPOI();
        handler->SendGlobalGMSysMessage("DB Table `quest_poi` and `quest_poi_points` reloaded.");
        return true;
    }

    static bool HandleReloadSpellClickSpellsCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading `npc_spellclick_spells` Table!");
        sObjectMgr->LoadNPCSpellClickSpells();
        handler->SendGlobalGMSysMessage("DB table `npc_spellclick_spells` reloaded.");
        return true;
    }

    static bool HandleReloadReservedNameCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Loading ReservedNames... (`reserved_name`)");
        sObjectMgr->LoadReservedPlayersNames();
        handler->SendGlobalGMSysMessage("DB table `reserved_name` (player reserved names) reloaded.");
        return true;
    }

    static bool HandleReloadReputationRewardRateCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading `reputation_reward_rate` Table!" );
        sObjectMgr->LoadReputationRewardRate();
        handler->SendGlobalSysMessage("DB table `reputation_reward_rate` reloaded.");
        return true;
    }

    static bool HandleReloadReputationSpilloverTemplateCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading `reputation_spillover_template` Table!" );
        sObjectMgr->LoadReputationSpilloverTemplate();
        handler->SendGlobalSysMessage("DB table `reputation_spillover_template` reloaded.");
        return true;
    }

    static bool HandleReloadSkillDiscoveryTemplateCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Skill Discovery Table...");
        LoadSkillDiscoveryTable();
        handler->SendGlobalGMSysMessage("DB table `skill_discovery_template` (recipes discovered at crafting) reloaded.");
        return true;
    }

    static bool HandleReloadSkillExtraItemTemplateCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Skill Extra Item Table...");
        LoadSkillExtraItemTable();
        handler->SendGlobalGMSysMessage("DB table `skill_extra_item_template` (extra item creation when crafting) reloaded.");
        return true;
    }

    static bool HandleReloadSkillFishingBaseLevelCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Skill Fishing base level requirements...");
        sObjectMgr->LoadFishingBaseSkillLevel();
        handler->SendGlobalGMSysMessage("DB table `skill_fishing_base_level` (fishing base level for zone/subzone) reloaded.");
        return true;
    }

    static bool HandleReloadSpellAreaCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading SpellArea Data...");
        sSpellMgr->LoadSpellAreas();
        handler->SendGlobalGMSysMessage("DB table `spell_area` (spell dependences from area/quest/auras state) reloaded.");
        return true;
    }

    static bool HandleReloadSpellRequiredCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Spell Required Data... ");
        sSpellMgr->LoadSpellRequired();
        handler->SendGlobalGMSysMessage("DB table `spell_required` reloaded.");
        return true;
    }

    static bool HandleReloadSpellGroupsCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Spell Groups...");
        sSpellMgr->LoadSpellGroups();
        handler->SendGlobalGMSysMessage("DB table `spell_group` (spell groups) reloaded.");
        return true;
    }

    static bool HandleReloadSpellLearnSpellCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Spell Learn Spells...");
        sSpellMgr->LoadSpellLearnSpells();
        handler->SendGlobalGMSysMessage("DB table `spell_learn_spell` reloaded.");
        return true;
    }

    static bool HandleReloadSpellLinkedSpellCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Spell Linked Spells...");
        sSpellMgr->LoadSpellLinked();
        handler->SendGlobalGMSysMessage("DB table `spell_linked_spell` reloaded.");
        return true;
    }

    static bool HandleReloadSpellProcEventCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Spell Proc Event conditions...");
        sSpellMgr->LoadSpellProcEvents();
        handler->SendGlobalGMSysMessage("DB table `spell_proc_event` (spell proc trigger requirements) reloaded.");
        return true;
    }

    static bool HandleReloadSpellProcsCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Spell Proc conditions and data...");
        sSpellMgr->LoadSpellProcs();
        handler->SendGlobalGMSysMessage("DB table `spell_proc` (spell proc conditions and data) reloaded.");
        return true;
    }

    static bool HandleReloadSpellBonusesCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Spell Bonus Data...");
        sSpellMgr->LoadSpellBonusess();
        handler->SendGlobalGMSysMessage("DB table `spell_bonus_data` (spell damage/healing coefficients) reloaded.");
        return true;
    }

    static bool HandleReloadSpellTargetPositionCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Spell target coordinates...");
        sSpellMgr->LoadSpellTargetPositions();
        handler->SendGlobalGMSysMessage("DB table `spell_target_position` (destination coordinates for spell targets) reloaded.");
        return true;
    }

    static bool HandleReloadSpellThreatsCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Aggro Spells Definitions...");
        sSpellMgr->LoadSpellThreats();
        handler->SendGlobalGMSysMessage("DB table `spell_threat` (spell aggro definitions) reloaded.");
        return true;
    }

    static bool HandleReloadSpellGroupStackRulesCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Spell Group Stack Rules...");
        sSpellMgr->LoadSpellGroupStackRules();
        handler->SendGlobalGMSysMessage("DB table `spell_group_stack_rules` (spell stacking definitions) reloaded.");
        return true;
    }

    static bool HandleReloadSpellPetAurasCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Spell pet auras...");
        sSpellMgr->LoadSpellPetAuras();
        handler->SendGlobalGMSysMessage("DB table `spell_pet_auras` reloaded.");
        return true;
    }

    static bool HandleReloadPageTextsCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Page Texts...");
        sObjectMgr->LoadPageTexts();
        handler->SendGlobalGMSysMessage("DB table `page_texts` reloaded.");
        return true;
    }

    static bool HandleReloadItemEnchantementsCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Item Random Enchantments Table...");
        LoadRandomEnchantmentsTable();
        handler->SendGlobalGMSysMessage("DB table `item_enchantment_template` reloaded.");
        return true;
    }

    static bool HandleReloadEventScriptsCommand(ChatHandler* handler, const char* args)
    {
        if (sScriptMgr->IsScriptScheduled())
        {
            handler->SendSysMessage("DB scripts used currently, please attempt reload later.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (*args != 'a')
            TC_LOG_INFO("misc", "Re-Loading Scripts from `event_scripts`...");

        sObjectMgr->LoadEventScripts();

        if (*args != 'a')
            handler->SendGlobalGMSysMessage("DB table `event_scripts` reloaded.");

        return true;
    }

    static bool HandleReloadWpScriptsCommand(ChatHandler* handler, const char* args)
    {
        if (sScriptMgr->IsScriptScheduled())
        {
            handler->SendSysMessage("DB scripts used currently, please attempt reload later.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (*args != 'a')
            TC_LOG_INFO("misc", "Re-Loading Scripts from `waypoint_scripts`...");

        sObjectMgr->LoadWaypointScripts();

        if (*args != 'a')
            handler->SendGlobalGMSysMessage("DB table `waypoint_scripts` reloaded.");

        return true;
    }

    static bool HandleReloadWpCommand(ChatHandler* handler, const char* args)
    {
        if (*args != 'a')
            TC_LOG_INFO("misc", "Re-Loading Waypoints data from 'waypoints_data'");

        sWaypointMgr->Load();

        if (*args != 'a')
            handler->SendGlobalGMSysMessage("DB Table 'waypoint_data' reloaded.");

        return true;
    }

    static bool HandleReloadSpellScriptsCommand(ChatHandler* handler, const char* args)
    {
        if (sScriptMgr->IsScriptScheduled())
        {
            handler->SendSysMessage("DB scripts used currently, please attempt reload later.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (*args != 'a')
            TC_LOG_INFO("misc", "Re-Loading Scripts from `spell_scripts`...");

        sObjectMgr->LoadSpellScripts();

        if (*args != 'a')
            handler->SendGlobalGMSysMessage("DB table `spell_scripts` reloaded.");

        return true;
    }

    static bool HandleReloadDbScriptStringCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Script strings from `db_script_string`...");
        sObjectMgr->LoadDbScriptStrings();
        handler->SendGlobalGMSysMessage("DB table `db_script_string` reloaded.");
        return true;
    }

    static bool HandleReloadGameGraveyardZoneCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Graveyard-zone links...");

        sObjectMgr->LoadGraveyardZones();

        handler->SendGlobalGMSysMessage("DB table `game_graveyard_zone` reloaded.");

        return true;
    }

    static bool HandleReloadGameTeleCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Game Tele coordinates...");

        sObjectMgr->LoadGameTele();

        handler->SendGlobalGMSysMessage("DB table `game_tele` reloaded.");

        return true;
    }

    static bool HandleReloadDisablesCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading disables table...");
        DisableMgr::LoadDisables();
        TC_LOG_INFO("misc", "Checking quest disables...");
        DisableMgr::CheckQuestDisables();
        handler->SendGlobalGMSysMessage("DB table `disables` reloaded.");
        return true;
    }

    static bool HandleReloadLocalesAchievementRewardCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Locales Achievement Reward Data...");
        sAchievementMgr->LoadRewardLocales();
        handler->SendGlobalGMSysMessage("DB table `locales_achievement_reward` reloaded.");
        return true;
    }

    static bool HandleReloadLfgRewardsCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading lfg dungeon rewards...");
        sLFGMgr->LoadRewards();
        handler->SendGlobalGMSysMessage("DB table `lfg_dungeon_rewards` reloaded.");
        return true;
    }

    static bool HandleReloadLocalesCreatureCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Locales Creature ...");
        sObjectMgr->LoadCreatureLocales();
        handler->SendGlobalGMSysMessage("DB table `locales_creature` reloaded.");
        return true;
    }

    static bool HandleReloadLocalesCreatureTextCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Locales Creature Texts...");
        sCreatureTextMgr->LoadCreatureTextLocales();
        handler->SendGlobalGMSysMessage("DB table `locales_creature_text` reloaded.");
        return true;
    }

    static bool HandleReloadLocalesGameobjectCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Locales Gameobject ... ");
        sObjectMgr->LoadGameObjectLocales();
        handler->SendGlobalGMSysMessage("DB table `locales_gameobject` reloaded.");
        return true;
    }

    static bool HandleReloadLocalesGossipMenuOptionCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Locales Gossip Menu Option ... ");
        sObjectMgr->LoadGossipMenuItemsLocales();
        handler->SendGlobalGMSysMessage("DB table `locales_gossip_menu_option` reloaded.");
        return true;
    }

    static bool HandleReloadLocalesItemCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Locales Item ... ");
        sObjectMgr->LoadItemLocales();
        handler->SendGlobalGMSysMessage("DB table `locales_item` reloaded.");
        return true;
    }

    static bool HandleReloadLocalesNpcTextCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Locales NPC Text ... ");
        sObjectMgr->LoadNpcTextLocales();
        handler->SendGlobalGMSysMessage("DB table `locales_npc_text` reloaded.");
        return true;
    }

    static bool HandleReloadLocalesPageTextCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Locales Page Text ... ");
        sObjectMgr->LoadPageTextLocales();
        handler->SendGlobalGMSysMessage("DB table `locales_page_text` reloaded.");
        return true;
    }

    static bool HandleReloadLocalesPointsOfInterestCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Locales Points Of Interest ... ");
        sObjectMgr->LoadPointOfInterestLocales();
        handler->SendGlobalGMSysMessage("DB table `locales_points_of_interest` reloaded.");
        return true;
    }

    static bool HandleReloadLocalesQuestCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Locales Quest ... ");
        sObjectMgr->LoadQuestLocales();
        handler->SendGlobalGMSysMessage("DB table `locales_quest` reloaded.");
        return true;
    }

    static bool HandleReloadMailLevelRewardCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Player level dependent mail rewards...");
        sObjectMgr->LoadMailLevelRewards();
        handler->SendGlobalGMSysMessage("DB table `mail_level_reward` reloaded.");
        return true;
    }

    static bool HandleReloadAuctionsCommand(ChatHandler* handler, const char* /*args*/)
    {
        ///- Reload dynamic data tables from the database
        TC_LOG_INFO("misc", "Re-Loading Auctions...");
        sAuctionMgr->LoadAuctionItems();
        sAuctionMgr->LoadAuctions();
        handler->SendGlobalGMSysMessage("Auctions reloaded.");
        return true;
    }

    static bool HandleReloadConditions(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Conditions...");
        sConditionMgr->LoadConditions(true);
        handler->SendGlobalGMSysMessage("Conditions reloaded.");
        return true;
    }

    static bool HandleReloadCreatureText(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Creature Texts...");
        sCreatureTextMgr->LoadCreatureTexts();
        handler->SendGlobalGMSysMessage("Creature Texts reloaded.");
        return true;
    }

    static bool HandleReloadSmartScripts(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Re-Loading Smart Scripts...");
        sSmartScriptMgr->LoadSmartAIFromDB();
        handler->SendGlobalGMSysMessage("Smart Scripts reloaded.");
        return true;
    }

    static bool HandleReloadVehicleAccessoryCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Reloading vehicle_accessory table...");
        sObjectMgr->LoadVehicleAccessories();
        handler->SendGlobalGMSysMessage("Vehicle accessories reloaded.");
        return true;
    }

    static bool HandleReloadVehicleTemplateAccessoryCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Reloading vehicle_template_accessory table...");
        sObjectMgr->LoadVehicleTemplateAccessories();
        handler->SendGlobalGMSysMessage("Vehicle template accessories reloaded.");
        return true;
    }

    static bool HandleReloadPhaseDefinitionsCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Reloading phase_definitions table...");
        sObjectMgr->LoadPhaseDefinitions();
        sWorld->UpdatePhaseDefinitions();
        handler->SendGlobalGMSysMessage("Phase Definitions reloaded.");
        return true;
    }

    static bool HandleReloadPerfConfigCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("server.worldserver", "Reloading PerformanceLog config");
        sPerfLog->LoadConfig();
        handler->SendGlobalGMSysMessage("PerformanceLog config reloaded");
        return true;
    }

    static bool HandleReloadTemplateNpcCommand(ChatHandler* handler, const char* /*args*/)
    {
        //sLog->outInfo(LOG_FILTER_WORLDSERVER, "Reloading templates for Template NPC table...");
        sTemplateMgr->LoadTalentTrees();
        sTemplateMgr->LoadNpcTemplates();
        handler->SendGlobalGMSysMessage("Template NPC - templates reloaded.");
        return true;
    }

    static bool HandleReloadInstanceTeleporterDestCommand(ChatHandler* handler, const char* /*args*/)
    {
        TC_LOG_INFO("misc", "Reloading instance teleporter destinations...");
        sObjectMgr->LoadTeleportersDest();
        handler->SendGlobalGMSysMessage("DB table `instance_teleporter_dest` reloaded.");
        return true;
    }

};

void AddSC_reload_commandscript()
{
    new reload_commandscript();
}
