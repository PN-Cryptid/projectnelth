/*
 * Copyright (C) 2008-2013 TrinityCore <http://www.trinitycore.org/>
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
SDName: Npcs_Special
SD%Complete: 100
SDComment: To be used for special NPCs that are located globally.
SDCategory: NPCs
EndScriptData
*/

/* ContentData
npc_air_force_bots       80%    support for misc (invisible) guard bots in areas where player allowed to fly. Summon guards after a preset time if tagged by spell
npc_lunaclaw_spirit      80%    support for quests 6001/6002 (Body and Heart)
npc_chicken_cluck       100%    support for quest 3861 (Cluck!)
npc_dancing_flames      100%    midsummer event NPC
npc_guardian            100%    guardianAI used to prevent players from accessing off-limits areas. Not in use by SD2
npc_garments_of_quests   80%    NPC's related to all Garments of-quests 5621, 5624, 5625, 5648, 565
npc_injured_patient     100%    patients for triage-quests (6622 and 6624)
npc_doctor              100%    Gustaf Vanhowzen and Gregory Victor, quest 6622 and 6624 (Triage)
npc_mount_vendor        100%    Regular mount vendors all over the world. Display gossip if player doesn't meet the requirements to buy
npc_rogue_trainer        80%    Scripted trainers, so they are able to offer item 17126 for class quest 6681
npc_sayge               100%    Darkmoon event fortune teller, buff player based on answers given
npc_snake_trap_serpents  80%    AI for snakes that summoned by Snake Trap
npc_shadowfiend         100%   restore 5% of owner's mana when shadowfiend die from damage
npc_locksmith            75%    list of keys needs to be confirmed
npc_firework            100%    NPC's summoned by rockets and rocket clusters, for making them cast visual
EndContentData */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "ScriptedEscortAI.h"
#include "ObjectMgr.h"
#include "ScriptMgr.h"
#include "World.h"
#include "PetAI.h"
#include "PassiveAI.h"
#include "CombatAI.h"
#include "GameEventMgr.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Cell.h"
#include "CellImpl.h"
#include "SpellAuras.h"
#include "Pet.h"
#include "Chat.h"
#include "Transport.h"
#include <string>


/*########
# npc_air_force_bots
#########*/
enum SpawnType
{
    SPAWNTYPE_TRIPWIRE_ROOFTOP,                             // no warning, summon Creature at smaller range
    SPAWNTYPE_ALARMBOT,                                     // cast guards mark and summon npc - if player shows up with that buff duration < 5 seconds attack
};

struct SpawnAssociation
{
    uint32 thisCreatureEntry;
    uint32 spawnedCreatureEntry;
    SpawnType spawnType;
};

enum eEnums
{
    SPELL_GUARDS_MARK               = 38067,
    AURA_DURATION_TIME_LEFT         = 5000,
    AURA_CHECK_TIMER                = 25000
};

float const RANGE_TRIPWIRE          = 15.0f;
float const RANGE_GUARDS_MARK       = 50.0f;

SpawnAssociation spawnAssociations[] =
{
    {2614,  15241, SPAWNTYPE_ALARMBOT},                     //Air Force Alarm Bot (Alliance)
    {2615,  15242, SPAWNTYPE_ALARMBOT},                     //Air Force Alarm Bot (Horde)
    {21974, 21976, SPAWNTYPE_ALARMBOT},                     //Air Force Alarm Bot (Area 52)
    {21993, 15242, SPAWNTYPE_ALARMBOT},                     //Air Force Guard Post (Horde - Bat Rider)
    {21996, 15241, SPAWNTYPE_ALARMBOT},                     //Air Force Guard Post (Alliance - Gryphon)
    {21997, 21976, SPAWNTYPE_ALARMBOT},                     //Air Force Guard Post (Goblin - Area 52 - Zeppelin)
    {21999, 15241, SPAWNTYPE_TRIPWIRE_ROOFTOP},             //Air Force Trip Wire - Rooftop (Alliance)
    {22001, 15242, SPAWNTYPE_TRIPWIRE_ROOFTOP},             //Air Force Trip Wire - Rooftop (Horde)
    {22002, 15242, SPAWNTYPE_TRIPWIRE_ROOFTOP},             //Air Force Trip Wire - Ground (Horde)
    {22003, 15241, SPAWNTYPE_TRIPWIRE_ROOFTOP},             //Air Force Trip Wire - Ground (Alliance)
    {22063, 21976, SPAWNTYPE_TRIPWIRE_ROOFTOP},             //Air Force Trip Wire - Rooftop (Goblin - Area 52)
    {22065, 22064, SPAWNTYPE_ALARMBOT},                     //Air Force Guard Post (Ethereal - Stormspire)
    {22066, 22067, SPAWNTYPE_ALARMBOT},                     //Air Force Guard Post (Scryer - Dragonhawk)
    {22068, 22064, SPAWNTYPE_TRIPWIRE_ROOFTOP},             //Air Force Trip Wire - Rooftop (Ethereal - Stormspire)
    {22069, 22064, SPAWNTYPE_ALARMBOT},                     //Air Force Alarm Bot (Stormspire)
    {22070, 22067, SPAWNTYPE_TRIPWIRE_ROOFTOP},             //Air Force Trip Wire - Rooftop (Scryer)
    {22071, 22067, SPAWNTYPE_ALARMBOT},                     //Air Force Alarm Bot (Scryer)
    {22078, 22077, SPAWNTYPE_ALARMBOT},                     //Air Force Alarm Bot (Aldor)
    {22079, 22077, SPAWNTYPE_ALARMBOT},                     //Air Force Guard Post (Aldor - Gryphon)
    {22080, 22077, SPAWNTYPE_TRIPWIRE_ROOFTOP},             //Air Force Trip Wire - Rooftop (Aldor)
    {22086, 22085, SPAWNTYPE_ALARMBOT},                     //Air Force Alarm Bot (Sporeggar)
    {22087, 22085, SPAWNTYPE_ALARMBOT},                     //Air Force Guard Post (Sporeggar - Spore Bat)
    {22088, 22085, SPAWNTYPE_TRIPWIRE_ROOFTOP},             //Air Force Trip Wire - Rooftop (Sporeggar)
    {22090, 22089, SPAWNTYPE_ALARMBOT},                     //Air Force Guard Post (Toshley's Station - Flying Machine)
    {22124, 22122, SPAWNTYPE_ALARMBOT},                     //Air Force Alarm Bot (Cenarion)
    {22125, 22122, SPAWNTYPE_ALARMBOT},                     //Air Force Guard Post (Cenarion - Stormcrow)
    {22126, 22122, SPAWNTYPE_ALARMBOT}                      //Air Force Trip Wire - Rooftop (Cenarion Expedition)
};

class npc_air_force_bots : public CreatureScript
{
public:
    npc_air_force_bots() : CreatureScript("npc_air_force_bots") { }

    struct npc_air_force_botsAI : public ScriptedAI
    {
        npc_air_force_botsAI(Creature* creature) : ScriptedAI(creature)
        {
            SpawnAssoc = NULL;
            SpawnedGUID = 0;

            // find the correct spawnhandling
            static uint32 entryCount = sizeof(spawnAssociations) / sizeof(SpawnAssociation);

            for (uint8 i = 0; i < entryCount; ++i)
            {
                if (spawnAssociations[i].thisCreatureEntry == creature->GetEntry())
                {
                    SpawnAssoc = &spawnAssociations[i];
                    break;
                }
            }

            if (!SpawnAssoc)
                TC_LOG_ERROR("sql.sql", "TCSR: Creature template entry %u has ScriptName npc_air_force_bots, but it's not handled by that script", creature->GetEntry());
            else
            {
                CreatureTemplate const* spawnedTemplate = sObjectMgr->GetCreatureTemplate(SpawnAssoc->spawnedCreatureEntry);

                if (!spawnedTemplate)
                {
                    TC_LOG_ERROR("sql.sql", "TCSR: Creature template entry %u does not exist in DB, which is required by npc_air_force_bots", SpawnAssoc->spawnedCreatureEntry);
                    SpawnAssoc = NULL;
                    return;
                }
            }
        }

        SpawnAssociation* SpawnAssoc;
        uint64 SpawnedGUID;
        std::set<uint64> targetGuids;
        EventMap playerCheckMap;

        void Reset() {}

        Creature* SummonGuard()
        {
            Creature* summoned = me->SummonCreature(SpawnAssoc->spawnedCreatureEntry, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 300000);

            if (summoned)
                SpawnedGUID = summoned->GetGUID();
            else
            {
                TC_LOG_ERROR("sql.sql", "TCSR: npc_air_force_bots: wasn't able to spawn Creature %u", SpawnAssoc->spawnedCreatureEntry);
                SpawnAssoc = NULL;
            }

            return summoned;
        }

        Creature* GetSummonedGuard()
        {
            Creature* creature = Unit::GetCreature(*me, SpawnedGUID);

            if (creature && creature->isAlive())
                return creature;

            return NULL;
        }

        void MoveInLineOfSight(Unit* who)
        {
            if (!SpawnAssoc)
                return;

            if (me->IsValidAttackTarget(who))
            {
                if (who->GetTypeId() == TYPEID_PLAYER)
                {
                    if (Aura* markAura = who->GetAura(SPELL_GUARDS_MARK))
                    {
                        if (!playerCheckMap.GetNextEventTime(who->GetGUIDLow()))
                            playerCheckMap.ScheduleEvent(who->GetGUIDLow(), (markAura->GetDuration() > AURA_DURATION_TIME_LEFT ? markAura->GetDuration() - AURA_DURATION_TIME_LEFT : 0));
                    }
                    else
                        targetGuids.insert(who->GetGUID());
                }
            }
        }

        void UpdateAI(uint32 const diff)
        {
            if (!SpawnAssoc)
            {
                me->IsAIEnabled = false;
                return;
            }

            playerCheckMap.Update(diff);

            // check players with applied aura and expired timer
            while (uint32 playerGuidLow = playerCheckMap.ExecuteEvent())
            {
                if (Player* player = ObjectAccessor::GetPlayer(*me, MAKE_NEW_GUID(playerGuidLow, 0, HIGHGUID_PLAYER)))
                {
                    Aura* markAura = player->GetAura(SPELL_GUARDS_MARK);
                    if (markAura)
                    {
                        Creature* lastSpawnedGuard = SpawnedGUID == 0 ? NULL : GetSummonedGuard();

                        if (!lastSpawnedGuard)
                        {
                            SpawnedGUID = 0;
                            lastSpawnedGuard = SummonGuard();

                            if (!lastSpawnedGuard)
                                continue;
                        }

                        // the player wasn't able to move out of our range within 25 seconds
                        if (player->IsWithinDistInMap(me, RANGE_GUARDS_MARK))
                            if (!lastSpawnedGuard->getVictim())
                            {
                                lastSpawnedGuard->AI()->AttackStart(player);
                                continue;
                            }

                            // check some updates later if we can/should attack
                            playerCheckMap.ScheduleEvent(playerGuidLow, 500);
                    }
                }
            }

            // handle players which MoveInLineOfSight
            for (std::set<uint64>::const_iterator itr = targetGuids.begin(); itr != targetGuids.end(); ++itr)
            {
                if (Player* playerTarget = ObjectAccessor::GetPlayer(*me, *itr))
                {
                    Creature* lastSpawnedGuard = SpawnedGUID == 0 ? NULL : GetSummonedGuard();

                    // prevent calling Unit::GetUnit at next call - speedup
                    if (!lastSpawnedGuard)
                        SpawnedGUID = 0;

                    switch (SpawnAssoc->spawnType)
                    {
                    case SPAWNTYPE_ALARMBOT:
                        {
                            // only mark players in range and not marked already
                            if (!playerTarget->IsWithinDistInMap(me, RANGE_GUARDS_MARK) || playerTarget->HasAura(SPELL_GUARDS_MARK))
                                break;

                            if (!lastSpawnedGuard)
                                lastSpawnedGuard = SummonGuard();

                            if (!lastSpawnedGuard)
                                break;

                            lastSpawnedGuard->CastSpell(playerTarget, SPELL_GUARDS_MARK, true);
                            playerCheckMap.RescheduleEvent(playerTarget->GetGUIDLow(), AURA_CHECK_TIMER);

                            break;
                        }
                    case SPAWNTYPE_TRIPWIRE_ROOFTOP:
                        {
                            if (!playerTarget->IsWithinDistInMap(me, RANGE_TRIPWIRE))
                                return;

                            if (!lastSpawnedGuard)
                                lastSpawnedGuard = SummonGuard();

                            if (!lastSpawnedGuard)
                                return;

                            // ROOFTOP only triggers if the player is on the ground
                            if (!playerTarget->IsFlying() && !lastSpawnedGuard->getVictim())
                                lastSpawnedGuard->AI()->AttackStart(playerTarget);

                            break;
                        }
                    }
                }
            }
            targetGuids.clear();
        }

    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_air_force_botsAI(creature);
    }
};
/*######
## npc_lunaclaw_spirit
######*/

enum
{
    QUEST_BODY_HEART_A      = 6001,
    QUEST_BODY_HEART_H      = 6002,

    TEXT_ID_DEFAULT         = 4714,
    TEXT_ID_PROGRESS        = 4715
};

#define GOSSIP_ITEM_GRANT   "You have thought well, spirit. I ask you to grant me the strength of your body and the strength of your heart."

class npc_lunaclaw_spirit : public CreatureScript
{
public:
    npc_lunaclaw_spirit() : CreatureScript("npc_lunaclaw_spirit") { }

    bool OnGossipHello(Player* player, Creature* creature)
    {
        if (player->GetQuestStatus(QUEST_BODY_HEART_A) == QUEST_STATUS_INCOMPLETE || player->GetQuestStatus(QUEST_BODY_HEART_H) == QUEST_STATUS_INCOMPLETE)
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_ITEM_GRANT, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);

        player->SEND_GOSSIP_MENU(TEXT_ID_DEFAULT, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action)
    {
        player->PlayerTalkClass->ClearMenus();
        if (action == GOSSIP_ACTION_INFO_DEF + 1)
        {
            player->SEND_GOSSIP_MENU(TEXT_ID_PROGRESS, creature->GetGUID());
            player->AreaExploredOrEventHappens(player->GetTeam() == ALLIANCE ? QUEST_BODY_HEART_A : QUEST_BODY_HEART_H);
        }
        return true;
    }
};

/*########
# npc_chicken_cluck
#########*/

enum ChickenCluck
{
    EMOTE_HELLO_A       = 0,
    EMOTE_HELLO_H       = 1,
    EMOTE_CLUCK_TEXT    = 2,

    QUEST_CLUCK         = 3861,
    FACTION_FRIENDLY    = 35,
    FACTION_CHICKEN     = 31
};

class npc_chicken_cluck : public CreatureScript
{
public:
    npc_chicken_cluck() : CreatureScript("npc_chicken_cluck") { }

    struct npc_chicken_cluckAI : public ScriptedAI
    {
        npc_chicken_cluckAI(Creature* creature) : ScriptedAI(creature) {}

        uint32 ResetFlagTimer;

        void Reset()
        {
            ResetFlagTimer = 120000;
            me->setFaction(FACTION_CHICKEN);
            me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
        }

        void EnterCombat(Unit* /*who*/) {}

        void UpdateAI(uint32 const diff)
        {
            // Reset flags after a certain time has passed so that the next player has to start the 'event' again
            if (me->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER))
            {
                if (ResetFlagTimer <= diff)
                {
                    EnterEvadeMode();
                    return;
                }
                else
                    ResetFlagTimer -= diff;
            }

            if (UpdateVictim())
                DoMeleeAttackIfReady();
        }

        void ReceiveEmote(Player* player, uint32 emote)
        {
            switch (emote)
            {
                case TEXT_EMOTE_CHICKEN:
                    if (player->GetQuestStatus(QUEST_CLUCK) == QUEST_STATUS_NONE && rand() % 30 == 1)
                    {
                        me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                        me->setFaction(FACTION_FRIENDLY);
                        Talk(player->GetTeam() == HORDE ? EMOTE_HELLO_H : EMOTE_HELLO_A);
                    }
                    break;
                case TEXT_EMOTE_CHEER:
                    if (player->GetQuestStatus(QUEST_CLUCK) == QUEST_STATUS_COMPLETE)
                    {
                        me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                        me->setFaction(FACTION_FRIENDLY);
                        Talk(EMOTE_CLUCK_TEXT);
                    }
                    break;
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_chicken_cluckAI(creature);
    }

    bool OnQuestAccept(Player* /*player*/, Creature* creature, Quest const* quest)
    {
        if (quest->GetQuestId() == QUEST_CLUCK)
            CAST_AI(npc_chicken_cluck::npc_chicken_cluckAI, creature->AI())->Reset();

        return true;
    }

    bool OnQuestComplete(Player* /*player*/, Creature* creature, Quest const* quest)
    {
        if (quest->GetQuestId() == QUEST_CLUCK)
            CAST_AI(npc_chicken_cluck::npc_chicken_cluckAI, creature->AI())->Reset();

        return true;
    }
};

/*######
## npc_dancing_flames
######*/

#define SPELL_BRAZIER       45423
#define SPELL_SEDUCTION     47057
#define SPELL_FIERY_AURA    45427

class npc_dancing_flames : public CreatureScript
{
public:
    npc_dancing_flames() : CreatureScript("npc_dancing_flames") { }

    struct npc_dancing_flamesAI : public ScriptedAI
    {
        npc_dancing_flamesAI(Creature* creature) : ScriptedAI(creature) {}

        bool Active;
        uint32 CanIteract;

        void Reset()
        {
            Active = true;
            CanIteract = 3500;
            DoCast(me, SPELL_BRAZIER, true);
            DoCast(me, SPELL_FIERY_AURA, false);
            float x, y, z;
            me->GetPosition(x, y, z);
            me->Relocate(x, y, z + 0.94f);
            me->SetDisableGravity(true);
            me->HandleEmoteCommand(EMOTE_ONESHOT_DANCE);
        }

        void UpdateAI(uint32 const diff)
        {
            if (!Active)
            {
                if (CanIteract <= diff)
                {
                    Active = true;
                    CanIteract = 3500;
                    me->HandleEmoteCommand(EMOTE_ONESHOT_DANCE);
                }
                else
                    CanIteract -= diff;
            }
        }

        void EnterCombat(Unit* /*who*/){}

        void ReceiveEmote(Player* player, uint32 emote)
        {
            if (me->IsWithinLOS(player->GetPositionX(), player->GetPositionY(), player->GetPositionZ()) && me->IsWithinDistInMap(player, 30.0f))
            {
                me->SetInFront(player);
                Active = false;

                switch (emote)
                {
                    case TEXT_EMOTE_KISS:
                        me->HandleEmoteCommand(EMOTE_ONESHOT_SHY);
                        break;
                    case TEXT_EMOTE_WAVE:
                        me->HandleEmoteCommand(EMOTE_ONESHOT_WAVE);
                        break;
                    case TEXT_EMOTE_BOW:
                        me->HandleEmoteCommand(EMOTE_ONESHOT_BOW);
                        break;
                    case TEXT_EMOTE_JOKE:
                        me->HandleEmoteCommand(EMOTE_ONESHOT_LAUGH);
                        break;
                    case TEXT_EMOTE_DANCE:
                        if (!player->HasAura(SPELL_SEDUCTION))
                            DoCast(player, SPELL_SEDUCTION, true);
                        break;
                }
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_dancing_flamesAI(creature);
    }
};

/*######
## Triage quest
######*/

enum Doctor
{
    SAY_DOC             = 0,

    DOCTOR_ALLIANCE     = 12939,
    DOCTOR_HORDE        = 12920,
    ALLIANCE_COORDS     = 7,
    HORDE_COORDS        = 6
};

struct Location
{
    float x, y, z, o;
};

static Location AllianceCoords[]=
{
    {-3757.38f, -4533.05f, 14.16f, 3.62f},                      // Top-far-right bunk as seen from entrance
    {-3754.36f, -4539.13f, 14.16f, 5.13f},                      // Top-far-left bunk
    {-3749.54f, -4540.25f, 14.28f, 3.34f},                      // Far-right bunk
    {-3742.10f, -4536.85f, 14.28f, 3.64f},                      // Right bunk near entrance
    {-3755.89f, -4529.07f, 14.05f, 0.57f},                      // Far-left bunk
    {-3749.51f, -4527.08f, 14.07f, 5.26f},                      // Mid-left bunk
    {-3746.37f, -4525.35f, 14.16f, 5.22f},                      // Left bunk near entrance
};

//alliance run to where
#define A_RUNTOX -3742.96f
#define A_RUNTOY -4531.52f
#define A_RUNTOZ 11.91f

static Location HordeCoords[]=
{
    {-1013.75f, -3492.59f, 62.62f, 4.34f},                      // Left, Behind
    {-1017.72f, -3490.92f, 62.62f, 4.34f},                      // Right, Behind
    {-1015.77f, -3497.15f, 62.82f, 4.34f},                      // Left, Mid
    {-1019.51f, -3495.49f, 62.82f, 4.34f},                      // Right, Mid
    {-1017.25f, -3500.85f, 62.98f, 4.34f},                      // Left, front
    {-1020.95f, -3499.21f, 62.98f, 4.34f}                       // Right, Front
};

//horde run to where
#define H_RUNTOX -1016.44f
#define H_RUNTOY -3508.48f
#define H_RUNTOZ 62.96f

uint32 const AllianceSoldierId[3] =
{
    12938,                                                  // 12938 Injured Alliance Soldier
    12936,                                                  // 12936 Badly injured Alliance Soldier
    12937                                                   // 12937 Critically injured Alliance Soldier
};

uint32 const HordeSoldierId[3] =
{
    12923,                                                  //12923 Injured Soldier
    12924,                                                  //12924 Badly injured Soldier
    12925                                                   //12925 Critically injured Soldier
};

/*######
## npc_doctor (handles both Gustaf Vanhowzen and Gregory Victor)
######*/
class npc_doctor : public CreatureScript
{
public:
    npc_doctor() : CreatureScript("npc_doctor") {}

    struct npc_doctorAI : public ScriptedAI
    {
        npc_doctorAI(Creature* creature) : ScriptedAI(creature) {}

        uint64 PlayerGUID;

        uint32 SummonPatientTimer;
        uint32 SummonPatientCount;
        uint32 PatientDiedCount;
        uint32 PatientSavedCount;

        bool Event;

        std::list<uint64> Patients;
        std::vector<Location*> Coordinates;

        void Reset()
        {
            PlayerGUID = 0;

            SummonPatientTimer = 10000;
            SummonPatientCount = 0;
            PatientDiedCount = 0;
            PatientSavedCount = 0;

            Patients.clear();
            Coordinates.clear();

            Event = false;

            me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        }

        void BeginEvent(Player* player)
        {
            PlayerGUID = player->GetGUID();

            SummonPatientTimer = 10000;
            SummonPatientCount = 0;
            PatientDiedCount = 0;
            PatientSavedCount = 0;

            switch (me->GetEntry())
            {
                case DOCTOR_ALLIANCE:
                    for (uint8 i = 0; i < ALLIANCE_COORDS; ++i)
                        Coordinates.push_back(&AllianceCoords[i]);
                    break;
                case DOCTOR_HORDE:
                    for (uint8 i = 0; i < HORDE_COORDS; ++i)
                        Coordinates.push_back(&HordeCoords[i]);
                    break;
            }

            Event = true;
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        }

        void PatientDied(Location* point)
        {
            Player* player = Unit::GetPlayer(*me, PlayerGUID);
            if (player && ((player->GetQuestStatus(6624) == QUEST_STATUS_INCOMPLETE) || (player->GetQuestStatus(6622) == QUEST_STATUS_INCOMPLETE)))
            {
                ++PatientDiedCount;

                if (PatientDiedCount > 5 && Event)
                {
                    if (player->GetQuestStatus(6624) == QUEST_STATUS_INCOMPLETE)
                        player->FailQuest(6624);
                    else if (player->GetQuestStatus(6622) == QUEST_STATUS_INCOMPLETE)
                        player->FailQuest(6622);

                    Reset();
                    return;
                }

                Coordinates.push_back(point);
            }
            else
                // If no player or player abandon quest in progress
                Reset();
        }

        void PatientSaved(Creature* /*soldier*/, Player* player, Location* point)
        {
            if (player && PlayerGUID == player->GetGUID())
            {
                if ((player->GetQuestStatus(6624) == QUEST_STATUS_INCOMPLETE) || (player->GetQuestStatus(6622) == QUEST_STATUS_INCOMPLETE))
                {
                    ++PatientSavedCount;

                    if (PatientSavedCount == 15)
                    {
                        if (!Patients.empty())
                        {
                            std::list<uint64>::const_iterator itr;
                            for (itr = Patients.begin(); itr != Patients.end(); ++itr)
                            {
                                if (Creature* patient = Unit::GetCreature((*me), *itr))
                                    patient->setDeathState(JUST_DIED);
                            }
                        }

                        if (player->GetQuestStatus(6624) == QUEST_STATUS_INCOMPLETE)
                            player->AreaExploredOrEventHappens(6624);
                        else if (player->GetQuestStatus(6622) == QUEST_STATUS_INCOMPLETE)
                            player->AreaExploredOrEventHappens(6622);

                        Reset();
                        return;
                    }

                    Coordinates.push_back(point);
                }
            }
        }

        void UpdateAI(uint32 const diff);

        void EnterCombat(Unit* /*who*/){}
    };

    bool OnQuestAccept(Player* player, Creature* creature, Quest const* quest)
    {
        if ((quest->GetQuestId() == 6624) || (quest->GetQuestId() == 6622))
            CAST_AI(npc_doctor::npc_doctorAI, creature->AI())->BeginEvent(player);

        return true;
    }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_doctorAI(creature);
    }
};

/*#####
## npc_injured_patient (handles all the patients, no matter Horde or Alliance)
#####*/

class npc_injured_patient : public CreatureScript
{
public:
    npc_injured_patient() : CreatureScript("npc_injured_patient") { }

    struct npc_injured_patientAI : public ScriptedAI
    {
        npc_injured_patientAI(Creature* creature) : ScriptedAI(creature) {}

        uint64 DoctorGUID;
        Location* Coord;

        void Reset()
        {
            DoctorGUID = 0;
            Coord = NULL;

            //no select
            me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);

            //no regen health
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT);

            //to make them lay with face down
            me->SetUInt32Value(UNIT_FIELD_BYTES_1, UNIT_STAND_STATE_DEAD);

            uint32 mobId = me->GetEntry();

            switch (mobId)
            {                                                   //lower max health
                case 12923:
                case 12938:                                     //Injured Soldier
                    me->SetHealth(me->CountPctFromMaxHealth(75));
                    break;
                case 12924:
                case 12936:                                     //Badly injured Soldier
                    me->SetHealth(me->CountPctFromMaxHealth(50));
                    break;
                case 12925:
                case 12937:                                     //Critically injured Soldier
                    me->SetHealth(me->CountPctFromMaxHealth(25));
                    break;
            }
        }

        void EnterCombat(Unit* /*who*/){}

        void SpellHit(Unit* caster, SpellInfo const* spell)
        {
            if (caster->GetTypeId() == TYPEID_PLAYER && me->isAlive() && spell->Id == 20804)
            {
                if ((CAST_PLR(caster)->GetQuestStatus(6624) == QUEST_STATUS_INCOMPLETE) || (CAST_PLR(caster)->GetQuestStatus(6622) == QUEST_STATUS_INCOMPLETE))
                    if (DoctorGUID)
                        if (Creature* doctor = Unit::GetCreature(*me, DoctorGUID))
                            CAST_AI(npc_doctor::npc_doctorAI, doctor->AI())->PatientSaved(me, CAST_PLR(caster), Coord);

                //make not selectable
                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);

                //regen health
                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT);

                //stand up
                me->SetUInt32Value(UNIT_FIELD_BYTES_1, UNIT_STAND_STATE_STAND);

                Talk(SAY_DOC);

                uint32 mobId = me->GetEntry();
                me->SetWalk(false);

                switch (mobId)
                {
                    case 12923:
                    case 12924:
                    case 12925:
                        me->GetMotionMaster()->MovePoint(0, H_RUNTOX, H_RUNTOY, H_RUNTOZ);
                        break;
                    case 12936:
                    case 12937:
                    case 12938:
                        me->GetMotionMaster()->MovePoint(0, A_RUNTOX, A_RUNTOY, A_RUNTOZ);
                        break;
                }
            }
        }

        void UpdateAI(uint32 const /*diff*/)
        {
            //lower HP on every world tick makes it a useful counter, not officlone though
            if (me->isAlive() && me->GetHealth() > 6)
                me->ModifyHealth(-5);

            if (me->isAlive() && me->GetHealth() <= 6)
            {
                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT);
                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                me->setDeathState(JUST_DIED);
                me->SetFlag(UNIT_DYNAMIC_FLAGS, 32);

                if (DoctorGUID)
                    if (Creature* doctor = Unit::GetCreature((*me), DoctorGUID))
                        CAST_AI(npc_doctor::npc_doctorAI, doctor->AI())->PatientDied(Coord);
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_injured_patientAI(creature);
    }
};

void npc_doctor::npc_doctorAI::UpdateAI(uint32 const diff)
{
    if (Event && SummonPatientCount >= 20)
    {
        Reset();
        return;
    }

    if (Event)
    {
        if (SummonPatientTimer <= diff)
        {
            if (Coordinates.empty())
                return;

            std::vector<Location*>::iterator itr = Coordinates.begin() + rand() % Coordinates.size();
            uint32 patientEntry = 0;

            switch (me->GetEntry())
            {
                case DOCTOR_ALLIANCE:
                    patientEntry = AllianceSoldierId[rand() % 3];
                    break;
                case DOCTOR_HORDE:
                    patientEntry = HordeSoldierId[rand() % 3];
                    break;
                default:
                    TC_LOG_ERROR("scripts", "Invalid entry for Triage doctor. Please check your database");
                    return;
            }

            if (Location* point = *itr)
            {
                if (Creature* Patient = me->SummonCreature(patientEntry, point->x, point->y, point->z, point->o, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 5000))
                {
                    //303, this flag appear to be required for client side item->spell to work (TARGET_SINGLE_FRIEND)
                    Patient->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE);

                    Patients.push_back(Patient->GetGUID());
                    CAST_AI(npc_injured_patient::npc_injured_patientAI, Patient->AI())->DoctorGUID = me->GetGUID();
                    CAST_AI(npc_injured_patient::npc_injured_patientAI, Patient->AI())->Coord = point;

                    Coordinates.erase(itr);
                }
            }
            SummonPatientTimer = 10000;
            ++SummonPatientCount;
        }
        else
            SummonPatientTimer -= diff;
    }
}

/*######
## npc_garments_of_quests
######*/

//TODO: get text for each NPC

enum Garments
{
    SPELL_LESSER_HEAL_R2    = 2052,
    SPELL_FORTITUDE_R1      = 1243,

    QUEST_MOON              = 5621,
    QUEST_LIGHT_1           = 5624,
    QUEST_LIGHT_2           = 5625,
    QUEST_SPIRIT            = 5648,
    QUEST_DARKNESS          = 5650,

    ENTRY_SHAYA             = 12429,
    ENTRY_ROBERTS           = 12423,
    ENTRY_DOLF              = 12427,
    ENTRY_KORJA             = 12430,
    ENTRY_DG_KEL            = 12428,

    // used by 12429, 12423, 12427, 12430, 12428, but signed for 12429
    SAY_THANKS              = 0,
    SAY_GOODBYE             = 1,
    SAY_HEALED              = 2,
};

class npc_garments_of_quests : public CreatureScript
{
public:
    npc_garments_of_quests() : CreatureScript("npc_garments_of_quests") { }

    struct npc_garments_of_questsAI : public npc_escortAI
    {
        npc_garments_of_questsAI(Creature* creature) : npc_escortAI(creature)
        {
            Reset();
        }

        uint64 CasterGUID;

        bool IsHealed;
        bool CanRun;

        uint32 RunAwayTimer;

        void Reset()
        {
            CasterGUID = 0;

            IsHealed = false;
            CanRun = false;

            RunAwayTimer = 5000;

            me->SetStandState(UNIT_STAND_STATE_KNEEL);
            // expect database to have RegenHealth=0
            me->SetHealth(me->CountPctFromMaxHealth(70));
        }

        void EnterCombat(Unit* /*who*/) { }

        void SpellHit(Unit* caster, SpellInfo const* spell)
        {
            if (spell->Id == SPELL_LESSER_HEAL_R2 || spell->Id == SPELL_FORTITUDE_R1)
            {
                //not while in combat
                if (me->isInCombat())
                    return;

                //nothing to be done now
                if (IsHealed && CanRun)
                    return;

                if (Player* player = caster->ToPlayer())
                {
                    switch (me->GetEntry())
                    {
                        case ENTRY_SHAYA:
                            if (player->GetQuestStatus(QUEST_MOON) == QUEST_STATUS_INCOMPLETE)
                            {
                                if (IsHealed && !CanRun && spell->Id == SPELL_FORTITUDE_R1)
                                {
                                    Talk(SAY_THANKS, caster->GetGUID());
                                    CanRun = true;
                                }
                                else if (!IsHealed && spell->Id == SPELL_LESSER_HEAL_R2)
                                {
                                    CasterGUID = caster->GetGUID();
                                    me->SetStandState(UNIT_STAND_STATE_STAND);
                                    Talk(SAY_HEALED, caster->GetGUID());
                                    IsHealed = true;
                                }
                            }
                            break;
                        case ENTRY_ROBERTS:
                            if (player->GetQuestStatus(QUEST_LIGHT_1) == QUEST_STATUS_INCOMPLETE)
                            {
                                if (IsHealed && !CanRun && spell->Id == SPELL_FORTITUDE_R1)
                                {
                                    Talk(SAY_THANKS, caster->GetGUID());
                                    CanRun = true;
                                }
                                else if (!IsHealed && spell->Id == SPELL_LESSER_HEAL_R2)
                                {
                                    CasterGUID = caster->GetGUID();
                                    me->SetStandState(UNIT_STAND_STATE_STAND);
                                    Talk(SAY_HEALED, caster->GetGUID());
                                    IsHealed = true;
                                }
                            }
                            break;
                        case ENTRY_DOLF:
                            if (player->GetQuestStatus(QUEST_LIGHT_2) == QUEST_STATUS_INCOMPLETE)
                            {
                                if (IsHealed && !CanRun && spell->Id == SPELL_FORTITUDE_R1)
                                {
                                    Talk(SAY_THANKS, caster->GetGUID());
                                    CanRun = true;
                                }
                                else if (!IsHealed && spell->Id == SPELL_LESSER_HEAL_R2)
                                {
                                    CasterGUID = caster->GetGUID();
                                    me->SetStandState(UNIT_STAND_STATE_STAND);
                                    Talk(SAY_HEALED, caster->GetGUID());
                                    IsHealed = true;
                                }
                            }
                            break;
                        case ENTRY_KORJA:
                            if (player->GetQuestStatus(QUEST_SPIRIT) == QUEST_STATUS_INCOMPLETE)
                            {
                                if (IsHealed && !CanRun && spell->Id == SPELL_FORTITUDE_R1)
                                {
                                    Talk(SAY_THANKS, caster->GetGUID());
                                    CanRun = true;
                                }
                                else if (!IsHealed && spell->Id == SPELL_LESSER_HEAL_R2)
                                {
                                    CasterGUID = caster->GetGUID();
                                    me->SetStandState(UNIT_STAND_STATE_STAND);
                                    Talk(SAY_HEALED, caster->GetGUID());
                                    IsHealed = true;
                                }
                            }
                            break;
                        case ENTRY_DG_KEL:
                            if (player->GetQuestStatus(QUEST_DARKNESS) == QUEST_STATUS_INCOMPLETE)
                            {
                                if (IsHealed && !CanRun && spell->Id == SPELL_FORTITUDE_R1)
                                {
                                    Talk(SAY_THANKS, caster->GetGUID());
                                    CanRun = true;
                                }
                                else if (!IsHealed && spell->Id == SPELL_LESSER_HEAL_R2)
                                {
                                    CasterGUID = caster->GetGUID();
                                    me->SetStandState(UNIT_STAND_STATE_STAND);
                                    Talk(SAY_HEALED, caster->GetGUID());
                                    IsHealed = true;
                                }
                            }
                            break;
                    }

                    // give quest credit, not expect any special quest objectives
                    if (CanRun)
                        player->TalkedToCreature(me->GetEntry(), me->GetGUID());
                }
            }
        }

        void WaypointReached(uint32 /*waypointId*/)
        {

        }

        void UpdateAI(uint32 const diff)
        {
            if (CanRun && !me->isInCombat())
            {
                if (RunAwayTimer <= diff)
                {
                    if (Unit* unit = Unit::GetUnit(*me, CasterGUID))
                    {
                        switch (me->GetEntry())
                        {
                            case ENTRY_SHAYA:
                                Talk(SAY_GOODBYE, unit->GetGUID());
                                break;
                            case ENTRY_ROBERTS:
                                Talk(SAY_GOODBYE, unit->GetGUID());
                                break;
                            case ENTRY_DOLF:
                                Talk(SAY_GOODBYE, unit->GetGUID());
                                break;
                            case ENTRY_KORJA:
                                Talk(SAY_GOODBYE, unit->GetGUID());
                                break;
                            case ENTRY_DG_KEL:
                                Talk(SAY_GOODBYE, unit->GetGUID());
                                break;
                        }

                        Start(false, true, true);
                    }
                    else
                        EnterEvadeMode();                       //something went wrong

                    RunAwayTimer = 30000;
                }
                else
                    RunAwayTimer -= diff;
            }

            npc_escortAI::UpdateAI(diff);
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_garments_of_questsAI(creature);
    }
};

/*######
## npc_guardian
######*/

#define SPELL_DEATHTOUCH                5

class npc_guardian : public CreatureScript
{
public:
    npc_guardian() : CreatureScript("npc_guardian") { }

    struct npc_guardianAI : public ScriptedAI
    {
        npc_guardianAI(Creature* creature) : ScriptedAI(creature) {}

        void Reset()
        {
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        }

        void EnterCombat(Unit* /*who*/)
        {
        }

        void UpdateAI(uint32 const /*diff*/)
        {
            if (!UpdateVictim())
                return;

            if (me->isAttackReady())
            {
                DoCastVictim(SPELL_DEATHTOUCH, true);
                me->resetAttackTimer();
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_guardianAI(creature);
    }
};

/*######
## npc_mount_vendor
######*/

class npc_mount_vendor : public CreatureScript
{
public:
    npc_mount_vendor() : CreatureScript("npc_mount_vendor") { }

    bool OnGossipHello(Player* player, Creature* creature)
    {
        if (creature->isQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        bool canBuy = false;
        uint32 vendor = creature->GetEntry();
        uint8 race = player->getRace();

        switch (vendor)
        {
            case 384:                                           //Katie Hunter
            case 1460:                                          //Unger Statforth
            case 2357:                                          //Merideth Carlson
            case 4885:                                          //Gregor MacVince
                if (player->GetReputationRank(72) != REP_EXALTED && race != RACE_HUMAN)
                    player->SEND_GOSSIP_MENU(5855, creature->GetGUID());
                else canBuy = true;
                break;
            case 1261:                                          //Veron Amberstill
                if (player->GetReputationRank(47) != REP_EXALTED && race != RACE_DWARF)
                    player->SEND_GOSSIP_MENU(5856, creature->GetGUID());
                else canBuy = true;
                break;
            case 3362:                                          //Ogunaro Wolfrunner
                if (player->GetReputationRank(76) != REP_EXALTED && race != RACE_ORC)
                    player->SEND_GOSSIP_MENU(5841, creature->GetGUID());
                else canBuy = true;
                break;
            case 3685:                                          //Harb Clawhoof
                if (player->GetReputationRank(81) != REP_EXALTED && race != RACE_TAUREN)
                    player->SEND_GOSSIP_MENU(5843, creature->GetGUID());
                else canBuy = true;
                break;
            case 4730:                                          //Lelanai
                if (player->GetReputationRank(69) != REP_EXALTED && race != RACE_NIGHTELF)
                    player->SEND_GOSSIP_MENU(5844, creature->GetGUID());
                else canBuy = true;
                break;
            case 4731:                                          //Zachariah Post
                if (player->GetReputationRank(68) != REP_EXALTED && race != RACE_UNDEAD_PLAYER)
                    player->SEND_GOSSIP_MENU(5840, creature->GetGUID());
                else canBuy = true;
                break;
            case 7952:                                          //Zjolnir
                if (player->GetReputationRank(530) != REP_EXALTED && race != RACE_TROLL)
                    player->SEND_GOSSIP_MENU(5842, creature->GetGUID());
                else canBuy = true;
                break;
            case 7955:                                          //Milli Featherwhistle
                if (player->GetReputationRank(54) != REP_EXALTED && race != RACE_GNOME)
                    player->SEND_GOSSIP_MENU(5857, creature->GetGUID());
                else canBuy = true;
                break;
            case 16264:                                         //Winaestra
                if (player->GetReputationRank(911) != REP_EXALTED && race != RACE_BLOODELF)
                    player->SEND_GOSSIP_MENU(10305, creature->GetGUID());
                else canBuy = true;
                break;
            case 17584:                                         //Torallius the Pack Handler
                if (player->GetReputationRank(930) != REP_EXALTED && race != RACE_DRAENEI)
                    player->SEND_GOSSIP_MENU(10239, creature->GetGUID());
                else canBuy = true;
                break;
        }

        if (canBuy)
        {
            if (creature->isVendor())
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_VENDOR, GOSSIP_TEXT_BROWSE_GOODS, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_TRADE);
            player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());
        }
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action)
    {
        player->PlayerTalkClass->ClearMenus();
        if (action == GOSSIP_ACTION_TRADE)
            player->GetSession()->SendListInventory(creature->GetGUID());

        return true;
    }
};

/*######
## npc_rogue_trainer
######*/

#define GOSSIP_HELLO_ROGUE1 "I wish to unlearn my talents"
#define GOSSIP_HELLO_ROGUE2 "<Take the letter>"
#define GOSSIP_HELLO_ROGUE3 "Purchase a Dual Talent Specialization."

#define GOSSIP_TEXT_TRAIN_RECKFUL "I am in need of training, Reckful."
#define GOSSIP_TEXT_GOOD_SEEING_YOU_AGAIN "It was good seeing you again."

class npc_rogue_trainer : public CreatureScript
{
public:
    npc_rogue_trainer() : CreatureScript("npc_rogue_trainer") { }

    bool OnGossipHello(Player* player, Creature* creature)
    {
        if (creature->isQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        if (creature->isTrainer())
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TRAINER, GOSSIP_TEXT_TRAIN, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_TRAIN);

        if (creature->isCanTrainingAndResetTalentsOf(player))
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TRAINER, GOSSIP_HELLO_ROGUE1, GOSSIP_SENDER_MAIN, GOSSIP_OPTION_UNLEARNTALENTS);

        if (player->GetSpecsCount() == 1 && creature->isCanTrainingAndResetTalentsOf(player) && player->getLevel() >= sWorld->getIntConfig(CONFIG_MIN_DUALSPEC_LEVEL))
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TRAINER, GOSSIP_HELLO_ROGUE3, GOSSIP_SENDER_MAIN, GOSSIP_OPTION_LEARNDUALSPEC);

        if (player->getClass() == CLASS_ROGUE && player->getLevel() >= 24 && !player->HasItemCount(17126) && !player->GetQuestRewardStatus(6681))
        {
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_HELLO_ROGUE2, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
            player->SEND_GOSSIP_MENU(5996, creature->GetGUID());
        } else
            player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());

        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action)
    {
        player->PlayerTalkClass->ClearMenus();
        switch (action)
        {
            case GOSSIP_ACTION_INFO_DEF + 1:
                player->CLOSE_GOSSIP_MENU();
                player->CastSpell(player, 21100, false);
                break;
            case GOSSIP_ACTION_TRAIN:
                player->GetSession()->SendTrainerList(creature->GetGUID());
                break;
            case GOSSIP_OPTION_UNLEARNTALENTS:
                player->CLOSE_GOSSIP_MENU();
                player->SendTalentWipeConfirm(creature->GetGUID());
                break;
            case GOSSIP_OPTION_LEARNDUALSPEC:
                if (player->GetSpecsCount() == 1 && !(player->getLevel() < sWorld->getIntConfig(CONFIG_MIN_DUALSPEC_LEVEL)))
                {
                    if (!player->HasEnoughMoney(uint64(100000)))
                    {
                        player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, 0, 0, 0);
                        player->PlayerTalkClass->SendCloseGossip();
                        break;
                    }
                    else
                    {
                        player->ModifyMoney(int64(-100000));

                        // Cast spells that teach dual spec
                        // Both are also ImplicitTarget self and must be cast by player
                        player->CastSpell(player, 63680, true, NULL, NULL, player->GetGUID());
                        player->CastSpell(player, 63624, true, NULL, NULL, player->GetGUID());

                        // Should show another Gossip text with "Congratulations..."
                        player->PlayerTalkClass->SendCloseGossip();
                    }
                }
                break;
        }
        return true;
    }
};

enum reckful_creature_text_groupid
{
    RECKFUL_SAY_LOVE = 0,
    RECKFUL_SAY_HUG = 1,
};

class npc_Reckful_trainer : public CreatureScript
{
public:
    npc_Reckful_trainer() : CreatureScript("npc_Reckful_trainer") { }

    struct npc_Reckful_trainerAI : public ScriptedAI
    {
        npc_Reckful_trainerAI(Creature* creature) : ScriptedAI(creature)
        {
            me->SetPower(POWER_ENERGY, 0);
            me->setPowerType(POWER_ENERGY, 110);
            me->SetMaxPower(POWER_ENERGY, 110);
            me->RemoveFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_REGENERATE_POWER);
            me->SetFlag(UNIT_FIELD_FLAGS, (UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC));
            me->SetMaxHealth(3000);
            if (auto bench = me->FindNearestGameObject(202057, 15.f))
                bench->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE);
        }

        void ReceiveEmote(Player* player, uint32 emote)
        {
            if (!player || !emote)
                return;

            switch (emote)
            {
            case TEXT_EMOTE_HUG:
            {
                auto players = me->GetPlayersInRange(30.f, false);
                if (players.size())
                    for (Player* player_nearby : players)
                    {
                        LocaleConstant loc_idx = player_nearby->GetSession()->GetSessionDbLocaleIndex();
                        WorldPacket data(SMSG_MESSAGECHAT, 200);
                        char const* text = player_nearby->GetGUID() == player->GetGUID() ? "%s hugs you." : "%s hugs $n.";
                        me->BuildMonsterChat(&data, CHAT_MSG_MONSTER_EMOTE, text, LANG_UNIVERSAL, me->GetNameForLocaleIdx(loc_idx), player->GetGUID());
                        player_nearby->GetSession()->SendPacket(&data);
                    }
                break;
            }
            case TEXT_EMOTE_LOVE:
            {
                auto players = me->GetPlayersInRange(30.f, false);
                if (players.size())
                    for (Player* player_nearby : players)
                        {
                            LocaleConstant loc_idx = player_nearby->GetSession()->GetSessionDbLocaleIndex();
                            WorldPacket data(SMSG_MESSAGECHAT, 200);
                            char const* text = player_nearby->GetGUID() == player->GetGUID() ? "%s loves you." : "%s loves $n.";
                            me->BuildMonsterChat(&data, CHAT_MSG_MONSTER_EMOTE, text, LANG_UNIVERSAL, me->GetNameForLocaleIdx(loc_idx), player->GetGUID());
                            player_nearby->GetSession()->SendPacket(&data);
                        }
                break;
            }
            default:
                //TC_LOG_ERROR("sql.sql", "Recieved emote %u from player.", emote);
                break;
            }
        }


        void Reset()
        {
            me->SetPower(POWER_ENERGY, 0);
            me->setPowerType(POWER_ENERGY, 110);
            me->SetMaxPower(POWER_ENERGY, 110);
            me->RemoveFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_REGENERATE_POWER);
            me->SetFlag(UNIT_FIELD_FLAGS, (UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC));
            me->SetMaxHealth(3000);
            if (auto bench = me->FindNearestGameObject(202057, 15.f))
                bench->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE);
        }
    };


    bool OnGossipHello(Player* player, Creature* creature)
    {
        if (creature->isQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        if (creature->isTrainer())
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TRAINER, GOSSIP_TEXT_TRAIN_RECKFUL, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_TRAIN);

        if (creature->isCanTrainingAndResetTalentsOf(player))
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TRAINER, GOSSIP_HELLO_ROGUE1, GOSSIP_SENDER_MAIN, GOSSIP_OPTION_UNLEARNTALENTS);

        if (player->GetSpecsCount() == 1 && creature->isCanTrainingAndResetTalentsOf(player) && player->getLevel() >= sWorld->getIntConfig(CONFIG_MIN_DUALSPEC_LEVEL))
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TRAINER, GOSSIP_HELLO_ROGUE3, GOSSIP_SENDER_MAIN, GOSSIP_OPTION_LEARNDUALSPEC);

        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_TEXT_GOOD_SEEING_YOU_AGAIN, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
        player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());

        return true;
    }
    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action)
    {
        player->PlayerTalkClass->ClearMenus();

        switch (action)
        {
        case GOSSIP_ACTION_INFO_DEF + 1:
            creature->CastCustomSpell(92572, SPELLVALUE_DURATION, 8000, creature);
            player->CLOSE_GOSSIP_MENU();
            break;
        case GOSSIP_ACTION_TRAIN:
            player->GetSession()->SendTrainerList(creature->GetGUID());
            break;
        case GOSSIP_OPTION_UNLEARNTALENTS:
            player->CLOSE_GOSSIP_MENU();
            player->SendTalentWipeConfirm(creature->GetGUID());
            break;
        case GOSSIP_OPTION_LEARNDUALSPEC:
            if (player->GetSpecsCount() == 1 && !(player->getLevel() < sWorld->getIntConfig(CONFIG_MIN_DUALSPEC_LEVEL)))
            {
                if (!player->HasEnoughMoney(uint64(100000)))
                {
                    player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, 0, 0, 0);
                    player->PlayerTalkClass->SendCloseGossip();
                    break;
                }
                else
                {
                    player->ModifyMoney(int64(-100000));
                    // Cast spells that teach dual spec
                    // Both are also ImplicitTarget self and must be cast by player
                    player->CastSpell(player, 63680, true, NULL, NULL, player->GetGUID());
                    player->CastSpell(player, 63624, true, NULL, NULL, player->GetGUID());
                    // Should show another Gossip text with "Congratulations..."
                    player->PlayerTalkClass->SendCloseGossip();
                }
            }
            break;
        }
        return true;
    }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_Reckful_trainerAI(creature);
    }
};

#define SPELL_PLAYER_CAPTURING      98322
#define SPELL_PLAYER_OPENING        21651
#define SPELL_CAPTURING_VISUAL      97389
#define SPELL_CAPTURING_RATED       96220

class npc_CapturePoint : public CreatureScript
{
public:
    npc_CapturePoint() : CreatureScript("npc_CapturePoint") { }


    struct npc_CapturePointAI : public ScriptedAI
    {
        uint32 HelixTimer = 1500;
        npc_CapturePointAI(Creature* creature) : ScriptedAI(creature) {}

        void InitializeAI() override
        {
            events.ScheduleEvent(EVENT_RECENTER, 15000);
        }
        void EnterCombat(Unit* /*who*/) {}
        void AttackStart(Unit* /*who*/) {}
        void MoveInLineOfSight(Unit* /*who*/) {}

        bool CapturingOccuringNearby()
        {
            //list of unfriendly players within 10 yards.
            std::list<Unit*> targets;
            Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(me, me, 10.0f);
            Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(me, targets, u_check);
            me->VisitNearbyObject(10.0f, searcher);

            //list of friendly players within 10 yards.
            std::list<Unit*> targets2;
            Trinity::AnyFriendlyUnitInObjectRangeCheck u_check2(me, me, 10.0f);
            Trinity::UnitListSearcher<Trinity::AnyFriendlyUnitInObjectRangeCheck> searcher2(me, targets2, u_check2);
            me->VisitNearbyObject(10.0f, searcher2);

            if (!targets.size() && !targets2.size())
            {
                return false;//nobody nearby
            }

            for (auto itr = targets.begin(); itr != targets.end();)
            {
                Unit* temp = (*itr);
                itr++;
                if (temp != NULL)
                    if ((temp)->ToPlayer() && (temp->GetGUID() != me->GetGUID()))
                        if ((temp)->GetCurrentSpell(CURRENT_GENERIC_SPELL))
                            if
                                ((temp)->GetCurrentSpell(CURRENT_GENERIC_SPELL)->GetSpellInfo()->Id == SPELL_PLAYER_CAPTURING
                                    ||
                                    (temp)->GetCurrentSpell(CURRENT_GENERIC_SPELL)->GetSpellInfo()->Id == SPELL_CAPTURING_RATED
                                    ||
                                    (temp)->GetCurrentSpell(CURRENT_GENERIC_SPELL)->GetSpellInfo()->Id == SPELL_PLAYER_OPENING)
                                return true;//a nonfriendly player is capturing.
            }

            for (auto itr = targets2.begin(); itr != targets2.end();)
            {
                Unit* temp = (*itr);
                itr++;
                if (temp != NULL)
                    if ((temp)->ToPlayer() && (temp->GetGUID() != me->GetGUID()))
                        if ((temp)->GetCurrentSpell(CURRENT_GENERIC_SPELL))
                            if
                                ((temp)->GetCurrentSpell(CURRENT_GENERIC_SPELL)->GetSpellInfo()->Id == SPELL_PLAYER_CAPTURING
                                    ||
                                    (temp)->GetCurrentSpell(CURRENT_GENERIC_SPELL)->GetSpellInfo()->Id == SPELL_CAPTURING_RATED
                                    ||
                                    (temp)->GetCurrentSpell(CURRENT_GENERIC_SPELL)->GetSpellInfo()->Id == SPELL_PLAYER_OPENING)
                                return true;//a friendly player is capturing
            }

            return false;//even though some players were nearby, none of them were capturing the spells we were looking for.
        }

        enum capturepoint_events
        {
            EVENT_RECENTER = 1,
        };
        void UpdateAI(uint32 const diff)
        {
            capturing = CapturingOccuringNearby();
            switch (capturing)
            {
            case true:
                if (HelixTimer >= 1500)
                {
                    DoCast(me, SPELL_CAPTURING_VISUAL, true);
                    HelixTimer = 0;
                }
                else
                {
                    HelixTimer += diff;
                }
                break;

            case false:
                HelixTimer = 1500;
                break;
            }
            events.Update(diff);
            if (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                case EVENT_RECENTER:
                    if (!capturing)
                        me->NearTeleportTo(me->GetHomePosition(), false);
                    events.ScheduleEvent(EVENT_RECENTER, 15000);
                    //TC_LOG_ERROR("sql.sql", "re-centering");
                default:
                    break;
                }
            }
        }

        private:
            bool capturing;
            EventMap events;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_CapturePointAI(creature);
    }
};

class npc_battle_standard : public CreatureScript
{
public:
    npc_battle_standard() : CreatureScript("npc_battle_standard") { }


    struct npc_battle_standardAI : public ScriptedAI
    {
        bool transformChecked = false;
        npc_battle_standardAI(Creature* creature) : ScriptedAI(creature) {}

        void Reset()
        {
            //TC_LOG_ERROR("sql.sql", "reset script started.");
            if (!transformChecked)
            {
                if (me->GetOwner())
                    if (Player* BannerOwner = me->GetOwner()->ToPlayer())
                        if (BannerOwner->GetBattleground())
                            if (!BannerOwner->IsPlayingNative())
                            {
                                if (me->UpdateEntry((BannerOwner->GetTeamId() == TEAM_ALLIANCE) ? 14465 : 14466, BannerOwner->GetTeam()))
                                {
                                    //TC_LOG_ERROR("sql.sql", "sucessful transformation.");
                                }
                                transformChecked = true;
                            }
                transformChecked = true;
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_battle_standardAI(creature);
    }
};


/*######
## npc_sayge
######*/

#define SPELL_DMG       23768                               //dmg
#define SPELL_RES       23769                               //res
#define SPELL_ARM       23767                               //arm
#define SPELL_SPI       23738                               //spi
#define SPELL_INT       23766                               //int
#define SPELL_STM       23737                               //stm
#define SPELL_STR       23735                               //str
#define SPELL_AGI       23736                               //agi
#define SPELL_FORTUNE   23765                               //faire fortune

#define GOSSIP_HELLO_SAYGE  "Yes"
#define GOSSIP_SENDACTION_SAYGE1    "Slay the Man"
#define GOSSIP_SENDACTION_SAYGE2    "Turn him over to liege"
#define GOSSIP_SENDACTION_SAYGE3    "Confiscate the corn"
#define GOSSIP_SENDACTION_SAYGE4    "Let him go and have the corn"
#define GOSSIP_SENDACTION_SAYGE5    "Execute your friend painfully"
#define GOSSIP_SENDACTION_SAYGE6    "Execute your friend painlessly"
#define GOSSIP_SENDACTION_SAYGE7    "Let your friend go"
#define GOSSIP_SENDACTION_SAYGE8    "Confront the diplomat"
#define GOSSIP_SENDACTION_SAYGE9    "Show not so quiet defiance"
#define GOSSIP_SENDACTION_SAYGE10   "Remain quiet"
#define GOSSIP_SENDACTION_SAYGE11   "Speak against your brother openly"
#define GOSSIP_SENDACTION_SAYGE12   "Help your brother in"
#define GOSSIP_SENDACTION_SAYGE13   "Keep your brother out without letting him know"
#define GOSSIP_SENDACTION_SAYGE14   "Take credit, keep gold"
#define GOSSIP_SENDACTION_SAYGE15   "Take credit, share the gold"
#define GOSSIP_SENDACTION_SAYGE16   "Let the knight take credit"
#define GOSSIP_SENDACTION_SAYGE17   "Thanks"

class npc_sayge : public CreatureScript
{
public:
    npc_sayge() : CreatureScript("npc_sayge") { }

    bool OnGossipHello(Player* player, Creature* creature)
    {
        if (creature->isQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        if (player->HasSpellCooldown(SPELL_INT) ||
            player->HasSpellCooldown(SPELL_ARM) ||
            player->HasSpellCooldown(SPELL_DMG) ||
            player->HasSpellCooldown(SPELL_RES) ||
            player->HasSpellCooldown(SPELL_STR) ||
            player->HasSpellCooldown(SPELL_AGI) ||
            player->HasSpellCooldown(SPELL_STM) ||
            player->HasSpellCooldown(SPELL_SPI))
            player->SEND_GOSSIP_MENU(7393, creature->GetGUID());
        else
        {
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_HELLO_SAYGE, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
            player->SEND_GOSSIP_MENU(7339, creature->GetGUID());
        }

        return true;
    }

    void SendAction(Player* player, Creature* creature, uint32 action)
    {
        switch (action)
        {
            case GOSSIP_ACTION_INFO_DEF + 1:
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SENDACTION_SAYGE1,            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SENDACTION_SAYGE2,            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SENDACTION_SAYGE3,            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 4);
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SENDACTION_SAYGE4,            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 5);
                player->SEND_GOSSIP_MENU(7340, creature->GetGUID());
                break;
            case GOSSIP_ACTION_INFO_DEF + 2:
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SENDACTION_SAYGE5,            GOSSIP_SENDER_MAIN + 1, GOSSIP_ACTION_INFO_DEF);
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SENDACTION_SAYGE6,            GOSSIP_SENDER_MAIN + 2, GOSSIP_ACTION_INFO_DEF);
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SENDACTION_SAYGE7,            GOSSIP_SENDER_MAIN + 3, GOSSIP_ACTION_INFO_DEF);
                player->SEND_GOSSIP_MENU(7341, creature->GetGUID());
                break;
            case GOSSIP_ACTION_INFO_DEF + 3:
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SENDACTION_SAYGE8,            GOSSIP_SENDER_MAIN + 4, GOSSIP_ACTION_INFO_DEF);
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SENDACTION_SAYGE9,            GOSSIP_SENDER_MAIN + 5, GOSSIP_ACTION_INFO_DEF);
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SENDACTION_SAYGE10,           GOSSIP_SENDER_MAIN + 2, GOSSIP_ACTION_INFO_DEF);
                player->SEND_GOSSIP_MENU(7361, creature->GetGUID());
                break;
            case GOSSIP_ACTION_INFO_DEF + 4:
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SENDACTION_SAYGE11,           GOSSIP_SENDER_MAIN + 6, GOSSIP_ACTION_INFO_DEF);
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SENDACTION_SAYGE12,           GOSSIP_SENDER_MAIN + 7, GOSSIP_ACTION_INFO_DEF);
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SENDACTION_SAYGE13,           GOSSIP_SENDER_MAIN + 8, GOSSIP_ACTION_INFO_DEF);
                player->SEND_GOSSIP_MENU(7362, creature->GetGUID());
                break;
            case GOSSIP_ACTION_INFO_DEF + 5:
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SENDACTION_SAYGE14,           GOSSIP_SENDER_MAIN + 5, GOSSIP_ACTION_INFO_DEF);
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SENDACTION_SAYGE15,           GOSSIP_SENDER_MAIN + 4, GOSSIP_ACTION_INFO_DEF);
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SENDACTION_SAYGE16,           GOSSIP_SENDER_MAIN + 3, GOSSIP_ACTION_INFO_DEF);
                player->SEND_GOSSIP_MENU(7363, creature->GetGUID());
                break;
            case GOSSIP_ACTION_INFO_DEF:
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SENDACTION_SAYGE17,           GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 6);
                player->SEND_GOSSIP_MENU(7364, creature->GetGUID());
                break;
            case GOSSIP_ACTION_INFO_DEF + 6:
                creature->CastSpell(player, SPELL_FORTUNE, false);
                player->SEND_GOSSIP_MENU(7365, creature->GetGUID());
                break;
        }
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action)
    {
        player->PlayerTalkClass->ClearMenus();
        switch (sender)
        {
            case GOSSIP_SENDER_MAIN:
                SendAction(player, creature, action);
                break;
            case GOSSIP_SENDER_MAIN + 1:
                creature->CastSpell(player, SPELL_DMG, false);
                player->AddSpellCooldown(SPELL_DMG, 0, time(NULL) + 7200);
                SendAction(player, creature, action);
                break;
            case GOSSIP_SENDER_MAIN + 2:
                creature->CastSpell(player, SPELL_RES, false);
                player->AddSpellCooldown(SPELL_RES, 0, time(NULL) + 7200);
                SendAction(player, creature, action);
                break;
            case GOSSIP_SENDER_MAIN + 3:
                creature->CastSpell(player, SPELL_ARM, false);
                player->AddSpellCooldown(SPELL_ARM, 0, time(NULL) + 7200);
                SendAction(player, creature, action);
                break;
            case GOSSIP_SENDER_MAIN + 4:
                creature->CastSpell(player, SPELL_SPI, false);
                player->AddSpellCooldown(SPELL_SPI, 0, time(NULL) + 7200);
                SendAction(player, creature, action);
                break;
            case GOSSIP_SENDER_MAIN + 5:
                creature->CastSpell(player, SPELL_INT, false);
                player->AddSpellCooldown(SPELL_INT, 0, time(NULL) + 7200);
                SendAction(player, creature, action);
                break;
            case GOSSIP_SENDER_MAIN + 6:
                creature->CastSpell(player, SPELL_STM, false);
                player->AddSpellCooldown(SPELL_STM, 0, time(NULL) + 7200);
                SendAction(player, creature, action);
                break;
            case GOSSIP_SENDER_MAIN + 7:
                creature->CastSpell(player, SPELL_STR, false);
                player->AddSpellCooldown(SPELL_STR, 0, time(NULL) + 7200);
                SendAction(player, creature, action);
                break;
            case GOSSIP_SENDER_MAIN + 8:
                creature->CastSpell(player, SPELL_AGI, false);
                player->AddSpellCooldown(SPELL_AGI, 0, time(NULL) + 7200);
                SendAction(player, creature, action);
                break;
        }
        return true;
    }
};

class npc_steam_tonk : public CreatureScript
{
public:
    npc_steam_tonk() : CreatureScript("npc_steam_tonk") { }

    struct npc_steam_tonkAI : public ScriptedAI
    {
        npc_steam_tonkAI(Creature* creature) : ScriptedAI(creature) {}

        void Reset() {}
        void EnterCombat(Unit* /*who*/) {}

        void OnPossess(bool apply)
        {
            if (apply)
            {
                // Initialize the action bar without the melee attack command
                me->InitCharmInfo();
                me->GetCharmInfo()->InitEmptyActionBar(false);

                me->SetReactState(REACT_PASSIVE);
            }
            else
                me->SetReactState(REACT_AGGRESSIVE);
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_steam_tonkAI(creature);
    }
};

#define SPELL_TONK_MINE_DETONATE 25099

class npc_tonk_mine : public CreatureScript
{
public:
    npc_tonk_mine() : CreatureScript("npc_tonk_mine") { }

    struct npc_tonk_mineAI : public ScriptedAI
    {
        npc_tonk_mineAI(Creature* creature) : ScriptedAI(creature)
        {
            me->SetReactState(REACT_PASSIVE);
        }

        uint32 ExplosionTimer;

        void Reset()
        {
            ExplosionTimer = 3000;
        }

        void EnterCombat(Unit* /*who*/) {}
        void AttackStart(Unit* /*who*/) {}
        void MoveInLineOfSight(Unit* /*who*/) {}

        void UpdateAI(uint32 const diff)
        {
            if (ExplosionTimer <= diff)
            {
                DoCast(me, SPELL_TONK_MINE_DETONATE, true);
                me->setDeathState(DEAD); // unsummon it
            }
            else
                ExplosionTimer -= diff;
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_tonk_mineAI(creature);
    }
};

/*####
## npc_brewfest_reveler
####*/

class npc_brewfest_reveler : public CreatureScript
{
public:
    npc_brewfest_reveler() : CreatureScript("npc_brewfest_reveler") { }

    struct npc_brewfest_revelerAI : public ScriptedAI
    {
        npc_brewfest_revelerAI(Creature* creature) : ScriptedAI(creature) {}
        void ReceiveEmote(Player* player, uint32 emote)
        {
            if (!IsHolidayActive(HOLIDAY_BREWFEST))
                return;

            if (emote == TEXT_EMOTE_DANCE)
                me->CastSpell(player, 41586, false);
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_brewfest_revelerAI(creature);
    }
};


/*####
## npc_snake_trap_serpents
####*/

#define SPELL_MIND_NUMBING_POISON    25810   //Viper
#define SPELL_DEADLY_POISON          34655   //Venomous Snake
#define SPELL_CRIPPLING_POISON       30981   //Viper

#define VENOMOUS_SNAKE_TIMER 1500
#define VIPER_TIMER 3000

#define C_VIPER 19921

class npc_snake_trap : public CreatureScript
{
public:
    npc_snake_trap() : CreatureScript("npc_snake_trap_serpents") { }

    struct npc_snake_trap_serpentsAI : public ScriptedAI
    {
        npc_snake_trap_serpentsAI(Creature* creature) : ScriptedAI(creature) {}

        uint32 SpellTimer;
        bool IsViper;

        void EnterCombat(Unit* /*who*/) {}

        void Reset()
        {
            SpellTimer = 0;

            CreatureTemplate const* Info = me->GetCreatureTemplate();

            IsViper = Info->Entry == C_VIPER ? true : false;

            me->SetMaxHealth(uint32(107 * (me->getLevel() - 40) * 0.025f));
            //Add delta to make them not all hit the same time
            uint32 delta = (rand() % 7) * 100;
            me->SetStatFloatValue(UNIT_FIELD_BASEATTACKTIME, float(Info->baseattacktime + delta));
            me->SetStatFloatValue(UNIT_FIELD_RANGED_ATTACK_POWER, float(Info->attackpower));

            // Start attacking attacker of owner on first ai update after spawn - move in line of sight may choose better target
            if (!me->getVictim() && me->isSummon())
                if (Unit* Owner = me->ToTempSummon()->GetSummoner())
                    if (Owner->getAttackerForHelper())
                        AttackStart(Owner->getAttackerForHelper());
        }

        //Redefined for random target selection:
        void MoveInLineOfSight(Unit* who)
        {
            if (!me->getVictim() && me->canCreatureAttack(who))
            {
                if (me->GetDistanceZ(who) > CREATURE_Z_ATTACK_RANGE)
                    return;

                float attackRadius = me->GetAttackDistance(who);
                if (me->IsWithinDistInMap(who, attackRadius) && me->IsWithinLOSInMap(who))
                {
                    if (!(rand() % 5))
                    {
                        me->setAttackTimer(BASE_ATTACK, (rand() % 10) * 100);
                        SpellTimer = (rand() % 10) * 100;
                        AttackStart(who);
                    }
                }
            }
        }

        void UpdateAI(uint32 const diff)
        {
            if (!UpdateVictim())
                return;

            if (me->getVictim()->HasBreakableByDamageCrowdControlAura(me))
            {
                me->InterruptNonMeleeSpells(false);
                return;
            }

            if (SpellTimer <= diff)
            {
                if (IsViper) //Viper
                {
                    if (urand(0, 2) == 0) //33% chance to cast
                    {
                        uint32 spell;
                        if (urand(0, 1) == 0)
                            spell = SPELL_MIND_NUMBING_POISON;
                        else
                            spell = SPELL_CRIPPLING_POISON;

                        DoCastVictim(spell);
                    }

                    SpellTimer = VIPER_TIMER;
                }
                else //Venomous Snake
                {
                    if (urand(0, 2) == 0) //33% chance to cast
                        DoCastVictim(SPELL_DEADLY_POISON);
                    SpellTimer = VENOMOUS_SNAKE_TIMER + (rand() % 5) * 100;
                }
            }
            else
                SpellTimer -= diff;

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_snake_trap_serpentsAI(creature);
    }
};

#define SAY_RANDOM_MOJO0    "Now that's what I call froggy-style!"
#define SAY_RANDOM_MOJO1    "Your lily pad or mine?"
#define SAY_RANDOM_MOJO2    "This won't take long, did it?"
#define SAY_RANDOM_MOJO3    "I thought you'd never ask!"
#define SAY_RANDOM_MOJO4    "I promise not to give you warts..."
#define SAY_RANDOM_MOJO5    "Feelin' a little froggy, are ya?"
#define SAY_RANDOM_MOJO6a   "Listen, "
#define SAY_RANDOM_MOJO6b   ", I know of a little swamp not too far from here...."
#define SAY_RANDOM_MOJO7    "There's just never enough Mojo to go around..."

class mob_mojo : public CreatureScript
{
public:
    mob_mojo() : CreatureScript("mob_mojo") { }

    struct mob_mojoAI : public ScriptedAI
    {
        mob_mojoAI(Creature* creature) : ScriptedAI(creature) {Reset();}
        uint32 hearts;
        uint64 victimGUID;
        void Reset()
        {
            victimGUID = 0;
            hearts = 15000;
            if (Unit* own = me->GetOwner())
                me->GetMotionMaster()->MoveFollow(own, 0, 0);
        }

        void EnterCombat(Unit* /*who*/){}

        void UpdateAI(uint32 const diff)
        {
            if (me->HasAura(20372))
            {
                if (hearts <= diff)
                {
                    me->RemoveAurasDueToSpell(20372);
                    hearts = 15000;
                } hearts -= diff;
            }
        }

        void ReceiveEmote(Player* player, uint32 emote)
        {
            me->HandleEmoteCommand(emote);
            Unit* own = me->GetOwner();
            if (!own || own->GetTypeId() != TYPEID_PLAYER || CAST_PLR(own)->GetTeam() != player->GetTeam())
                return;
            if (emote == TEXT_EMOTE_KISS)
            {
                std::string whisp = "";
                switch (rand() % 8)
                {
                    case 0:
                        whisp.append(SAY_RANDOM_MOJO0);
                        break;
                    case 1:
                        whisp.append(SAY_RANDOM_MOJO1);
                        break;
                    case 2:
                        whisp.append(SAY_RANDOM_MOJO2);
                        break;
                    case 3:
                        whisp.append(SAY_RANDOM_MOJO3);
                        break;
                    case 4:
                        whisp.append(SAY_RANDOM_MOJO4);
                        break;
                    case 5:
                        whisp.append(SAY_RANDOM_MOJO5);
                        break;
                    case 6:
                        whisp.append(SAY_RANDOM_MOJO6a);
                        whisp.append(player->GetName());
                        whisp.append(SAY_RANDOM_MOJO6b);
                        break;
                    case 7:
                        whisp.append(SAY_RANDOM_MOJO7);
                        break;
                }

                me->MonsterWhisper(whisp.c_str(), player->GetGUID());
                if (victimGUID)
                    if (Player* victim = Unit::GetPlayer(*me, victimGUID))
                        victim->RemoveAura(43906);//remove polymorph frog thing
                me->AddAura(43906, player);//add polymorph frog thing
                victimGUID = player->GetGUID();
                DoCast(me, 20372, true);//tag.hearts
                me->GetMotionMaster()->MoveFollow(player, 0, 0);
                hearts = 15000;
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new mob_mojoAI(creature);
    }
};

enum mirrorImageSpells
{
    SPELL_COPY_WEAPON           = 41055,
    SPELL_COPY_OFFHAND_WEAPON   = 45206,
    SPELL_GLYPH_OF_MIRROR_IMAGE = 63093,
    SPELL_ARCANE_BLAST          = 79868,
    SPELL_FIREBALL              = 88082,
    SPELL_T12_FIREBALL          = 99062,
    SPELL_FROSTBOLT             = 59638,
    SPELL_COPY_THREAT_LIST      = 58838,
    SPELL_CLONE_SUMMONER        = 45204
};

class npc_mirror_image : public CreatureScript
{
public:
    npc_mirror_image() : CreatureScript("npc_mirror_image") { }

    struct npc_mirror_imageAI : CasterAI
    {
        npc_mirror_imageAI(Creature* creature) : CasterAI(creature) { }


        /*
        * Preserved code to potentially re-enable should a more reliable piece of evidence be found.
        * 
        * Q: "Should Mirror image knock detect and knock rogues/hunters out of stealth?"
        * A: Source material from 2008 discussions as well as a very clear 2015 video agree that mirror images do indeed remove players from stealth.
        * It is possible that a 2010-2012 source is found that would overrule these findings.
        * 2008 discussion of the topic: https://www.mmo-champion.com/threads/626777-Is-Mirror-Image-ever-knocking-you-out-of-stealth
        * 2015 video of the topic:      https://www.youtube.com/watch?v=6JCmeieUWiQ
        */
        /*
            bool CanAIAttack(Unit const* target) const override
            {
                if (target->HasAuraType(SPELL_AURA_FEIGN_DEATH) 
                    || target->HasAuraType(SPELL_AURA_MOD_STEALTH) 
                    || target->HasAuraType(SPELL_AURA_MOD_INVISIBILITY))
                return false;
                return true;
            }
        */
        void InitializeAI() override
        {
            /// @todo: update spells in creature_template ?
            CasterAI::InitializeAI();

            Unit* owner = me->GetOwner();
            if (!owner)
                return;

            // Glyph of Mirror Image
            if (owner->GetTypeId() == TYPEID_PLAYER && owner->HasAura(SPELL_GLYPH_OF_MIRROR_IMAGE))
            {
                spells.clear();
                switch (owner->ToPlayer()->GetPrimaryTalentTree(owner->ToPlayer()->GetActiveSpec()))
                {
                    case TALENT_TREE_MAGE_ARCANE:
                        spells.push_back(SPELL_ARCANE_BLAST);
                        break;
                    case TALENT_TREE_MAGE_FIRE:
                        spells.push_back(SPELL_FIREBALL);
                        break;
                    case TALENT_TREE_MAGE_FROST:
                        spells.push_back(SPELL_FROSTBOLT);
                        break;
                    default:
                        break;
                }
            }

            // Mage T12 2P Bonus mirror image
            if (me->GetEntry() == 53438)
            {
                spells.clear();
                spells.push_back(SPELL_T12_FIREBALL);
            }

            // Inherit Master's Threat List (not yet implemented)
            owner->CastSpell((Unit*)NULL, SPELL_COPY_THREAT_LIST, true);
            // here mirror image casts on summoner spell (not present in client dbc) 49866
            // here should be auras (not present in client dbc): 35657, 35658, 35659, 35660 selfcasted by mirror images (stats related?)
            // Clone Me!
            owner->CastSpell(me, SPELL_CLONE_SUMMONER, true);
            owner->CastSpell(me, SPELL_COPY_WEAPON, true);
            owner->CastSpell(me, SPELL_COPY_OFFHAND_WEAPON, true);
            me->SetReactState(REACT_ASSIST);
        }

        // Do not reload Creature templates on evade mode enter - prevent visual lost
        void EnterEvadeMode() override
        {
            if (me->IsInEvadeMode() || !me->isAlive())
                return;

            Unit* owner = me->GetOwner();

            me->CombatStop(true);
            if (owner && !me->HasUnitState(UNIT_STATE_FOLLOW))
            {
                me->GetMotionMaster()->Clear(false);
                me->GetMotionMaster()->MoveFollow(owner, PET_FOLLOW_DIST, me->GetFollowAngle(), MOTION_SLOT_ACTIVE);
                //the only motionmaster function in the script.
            }
            me->SetReactState(REACT_ASSIST);
        }

        void UpdateAI(const uint32 diff) override
        {
            if (me->GetReactState() == REACT_AGGRESSIVE && !me->getVictim())
                me->SetReactState(REACT_ASSIST);

            if (me->GetReactState() == REACT_ASSIST && me->getVictim())
                me->SetReactState(REACT_AGGRESSIVE);

            if (Unit* Owner = me->GetOwner())
            {
                if (!me->getVictim() && Owner->getVictim())
                    AttackStart(Owner->getVictim());

                if (!me->getVictim() && !Owner->getVictim())
                    EnterEvadeMode();
            }

            if (!me->getVictim())
                return;

            events.Update(diff);

            if (me->getVictim()->HasBreakableByDamageCrowdControlAura(me) || me->getVictim()->HasAura(31661))
            {
                me->InterruptNonMeleeSpells(false);
                return;
            }

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            if (uint32 eventId = events.ExecuteEvent())
            {
                uint32 spellId = eventId > spells.size() ? 0 : spells.at(eventId - 1);
                if (!spellId)
                    return;

                if (me->HasSpellCooldown(spellId))
                    return;
                /*
                * Disabled but preserved no-stealth check, see note on CanAIAttack notes above.
                if (auto v = me->getVictim())
                    if (CanAIAttack(v))
                */
                        DoCast(spellId);
                uint32 casttime = me->GetCurrentSpellCastTime(spellId);
                events.ScheduleEvent(eventId, (casttime ? casttime : 500) + GetAISpellInfo(spellId)->realCooldown);
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_mirror_imageAI(creature);
    }
};

class npc_mage_orb : public CreatureScript
{
public:
   npc_mage_orb() : CreatureScript("npc_mage_orb") {}

   enum OrbSpells
   {
       SPELL_FIRE_POWER_RANK_1  = 18459,
       SPELL_FIRE_POWER_RANK_2  = 18460,
       SPELL_FIRE_POWER_RANK_3  = 54734,
       SPELL_FIRE_POWER         = 83619,
       SPELL_PET_SCALING_MAGE   = 89764,
       EVENT_EXPLODE            = 1
   };

   struct npc_mage_orbAI : public ScriptedAI
   {
       float newx, newy, newz;

       npc_mage_orbAI(Creature* creature) : ScriptedAI(creature)
       {
           Position pos(*(me->GetOwner()));
           me->MoveBlink(pos, 40.f, 0.0f);
           DestPosition = pos;
       }

       void Reset()
       {
           //me->SetDisableGravity(false, true);
           me->SetHover(true);
           uint32 spellId = me->GetEntry() == 45322 ? 84717 : 82690;
           DoCast(me, spellId, true);
           DoCast(me, SPELL_PET_SCALING_MAGE, true);
           me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE|UNIT_FLAG_NON_ATTACKABLE);
           me->SetReactState(REACT_PASSIVE);

           me->GetMotionMaster()->MovePoint(0, DestPosition, true, 2.666666f, false);
           events.ScheduleEvent(EVENT_EXPLODE, 15000);
       }

       void UpdateAI(const uint32 diff)
       {
           events.Update(diff);

           if (uint32 eventId = events.ExecuteEvent())
           {
               if (eventId == EVENT_EXPLODE)
               {
                   uint32 spellId = me->GetEntry() == 45322 ? 84717 : 82690;
                   me->RemoveAurasDueToSpell(spellId);
                   me->SetHover(false);
                   me->SetDisableGravity(false, false);
                   me->GetMotionMaster()->MoveFall();
                   me->SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_UNK1);//hide model
                   if (!exploded)
                   if (Unit* owner = me->GetOwner())
                   {
                       if (owner->HasAura(SPELL_FIRE_POWER_RANK_3))
                           me->CastSpell(me, SPELL_FIRE_POWER, true, NULL, NULL, me->GetOwnerGUID());
                       else if (owner->HasAura(SPELL_FIRE_POWER_RANK_2) && roll_chance_i(66))
                           me->CastSpell(me, SPELL_FIRE_POWER, true, NULL, NULL, me->GetOwnerGUID());
                       else if (owner->HasAura(SPELL_FIRE_POWER_RANK_1) && roll_chance_i(33))
                           me->CastSpell(me, SPELL_FIRE_POWER, true, NULL, NULL, me->GetOwnerGUID());

                       exploded = true;
                   }
               }
           }
       }

       void EnterCombat(Unit* /*who*/) {}
       void AttackStart(Unit* /*who*/) {}

   private:
       bool exploded = false;
       Position DestPosition;
       uint32 explosionTimer;
       EventMap events;
   };

   CreatureAI* GetAI(Creature* creature) const
   {
       return new npc_mage_orbAI(creature);
   }
};

enum shadowySpells
{
    SPELL_APPARITION_SPAWN_EFFECT   = 87213,
    SPELL_APPARITION_SCALING        = 89962,
    SPELL_APPARITION_VISUAL         = 87427,
    SPELL_APPARITION_DEATH_VISUAL   = 87529,
    SPELL_APPARITION_TARGET_DAMAGE  = 87532,
};

class npc_shadowy_apparition : public CreatureScript
{
public:
    npc_shadowy_apparition() : CreatureScript("npc_shadowy_apparition") { }

    struct npc_shadowy_apparitionAI : ScriptedAI
    {
        npc_shadowy_apparitionAI(Creature* creature) : ScriptedAI(creature) {}

        bool init;
        uint64 targetGuid;

        void Reset()
        {
            init = false;
            me->SetMaxHealth(5);
            me->setPowerType(POWER_MANA);
            Unit* owner = me->GetOwner();
            if (!owner)
            {
                me->DespawnOrUnsummon();
                return;
            }
            owner->CastSpell(me, SPELL_APPARITION_SPAWN_EFFECT, true);
            me->CastSpell(me, SPELL_APPARITION_SCALING, true);
            me->CastSpell(me, SPELL_APPARITION_VISUAL, false);
            targetGuid = owner->GetProcTargetGuid();
        }

        void DamageTaken(Unit* /*doneBy*/, uint32& /*damage*/)
        {
            me->CastSpell(me, SPELL_APPARITION_DEATH_VISUAL, false);
            me->DespawnOrUnsummon();
        }

        void EnterCombat(Unit* /*who*/) {}
        void AttackStart(Unit* /*who*/) {}
        void EnterEvadeMode() {}

        void UpdateAI(const uint32 /*diff*/)
        {
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            if (!targetGuid)
                return;

            Unit* victim = NULL;
            if (Unit* owner = me->GetOwner())
            {
                if (victim = ObjectAccessor::FindUnit(targetGuid))
                {
                    if (!init)
                    {
                        me->SetInCombatWith(victim);
                        me->Attack(victim, false);
                        me->GetMotionMaster()->MoveChase(victim);
                        init = true;
                    }
                }
            }
            else
            {
                me->DespawnOrUnsummon();
                return;
            }

            if (!me->IsWalking())
            {
                me->SetWalk(true);
                me->UpdateSpeed(MOVE_WALK, true);
            }

            if (!victim || !victim->isAlive())
            {
                me->DespawnOrUnsummon();
                return;
            }

            if (!me->isInCombat())
            {
                me->SetInCombatWith(victim);
                me->Attack(victim, false);
            }

            if (me->GetDistance(victim) <= 2.0f)
            {
                if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(SPELL_APPARITION_TARGET_DAMAGE))
                {
                    // for some reason this spell dont trigger a immune check
                    if (!victim->IsImmunedToDamage(spellInfo))
                    {
                        me->CastSpell(victim, SPELL_APPARITION_TARGET_DAMAGE, false, NULL, NULL, me->GetOwnerGUID());

                        // Priest T13 Shadow 4P Bonus (Shadowfiend and Shadowy Apparition)
                        if (Unit* owner = me->GetOwner())
                            if (owner->HasAura(105844, owner->GetGUID()))
                                owner->CastCustomSpell(77487, SPELLVALUE_AURA_STACK, 3, owner, true);
                    }
                }
                me->CastSpell(me, SPELL_APPARITION_DEATH_VISUAL, false);
                me->DespawnOrUnsummon();
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_shadowy_apparitionAI(creature);
    }
};

enum EbgonGargoyle
{
    SPELL_GARGOYLE_SCALING_03       = 61697,
    SPELL_GARGOYLE_SCALING_05       = 110474,
    SPELL_SUMMON_GARGOYLE           = 50514,
    SPELL_GARGOYLE_TARGET_MARKER    = 49206,
    SPELL_DISMISS_GARGOYLE          = 50515,
    SPELL_GARGOYLE_SENCTUARY        = 54661,
    SPELL_GARGOYLE_STRIKE           = 51963,
    SPELL_SHAMAN_HEROISM            = 32182,
    SPELL_SHAMAN_BLOODLUST          = 2825,
    SPELL_MAGE_TIME_WARP            = 80353,
    SPELL_PRIEST_POWER_INFUSION     = 10060,
    EVENT_GARGOYLE_STRIKE           = 1
};

class npc_ebon_gargoyle : public CreatureScript
{
public:
    npc_ebon_gargoyle() : CreatureScript("npc_ebon_gargoyle") { }

    struct npc_ebon_gargoyleAI : PetAI
    {
        npc_ebon_gargoyleAI(Creature* creature) : PetAI(creature) {}

        void IsSummonedBy(Unit* /*summoner*/)
        {
            me->ApplySpellImmune(SPELL_SHAMAN_HEROISM, IMMUNITY_ID, SPELL_SHAMAN_HEROISM, true);
            me->ApplySpellImmune(SPELL_SHAMAN_BLOODLUST, IMMUNITY_ID, SPELL_SHAMAN_BLOODLUST, true);
            me->ApplySpellImmune(SPELL_MAGE_TIME_WARP, IMMUNITY_ID, SPELL_MAGE_TIME_WARP, true);
            me->ApplySpellImmune(SPELL_PRIEST_POWER_INFUSION, IMMUNITY_ID, SPELL_PRIEST_POWER_INFUSION, true);
            events.ScheduleEvent(EVENT_GARGOYLE_STRIKE, 500);
            DoCast(me, SPELL_GARGOYLE_SCALING_03, true);
            DoCast(me, SPELL_GARGOYLE_SCALING_05, true);
            init = false;
        }

        void JustDied(Unit* /*killer*/)
        {
            // Stop Feeding Gargoyle when it dies
            if (Unit* owner = me->GetOwner())
                owner->RemoveAurasDueToSpell(SPELL_SUMMON_GARGOYLE);
        }

        // Fly away when dismissed
        void SpellHit(Unit* source, SpellInfo const* spell)
        {
            if (spell->Id != SPELL_DISMISS_GARGOYLE || !me->isAlive())
                return;

            Unit* owner = me->GetOwner();

            if (!owner || owner != source)
                return;

            // Stop Fighting
            me->ApplyModFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE, true);
            // Sanctuary
            DoCast(me, SPELL_GARGOYLE_SENCTUARY, true);
            me->SetReactState(REACT_PASSIVE);

            //! HACK: Creature's can't have MOVEMENTFLAG_FLYING
            // Fly Away
            me->AddUnitMovementFlag(MOVEMENTFLAG_CAN_FLY|MOVEMENTFLAG_ASCENDING|MOVEMENTFLAG_FLYING);
            me->SetSpeed(MOVE_FLIGHT, 0.75f, true);
            me->SetSpeed(MOVE_RUN, 0.75f, true);
            float x = me->GetPositionX() + 20 * std::cos(me->GetOrientation());
            float y = me->GetPositionY() + 20 * std::sin(me->GetOrientation());
            float z = me->GetPositionZ() + 40;
            me->GetMotionMaster()->Clear(false);
            me->GetMotionMaster()->MovePoint(0, x, y, z);

            // Despawn as soon as possible
            me->DespawnOrUnsummon(4 * IN_MILLISECONDS);
        }

        void UpdateAI(const uint32 diff)
        {
            if (!init && me->GetOwnerGUID())
            {
                init = true;
                std::list<Unit*> targets;
                Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(me, me, 30.00f);
                Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(me, targets, u_check);
                me->VisitNearbyObject(30.00f, searcher);
                bool targetFound = false;

                for (auto iter = targets.begin(); iter != targets.end(); ++iter)
                {
                    if ((*iter)->GetAura(SPELL_GARGOYLE_TARGET_MARKER, me->GetOwnerGUID()))
                    {
                        targetFound = true;
                        me->Attack((*iter), false);
                        me->GetMotionMaster()->MoveChase((*iter), 20.00f, 0.00f);
                        break;
                    }
                }

                if (!targetFound)
                {
                    if (!targets.empty())
                    {
                        targets.sort(Trinity::ObjectDistanceOrderPred(me));
                        me->Attack(targets.front(), false);
                        me->GetMotionMaster()->MoveChase(targets.front(), 20.00f, 0.00f);
                    }
                }
            }


            if (me->HasBreakableByDamageCrowdControlAura())
                return;

            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                    case EVENT_GARGOYLE_STRIKE:
                    {
                        Unit* target = me->getVictim();
                        uint32 castTime = 2000;
                        if (!target)
                            target = me->SelectVictim();

                        if (target)
                        {
                            me->SetFacingToObject(target);
                            me->GetMotionMaster()->MoveChase(target, 20.00f, 0.00f);
                            DoCast(target, SPELL_GARGOYLE_STRIKE);
                        }

                        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(SPELL_GARGOYLE_STRIKE))
                            castTime = std::max<uint32>(spellInfo->CalcCastTime(me), 1500);

                        events.ScheduleEvent(EVENT_GARGOYLE_STRIKE, castTime);
                        break;
                    }
                    default:
                        break;
                }
            }
        }

    private:
        EventMap events;
        bool init;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_ebon_gargoyleAI(creature);
    }
};

/*class npc_melee_guardian : public CreatureScript
{
public:
    npc_melee_guardian() : CreatureScript("npc_melee_guardian") { }

    struct npc_melee_guardianAI : CombatAI
    {
        npc_melee_guardianAI(Creature* creature) : CombatAI(creature) {}

        void IsSummonedBy(Unit* owner)
        {
            me->CastSpell(me, 86703, true);
            me->CastSpell(owner, 41054, true);
            me->m_modMeleeHitChance += owner->ToPlayer()->GetRatingBonusValue(CR_HIT_MELEE);
            me->m_modMeleeHitChance += owner->GetTotalAuraModifier(SPELL_AURA_MOD_HIT_CHANCE);

            Owner = me->GetCharmerOrOwner();
            followdist = PET_FOLLOW_DIST;
            //me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
            //me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        }

        void UpdateAI(const uint32 diff)
        {
            // All operations available only for Retribution Guardian
            if (Owner)
            {
                if (!me->GetAura(86703))
                {
                    // Cast Ancient Crusader on the guardian
                    Owner->AddAura(86703, me);
                }

                Unit* ownerVictim = Owner->getVictim();

                if (!ownerVictim && me->getVictim())
                {
                    DoMeleeAttackIfReady();
                    return;
                }

                // Paladin's attacking check, also check range because state is applied when right clicking (even out of melee range)
                if (ownerVictim && Owner->HasUnitState(UNIT_STATE_MELEE_ATTACKING)
                    && Owner->IsInRange(ownerVictim, 0.0f, NOMINAL_MELEE_RANGE))
                {
                    meVictim = me->getVictim();

                    // Guardian's target switching only when paladin switch
                    if (ownerVictim != meVictim && ownerVictim != Owner)
                    {
                        me->GetMotionMaster()->Clear(false);
                        meVictim = ownerVictim;
                        me->Attack(meVictim, true);
                        me->GetMotionMaster()->MoveChase(meVictim);
                    }

                    // Required with me->Attack
                    if (me->isInCombat())
                    {
                        DoMeleeAttackIfReady();
                    }
                }
                else
                {
                    followdist = PET_FOLLOW_DIST;
                    me->GetMotionMaster()->Clear(false);
                    me->GetMotionMaster()->MoveFollow(Owner, followdist, me->GetFollowAngle());
                }
            }
        }

    private:
        Unit* Owner;
        Unit* meVictim;
        uint32 guardianEntry;
        float followdist;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_melee_guardianAI(creature);
    }
};*/

class npc_melee_guardian : public CreatureScript
{
public:
    npc_melee_guardian() : CreatureScript("npc_melee_guardian") { }

    struct npc_melee_guardianAI : ScriptedAI
    {
        npc_melee_guardianAI(Creature* creature) : ScriptedAI(creature) { hasTarget = false; }

        void InitializeAI()
        {
            Unit* owner = me->GetCharmerOrOwner();
            if (!owner)
                return;

            me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID, 62902);
            me->SetReactState(ReactStates(REACT_ASSIST));
            me->m_modMeleeHitChance += owner->ToPlayer()->GetRatingBonusValue(CR_HIT_MELEE);
            me->m_modMeleeHitChance += owner->GetTotalAuraModifier(SPELL_AURA_MOD_HIT_CHANCE);
        }
        void Reset()
        {
            Unit* owner = me->GetOwner();
            if (!owner)
            {
                me->DespawnOrUnsummon();
                return;
            }

            me->CastSpell(me, 86703, true);
        }

        void UpdateAI(const uint32 diff)
        {
            Unit* owner = me->GetOwner();
            if (!owner)
            {
                me->DespawnOrUnsummon();
                return;
            }

            Unit* guarTarget = me->getVictim();

            if (guarTarget && !hasTarget)
                hasTarget = true;           

            if(hasTarget)
                if (!guarTarget || !me->IsInRange(owner, 0.f, 150.f) || !guarTarget->isAlive())
                {
                    followdist = PET_FOLLOW_DIST;
                    me->AttackStop();
                    me->GetMotionMaster()->Clear(false);
                    me->GetMotionMaster()->MoveFollow(owner, followdist, me->GetFollowAngle());
                    hasTarget = false;
                }

            if (guarTarget && guarTarget->HasBreakableByDamageCrowdControlAura())
                return;

            DoMeleeAttackIfReady();
        }

        void AttackStart(Unit* victim)
        {
            if (me->GetOwner())
                if (!me->GetOwner()->IsValidAttackTarget(victim))
                    return;

            if (victim->HasBreakableByDamageCrowdControlAura())
                return;

            UnitAI::AttackStart(victim);
        }

    private:
        float followdist;
        bool hasTarget;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_melee_guardianAI(creature);
    }
};

class npc_protection_guardian : public CreatureScript
{
    /*
        UPDATE `creature_template` SET `ScriptName`='npc_protection_guardian' WHERE  `entry`=46490;
    */
public:
    npc_protection_guardian() : CreatureScript("npc_protection_guardian") { }

    struct npc_protection_guardianAI : ScriptedAI
    {
        npc_protection_guardianAI(Creature* creature) : ScriptedAI(creature) { hasTarget = false; }
        //
        void InitializeAI()
        {
            Unit* owner = me->GetCharmerOrOwner();
            if (!owner)
                return;

            me->AddUnitState(UnitState::UNIT_STATE_ROOT);
            me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID, 0);
            me->SetReactState(ReactStates(REACT_PASSIVE));
            me->ToUnit()->CastWithDelay(0, owner, 86657, true, false);
        }
        void Reset()
        {
            Unit* owner = me->GetOwner();
            if (!owner)
            {
                me->DespawnOrUnsummon();
                return;
            }
        }

        void UpdateAI(const uint32 diff)
        {
            Unit* owner = me->GetOwner();
            if (!owner)
            {
                me->DespawnOrUnsummon();
                return;
            }
            else
            {
            }
        }

    private:
        float followdist;
        bool hasTarget;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_protection_guardianAI(creature);
    }
};


class npc_dancing_rune_weapon : public CreatureScript
{
public:
    npc_dancing_rune_weapon() : CreatureScript("npc_dancing_rune_weapon") { }

    struct npc_dancing_rune_weaponAI : ScriptedAI
    {
        npc_dancing_rune_weaponAI(Creature* creature) : ScriptedAI(creature) { hasTarget = false; }

        void InitializeAI()
        {
            Unit* owner = me->GetCharmerOrOwner();
            if (!owner)
                return;

            me->SetReactState(ReactStates(REACT_PASSIVE));
        }
        void Reset()
        {
            Unit* owner = me->GetOwner();
            if (!owner)
            {
                me->DespawnOrUnsummon();
                return;
            }

            if (owner->getVictim())
            {
                me->SetReactState(ReactStates(REACT_ASSIST));
                AttackStart(owner->getVictim());
            }
        }

        void UpdateAI(const uint32 diff)
        {
            Unit* owner = me->GetOwner();
            if (!owner)
            {
                me->DespawnOrUnsummon();
                return;
            }

            Unit* guarTarget = me->getVictim();

            if (guarTarget && !hasTarget)
                hasTarget = true;

            if (hasTarget)
                if (!guarTarget || !me->IsInRange(owner, 0.f, 150.f) || !guarTarget->isAlive())
                {
                    followdist = PET_FOLLOW_DIST;
                    me->AttackStop();
                    me->GetMotionMaster()->Clear(false);
                    me->GetMotionMaster()->MoveFollow(owner, followdist, me->GetFollowAngle());
                    hasTarget = false;
                }

            DoMeleeAttackIfReady();
        }

        void AttackStart(Unit* victim)
        {
            if (me->GetOwner())
                if (!me->GetOwner()->IsValidAttackTarget(victim))
                    return;

            if (victim->HasBreakableByDamageCrowdControlAura())
                return;

            UnitAI::AttackStart(victim);
        }

    private:
        float followdist;
        bool hasTarget;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_dancing_rune_weaponAI(creature);
    }
};

enum lightwell
{
    SPELL_LIGHTWELL_SCALING_AND_STACKS  = 59907,
    LIGHTWELL_DISPLAY_ID                = 27769
};

class npc_lightwell : public CreatureScript
{
    public:
        npc_lightwell() : CreatureScript("npc_lightwell") { }

        struct npc_lightwellAI : public ScriptedAI
        {
            npc_lightwellAI(Creature* creature) : ScriptedAI(creature)
            {
                DoCast(me, SPELL_LIGHTWELL_SCALING_AND_STACKS, false);
                me->SetDisplayId(LIGHTWELL_DISPLAY_ID);
                me->SetReactState(REACT_PASSIVE);
                me->DeleteThreatList();
            }

            void EnterEvadeMode()
            {
                if (!me->isAlive())
                    return;

                me->DeleteThreatList();
                me->CombatStop(true);
                me->ResetPlayerDamageReq();
            }
        };

        CreatureAI* GetAI(Creature* creature) const
        {
            return new npc_lightwellAI(creature);
        }
};

class npc_mushroom : public CreatureScript
{
    public:
        npc_mushroom() : CreatureScript("npc_mushroom"){ }

        struct npc_mushroomAI : public PassiveAI
        {
            uint32 invisTimer;

            npc_mushroomAI(Creature* creature) : PassiveAI(creature)
            {
                invisTimer = 6000;
                //begin removing and replacing a possibly old mushroom
                TempSummon* oldestSummon = NULL;
                uint32 duration = 300000;
                me->ToTempSummon()->SetMushroomTotemSlot(1);
                std::list<Creature*> MinionList;
                me->GetOwner()->GetAllMinionsByEntry(MinionList, me->GetEntry());
                if (MinionList.size() > 3)
                {
                    //Tempsummon 
                    for (auto itr = MinionList.begin(); itr != MinionList.end(); itr++)
                    {
                        TempSummon* temp = (*itr)->ToTempSummon();

                        if (temp->GetTimer() < duration)
                        {
                            duration = temp->GetTimer();
                            oldestSummon = temp;
                        }
                    }
                }

                if (oldestSummon)
                {
                    if (me->ToTempSummon())
                        me->ToTempSummon()->SetMushroomTotemSlot(oldestSummon->GetMushroomTotemSlot());
                    oldestSummon->UnSummon();
                }
                else if (MinionList.size() > 1)
                {
                    me->ToTempSummon()->SetMushroomTotemSlot(uint8(MinionList.size()));
                }

                if (Unit* owner = me->GetOwner())
                    if (owner->ToPlayer())
                    {
                        WorldPacket data(SMSG_TOTEM_CREATED, 1 + 8 + 4 + 4);
                        data << uint8(me->ToTempSummon()->GetMushroomTotemSlot() - 1);
                        data << uint64(me->GetGUID());
                        data << uint32(300000);
                        data << uint32(me->GetUInt32Value(UNIT_CREATED_BY_SPELL));
                        owner->ToPlayer()->SendDirectMessage(&data);
                    }
            }

            void Reset()
            {
                Unit* owner = me->GetOwner();
                if (!owner)
                    return;

                me->SetLevel(owner->getLevel());
                me->SetMaxHealth(5);
                me->setFaction(owner->getFaction());
            }

            void UpdateAI(const uint32 diff)
            {
                if (invisTimer > 0)
                {
                    if (invisTimer > diff)
                        invisTimer -= diff;
                    else
                    {
                        invisTimer = 0;
                        DoCast(me, 92661);
                    }
                    return;
                }
                PassiveAI::UpdateAI(diff);
            }
        };

        CreatureAI* GetAI(Creature* creature) const
        {
            return new npc_mushroomAI(creature);
        }
};

class npc_barrier : public CreatureScript
{
    public:
        npc_barrier() : CreatureScript("npc_barrier") { }

        struct npc_barrierAI : public PassiveAI
        {
            npc_barrierAI(Creature* creature) : PassiveAI(creature)
            {
                DoCast(me, 81781, false);
            }

            void EnterEvadeMode() {}
        };

        CreatureAI* GetAI(Creature* creature) const
        {
            return new npc_barrierAI(creature);
        }
};

class npc_consecration : public CreatureScript
{
    public:
        npc_consecration() : CreatureScript("npc_consecration") { }

        struct npc_consecrationAI : public PassiveAI
        {
            npc_consecrationAI(Creature* creature) : PassiveAI(creature)
            {
                DoCast(me, 81298, false);
            }

            void EnterEvadeMode() {}
        };

        CreatureAI* GetAI(Creature* creature) const
        {
            return new npc_consecrationAI(creature);
        }
};

class npc_fungal_growth_one : public CreatureScript
{
    public:
        npc_fungal_growth_one() : CreatureScript("npc_fungal_growth_one") { }

        struct npc_fungal_growth_oneAI : public PassiveAI
        {
            npc_fungal_growth_oneAI(Creature* creature) : PassiveAI(creature)
            {
                DoCast(me, 94339, false);
                me->AddAura(81289, me);
                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE);
            }

            void EnterEvadeMode() {}
        };

        CreatureAI* GetAI(Creature* creature) const
        {
            return new npc_fungal_growth_oneAI(creature);
        }
};


class npc_fungal_growth_two : public CreatureScript
{
    public:
        npc_fungal_growth_two() : CreatureScript("npc_fungal_growth_two") { }

        struct npc_fungal_growth_twoAI : public PassiveAI
        {
            npc_fungal_growth_twoAI(Creature* creature) : PassiveAI(creature)
            {
                DoCast(me, 94339, false);
                me->AddAura(81282, me);
                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE);
            }

            void EnterEvadeMode() {}
        };

        CreatureAI* GetAI(Creature* creature) const
        {
            return new npc_fungal_growth_twoAI(creature);
        }
};

enum eTrainingDummy
{
    NPC_ADVANCED_TARGET_DUMMY                  = 2674,
    NPC_TARGET_DUMMY                           = 2673,
    EVENT_CLEAR_AGGRO                           = 10,
};

class npc_training_dummy : public CreatureScript
{
public:
    npc_training_dummy() : CreatureScript("npc_training_dummy") { }

    struct npc_training_dummyAI : Scripted_NoMovementAI
    {
        npc_training_dummyAI(Creature* creature) : Scripted_NoMovementAI(creature)
        {
        }


        void SpellHit(Unit* caster, SpellInfo const* spell)
        {
            switch (spell->Id)
            {
            case 58683://Savage Combat debuff from combat rogues
            case 58684://Savage Combat debuff from combat rogues (2/2 talent)
                if (Aura* savage_combat = me->GetAura(spell->Id, caster->GetGUID()))
                {
                    savage_combat->SetMaxDuration(12000);
                    savage_combat->SetDuration(12000);
                }
                break;
            default:
                if (Aura* unknown_permaDebuff = me->GetAura(spell->Id, caster->GetGUID()))
                    if (unknown_permaDebuff->GetDuration() == 0)
                    {
                        unknown_permaDebuff->SetMaxDuration(60000);
                        unknown_permaDebuff->SetDuration(60000);
                    }
                break;
            }
        }

        void SetUpDummy()
        {
            if (me->GetHealth() < me->GetMaxHealth())
                me->SetHealth(me->GetMaxHealth());

            me->SetReactState(REACT_PASSIVE);
            /*
                Clear all spell immunities, fuck whatever the db has
            */
            me->ApplyAllSpellImmune(NULL,                                           false);   
            /*
                Apply Immunities
            */
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK,       true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK_DEST,  true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_BIND,             true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_BIND_SIGHT,       true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_ATTACK,           true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_ATTACK_ME, true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_DISPEL, true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_DURABILITY_DAMAGE, true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_DURABILITY_DAMAGE_PCT, true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_FORCE_CAST, true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_FORCE_CAST_2, true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_INSTAKILL, true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_INEBRIATE, true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_LEAP_BACK, true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_LEAP, true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_PULL, true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_PULL_TOWARDS, true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_POWER_BURN, true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_POWER_DRAIN, true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_PULL_TOWARDS_DEST, true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_THREAT, true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_THREAT_ALL, true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_TAMECREATURE, true);

            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_GRIP,               true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_CHARM,              true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_BANISH,             true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_DISORIENTED,        true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_POLYMORPH,          true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_TURN,               true);

            me->AddUnitState(UNIT_STATE_ROOT);
            me->AddUnitState(UNIT_STATE_STUNNED);
            me->AddUnitState(UNIT_STATE_CANNOT_TURN);
            me->AddUnitState(UNIT_STATE_CANNOT_AUTOATTACK);
        }
        void JustDied(Unit* /*killer*/) override
        {
            me->SetHealth(me->GetMaxHealth());
            SetUpDummy();

        }

        void InitializeAI()
        {
            SetUpDummy();
        }

        void Reset()
        {
            SetUpDummy();
        }

        void EnterEvadeMode()
        {
            _EnterEvadeMode();
            Reset();
        }

        void DamageTaken(Unit* /*doneBy*/, uint32& damage)
        {
            damage = 0;
            if (me->GetHealth() < me->GetMaxHealth())
                me->SetHealth(me->GetMaxHealth());

            if (events.HasEvent(EVENT_CLEAR_AGGRO))
                events.DelayEvent(EVENT_CLEAR_AGGRO, 5000);
            else
                events.ScheduleEvent(EVENT_CLEAR_AGGRO, 5000);
        }

        void EnterCombat(Unit* /*who*/)
        {
            if (events.HasEvent(EVENT_CLEAR_AGGRO))
                events.DelayEvent(EVENT_CLEAR_AGGRO, 5000);
            else
                events.ScheduleEvent(EVENT_CLEAR_AGGRO, 5000);
        }

        void UpdateAI(uint32 const diff)
        {
            events.Update(diff);
            while (uint32 eventId = events.ExecuteEvent())
                switch (eventId)
                {
                case EVENT_CLEAR_AGGRO:
                    summons.DespawnAll();
                    me->CombatStop();
                    me->CombatStopWithPets();
                    me->AttackStop();
                    me->DeleteThreatList();
                    me->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_LEAVE_COMBAT);
                    break;
                default:
                    break;
                }

        }

    private:
        EventMap events;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_training_dummyAI(creature);
    }
};

/*######
# npc_shadowfiend
######*/

enum shadowfiendSpells
{
    GLYPH_OF_SHADOWFIEND_MANA   = 58227,
    GLYPH_OF_SHADOWFIEND        = 58228,
    SHADOWFIEND_AOE_AVOIDANCE   = 63623,
    SPELL_PRIEST_T12_2P_BONUS   = 99154,
    SPELL_SHADOWFLAME           = 99155,
    SPELL_SHADOWFIEND_SCALING   = 89962

};

class npc_shadowfiend : public CreatureScript
{
public:
    npc_shadowfiend() : CreatureScript("npc_shadowfiend") { }

    struct npc_shadowfiendAI : public PetAI
    {
        npc_shadowfiendAI(Creature* creature) : PetAI(creature) {}

        void Reset()
        {
            PetAI::Reset();
            if (Unit* target = me->SelectNearestTarget(15.0f))
                AttackStart(target);
        }

        void IsSummonedBy(Unit* summoner)
        {
            DoCast(me, SHADOWFIEND_AOE_AVOIDANCE, true);
            DoCast(me, SPELL_SHADOWFIEND_SCALING, true);

            // Priest T12 Shadow 2P Bonus
            if (summoner->HasAura(SPELL_PRIEST_T12_2P_BONUS))
                DoCast(me, SPELL_SHADOWFLAME, true);
        }

        void JustDied(Unit* killer)
        {
            if (me->isSummon())
                if (Unit* owner = me->ToTempSummon()->GetSummoner())
                    if (owner->HasAura(GLYPH_OF_SHADOWFIEND))
                        owner->CastSpell(owner, GLYPH_OF_SHADOWFIEND_MANA, true);
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_shadowfiendAI(creature);
    }
};

/*######
# npc_fire_elemental
######*/
#define SPELL_FIRENOVA        12470
#define SPELL_FIRESHIELD      13376
#define SPELL_FIREBLAST       57984

class npc_fire_elemental : public CreatureScript
{
public:
    npc_fire_elemental() : CreatureScript("npc_fire_elemental") { }

    struct npc_fire_elementalAI : public ScriptedAI
    {
        npc_fire_elementalAI(Creature* creature) : ScriptedAI(creature) {}

        uint32 FireNova_Timer;
        uint32 FireShield_Timer;
        uint32 FireBlast_Timer;

        void Reset()
        {
            FireNova_Timer = 5000 + rand() % 15000; // 5-20 sec cd
            FireBlast_Timer = 5000 + rand() % 15000; // 5-20 sec cd
            FireShield_Timer = 0;
            me->ApplySpellImmune(0, IMMUNITY_SCHOOL, SPELL_SCHOOL_MASK_FIRE, true);
            me->SetReactState(REACT_ASSIST);
        }

        void UpdateAI(const uint32 diff)
        {
            if (me->GetReactState() == REACT_AGGRESSIVE && !me->getVictim())
                me->SetReactState(REACT_ASSIST);

            if (me->GetReactState() == REACT_ASSIST && me->getVictim())
                me->SetReactState(REACT_AGGRESSIVE);

            if (Unit* Owner = me->GetOwner())
            {
                if (auto v = me->getVictim())
                {
                }
                else
                {
                    if (auto v2 = Owner->getVictim())
                        if (v2->IsValidAttackTarget(me))
                            AttackStart(v2);
                }
            }


            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
            else
            {
                if (UpdateVictim())
                    DoMeleeAttackIfReady();
            }

            if (FireShield_Timer <= diff)
            {
                DoCastVictim(SPELL_FIRESHIELD);
                FireShield_Timer = 2 * IN_MILLISECONDS;
            }
            else
                FireShield_Timer -= diff;

            if (FireBlast_Timer <= diff)
            {
                DoCastVictim(SPELL_FIREBLAST);
                FireBlast_Timer = 5000 + rand() % 15000; // 5-20 sec cd
            }
            else
                FireBlast_Timer -= diff;

            if (FireNova_Timer <= diff)
            {
                DoCastVictim(SPELL_FIRENOVA);
                FireNova_Timer = 5000 + rand() % 15000; // 5-20 sec cd
            }
            else
                FireNova_Timer -= diff;

        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_fire_elementalAI(creature);
    }
};

/*######
# npc_earth_elemental
######*/
#define SPELL_ANGEREDEARTH        36213

class npc_earth_elemental : public CreatureScript
{
public:
    npc_earth_elemental() : CreatureScript("npc_earth_elemental") { }

    struct npc_earth_elementalAI : public ScriptedAI
    {
        npc_earth_elementalAI(Creature* creature) : ScriptedAI(creature) {}

        uint32 AngeredEarth_Timer;

        void Reset()
        {
            AngeredEarth_Timer = 0;
            me->ApplySpellImmune(0, IMMUNITY_SCHOOL, SPELL_SCHOOL_MASK_NATURE, true);
            me->SetReactState(REACT_ASSIST);
        }

        void UpdateAI(const uint32 diff)
        {
            if (me->GetReactState() == REACT_AGGRESSIVE && !me->getVictim())
                me->SetReactState(REACT_ASSIST);

            if (me->GetReactState() == REACT_ASSIST && me->getVictim())
                me->SetReactState(REACT_AGGRESSIVE);

            if (Unit* Owner = me->GetOwner())
            {
                if (auto v = me->getVictim())
                {
                }
                else
                {
                    if (auto v2 = Owner->getVictim())
                        if (v2->IsValidAttackTarget(me))
                            AttackStart(v2);
                }
            }


            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
            else
            {
                if (UpdateVictim())
                    DoMeleeAttackIfReady();
            }

            if (AngeredEarth_Timer <= diff)
            {
                DoCastVictim(SPELL_ANGEREDEARTH);
                AngeredEarth_Timer = 5000 + rand() % 15000; // 5-20 sec cd
            }
            else
                AngeredEarth_Timer -= diff;
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_earth_elementalAI(creature);
    }
};

/*######
# npc_wormhole
######*/

#define GOSSIP_ENGINEERING1   "Borean Tundra"
#define GOSSIP_ENGINEERING2   "Howling Fjord"
#define GOSSIP_ENGINEERING3   "Sholazar Basin"
#define GOSSIP_ENGINEERING4   "Icecrown"
#define GOSSIP_ENGINEERING5   "Storm Peaks"
#define GOSSIP_ENGINEERING6   "Underground..."

enum WormholeSpells
{
    SPELL_BOREAN_TUNDRA         = 67834,
    SPELL_SHOLAZAR_BASIN        = 67835,
    SPELL_ICECROWN              = 67836,
    SPELL_STORM_PEAKS           = 67837,
    SPELL_HOWLING_FJORD         = 67838,
    SPELL_UNDERGROUND           = 68081,

    TEXT_WORMHOLE               = 907,

    DATA_SHOW_UNDERGROUND       = 1,
};

class npc_wormhole : public CreatureScript
{
    public:
        npc_wormhole() : CreatureScript("npc_wormhole") {}

        struct npc_wormholeAI : public PassiveAI
        {
            npc_wormholeAI(Creature* creature) : PassiveAI(creature) {}

            void InitializeAI()
            {
                _showUnderground = urand(0, 100) == 0; // Guessed value, it is really rare though
            }

            uint32 GetData(uint32 type) const
            {
                return (type == DATA_SHOW_UNDERGROUND && _showUnderground) ? 1 : 0;
            }

        private:
            bool _showUnderground;
        };

        bool OnGossipHello(Player* player, Creature* creature)
        {
            if (creature->isSummon())
            {
                if (player == creature->ToTempSummon()->GetSummoner())
                {
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_ENGINEERING1, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_ENGINEERING2, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_ENGINEERING3, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_ENGINEERING4, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 4);
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_ENGINEERING5, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 5);

                    if (creature->AI()->GetData(DATA_SHOW_UNDERGROUND))
                        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_ENGINEERING6, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 6);

                    player->PlayerTalkClass->SendGossipMenu(TEXT_WORMHOLE, creature->GetGUID());
                }
            }

            return true;
        }

        bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action)
        {
            player->PlayerTalkClass->ClearMenus();

            switch (action)
            {
                case GOSSIP_ACTION_INFO_DEF + 1: // Borean Tundra
                    player->CLOSE_GOSSIP_MENU();
                    creature->CastSpell(player, SPELL_BOREAN_TUNDRA, false);
                    break;
                case GOSSIP_ACTION_INFO_DEF + 2: // Howling Fjord
                    player->CLOSE_GOSSIP_MENU();
                    creature->CastSpell(player, SPELL_HOWLING_FJORD, false);
                    break;
                case GOSSIP_ACTION_INFO_DEF + 3: // Sholazar Basin
                    player->CLOSE_GOSSIP_MENU();
                    creature->CastSpell(player, SPELL_SHOLAZAR_BASIN, false);
                    break;
                case GOSSIP_ACTION_INFO_DEF + 4: // Icecrown
                    player->CLOSE_GOSSIP_MENU();
                    creature->CastSpell(player, SPELL_ICECROWN, false);
                    break;
                case GOSSIP_ACTION_INFO_DEF + 5: // Storm peaks
                    player->CLOSE_GOSSIP_MENU();
                    creature->CastSpell(player, SPELL_STORM_PEAKS, false);
                    break;
                case GOSSIP_ACTION_INFO_DEF + 6: // Underground
                    player->CLOSE_GOSSIP_MENU();
                    creature->CastSpell(player, SPELL_UNDERGROUND, false);
                    break;
            }

            return true;
        }

        CreatureAI* GetAI(Creature* creature) const
        {
            return new npc_wormholeAI(creature);
        }
};

/*######
## npc_pet_trainer
######*/

enum ePetTrainer
{
    TEXT_ISHUNTER               = 5838,
    TEXT_NOTHUNTER              = 5839,
    TEXT_PETINFO                = 13474,
    TEXT_CONFIRM                = 7722
};

#define GOSSIP_PET1             "How do I train my pet?"
#define GOSSIP_PET2             "I wish to untrain my pet."
#define GOSSIP_PET_CONFIRM      "Yes, please do."

class npc_pet_trainer : public CreatureScript
{
public:
    npc_pet_trainer() : CreatureScript("npc_pet_trainer") { }

    bool OnGossipHello(Player* player, Creature* creature)
    {
        if (creature->isQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        if (player->getClass() == CLASS_HUNTER)
        {
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_PET1, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
            if (player->GetPet() && player->GetPet()->getPetType() == HUNTER_PET)
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_PET2, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);

            player->PlayerTalkClass->SendGossipMenu(TEXT_ISHUNTER, creature->GetGUID());
            return true;
        }
        player->PlayerTalkClass->SendGossipMenu(TEXT_NOTHUNTER, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action)
    {
        player->PlayerTalkClass->ClearMenus();
        switch (action)
        {
            case GOSSIP_ACTION_INFO_DEF + 1:
                player->PlayerTalkClass->SendGossipMenu(TEXT_PETINFO, creature->GetGUID());
                break;
            case GOSSIP_ACTION_INFO_DEF + 2:
                {
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_PET_CONFIRM, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
                    player->PlayerTalkClass->SendGossipMenu(TEXT_CONFIRM, creature->GetGUID());
                }
                break;
            case GOSSIP_ACTION_INFO_DEF + 3:
                {
                    player->ResetPetTalents();
                    player->CLOSE_GOSSIP_MENU();
                }
                break;
        }
        return true;
    }
};

/*######
## npc_locksmith
######*/

enum eLockSmith
{
    QUEST_HOW_TO_BRAKE_IN_TO_THE_ARCATRAZ = 10704,
    QUEST_DARK_IRON_LEGACY                = 3802,
    QUEST_THE_KEY_TO_SCHOLOMANCE_A        = 5505,
    QUEST_THE_KEY_TO_SCHOLOMANCE_H        = 5511,
    QUEST_HOTTER_THAN_HELL_A              = 10758,
    QUEST_HOTTER_THAN_HELL_H              = 10764,
    QUEST_RETURN_TO_KHAGDAR               = 9837,
    QUEST_CONTAINMENT                     = 13159,
    QUEST_ETERNAL_VIGILANCE               = 11011,
    QUEST_KEY_TO_THE_FOCUSING_IRIS        = 13372,
    QUEST_HC_KEY_TO_THE_FOCUSING_IRIS     = 13375,

    ITEM_ARCATRAZ_KEY                     = 31084,
    ITEM_SHADOWFORGE_KEY                  = 11000,
    ITEM_SKELETON_KEY                     = 13704,
    ITEM_SHATTERED_HALLS_KEY              = 28395,
    ITEM_THE_MASTERS_KEY                  = 24490,
    ITEM_VIOLET_HOLD_KEY                  = 42482,
    ITEM_ESSENCE_INFUSED_MOONSTONE        = 32449,
    ITEM_KEY_TO_THE_FOCUSING_IRIS         = 44582,
    ITEM_HC_KEY_TO_THE_FOCUSING_IRIS      = 44581,

    SPELL_ARCATRAZ_KEY                    = 54881,
    SPELL_SHADOWFORGE_KEY                 = 54882,
    SPELL_SKELETON_KEY                    = 54883,
    SPELL_SHATTERED_HALLS_KEY             = 54884,
    SPELL_THE_MASTERS_KEY                 = 54885,
    SPELL_VIOLET_HOLD_KEY                 = 67253,
    SPELL_ESSENCE_INFUSED_MOONSTONE       = 40173,
};

#define GOSSIP_LOST_ARCATRAZ_KEY                "I've lost my key to the Arcatraz."
#define GOSSIP_LOST_SHADOWFORGE_KEY             "I've lost my key to the Blackrock Depths."
#define GOSSIP_LOST_SKELETON_KEY                "I've lost my key to the Scholomance."
#define GOSSIP_LOST_SHATTERED_HALLS_KEY         "I've lost my key to the Shattered Halls."
#define GOSSIP_LOST_THE_MASTERS_KEY             "I've lost my key to the Karazhan."
#define GOSSIP_LOST_VIOLET_HOLD_KEY             "I've lost my key to the Violet Hold."
#define GOSSIP_LOST_ESSENCE_INFUSED_MOONSTONE   "I've lost my Essence-Infused Moonstone."
#define GOSSIP_LOST_KEY_TO_THE_FOCUSING_IRIS    "I've lost my Key to the Focusing Iris."
#define GOSSIP_LOST_HC_KEY_TO_THE_FOCUSING_IRIS "I've lost my Heroic Key to the Focusing Iris."

class npc_locksmith : public CreatureScript
{
public:
    npc_locksmith() : CreatureScript("npc_locksmith") { }

    bool OnGossipHello(Player* player, Creature* creature)
    {
        // Arcatraz Key
        if (player->GetQuestRewardStatus(QUEST_HOW_TO_BRAKE_IN_TO_THE_ARCATRAZ) && !player->HasItemCount(ITEM_ARCATRAZ_KEY, 1, true))
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_LOST_ARCATRAZ_KEY, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);

        // Shadowforge Key
        if (player->GetQuestRewardStatus(QUEST_DARK_IRON_LEGACY) && !player->HasItemCount(ITEM_SHADOWFORGE_KEY, 1, true))
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_LOST_SHADOWFORGE_KEY, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);

        // Skeleton Key
        if ((player->GetQuestRewardStatus(QUEST_THE_KEY_TO_SCHOLOMANCE_A) || player->GetQuestRewardStatus(QUEST_THE_KEY_TO_SCHOLOMANCE_H)) &&
            !player->HasItemCount(ITEM_SKELETON_KEY, 1, true))
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_LOST_SKELETON_KEY, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);

        // Shatered Halls Key
        if ((player->GetQuestRewardStatus(QUEST_HOTTER_THAN_HELL_A) || player->GetQuestRewardStatus(QUEST_HOTTER_THAN_HELL_H)) &&
            !player->HasItemCount(ITEM_SHATTERED_HALLS_KEY, 1, true))
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_LOST_SHATTERED_HALLS_KEY, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 4);

        // Master's Key
        if (player->GetQuestRewardStatus(QUEST_RETURN_TO_KHAGDAR) && !player->HasItemCount(ITEM_THE_MASTERS_KEY, 1, true))
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_LOST_THE_MASTERS_KEY, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 5);

        // Violet Hold Key
        if (player->GetQuestRewardStatus(QUEST_CONTAINMENT) && !player->HasItemCount(ITEM_VIOLET_HOLD_KEY, 1, true))
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_LOST_VIOLET_HOLD_KEY, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 6);

        // Essence-Infused Moonstone
        if (player->GetQuestRewardStatus(QUEST_ETERNAL_VIGILANCE) && !player->HasItemCount(ITEM_ESSENCE_INFUSED_MOONSTONE, 1, true))
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_LOST_ESSENCE_INFUSED_MOONSTONE, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 7);

        // Key to the Focusing Iris
        if (player->GetQuestRewardStatus(QUEST_KEY_TO_THE_FOCUSING_IRIS) && !player->HasItemCount(ITEM_KEY_TO_THE_FOCUSING_IRIS, 1, true))
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_LOST_KEY_TO_THE_FOCUSING_IRIS, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 8);

        // Heroic Key to the Focusing Iris
        if (player->GetQuestRewardStatus(QUEST_HC_KEY_TO_THE_FOCUSING_IRIS) && !player->HasItemCount(ITEM_HC_KEY_TO_THE_FOCUSING_IRIS, 1, true))
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_LOST_HC_KEY_TO_THE_FOCUSING_IRIS, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 9);

        player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());

        return true;
    }

    bool OnGossipSelect(Player* player, Creature* /*creature*/, uint32 /*sender*/, uint32 action)
    {
        player->PlayerTalkClass->ClearMenus();
        switch (action)
        {
            case GOSSIP_ACTION_INFO_DEF + 1:
                player->CLOSE_GOSSIP_MENU();
                player->CastSpell(player, SPELL_ARCATRAZ_KEY, false);
                break;
            case GOSSIP_ACTION_INFO_DEF + 2:
                player->CLOSE_GOSSIP_MENU();
                player->CastSpell(player, SPELL_SHADOWFORGE_KEY, false);
                break;
            case GOSSIP_ACTION_INFO_DEF + 3:
                player->CLOSE_GOSSIP_MENU();
                player->CastSpell(player, SPELL_SKELETON_KEY, false);
                break;
            case GOSSIP_ACTION_INFO_DEF + 4:
                player->CLOSE_GOSSIP_MENU();
                player->CastSpell(player, SPELL_SHATTERED_HALLS_KEY, false);
                break;
            case GOSSIP_ACTION_INFO_DEF + 5:
                player->CLOSE_GOSSIP_MENU();
                player->CastSpell(player, SPELL_THE_MASTERS_KEY, false);
                break;
            case GOSSIP_ACTION_INFO_DEF + 6:
                player->CLOSE_GOSSIP_MENU();
                player->CastSpell(player, SPELL_VIOLET_HOLD_KEY, false);
                break;
            case GOSSIP_ACTION_INFO_DEF + 7:
                player->CLOSE_GOSSIP_MENU();
                player->CastSpell(player, SPELL_ESSENCE_INFUSED_MOONSTONE, false);
                break;
            case GOSSIP_ACTION_INFO_DEF + 8:
                player->CLOSE_GOSSIP_MENU();
                player->AddItem(ITEM_KEY_TO_THE_FOCUSING_IRIS, 1);
                break;
            case GOSSIP_ACTION_INFO_DEF + 9:
                player->CLOSE_GOSSIP_MENU();
                player->AddItem(ITEM_HC_KEY_TO_THE_FOCUSING_IRIS, 1);
                break;
        }
        return true;
    }
};

/*######
## npc_experience
######*/

#define EXP_COST                100000 //10 00 00 copper (10golds)
#define GOSSIP_TEXT_EXP         14736
#define GOSSIP_XP_OFF           "I no longer wish to gain experience."
#define GOSSIP_XP_ON            "I wish to start gaining experience again."

class npc_experience : public CreatureScript
{
public:
    npc_experience() : CreatureScript("npc_experience") { }

    bool OnGossipHello(Player* player, Creature* creature)
    {
        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_XP_OFF, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_XP_ON, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
        player->PlayerTalkClass->SendGossipMenu(GOSSIP_TEXT_EXP, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* /*creature*/, uint32 /*sender*/, uint32 action)
    {
        player->PlayerTalkClass->ClearMenus();
        bool noXPGain = player->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_NO_XP_GAIN);
        bool doSwitch = false;

        switch (action)
        {
            case GOSSIP_ACTION_INFO_DEF + 1://xp off
                {
                    if (!noXPGain)//does gain xp
                        doSwitch = true;//switch to don't gain xp
                }
                break;
            case GOSSIP_ACTION_INFO_DEF + 2://xp on
                {
                    if (noXPGain)//doesn't gain xp
                        doSwitch = true;//switch to gain xp
                }
                break;
        }
        if (doSwitch)
        {
            if (!player->HasEnoughMoney(uint64(EXP_COST)))
                player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, 0, 0, 0);
            else if (noXPGain)
            {
                player->ModifyMoney(-int64(EXP_COST));
                player->RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_NO_XP_GAIN);
            }
            else if (!noXPGain)
            {
                player->ModifyMoney(-EXP_COST);
                player->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_NO_XP_GAIN);
            }
        }
        player->PlayerTalkClass->SendCloseGossip();
        return true;
    }
};

enum Fireworks
{
    NPC_OMEN                = 15467,
    NPC_MINION_OF_OMEN      = 15466,
    NPC_FIREWORK_BLUE       = 15879,
    NPC_FIREWORK_GREEN      = 15880,
    NPC_FIREWORK_PURPLE     = 15881,
    NPC_FIREWORK_RED        = 15882,
    NPC_FIREWORK_YELLOW     = 15883,
    NPC_FIREWORK_WHITE      = 15884,
    NPC_FIREWORK_BIG_BLUE   = 15885,
    NPC_FIREWORK_BIG_GREEN  = 15886,
    NPC_FIREWORK_BIG_PURPLE = 15887,
    NPC_FIREWORK_BIG_RED    = 15888,
    NPC_FIREWORK_BIG_YELLOW = 15889,
    NPC_FIREWORK_BIG_WHITE  = 15890,

    NPC_CLUSTER_BLUE        = 15872,
    NPC_CLUSTER_RED         = 15873,
    NPC_CLUSTER_GREEN       = 15874,
    NPC_CLUSTER_PURPLE      = 15875,
    NPC_CLUSTER_WHITE       = 15876,
    NPC_CLUSTER_YELLOW      = 15877,
    NPC_CLUSTER_BIG_BLUE    = 15911,
    NPC_CLUSTER_BIG_GREEN   = 15912,
    NPC_CLUSTER_BIG_PURPLE  = 15913,
    NPC_CLUSTER_BIG_RED     = 15914,
    NPC_CLUSTER_BIG_WHITE   = 15915,
    NPC_CLUSTER_BIG_YELLOW  = 15916,
    NPC_CLUSTER_ELUNE       = 15918,

    GO_FIREWORK_LAUNCHER_1  = 180771,
    GO_FIREWORK_LAUNCHER_2  = 180868,
    GO_FIREWORK_LAUNCHER_3  = 180850,
    GO_CLUSTER_LAUNCHER_1   = 180772,
    GO_CLUSTER_LAUNCHER_2   = 180859,
    GO_CLUSTER_LAUNCHER_3   = 180869,
    GO_CLUSTER_LAUNCHER_4   = 180874,

    SPELL_ROCKET_BLUE       = 26344,
    SPELL_ROCKET_GREEN      = 26345,
    SPELL_ROCKET_PURPLE     = 26346,
    SPELL_ROCKET_RED        = 26347,
    SPELL_ROCKET_WHITE      = 26348,
    SPELL_ROCKET_YELLOW     = 26349,
    SPELL_ROCKET_BIG_BLUE   = 26351,
    SPELL_ROCKET_BIG_GREEN  = 26352,
    SPELL_ROCKET_BIG_PURPLE = 26353,
    SPELL_ROCKET_BIG_RED    = 26354,
    SPELL_ROCKET_BIG_WHITE  = 26355,
    SPELL_ROCKET_BIG_YELLOW = 26356,
    SPELL_LUNAR_FORTUNE     = 26522,

    ANIM_GO_LAUNCH_FIREWORK = 3,
    ZONE_MOONGLADE          = 493,
};

Position omenSummonPos = {7558.993f, -2839.999f, 450.0214f, 4.46f};

class npc_firework : public CreatureScript
{
public:
    npc_firework() : CreatureScript("npc_firework") { }

    struct npc_fireworkAI : public ScriptedAI
    {
        npc_fireworkAI(Creature* creature) : ScriptedAI(creature) {}

        bool isCluster()
        {
            switch (me->GetEntry())
            {
                case NPC_FIREWORK_BLUE:
                case NPC_FIREWORK_GREEN:
                case NPC_FIREWORK_PURPLE:
                case NPC_FIREWORK_RED:
                case NPC_FIREWORK_YELLOW:
                case NPC_FIREWORK_WHITE:
                case NPC_FIREWORK_BIG_BLUE:
                case NPC_FIREWORK_BIG_GREEN:
                case NPC_FIREWORK_BIG_PURPLE:
                case NPC_FIREWORK_BIG_RED:
                case NPC_FIREWORK_BIG_YELLOW:
                case NPC_FIREWORK_BIG_WHITE:
                    return false;
                case NPC_CLUSTER_BLUE:
                case NPC_CLUSTER_GREEN:
                case NPC_CLUSTER_PURPLE:
                case NPC_CLUSTER_RED:
                case NPC_CLUSTER_YELLOW:
                case NPC_CLUSTER_WHITE:
                case NPC_CLUSTER_BIG_BLUE:
                case NPC_CLUSTER_BIG_GREEN:
                case NPC_CLUSTER_BIG_PURPLE:
                case NPC_CLUSTER_BIG_RED:
                case NPC_CLUSTER_BIG_YELLOW:
                case NPC_CLUSTER_BIG_WHITE:
                case NPC_CLUSTER_ELUNE:
                default:
                    return true;
            }
        }

        GameObject* FindNearestLauncher()
        {
            GameObject* launcher = NULL;

            if (isCluster())
            {
                GameObject* launcher1 = GetClosestGameObjectWithEntry(me, GO_CLUSTER_LAUNCHER_1, 0.5f);
                GameObject* launcher2 = GetClosestGameObjectWithEntry(me, GO_CLUSTER_LAUNCHER_2, 0.5f);
                GameObject* launcher3 = GetClosestGameObjectWithEntry(me, GO_CLUSTER_LAUNCHER_3, 0.5f);
                GameObject* launcher4 = GetClosestGameObjectWithEntry(me, GO_CLUSTER_LAUNCHER_4, 0.5f);

                if (launcher1)
                    launcher = launcher1;
                else if (launcher2)
                    launcher = launcher2;
                else if (launcher3)
                    launcher = launcher3;
                else if (launcher4)
                    launcher = launcher4;
            }
            else
            {
                GameObject* launcher1 = GetClosestGameObjectWithEntry(me, GO_FIREWORK_LAUNCHER_1, 0.5f);
                GameObject* launcher2 = GetClosestGameObjectWithEntry(me, GO_FIREWORK_LAUNCHER_2, 0.5f);
                GameObject* launcher3 = GetClosestGameObjectWithEntry(me, GO_FIREWORK_LAUNCHER_3, 0.5f);

                if (launcher1)
                    launcher = launcher1;
                else if (launcher2)
                    launcher = launcher2;
                else if (launcher3)
                    launcher = launcher3;
            }

            return launcher;
        }

        uint32 GetFireworkSpell(uint32 entry)
        {
            switch (entry)
            {
                case NPC_FIREWORK_BLUE:
                    return SPELL_ROCKET_BLUE;
                case NPC_FIREWORK_GREEN:
                    return SPELL_ROCKET_GREEN;
                case NPC_FIREWORK_PURPLE:
                    return SPELL_ROCKET_PURPLE;
                case NPC_FIREWORK_RED:
                    return SPELL_ROCKET_RED;
                case NPC_FIREWORK_YELLOW:
                    return SPELL_ROCKET_YELLOW;
                case NPC_FIREWORK_WHITE:
                    return SPELL_ROCKET_WHITE;
                case NPC_FIREWORK_BIG_BLUE:
                    return SPELL_ROCKET_BIG_BLUE;
                case NPC_FIREWORK_BIG_GREEN:
                    return SPELL_ROCKET_BIG_GREEN;
                case NPC_FIREWORK_BIG_PURPLE:
                    return SPELL_ROCKET_BIG_PURPLE;
                case NPC_FIREWORK_BIG_RED:
                    return SPELL_ROCKET_BIG_RED;
                case NPC_FIREWORK_BIG_YELLOW:
                    return SPELL_ROCKET_BIG_YELLOW;
                case NPC_FIREWORK_BIG_WHITE:
                    return SPELL_ROCKET_BIG_WHITE;
                default:
                    return 0;
            }
        }

        uint32 GetFireworkGameObjectId()
        {
            uint32 spellId = 0;

            switch (me->GetEntry())
            {
                case NPC_CLUSTER_BLUE:
                    spellId = GetFireworkSpell(NPC_FIREWORK_BLUE);
                    break;
                case NPC_CLUSTER_GREEN:
                    spellId = GetFireworkSpell(NPC_FIREWORK_GREEN);
                    break;
                case NPC_CLUSTER_PURPLE:
                    spellId = GetFireworkSpell(NPC_FIREWORK_PURPLE);
                    break;
                case NPC_CLUSTER_RED:
                    spellId = GetFireworkSpell(NPC_FIREWORK_RED);
                    break;
                case NPC_CLUSTER_YELLOW:
                    spellId = GetFireworkSpell(NPC_FIREWORK_YELLOW);
                    break;
                case NPC_CLUSTER_WHITE:
                    spellId = GetFireworkSpell(NPC_FIREWORK_WHITE);
                    break;
                case NPC_CLUSTER_BIG_BLUE:
                    spellId = GetFireworkSpell(NPC_FIREWORK_BIG_BLUE);
                    break;
                case NPC_CLUSTER_BIG_GREEN:
                    spellId = GetFireworkSpell(NPC_FIREWORK_BIG_GREEN);
                    break;
                case NPC_CLUSTER_BIG_PURPLE:
                    spellId = GetFireworkSpell(NPC_FIREWORK_BIG_PURPLE);
                    break;
                case NPC_CLUSTER_BIG_RED:
                    spellId = GetFireworkSpell(NPC_FIREWORK_BIG_RED);
                    break;
                case NPC_CLUSTER_BIG_YELLOW:
                    spellId = GetFireworkSpell(NPC_FIREWORK_BIG_YELLOW);
                    break;
                case NPC_CLUSTER_BIG_WHITE:
                    spellId = GetFireworkSpell(NPC_FIREWORK_BIG_WHITE);
                    break;
                case NPC_CLUSTER_ELUNE:
                    spellId = GetFireworkSpell(urand(NPC_FIREWORK_BLUE, NPC_FIREWORK_WHITE));
                    break;
            }

            const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);

            if (spellInfo && spellInfo->Effects[0].Effect == SPELL_EFFECT_SUMMON_OBJECT_WILD)
                return spellInfo->Effects[0].MiscValue;

            return 0;
        }

        void Reset()
        {
            if (GameObject* launcher = FindNearestLauncher())
            {
                launcher->SendCustomAnim(ANIM_GO_LAUNCH_FIREWORK);
                me->SetOrientation(launcher->GetOrientation() + M_PI/2);
            }
            else
                return;

            if (isCluster())
            {
                // Check if we are near Elune'ara lake south, if so try to summon Omen or a minion
                if (me->GetZoneId() == ZONE_MOONGLADE)
                {
                    if (!me->FindNearestCreature(NPC_OMEN, 100.0f, false) && me->GetDistance2d(omenSummonPos.GetPositionX(), omenSummonPos.GetPositionY()) <= 100.0f)
                    {
                        switch (urand(0, 9))
                        {
                            case 0:
                            case 1:
                            case 2:
                            case 3:
                                if (Creature* minion = me->SummonCreature(NPC_MINION_OF_OMEN, me->GetPositionX()+frand(-5.0f, 5.0f), me->GetPositionY()+frand(-5.0f, 5.0f), me->GetPositionZ(), 0.0f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000))
                                    minion->AI()->AttackStart(me->SelectNearestPlayer(20.0f));
                                break;
                            case 9:
                                me->SummonCreature(NPC_OMEN, omenSummonPos);
                                break;
                        }
                    }
                }
                if (me->GetEntry() == NPC_CLUSTER_ELUNE)
                    DoCast(SPELL_LUNAR_FORTUNE);

                float displacement = 0.7f;
                for (uint8 i = 0; i < 4; i++)
                    me->SummonGameObject(GetFireworkGameObjectId(), me->GetPositionX() + (i%2 == 0 ? displacement : -displacement), me->GetPositionY() + (i > 1 ? displacement : -displacement), me->GetPositionZ() + 4.0f, me->GetOrientation(), 0.0f, 0.0f, 0.0f, 0.0f, 1);
            }
            else
                //me->CastSpell(me, GetFireworkSpell(me->GetEntry()), true);
                me->CastSpell(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), GetFireworkSpell(me->GetEntry()), true);
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_fireworkAI(creature);
    }
};

/*#####
# npc_spring_rabbit
#####*/

enum rabbitSpells
{
    SPELL_SPRING_FLING          = 61875,
    SPELL_SPRING_RABBIT_JUMP    = 61724,
    SPELL_SPRING_RABBIT_WANDER  = 61726,
    SPELL_SUMMON_BABY_BUNNY     = 61727,
    SPELL_SPRING_RABBIT_IN_LOVE = 61728,
    NPC_SPRING_RABBIT           = 32791
};

class npc_spring_rabbit : public CreatureScript
{
public:
    npc_spring_rabbit() : CreatureScript("npc_spring_rabbit") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_spring_rabbitAI(creature);
    }

    struct npc_spring_rabbitAI : public ScriptedAI
    {
        npc_spring_rabbitAI(Creature* creature) : ScriptedAI(creature) { }

        bool inLove;
        uint32 jumpTimer;
        uint32 bunnyTimer;
        uint32 searchTimer;
        uint64 rabbitGUID;

        void Reset()
        {
            inLove = false;
            rabbitGUID = 0;
            jumpTimer = urand(5000, 10000);
            bunnyTimer = urand(10000, 20000);
            searchTimer = urand(5000, 10000);
            if (Unit* owner = me->GetOwner())
                me->GetMotionMaster()->MoveFollow(owner, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);
        }

        void EnterCombat(Unit* /*who*/) { }

        void DoAction(const int32 /*param*/)
        {
            inLove = true;
            if (Unit* owner = me->GetOwner())
                owner->CastSpell(owner, SPELL_SPRING_FLING, true);
        }

        void UpdateAI(const uint32 diff)
        {
            if (inLove)
            {
                if (jumpTimer <= diff)
                {
                    if (Unit* rabbit = Unit::GetUnit(*me, rabbitGUID))
                        DoCast(rabbit, SPELL_SPRING_RABBIT_JUMP);
                    jumpTimer = urand(5000, 10000);
                } else jumpTimer -= diff;

                if (bunnyTimer <= diff)
                {
                    DoCast(SPELL_SUMMON_BABY_BUNNY);
                    bunnyTimer = urand(20000, 40000);
                } else bunnyTimer -= diff;
            }
            else
            {
                if (searchTimer <= diff)
                {
                    if (Creature* rabbit = me->FindNearestCreature(NPC_SPRING_RABBIT, 10.0f))
                    {
                        if (rabbit == me || rabbit->HasAura(SPELL_SPRING_RABBIT_IN_LOVE))
                            return;

                        me->AddAura(SPELL_SPRING_RABBIT_IN_LOVE, me);
                        DoAction(1);
                        rabbit->AddAura(SPELL_SPRING_RABBIT_IN_LOVE, rabbit);
                        rabbit->AI()->DoAction(1);
                        rabbit->CastSpell(rabbit, SPELL_SPRING_RABBIT_JUMP, true);
                        rabbitGUID = rabbit->GetGUID();
                    }
                    searchTimer = urand(5000, 10000);
                } else searchTimer -= diff;
            }
        }
    };
};

/*######
## npc_generic_harpoon_cannon
######*/

class npc_generic_harpoon_cannon : public CreatureScript
{
public:
    npc_generic_harpoon_cannon() : CreatureScript("npc_generic_harpoon_cannon") { }

    struct npc_generic_harpoon_cannonAI : public ScriptedAI
    {
        npc_generic_harpoon_cannonAI(Creature* creature) : ScriptedAI(creature) {}

        void Reset()
        {
            me->SetUnitMovementFlags(MOVEMENTFLAG_ROOT);
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_generic_harpoon_cannonAI(creature);
    }
};

class npc_hand_of_guldan : public CreatureScript
{
public:
   npc_hand_of_guldan() : CreatureScript("npc_hand_of_guldan") {}

   struct npc_hand_of_guldanAI : public ScriptedAI
   {
       npc_hand_of_guldanAI(Creature* creature) : ScriptedAI(creature) { }

       bool SummonerHaveAuraofForeboding;
       uint32 AuraofForebodingStunTimer;
       uint32 AuraofForebodingStunSpell;

       void Reset()
       {
           SummonerHaveAuraofForeboding = false;
           AuraofForebodingStunTimer = 6000;
           AuraofForebodingStunSpell = 0;

           me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE|UNIT_FLAG_NON_ATTACKABLE);
           me->SetReactState(REACT_PASSIVE);

           me->CastSpell(me, 85526, true);

           if (Unit* owner = me->GetOwner())
           {
               me->setFaction(owner->getFaction());
               me->CastSpell(me, 86000, true);

               if (owner->HasAura(89605))
               {
                   SummonerHaveAuraofForeboding = true;
                   AuraofForebodingStunSpell = 93986;
                   me->CastSpell(me, 93987, true);
               }
               else if (owner->HasAura(89604))
               {
                   SummonerHaveAuraofForeboding = true;
                   AuraofForebodingStunSpell = 93975;
                   me->CastSpell(me, 93974, true);
               }
           }
       }

       void UpdateAI(const uint32 diff)
       {
           if (SummonerHaveAuraofForeboding)
           {
               if (AuraofForebodingStunTimer <= diff)
               {
                   me->CastSpell(me, AuraofForebodingStunSpell, true);
                   SummonerHaveAuraofForeboding = false;
               }
               else
                   AuraofForebodingStunTimer -= diff;
           }
       }

       void EnterCombat(Unit* /*who*/) {}
       void AttackStart(Unit* /*who*/) {}
       void EnterEvadeMode() {}
   };

   CreatureAI* GetAI(Creature* creature) const
   {
       return new npc_hand_of_guldanAI(creature);
   }
};

class npc_druid_treant : public CreatureScript
{
public:
    npc_druid_treant() : CreatureScript("npc_druid_treant") { }

    struct npc_druid_treantAI : public ScriptedAI
    {
        npc_druid_treantAI(Creature* creature) : ScriptedAI(creature)
        {
            DoCast(me, 65220, true);
        }

        void JustDied(Unit* /*killer*/)
        {
            if (Unit* owner = me->GetOwner())
            {
                if (owner)
                {
                    if (AuraEffect* aurEff = owner->GetDummyAuraEffect(SPELLFAMILY_DRUID, 2681, EFFECT_0))
                    {
                        uint32 triggeredId = 0;
                        switch (aurEff->GetAmount())
                        {
                        case 25:
                            triggeredId = 81291;
                            break;
                        case 50:
                            triggeredId = 81283;
                            break;
                        }
                        owner->CastSpell(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), triggeredId, true);
                    }
                }
            }
        }

        void Reset()
        {
            me->SetReactState(REACT_ASSIST);
        }

        void UpdateAI(uint32 diff)
        {
            if (me->GetReactState() == REACT_AGGRESSIVE && !me->getVictim())
                me->SetReactState(REACT_ASSIST);

            if (me->GetReactState() == REACT_ASSIST && me->getVictim())
                me->SetReactState(REACT_AGGRESSIVE);

            if (Unit* Owner = me->GetOwner())
            {
                if (!me->getVictim() && Owner->getVictim())
                    AttackStart(Owner->getVictim());

                if (!me->getVictim() && !Owner->getVictim())
                    //EnterEvadeMode();
                    _EnterEvadeMode();
            }

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_druid_treantAI(creature);
    }
};


enum eArmyDeadGhoul
{
    SPELL_TAUNT             = 85667,
    SPELL_CLAW              = 91776,
    SPELL_LEAP              = 91809,
    SPELL_AOE_AVOIDANCE     = 62137
};

class npc_army_dead_ghoul : public CreatureScript
{
public:
    npc_army_dead_ghoul() : CreatureScript("npc_army_dead_ghoul") { }

    struct npc_army_dead_ghoulAI : public ScriptedAI
    {
        npc_army_dead_ghoulAI(Creature* creature) : ScriptedAI(creature)
        {
            DoCast(me, SPELL_AOE_AVOIDANCE, true);
        }

        void Reset()
        {
            me->SetReactState(REACT_ASSIST);
            me->setPowerType(POWER_ENERGY);
            me->SetMaxPower(POWER_ENERGY, 100);
            ClawTimer = 2000;
            TauntTimer = 2500;
        }

        void EnterCombat(Unit* who)
        {
            if (me->GetDistance2d(who) > 5.0f)
                DoCast(who, SPELL_LEAP, true);
        }

        void UpdateAI(const uint32 diff)
        {
            if (me->GetReactState() == REACT_AGGRESSIVE && !me->getVictim())
                me->SetReactState(REACT_ASSIST);

            if (me->GetReactState() == REACT_ASSIST && me->getVictim())
                me->SetReactState(REACT_AGGRESSIVE);

            if (Unit* Owner = me->GetOwner())
            {
                if (!me->getVictim() && Owner->getVictim())
                    AttackStart(Owner->getVictim());

                if (!me->getVictim() && !Owner->getVictim())
                    EnterEvadeMode();
            }

            if (ClawTimer < diff)
            {
                if (me->GetPower(POWER_ENERGY) >= 40)
                {
                    DoCastVictim(SPELL_CLAW, true);
                    me->SetPower(POWER_ENERGY, me->GetPower(POWER_ENERGY) - 40);
                }
                ClawTimer = urand(3000, 5000);
            } else ClawTimer -= diff;

            if (TauntTimer < diff)
            {
                if (Unit* target = me->getVictim())
                {
                    if (target->GetTypeId() == TYPEID_UNIT)
                    {
                        if (!target->ToCreature()->isWorldBoss())
                            DoCastVictim(SPELL_TAUNT, true);
                    } else DoCastVictim(SPELL_TAUNT, true);
                }
                TauntTimer = urand(1000, 2000);
            } else TauntTimer -= diff;

            DoMeleeAttackIfReady();
        }

    private:
        uint32 ClawTimer;
        uint32 TauntTimer;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_army_dead_ghoulAI(creature);
    }
};

// plant vs zombi
enum pvszspells
{
    SPELL_PLANTER_TOURNESOL = 91646,
    SPELL_PLANTER_CRACHEUR = 91649,
    SPELL_PLANTER_VIGNE_ETRANGLEUSE = 91710,
    SPELL_PLANTER_GENRAIUM = 92440,
    SPELL_BOMBE_CITROUILLE = 92157,
    SPELL_NOIX_PIERRE = 91704,
    SPELL_OBTENIR_PURIN = 93100,

    SPELL_SUMMON_GOAL_NPC = 91988,

    SPELL_GELANIUM_BREATH = 92441,

};

enum pvszgob
{
    GO_TOURNESOL   = 207282,
    GO_CRACHEUR    = 207284,
    GO_VIGNE       = 207283,
    GO_GENRAIUM    = 207350,
    GO_CITROUILLE  = 207326,
    GO_NOIX_PIERRE = 207285,
};

enum pvsznpcs
{
    NPC_BRAZIE_PLAYER_VEHICLE = 49176,

    NPC_SUNFLOWER = 49692,
    NPC_CRACHEUR  = 49697,

    NPC_GELANIUM  = 49696,
};

class npc_tower_defense : public CreatureScript
{
public:
    npc_tower_defense() : CreatureScript("npc_tower_defense") { }

    struct npc_tower_defenseAI : public ScriptedAI
    {
        npc_tower_defenseAI(Creature* creature) : ScriptedAI(creature) {}

        void AttackStart(Unit* /*who*/) {}
        void EnterCombat(Unit* /*who*/) {}
        void EnterEvadeMode() {}

        void Reset()
        {
            me->SetMaxPower(POWER_ENERGY, 100);
            me->SetPower(POWER_ENERGY, 50);
        }

        void PassengerBoarded(Unit* who, int8 /*seatId*/, bool /*apply*/)
        {
            if (who->GetTypeId() == TYPEID_PLAYER)
                me->SetPower(POWER_ENERGY, 100);
        }

        void UpdateAI(const uint32 /*diff*/)
        {
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_tower_defenseAI (creature);
    }
};

#define NPC_TD_MARKER 300000

class npc_td_marker : public CreatureScript
{
public:
    npc_td_marker() : CreatureScript("npc_td_marker") { }

    struct npc_td_markerAI : public ScriptedAI
    {
        npc_td_markerAI(Creature* creature) : ScriptedAI(creature) {}

        void AttackStart(Unit* /*who*/) {}
        void EnterCombat(Unit* /*who*/) {}
        void EnterEvadeMode() {}

        void Reset()
        {
            _data = 0;
        }

        uint32 GetData(uint32 /*data*/) const
        {
            return _data;
        }

        void SetData(uint32 /*type*/, uint32 data)
        {
            _data = data;
        }

        bool IsSpawnSpell(uint32 spell_id)
        {
            switch (spell_id)
            {
                case SPELL_PLANTER_TOURNESOL:
                case SPELL_PLANTER_CRACHEUR:
                case SPELL_PLANTER_VIGNE_ETRANGLEUSE:
                case SPELL_PLANTER_GENRAIUM:
                case SPELL_BOMBE_CITROUILLE:
                case SPELL_NOIX_PIERRE:
                    return true;
                default:
                    break;
            }
            return false;
        }

        uint32 GetRelativeNpc(uint32 spell_id)
        {
            switch (spell_id)
            {
                case SPELL_PLANTER_TOURNESOL:
                    return NPC_SUNFLOWER;
                case SPELL_PLANTER_CRACHEUR:
                    return NPC_CRACHEUR;
                case SPELL_PLANTER_VIGNE_ETRANGLEUSE:
                    return 0;
                case SPELL_PLANTER_GENRAIUM:
                    return NPC_GELANIUM;
                case SPELL_BOMBE_CITROUILLE:
                    return 0;
                case SPELL_NOIX_PIERRE:
                    return 0;
                default:
                    break;
            }
            return 0;
        }


        uint32 GetRelativeGob(uint32 spell_id)
        {
            switch (spell_id)
            {
                case SPELL_PLANTER_TOURNESOL:
                    return GO_TOURNESOL;
                case SPELL_PLANTER_CRACHEUR:
                    return GO_CRACHEUR;
                case SPELL_PLANTER_VIGNE_ETRANGLEUSE:
                    return GO_VIGNE;
                case SPELL_PLANTER_GENRAIUM:
                    return GO_GENRAIUM;
                case SPELL_BOMBE_CITROUILLE:
                    return GO_CITROUILLE;
                case SPELL_NOIX_PIERRE:
                    return GO_NOIX_PIERRE;
                default:
                    break;
            }
            return 0;
        }

        void ClearOldTower()
        {
            if (Creature *c = me->FindNearestCreature(GetRelativeNpc(SPELL_PLANTER_TOURNESOL), 5.0f))
                c->DespawnOrUnsummon();
            if (Creature *c = me->FindNearestCreature(GetRelativeNpc(SPELL_PLANTER_CRACHEUR), 5.0f))
                c->DespawnOrUnsummon();
            if (Creature *c = me->FindNearestCreature(GetRelativeNpc(SPELL_PLANTER_VIGNE_ETRANGLEUSE), 5.0f))
                c->DespawnOrUnsummon();
            if (Creature *c = me->FindNearestCreature(GetRelativeNpc(SPELL_PLANTER_GENRAIUM), 5.0f))
                c->DespawnOrUnsummon();
            if (Creature *c = me->FindNearestCreature(GetRelativeNpc(SPELL_BOMBE_CITROUILLE), 5.0f))
                c->DespawnOrUnsummon();
            if (Creature *c = me->FindNearestCreature(GetRelativeNpc(SPELL_NOIX_PIERRE), 5.0f))
                c->DespawnOrUnsummon();
        }

        void SpellHit(Unit* /*caster*/, SpellInfo const* spell)
        {
            if (IsSpawnSpell(spell->Id))
            {
                ClearOldTower();
                if (GameObject *go = me->FindNearestGameObject(GetRelativeGob(spell->Id), 6.0f))
                    go->Delete();
                Position pos;
                me->GetPosition(&pos);
                me->SummonCreature(GetRelativeNpc(spell->Id), pos);
            }
        }

        void UpdateAI(const uint32 /*diff*/)
        {
        }

        uint32 _data;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_td_markerAI (creature);
    }
};

enum parasiteSpells
{
    SPELL_BLOOD_BURST  = 81280,
    SPELL_DK_SCALING_5 = 110474,
};

class npc_dk_blood_parasite : public CreatureScript
{
public:
    npc_dk_blood_parasite() : CreatureScript("npc_dk_blood_parasite") { }

    struct npc_dk_blood_parasiteAI : public ScriptedAI
    {
        npc_dk_blood_parasiteAI(Creature* creature) : ScriptedAI(creature)
        {
            DoCast(me, SPELL_DK_SCALING_5, true);
        }

        void Reset()
        {
            if (Unit* summoner = me->GetOwner())
                if (Unit* target = summoner->getVictim())
                    if (me->canCreatureAttack(target))
                        me->AI()->AttackStart(target);
        }

        void JustDied(Unit* /*killer*/)
        {
            DoCast(me, SPELL_BLOOD_BURST, true);
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_dk_blood_parasiteAI(creature);
    }
};

enum searingTotem
{
    SPELL_TOTEMIC_WRATH  = 77746,
    SPELL_SEARING_BOLT   = 3606,
    SPELL_FLAME_SHOCK    = 8050,
    SPELL_STORMSTRIKE    = 17364,
    EVENT_SEARING_BOLT   = 1
};

class npc_sha_searing_totem : public CreatureScript
{
public:
    npc_sha_searing_totem() : CreatureScript("npc_sha_searing_totem") { }

    struct npc_sha_searing_totemAI : public ScriptedAI
    {
        npc_sha_searing_totemAI(Creature* creature) : ScriptedAI(creature) {}

        void Reset()
        {
            if (!me)
                return;

            if (!me->GetGUID())
                return;

            if (!me->HasUnitState(UnitState::UNIT_STATE_ROOT))
                me->AddUnitState(UnitState::UNIT_STATE_ROOT);

            if (me->GetReactState() != REACT_ASSIST)
                me->SetReactState(REACT_ASSIST);

            if (Unit* owner = me->GetOwner())
            {
                if (owner->getVictim())
                    AttackStart(owner->getVictim());

                if (owner->HasAura(28999))
                    if (!me->HasAura(28999))
                        me->AddAura(28999, me);

                if (owner->HasAura(29000))
                    if (!me->HasAura(29000))
                        me->AddAura(29000, me);

                if (owner->HasAura(86935))
                    if (!me->HasAura(86935))
                        me->AddAura(86935, me);

                if (owner->HasAura(86936))
                    if (!me->HasAura(86936))
                        me->AddAura(86936, me);
            }
            else
            {
                me->DespawnOrUnsummon();
            }
        }

        void EnterCombat(Unit* who)
        {
            if (!me)
                return;

            if (!who)
                return;
            if (!me->isAlive())
                return;
            if (!me->isInCombat())
                if (me->ToUnit()->IsValidAttackTarget(who))
                if (me->GetDistance(who) <= GetSearingDistance())
                {
                    AttackStartNoMove(who);
                }
        }

        float GetSearingDistance()
        {
            float value;
            value = sSpellMgr->GetSpellInfo(3606)->GetMaxRange();

            if (!value)
            {
                TC_LOG_ERROR("bg.battleground", "SearingTotem npc script: Searing distance is null.");
            }
            if (Unit* caster = me->ToUnit())
            {
                if (AuraEffect* eff1 = caster->GetAuraEffect(86935, EFFECT_0, caster->GetGUID()))
                    AddPct(value, eff1->GetAmount());
                if (AuraEffect* eff2 = caster->GetAuraEffect(86936, EFFECT_0, caster->GetGUID()))
                    AddPct(value, eff2->GetAmount());
                if (AuraEffect* eff3 = caster->GetAuraEffect(28999, EFFECT_2, caster->GetGUID()))
                    value += eff3->GetAmount();
                if (AuraEffect* eff4 = caster->GetAuraEffect(29000, EFFECT_2, caster->GetGUID()))
                    value += eff4->GetAmount();
            }
            return value;
        }//comment added to simplify diff patch

        void UpdateAI(const uint32 diff)
        {
            if (!me)
                return;
            if (UpdateVictim())
            if (Unit* owner = me->GetOwner())
            {
                if (auto v = me->getVictim())
                {
                    if (me->GetDistance(v) > GetSearingDistance())
                        return;
                    if (me->HasUnitState(UnitState::UNIT_STATE_CASTING))
                        return;

                    if (me->ToUnit()->IsValidAttackTarget(v))
                        me->CastSpell(v, SPELL_SEARING_BOLT, false);
                }
            }
            else
            {
                Unit* ownertarget = NULL;
                if (owner->ToPlayer())
                {
                    ownertarget = owner->ToPlayer()->GetSelectedUnit();
                }
                else
                {
                    ownertarget = owner->getVictim();
                }
                if (ownertarget)
                    if (owner->isInCombat())
                        if (ownertarget->isInCombat())
                            if (me->IsValidAttackTarget(ownertarget))
                                if (me->GetDistance(ownertarget) <= GetSearingDistance())
                                {
                                    AttackStartNoMove(ownertarget);
                                }
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_sha_searing_totemAI(creature);
    }
};

enum doomguardSpells
{
    SPELL_RITUAL_ENSLAVEMENT    = 22987,
    SPELL_DOOM_BOLT             = 85692,
    SPELL_BANE_OF_DOOM          = 603,
    SPELL_BANE_OF_AGONY         = 980,
    SPELL_WARLOCK_SCALING_01    = 34947,
    SPELL_WARLOCK_SCALING_02    = 34956,
    SPELL_WARLOCK_SCALING_03    = 34957,
    SPELL_WARLOCK_SCALING_04    = 34958,
    SPELL_WARLOCK_SCALING_05    = 61013,
    SPELL_WARLOCK_SCALING_06    = 89953,
    SPELL_WARL_AOE_AVOIDANCE    = 32233,
    SPELL_PET_PASSIVE_DND       = 96101, // @TODO: check if impl.

};

class npc_warl_doomguard : public CreatureScript
{
public:
    npc_warl_doomguard() : CreatureScript("npc_warl_doomguard") { }

    struct npc_warl_doomguardAI : public ScriptedAI
    {
        npc_warl_doomguardAI(Creature* creature) : ScriptedAI(creature)
        {
            DoCast(me, SPELL_WARL_AOE_AVOIDANCE, true);
            DoCast(me, SPELL_PET_PASSIVE_DND, true);
            DoCast(me, SPELL_WARLOCK_SCALING_01, true);
            DoCast(me, SPELL_WARLOCK_SCALING_02, true);
            DoCast(me, SPELL_WARLOCK_SCALING_03, true);
            DoCast(me, SPELL_WARLOCK_SCALING_04, true);
            DoCast(me, SPELL_WARLOCK_SCALING_05, true);
            DoCast(me, SPELL_WARLOCK_SCALING_06, true);
            DoCast(me, SPELL_RITUAL_ENSLAVEMENT, true);
        }

        void Reset()
        {
            doomboltTimer = 500;
        }

        void UpdateAI(const uint32 diff)
        {
            if (doomboltTimer < diff)
            {
                if (Unit* owner = me->GetOwner())
                {
                    targetFound = false;
                    std::list<Unit*> targets;
                    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(me, me, range);
                    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(me, targets, u_check);
                    me->VisitNearbyObject(range, searcher);
                    for (std::list<Unit*>::iterator itr = targets.begin(); itr != targets.end(); ++itr)
                    {
                        if (Unit* target = (*itr)->ToUnit())
                        {
                            if (target->HasAura(SPELL_BANE_OF_DOOM, owner->GetGUID()) || target->HasAura(SPELL_BANE_OF_AGONY, owner->GetGUID()))
                            {
                                DoCast(target, SPELL_DOOM_BOLT, false);
                                targetFound = true;
                                break;
                            }
                        }
                    }

                    if (!targetFound)
                        if (Unit* target = owner->getAttackerForHelper())
                            DoCast(target, SPELL_DOOM_BOLT, false);
                }

                doomboltTimer = 1500;
            } else doomboltTimer -= diff;
        }

    private:
        bool targetFound;
        uint32 doomboltTimer;
        float range = 30.f;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_warl_doomguardAI(creature);
    }
};

class npc_warl_ebon_imp : public CreatureScript
{
public:
    npc_warl_ebon_imp() : CreatureScript("npc_warl_ebon_imp") { }

    struct npc_warl_ebon_impAI : public ScriptedAI
    {
        npc_warl_ebon_impAI(Creature* creature) : ScriptedAI(creature)
        {
            DoCast(me, SPELL_WARLOCK_SCALING_04, true);
            DoCast(me, SPELL_WARLOCK_SCALING_05, true);
        }

        void Reset()
        {
            fireboltTimer = 500;
            spellId = me->GetEntry() == 50675 ? 3110 : 99226;
        }

        void UpdateAI(const uint32 diff)
        {
            if (fireboltTimer < diff)
            {
                if (Unit* owner = me->GetOwner())
                {
                    targetFound = false;
                    std::list<Unit*> targets;
                    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(me, me, 40);
                    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(me, targets, u_check);
                    me->VisitNearbyObject(40, searcher);
                    for (std::list<Unit*>::iterator itr = targets.begin(); itr != targets.end(); ++itr)
                    {
                        if (Unit* target = (*itr)->ToUnit())
                        {
                            if (target->HasAura(SPELL_BANE_OF_DOOM, owner->GetGUID()))
                            {
                                DoCast(target, spellId, false);
                                targetFound = true;
                                break;
                            }
                        }
                    }

                    if (!targetFound)
                        if (Unit* owner = me->GetOwner())
                            if (Unit* target = owner->getVictim())
                                DoCast(target, spellId, false);
                }

                fireboltTimer = me->GetEntry() == 50675 ? 2500 : 1500;
            }
            else fireboltTimer -= diff;
        }

    private:
        bool targetFound;
        uint32 fireboltTimer;
        uint32 spellId;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_warl_ebon_impAI(creature);
    }
};

/*####
## npc_baby_moonkin
####*/

#define SPELL_FLOWER_SUMMON 61818
#define MOONKIN_PASSIVE 95812
#define MOONKIN_DANCE 95801
class npc_baby_moonkin : public CreatureScript
{
public:
    npc_baby_moonkin() : CreatureScript("npc_baby_moonkin") { }

    struct npc_baby_moonkinAI : public PassiveAI
    {
        npc_baby_moonkinAI(Creature* creature) : PassiveAI(creature) { }

        void ReceiveEmote(Player* /*player*/, uint32 emote)
        {
            if (emote == TEXT_EMOTE_DANCE)
                DoCast(MOONKIN_DANCE);
        }

        void Reset()
        {
            DoCast(MOONKIN_PASSIVE);
        }

        void UpdateAI(const uint32 /*diff*/)
        {
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_baby_moonkinAI(creature);
    }
};

// npc_winter_reveler

enum WinterReveler
{
    SPELL_MISTLETOE_DEBUFF       = 26218,
    SPELL_CREATE_MISTLETOE       = 26206,
    SPELL_CREATE_HOLLY           = 26207,
    SPELL_CREATE_SNOWFLAKES      = 45036,
};

class npc_winter_reveler : public CreatureScript
{
public:
    npc_winter_reveler() : CreatureScript("npc_winter_reveler") { }

    struct npc_winter_revelerAI : public ScriptedAI
    {
        npc_winter_revelerAI(Creature* c) : ScriptedAI(c) {}

        void ReceiveEmote(Player* player, uint32 emote)
        {
            if (player->HasAura(SPELL_MISTLETOE_DEBUFF))
                return;

            if (!IsHolidayActive(HOLIDAY_FEAST_OF_WINTER_VEIL))
                return;

            if (emote == TEXT_EMOTE_KISS)
            {
                me->CastSpell(player, SPELL_MISTLETOE_DEBUFF, true);
                uint32 spellId = RAND<uint32>(SPELL_CREATE_MISTLETOE, SPELL_CREATE_HOLLY, SPELL_CREATE_SNOWFLAKES);
                me->CastSpell(player, spellId, true);
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_winter_revelerAI(creature);
    }
};

/*######
# npc_abo_grinch
######*/
enum spellsGrinch
{
    SPELL_ABOMINABLE_CURSH  = 101885,
    SPELL_CLEAVE            = 40504,
    SPELL_SHRINK_HEART      = 101873,
    THROW_WINTER_VEIL       = 101938,
    THROW_STRANGE_SNOWMAN   = 101910,
    //snowman
    SPELL_SNOW_CRASH        = 101907,
    SPELL_METZEN_CREDITS    = 102760,
};

enum creaturesGrinch
{
    NPC_ABOMINABLE_GRINCH       = 54499,
    NPC_WINTER_TREE             = 54519,
    NPC_STRANGE_SNOWMAN         = 54523,
    NPC_NASTY_LITTLE_HELPER     = 54509,
    NPC_WICKED_LITTLE_HELPER    = 54524,
    NPC_METZEN                  = 15664,
    NPC_ROTTEN_LITTLE_HELPER    = 55003,
};

enum eventsGrinch
{
    EVENT_ABOMINABLE_CRUSH = 1,
    EVENT_ABO_CHARGE,
    EVENT_THROW_WINTER_VEIL_TREE,
    EVENT_THROW_STRANGE_SNOWMAN,
    EVENT_SHRINK,
    EVENT_CLEAVE,
};

Position const posGob[10] =
{
    { 225.609f, -259.333f, 145.006f, 0.0f },
    { 232.154f, -266.674f, 145.929f, 0.0f },
    { 224.170f, -278.483f, 146.675f, 0.0f },
    { 225.609f, -259.333f, 145.006f, 0.0f },
    { 232.154f, -266.674f, 145.929f, 0.0f },
    { 224.170f, -278.483f, 146.675f, 0.0f },
    { 234.243f, -271.249f, 146.583f, 0.0f },
    { 228.542f, -265.848f, 145.672f, 0.0f },
    { 225.439f, -271.361f, 146.145f, 0.0f },
    { 225.212f, -267.329f, 145.440f, 0.0f }
};

class npc_abo_grinch : public CreatureScript
{
public:
    npc_abo_grinch() : CreatureScript("npc_abo_grinch") { }

    class SnowManEvent : public BasicEvent
    {
    public:
        SnowManEvent(Creature* own) : owner(own)
        {
        }

        bool Execute(uint64 execTime, uint32 /*diff*/)
        {
            owner->AI()->DoCastRandom(SPELL_SNOW_CRASH, 0.0f);
            owner->m_Events.AddEvent(this, execTime + 6000);
            return false;
        }

    private:
        Creature *owner;
    };

    class metzenRunDelayed : public BasicEvent
    {
    public:
        metzenRunDelayed(Creature* own) : owner(own) {}

        bool Execute(uint64 /*execTime*/, uint32 /*diff*/)
        {
            owner->GetMotionMaster()->MovePoint(0, 190.005f, -288.830f, 151.532f, true);
            owner->m_Events.AddEvent(new RewardQuestCredit(owner), owner->m_Events.CalculateTime(5000));
            return true;
        }
    private:
        Creature *owner;
    };

    class RewardQuestCredit : public BasicEvent
    {
    public:
        RewardQuestCredit(Creature* own) : owner(own) {}

        bool Execute(uint64 /*execTime*/, uint32 /*diff*/)
        {
            owner->CastSpell((Unit*)NULL, SPELL_METZEN_CREDITS, true);
            owner->DespawnOrUnsummon(10000);
            return true;
        }
    private:
        Creature *owner;
    };

    struct npc_abo_grinchAI : public ScriptedAI
    {
        npc_abo_grinchAI(Creature* creature) : ScriptedAI(creature),  Summons(me) {}

        void Reset()
        {
            events.Reset();
            Summons.DespawnAll();
            if (Vehicle *veh = me->GetVehicleKit())
                veh->RemoveAllPassengers();
            if (Creature *metzen = me->FindNearestCreature(NPC_METZEN, 200.0f, false))
                if (!metzen->isAlive())
                    metzen->Respawn(true);

            std::list<Creature*> creatures;
            GetCreatureListWithEntryInGrid(creatures, me, NPC_ROTTEN_LITTLE_HELPER, 200.0f);
            if (!creatures.empty())
                for (auto iter = creatures.begin(); iter != creatures.end(); ++iter)
                    if (!(*iter)->isAlive())
                        (*iter)->Respawn(true);
        }

        void JustDied(Unit* /*who*/)
        {
            if (Creature* metzen = me->FindNearestCreature(NPC_METZEN, 200.0f, true))
                metzen->m_Events.AddEvent(new metzenRunDelayed(metzen), metzen->m_Events.CalculateTime(2000));

            for (uint8 i = 0; i < 9; ++i)
                me->SummonGameObject(209497, posGob[i].GetPositionX(), posGob[i].GetPositionY(), posGob[i].GetPositionZ(), posGob[i].GetOrientation(), 0, 0, 0, 0, 0);

            std::list<Creature*> creatures;
            GetCreatureListWithEntryInGrid(creatures, me, NPC_ROTTEN_LITTLE_HELPER, 200.0f);
            if (!creatures.empty())
            {
                bool first = true;
                for (auto iter = creatures.begin(); iter != creatures.end(); ++iter)
                {
                    if (first)
                    {
                        first = false;
                        (*iter)->MonsterYell("They've killed the Greench!", LANG_UNIVERSAL, 0);
                        (*iter)->GetMotionMaster()->MovePoint(0, 133.650f, -334.167f, 147.087f, true);
                        (*iter)->DespawnOrUnsummon(10000);
                    }
                    else
                    {
                        (*iter)->MonsterYell("Run!", LANG_UNIVERSAL, 0);
                        (*iter)->GetMotionMaster()->MovePoint(0, 218.851f, -393.953f, 154.108f, true);
                        (*iter)->DespawnOrUnsummon(10000);
                    }
                }
            }

            Summons.DespawnAll();
        }

        void EnterCombat(Unit* /*who*/)
        {
            if (Creature* c = me->SummonCreature(NPC_NASTY_LITTLE_HELPER, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ()))
                c->CastCustomSpell(VEHICLE_SPELL_RIDE_HARDCODED, SPELLVALUE_BASE_POINT0, 1, me, TRIGGERED_IGNORE_CASTER_MOUNTED_OR_ON_VEHICLE);
            if (Creature* c = me->SummonCreature(NPC_WICKED_LITTLE_HELPER, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ()))
                c->CastCustomSpell(VEHICLE_SPELL_RIDE_HARDCODED, SPELLVALUE_BASE_POINT0, 2, me, TRIGGERED_IGNORE_CASTER_MOUNTED_OR_ON_VEHICLE);
            events.ScheduleEvent(EVENT_THROW_WINTER_VEIL_TREE, 10000);
            events.ScheduleEvent(EVENT_THROW_STRANGE_SNOWMAN, 3000);
            events.ScheduleEvent(EVENT_CLEAVE, 8000);
            events.ScheduleEvent(EVENT_SHRINK, 5000);
        }

        void JustSummoned(Creature* summoned)
        {
            Summons.Summon(summoned);
            summoned->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
            switch (summoned->GetEntry())
            {
                case NPC_NASTY_LITTLE_HELPER:
                case NPC_WICKED_LITTLE_HELPER:
                    break;
                case NPC_WINTER_TREE:
                    summoned->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE);
                    break;
                case NPC_STRANGE_SNOWMAN:
                    summoned->m_Events.AddEvent(new SnowManEvent(summoned), summoned->m_Events.CalculateTime(3000));
                    break;
                default:
                    break;
            }
        }

        void MovementInform(uint32 type, uint32 pointId)
        {
            if (type != POINT_MOTION_TYPE && type != EFFECT_MOTION_TYPE)
                return;

            if (pointId == 1)
                events.ScheduleEvent(EVENT_ABOMINABLE_CRUSH, 0);
        }

        void UpdateAI(const uint32 diff)
        {
            if (!UpdateVictim())
                return;

            events.Update(diff);

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                    case EVENT_SHRINK:
                        DoCast(SPELL_SHRINK_HEART);
                        events.ScheduleEvent(EVENT_SHRINK, 5000);
                        break;
                    case EVENT_CLEAVE:
                        DoCastVictim(SPELL_CLEAVE);
                        events.ScheduleEvent(EVENT_CLEAVE, 8000);
                        break;
                    case EVENT_THROW_WINTER_VEIL_TREE:
                        if (me->GetVehicleKit())
                            if (Unit *nasty = me->GetVehicleKit()->GetPassenger(0))
                                if (nasty->ToCreature())
                                    nasty->ToCreature()->AI()->DoCastRandom(THROW_WINTER_VEIL, 0.0f, true);
                        events.ScheduleEvent(EVENT_ABO_CHARGE, 5000);
                        events.ScheduleEvent(EVENT_THROW_WINTER_VEIL_TREE, 15000);
                        break;
                    case EVENT_ABO_CHARGE:
                        if (Creature *target = me->FindNearestCreature(NPC_WINTER_TREE, 100.0f, true))
                            me->GetMotionMaster()->MoveCharge(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(), 42, 1);
                        break;
                    case EVENT_ABOMINABLE_CRUSH:
                        DoCast(SPELL_ABOMINABLE_CURSH);
                        break;
                    case EVENT_THROW_STRANGE_SNOWMAN:
                        if (me->GetVehicleKit())
                            if (Unit *nasty = me->GetVehicleKit()->GetPassenger(1))
                                if (nasty->ToCreature())
                                    nasty->ToCreature()->AI()->DoCastRandom(THROW_STRANGE_SNOWMAN, 0.0f, true);
                        events.ScheduleEvent(EVENT_THROW_STRANGE_SNOWMAN, 15000);
                        break;
                }
            }

            DoMeleeAttackIfReady();
        }

    private:
        EventMap events;
    SummonList Summons;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_abo_grinchAI(creature);
    }
};

class npc_druid_t12_balance : public CreatureScript
{
public:
    npc_druid_t12_balance() : CreatureScript("npc_druid_t12_balance") { }

    struct npc_druid_t12_balanceAI : public ScriptedAI
    {
        npc_druid_t12_balanceAI(Creature* creature) : ScriptedAI(creature) { }

        void Reset()
        {
            castTimer = 2000;
        }

        void UpdateAI(const uint32 diff)
        {
            if (castTimer < diff)
            {
                if (Player* owner = me->GetCharmerOrOwnerPlayerOrPlayerItself())
                {
                    if (Unit* target = owner->GetSelectedUnit())
                        me->CastSpell(target, 99026, TRIGGERED_IGNORE_POWER_AND_REAGENT_COST);
                }

                castTimer = 2000;
            }
            else castTimer -= diff;
        }

    private:
        uint32 castTimer;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_druid_t12_balanceAI(creature);
    }
};

class npc_priest_t12_holy : public CreatureScript
{
public:
    npc_priest_t12_holy() : CreatureScript("npc_priest_t12_holy") { }

    struct npc_priest_t12_holyAI : public ScriptedAI
    {
        npc_priest_t12_holyAI(Creature* creature) : ScriptedAI(creature) { }

        void IsSummonedBy(Unit* summoner)
        {
            me->setFaction(summoner->getFaction());
            me->ApplySpellImmune(0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_ALL, true);
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_priest_t12_holyAI(creature);
    }
};

enum infernalSpells
{
    SPELL_PET_PASSIVE_INFERNAL = 96100,
    SPELL_INFERNAL_SCALING_01  = 61013,
    SPELL_INFERNAL_SCALING_02  = 89953
};

class npc_warl_infernal : public CreatureScript
{
public:
    npc_warl_infernal() : CreatureScript("npc_warl_infernal") { }

    struct npc_warl_infernalAI : public ScriptedAI
    {
        npc_warl_infernalAI(Creature* creature) : ScriptedAI(creature) {}

        void IsSummonedBy(Unit* owner)
        {
            if (!me->HasAura(SPELL_WARL_AOE_AVOIDANCE))
                DoCast(me, SPELL_WARL_AOE_AVOIDANCE, true);
            DoCast(me, SPELL_PET_PASSIVE_INFERNAL, true);
            DoCast(me, SPELL_INFERNAL_SCALING_01, true);
            DoCast(me, SPELL_INFERNAL_SCALING_02, true);
            targetCheckTimer = 500;
        }

        void UpdateAI(const uint32 diff)
        {
            if (targetCheckTimer < diff)
            {
                if (Unit* owner = me->GetOwner())
                {
                    targetFound = false;
                    Position casterPos;
                    Unit* closestTarget = NULL;
                    float closestDistance = 40.0f;
                    std::list<Unit*> targets;
                    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(me, me, 40.0f);
                    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(me, targets, u_check);
                    me->VisitNearbyObject(40.0f, searcher);
                    me->GetPosition(&casterPos);
                    for (Unit* target : targets)
                    {
                        Position comparePos;
                        target->GetPosition(&comparePos);
                        float exactDist = casterPos.GetExactDist(&comparePos);
                        if (exactDist < closestDistance && (target->HasAura(SPELL_BANE_OF_DOOM, owner->GetGUID()) || target->HasAura(SPELL_BANE_OF_AGONY, owner->GetGUID())))
                        {
                            closestDistance = exactDist;
                            closestTarget = target;
                            targetFound = true;
                        }
                    }

                    if (!targetFound)
                    {
                        if (Unit* target = owner->getAttackerForHelper())
                            me->AI()->AttackStart(target);
                    }
                    else
                        me->AI()->AttackStart(closestTarget);
                }
                targetCheckTimer = 2000;
            }
            else targetCheckTimer -= diff;

            DoMeleeAttackIfReady();
        }

    private:
        bool targetFound;
        uint32 targetCheckTimer;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_warl_infernalAI(creature);
    }
};

enum TentacleStuff
{
    EVENT_MIND_FLAY = 1,
    SPELL_MIND_FLAY = 52586,
    BASE_DAMAGE_LFR = 9881,
    BASE_DAMAGE_NH  = 11155,
    BASE_DAMAGE_HC  = 12429
};

class npc_tentacle_of_the_old_ones : public CreatureScript
{
public:
    npc_tentacle_of_the_old_ones() : CreatureScript("npc_tentacle_of_the_old_ones") {}

    struct npc_tentacle_of_the_old_onesAI : public ScriptedAI
    {
        npc_tentacle_of_the_old_onesAI(Creature* creature) : ScriptedAI(creature) {}

        void IsSummonedBy(Unit* summoner)
        {
            DoCast(me, 89962, true); // not sniff verified but include all scalings which we need
            DoCast(me, 61783, true); // not sniff verified but include all scalings which we need

            //Because something removes this Aura after this Hook..
            me->CastWithDelay(100, me, 70416, true); // Sniff verified
            me->CastWithDelay(100, me, 109905, true); // Sniff verified
        }

        void Reset()
        {
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE);
            events.ScheduleEvent(EVENT_MIND_FLAY, 100);
        }

        void UpdateAI(uint32 const diff) override
        {
            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                    case EVENT_MIND_FLAY:
                        if (Unit* summoner = me->ToTempSummon()->GetSummoner())
                        {
                            if (Unit* target = summoner->getAttackerForHelper())
                            {
                                int32 baseDamage = BASE_DAMAGE_LFR;
                                if (me->GetEntry() != 58077)
                                    baseDamage = me->GetEntry() == 57220 ? BASE_DAMAGE_NH : BASE_DAMAGE_HC;

                                    if (AuraEffect* aurEff = summoner->GetAuraEffect(77515, EFFECT_0))
                                        AddPct(baseDamage, aurEff->GetAmount());

                                me->CastCustomSpell(target, SPELL_MIND_FLAY, &baseDamage, NULL, NULL, TRIGGERED_NONE);
                                events.ScheduleEvent(EVENT_MIND_FLAY, 3000);
                            }
                            else
                                events.ScheduleEvent(EVENT_MIND_FLAY, 100);
                        }
                        break;
                    default:
                        break;
                }
            }
        }

    private:
        EventMap events;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_tentacle_of_the_old_onesAI(creature);
    }
};

/*######
## npc_argent_squire/gruntling
######*/

enum Misc
{
    STATE_BANK = 0x1,
    STATE_SHOP = 0x2,
    STATE_MAIL = 0x4,

    GOSSIP_ACTION_MAIL = 3,
    ACHI_PONY_UP = 3736,

    SPELL_CHECK_MOUNT = 67039,
    SPELL_CHECK_TIRED = 67334,
    SPELL_SQUIRE_BANK_ERRAND = 67368,
    SPELL_SQUIRE_POSTMAN = 67376,
    SPELL_SQUIRE_SHOP = 67377,
    SPELL_SQUIRE_TIRED = 67401
};
enum Quests
{
    QUEST_CHAMP_ORGRIMMAR = 13726,
    QUEST_CHAMP_UNDERCITY = 13729,
    QUEST_CHAMP_SENJIN = 13727,
    QUEST_CHAMP_SILVERMOON = 13731,
    QUEST_CHAMP_THUNDERBLUFF = 13728,
    QUEST_CHAMP_STORMWIND = 13699,
    QUEST_CHAMP_IRONFORGE = 13713,
    QUEST_CHAMP_GNOMEREGAN = 13723,
    QUEST_CHAMP_DARNASSUS = 13725,
    QUEST_CHAMP_EXODAR = 13724
};
enum Pennants
{
    SPELL_DARNASSUS_PENNANT = 63443,
    SPELL_EXODAR_PENNANT = 63439,
    SPELL_GNOMEREGAN_PENNANT = 63442,
    SPELL_IRONFORGE_PENNANT = 63440,
    SPELL_ORGRIMMAR_PENNANT = 63444,
    SPELL_SENJIN_PENNANT = 63446,
    SPELL_SILVERMOON_PENNANT = 63438,
    SPELL_STORMWIND_PENNANT = 62727,
    SPELL_THUNDERBLUFF_PENNANT = 63445,
    SPELL_UNDERCITY_PENNANT = 63441
};

class npc_argent_squire_gruntling : public CreatureScript
{
public:
    npc_argent_squire_gruntling() : CreatureScript("npc_argent_squire_gruntling") { }

    bool OnGossipHello(Player* pPlayer, Creature* pCreature)
    {
        // Argent Pony Bridle options
        if (pPlayer->HasAchieved(ACHI_PONY_UP))
            if (!pCreature->HasAura(SPELL_SQUIRE_TIRED))
            {
                uint8 uiBuff = (STATE_BANK | STATE_SHOP | STATE_MAIL);
                if (pCreature->HasAura(SPELL_SQUIRE_BANK_ERRAND))
                    uiBuff = STATE_BANK;
                if (pCreature->HasAura(SPELL_SQUIRE_SHOP))
                    uiBuff = STATE_SHOP;
                if (pCreature->HasAura(SPELL_SQUIRE_POSTMAN))
                    uiBuff = STATE_MAIL;

                if (uiBuff & STATE_BANK)
                    pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_MONEY_BAG, "Visit a bank.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_BANK);
                if (uiBuff & STATE_SHOP)
                    pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_VENDOR, "Visit a trader.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_TRADE);
                if (uiBuff & STATE_MAIL)
                    pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Visit a mailbox.", GOSSIP_SENDER_MAIN, 3);
            }

        // Horde
        if (pPlayer->GetQuestRewardStatus(QUEST_CHAMP_SENJIN))
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Darkspear Champion's Pennant", GOSSIP_SENDER_MAIN, SPELL_SENJIN_PENNANT);
        if (pPlayer->GetQuestRewardStatus(QUEST_CHAMP_UNDERCITY))
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Forsaken Champion's Pennant", GOSSIP_SENDER_MAIN, SPELL_UNDERCITY_PENNANT);
        if (pPlayer->GetQuestRewardStatus(QUEST_CHAMP_ORGRIMMAR))
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Orgrimmar Champion's Pennant", GOSSIP_SENDER_MAIN, SPELL_ORGRIMMAR_PENNANT);
        if (pPlayer->GetQuestRewardStatus(QUEST_CHAMP_SILVERMOON))
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Silvermoon Champion's Pennant", GOSSIP_SENDER_MAIN, SPELL_SILVERMOON_PENNANT);
        if (pPlayer->GetQuestRewardStatus(QUEST_CHAMP_THUNDERBLUFF))
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Thunder Bluff Champion's Pennant", GOSSIP_SENDER_MAIN, SPELL_THUNDERBLUFF_PENNANT);

        //Alliance
        if (pPlayer->GetQuestRewardStatus(QUEST_CHAMP_DARNASSUS))
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Darnassus Champion's Pennant", GOSSIP_SENDER_MAIN, SPELL_DARNASSUS_PENNANT);
        if (pPlayer->GetQuestRewardStatus(QUEST_CHAMP_EXODAR))
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Exodar Champion's Pennant", GOSSIP_SENDER_MAIN, SPELL_EXODAR_PENNANT);
        if (pPlayer->GetQuestRewardStatus(QUEST_CHAMP_GNOMEREGAN))
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Gnomeregan Champion's Pennant", GOSSIP_SENDER_MAIN, SPELL_GNOMEREGAN_PENNANT);
        if (pPlayer->GetQuestRewardStatus(QUEST_CHAMP_IRONFORGE))
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Ironforge Champion's Pennant", GOSSIP_SENDER_MAIN, SPELL_IRONFORGE_PENNANT);
        if (pPlayer->GetQuestRewardStatus(QUEST_CHAMP_STORMWIND))
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Stormwind Champion's Pennant", GOSSIP_SENDER_MAIN, SPELL_STORMWIND_PENNANT);

        pPlayer->SEND_GOSSIP_MENU(pPlayer->GetGossipTextId(pCreature), pCreature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction)
    {
        pPlayer->PlayerTalkClass->ClearMenus();
        if (uiAction >= 1000) // remove old pennant aura
            pCreature->AI()->SetData(0, 0);
        switch (uiAction)
        {
        case GOSSIP_ACTION_BANK:
            pCreature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_BANKER);
            pPlayer->GetSession()->SendShowBank(pCreature->GetGUID());
            if (!pCreature->HasAura(SPELL_SQUIRE_BANK_ERRAND))
                pCreature->AddAura(SPELL_SQUIRE_BANK_ERRAND, pCreature);
            if (!pPlayer->HasAura(SPELL_CHECK_TIRED))
                pPlayer->AddAura(SPELL_CHECK_TIRED, pPlayer);
            break;
        case GOSSIP_ACTION_TRADE:
            pCreature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_VENDOR);
            pPlayer->GetSession()->SendListInventory(pCreature->GetGUID());
            if (!pCreature->HasAura(SPELL_SQUIRE_SHOP))
                pCreature->AddAura(SPELL_SQUIRE_SHOP, pCreature);
            if (!pPlayer->HasAura(SPELL_CHECK_TIRED))
                pPlayer->AddAura(SPELL_CHECK_TIRED, pPlayer);
            break;
        case GOSSIP_ACTION_MAIL:
            pCreature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_MAILBOX);
            pPlayer->GetSession()->SendShowMailBox(pCreature->GetGUID());
            if (!pCreature->HasAura(SPELL_SQUIRE_POSTMAN))
                pCreature->AddAura(SPELL_SQUIRE_POSTMAN, pCreature);
            if (!pPlayer->HasAura(SPELL_CHECK_TIRED))
                pPlayer->AddAura(SPELL_CHECK_TIRED, pPlayer);
            break;

        case SPELL_SENJIN_PENNANT:
        case SPELL_UNDERCITY_PENNANT:
        case SPELL_ORGRIMMAR_PENNANT:
        case SPELL_SILVERMOON_PENNANT:
        case SPELL_THUNDERBLUFF_PENNANT:
        case SPELL_DARNASSUS_PENNANT:
        case SPELL_EXODAR_PENNANT:
        case SPELL_GNOMEREGAN_PENNANT:
        case SPELL_IRONFORGE_PENNANT:
        case SPELL_STORMWIND_PENNANT:
            pCreature->AI()->SetData(1, uiAction);
            pPlayer->CLOSE_GOSSIP_MENU();
            break;
        }
        //pPlayer->PlayerTalkClass->CloseGossip();
        return true;
    }

    struct npc_argent_squire_gruntlingAI : public ScriptedAI
    {
        npc_argent_squire_gruntlingAI(Creature* pCreature) : ScriptedAI(pCreature)
        {
            m_current_pennant = 0;
            check_timer = 1000;
        }

        uint32 m_current_pennant;
        uint32 check_timer;

        void UpdateAI(const uint32 uiDiff)
        {
            // have to delay the check otherwise it wont work
            if (check_timer && (check_timer <= uiDiff))
            {
                me->AddAura(SPELL_CHECK_MOUNT, me);
                if (Aura* tired = me->GetOwner()->GetAura(SPELL_CHECK_TIRED))
                {
                    if (!(me->HasAura(SPELL_SQUIRE_BANK_ERRAND) || me->HasAura(SPELL_SQUIRE_SHOP) || me->HasAura(SPELL_SQUIRE_POSTMAN)))
                    {
                        int32 duration = tired->GetDuration();
                        tired = me->AddAura(SPELL_SQUIRE_TIRED, me);
                        tired->SetDuration(duration);
                    }
                }
                check_timer = 1000;
            }
            else check_timer -= uiDiff;

            if (me->HasAura(SPELL_SQUIRE_TIRED) && me->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_BANKER | UNIT_NPC_FLAG_VENDOR | UNIT_NPC_FLAG_MAILBOX))
            {
                me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_BANKER | UNIT_NPC_FLAG_VENDOR | UNIT_NPC_FLAG_MAILBOX);
                me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
            }
        }

        void SetData(uint32 add, uint32 spell)
        {
            if (add)
            {
                me->AddAura(spell, me);
                m_current_pennant = spell;
            }
            else if (m_current_pennant)
            {
                me->RemoveAura(m_current_pennant);
                m_current_pennant = 0;
            }
        }
    };

    CreatureAI *GetAI(Creature *creature) const
    {
        return new npc_argent_squire_gruntlingAI(creature);
    }
};

uint32 const mostraszPathSize = 27;
const G3D::Vector3 mostraszPath[mostraszPathSize] =
{
    { -3074.91f, -3987.97f, 268.289f },
    { -3103.24f, -3902.06f, 302.121f },
    { -3022.64f, -3861.5f,  330.709f },
    { -2808.39f, -3655.46f, 284.67f  },
    { -2778.29f, -3472.36f, 273.78f  },
    { -2753.74f, -3388.52f, 284.0f   },
    { -2731.53f, -3262.98f, 278.67f  },
    { -2576.85f, -3163.74f, 260.26f  },
    { -2411.30f, -3073.74f, 246.61f  },
    { -2227.38f, -2964.84f, 266.50f  },
    { -2109.88f, -2892.31f, 265.40f  },
    { -1957.37f, -2797.25f, 218.9f   },
    { -1703.78f, -2686.17f, 144.21f  },
    { -1550.86f, -2560.88f, 129.23f  },
    { -1479.85f, -2417.8f,  117.11f  },
    { -1398.83f, -2207.11f, 126.38f  },
    { -1346.14f, -2055.27f, 119.66f  },
    { -1326.42f, -1915.46f, 121.89f  },
    { -1201.67f, -1871.35f, 123.55f  },
    { -1002.08f, -1784.49f, 116.24f  },
    { -638.69f,  -1720.77f, 165.99f  },
    { -406.34f,  -1704.62f, 161.60f  },
    { -345.83f,  -1550.08f, 140.95f  },
    { -270.44f,  -1348.41f, 138.84f  },
    { -224.60f,  -1301.72f, 146.29f  },
    { -95.20f,   -1335.12f, 167.58f  },
    { -31.16f,   -1412.79f, 187.94f  },
};

class npc_mostrasz_fly_event : public CreatureScript
{
public:
    npc_mostrasz_fly_event() : CreatureScript("npc_mostrasz_fly_event")
    {}

    struct npc_mostrasz_fly_eventAI : public PassiveAI
    {
        npc_mostrasz_fly_eventAI(Creature* creature) : PassiveAI(creature)
        {}

        enum Mostrasz
        {
            POINT_START = 0,
            POINT_END = 26,

            SPELL_EJECT_ALL_PASSENGERS = 50630,
            SPELL_RIDE_VEHICLE = 108582,
        };

        void IsSummonedBy(Unit* summoner)
        {
            me->SetDisableGravity(true);
            me->SetCanFly(true);
            mounted = false;
            textId = 0;
        }

        void PassengerBoarded(Unit* player, int8 seatId, bool apply)
        {
            if (!apply)
                me->DespawnOrUnsummon(1000);
            else
            {
                me->GetMotionMaster()->MoveSmoothPath(mostraszPath, mostraszPathSize);
                mounted = true;
            }
        }

        void MovementInform(uint32 type, uint32 point)
        {
            if (type == SPLINE_MOTION_TYPE)
            {
                switch (point)
                {
                    case 1:
                    case 3:
                    case 4:
                    case 6:
                    case 8:
                    case 9:
                    case 11:
                    case 14:
                    case 16:
                    case 19:
                    case 20:
                    case 23:
                    case 24:
                        me->AI()->Talk(textId);
                        textId++;
                        break;
                    case POINT_END:
                        me->CastWithDelay(1000, me, SPELL_EJECT_ALL_PASSENGERS, true);
                    break;
                }
            }
        }

        void UpdateAI(const uint32 diff)
        {
            if (!mounted)
            {
                if (Unit* owner = me->GetOwner())
                    owner->CastSpell(me, SPELL_RIDE_VEHICLE, true);
            }
        }

    private:
        int8 textId;
        bool mounted;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_mostrasz_fly_eventAI(creature);
    }
};

// Achievement: The Turkinator
enum WildTurkey
{
    SPELL_TURKEY_TRACKER        = 62014,
};

class npc_wild_turkey : public CreatureScript
{
public:
    npc_wild_turkey() : CreatureScript("npc_wild_turkey") { }

    struct npc_wild_turkeyAI : public ScriptedAI
    {
        npc_wild_turkeyAI(Creature* creature) : ScriptedAI(creature) {}

        void JustDied(Unit* killer)
        {
            if (killer && killer->GetTypeId() == TYPEID_PLAYER)
                killer->CastSpell(killer, SPELL_TURKEY_TRACKER);
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_wild_turkeyAI(creature);
    }
};

// Item: Turkey Caller
enum LonelyTurkey
{
    SPELL_STINKER_BROKEN_HEART  = 62004,
};

class npc_lonely_turkey : public CreatureScript
{
public:
    npc_lonely_turkey() : CreatureScript("npc_lonely_turkey") { }

    struct npc_lonely_turkeyAI : public ScriptedAI
    {
        npc_lonely_turkeyAI(Creature* creature) : ScriptedAI(creature) {}

        void Reset()
        {
            if (me->isSummon())
                if (Unit* owner = me->ToTempSummon()->GetSummoner())
                    me->GetMotionMaster()->MovePoint(0, owner->GetPositionX() + 25 * cos(owner->GetOrientation()), owner->GetPositionY() + 25 * cos(owner->GetOrientation()), owner->GetPositionZ());

            _StinkerBrokenHeartTimer = 3.5 * IN_MILLISECONDS;
        }

        void UpdateAI(uint32 const diff)
        {
            if (_StinkerBrokenHeartTimer <= diff)
            {
                DoCast(SPELL_STINKER_BROKEN_HEART);
                me->setFaction(31);
            }
            _StinkerBrokenHeartTimer -= diff;
        }
    private:
        uint32 _StinkerBrokenHeartTimer;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_lonely_turkeyAI(creature);
    }
};

class npc_crashin_trashin_robot : public CreatureScript
{
public:
    npc_crashin_trashin_robot() : CreatureScript("npc_crashin_trashin_robot") {}

    struct npc_crashin_trashin_robotAI : public ScriptedAI
    {
        npc_crashin_trashin_robotAI(Creature* creature) : ScriptedAI(creature)
        {
        }

        void Reset()
        {
            // The Crashin' Trashin' Robot should despawn after 3 minutes
            me->DespawnOrUnsummon(3 * IN_MILLISECONDS * MINUTE);
        }

        void UpdateAI(const uint32 diff)
        {
        }

    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_crashin_trashin_robotAI(creature);
    }
};

class npc_immune_knockback : public CreatureScript
{
public:
    npc_immune_knockback() : CreatureScript("npc_immune_knockback") { }

    struct npc_immune_knockbackAI : public ScriptedAI
    {
        npc_immune_knockbackAI(Creature* creature) : ScriptedAI(creature) {}

        void UpdateAI(const uint32 diff)
        {
            if (Creature* creature = me->FindNearestCreature(39802, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(41257, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(41374, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(41212, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(41208, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(41371, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(41148, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(41364, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(39440, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(39722, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(48139, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(48141, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(48140, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(48143, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(39370, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(39369, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(39373, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(39366, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(40252, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(39804, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(40251, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(40106, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(40585, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(40170, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(51329, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(40716, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(40550, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(39444, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(40668, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(40715, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(42556, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(40458, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(40033, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(39800, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(39801, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(40311, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(40808, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(40787, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(40450, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(41055, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(39795, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(39721, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(39443, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(40669, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(49941, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(40630, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(40620, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(40310, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(39720, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(39803, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(47136, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(3870, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(47145, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(47132, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(47141, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(47146, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(47138, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(50561, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(47135, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(47140, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(3877, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(3873, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            if (Creature* creature = me->FindNearestCreature(47131, 500.0f, true))
                creature->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);

        }

    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_immune_knockbackAI(creature);
    }
};

//Stone Trogg Pillager
class stone_trogg_pill : public CreatureScript
{
public:
    stone_trogg_pill() : CreatureScript("stone_trogg_pill") { }

    struct stone_trogg_pillAI : public ScriptedAI
    {
        stone_trogg_pillAI(Creature* creature) : ScriptedAI(creature) {}

        void UpdateAI(const uint32 diff)
        {
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK_DEST, true);
        }

    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new stone_trogg_pillAI(creature);
    }
};

//Eye of Kilrogg - Warlock 4277

enum EyeMisc
{
    GLYPH_OF_EYE_OF_KILROGG = 58081,

};

class npc_eye_of_kilrogg : public CreatureScript
{
public:
    npc_eye_of_kilrogg() : CreatureScript("npc_eye_of_kilrogg") {}

    struct npc_eye_of_kilroggAI : public PetAI
    {
        npc_eye_of_kilroggAI(Creature* creature) : PetAI(creature)
        {
            if (Unit* owner = me->GetCharmerOrOwner())
            {

                if (owner->HasAura(GLYPH_OF_EYE_OF_KILROGG))
                {
                    me->SetSpeed(MOVE_WALK, 2.0f);
                    me->SetSpeed(MOVE_RUN, 2.0f);
                    me->SetSpeed(MOVE_FLIGHT, 2.0f);
                    if (owner->GetMap()->GetEntry()->addon > 1)
                        me->SetCanFly(true);
                }
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_eye_of_kilroggAI(creature);
    }
};



#define CUSTOM_TRANSMOG_TELL_ME_MORE "Please tell me what you can do."
#define CUSTOM_TRANSMOG_NO_THANKS "No, thank you."
#define CUSTOM_TRANSMOG_SLOT_EMPTY "The first (top left) slot of your standard 16-slot backpack is empty."
enum Custom_Transmog_Menus
{
    MENU_TRANSMOG_WELCOME = 54004,
    MENU_I_WILL_LET_YOU_KNOW = 54005,
    MENU_DONT_NEED_ME = 54006,
    MENU_ONLY_I_CAN_DO_THIS = 54007,
    MENU_FINAL_CONFIRM = 54008,
};
enum custom_mog_events
{
    EVENT_CHECK_TO_CLEAR_DATA,
};

class npc_custom_transmog : public CreatureScript
{
public:
    npc_custom_transmog() : CreatureScript("npc_custom_transmog") { }
    struct npc_custom_transmogAI : public ScriptedAI
    {
        npc_custom_transmogAI(Creature* creature) : ScriptedAI(creature)
        {

        }
        void InitializeAI() override
        {
            me->SetSheath(SHEATH_STATE_UNARMED);
        }
    };

    bool OnGossipHello(Player* player, Creature* creature)
    {
        if (!npc_me)
            npc_me = creature;

        if (!player || !creature)
            return false;
        if (player->GetDistance(creature) > 8.00f)
            return false;

        creature->PlayDistanceSound(26365/*Transmog NPC Hello*/, player);

        tmog_events.ScheduleEvent(EVENT_CHECK_TO_CLEAR_DATA, 15000);
        //Add a player's selections to the NPC

        if (Active_transmog_users.find(player->GetGUID()) == Active_transmog_users.end())
            Active_transmog_users[player->GetGUID()] = CT;

        Active_transmog_users[player->GetGUID()].isTalkingToNPC = true;
        Active_transmog_users[player->GetGUID()].mogAppearanceItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START);

        if (Active_transmog_users[player->GetGUID()].mogAppearanceItem)
            CheckAppearanceValidity(player);

        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, CUSTOM_TRANSMOG_TELL_ME_MORE, GOSSIP_SENDER_MAIN, 1);
        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, CUSTOM_TRANSMOG_NO_THANKS, GOSSIP_SENDER_MAIN, 2);
        player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());
        return true;
    }
    void RemoveFromGossip(Player* player)
    {
        Active_transmog_users[player->GetGUID()].isTalkingToNPC = false;
        player->PlayerTalkClass->ClearMenus();       
        player->CLOSE_GOSSIP_MENU();
    }
    void PurgeAllMemory()
    {
        Active_transmog_users.clear();
    }
    uint32 getEquipmentSlotForInventoryItem(bool alternate, Player* player)
    {
        if (auto t = Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetTemplate())
        switch (t->InventoryType)
        {
        case INVTYPE_HEAD:              return EQUIPMENT_SLOT_HEAD;                                                 break;
        case INVTYPE_SHOULDERS:         return EQUIPMENT_SLOT_SHOULDERS;                                            break;
        case INVTYPE_BODY:
        case INVTYPE_ROBE:
        case INVTYPE_CHEST:             return EQUIPMENT_SLOT_CHEST;                                                break;
        case INVTYPE_WAIST:             return EQUIPMENT_SLOT_WAIST;                                                break;
        case INVTYPE_LEGS:              return EQUIPMENT_SLOT_LEGS;                                                 break;
        case INVTYPE_FEET:              return EQUIPMENT_SLOT_FEET;                                                 break;
        case INVTYPE_WRISTS:            return EQUIPMENT_SLOT_WRISTS;                                               break;
        case INVTYPE_HANDS:             return EQUIPMENT_SLOT_HANDS;                                                break;
        case INVTYPE_WEAPON:            return (!alternate ? EQUIPMENT_SLOT_MAINHAND : EQUIPMENT_SLOT_OFFHAND);     break;
        case INVTYPE_SHIELD:            return EQUIPMENT_SLOT_OFFHAND;                                              break;
        case INVTYPE_RANGED:            return EQUIPMENT_SLOT_RANGED;                                               break;
        case INVTYPE_CLOAK:             return EQUIPMENT_SLOT_BACK;                                                 break;
        case INVTYPE_2HWEAPON:          return (!alternate ? EQUIPMENT_SLOT_MAINHAND : EQUIPMENT_SLOT_OFFHAND);     break;
        case INVTYPE_WEAPONMAINHAND:    return (!alternate ? EQUIPMENT_SLOT_MAINHAND : EQUIPMENT_SLOT_OFFHAND);     break;
        case INVTYPE_WEAPONOFFHAND:     return (!alternate ? EQUIPMENT_SLOT_OFFHAND : EQUIPMENT_SLOT_MAINHAND);     break;
        case INVTYPE_HOLDABLE:          return EQUIPMENT_SLOT_OFFHAND;                                              break;//huge offhand items
        case INVTYPE_THROWN:            return EQUIPMENT_SLOT_RANGED;                                               break;
        case INVTYPE_RANGEDRIGHT:       
        {
            if (t->SubClass == ITEM_SUBCLASS_WEAPON_WAND)
                return (!alternate ? EQUIPMENT_SLOT_RANGED : EQUIPMENT_SLOT_OFFHAND);
            else
                return EQUIPMENT_SLOT_RANGED;
            break;
        }
        default:
            return NULL;
            break;
        }
        return NULL;
    }
    void CheckAppearanceValidity(Player* player)
    {
        if (!player)
        {
            //TC_LOG_ERROR("sql.sql", "npc_special::CheckAppearanceValidity error - no player");
            return;
        }
        /*
        if (Active_transmog_users[player->GetGUID()])
        {
            TC_LOG_ERROR("sql.sql", "npc_special::CheckAppearanceValidity error - no member of active_transmog_users");
            return;
        }
        */
        switch (Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetTemplate()->InventoryType)
        {
        case INVTYPE_HEAD:
        case INVTYPE_SHOULDERS:
        case INVTYPE_BODY:
        case INVTYPE_CHEST:
        case INVTYPE_WAIST:
        case INVTYPE_LEGS:
        case INVTYPE_FEET:
        case INVTYPE_WRISTS:
        case INVTYPE_HANDS:
        case INVTYPE_WEAPON:
        case INVTYPE_SHIELD:
        case INVTYPE_RANGED:
        case INVTYPE_CLOAK:
        case INVTYPE_2HWEAPON:
        case INVTYPE_ROBE:
        case INVTYPE_WEAPONMAINHAND:
        case INVTYPE_WEAPONOFFHAND:
        case INVTYPE_THROWN:
        case INVTYPE_HOLDABLE:
        case INVTYPE_RANGEDRIGHT:
            Active_transmog_users[player->GetGUID()].AppearanceCanApply = true;
            break;
        default:
            //TC_LOG_ERROR("sql.sql", "invalid item inventory type: %u", Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetTemplate()->InventoryType);
            Active_transmog_users[player->GetGUID()].AppearanceCanApply = false;
            break;
        }
    }
    bool IsPossiblyDualWieldedWeapon(Item* item, Player* player)
    {
        if (!item)
        {
            //TC_LOG_ERROR("sql.sql", "npc_special::IsPossiblyDualWieldedWeapon error - no item input");
            return false;
        }
        else
            if (auto t = item->GetTemplate())
            {
                //TC_LOG_ERROR("sql.sql", "npc_special::IsPossiblyDualWieldedWeapon inventory type: %u", item->GetTemplate()->InventoryType);

                switch (t->InventoryType)
                {
                case INVTYPE_2HWEAPON:
                case INVTYPE_WEAPONMAINHAND:
                case INVTYPE_WEAPONOFFHAND:
                case INVTYPE_WEAPON:
                    //TC_LOG_ERROR("sql.sql", "IsPossiblyDualWieldedWeapon = true");
                    return true;
                    break;
                case INVTYPE_RANGEDRIGHT:
                    if (t->SubClass == ITEM_SUBCLASS_WEAPON_WAND)
                    {
                        //TC_LOG_ERROR("sql.sql", "npc_special::IsPossiblyDualWieldedWeapon returning true for item %u", t->ItemId);
                        return true;
                    }
                    //else TC_LOG_ERROR("sql.sql", "INVTYPE_RANGEDRIGHT subclass: %u", t->SubClass);
                    return false;
                default:
                    //TC_LOG_ERROR("sql.sql", "IsPossiblyDualWieldedWeapon = FALSE");
                    return false;
                    break;
                }
                //TC_LOG_ERROR("sql.sql", "ERROR FALSE AT IsPossiblyDualWieldedWeapon");
            }
        return false;
    }
    enum PNELTH_RULES
    {
        RULE_NULL               = 0,
        RULE_ARMOR              = 1,
        RULE_MELEEWEAPON        = 2,
        RULE_OFFHAND            = 3,
        RULE_THROWN             = 4,
        RULE_RANGED             = 5,      
        RULE_SHIELD             = 6,
        RULE_MAX                = 7,
        TRANSMOG_DEFAULT_MULTIPLIER = 200,

    };

    void PNELTH_CUSTOM_TRANSMOG_RULES(Player* player, Item* stats_item, Item* appearance_item)
    {
        Active_transmog_users[player->GetGUID()].COST_MULTIPLIER = TRANSMOG_DEFAULT_MULTIPLIER;
        if (!stats_item || !appearance_item)
        {
            RemoveFromGossip(player);
            return;
        }

        //Establish which rule this item will follow
        switch (stats_item->GetTemplate()->InventoryType)
        {
        case INVTYPE_HEAD:
        case INVTYPE_SHOULDERS:
        case INVTYPE_BODY:
        case INVTYPE_CHEST:
        case INVTYPE_WAIST:
        case INVTYPE_LEGS:
        case INVTYPE_FEET:
        case INVTYPE_WRISTS:
        case INVTYPE_HANDS:
        case INVTYPE_ROBE:
            Active_transmog_users[player->GetGUID()].PN_RULE = RULE_ARMOR;
            break;
        case INVTYPE_WEAPON://No idea what the hell this is
            Active_transmog_users[player->GetGUID()].PN_RULE = RULE_MELEEWEAPON;
            break;
        case INVTYPE_SHIELD:
            Active_transmog_users[player->GetGUID()].PN_RULE = RULE_SHIELD;
            break;
        case INVTYPE_RANGED:
            Active_transmog_users[player->GetGUID()].PN_RULE = RULE_RANGED;
            break;
        case INVTYPE_CLOAK:
            Active_transmog_users[player->GetGUID()].PN_RULE = RULE_ARMOR;
            break;
        case INVTYPE_2HWEAPON:
        case INVTYPE_WEAPONMAINHAND:
        case INVTYPE_WEAPONOFFHAND:
            Active_transmog_users[player->GetGUID()].PN_RULE = RULE_MELEEWEAPON;
            break;
        case INVTYPE_THROWN:
            Active_transmog_users[player->GetGUID()].PN_RULE = RULE_THROWN;
            break;
        case INVTYPE_RANGEDRIGHT:
            Active_transmog_users[player->GetGUID()].PN_RULE = RULE_RANGED;
            break;
        case INVTYPE_HOLDABLE:
            Active_transmog_users[player->GetGUID()].PN_RULE = RULE_OFFHAND;
            break;
        default:
            break;
        }

        if (Active_transmog_users[player->GetGUID()].PN_RULE)   //null check
        {
            switch (Active_transmog_users[player->GetGUID()].PN_RULE)
            {
            case RULE_ARMOR:
                /*
                        ITEM_SUBCLASS_ARMOR_CLOTH                   = 1,
                        ITEM_SUBCLASS_ARMOR_LEATHER                 = 2,
                        ITEM_SUBCLASS_ARMOR_MAIL                    = 3,
                        ITEM_SUBCLASS_ARMOR_PLATE                   = 4,
                        */
                if (appearance_item->GetTemplate()->SubClass == stats_item->GetTemplate()->SubClass)//plate <-> plate only
                {
                    Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = true;
                }
                else if (appearance_item->GetTemplate()->SubClass < stats_item->GetTemplate()->SubClass)//plate trying to appear as cloth mail or leather
                {
                    //Active_transmog_users[player->GetGUID()].COST_MULTIPLIER = (10*(Active_transmog_users[player->GetGUID()].COST_MULTIPLIER));
                    //Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = true;
                    Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = false;
                }
                else if (appearance_item->GetTemplate()->SubClass > stats_item->GetTemplate()->SubClass)//cloth trying to appear as leather, mail, cloth
                {
                    //Active_transmog_users[player->GetGUID()].COST_MULTIPLIER = (10*(Active_transmog_users[player->GetGUID()].COST_MULTIPLIER));
                    //Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = true;
                    Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = false;
                }
                else
                {
                    Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = false;
                }
                break;
            case RULE_MELEEWEAPON:               
                //appearances we want to block entirely
                switch (appearance_item->GetEntry())
                {
                case 17802://thunderfury
                case 19019://thunderfury
                case 32837://warglaive 32837 = Main Hand
                case 32838://warglaive 32838 = Off Hand
                    Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = false;
                    return;
                    break;
                default:
                    Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = true;
                    break;
                }

                if (stats_item->GetTemplate()->InventoryType != INVTYPE_2HWEAPON)
                    if (appearance_item->GetTemplate()->InventoryType == INVTYPE_2HWEAPON)
                    {
                        //TC_LOG_ERROR("sql.sql", "custom transmog npc - price spike 1 active");
                        //Example of how to allow something but spike the price.
                        //Active_transmog_users[player->GetGUID()].COST_MULTIPLIER = (10*(Active_transmog_users[player->GetGUID()].COST_MULTIPLIER));
                        //Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = true;

                        Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = false;
                        return;
                    }
                if (stats_item->GetTemplate()->InventoryType == INVTYPE_2HWEAPON)
                    if (appearance_item->GetTemplate()->InventoryType != INVTYPE_2HWEAPON)
                    {
                        //TC_LOG_ERROR("sql.sql", "custom transmog npc - price spike 2 active");
                        //Example of how to allow something but spike the price.
                        //Active_transmog_users[player->GetGUID()].COST_MULTIPLIER = (10*(Active_transmog_users[player->GetGUID()].COST_MULTIPLIER));
                        //Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = true;

                        Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = false;
                        return;
                    }

                        /*
                           Setup for rules to which  appearances a 2h item can take
                           current rules:
                           axe, sword, mace, and polearm can interchange
                           polearms can take axe, sword, mace or staves
                           staves can take axe, sword, mace or polearm
                       */
                if (stats_item->GetTemplate()->InventoryType == INVTYPE_2HWEAPON && appearance_item->GetTemplate()->InventoryType == INVTYPE_2HWEAPON)
                {
                    switch (stats_item->GetTemplate()->SubClass)
                    {
                    case ITEM_SUBCLASS_WEAPON_AXE2:
                    case ITEM_SUBCLASS_WEAPON_MACE2:
                        switch (appearance_item->GetTemplate()->SubClass)
                        {
                        case ITEM_SUBCLASS_WEAPON_EXOTIC2:
                        case ITEM_SUBCLASS_WEAPON_STAFF:
                            Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = false;
                            return;
                        default:
                            break;
                        }
                        break;
                    case ITEM_SUBCLASS_WEAPON_POLEARM:
                        switch (appearance_item->GetTemplate()->SubClass)
                        {
                        case ITEM_SUBCLASS_WEAPON_EXOTIC2:
                            Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = false;
                            return;
                        default:
                            break;
                        }
                        break;
                    case ITEM_SUBCLASS_WEAPON_SWORD2:
                        switch (appearance_item->GetTemplate()->SubClass)
                        {
                        case ITEM_SUBCLASS_WEAPON_STAFF:
                        case ITEM_SUBCLASS_WEAPON_EXOTIC2:
                            Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = false;
                            return;
                        default:
                            break;
                        }
                        break;
                    case ITEM_SUBCLASS_WEAPON_EXOTIC2:
                        Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = false;
                        return;
                        break;
                    case ITEM_SUBCLASS_WEAPON_STAFF:
                        switch (appearance_item->GetTemplate()->SubClass)
                        {
                        case ITEM_SUBCLASS_WEAPON_EXOTIC2:
                            Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = false;
                            return;
                        default:
                            break;
                        }
                        break;
                    default:
                        break;
                    }
                }
                Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = true;
                break;
            case RULE_OFFHAND:
                switch (appearance_item->GetEntry())
                {
                case 17802://thunderfury
                case 19019://thunderfury
                case 32837://warglaive 32837 = Main Hand
                case 32838://warglaive 32838 = Off Hand
                    Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = false;
                    return;
                    break;
                default:
                    Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = true;
                    break;
                }

                if (stats_item->GetTemplate()->InventoryType == INVTYPE_2HWEAPON)
                    if (appearance_item->GetTemplate()->InventoryType != INVTYPE_2HWEAPON)
                    {
                        Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = false;
                        return;
                    }

                Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = true;
                break;
            case RULE_THROWN:
                if (appearance_item->GetTemplate()->InventoryType != stats_item->GetTemplate()->InventoryType)//prevent bows and guns
                {
                    Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = false;
                    return;
                }
                Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = true;
                break;
            case RULE_RANGED:
                if (appearance_item->GetTemplate()->InventoryType == INVTYPE_THROWN)
                {
                    Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = false;
                    return;
                }
                Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = true;
                break;
            case RULE_SHIELD:
                Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN = true;
                break;
            default:
                break;
            }
        }
        else
        {//some reasno this is still a null
            RemoveFromGossip(player);
        }

        if ((appearance_item->GetTemplate()->AllowableClass & player->getClassMask()) == 0 || (appearance_item->GetTemplate()->AllowableRace & player->getORaceMask()) == 0)//If the item is for another race/class
             {
                //TC_LOG_ERROR("misc", "other-class item selected");
                Active_transmog_users[player->GetGUID()].COST_MULTIPLIER = (2 * (Active_transmog_users[player->GetGUID()].COST_MULTIPLIER));
            }
    }
    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action)
    {

        //TC_LOG_ERROR("sql.sql", "OnGossipSelect: %u %u %u %u", player->GetGUID(), creature->GetGUID(), sender, action);

        if (!player || !creature)
        {
            //TC_LOG_ERROR("sql.sql", "custom transmog npc - player or creature NULL check");
            return false;
        }

        if (Active_transmog_users.find(player->GetGUID()) == Active_transmog_users.end())
        {
            TC_LOG_ERROR("sql.sql", "custom transmog npc - OnGossipSelect player failed data save NULL check");
            return false;
        }

        if (!Active_transmog_users[player->GetGUID()].isTalkingToNPC)
        {
            //refresh saved data
            Active_transmog_users[player->GetGUID()] = CT;
            Active_transmog_users[player->GetGUID()].isTalkingToNPC = true;
        }
        //TC_LOG_ERROR("misc", "custom transmog action recieved %u", action);
        player->PlayerTalkClass->ClearMenus();
        switch (action)
        {
        case 0:
        {
            if (!npc_me)
                npc_me = creature;

            if (!player || !creature)
                return false;

            if (player->GetDistance(creature) > 8.00f)
                return false;

            player->PlayDirectSound(62540/*Transmog Page Click*/);
            tmog_events.ScheduleEvent(EVENT_CHECK_TO_CLEAR_DATA, 15000);
            //Add a player's selections to the NPC
            Active_transmog_users[player->GetGUID()] = CT;
            Active_transmog_users[player->GetGUID()].mogAppearanceItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START);
            if (Active_transmog_users[player->GetGUID()].mogAppearanceItem)
                CheckAppearanceValidity(player);
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, CUSTOM_TRANSMOG_TELL_ME_MORE, GOSSIP_SENDER_MAIN, 1);
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, CUSTOM_TRANSMOG_NO_THANKS, GOSSIP_SENDER_MAIN, 2);
            player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());
            return true;
            break;
        }
        case 1://Show me what you can do for me
        {
            if (auto mog_item = Active_transmog_users[player->GetGUID()].mogAppearanceItem)
                if (auto templ = mog_item->GetTemplate())
                {
                    player->PlayDirectSound(62540/*Transmog Page Click*/);

                    if (Active_transmog_users[player->GetGUID()].AppearanceCanApply)
                        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, ("I want to apply the appearance of \n\n" + templ->GetItemLink(true, 0, 0, 0, 0, 0, 0, 0) + "\n\nto one of my items."), GOSSIP_SENDER_MAIN, 5);
                    else
                        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, ("Sorry, but " + templ->GetItemLink(true, 0, 0, 0, 0, 0, 0, 0) + " cannot be transmogrified to any item."), GOSSIP_SENDER_MAIN, 4);

                    if (mog_item->HasFlag(ITEM_FIELD_FLAGS, ITEM_FLAG_ENCHANT_HIDDEN))
                        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, ("Please re-enable the enchantment glow of \n\n" + templ->GetItemLink(true, 0, 0, 0, 0, 0, 0, 0) + "\n\n."), GOSSIP_SENDER_MAIN, 14);
                    else
                        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, ("Please disable the enchantment glow of \n\n" + templ->GetItemLink(true, 0, 0, 0, 0, 0, 0, 0) + "\n\n."), GOSSIP_SENDER_MAIN, 12);
                }
                else player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, CUSTOM_TRANSMOG_SLOT_EMPTY, GOSSIP_SENDER_MAIN, 2);
            else player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, CUSTOM_TRANSMOG_SLOT_EMPTY, GOSSIP_SENDER_MAIN, 2);

            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, CUSTOM_TRANSMOG_NO_THANKS, GOSSIP_SENDER_MAIN, 3);
            player->SEND_GOSSIP_MENU(MENU_I_WILL_LET_YOU_KNOW, creature->GetGUID());
            break;
        }
        case 2:
        case 3:
        case 4:
            RemoveFromGossip(player);
            break;
        case 5: 
        {
            //Player has indicated they want to apply the appearance of an item after seeing the name.
        /**                                        ACCESS VIOLATIONS                                         **/
            if (!Active_transmog_users[player->GetGUID()].mogAppearanceItem)
            {//should not be here, item does not exist
                ChatHandler(player->GetSession()).PSendSysMessage(("Transmogrification: access violation, item does not exist."));
                TC_LOG_ERROR("misc", "Transmog access violation 1: player GUID: %u", player->GetGUID());
                RemoveFromGossip(player);
                return false;
            }
            if (!Active_transmog_users[player->GetGUID()].AppearanceCanApply)
            {//Should not be here, Item cannot be equipped
                ChatHandler(player->GetSession()).PSendSysMessage(("Transmogrification: access violation, item cannot be equipped."));
                TC_LOG_ERROR("misc", "Transmog access violation 2: player GUID: %u", player->GetGUID());
                RemoveFromGossip(player);
                return false;
            }
            /**                                        ACCESS VIOLATIONS                                         **/

            /**                                        INITIALIZING VARIABLES                                    **/
            Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats = NULL;
            Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2 = NULL;
            Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_selected = NULL;
            Active_transmog_users[player->GetGUID()].pricetag1 = NULL;
            Active_transmog_users[player->GetGUID()].pricetag2 = NULL;
            Active_transmog_users[player->GetGUID()].cost = NULL;
            Active_transmog_users[player->GetGUID()].block_target_stats_item_1 = false;
            Active_transmog_users[player->GetGUID()].block_target_stats_item_2 = false;

            /**                                        INITIALIZING VARIABLES                                    **/


                /**                                        DEFINING POTENTIAL TARGET ITEMS                           **/
            Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats = player->GetItemByPos(INVENTORY_SLOT_BAG_0, getEquipmentSlotForInventoryItem(false, player));

            //define secondary mog target (offhand if applicable)
            if (IsPossiblyDualWieldedWeapon(Active_transmog_users[player->GetGUID()].mogAppearanceItem, player)//if this item could be applied to the offhand as well
                || Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetTemplate()->SubClass == ITEM_SUBCLASS_WEAPON_WAND)//wands can go into offhand
            {
                Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2 = player->GetItemByPos(INVENTORY_SLOT_BAG_0, getEquipmentSlotForInventoryItem(true, player));
            }

            //At this point, it's possible that we've got a shield in the bag slot and a weapon in the offhand or vice-versa
            if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats)
                if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats->GetTemplate())
                    if (Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetTemplate()->InventoryType == INVTYPE_SHIELD)
                        if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats->GetTemplate()->InventoryType != INVTYPE_SHIELD)
                            Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats = NULL;
            //Item in bag is a shield, but item in mainhand is not a shield.

            if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats)
                if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats->GetTemplate())
                    if (Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetTemplate()->InventoryType != INVTYPE_SHIELD)
                        if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats->GetTemplate()->InventoryType == INVTYPE_SHIELD)
                            Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats = NULL;
            //Item in bag is not a shield, but item in mainhand is a shield (should never happen but I'll add it)


            //At this point, it's possible that we've got a shield in the bag slot and a weapon in the offhand or vice-versa
            if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2)
                if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2->GetTemplate())
                    if (Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetTemplate()->InventoryType == INVTYPE_SHIELD)
                        if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2->GetTemplate()->InventoryType != INVTYPE_SHIELD)
                            Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2 = NULL;

            //Item in bag is a shield, but item in offhand is not.
            if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2)
                if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2->GetTemplate())
                    if (Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetTemplate()->InventoryType != INVTYPE_SHIELD)
                        if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2->GetTemplate()->InventoryType == INVTYPE_SHIELD)
                            Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2 = NULL;
            //Item in offhand is a shield, item in inventory is not.
            /**                                        DEFINING POTENTIAL TARGET ITEMS                           **/



            /**                                        DECISION - ITEM 1 NEEDS THIS NPC                          **/
            if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats)  //null check
                if (Active_transmog_users[player->GetGUID()].mogAppearanceItem->CanBeTransmogrified()
                    && Active_transmog_users[player->GetGUID()].mogAppearanceItem->CanTransmogrify())//Appearance item has properties to blizzlike transmog
                    if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats->CanBeTransmogrified()
                        && Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats->CanTransmogrify())//Stats item 1 has properties to blizzlike transmog
                        if (Active_transmog_users[player->GetGUID()].mogAppearanceItem->CanTransmogrifyItemWithItem(Active_transmog_users[player->GetGUID()].mogAppearanceItem, Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats))
                        {

                            //TC_LOG_ERROR("misc", "All conditions met GUID: %u", player->GetGUID());
                            //Transmog Item 1 can be transmogged with the appearance item normally
                            Active_transmog_users[player->GetGUID()].block_target_stats_item_1 = true;
                        }
                        else
                        {
                            //TC_LOG_ERROR("misc", "NOT All conditions met GUID: %u", player->GetGUID());
                            //Transmog Item 1 needs this npc to transmog
                            Active_transmog_users[player->GetGUID()].block_target_stats_item_1 = false;
                        }
            /**                                        DECISION - ITEM 1 NEEDS THIS NPC                          **/

                /**                                        DECISION - ITEM 2 NEEDS THIS NPC                          **/
            if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2) //null check
                if (Active_transmog_users[player->GetGUID()].mogAppearanceItem->CanBeTransmogrified() && Active_transmog_users[player->GetGUID()].mogAppearanceItem->CanTransmogrify())//Appearance item has properties to blizzlike transmog
                    if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2->CanBeTransmogrified() && Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2->CanTransmogrify())//Stats item 2 has properties to blizzlike transmog
                        if (Active_transmog_users[player->GetGUID()].mogAppearanceItem->CanTransmogrifyItemWithItem(Active_transmog_users[player->GetGUID()].mogAppearanceItem, Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2))
                        {
                            //TC_LOG_ERROR("misc", "2- All conditions met GUID: %u", player->GetGUID());
                            //Transmog Item 2 can be transmogged with the appearance item normally
                            Active_transmog_users[player->GetGUID()].block_target_stats_item_2 = true;
                        }
                        else
                        {
                            //TC_LOG_ERROR("misc", "2- NOT All conditions met GUID: %u", player->GetGUID());
                            //Transmog Item 2 needs this npc to transmog
                            Active_transmog_users[player->GetGUID()].block_target_stats_item_2 = false;
                        }
            /**                                        DECISION - ITEM 2 NEEDS THIS NPC                          **/


            /**                                        SET UP MENU 5                                             **/
            if (Active_transmog_users[player->GetGUID()].block_target_stats_item_1 && Active_transmog_users[player->GetGUID()].block_target_stats_item_2)
            {
                //There is no good reason for the player to use this npc. Either the items are invalid or they can be transmogged with the other NPC.
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, ("Okay, I will go talk to the standard transmogrifier"), GOSSIP_SENDER_MAIN, 10);
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, CUSTOM_TRANSMOG_NO_THANKS, GOSSIP_SENDER_MAIN, 2);
                player->SEND_GOSSIP_MENU(MENU_DONT_NEED_ME, creature->GetGUID());
                return true;
            }

            if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats)
            {
                if (!Active_transmog_users[player->GetGUID()].block_target_stats_item_1)
                {
                    //Item 1 is valid and needs this NPC
                    //Show option for Item 1
                    if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats)                      //null check
                        if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats->GetTemplate())   //null check
                        {
                            PNELTH_CUSTOM_TRANSMOG_RULES(player, Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats, Active_transmog_users[player->GetGUID()].mogAppearanceItem);
                            Active_transmog_users[player->GetGUID()].pricetag1 = int64(Active_transmog_users[player->GetGUID()].COST_MULTIPLIER * ((Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats->GetSpecialPrice()) + (Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetSpecialPrice()) + 1) / 10000);

                            auto equipped_item{ Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats->GetTemplate() };
                            if (player->HasEnoughMoney(Active_transmog_users[player->GetGUID()].pricetag1 * 10000))
                            {
                                player->PlayDirectSound(62540/*Transmog Page Click*/);
                                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, ("Apply this appearance to \n"
                                    + equipped_item->GetItemLink(true, 0, 0, 0, 0, 0, 0, 0)
                                    + (IsPossiblyDualWieldedWeapon(Active_transmog_users[player->GetGUID()].mogAppearanceItem, player)
                                        ? (Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetTemplate()->SubClass == ITEM_SUBCLASS_WEAPON_WAND
                                            ? "(wand).\n\nPrice: "
                                            : "(mainhand).\n\nPrice: ")
                                        : ".\n\nPrice: ")
                                    + std::to_string(Active_transmog_users[player->GetGUID()].pricetag1) //in gold, so will need to be multiplied by 10k once selected
                                    + " gold\n\n"), GOSSIP_SENDER_MAIN, 7);
                            }
                            else
                            {
                                player->PlayDirectSound(62540/*Transmog Page Click*/);
                                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, ("Apply this appearance to \n"
                                    + equipped_item->GetItemLink(true, 0, 0, 0, 0, 0, 0, 0)
                                    + (IsPossiblyDualWieldedWeapon(Active_transmog_users[player->GetGUID()].mogAppearanceItem, player)
                                        ? (Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetTemplate()->SubClass == ITEM_SUBCLASS_WEAPON_WAND
                                            ? "(wand).\n\nPrice: "
                                            : "(mainhand).\n\nPrice: ")
                                        : ".\n\nPrice: ")
                                    + std::to_string(Active_transmog_users[player->GetGUID()].pricetag1) //in gold, so will need to be multiplied by 10k once selected
                                    + " gold (Not enough money)\n\n"), GOSSIP_SENDER_MAIN, 6);
                            }
                        }
                        else
                        {
                            ChatHandler(player->GetSession()).PSendSysMessage(("Error: Item does not exist"));
                            TC_LOG_ERROR("misc", "Transmog error 1: player GUID: %u", player->GetGUID());
                        }
                }
                else
                {
                    auto equipped_item{ Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats->GetTemplate() };
                    player->PlayDirectSound(62540/*Transmog Page Click*/);
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, ("This appearance can be applied to \n\n" + equipped_item->GetItemLink(true, 0, 0, 0, 0, 0, 0, 0) + "(mainhand)\n\nmuch cheaper through the other transmogrifier."), GOSSIP_SENDER_MAIN, 2);
                }
            }

            if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2)
            {
                if (!Active_transmog_users[player->GetGUID()].block_target_stats_item_2)
                {
                    //Item 2 is valid and needs this npc
                    //Show option for Item 2
                    if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2)                //null check
                        if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2->GetTemplate()) //null check
                        {
                            PNELTH_CUSTOM_TRANSMOG_RULES(player, Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2, Active_transmog_users[player->GetGUID()].mogAppearanceItem);
                            Active_transmog_users[player->GetGUID()].pricetag2 = int64(Active_transmog_users[player->GetGUID()].COST_MULTIPLIER * ((Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2->GetSpecialPrice()) + (Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetSpecialPrice()) + 1) / 10000);

                            if (player->HasEnoughMoney(Active_transmog_users[player->GetGUID()].pricetag2 * 10000))
                            {
                                player->PlayDirectSound(62540/*Transmog Page Click*/);
                                auto equipped_item_2{ Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2->GetTemplate() };

                                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, ("Apply this appearance to\n"
                                    + equipped_item_2->GetItemLink(true, 0, 0, 0, 0, 0, 0, 0)
                                    + (IsPossiblyDualWieldedWeapon(Active_transmog_users[player->GetGUID()].mogAppearanceItem, player)
                                        ? "(offhand).\n\nPrice: "
                                        : ".\n\nPrice: ")
                                    + std::to_string(Active_transmog_users[player->GetGUID()].pricetag2) //in gold, so will need to be multiplied by 10k once selected
                                    + " gold\n\n"), GOSSIP_SENDER_MAIN, 8);
                            }
                            else
                            {
                                auto equipped_item_2{ Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2->GetTemplate() };
                                player->PlayDirectSound(62540/*Transmog Page Click*/);
                                Active_transmog_users[player->GetGUID()].pricetag2 = int64(Active_transmog_users[player->GetGUID()].COST_MULTIPLIER
                                    * ((Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2->GetSpecialPrice()) + (Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetSpecialPrice()) + 1) / 10000);
                                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, ("Apply this appearance to\n"
                                    + equipped_item_2->GetItemLink(true, 0, 0, 0, 0, 0, 0, 0)
                                    + (IsPossiblyDualWieldedWeapon(Active_transmog_users[player->GetGUID()].mogAppearanceItem, player)
                                        ? "(offhand).\n\nPrice: "
                                        : ".\n\nPrice: ")
                                    + std::to_string(Active_transmog_users[player->GetGUID()].pricetag2) //in gold, so will need to be multiplied by 10k once selected
                                    + " gold (Not enough money)\n\n"), GOSSIP_SENDER_MAIN, 6);
                            }
                        }
                        else
                        {
                            ChatHandler(player->GetSession()).PSendSysMessage(("Error 2: Item does not exist"));
                            TC_LOG_ERROR("misc", "Transmog error 2: player GUID: %u", player->GetGUID());
                        }
                }
                else
                {
                    auto equipped_item_2{ Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2->GetTemplate() };
                    player->PlayDirectSound(62540/*Transmog Page Click*/);
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, ("This appearance can be applied to \n\n"
                        + equipped_item_2->GetItemLink(true, 0, 0, 0, 0, 0, 0, 0)
                        + "(Offhand)\n\nmuch cheaper through the other transmogrifier."), GOSSIP_SENDER_MAIN, 3);
                }
            }
            if (!Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats && !Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2)
            {
                ChatHandler(player->GetSession()).PSendSysMessage(("Transmogrification: you do not have any items equipped for that appearance."));
                RemoveFromGossip(player);
            }
            else
            {
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, CUSTOM_TRANSMOG_NO_THANKS, GOSSIP_SENDER_MAIN, 2);
                player->SEND_GOSSIP_MENU(MENU_ONLY_I_CAN_DO_THIS, creature->GetGUID());
            }
            /**                                        END MENU 5                                             **/
            break;
        }

        case 6:
        {
            //removal due to missing gold
            ChatHandler(player->GetSession()).PSendSysMessage(("Transmogrification failed: you cannot afford that."));
            RemoveFromGossip(player);
            break;
        }
            /*
            Cases 7 and 8 are supposed to be primary and secondary transmog slot options if the appearance item is a weapon and player has 2 weapons equipped.
            */
        case 7:
        {
            if (Active_transmog_users[player->GetGUID()].pricetag1 != NULL)
                if (Active_transmog_users[player->GetGUID()].mogAppearanceItem != NULL)
                    if (Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetTemplate() != NULL)
                        if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats != NULL)
                            if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats->GetTemplate() != NULL)
                            {
                                //target item selected
                                Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_selected = Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats;
                                //target price selected
                                PNELTH_CUSTOM_TRANSMOG_RULES(player, Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_selected, Active_transmog_users[player->GetGUID()].mogAppearanceItem);
                                if (Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN)
                                {
                                    player->PlayDirectSound(62540/*Transmog Page Click*/);
                                    auto transmog_template{ Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetTemplate() };
                                    auto selected_template{ Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_selected->GetTemplate() };

                                    Active_transmog_users[player->GetGUID()].cost = (Active_transmog_users[player->GetGUID()].pricetag1) * 10000;
                                    //TC_LOG_ERROR("sql.sql", "7 - CLEARED NULL CHECKS");
                                    std::ostringstream confirmation{ "" };
                                    confirmation << ("CONFIRMATION:\n\nI would like to apply the appearance of \n\n"
                                        + transmog_template->GetItemLink(true, 0, 0, 0, 0, 0, 0, 0)
                                        + "\n(Backpack slot 1)\n\nto\n\n"
                                        + selected_template->GetItemLink(true, 0, 0, 0, 0, 0, 0, 0)
                                        + "\n(Equipped).\n\nPrice: "
                                        + std::to_string(Active_transmog_users[player->GetGUID()].pricetag1) //in gold, so will need to be multiplied by 10k once selected
                                        + " gold\n\n");
                                    auto conf{ confirmation.str() };


                                    player->ADD_GOSSIP_ITEM_EXTENDED(-1, conf, GOSSIP_SENDER_MAIN, 11, conf, Active_transmog_users[player->GetGUID()].cost, false);

                                    //TC_LOG_ERROR("sql.sql", "7 - CLEARED STRINGS");
                                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, CUSTOM_TRANSMOG_NO_THANKS, GOSSIP_SENDER_MAIN, 2);
                                    player->SEND_GOSSIP_MENU(MENU_FINAL_CONFIRM, creature->GetGUID());
                                }
                                else
                                {
                                    ChatHandler(player->GetSession()).PSendSysMessage(("Transmogrification failed: category rules."));
                                    RemoveFromGossip(player);
                                }
                            }
                            else
                            {
                                //TC_LOG_ERROR("sql.sql", "7 - FAILED TO CLEAR NULL CHECKS");
                            }
            break;
        }
        case 8:
        {
            if (Active_transmog_users[player->GetGUID()].pricetag2 != NULL)
                if (Active_transmog_users[player->GetGUID()].mogAppearanceItem != NULL)
                    if (Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetTemplate() != NULL)
                        if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2 != NULL)
                            if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2->GetTemplate() != NULL)
                            {
                                //target item selected
                                Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_selected = Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_2;


                                PNELTH_CUSTOM_TRANSMOG_RULES(player, Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_selected, Active_transmog_users[player->GetGUID()].mogAppearanceItem);
                                if (Active_transmog_users[player->GetGUID()].mog_ALLOWED_BY_PN)
                                {
                                    player->PlayDirectSound(62540/*Transmog Page Click*/);
                                    //target price selected
                                    Active_transmog_users[player->GetGUID()].cost = (Active_transmog_users[player->GetGUID()].pricetag2) * 10000;
                                    //TC_LOG_ERROR("sql.sql", "8 - CLEARED NULL CHECKS");
                                    auto transmog_template{ Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetTemplate() };
                                    auto stats_template{ Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_selected->GetTemplate() };

                                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, ("CONFIRMATION:\n\nI would like to apply the appearance of \n\n"
                                        + transmog_template->GetItemLink(true, 0, 0, 0, 0, 0, 0, 0)
                                        + "\n(Backpack slot 1)\n\nto\n\n"
                                        + stats_template->GetItemLink(true, 0, 0, 0, 0, 0, 0, 0)
                                        + "\n(Equipped).\n\nPrice: "
                                        + std::to_string(Active_transmog_users[player->GetGUID()].pricetag2) //in gold, so will need to be multiplied by 10k once selected
                                        + " gold\n\n"), GOSSIP_SENDER_MAIN, 11);
                                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, CUSTOM_TRANSMOG_NO_THANKS, GOSSIP_SENDER_MAIN, 2);
                                    player->SEND_GOSSIP_MENU(MENU_FINAL_CONFIRM, creature->GetGUID());
                                }
                                else
                                {
                                    ChatHandler(player->GetSession()).PSendSysMessage(("Transmogrification failed: category rules."));
                                    RemoveFromGossip(player);
                                }
                            }
            //else    TC_LOG_ERROR("sql.sql", "8 - FAILED TO CLEAR NULL CHECKS");
            break;
        }
        case 9:
        case 10:
             RemoveFromGossip(player);
            break;
        case 11://FINAL CONFIRMATION
        {
            if (Active_transmog_users[player->GetGUID()].cost)
                if (Active_transmog_users[player->GetGUID()].mogAppearanceItem)
                    if (Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetTemplate())
                        if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_selected)
                            if (Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_selected->GetTemplate())
                            {
                                if (!player->HasEnoughMoney(Active_transmog_users[player->GetGUID()].cost))
                                {
                                    ChatHandler(player->GetSession()).PSendSysMessage(("Transmogrification failed: you cannot afford that."));
                                    RemoveFromGossip(player);
                                    return false;
                                }
                                player->ModifyMoney(-Active_transmog_users[player->GetGUID()].cost);

                                Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_selected->SetEnchantment(TRANSMOGRIFY_ENCHANTMENT_SLOT, Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetEntry(), 0, 0);
                                player->SetVisibleItemSlot(Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_selected->GetSlot(), Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_selected);

                                Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_selected->UpdatePlayedTime(player);

                                Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_selected->SetOwnerGUID(player->GetGUID());
                                Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_selected->SetNotRefundable(player);
                                Active_transmog_users[player->GetGUID()].mog_equipped_item_with_stats_selected->ClearSoulboundTradeable(player);

                                if (Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetTemplate()->Bonding == BIND_WHEN_EQUIPED || Active_transmog_users[player->GetGUID()].mogAppearanceItem->GetTemplate()->Bonding == BIND_WHEN_USE)
                                    Active_transmog_users[player->GetGUID()].mogAppearanceItem->SetBinding(true);

                                Active_transmog_users[player->GetGUID()].mogAppearanceItem->SetOwnerGUID(player->GetGUID());
                                Active_transmog_users[player->GetGUID()].mogAppearanceItem->SetNotRefundable(player);
                                Active_transmog_users[player->GetGUID()].mogAppearanceItem->ClearSoulboundTradeable(player);

                                ChatHandler(player->GetSession()).PSendSysMessage("Transmogrification successful, %s deducted.", moneyToString(Active_transmog_users[player->GetGUID()].cost));

                                player->PlayDirectSound(25714/*Transmog Apply*/);
                                RemoveFromGossip(player);
                            }
                            else
                            {
                                ChatHandler(player->GetSession()).PSendSysMessage(("Transmogrification failed: internal error 1"));
                                RemoveFromGossip(player);
                            }
                        else
                        {
                            ChatHandler(player->GetSession()).PSendSysMessage(("Transmogrification failed: internal error 2"));
                            RemoveFromGossip(player);
                        }
                    else
                    {
                        ChatHandler(player->GetSession()).PSendSysMessage(("Transmogrification failed: internal error 3"));
                        RemoveFromGossip(player);
                    }
                else
                {
                    ChatHandler(player->GetSession()).PSendSysMessage(("Transmogrification failed: internal error 4"));
                    RemoveFromGossip(player);
                }
            else
            {
                ChatHandler(player->GetSession()).PSendSysMessage(("Transmogrification failed: internal error 5"));
                RemoveFromGossip(player);
            }
            break;
        }
        case 12:
        {
            if (auto mog_item = Active_transmog_users[player->GetGUID()].mogAppearanceItem)
            if (auto templ = mog_item->GetTemplate())
            {
                std::ostringstream option{ "" };
                int64 gold_cost{ 500 };

                option << templ->GetItemLink(true, 0, 0, 0, 0, 0, 0, 0) << "\n\nHide weapon enchantment" << "\n";

                player->ADD_GOSSIP_ITEM_EXTENDED(-1, option.str(), GOSSIP_SENDER_MAIN, 13, option.str(), gold_cost*10000, false);

                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, CUSTOM_TRANSMOG_NO_THANKS, GOSSIP_SENDER_MAIN, 3);
                player->SEND_GOSSIP_MENU(MENU_I_WILL_LET_YOU_KNOW, creature->GetGUID());
            }
            else RemoveFromGossip(player);
            else RemoveFromGossip(player);
            break;
        }
        case 13://adding the flag
        {

            int64 gold_cost{ 500 };
            if (!player->HasEnoughMoney(gold_cost*10000))
            {
                ChatHandler(player->GetSession()).PSendSysMessage(("Echantment modification failed: you cannot afford that."));
                RemoveFromGossip(player);
                return false;
            }

            if (auto enchanted_item = Active_transmog_users[player->GetGUID()].mogAppearanceItem)
            {
                if (enchanted_item->HasFlag(ITEM_FIELD_FLAGS, ITEM_FLAG_ENCHANT_HIDDEN))
                {
                    //the flag is already here.
                    player->PlayDirectSound(25714/*Transmog Apply*/);
                    ChatHandler(player->GetSession()).PSendSysMessage(("Transmogrification failed: internal error 6"));
                    RemoveFromGossip(player);
                }
                else
                {
                    player->PlayDirectSound(25714/*Transmog Apply*/);
                    RemoveFromGossip(player);
                    player->ModifyMoney(-(gold_cost * 10000));

                    enchanted_item->SetFlag(ITEM_FIELD_FLAGS, ITEM_FLAG_ENCHANT_HIDDEN);

                    std::ostringstream option{ "" };
                    option << "Echantment adjustment success: " << enchanted_item->GetTemplate()->GetItemLink(false, 0, 0, 0, 0, 0, 0, 0) << " will no longer glow with enchantments.";
                    auto msg{ option.str() };

                    ChatHandler(player->GetSession()).SendSysMessage(msg.c_str());
                }
            }
            else
                RemoveFromGossip(player);
            break;
        }
        case 14:
        {
            if (auto mog_item = Active_transmog_users[player->GetGUID()].mogAppearanceItem)
                if (auto templ = mog_item->GetTemplate())
                {
                    std::ostringstream option{ "" };
                    int32 gold_cost{ 0 };

                    option << templ->GetItemLink(true, 0, 0, 0, 0, 0, 0, 0) << "\n\n Re-Enable weapon enchantment glow" << "\n(Free)";

                    player->ADD_GOSSIP_ITEM_EXTENDED(-1, option.str(), GOSSIP_SENDER_MAIN, 15, option.str(), gold_cost * 10000, false);

                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, CUSTOM_TRANSMOG_NO_THANKS, GOSSIP_SENDER_MAIN, 3);
                    player->SEND_GOSSIP_MENU(MENU_I_WILL_LET_YOU_KNOW, creature->GetGUID());
                }
                else RemoveFromGossip(player);
            else RemoveFromGossip(player);
            break;
        }

        case 15://Removing the flag
        {
            if (auto enchanted_item = Active_transmog_users[player->GetGUID()].mogAppearanceItem)
            {
                if (enchanted_item->HasFlag(ITEM_FIELD_FLAGS, ITEM_FLAG_ENCHANT_HIDDEN))
                {
                    enchanted_item->RemoveFlag(ITEM_FIELD_FLAGS, ITEM_FLAG_ENCHANT_HIDDEN);
                    //the flag is already here.
                    player->PlayDirectSound(25714/*Transmog Apply*/);
                    std::ostringstream option{ "" };
                    option << "Echantment adjustment success: " << enchanted_item->GetTemplate()->GetItemLink(false, 0, 0, 0, 0, 0, 0, 0) << " will now glow with enchantments.";
                    auto msg{ option.str() };

                    ChatHandler(player->GetSession()).SendSysMessage(msg.c_str());
                    RemoveFromGossip(player);
                }
                else
                {
                    player->PlayDirectSound(25714/*Transmog Apply*/);
                    RemoveFromGossip(player);
                    player->PlayDirectSound(25714/*Transmog Apply*/);
                    ChatHandler(player->GetSession()).PSendSysMessage(("Transmogrification failed: internal error 7"));
                    RemoveFromGossip(player);
                }
            }
            else
                RemoveFromGossip(player);
            break;
        }
        default:
            break;
        }
        return true;
    }
    bool OnGossipSelectCode(Player* player, Creature* creature, uint32 sender, uint32 action, const char* code)
    {
        //TC_LOG_ERROR("sql.sql", "OnGossipSelectCode %u %u %u %u %s", player->GetGUID(), creature->GetGUID(), sender, action, std::string(code));
        return false;
    }
    void UpdateAI(uint32 const diff)
    {
        tmog_events.Update(diff);
        while (uint32 eventId = tmog_events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_CHECK_TO_CLEAR_DATA:
                if (!Active_transmog_users.empty())
                    if (npc_me)
                        if (!npc_me->FindNearestPlayer(20.0f))
                        {
                            //TC_LOG_ERROR("sql.sql", "No nearby players, safe to purge data.");
                            PurgeAllMemory();
                        }
                        else
                        {
                            //TC_LOG_ERROR("sql.sql", "Players are nearby, delaying.");
                        }
                tmog_events.ScheduleEvent(EVENT_CHECK_TO_CLEAR_DATA, 15000);
                break;
            default:
                break;
            }
        }
    }
public:
    Creature* npc_me;
    EventMap tmog_events;
private:
    struct CustomTransmogData_per_player
    {
        int64 COST_MULTIPLIER                           = TRANSMOG_DEFAULT_MULTIPLIER;
        bool isTalkingToNPC                             = false;
        int64 pricetag1                                 = NULL;
        int64 pricetag2                                 = NULL;
        int64 cost                                      = NULL;
        Item* mog_equipped_item_with_stats              = NULL;
        Item* mog_equipped_item_with_stats_2            = NULL;
        Item* mog_equipped_item_with_stats_selected     = NULL;
        Item* mogAppearanceItem                         = NULL;
        bool AppearanceCanApply                         = false;
        bool block_target_stats_item_1                  = false;
        bool block_target_stats_item_2                  = false;
        int8 PN_RULE                                    = NULL;
        bool mog_ALLOWED_BY_PN                          = false;
    };
    typedef std::map<uint64, CustomTransmogData_per_player> CustomTransmogUsers_map;

    CustomTransmogUsers_map Active_transmog_users;
    CustomTransmogData_per_player CT;
    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_custom_transmogAI(creature);
    }
};

class npc_guild_herald : public CreatureScript
{
public:
    npc_guild_herald() : CreatureScript("npc_guild_herald") {}

    struct npc_guild_heraldAI : public PassiveAI
    {
        npc_guild_heraldAI(Creature* creature) : PassiveAI(creature) {}

        void InitializeAI()
        {
            if (auto p = me->GetOwner())
                if (auto player = p->ToPlayer())
                {
                    if (auto guild = player->GetGuildId())
                    {
                        me->SetUInt64Value(OBJECT_FIELD_DATA, MAKE_NEW_GUID(guild, 0, HIGHGUID_GUILD));
                        me->SetUInt16Value(OBJECT_FIELD_TYPE, 1, guild != 0);
                    }
                    me->SetLevel(player->getLevel());
                }
            me->SetReactState(REACT_PASSIVE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_ATTACKABLE_1);
        }

    private:
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_guild_heraldAI(creature);
    }
};

class npc_hidden_egg : public CreatureScript
{
public:
    npc_hidden_egg() : CreatureScript("npc_hidden_egg") { }


    struct npc_hidden_eggAI : public ScriptedAI
    {
        npc_hidden_eggAI(Creature* creature) : ScriptedAI(creature)
        {
        }

        enum
        {
            EVENT_PLAYER_DISTANCE_CHECK = 1,
        };


        void UpdateAI(uint32 const diff)
        {
            if (!events.HasEvent(EVENT_PLAYER_DISTANCE_CHECK))
                events.ScheduleEvent(EVENT_PLAYER_DISTANCE_CHECK, 500);
            events.Update(diff);
            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                case EVENT_PLAYER_DISTANCE_CHECK:
                {
                    if (!events.HasEvent(EVENT_PLAYER_DISTANCE_CHECK))
                        events.ScheduleEvent(EVENT_PLAYER_DISTANCE_CHECK, 500);
                    auto list_of_players = me->GetPlayersInRange(7.f, true);
                    if (list_of_players.size())
                        for (auto itr = list_of_players.begin(); itr != list_of_players.end();)
                            if (auto t = (*itr))
                                if (me->GetExactDist(t) > 6.25f)//filter condition
                                {
                                    list_of_players.erase(itr++);
                                }
                                else
                                {
                                    ++itr;
                                }
                    if (me->IsVisible())
                    {
                        if (!list_of_players.size())
                            me->SetVisible(false);
                    }
                    else
                    {
                        if (list_of_players.size())
                            me->SetVisible(true);
                    }
                }
                    break;
                }
            }
        }

    private:
        EventMap events;

    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_hidden_eggAI(creature);
    }
};

class npc_Unscripted_Combat_Bug_Prevention : public CreatureScript
{
public:
    npc_Unscripted_Combat_Bug_Prevention() : CreatureScript("npc_Unscripted_Combat_Bug_Prevention") { }


    struct npc_Unscripted_Combat_Bug_PreventionAI : public ScriptedAI
    {
        npc_Unscripted_Combat_Bug_PreventionAI(Creature* creature) : ScriptedAI(creature)
        {
        }

        enum unnamed
        {
            EVENT_WIPE_CHECK = 1,
        };

        void Reset()
        {
            events.Reset();
            me->DeleteThreatList();
        }

        void EnterCombat(Unit* /*who*/)
        {
            events.Reset();
            events.ScheduleEvent(EVENT_WIPE_CHECK, 5000);
        }


        void JustDied(Unit* /*Killer*/)
        {
            me->DeleteThreatList();
            me->CombatStopWithPets();
        }

        bool checkWipe()
        {
            auto players = me->GetPlayersInRange(100.f, true);

            if (players.size())
            for (Player* player : players)
                if (!player->HasAuraType(SPELL_AURA_FEIGN_DEATH) && !player->HasAuraType(SPELL_AURA_MOD_STEALTH) && !player->HasAuraType(SPELL_AURA_MOD_INVISIBILITY))
                    return false;
            return true;
        }

        void UpdateAI(uint32 const diff)
        {
            events.Update(diff);
            while (uint32 eventId = events.ExecuteEvent())
                switch (eventId)
                {
                    case EVENT_WIPE_CHECK:
                        if (checkWipe())
                        {
                            me->DeleteThreatList();
                            me->CombatStop(true);
                            Reset();
                            if (me->isAlive())
                            {
                                me->Respawn(true);
                            }
                        }
                        else
                            events.ScheduleEvent(EVENT_WIPE_CHECK, 5000);
                    break;
                    default:
                        break;
                }
        }

    private:
        EventMap events;

    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_Unscripted_Combat_Bug_PreventionAI(creature);
    }
};

class mob_arena_tornado : public CreatureScript
{
public:
    mob_arena_tornado() : CreatureScript("mob_arena_tornado") { }

    struct mob_arena_tornadoAI : public VehicleAI
    {
        mob_arena_tornadoAI(Creature* creature) : VehicleAI(creature)
        {
            //TC_LOG_ERROR("sql.sql", "Summoned storming npc");
        }

        void Reset()
        {
            events.Reset();
            me->AddAura(88922, me);
            events.ScheduleEvent(1, 3500);

            events.ScheduleEvent(5, 120000);
        }
        void IsSummonedBy(Unit* summoner)
        {
            revolve = (*summoner);
        }

        void MovementInform(uint32 type, uint32 id)
        {
            if (type == POINT_MOTION_TYPE)
                if (id == AFFIX_STORMING)
                    events.ScheduleEvent(2, 0);
        }

        void UpdateAI(uint32 diff)
        {
            events.Update(diff);
            while (uint32 eventId = events.ExecuteEvent())
                switch (eventId)
                {
                case 1:
                {
                    me->AddAura(96665, me);
                    Position pos(*me);
                    auto p = me->FindNearestPlayer(100.f);
                    me->MoveBlink(pos, 4.f, p ? me->GetAngle(p) : frand(0.f, M_PI * 2.f));
                    me->GetMotionMaster()->MovePoint(AFFIX_STORMING, pos);
                    break;
                }
                case 2:
                    me->GetMotionMaster()->MoveAroundPoint(me->GetHomePosition(), me->GetDistance(me->GetHomePosition()), bool(urand(0, 1)), 32, me->GetPositionZ(), true);
                    events.ScheduleEvent(3, 1);
                    me->SetDisableGravity(true);
                    me->SetCanFly(true);

                    me->RemoveAurasDueToSpell(88922);
                    break;
                case 3:
                {
                    auto list_of_players = me->GetPlayersInRange(2.f, true);
                    if (list_of_players.size())
                        for (auto current_target_within_list : list_of_players)
                        {
                            CustomSpellValues values;
                            int32 damage{ int32(float(current_target_within_list->GetMaxHealth()) * .15f) };
                            values.AddSpellMod(SPELLVALUE_BASE_POINT1, damage);
                            values.AddSpellMod(SPELLVALUE_BASE_POINT0, 180);
                            me->CastCustomSpell(61663, values, current_target_within_list, TRIGGERED_FULL_MASK, NULL, NULL, me->GetGUID());
                        }
                    events.ScheduleEvent(3, 300);
                    break;
                }
                case 5:
                    me->SetPhaseMask(0x2, true);
                    events.ScheduleEvent(6, 25000);
                    break;
                case 6:
                    me->SetPhaseMask(0x1, true);
                    events.ScheduleEvent(5, 25000);
                    break;
                default:
                    break;
                }
        }

    private:
        Position revolve{ 0.f, 0.f, 0.f, 0.f };
        InstanceScript* instance{ me->GetInstanceScript() };
        EventMap events;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new mob_arena_tornadoAI(creature);
    }
};

class npc_account_manager : public CreatureScript
{
public:
    npc_account_manager() : CreatureScript("npc_account_manager") { }

    bool OnGossipHello(Player* player, Creature* creature)
    {
        player->PlayerTalkClass->ClearMenus();
        if (auto s = player->GetSession())
        {
            if (s->premium_HasPerk(ACCOUNT_PERK_PROJECT_CONTRIBUTOR))
                player->ADD_GOSSIP_ITEM(-1, "|TInterface\\icons\\INV_Misc_GroupLooking:20:20:-10:0|tCollect contributor tabard", GOSSIP_SENDER_MAIN, 7);
            if (s->premium_HasPerk(ACCOUNT_PERK_EARLY_OPEN_BETA_TESTER))
                player->ADD_GOSSIP_ITEM(-1, "|TInterface\\icons\\INV_Misc_GroupLooking:20:20:-10:0|tCollect early tester reward", GOSSIP_SENDER_MAIN, 8);

            if (s->premium_HasPerk(ACCOUNT_PERK_50_CHARACTERS))
                player->ADD_GOSSIP_ITEM(-1, "|TInterface\\icons\\INV_Misc_GroupLooking:20:20:-10:0|tAdd excess characters", GOSSIP_SENDER_MAIN, 6);

            player->SEND_GOSSIP_MENU(DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
            player->ADD_GOSSIP_ITEM(-1, "|TInterface\\icons\\misc_arrowleft:20:20:-10:0|tExit", GOSSIP_SENDER_MAIN, 1);
        }
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action)
    {
        player->PlayerTalkClass->ClearMenus();

        if (sender == GOSSIP_SENDER_MAIN)
            switch (action)
            {
            case 1:
                player->CLOSE_GOSSIP_MENU();
                break;
            case 6:
                if (auto s = player->GetSession())
                    if (s->premium_HasPerk(ACCOUNT_PERK_50_CHARACTERS))
                    {
                        s->SetRequestingNewCharacter(true);
                        creature->MonsterWhisper("Logout to create a new character.", player->GetGUID(), true);
                        //creature->MonsterWhisper("Some characters may appear missing when you log out. They will reappear after your character has been created or you log out.", player->GetGUID(), false);
                    }
                player->CLOSE_GOSSIP_MENU();
                break;
            case 7:
                if (auto s = player->GetSession())
                    if (s->premium_HasPerk(ACCOUNT_PERK_PROJECT_CONTRIBUTOR))
                    {
                        if (!player->HasItemCount(100006/*Tabard of the old gods*/, 1, true))
                        {

                            player->AddItem(100006/*Tabard of the old gods*/, 1);
                            creature->MonsterWhisper("Thank you for your contribution.", player->GetGUID(), false);
                        }
                        else
                        {
                            creature->MonsterWhisper("You already have a tabard in your inventory or bank storage.", player->GetGUID(), false);
                        }
                    }
                player->CLOSE_GOSSIP_MENU();
                break;
            case 8:
                if (auto s = player->GetSession())
                    if (s->premium_HasPerk(ACCOUNT_PERK_EARLY_OPEN_BETA_TESTER))
                    {
                        /*
                        if (!player->HasItemCount(100006Tabard of the old gods, 1, true))
                        {

                            player->AddItem(100006, 1);
                            creature->MonsterWhisper("Thank you for your contribution.", player->GetGUID(), false);
                        }
                        else
                        {
                            creature->MonsterWhisper("You already have a tabard in your inventory or bank storage.", player->GetGUID(), false);
                        }
                        */
                    }
                player->CLOSE_GOSSIP_MENU();
                break;
            default:
                break;
            }

        return true;
    }
};

void AddSC_npcs_special()
{
    new npc_custom_transmog();
    new npc_air_force_bots();
    new npc_lunaclaw_spirit();
    new npc_chicken_cluck();
    new npc_dancing_flames();
    new npc_doctor();
    new npc_injured_patient();
    new npc_garments_of_quests();
    new npc_guardian();
    new npc_mount_vendor();
    new npc_rogue_trainer();
    new npc_Reckful_trainer();
    new npc_battle_standard();
    new npc_CapturePoint();
    new npc_sayge();
    new npc_steam_tonk();
    new npc_tonk_mine();
    new npc_brewfest_reveler();
    new npc_snake_trap();
    new npc_mirror_image();
    new npc_ebon_gargoyle();
    new npc_lightwell();
    new npc_barrier();
    new mob_mojo();
    new npc_training_dummy();
    new npc_shadowfiend();
    new npc_wormhole();
    new npc_pet_trainer();
    new npc_locksmith();
    new npc_experience();
    new npc_fire_elemental();
    new npc_earth_elemental();
    new npc_firework();
    new npc_spring_rabbit();
    new npc_generic_harpoon_cannon();
    new npc_shadowy_apparition();
    new npc_mage_orb();
    new npc_hand_of_guldan();
    new npc_mushroom();
    new npc_fungal_growth_one();
    new npc_fungal_growth_two();
    new npc_consecration();
    new npc_melee_guardian();
    new npc_protection_guardian();
    new npc_druid_treant();
    new npc_army_dead_ghoul();
    new npc_tower_defense();
    new npc_td_marker();
    new npc_dk_blood_parasite();
    new npc_sha_searing_totem();
    new npc_warl_doomguard();
    new npc_warl_ebon_imp();
    new npc_baby_moonkin();
    new npc_winter_reveler();
    new npc_abo_grinch();
    new npc_druid_t12_balance();
    new npc_priest_t12_holy();
    new npc_warl_infernal();
    new npc_tentacle_of_the_old_ones();
    new npc_argent_squire_gruntling();
    new npc_mostrasz_fly_event();
    new npc_wild_turkey();
    new npc_lonely_turkey();
    new npc_crashin_trashin_robot();
    new npc_immune_knockback();
    new stone_trogg_pill();
    new npc_eye_of_kilrogg();
    new npc_dancing_rune_weapon();
    new npc_guild_herald();
    new npc_hidden_egg();
    new npc_Unscripted_Combat_Bug_Prevention();
    new mob_arena_tornado();
    new npc_account_manager();
}
