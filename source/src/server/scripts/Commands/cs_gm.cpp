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
Name: gm_commandscript
%Complete: 100
Comment: All gm related commands
Category: commandscripts
EndScriptData */

#include "ScriptMgr.h"
#include "ObjectMgr.h"
#include "Chat.h"
#include "AccountMgr.h"
#include "Language.h"
#include "World.h"
#include "Player.h"
#include "Opcodes.h"

class gm_commandscript : public CommandScript
{
public:
    gm_commandscript() : CommandScript("gm_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> gmCommandTable =
        {
            { "chat",           SEC_CONSOLE,      false, &HandleGMChatCommand,              "" },
            { "fly",            SEC_CONSOLE,  false, &HandleGMFlyCommand,               "" },
            { "ingame",         SEC_CONSOLE,         true,  &HandleGMListIngameCommand,        "" },
            { "list",           SEC_CONSOLE,  true,  &HandleGMListFullCommand,          "" },
            { "visible",        SEC_CONSOLE,      false, &HandleGMVisibleCommand,           "" },
            { "",               SEC_CONSOLE,      false, &HandleGMCommand,                  "" },
        };

        static std::vector<ChatCommand> commandTable =
        {
            { "gm",             SEC_CONSOLE,      false, NULL,                     "", gmCommandTable },
            { "movie",          SEC_CONSOLE,      false, &HandleMovieMakerCommand,           "" },
        };
        return commandTable;
    }

    // Enables or disables hiding of the staff badge
    static bool HandleGMChatCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
        {
            WorldSession* session = handler->GetSession();
            if (!AccountMgr::IsPlayerAccount(session->GetSecurity()) && session->GetPlayer()->isGMChat())
                session->SendNotification(LANG_GM_CHAT_ON);
            else
                session->SendNotification(LANG_GM_CHAT_OFF);
            return true;
        }

        std::string param = (char*)args;

        if (param == "on")
        {
            handler->GetSession()->GetPlayer()->SetGMChat(true);
            handler->GetSession()->SendNotification(LANG_GM_CHAT_ON);
            return true;
        }

        if (param == "off")
        {
            handler->GetSession()->GetPlayer()->SetGMChat(false);
            handler->GetSession()->SendNotification(LANG_GM_CHAT_OFF);
            return true;
        }

        handler->SendSysMessage(LANG_USE_BOL);
        handler->SetSentErrorMessage(true);
        return false;
    }

    static bool HandleGMFlyCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        //Player* target =  handler->getSelectedPlayer();
        Player* target =  handler->GetSession()->GetPlayer();
        if (!target)
            target = handler->GetSession()->GetPlayer();

        WorldPacket data;
        if (strncmp(args, "on", 3) == 0)
        {
            target->SetCanFlybyServer(true);
            target->AddUnitMovementFlag(MOVEMENTFLAG_CAN_FLY);
            target->SendMovementCanFlyChange();
        }
        else if (strncmp(args, "off", 4) == 0)
        {
            target->SetCanFlybyServer(false);
            target->RemoveUnitMovementFlag(MOVEMENTFLAG_CAN_FLY);
            target->SendMovementCanFlyChange();
        }
        else
        {
            handler->SendSysMessage(LANG_USE_BOL);
            return false;
        }
        handler->PSendSysMessage(LANG_COMMAND_FLYMODE_STATUS, handler->GetNameLink(target).c_str(), args);
        return true;
    }

    static bool HandleGMListIngameCommand(ChatHandler* handler, char const* /*args*/)
    {
        bool first = true;
        bool footer = false;

        TRINITY_READ_GUARD(HashMapHolder<Player>::LockType, *HashMapHolder<Player>::GetLock());
        HashMapHolder<Player>::MapType const& m = sObjectAccessor->GetPlayers();
        for (HashMapHolder<Player>::MapType::const_iterator itr = m.begin(); itr != m.end(); ++itr)
        {
            AccountTypes itrSec = itr->second->GetSession()->GetSecurity();
            if ((itr->second->isGameMaster() || (!AccountMgr::IsPlayerAccount(itrSec) && itrSec <= AccountTypes(sWorld->getIntConfig(CONFIG_GM_LEVEL_IN_GM_LIST)))) &&
                (!handler->GetSession() || itr->second->IsVisibleGloballyFor(handler->GetSession()->GetPlayer())))
            {
                if (itr->second->IsSpectator())
                    continue; // don't show spectators, they're not really gms 
                
                if (first)
                {
                    first = false;
                    footer = true;
                    handler->SendSysMessage(LANG_GMS_ON_SRV);
                    handler->SendSysMessage("========================");
                }
                std::string const& name = itr->second->GetName();
                uint8 size = name.size();
                uint8 security = itrSec;
                if (security == 0)
                    continue;
                uint8 max = ((16 - size) / 2);
                uint8 max2 = max;
                if ((max + max2 + size) == 16)
                    max2 = max - 1;
                if (handler->GetSession())
                    handler->PSendSysMessage("|    %s GMLevel %u", name.c_str(), security);
                else
                    handler->PSendSysMessage("|%*s%s%*s|   %u  |", max, " ", name.c_str(), max2, " ", security);
            }
        }
        if (footer)
            handler->SendSysMessage("========================");
        if (first)
            handler->SendSysMessage(LANG_GMS_NOT_LOGGED);
        return true;
    }

    /// Display the list of GMs
    static bool HandleGMListFullCommand(ChatHandler* handler, char const* /*args*/)
    {
        ///- Get the accounts with GM Level >0
        PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_GM_ACCOUNTS);
        stmt->setUInt8(0, uint8(SEC_CONSOLE));
        stmt->setInt32(1, int32(realmID));
        PreparedQueryResult result = LoginDatabase.Query(stmt);

        if (result)
        {
            handler->SendSysMessage(LANG_GMLIST);
            handler->SendSysMessage("========================");
            ///- Cycle through them. Display username and GM level
            do
            {
                Field* fields = result->Fetch();
                char const* name = fields[0].GetCString();
                uint8 security = fields[1].GetUInt8();
                uint8 max = (16 - strlen(name)) / 2;
                uint8 max2 = max;
                if ((max + max2 + strlen(name)) == 16)
                    max2 = max - 1;
                if (handler->GetSession())
                    handler->PSendSysMessage("|    %s GMLevel %u", name, security);
                else
                    handler->PSendSysMessage("|%*s%s%*s|   %u  |", max, " ", name, max2, " ", security);
            } while (result->NextRow());
            handler->SendSysMessage("========================");
        }
        else
            handler->PSendSysMessage(LANG_GMLIST_EMPTY);
        return true;
    }

    //Enable\Disable Invisible mode
    static bool HandleGMVisibleCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
        {
            handler->PSendSysMessage(LANG_YOU_ARE, handler->GetSession()->GetPlayer()->isGMVisible() ? handler->GetTrinityString(LANG_VISIBLE) : handler->GetTrinityString(LANG_INVISIBLE));
            return true;
        }

        const uint32 VISUAL_AURA = 44816;/*Same as current but doesn't show as active debuff. :feelscleanman:*/
        std::string param = (char*)args;
        Player* player = handler->GetSession()->GetPlayer();

        if (param == "on")
        {
            if (player->HasAura(VISUAL_AURA, 0))
                player->RemoveAurasDueToSpell(VISUAL_AURA);

            player->SetGMVisible(true);
            handler->GetSession()->SendNotification(LANG_INVISIBLE_VISIBLE);
            player->UpdateObjectVisibility(true);
            return true;
        }

        if (param == "off")
        {
            handler->GetSession()->SendNotification(LANG_INVISIBLE_INVISIBLE);
            player->SetGMVisible(false);

            player->AddAura(VISUAL_AURA, player);
            player->UpdateObjectVisibility(true);

            return true;
        }

        handler->SendSysMessage(LANG_USE_BOL);
        handler->SetSentErrorMessage(true);
        return false;
    }

    //Enable\Disable GM Mode
    static bool HandleGMCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
        {
            if (handler->GetSession()->GetPlayer()->isGameMaster())
                handler->GetSession()->SendNotification(LANG_GM_ON);
            else
                handler->GetSession()->SendNotification(LANG_GM_OFF);
            return true;
        }

        std::string param = (char*)args;

        if (param == "on")
        {
            handler->GetSession()->GetPlayer()->SetGameMaster(true);
            handler->GetSession()->SendNotification(LANG_GM_ON);
            handler->GetSession()->GetPlayer()->UpdateTriggerVisibility();
#ifdef _DEBUG_VMAPS
            VMAP::IVMapManager* vMapManager = VMAP::VMapFactory::createOrGetVMapManager();
            vMapManager->processCommand("stoplog");
#endif
            return true;
        }

        if (param == "off")
        {
            handler->GetSession()->GetPlayer()->SetGameMaster(false);
            handler->GetSession()->SendNotification(LANG_GM_OFF);
            handler->GetSession()->GetPlayer()->UpdateTriggerVisibility();
#ifdef _DEBUG_VMAPS
            VMAP::IVMapManager* vMapManager = VMAP::VMapFactory::createOrGetVMapManager();
            vMapManager->processCommand("startlog");
#endif
            return true;
        }

        handler->SendSysMessage(LANG_USE_BOL);
        handler->SetSentErrorMessage(true);
        return false;
    }

    //Enable\Disable MovieMaker Mode
    static bool HandleMovieMakerCommand(ChatHandler* handler, char const* args)
    {
        Player *player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;
        if (!*args)
        {
            handler->PSendSysMessage("MovieMaker mode is '%s', Syntax .movie on/off", player->isMovieMaker() ? "activated" : "not activated");
            return true;
        }

        std::string param = (char*)args;

        if (param == "on")
        {
            handler->GetSession()->GetPlayer()->SetMovieMaker(true);
            handler->GetSession()->GetPlayer()->UpdateTriggerVisibility();
            handler->PSendSysMessage("MovieMaker turned on");
            return true;
        }
        else if (param == "off")
        {
            handler->GetSession()->GetPlayer()->SetMovieMaker(false);
            handler->GetSession()->GetPlayer()->UpdateTriggerVisibility();
            handler->PSendSysMessage("MovieMaker turned off");
            return true;
        }

        handler->PSendSysMessage("Syntax .movie on/off");
        return false;
    }
};

void AddSC_gm_commandscript()
{
    new gm_commandscript();
}
