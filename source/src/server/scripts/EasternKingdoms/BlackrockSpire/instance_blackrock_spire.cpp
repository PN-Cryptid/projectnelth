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

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "blackrock_spire.h"

class instance_blackrock_spire : public InstanceMapScript
{
public:
    instance_blackrock_spire() : InstanceMapScript("instance_blackrock_spire", 229) { }

    InstanceScript* GetInstanceScript(InstanceMap* map) const
    {
        return new instance_blackrock_spireMapScript(map);
    }

    struct instance_blackrock_spireMapScript : public InstanceScript
    {
        instance_blackrock_spireMapScript(InstanceMap* map) : InstanceScript(map) {}

        uint64 HighlordOmokk;
        uint64 ShadowHunterVoshgajin;
        uint64 WarMasterVoone;
        uint64 MotherSmolderweb;
        uint64 UrokDoomhowl;
        uint64 QuartermasterZigris;
        uint64 GizrultheSlavener;
        uint64 Halycon;
        uint64 OverlordWyrmthalak;
        uint64 PyroguardEmberseer;
        uint64 WarchiefRendBlackhand;
        uint64 Gyth;
        uint64 TheBeast;
        uint64 GeneralDrakkisath;
        uint64 go_emberseerin;
        uint64 go_doors;
        uint64 go_emberseerout;
        uint64 go_roomrunes[MAX_DRAGONSPIRE_HALL_RUNES];
        uint8 Runemaxprotectors[MAX_DRAGONSPIRE_HALL_RUNES];
        uint8 Runeprotectorsdead[MAX_DRAGONSPIRE_HALL_RUNES];

        void Initialize()
        {
            SetHeaders(DataHeader);
            SetBossNumber(MAX_ENCOUNTER);
            HighlordOmokk           = 0;
            ShadowHunterVoshgajin   = 0;
            WarMasterVoone          = 0;
            MotherSmolderweb        = 0;
            UrokDoomhowl            = 0;
            QuartermasterZigris     = 0;
            GizrultheSlavener       = 0;
            Halycon                 = 0;
            OverlordWyrmthalak      = 0;
            PyroguardEmberseer      = 0;
            WarchiefRendBlackhand   = 0;
            Gyth                    = 0;
            TheBeast                = 0;
            GeneralDrakkisath       = 0;
            go_emberseerin          = 0;
            go_doors                = 0;
            go_emberseerout         = 0;
        }

        void OnCreatureCreate(Creature* creature)
        {
            InstanceScript::OnCreatureCreate(creature);

            switch (creature->GetEntry())
            {
                case NPC_OMOKK:
                    HighlordOmokk = creature->GetGUID();
                    break;
                case NPC_SHADOW_HUNTER_VOSHGAJIN:
                    ShadowHunterVoshgajin = creature->GetGUID();
                    break;
                case NPC_WARMASTER_VOONE:
                    WarMasterVoone = creature->GetGUID();
                    break;
                case NPC_MOTHER_SMOLDERWEB:
                    MotherSmolderweb = creature->GetGUID();
                    break;
                case NPC_UROK_DOOMHOWL:
                    UrokDoomhowl = creature->GetGUID();
                    break;
                case NPC_QUARTERMASTER_ZIGRIS:
                    QuartermasterZigris = creature->GetGUID();
                    break;
                case NPC_GIZRUL_THE_SLAVENER:
                    GizrultheSlavener = creature->GetGUID();
                    break;
                case  NPC_HALYCON:
                    Halycon = creature->GetGUID();
                    break;
                case  NPC_OVERLORD_WYRMTHALAK:
                    OverlordWyrmthalak = creature->GetGUID();
                    break;
                case  NPC_PYROGAURD_EMBERSEER:
                    PyroguardEmberseer = creature->GetGUID();
                    break;
                case  NPC_WARCHIEF_REND_BLACKHAND:
                    WarchiefRendBlackhand = creature->GetGUID();
                    break;
                case NPC_GYTH:
                    Gyth = creature->GetGUID();
                    break;
                case NPC_THE_BEAST:
                    TheBeast = creature->GetGUID();
                    break;
                case NPC_GENERAL_DRAKKISATH:
                    GeneralDrakkisath = creature->GetGUID();
                    break;
                case NPC_BLACKHAND_DREADWEAVER:
                case NPC_BLACKHAND_SUMMONER:
                case NPC_BLACKHAND_VETERAN:
                    break;
             }
         }

        void OnGameObjectCreate(GameObject* go)
        {
            switch (go->GetEntry())
            {
                case GO_WHELP_SPAWNER:
                    go->CastSpell(NULL, SPELL_SUMMON_ROOKERY_WHELP);
                    break;
                case GO_EMBERSEER_IN:
                    go_emberseerin = go->GetGUID();
                    break;
                case GO_DOORS:
                    go_doors = go->GetGUID();
                    break;
                case GO_EMBERSEER_OUT:
                    go_emberseerout = go->GetGUID();
                    break;
                case GO_ROOM_1_RUNE:
                    go_roomrunes[0] = go->GetGUID();
                    break;
                case GO_ROOM_2_RUNE:
                    go_roomrunes[1] = go->GetGUID();
                    break;
                case GO_ROOM_3_RUNE:
                    go_roomrunes[2] = go->GetGUID();
                    break;
                case GO_ROOM_4_RUNE:
                    go_roomrunes[3] = go->GetGUID();
                    break;
                case GO_ROOM_5_RUNE:
                    go_roomrunes[4] = go->GetGUID();
                    break;
                case GO_ROOM_6_RUNE:
                    go_roomrunes[5] = go->GetGUID();
                    break;
                case GO_ROOM_7_RUNE:
                    go_roomrunes[6] = go->GetGUID();
                    break;
            }
        }

        bool SetBossState(uint32 type, EncounterState state)
        {
            if (!InstanceScript::SetBossState(type, state))
                return false;

            switch (type)
            {
                case DATA_OMOKK:
                case DATA_SHADOW_HUNTER_VOSHGAJIN:
                case DATA_WARMASTER_VOONE:
                case DATA_MOTHER_SMOLDERWEB:
                case DATA_UROK_DOOMHOWL:
                case DATA_QUARTERMASTER_ZIGRIS:
                case DATA_GIZRUL_THE_SLAVENER:
                case DATA_HALYCON:
                case DATA_OVERLORD_WYRMTHALAK:
                case DATA_PYROGAURD_EMBERSEER:
                case DATA_WARCHIEF_REND_BLACKHAND:
                case DATA_GYTH:
                case DATA_THE_BEAST:
                case DATA_GENERAL_DRAKKISATH:
                    break;
                default:
                    break;
            }

             return true;
        }

        void ProcessEvent(WorldObject* /*gameObject*/, uint32 eventId)
        {
            switch (eventId)
            {
                case EVENT_PYROGUARD_EMBERSEER:
                    if (GetBossState(DATA_PYROGAURD_EMBERSEER) == NOT_STARTED)
                        SetBossState(DATA_PYROGAURD_EMBERSEER, IN_PROGRESS);
                    break;
                default:
                    break;
            }
        }

        uint64 GetData64(uint32 type) const
        {
            switch (type)
            {
                case DATA_OMOKK:
                    return HighlordOmokk;
                case DATA_SHADOW_HUNTER_VOSHGAJIN:
                    return ShadowHunterVoshgajin;
                case DATA_WARMASTER_VOONE:
                    return WarMasterVoone;
                case DATA_MOTHER_SMOLDERWEB:
                    return MotherSmolderweb;
                case DATA_UROK_DOOMHOWL:
                    return UrokDoomhowl;
                case DATA_QUARTERMASTER_ZIGRIS:
                    return QuartermasterZigris;
                case DATA_GIZRUL_THE_SLAVENER:
                    return GizrultheSlavener;
                case DATA_HALYCON:
                    return Halycon;
                case DATA_OVERLORD_WYRMTHALAK:
                    return OverlordWyrmthalak;
                case DATA_PYROGAURD_EMBERSEER:
                    return PyroguardEmberseer;
                case DATA_WARCHIEF_REND_BLACKHAND:
                    return WarchiefRendBlackhand;
                case DATA_GYTH:
                    return Gyth;
                case DATA_THE_BEAST:
                    return TheBeast;
                case DATA_GENERAL_DRAKKISATH:
                    return GeneralDrakkisath;
                case GO_EMBERSEER_IN:
                    return go_emberseerin;
                case GO_DOORS:
                    return go_doors;
                case GO_EMBERSEER_OUT:
                    return go_emberseerout;
                case GO_ROOM_1_RUNE:
                    return go_roomrunes[0];
                case GO_ROOM_2_RUNE:
                    return go_roomrunes[1];
                case GO_ROOM_3_RUNE:
                    return go_roomrunes[2];
                case GO_ROOM_4_RUNE:
                    return go_roomrunes[3];
                case GO_ROOM_5_RUNE:
                    return go_roomrunes[4];
                case GO_ROOM_6_RUNE:
                    return go_roomrunes[5];
                case GO_ROOM_7_RUNE:
                    return go_roomrunes[6];
            }

            return 0;
        }
    };

};

void AddSC_instance_blackrock_spire()
{
    new instance_blackrock_spire();
}
