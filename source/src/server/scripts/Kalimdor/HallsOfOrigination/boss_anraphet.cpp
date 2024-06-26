/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
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
#include "ScriptedGossip.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"
#include "GridNotifiers.h"
#include "Player.h"
#include "halls_of_origination.h"

enum Texts
{
    ANRAPHET_SAY_INTRO              = -40277,
    ANRAPHET_SAY_AGGRO              = -47919,
    ANRAPHET_SAY_OMEGA_STANCE       = -47920,
    ANRAPHET_SAY_KILL               = -47921,
    ANRAPHET_SAY_DEATH              = -47923,

    BRANN_SAY_DOOR_INTRO            = -40231,  // Right, let's go! Just need to input the final entry sequence into the door mechanism... and...
    BRANN_SAY_UNLOCK_DOOR           = -40232,  // That did the trick! The control room should be right behind this... oh... wow...
    BRANN_SAY_TROGGS                = -40233,  // What? This isn't the control room! There's another entire defense mechanism in place, and the blasted Rock Troggs broke into here somehow. Troggs. Why did it have to be Troggs!
    BRANN_SAY_THINK                 = -40234,  // Ok, let me think a moment.
    BRANN_SAY_MIRRORS               = -40240,  // Mirrors pointing all over the place.
    BRANN_SAY_ELEMENTALS            = -40235,  // Four platforms with huge elementals.
    BRANN_SAY_GET_IT                = -40236,  // I got it! I saw a tablet that mentioned this chamber. This is the Vault of Lights! Ok, simple enough. I need you adventurers to take out each of the four elementals to trigger the opening sequence for the far door!
    BRANN_1_ELEMENTAL_DEAD          = -40270,  // One down!
    BRANN_2_ELEMENTAL_DEAD          = -40271,  // Another one down! Just look at those light beams! They seem to be connecting to the far door!
    BRANN_3_ELEMENTAL_DEAD          = -40272,  // One more elemental to go! The door is almost open!
    BRANN_4_ELEMENTAL_DEAD          = -40273, // That''s it, you''ve done it! The vault door is opening! Now we can... oh, no!
    BRANN_SAY_ANRAPHET_DIED         = -49687, // We''ve done it! The control room is breached!
    BRANN_SAY_MOMENT                = -49688  // Here we go! Now this should only take a moment...
};

enum Events
{
    EVENT_BRANN_MOVE_INTRO          = 1,
    EVENT_BRANN_UNLOCK_DOOR         = 2,
    EVENT_BRANN_THINK               = 3,
    EVENT_BRANN_SET_ORIENTATION_1   = 4,
    EVENT_BRANN_SET_ORIENTATION_2   = 5,
    EVENT_BRANN_SET_ORIENTATION_3   = 6,
    EVENT_BRANN_SAY_ELEMENTALS      = 7,
    EVENT_BRANN_SAY_GET_IT          = 8,
    EVENT_BRANN_SET_ORIENTATION_4   = 9,

    EVENT_ANRAPHET_APPEAR           = 10,
    EVENT_ANRAPHET_MOVE_DOWNSTAIRS  = 11,
    EVENT_ANRAPHET_ACTIVATE         = 12,
    EVENT_ANRAPHET_DESTROY          = 13,
    EVENT_ANRAPHET_READY            = 14,
    EVENT_ANRAPHET_NEMESIS_STRIKE   = 15,
    EVENT_ANRAPHET_ALPHA_BEAMS,
    EVENT_ANRAPHET_ALPHA_BEAMS_END,
    EVENT_ANRAPHET_OMEGA_STANCE,
    EVENT_ANRAPHET_CRUMBLING_RUIN,
    EVENT_ANRAPHET_ACTIVATE_OMEGA,
    EVENT_ANRAPHET_OMEGA_ENDS,
    EVENT_3_PART_ROTATION_SELECTION,
    ACTION_ALPHA_BEAM_TRIGGER,
    ACTION_ALPHA_BEAM_STOP,
};

enum Spells
{
    SPELL_DESTRUCTION_PROTOCOL          = 77437,
    SPELL_ALPHA_BEAMS                   = 76184,
    SPELL_ALPHA_BEAMS_BACK_CAST         = 76912,
    SPELL_ALPHA_BEAMS_HEROIC            = 91205,
    SPELL_ALPHA_BEAMS_AURA              = 76956,
    SPELL_CRUMBLING_RUIN                = 75609,
    SPELL_NEMESIS_STRIKE                = 75604,
    SPELL_OMEGA_STANCE_SUMMON           = 77106,
    SPELL_OMEGA_STANCE                  = 75622,
    SPELL_OMEGA_STANCE_SPIDER_TRIGGER   = 77121,
    SPELL_OMEGA_STANCE_SPIDER_VISUAL    = 77126,
    //Wardens
    SPELL_LAVA_ERUPTION                 = 77273,
    SPELL_RAGING_INFERNO                = 77241,
    SPELL_WIND_SHEAR                    = 77334,
    SPELL_IMPALE                        = 77235,
    SPELL_ROCKWAVE                      = 77234,
    SPELL_BUBBLE_BOUND                  = 77335,
    SPELL_BUBBLE_BOUND_H                = 91158,
    SPELL_AQUA_BOMBS                    = 77349,
    SPELL_FASTER_THAN_THE_SPEED_OF_LIGHT = 94067,
};

enum Phases
{
    PHASE_INTRO         = 1,
};

enum Points
{
    POINT_ANRAPHET_STAIR_POS    = 1,
    POINT_ANRAPHET_ACTIVATE     = 2,

    MAX_BRANN_WAYPOINTS_INTRO   = 17
};

Position const AnraphetActivatePos[2] = 
{
    {-141.535f, 366.892f, 89.76741f, 3.139857f},
    {-193.656f, 366.689f, 75.91001f, 3.138207f},
};

Position const BrannIntroWaypoint[MAX_BRANN_WAYPOINTS_INTRO] =
{
    {-429.583f,  367.019f,  89.79282f, 0.0f},
    {-409.9531f, 367.0469f, 89.81111f, 0.0f},
    {-397.8246f, 366.967f,  86.37722f, 0.0f},
    {-383.7813f, 366.8229f, 82.07919f, 0.0f},
    {-368.2604f, 366.7448f, 77.0984f,  0.0f},
    {-353.6458f, 366.4896f, 75.92504f, 0.0f},
    {-309.0608f, 366.7205f, 75.91345f, 0.0f},
    {-276.3303f, 367.0f,    75.92413f, 0.0f},
    {-246.5104f, 366.6389f, 75.87791f, 0.0f},
    {-202.0417f, 366.7517f, 75.92508f, 0.0f},
    {-187.6024f, 366.7656f, 76.23077f, 0.0f},
    {-155.0938f, 366.783f,  86.45834f, 0.0f},
    {-143.5694f, 366.8177f, 89.73354f, 0.0f},
    {-128.5608f, 366.8629f, 89.74199f, 0.0f},
    {-103.559f,  366.5938f, 89.79725f, 0.0f},
    {-71.58507f, 367.0278f, 89.77069f, 0.0f},
    {-35.04861f, 366.6563f, 89.77447f, 0.0f},
};

struct boss_anraphet : public BossAI
{
    boss_anraphet(Creature* creature) : BossAI(creature, DATA_ANRAPHET) {}

    void Reset()
    {
        rotation = 0;
        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_CRUMBLING_RUIN);
        if (instance->GetData(DATA_DEAD_ELEMENTALS) == 4)
        {
            if (GameObject* door = ObjectAccessor::GetGameObject(*me, instance->GetData64(DATA_ANRAPHET_DOOR)))
                door->SetGoState(GO_STATE_ACTIVE);

            // Set to combat automatically, Brann's event won't repeat
            me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            me->SetReactState(REACT_AGGRESSIVE);
            me->SetHomePosition(AnraphetActivatePos[1]);
            me->GetMotionMaster()->MovePoint(0, AnraphetActivatePos[1]);
        }
        else
        {
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            me->SetReactState(REACT_PASSIVE);
            me->SetWalk(false);
            events.SetPhase(PHASE_INTRO);
        }
        _Reset();
    }

    void EnterCombat(Unit* /*who*/)
    {
        if (!me->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE))
        Talk(ANRAPHET_SAY_AGGRO);
        instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me, 1);
        events.RemovePhase(PHASE_INTRO);
        events.ScheduleEvent(GetNextSpellRotation(), 10000);
        events.ScheduleEvent(EVENT_ANRAPHET_NEMESIS_STRIKE, 7500);
        _EnterCombat();
    }

    void JustDied(Unit* /*killer*/)
    {
        Talk(ANRAPHET_SAY_DEATH);
        summons.DespawnAll();
        me->DeleteThreatList();
        instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_CRUMBLING_RUIN);
        if (Creature* brann = ObjectAccessor::GetCreature(*me, instance->GetData64(DATA_BRANN_0_GUID)))
            brann->AI()->DoAction(ACTION_ANRAPHET_DIED);
        instance->SetBossState(DATA_ANRAPHET, DONE);
        _JustDied();
    }

    void JustSummoned(Creature* summon)
    {
        switch (summon->GetEntry())
        {
            case NPC_ALPHA_BEAM:
                me->CastWithDelay(1, summon, SPELL_ALPHA_BEAMS_BACK_CAST);
                if (IsHeroic())
                    summon->CastSpell(summon, SPELL_ALPHA_BEAMS_HEROIC, true);
                break;
            default:
                break;
        }
        BossAI::JustSummoned(summon);
    }

    void KilledUnit(Unit* victim)
    {
        if (victim->GetTypeId() == TYPEID_PLAYER)
            Talk(ANRAPHET_SAY_KILL);
    }

    void JustReachedHome()
    {
        instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
        _JustReachedHome();
    }

    void DoAction(int32 const action)
    {
        if (action == ACTION_ANRAPHET_INTRO)
        {                
            events.SetPhase(PHASE_INTRO);
            events.ScheduleEvent(EVENT_ANRAPHET_APPEAR, 6000, 0, PHASE_INTRO);
        }

        if (action == ACTION_ALPHA_BEAM_TRIGGER)
        {
            /*
            if (alphabeams_target)
            {
                me->SummonCreature(41144, *alphabeams_target);
                me->SetTarget(alphabeams_target->GetGUID());
            }
            else
            */
            //{
            if (auto target = SelectTarget(SELECT_TARGET_RANDOM, 0, 100.00f, true))
            {
                me->SummonCreature(41144, *target);
                me->SetTarget(target->GetGUID());
            }
            //}
        }
        if (action == ACTION_ALPHA_BEAM_STOP)
            events.ScheduleEvent(EVENT_ANRAPHET_ALPHA_BEAMS_END, 3000);
    }

    void MovementInform(uint32 type, uint32 point)
    {
        if (type != POINT_MOTION_TYPE)
            return;

        switch(point)
        {
            case POINT_ANRAPHET_STAIR_POS:
                events.ScheduleEvent(EVENT_ANRAPHET_MOVE_DOWNSTAIRS, 0, 0, PHASE_INTRO);
                break;
            case POINT_ANRAPHET_ACTIVATE:
                events.ScheduleEvent(EVENT_ANRAPHET_ACTIVATE, 1500, 0, PHASE_INTRO);
                me->SetHomePosition(AnraphetActivatePos[1]);
                break;
        }                            
    }

    void UpdateAI(uint32 const diff)
    {
        if (!events.IsInPhase(PHASE_INTRO) && (!UpdateVictim() || !CheckInRoom()))
            return;

        events.Update(diff);
        while (uint32 eventId = events.ExecuteEvent())
            switch (eventId)
            {
                case EVENT_ANRAPHET_APPEAR:
                    me->GetMotionMaster()->MovePoint(POINT_ANRAPHET_STAIR_POS, AnraphetActivatePos[0]);
                    instance->DoUpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET2, SPELL_FASTER_THAN_THE_SPEED_OF_LIGHT);
                    break;
                case EVENT_ANRAPHET_MOVE_DOWNSTAIRS:
                    me->GetMotionMaster()->MovePoint(POINT_ANRAPHET_ACTIVATE, AnraphetActivatePos[1]);
                    break;
                case EVENT_ANRAPHET_ACTIVATE:
                    Talk(ANRAPHET_SAY_INTRO);
                    events.ScheduleEvent(EVENT_ANRAPHET_DESTROY, 17500, 0, PHASE_INTRO);
                    break;
                case EVENT_ANRAPHET_DESTROY:
                    //DoCastAOE(SPELL_DESTRUCTION_PROTOCOL);
                    DoCast(SPELL_DESTRUCTION_PROTOCOL);
                    me->CombatStop();
                    me->DeleteThreatList();
                    events.ScheduleEvent(EVENT_ANRAPHET_READY, 6000, 0, PHASE_INTRO);
                    break;
                case EVENT_ANRAPHET_READY:
                    me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                    me->SetReactState(REACT_AGGRESSIVE);
                    me->CombatStop();
                    me->DeleteThreatList();
                    break;

                    /*COMBAT*/
                case EVENT_ANRAPHET_NEMESIS_STRIKE:
                    if (me->HasUnitState(UNIT_STATE_CASTING) || me->HasReactState(REACT_PASSIVE)) 
                        events.ScheduleEvent(eventId, 250);
                    else
                    {
                        DoCastVictim(SPELL_NEMESIS_STRIKE);
                        events.ScheduleEvent(EVENT_ANRAPHET_NEMESIS_STRIKE, 15000);
                    }
                    break;
                case EVENT_ANRAPHET_ALPHA_BEAMS:
                    if (me->HasUnitState(UNIT_STATE_CASTING) || me->HasReactState(REACT_PASSIVE))
                        events.ScheduleEvent(eventId, 250);
                    else
                    {
                        alphabeams_target = SelectTarget(SELECT_TARGET_RANDOM, 0, 100.00f, true);
                        me->SetReactState(REACT_PASSIVE);
                        me->CastStop();
                        me->StopMoving();
                        me->AttackStop();
                        me->ClearUnitState(UNIT_STATE_CASTING);
                        me->CastWithDelay(500, me, SPELL_ALPHA_BEAMS, false);
                    }
                    break;
                case EVENT_ANRAPHET_ALPHA_BEAMS_END:
                        //TC_LOG_ERROR("sql.sql", "alpha beams end event.");
                        me->SetReactState(REACT_AGGRESSIVE);
                        UpdateVictim();
                        events.ScheduleEvent(EVENT_ANRAPHET_CRUMBLING_RUIN, 5000);
                    break;

                case EVENT_ANRAPHET_OMEGA_STANCE:
                    if (me->HasUnitState(UNIT_STATE_CASTING) || me->HasReactState(REACT_PASSIVE))
                        events.ScheduleEvent(eventId, 250);
                    else
                    {
                        //TC_LOG_ERROR("sql.sql", "Omega stance event");
                        Talk(ANRAPHET_SAY_OMEGA_STANCE);
                        DoCast(SPELL_OMEGA_STANCE);
                        me->CastWithDelay(2000, me, SPELL_OMEGA_STANCE_SUMMON, true);
                        events.ScheduleEvent(EVENT_ANRAPHET_OMEGA_ENDS, 17000);
                    }
                    break;
                case EVENT_ANRAPHET_OMEGA_ENDS:
                    if (me->HasUnitState(UNIT_STATE_CASTING) || me->HasReactState(REACT_PASSIVE))
                        events.ScheduleEvent(eventId, 250);
                    else
                    {
                        summons.DespawnEntry(NPC_OMEGA_STANCE);
                        events.ScheduleEvent(EVENT_ANRAPHET_CRUMBLING_RUIN, 5000);
                    }
                    break;
                case EVENT_ANRAPHET_CRUMBLING_RUIN:
                    if (me->HasUnitState(UNIT_STATE_CASTING) || me->HasReactState(REACT_PASSIVE))
                        events.ScheduleEvent(eventId, 250);
                    else
                    {
                        events.ScheduleEvent(GetNextSpellRotation(), 10000);
                        DoCast(me, SPELL_CRUMBLING_RUIN);
                    }
                    break;
            }
        if (!me->HasUnitState(UNIT_STATE_CASTING))
            DoMeleeAttackIfReady();
    }
    uint32 GetNextSpellRotation()
    {
        switch (rotation)
        {
        case 0:
        case 1:
            rotation++;
            //TC_LOG_ERROR("sql.sql", "next event will be alpha beams");
            return EVENT_ANRAPHET_ALPHA_BEAMS;
            break;
        case 2:
            rotation = 0;
            //TC_LOG_ERROR("sql.sql", "next event will be omega stance");
            return EVENT_ANRAPHET_OMEGA_STANCE;
            break;
        default:
            //TC_LOG_ERROR("sql.sql", "DEFAULTED: next event will be omega stance");
            return EVENT_ANRAPHET_ALPHA_BEAMS;
            break;
        }
        //TC_LOG_ERROR("sql.sql", "DEFAULTED_2: next event will be omega stance");
        return EVENT_ANRAPHET_ALPHA_BEAMS;
    }
    private:
        Unit* alphabeams_target{ nullptr };
        uint32 alphaBeamCount{ 0 };
        uint32 rotation{ 0 };
};

class npc_brann_bronzebeard_anraphet : public CreatureScript
{
    public:
        npc_brann_bronzebeard_anraphet() : CreatureScript("npc_brann_bronzebeard_anraphet") { }

        struct npc_brann_bronzebeard_anraphetAI : public CreatureAI
        {
            npc_brann_bronzebeard_anraphetAI(Creature* creature) : CreatureAI(creature), _currentPoint(0), _instance(creature->GetInstanceScript()) { }

            void DoAction(int32 const action)
            {
                switch (action)
                {
                    case ACTION_ELEMENTAL_DIED:
                    {
                        uint32 dead = _instance->GetData(DATA_DEAD_ELEMENTALS);
                        Talk(BRANN_1_ELEMENTAL_DEAD - dead + 1);
                        if (dead == 4)
                        {
                            _instance->DoCastSpellOnPlayers(SPELL_VAULT_OF_LIGHTS_CREDIT);
                            if (Creature* anraphet = ObjectAccessor::GetCreature(*me, _instance->GetData64(DATA_ANRAPHET_GUID)))
                                anraphet->AI()->DoAction(ACTION_ANRAPHET_INTRO);
                        }
                        break;
                    }
                    case ACTION_ANRAPHET_DIED:
                        me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                        events.ScheduleEvent(EVENT_BRANN_MOVE_INTRO, 1000);
                        break;
                    case ACTION_START_EVENT:
                        _instance->DoStartTimedAchievement(ACHIEVEMENT_TIMED_TYPE_EVENT, ACHIEV_VAULT_OF_LIGHTS_EVENT);
                        _instance->SetBossState(DATA_VAULT_OF_LIGHTS, IN_PROGRESS);
                        _currentPoint = 0;
                        events.Reset();
                        me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                        me->SetWalk(true);
                        Talk(BRANN_SAY_DOOR_INTRO);
                        events.ScheduleEvent(EVENT_BRANN_UNLOCK_DOOR, 7500);
                        break;
                }
            }

            void UpdateAI(uint32 const diff)
            {
                events.Update(diff);

                while (uint32 eventId = events.ExecuteEvent())
                {
                    switch (eventId)
                    {
                        case EVENT_BRANN_MOVE_INTRO:
                            if (_currentPoint < MAX_BRANN_WAYPOINTS_INTRO)
                                me->GetMotionMaster()->MovePoint(_currentPoint, BrannIntroWaypoint[_currentPoint]);
                            break;
                        case EVENT_BRANN_UNLOCK_DOOR:
                            Talk(BRANN_SAY_UNLOCK_DOOR);
                            _instance->SetBossState(DATA_VAULT_OF_LIGHTS, DONE);
                            events.ScheduleEvent(EVENT_BRANN_MOVE_INTRO, 3500);
                            break;
                        case EVENT_BRANN_THINK:
                            Talk(BRANN_SAY_THINK);
                            events.ScheduleEvent(EVENT_BRANN_SET_ORIENTATION_1, 6000);
                            break;
                        case EVENT_BRANN_SET_ORIENTATION_1:
                            me->SetFacingTo(5.445427f);
                            Talk(BRANN_SAY_MIRRORS);
                            events.ScheduleEvent(EVENT_BRANN_SET_ORIENTATION_2, 1000);
                            break;
                        case EVENT_BRANN_SET_ORIENTATION_2:
                            me->SetFacingTo(0.6283185f);
                            events.ScheduleEvent(EVENT_BRANN_SET_ORIENTATION_3, 2500);
                            break;
                        case EVENT_BRANN_SET_ORIENTATION_3:
                            me->SetFacingTo(0.01745329f);
                            events.ScheduleEvent(EVENT_BRANN_SAY_ELEMENTALS, 200);
                            break;
                        case EVENT_BRANN_SAY_ELEMENTALS:
                            Talk(BRANN_SAY_ELEMENTALS);
                            events.ScheduleEvent(EVENT_BRANN_SAY_GET_IT, 3500);
                            break;
                        case EVENT_BRANN_SAY_GET_IT:
                            Talk(BRANN_SAY_GET_IT);
                            me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                            break;
                        case EVENT_BRANN_SET_ORIENTATION_4:
                            me->SetFacingTo(3.141593f);
                            break;
                    }
                }
            }

            void MovementInform(uint32 movementType, uint32 pointId)
            {
                if (movementType != POINT_MOTION_TYPE)
                    return;

                _currentPoint = pointId + 1;
                uint32 delay = 1;

                switch (pointId)
                {
                    case 0:
                        Talk(BRANN_SAY_TROGGS);
                        events.ScheduleEvent(EVENT_BRANN_THINK, 15000);
                        return;
                    case 1:
                        Talk(BRANN_SAY_ANRAPHET_DIED);
                        delay = 1000;
                        break;
                    case 14:
                        Talk(BRANN_SAY_MOMENT);
                        delay = 2200;
                        break;
                    case 16:
                        events.ScheduleEvent(EVENT_BRANN_SET_ORIENTATION_4, 6000);
                        return;
                    default:
                        break;
                }

                events.ScheduleEvent(EVENT_BRANN_MOVE_INTRO, delay);
            }

        protected:
            EventMap events;
            uint32 _currentPoint;
            InstanceScript* _instance;
        };

        bool OnGossipHello(Player* player, Creature* creature)
        {
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Alright Brann, Let\'s start!", GOSSIP_SENDER_MAIN, 1);
            player->PlayerTalkClass->SendGossipMenu(1, creature->GetGUID());
            return true;
        }

        bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action)
        {
            if (creature->GetInstanceScript()->IsDone(DATA_VAULT_OF_LIGHTS))
                return false;

            creature->AI()->DoAction(ACTION_START_EVENT);
            player->PlayerTalkClass->SendCloseGossip();

            return true;
        }

        CreatureAI* GetAI(Creature* creature) const
        {
            return GetHallsOfOriginationAI<npc_brann_bronzebeard_anraphetAI>(creature);
        }
};

class npc_omega_stance : public CreatureScript
{
public:
    npc_omega_stance() : CreatureScript("npc_omega_stance") { }

    struct npc_omega_stanceAI : public ScriptedAI
    {
        npc_omega_stanceAI(Creature* creature) : ScriptedAI(creature) { }

        void IsSummonedBy(Unit* /*who*/)
        {
            DoCast(me, SPELL_OMEGA_STANCE_SPIDER_TRIGGER, true);
        }

        void EnterEvadeMode() { }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_omega_stanceAI(creature);
    }
};

class npc_alpha_beam : public CreatureScript
{
public:
    npc_alpha_beam() : CreatureScript("npc_alpha_beam") { }

    struct npc_alpha_beamAI : public ScriptedAI
    {
        npc_alpha_beamAI(Creature* creature) : ScriptedAI(creature), _instance(creature->GetInstanceScript()) { }

        void IsSummonedBy(Unit* /*summoner*/)
        {
            if (Creature* anraphet = ObjectAccessor::GetCreature(*me, _instance->GetData64(DATA_ANRAPHET_GUID)))
                anraphet->AI()->JustSummoned(me);
        }

        void EnterEvadeMode() { } // Never evade

    private:
        InstanceScript* _instance;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return GetHallsOfOriginationAI<npc_alpha_beamAI>(creature);
    }
};

class npc_flame_warden : public CreatureScript
{
public:
    npc_flame_warden() : CreatureScript("npc_flame_warden") { }

    struct npc_flame_wardenAI : public ScriptedAI
    {
        npc_flame_wardenAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = me->GetInstanceScript();
        }

        InstanceScript* instance;

        uint32 LavaTimer;
        uint32 InfernoTimer;

        void Reset()
        {
            LavaTimer = 5000;
            InfernoTimer = 20000;
            instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
        }
        void JustDied(Unit* /*killer*/)
        {
            instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
        }
        void EnterCombat(Unit* /*who*/)
        {
            instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me);
        }

        void UpdateAI(uint32 const diff)
        {
            if (!UpdateVictim())
                return;

            if (LavaTimer <= diff)
            {
                DoCastRandom(SPELL_LAVA_ERUPTION, 40.0f);
                LavaTimer = urand(5000, 10000);
            } else LavaTimer -= diff;

            if (InfernoTimer <= diff)
            {
                DoCast(SPELL_RAGING_INFERNO);
                InfernoTimer = urand(20000, 27500);
            } else InfernoTimer -= diff;

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_flame_wardenAI(creature);
    }
};

class npc_air_warden : public CreatureScript
{
public:
    npc_air_warden() : CreatureScript("npc_air_warden") { }

    struct npc_air_wardenAI : public ScriptedAI
    {
        npc_air_wardenAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = me->GetInstanceScript();
        }

        InstanceScript* instance;

        uint32 WindTimer;

        void Reset()
        {
            WindTimer = 7500;
            instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
        }
        void JustDied(Unit* /*killer*/)
        {
            instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
        }
        void EnterCombat(Unit* /*who*/)
        {
            instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me);
        }

        void UpdateAI(uint32 const diff)
        {
            if (!UpdateVictim())
                return;

            if (WindTimer <= diff)
            {
                DoCastRandom(SPELL_WIND_SHEAR, 40.0f);
                WindTimer = urand(7500, 15000);
            } else WindTimer -= diff;

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_air_wardenAI(creature);
    }
};

class npc_earth_warden : public CreatureScript
{
public:
    npc_earth_warden() : CreatureScript("npc_earth_warden") { }

    struct npc_earth_wardenAI : public ScriptedAI
    {
        npc_earth_wardenAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = me->GetInstanceScript();
        }

        InstanceScript* instance;

        uint32 RockTimer;
        uint32 ImpaleTimer;

        void Reset()
        {
            RockTimer = 10000;
            ImpaleTimer = 5000;
            instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
        }
        void JustDied(Unit* /*killer*/)
        {
            instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
        }
        void EnterCombat(Unit* /*who*/)
        {
            instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me);
        }

        void UpdateAI(uint32 const diff)
        {
            if (!UpdateVictim())
                return;

            if (RockTimer <= diff)
            {
                DoCastRandom(SPELL_ROCKWAVE, 40.0f);
                RockTimer = urand(20000, 27500);
            } else RockTimer -= diff;

            if (ImpaleTimer <= diff)
            {
                DoCastVictim(SPELL_IMPALE);                
                ImpaleTimer = urand(7500, 15000);
            } else ImpaleTimer -= diff;

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_earth_wardenAI(creature);
    }
};

class npc_water_warden : public CreatureScript
{
public:
    npc_water_warden() : CreatureScript("npc_water_warden") { }

    struct npc_water_wardenAI : public ScriptedAI
    {
        npc_water_wardenAI(Creature* creature) : ScriptedAI(creature), instance(creature->GetInstanceScript()) { }

        void Reset() override
        {
            instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
            events.Reset();
        }
        void JustDied(Unit* /*killer*/)
        {
            instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
        }
        void JustSummoned(Creature* summon)
        {
            //TC_LOG_ERROR("sql.sql", "Just summoned: %u", summon->GetEntry());
            if (summon->GetEntry() == 41264)
            {
                summon->CastSpell(summon, 77350, true);
            }
        }
        void OnSpellCastFinished(SpellInfo const* spell, SpellFinishReason reason)
        {
            //TC_LOG_ERROR("sql.sql", "Just casted: %u", spell->Id);
        }
        void EnterCombat(Unit* /*who*/)
        {

            instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me);
            events.ScheduleEvent(SPELL_BUBBLE_BOUND, 5000);
            events.ScheduleEvent(SPELL_AQUA_BOMBS, 9000);
        }
        void UpdateAI(const uint32 diff)
        {
            events.Update(diff);
            while (uint32 eventId = events.ExecuteEvent())
                switch (eventId)
                {
                    case SPELL_BUBBLE_BOUND:
                    {
                        auto list_of_players = me->GetPlayersInRange(100.f, true);
                        if (list_of_players.size())
                            if (auto random_target = Trinity::Containers::SelectRandomContainerElement(list_of_players))
                                me->CastSpell(random_target, SPELL_BUBBLE_BOUND);
                        events.ScheduleEvent(SPELL_BUBBLE_BOUND, urand(15000, 25000));
                        break;
                    }
                    case SPELL_AQUA_BOMBS:
                        //TC_LOG_ERROR("sql.sql", "enter SPELL_AQUA_BOMBS");
                        if (!me->HasAura(SPELL_AQUA_BOMBS))
                        me->CastSpell(me, SPELL_AQUA_BOMBS, true);
                        events.ScheduleEvent(SPELL_AQUA_BOMBS, urand(15000, 25000));
                        break;
                }
            if (!me->HasUnitState(UNIT_STATE_CASTING))
                if (UpdateVictim())
                    DoMeleeAttackIfReady();
        }

    private:
        EventMap events;
        InstanceScript* instance;
    };


    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_water_wardenAI(creature);
    }
};

struct npc_water_warden_water_bubble : public Scripted_NoMovementAI
{
    npc_water_warden_water_bubble(Creature* creature) : Scripted_NoMovementAI(creature)
    {
        me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
        me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK_DEST, true);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_GRIP, true);
    }

    void Reset()
    {
        me->SetReactState(REACT_PASSIVE);
    }

    void JustDied(Unit* /*killer*/)
    {
        if (TempSummon* meTmp = me->ToTempSummon())
        {
            if (Unit* summoner = meTmp->GetSummoner())
            {
                summoner->RemoveAurasDueToSpell(SPELL_BUBBLE_BOUND);
                summoner->RemoveAurasDueToSpell(SPELL_BUBBLE_BOUND_H);
            }
        }

        me->DespawnOrUnsummon();
    }

    void UpdateAI(const uint32 diff)
    {
        TempSummon* meTmp = me->ToTempSummon();
        if (!meTmp)
        {
            me->DespawnOrUnsummon();
            return;
        }

        Unit* summoner = meTmp->GetSummoner();
        if (!summoner)
        {
            me->DespawnOrUnsummon();
            return;
        }

        if (!summoner->isAlive())
            me->DespawnOrUnsummon();

        if (!summoner->HasAura(SPELL_BUBBLE_BOUND) &&
            !summoner->HasAura(SPELL_BUBBLE_BOUND_H))
            me->DespawnOrUnsummon();
    }
};

class npc_anraphet_trogg : public CreatureScript
{
public:
    npc_anraphet_trogg() : CreatureScript("npc_anraphet_trogg") { }

    struct npc_anraphet_troggAI : public ScriptedAI
    {
        npc_anraphet_troggAI(Creature* creature) : ScriptedAI(creature) { }
	
    void Reset()
    {
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK_DEST, true);
    }

    bool CanRespawn()
    {
        return instance->GetData(DATA_DEAD_ELEMENTALS) < 4;
    }
};
	    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_anraphet_troggAI(creature);
    }
};

class spell_anraphet_alpha_beams : public SpellScriptLoader
{
public:
    spell_anraphet_alpha_beams() : SpellScriptLoader("spell_anraphet_alpha_beams") { }

    class spell_anraphet_alpha_beams_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_anraphet_alpha_beams_SpellScript);

        void FilterTargets(std::list<WorldObject*>& targets)
        {
            if (targets.size())
                targets.remove_if(Trinity::UnitAuraCheck(true, SPELL_ALPHA_BEAMS_AURA));

            if (targets.empty())
            if (WorldObject* target = Trinity::Containers::SelectRandomContainerElement(targets))
            {
                targets.clear();
                targets.push_back(target);
            }
        }

        void Register()
        {
            OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_anraphet_alpha_beams_SpellScript::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        }
    };

    SpellScript* GetSpellScript() const
    {
        return new spell_anraphet_alpha_beams_SpellScript();
    }
};

class spell_anraphet_omega_stance_summon : public SpellScriptLoader
{
public:
    spell_anraphet_omega_stance_summon() : SpellScriptLoader("spell_anraphet_omega_stance_summon") { }

    class spell_anraphet_omega_stance_summon_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_anraphet_omega_stance_summon_SpellScript);

        void ModDestHeight(SpellEffIndex /*effIndex*/)
        {
            Position offset = {0.0f, 0.0f, 30.0f, 0.0f};
            const_cast<WorldLocation*>(GetExplTargetDest())->RelocateOffset(offset);
            GetHitDest()->RelocateOffset(offset);
        }

        void Register()
        {
            OnEffectLaunch += SpellEffectFn(spell_anraphet_omega_stance_summon_SpellScript::ModDestHeight, EFFECT_0, SPELL_EFFECT_SUMMON);
        }
    };

    SpellScript* GetSpellScript() const
    {
        return new spell_anraphet_omega_stance_summon_SpellScript();
    }
};

class spell_omega_stance_spider_effect : public SpellScriptLoader
{
public:
    spell_omega_stance_spider_effect() : SpellScriptLoader("spell_omega_stance_spider_effect") { }

    class spell_omega_stance_spider_effect_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_omega_stance_spider_effect_SpellScript);

        void SetDestPosition(SpellEffIndex effIndex)
        {
            // Do our own calculations for the destination position.
            /// TODO: Remove this once we find a general rule for WorldObject::MovePosition (this spell shouldn't take the Z change into consideration)
            Unit* caster = GetCaster();
            float angle = float(rand_norm()) * static_cast<float>(2 * M_PI);
            uint32 dist = caster->GetObjectSize() + GetSpellInfo()->Effects[effIndex].CalcRadius(GetCaster()) * (float)rand_norm();

            float x = caster->GetPositionX() + dist * std::cos(angle);
            float y = caster->GetPositionY() + dist * std::sin(angle);
            float z = caster->GetMap()->GetHeight(x, y, caster->GetPositionZ());

            const_cast<WorldLocation*>(GetExplTargetDest())->Relocate(x, y, z);
            GetHitDest()->Relocate(x, y, z);
        }

        void HandleDummy(SpellEffIndex /*effIndex*/)
        {
            Position destPos;
            GetHitDest()->GetPosition(&destPos);
            GetCaster()->CastSpell(destPos.GetPositionX(), destPos.GetPositionY(), destPos.GetPositionZ(), SPELL_OMEGA_STANCE_SPIDER_VISUAL, true);
        }

        void Register()
        {
            OnEffectLaunch += SpellEffectFn(spell_omega_stance_spider_effect_SpellScript::SetDestPosition, EFFECT_0, SPELL_EFFECT_DUMMY);
            OnEffectHit += SpellEffectFn(spell_omega_stance_spider_effect_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
        }
    };

    SpellScript* GetSpellScript() const
    {
        return new spell_omega_stance_spider_effect_SpellScript();
    }
};

class spell_hoo_destruction_protocol : public SpellScript
{
    PrepareSpellScript(spell_hoo_destruction_protocol);

    void HandleDamage(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
        {
            if (InstanceScript* script = GetCaster()->GetInstanceScript())
            {
                float damageMultiplier = sChallengeModeMgr->GetDamageMultiplier(script->GetChallengeModeLevel());
                SetHitDamage(target->CountPctFromMaxHealth(GetHitDamage()) / damageMultiplier);
            }

            if (Creature* c = target->ToCreature())
                if (c->GetEntry() == NPC_VAULT_TROGG_BRUTE || c->GetEntry() == NPC_VAULT_TROGG_ROCKFLINGER || c->GetEntry() == NPC_VAULT_TROGG_PILLAGER)
                {
                    //c->SetRespawnTime(2147000000);
                    c->SetPhaseMask(2, true);
                }
        }
    }

    void Register()
    {
        OnEffectHitTarget += SpellEffectFn(spell_hoo_destruction_protocol::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

class spell_anraphet_alpha_beam : public SpellScriptLoader
{
public:
    spell_anraphet_alpha_beam() : SpellScriptLoader("spell_anraphet_alpha_beam") 
    { 
    }

    class spell_anraphet_alpha_beam_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_anraphet_alpha_beam_AuraScript);

        void HandleEffectPeriodic(AuraEffect const* /*aurEff*/)
        {
            PreventDefaultAction();
            if (auto c = GetCaster())
                if (auto creature = c->ToCreature())
                    if (auto ai = creature->AI())
                        ai->DoAction(ACTION_ALPHA_BEAM_TRIGGER);
        }


        void HandleOnEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
        {
            if (auto c = GetCaster())
                if (auto creature = c->ToCreature())
                    if (auto ai = creature->AI())
                        ai->DoAction(ACTION_ALPHA_BEAM_STOP);
        }

        void Register()
        {
            OnEffectPeriodic += AuraEffectPeriodicFn(spell_anraphet_alpha_beam_AuraScript::HandleEffectPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
            OnEffectRemove += AuraEffectRemoveFn(spell_anraphet_alpha_beam_AuraScript::HandleOnEffectRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
        }

    private:
        bool Spawned{ false };
    };

    AuraScript* GetAuraScript() const
    {
        return new spell_anraphet_alpha_beam_AuraScript();
    }
};

class spell_water_worden_aqua_bombs : public SpellScriptLoader
{
public:
    spell_water_worden_aqua_bombs() : SpellScriptLoader("spell_water_worden_aqua_bombs") { }

    class spell_water_worden_aqua_bombs_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_water_worden_aqua_bombs_AuraScript);

        void CalcPeriodic(AuraEffect const* effect, bool& isPeriodic, int32& amplitude)
        {
            isPeriodic = true;
        }

        void HandleEffectPeriodicUpdate(AuraEffect* aurEff)
        {
            if (auto a = aurEff->GetBase())
                if (auto o = a->GetOwner())
                    if (auto c = o->ToUnit())
                        c->CastSpell(c, GetSpellInfo()->Effects[0].TriggerSpell, true);
        }

        void Register()
        {
            OnEffectUpdatePeriodic += AuraEffectUpdatePeriodicFn(spell_water_worden_aqua_bombs_AuraScript::HandleEffectPeriodicUpdate, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        }

    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_water_worden_aqua_bombs_AuraScript();
    }
};

class spell_water_worden_aqua_pool_resize : public SpellScriptLoader
{
    /*
        Normal 10: 77351
        Normal 25: 91157
    */
public:
    spell_water_worden_aqua_pool_resize() : SpellScriptLoader("spell_water_worden_aqua_pool_resize") { }

    class spell_water_worden_aqua_pool_resize_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_water_worden_aqua_pool_resize_SpellScript);

        void FilterTargets(std::list<WorldObject*>& unitList)
        {
            if (Unit* caster = GetCaster())
            {
                for (std::list<WorldObject*>::const_iterator itr = unitList.begin(); itr != unitList.end();)
                {
                    WorldObject* temp = (*itr);
                    itr++;
                    if (temp->GetDistance(caster) > caster->GetFloatValue(OBJECT_FIELD_SCALE_X) * 0.5f)   //criteria for being removed from target list
                        unitList.remove(temp);
                }
            }
        }


        void HandleAfterCast()
        {
            if (auto c = GetCaster())
                if (auto cr = c->ToCreature())
                cr->DespawnOrUnsummon(2000);
        }



        void Register()
        {
            AfterCast += SpellCastFn(spell_water_worden_aqua_pool_resize_SpellScript::HandleAfterCast);
            OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_water_worden_aqua_pool_resize_SpellScript::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
            OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_water_worden_aqua_pool_resize_SpellScript::FilterTargets, EFFECT_1, TARGET_UNIT_SRC_AREA_ENEMY);
        };
    };

    SpellScript* GetSpellScript() const
    {
        return new spell_water_worden_aqua_pool_resize_SpellScript();
    }
};


class npc_water_bomb_trigger : public CreatureScript
{
public:
    npc_water_bomb_trigger() : CreatureScript("npc_water_bomb_trigger") { }

    struct npc_water_bomb_triggerAI : public ScriptedAI
    {
        npc_water_bomb_triggerAI(Creature* creature) : ScriptedAI(creature), instance(creature->GetInstanceScript())
        {

        }

        void InitializeAI()
        {
            events.ScheduleEvent(SPELL_AQUA_BOMBS, 500);
        }

        void UpdateAI(const uint32 diff)
        {
            events.Update(diff);
            while (uint32 eventId = events.ExecuteEvent())
                switch (eventId)
                {
                case SPELL_AQUA_BOMBS:
                    me->SetObjectScale(me->GetFloatValue(OBJECT_FIELD_SCALE_X) + 0.4f);
                    events.ScheduleEvent(SPELL_AQUA_BOMBS, 1000);
                    break;
                }
        }

    private:
        EventMap events;
        InstanceScript* instance;
    };


    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_water_bomb_triggerAI(creature);
    }
};

class spell_aqua_bomb_timer_aura : public SpellScriptLoader
{
public:
    spell_aqua_bomb_timer_aura() : SpellScriptLoader("spell_aqua_bomb_timer_aura")
    {
    }

    class spell_aqua_bomb_timer_aura_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_aqua_bomb_timer_aura_AuraScript);

        void HandleEffectPeriodic(AuraEffect const* aurEff)
        {
            if (auto c = GetCaster())
                if (c->GetEntry() == 41264)
                    c->CastSpell(c, GetSpellInfo()->Effects[aurEff->GetEffIndex()].TriggerSpell, true);
        }


        void HandleOnEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
        {
        }

        void Register()
        {
            OnEffectPeriodic += AuraEffectPeriodicFn(spell_aqua_bomb_timer_aura_AuraScript::HandleEffectPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
            //OnEffectRemove += AuraEffectRemoveFn(spell_aqua_bomb_timer_aura_AuraScript::HandleOnEffectRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
        }

    private:
        bool Spawned{ false };
    };

    AuraScript* GetAuraScript() const
    {
        return new spell_aqua_bomb_timer_aura_AuraScript();
    }
};


void AddSC_boss_anraphet()
{
    RegisterCreatureAI(boss_anraphet);
    new npc_brann_bronzebeard_anraphet();
    new npc_alpha_beam();
    new npc_omega_stance();
    new npc_flame_warden();
    new npc_air_warden();
    new npc_earth_warden();
    new npc_water_warden();
    RegisterCreatureAI(npc_water_warden_water_bubble);
    new npc_anraphet_trogg();
    new spell_anraphet_alpha_beams();
    new spell_anraphet_omega_stance_summon();
    new spell_omega_stance_spider_effect();
    RegisterSpellScript(spell_hoo_destruction_protocol);
    new spell_anraphet_alpha_beam();
    new spell_water_worden_aqua_bombs();
    new spell_water_worden_aqua_pool_resize();
    new npc_water_bomb_trigger();
    new spell_aqua_bomb_timer_aura();
}
