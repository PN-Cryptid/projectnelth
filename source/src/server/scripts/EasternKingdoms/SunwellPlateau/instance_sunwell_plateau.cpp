/*
 * Copyright (C) 2008-2010 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2006-2009 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
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
SDName: Instance_Sunwell_Plateau
SD%Complete: 25
SDComment: VERIFY SCRIPT
SDCategory: Sunwell_Plateau
EndScriptData */

#include "InstanceScript.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "sunwell_plateau.h"

#define MAX_ENCOUNTER 6

/* Sunwell Plateau:
0 - Kalecgos and Sathrovarr
1 - Brutallus
2 - Felmyst
3 - Eredar Twins (Alythess and Sacrolash)
4 - M'uru
5 - Kil'Jaeden
*/

class instance_sunwell_plateau : public InstanceMapScript
{
public:
    instance_sunwell_plateau() : InstanceMapScript("instance_sunwell_plateau", 580) { }

    InstanceScript* GetInstanceScript(InstanceMap* pMap) const
    {
        return new instance_sunwell_plateau_InstanceMapScript(pMap);
    }

    struct instance_sunwell_plateau_InstanceMapScript : public InstanceScript
    {
        instance_sunwell_plateau_InstanceMapScript(Map* pMap) : InstanceScript(pMap) {};

        uint32 m_auiEncounter[MAX_ENCOUNTER];

        bool modus80;
        /** Creatures **/
        uint64 Kalecgos_Dragon;
        uint64 Kalecgos_Human;
        uint64 Sathrovarr;
        uint64 Brutallus;
        uint64 Madrigosa;
        uint64 Felmyst;
        uint64 Alythess;
        uint64 Sacrolash;
        uint64 Muru;
        uint64 KilJaeden;
        uint64 KilJaedenController;
        uint64 Anveena;
        uint64 KalecgosKJ;
        uint32 SpectralPlayers;
        uint64 QuelDelarIntro;

        /** GameObjects **/
        uint64 ForceField;                                      // Kalecgos Encounter
        uint64 KalecgosWall[2];
        uint64 FireBarrier;                                     // Felmysts Encounter
        uint64 MurusGate[2];                                    // Murus Encounter
        uint64 Orbs[4];                                         //Kiljaeden Encounter

        /*** Misc ***/
        uint32 SpectralRealmTimer;
        std::vector<uint64> SpectralRealmList;
        std::vector<uint64> deceiverList;

        void Initialize()
        {
            SetHeaders(DataHeader);
            memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));

            modus80 = false;
            /*** Creatures ***/
            Kalecgos_Dragon         = 0;
            Kalecgos_Human          = 0;
            Sathrovarr              = 0;
            Brutallus               = 0;
            Madrigosa               = 0;
            Felmyst                 = 0;
            Alythess                = 0;
            Sacrolash               = 0;
            Muru                    = 0;
            KilJaeden               = 0;
            KilJaedenController     = 0;
            Anveena                 = 0;
            KalecgosKJ              = 0;
            SpectralPlayers         = 0;
            QuelDelarIntro          = 0;

            /*** GameObjects ***/
            ForceField  = 0;
            FireBarrier = 0;
            MurusGate[0] = 0;
            MurusGate[1] = 0;
            KalecgosWall[0] = 0;
            KalecgosWall[1] = 0;
            Orbs[0] = 0;
            Orbs[1] = 0;
            Orbs[2] = 0;
            Orbs[3] = 0;

            /*** Misc ***/
            SpectralRealmTimer = 5000;
        }

        bool IsEncounterInProgress() const
        {
            for (uint8 i = 0; i < MAX_ENCOUNTER; ++i)
                if (m_auiEncounter[i] == IN_PROGRESS)
                    return true;

            return false;
        }

        Player const* GetPlayerInMap() const
        {
            Map::PlayerList const& players = instance->GetPlayers();

            if (!players.isEmpty())
            {
                for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
                {
                    if (Player* player = itr->getSource())
                        if (!player->HasAura(45839,0))
                            return player;
                }
            }

            TC_LOG_DEBUG("scripts", "Instance Sunwell Plateau: GetPlayerInMap, but PlayerList is empty!");
            return NULL;
        }

        void OnCreatureCreate(Creature* creature)
        {
            InstanceScript::OnCreatureCreate(creature);

            switch (creature->GetEntry())
            {
                case 24850: Kalecgos_Dragon     = creature->GetGUID(); break;
                case 24891: Kalecgos_Human      = creature->GetGUID(); break;
                case 24892: Sathrovarr          = creature->GetGUID(); break;
                case 24882: Brutallus           = creature->GetGUID(); break;
                case 25160: Madrigosa           = creature->GetGUID(); break;
                case 25038: Felmyst             = creature->GetGUID(); break;
                case 25166: Alythess            = creature->GetGUID(); break;
                case 25165: Sacrolash           = creature->GetGUID(); break;
                case 25741: Muru                = creature->GetGUID(); break;
                case 25315: KilJaeden           = creature->GetGUID(); break;
                case 25608: KilJaedenController = creature->GetGUID(); break;
                case 26046: Anveena             = creature->GetGUID(); break;
                case 25319: KalecgosKJ          = creature->GetGUID(); break;
                case 38056: QuelDelarIntro      = creature->GetGUID(); break;
                case 25588: deceiverList.push_back(creature->GetGUID()); break;
            }
        }

        void OnGameObjectCreate(GameObject* go)
        {
            switch (go->GetEntry())
            {
                case 188421: ForceField = go->GetGUID(); break;
                case 188523: KalecgosWall[0] = go->GetGUID(); break;
                case 188524: KalecgosWall[1] = go->GetGUID(); break;
                case 188075:
                    if (m_auiEncounter[DATA_FELMYST_EVENT] == DONE)
                        HandleGameObject(0, true, go);
                    FireBarrier = go->GetGUID();
                    break;
                case 187990: MurusGate[0] = go->GetGUID(); break;
                case 188118:
                    if (m_auiEncounter[DATA_MURU_EVENT] == DONE)
                        HandleGameObject(0, true, go);
                    MurusGate[1]= go->GetGUID();
                    break;
                case GAMEOBJECT_ORB_OF_THE_BLUE_FLIGHT_1: //orbs_of_blue_flight, boss Kiljaeden
                    Orbs[0] = go->GetGUID();
                    go->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NOT_SELECTABLE);
                    break;
                case GAMEOBJECT_ORB_OF_THE_BLUE_FLIGHT_2:
                    Orbs[1] = go->GetGUID();
                    go->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NOT_SELECTABLE);
                    break;
                case GAMEOBJECT_ORB_OF_THE_BLUE_FLIGHT_3:
                    Orbs[2] = go->GetGUID();
                    go->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NOT_SELECTABLE);
                    break;
                case GAMEOBJECT_ORB_OF_THE_BLUE_FLIGHT_4:
                    Orbs[3] = go->GetGUID();
                    go->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NOT_SELECTABLE);
                    break;
                default:
                    break;
            }
        }

        uint32 GetData(uint32 id) const
        {
            switch (id)
            {
                case DATA_KALECGOS_EVENT:       return m_auiEncounter[0];
                case DATA_BRUTALLUS_EVENT:      return m_auiEncounter[1];
                case DATA_FELMYST_EVENT:        return m_auiEncounter[2];
                case DATA_EREDAR_TWINS_EVENT:   return m_auiEncounter[3];
                case DATA_MURU_EVENT:           return m_auiEncounter[4];
                case DATA_KILJAEDEN_EVENT:      return m_auiEncounter[5];
                case DATA_MODUS80:              return modus80 ? 1 : 0;
            }
            return 0;
        }

        uint64 GetData64(uint32 id) const
        {
            switch (id)
            {
                case DATA_KALECGOS_DRAGON:      return Kalecgos_Dragon;
                case DATA_KALECGOS_HUMAN:       return Kalecgos_Human;
                case DATA_SATHROVARR:           return Sathrovarr;
                case DATA_GO_FORCEFIELD:        return ForceField;
                case DATA_BRUTALLUS:            return Brutallus;
                case DATA_MADRIGOSA:            return Madrigosa;
                case DATA_FELMYST:              return Felmyst;
                case DATA_ALYTHESS:             return Alythess;
                case DATA_SACROLASH:            return Sacrolash;
                case DATA_MURU:                 return Muru;
                case DATA_KILJAEDEN:            return KilJaeden;
                case DATA_KILJAEDEN_CONTROLLER: return KilJaedenController;
                case DATA_ANVEENA:              return Anveena;
                case DATA_KALECGOS_KJ:          return KalecgosKJ;
                case DATA_QUELDELAR_INTRODUCER: return QuelDelarIntro;
                case DATA_PLAYER_GUID:
                    if (Player const* Target = GetPlayerInMap())
                        return Target->GetGUID();
                    return 0;
                case DATA_ORB_OF_THE_BLUE_FLIGHT_1: return Orbs[0];
                case DATA_ORB_OF_THE_BLUE_FLIGHT_2: return Orbs[1];
                case DATA_ORB_OF_THE_BLUE_FLIGHT_3: return Orbs[2];
                case DATA_ORB_OF_THE_BLUE_FLIGHT_4: return Orbs[3];
            }
            return 0;
        }

        void SetData(uint32 id, uint32 data)
        {
            switch (id)
            {
                case DATA_KALECGOS_EVENT:
                    {
                        if (data == NOT_STARTED || data == DONE)
                        {
                            HandleGameObject(ForceField,true);
                            HandleGameObject(KalecgosWall[0],true);
                            HandleGameObject(KalecgosWall[1],true);
                        }
                        else if (data == IN_PROGRESS)
                        {
                            HandleGameObject(ForceField,false);
                            HandleGameObject(KalecgosWall[0],false);
                            HandleGameObject(KalecgosWall[1],false);
                        }
                        m_auiEncounter[0] = data;
                    }
                    break;
                case DATA_BRUTALLUS_EVENT:     m_auiEncounter[1] = data; break;
                case DATA_FELMYST_EVENT:
                    if (data == DONE)
                        HandleGameObject(FireBarrier, true);
                    m_auiEncounter[2] = data; break;
                case DATA_EREDAR_TWINS_EVENT:  m_auiEncounter[3] = data; break;
                case DATA_MURU_EVENT:
                    switch (data)
                    {
                        case DONE:
                            HandleGameObject(MurusGate[0], true);
                            HandleGameObject(MurusGate[1], true);
                            break;
                        case IN_PROGRESS:
                            HandleGameObject(MurusGate[0], false);
                            HandleGameObject(MurusGate[1], false);
                            break;
                        case NOT_STARTED:
                            HandleGameObject(MurusGate[0], true);
                            HandleGameObject(MurusGate[1], false);
                            break;
                    }
                    m_auiEncounter[4] = data; break;
                case DATA_KILJAEDEN_EVENT:     m_auiEncounter[5] = data; break;
                case DATA_MODUS80:
                    modus80 = data ? true : false;
                    for (std::vector<uint64>::iterator itr = deceiverList.begin(); itr != deceiverList.end(); ++itr)
                    {
                        if (Creature* deceiver = instance->GetCreature(*itr))
                            deceiver->AI()->Reset();
                    }

                    break;
            }

            if (data == DONE)
                SaveToDB();
        }

        std::string GetSaveData()
        {
            OUT_SAVE_INST_DATA;
            std::ostringstream stream;
            stream << m_auiEncounter[0] << ' '  << m_auiEncounter[1] << ' '  << m_auiEncounter[2] << ' '  << m_auiEncounter[3] << ' '
                << m_auiEncounter[4] << ' '  << m_auiEncounter[5];

            OUT_SAVE_INST_DATA_COMPLETE;
            return stream.str();
        }

        void Load(const char* in)
        {
            if (!in)
            {
                OUT_LOAD_INST_DATA_FAIL;
                return;
            }

            OUT_LOAD_INST_DATA(in);
            std::istringstream stream(in);
            stream >> m_auiEncounter[0] >> m_auiEncounter[1] >> m_auiEncounter[2] >> m_auiEncounter[3]
                >> m_auiEncounter[4] >> m_auiEncounter[5];
            for (uint8 i = 0; i < MAX_ENCOUNTER; ++i)
                if (m_auiEncounter[i] == IN_PROGRESS)                // Do not load an encounter as "In Progress" - reset it instead.
                    m_auiEncounter[i] = NOT_STARTED;
            OUT_LOAD_INST_DATA_COMPLETE;

            if ((GetData(DATA_BRUTALLUS_EVENT) == DONE) && (GetData(DATA_FELMYST_EVENT) != DONE) && (GetData64(DATA_FELMYST) == 0))
            {
                Position pos;
                pos.Relocate(1460.0f, 637.0f, 50.0f);
                instance->SummonCreature(25038, pos);
            }
        }
    };

};


void AddSC_instance_sunwell_plateau()
{
    new instance_sunwell_plateau();
}
