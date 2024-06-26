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
#include "WorldPacket.h"
#include "WorldSession.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Log.h"
#include "Opcodes.h"
#include "Spell.h"
#include "ObjectAccessor.h"
#include "CreatureAI.h"
#include "Util.h"
#include "Pet.h"
#include "World.h"
#include "Group.h"
#include "SpellInfo.h"
#include "Player.h"

void WorldSession::HandleDismissCritter(WorldPacket& recvData)
{
    uint64 guid;
    recvData >> guid;

    TC_LOG_DEBUG("network.opcode", "WORLD: Received CMSG_DISMISS_CRITTER for GUID " UI64FMTD, guid);

    Unit* pet = ObjectAccessor::GetCreatureOrPetOrVehicle(*_player, guid);

    if (!pet)
    {
        TC_LOG_DEBUG("network.opcode", "Vanitypet (guid: %u) does not exist - player '%s' (guid: %u / account: %u) attempted to dismiss it (possibly lagged out)",
            uint32(GUID_LOPART(guid)), GetPlayer()->GetName().c_str(), GetPlayer()->GetGUIDLow(), GetAccountId());
        return;
    }

    if (_player->GetCritterGUID() == pet->GetGUID())
    {
         if (pet->GetTypeId() == TYPEID_UNIT && pet->ToCreature()->isSummon())
             pet->ToTempSummon()->UnSummon();
    }
}

void WorldSession::HandlePetAction(WorldPacket& recvData)
{
    uint64 guid1;
    uint32 data;
    uint64 guid2;
    float x, y, z;
    recvData >> guid1;                                     //pet guid
    recvData >> data;
    recvData >> guid2;                                     //tag guid
    // Position
    recvData >> x;
    recvData >> y;
    recvData >> z;

    uint32 spellid = UNIT_ACTION_BUTTON_ACTION(data);
    uint8 flag = UNIT_ACTION_BUTTON_TYPE(data);             //delete = 0x07 CastSpell = C1

    // used also for charmed creature
    Unit* pet= ObjectAccessor::GetUnit(*_player, guid1);
    TC_LOG_INFO("network.opcode", "HandlePetAction: Pet %u - flag: %u, spellid: %u, target: %u.", uint32(GUID_LOPART(guid1)), uint32(flag), spellid, uint32(GUID_LOPART(guid2)));

    if (!pet)
    {
        TC_LOG_ERROR("network.opcode", "HandlePetAction: Pet (GUID: %u) doesn't exist for player '%s'", uint32(GUID_LOPART(guid1)), GetPlayer()->GetName().c_str());
        return;
    }

    if (pet != GetPlayer()->GetFirstControlled())
    {
        TC_LOG_ERROR("network.opcode", "HandlePetAction: Pet (GUID: %u) does not belong to player '%s'", uint32(GUID_LOPART(guid1)), GetPlayer()->GetName().c_str());
        return;
    }

   

    //TODO: allow control charmed player?
    if (pet->GetTypeId() == TYPEID_PLAYER && !(flag == ACTIONBAR_PET_MOTIONCOMMAND && spellid == COMMAND_ATTACK))
        return;

    if (GetPlayer()->m_Controlled.size() == 1)
        HandlePetActionHelper(pet, guid1, spellid, flag, guid2, x, y, z);
    else
    {
        //If a pet is dismissed, m_Controlled will change
        std::vector<Unit*> controlled;
        for (auto itr = GetPlayer()->m_Controlled.begin(); itr != GetPlayer()->m_Controlled.end(); ++itr)
            if ((*itr)->GetEntry() == pet->GetEntry() && (*itr)->isAlive())
                controlled.push_back(*itr);
        for (auto itr = controlled.begin(); itr != controlled.end(); ++itr)
            HandlePetActionHelper(*itr, guid1, spellid, flag, guid2, x, y, z);
    }
}

void WorldSession::HandlePetStopAttack(WorldPacket &recvData)
{
    uint64 guid;
    recvData >> guid;

    TC_LOG_DEBUG("network.opcode", "WORLD: Received CMSG_PET_STOP_ATTACK for GUID " UI64FMTD "", guid);

    Unit* pet = ObjectAccessor::GetCreatureOrPetOrVehicle(*_player, guid);

    if (!pet)
    {
        TC_LOG_ERROR("network.opcode", "HandlePetStopAttack: Pet %u does not exist", uint32(GUID_LOPART(guid)));
        return;
    }

    if (pet != GetPlayer()->GetPet() && pet != GetPlayer()->GetCharm())
    {
        TC_LOG_ERROR("network.opcode", "HandlePetStopAttack: Pet GUID %u isn't a pet or charmed creature of player %s",
            uint32(GUID_LOPART(guid)), GetPlayer()->GetName().c_str());
        return;
    }

    if (!pet->isAlive())
        return;

    pet->AttackStop();
}

void WorldSession::HandlePetActionHelper(Unit* pet, uint64 guid1, uint32 spellid, uint16 flag, uint64 guid2, float x, float y, float z)
{
    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
    {
        TC_LOG_ERROR("network.opcode", "WorldSession::HandlePetAction(petGuid: " UI64FMTD ", tagGuid: " UI64FMTD ", spellId: %u, flag: %u): object (entry: %u TypeId: %u) is considered pet-like but doesn't have a charminfo!",
            guid1, guid2, spellid, flag, pet->GetGUIDLow(), pet->GetTypeId());
        return;
    }

    if (!pet->isAlive())
    {
        SpellInfo const* spell = sSpellMgr->GetSpellInfo(spellid);
        if (!spell)
            return;
        if (!(spell->Attributes & SPELL_ATTR0_CASTABLE_WHILE_DEAD))
            return;
    }

    switch (flag)
    {
    case ACTIONBAR_PET_MOTIONCOMMAND:                                   //0x07
        HandlePetExecuteAction(pet, guid1, spellid, flag, guid2, x, y, z);
        break;
    case ACTIONBAR_PET_REACTSTATE:                                  // 0x6
        HandlePetReactStateUpdate(pet, spellid);
        break;
    case ACTIONBAR_STATE_AURA_NOTACTIVE_OR_SPELL_NOTAUTO:                                  // 0x81    spell (disabled), ignore
    case ACT_PASSIVE:                                   // 0x01
    case ACTIONBAR_STATE_AURA_ACTIVE_OR_SPELL_AUTOCAST:                                   // 0xC1    spell
    {
        Unit* unit_target = NULL;

        if (guid2)
            unit_target = ObjectAccessor::GetUnit(*_player, guid2);

        // do not cast unknown spells
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellid);
        if (!spellInfo)
        {
            TC_LOG_ERROR("network.opcode", "WORLD: unknown PET spell id %i", spellid);
            return;
        }

        if (spellInfo->StartRecoveryCategory > 0)
            if (pet->GetCharmInfo() && pet->GetCharmInfo()->GetGlobalCooldownMgr().HasGlobalCooldown(spellInfo))
                return;

        for (uint32 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            if (spellInfo->Effects[i].TargetA.GetTarget() == TARGET_UNIT_SRC_AREA_ENEMY || spellInfo->Effects[i].TargetA.GetTarget() == TARGET_UNIT_DEST_AREA_ENEMY || spellInfo->Effects[i].TargetA.GetTarget() == TARGET_DEST_DYNOBJ_ENEMY)
                return;
        }

        // do not cast not learned spells
        if (!pet->HasSpell(spellid) || spellInfo->IsPassive())
            return;

        //  Clear the flags as if owner clicked 'attack'. AI will reset them
        //  after AttackStart, even if spell failed
        if (pet->GetCharmInfo())
        {
            pet->GetCharmInfo()->SetIsAtStay(false);
            pet->GetCharmInfo()->SetIsCommandAttack(true);
            pet->GetCharmInfo()->SetIsReturning(false);
            pet->GetCharmInfo()->SetIsFollowing(false);
        }

        Spell* spell = new Spell(pet, spellInfo, TRIGGERED_NONE);

        SpellCastResult result = spell->CheckPetCast(unit_target);

        //auto turn to target unless possessed
        if (result == SPELL_FAILED_UNIT_NOT_INFRONT && !pet->isPossessed() && !pet->IsVehicle())
        {
            if (unit_target)
            {
                pet->SetInFront(unit_target);
                if (unit_target->GetTypeId() == TYPEID_PLAYER)
                    pet->SendUpdateToPlayer((Player*)unit_target);
            }
            else if (Unit* unit_target2 = spell->m_targets.GetUnitTarget())
            {
                pet->SetInFront(unit_target2);
                if (unit_target2->GetTypeId() == TYPEID_PLAYER)
                    pet->SendUpdateToPlayer((Player*)unit_target2);
            }

            if (Unit* powner = pet->GetCharmerOrOwner())
                if (powner->GetTypeId() == TYPEID_PLAYER)
                    pet->SendUpdateToPlayer(powner->ToPlayer());

            result = SPELL_CAST_OK;
        }

        if (result == SPELL_FAILED_LINE_OF_SIGHT || result == SPELL_FAILED_OUT_OF_RANGE)
        {
            if (pet->ToPet())
            {
                uint32 spellId = spell->GetSpellInfo()->Id;
                unit_target = spell->m_targets.GetUnitTarget();
                if (unit_target)
                {
                    if (!unit_target->IsFriendlyTo(pet))
                    {
                        if (pet->getVictim() != unit_target || (pet->getVictim() == unit_target && !charmInfo->IsCommandAttack()))
                        {
                            if (pet->getVictim())
                                pet->AttackStop();
                            pet->GetMotionMaster()->Clear();
                            if (pet->ToCreature()->IsAIEnabled)
                                pet->GetAI()->AttackStart(unit_target, spellId);
                        }
                    }
                    else
                    {
                        pet->AttackStop();
                        pet->InterruptNonMeleeSpells(false);
                        pet->GetMotionMaster()->MoveFollow(unit_target, PET_FOLLOW_DIST, pet->GetFollowAngle(), MOTION_SLOT_ACTIVE, spellId);

                        charmInfo->SetIsCommandAttack(false);
                        charmInfo->SetIsAtStay(false);
                        charmInfo->SetIsReturning(true);
                        charmInfo->SetIsCommandFollow(true);
                        charmInfo->SetIsFollowing(false);
                    }
                    charmInfo->SetIsMovingForCast(true);
                }
            }
        }

        if (result == SPELL_CAST_OK)
        {
            unit_target = spell->m_targets.GetUnitTarget();

            //10% chance to play special pet attack talk, else growl
            //actually this only seems to happen on special spells, fire shield for imp, torment for voidwalker, but it's stupid to check every spell
            if (pet->ToCreature()->isPet() && (((Pet*)pet)->getPetType() == SUMMON_PET) && (pet != unit_target) && (urand(0, 100) < 10))
                pet->SendPetTalk((uint32)PET_TALK_SPECIAL_SPELL);
            else
                pet->SendPetAIReaction(guid1);

            if (!pet->ToCreature()->HasReactState(REACT_PASSIVE) && unit_target && !GetPlayer()->IsFriendlyTo(unit_target) && !pet->isPossessed() && !pet->IsVehicle())
            {
                // This is true if pet has no target or has target but targets differs.
                if (pet->getVictim() != unit_target)
                {
                    if (pet->getVictim())
                        pet->AttackStop();
                    pet->GetMotionMaster()->Clear();
                    if (pet->ToCreature()->IsAIEnabled)
                        pet->ToCreature()->AI()->AttackStart(unit_target);
                }
            }

            spell->prepare(&(spell->m_targets));
        }
        else
        {
            if (pet->isPossessed() || pet->IsVehicle())
                Spell::SendCastResult(GetPlayer(), spellInfo, 0, result);
            else
                pet->SendPetCastFail(0, spellInfo, result);


            /*
                THIS IS THE MONKEY VISUAL BUG SOURCE
            */
            //if (!pet->ToCreature()->HasSpellCooldown(spellid))
            //    GetPlayer()->SendClearCooldown(spellid, pet);

            spell->finish(false);
            delete spell;

            // reset specific flags in case of spell fail. AI will reset other flags
            if (pet->GetCharmInfo() && result != SPELL_FAILED_OUT_OF_RANGE)
                pet->GetCharmInfo()->SetIsCommandAttack(false);
        }
        break;
    }
    default:
        TC_LOG_ERROR("network.opcode", "WORLD: unknown PET flag Action %i and spellid %i.", uint32(flag), spellid);
    }
}

void WorldSession::HandlePetReactStateUpdate(Unit* pet, uint32 react_state)
{
    if (pet)
    {
        if (auto c = pet->ToCreature())
        {
            c->SetReactState(ReactStates(react_state));

            if (auto p = GetPlayer())
                switch (react_state)
                {
                case REACT_PASSIVE:                         //p assive
                    pet->AttackStop();


                    break;
                case REACT_DEFENSIVE:                       //recovery
                    break;
                case REACT_ASSIST:
                    if (auto o = pet->GetOwner())
                        if (auto v = o->getVictim())
                        {
                            if (auto v2 = pet->getVictim())
                            {
                                if (v2->isDead())
                                    c->AI()->AttackStart(v);
                            }
                            else
                            {
                                c->AI()->AttackStart(v);
                            }
                        }
                    break;

                case REACT_AGGRESSIVE:                      //activete
                    TC_LOG_ERROR("sql.sql", "player %u attempted to set pet entry %u to REACT_AGGRESSIVE, a disabled react state", p->GetGUID(), pet->GetEntry());
                    break;
                }
        }
    }
}


void WorldSession::HandlePetExecuteAction(Unit* pet, uint64 guid1, uint32 spellid, uint16 flag, uint64 guid2, float x, float y, float z)
{
    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
    {
        TC_LOG_ERROR("network.opcode", "WorldSession::HandlePetAction(petGuid: " UI64FMTD ", tagGuid: " UI64FMTD ", spellId: %u, flag: %u): object (entry: %u TypeId: %u) is considered pet-like but doesn't have a charminfo!",
            guid1, guid2, spellid, flag, pet->GetGUIDLow(), pet->GetTypeId());
        return;
    }

    switch (spellid)
    {
    case COMMAND_STAY:                          //flat=1792  //STAY

        if (auto p = pet->ToPet())
            if (p->hasCrowdControlPreventingActions())
                return;

                    pet->StopMoving();
                    pet->GetMotionMaster()->Clear(false);
                    pet->GetMotionMaster()->MoveIdle();
                    charmInfo->SetCommandState(COMMAND_STAY);

                    charmInfo->SetIsCommandAttack(false);
                    charmInfo->SetIsAtStay(true);
                    charmInfo->SetIsCommandFollow(false);
                    charmInfo->SetIsFollowing(false);
                    charmInfo->SetIsReturning(false);
        charmInfo->SaveStayPosition();
        break;
    case COMMAND_FOLLOW:                        //spellid=1792  //FOLLOW

        if (auto p = pet->ToPet())
            if (p->hasCrowdControlPreventingActions())
                return;

        pet->AttackStop();
            pet->InterruptNonMeleeSpells(false);
            pet->GetMotionMaster()->MoveFollow(_player, PET_FOLLOW_DIST, pet->GetFollowAngle());
            charmInfo->SetCommandState(COMMAND_FOLLOW);

            charmInfo->SetIsCommandAttack(false);
            charmInfo->SetIsAtStay(false);
            charmInfo->SetIsReturning(true);
            charmInfo->SetIsCommandFollow(true);
            charmInfo->SetIsFollowing(false);
        break;
    case COMMAND_ATTACK:                        //spellid=1792  //ATTACK
    {


        if (auto p = pet->ToPet())
            if (p->hasCrowdControlPreventingActions())
                return;

        charmInfo->SetIsCommandAttack(true);

        if (auto aura = pet->GetAura(51755))
            aura->Remove(AURA_REMOVE_BY_DAMAGE);

        if (auto o = pet->GetOwner())
            if (auto aura = o->GetAura(51755))
                aura->Remove(AURA_REMOVE_BY_DAMAGE);

        // Can't attack if owner is pacified
        if (_player->HasAuraType(SPELL_AURA_MOD_PACIFY))
        {
            //pet->SendPetCastFail(spellid, SPELL_FAILED_PACIFIED);
            //TODO: Send proper error message to client
            return;
        }

        // only place where pet can be player
        Unit* TargetUnit = ObjectAccessor::GetUnit(*_player, guid2);
        if (!TargetUnit)
            return;

        if (Unit* owner = pet->GetOwner())
            if (!owner->IsValidAttackTarget(TargetUnit))
                return;

        pet->ClearUnitState(UNIT_STATE_FOLLOW);
        // This is true if pet has no target or has target but targets differs.
        if (pet->getVictim() != TargetUnit || (pet->getVictim() == TargetUnit && !pet->GetCharmInfo()->IsCommandAttack()))
        {
            if (pet->getVictim())
                pet->AttackStop();

            if (pet->GetTypeId() != TYPEID_PLAYER && pet->ToCreature()->IsAIEnabled)
            {
                charmInfo->SetIsCommandAttack(true);
                charmInfo->SetIsAtStay(false);
                charmInfo->SetIsFollowing(false);
                charmInfo->SetIsCommandFollow(false);
                charmInfo->SetIsReturning(false);

                pet->ToCreature()->AI()->AttackStart(TargetUnit);
                if (pet->GetOwner() != NULL && pet->GetOwner()->GetTypeId() == TYPEID_PLAYER)
                    if (Player* owner = pet->GetOwner()->ToPlayer())
                        for (auto itr = owner->m_Controlled.begin(); itr != owner->m_Controlled.end(); ++itr)
                            if ((*itr) && (*itr)->isGuardian())
                                (*itr)->ToCreature()->AI()->AttackStart(TargetUnit);

                //10% chance to play special pet attack talk, else growl
                if (pet->ToCreature()->isPet() && ((Pet*)pet)->getPetType() == SUMMON_PET && pet != TargetUnit && urand(0, 100) < 10)
                    pet->SendPetTalk((uint32)PET_TALK_ATTACK);
                else
                {
                    // 90% chance for pet and 100% chance for charmed creature
                    pet->SendPetAIReaction(guid1);
                }
            }
            else                                // charmed player
            {
                if (auto v = pet->getVictim())
                if (v != TargetUnit)
                    pet->AttackStop(v);

                charmInfo->SetIsCommandAttack(true);
                charmInfo->SetIsAtStay(false);
                charmInfo->SetIsFollowing(false);
                charmInfo->SetIsCommandFollow(false);
                charmInfo->SetIsReturning(false);

                pet->Attack(TargetUnit, true);
                pet->SendPetAIReaction(guid1);
            }
        }
        break;
    }
    case COMMAND_ABANDON:                       // abandon (hunter pet) or dismiss (summoned pet)
        if (pet->GetCharmerGUID() == GetPlayer()->GetGUID())
            _player->StopCastingCharm();
        else if (pet->GetOwnerGUID() == GetPlayer()->GetGUID())
        {
            ASSERT(pet->GetTypeId() == TYPEID_UNIT);
            if (pet->isPet())
            {
                if (auto aura = pet->GetAura(51755))
                    aura->Remove(AURA_REMOVE_BY_DAMAGE);

                if (auto o = pet->GetOwner())
                    if (auto aura = o->GetAura(51755))
                        aura->Remove(AURA_REMOVE_BY_DAMAGE);

                if (((Pet*)pet)->getPetType() == HUNTER_PET)
                {
                    GetPlayer()->RemovePet((Pet*)pet, PET_SLOT_DELETED);
                    GetPlayer()->SetCurrentPetSlot(int8(PET_SLOT_FULL_LIST));
                }
                else
                    //dismissing a summoned pet is like killing them (this prevents returning a soulshard...)
                    pet->setDeathState(CORPSE);
            }
            else if (pet->HasUnitTypeMask(UNIT_MASK_MINION))
            {
                ((Minion*)pet)->UnSummon();
            }
        }
        break;
    case COMMAND_MOVE_TO:
    {

        if (auto p = pet->ToPet())
            if (p->hasCrowdControlPreventingActions())
                return;

        pet->RemoveUnitMovementFlag(MOVEMENTFLAG_DISABLE_COLLISION);
        PathGenerator path(pet);
        path.SetPathLengthLimit(500.f);

        bool fail{ false };

        if (!path.CalculatePath(x, y, z) || path.GetPathType() != PATHFIND_NORMAL)
        {
            fail = true;
        }

        if (auto p = pet->GetOwner())
            if (auto caster = p->ToPlayer())
            {
                WorldLocation dest{ caster->GetMapId(), x, y, z, 0.0f};

                float travelDistance        = caster->GetExactDist2d(&dest);                    //base edge of right triangle
                float pet_dZ                = (dest.GetPositionZ() - pet->GetPositionZ());      //vertical edge of right triangle
                float pet_travelDistance    = pet->GetExactDist2d(&dest);                       //base edge of right triangle

                if (travelDistance > 100.f)
                    fail = true;

                //only check slope if dZ is more than 5
                if (pet_dZ > 5.00f)
                    if ((pet_dZ * 2.0f) > pet_travelDistance)
                        fail = true;

                if (fail)
                    {
                        caster->GetSession()->SendNotification("|cffff0000No path available.|r");
                        if (!pet->isMoving())
                        pet->SetFacingTo(pet->GetAngle(&Position({ x, y, z, 0.0f })));
                        return;
                    }
            }

        pet->StopMoving();
        pet->GetMotionMaster()->Clear(false);
        pet->GetMotionMaster()->MovePoint(0, x, y, z);
        charmInfo->SetCommandState(COMMAND_MOVE_TO);

        charmInfo->SetIsCommandAttack(false);
        charmInfo->SetIsAtStay(true);
        charmInfo->SetIsFollowing(false);
        charmInfo->SetIsReturning(false);
        charmInfo->SaveStayPosition();
        break;
    }
    default:
        TC_LOG_ERROR("network.opcode", "WORLD: unknown PET flag Action %i and spellid %i.", uint32(flag), spellid);
    }
}

void WorldSession::HandlePetNameQuery(WorldPacket& recvData)
{
    TC_LOG_INFO("network.opcode", "HandlePetNameQuery. CMSG_PET_NAME_QUERY");

    uint32 petnumber;
    uint64 petguid;

    recvData >> petnumber;
    recvData >> petguid;

    SendPetNameQuery(petguid, petnumber);
}

void WorldSession::SendPetNameQuery(uint64 petguid, uint32 petnumber)
{
    Creature* pet = ObjectAccessor::GetCreatureOrPetOrVehicle(*_player, petguid);
    if (!pet)
    {
        WorldPacket data(SMSG_PET_NAME_QUERY_RESPONSE, (4+1+4+1));
        data << uint32(petnumber);
        data << uint8(0);
        data << uint32(0);
        data << uint8(0);
        _player->GetSession()->SendPacket(&data);
        return;
    }

    WorldPacket data(SMSG_PET_NAME_QUERY_RESPONSE, (4+4+pet->GetName().size()+1));
    data << uint32(petnumber);
    data << pet->GetName();
    data << uint32(pet->GetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP));

    if (pet->isPet() && ((Pet*)pet)->GetDeclinedNames())
    {
        data << uint8(1);
        for (uint8 i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
            data << ((Pet*)pet)->GetDeclinedNames()->name[i];
    }
    else
        data << uint8(0);

    _player->GetSession()->SendPacket(&data);
}

bool WorldSession::CheckStableMaster(uint64 guid)
{
    // spell case or GM
    if (guid == GetPlayer()->GetGUID())
    {
        if (!GetPlayer()->isGameMaster() && !GetPlayer()->HasAuraType(SPELL_AURA_OPEN_STABLE))
        {
            TC_LOG_DEBUG("network.opcode", "Player (GUID:%u) attempt open stable in cheating way.", GUID_LOPART(guid));
            return false;
        }
    }
    // stable master case
    else
    {
        if (!GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_STABLEMASTER))
        {
            TC_LOG_DEBUG("network.opcode", "Stablemaster (GUID:%u) not found or you can't interact with him.", GUID_LOPART(guid));
            return false;
        }
    }
    return true;
}

void WorldSession::HandlePetSetAction(WorldPacket& recvData)
{
    TC_LOG_INFO("network.opcode", "HandlePetSetAction. CMSG_PET_SET_ACTION");
    uint64 petguid;
    uint8  count;

    recvData >> petguid;

    Unit* pet = ObjectAccessor::GetUnit(*_player, petguid);

    if (!pet || pet != _player->GetFirstControlled())
    {
        TC_LOG_ERROR("network.opcode", "HandlePetSetAction: Unknown pet (GUID: %u) or pet owner (GUID: %u)", GUID_LOPART(petguid), _player->GetGUIDLow());
        return;
    }

    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
    {
        TC_LOG_ERROR("network.opcode", "WorldSession::HandlePetSetAction: object (GUID: %u TypeId: %u) is considered pet-like but doesn't have a charminfo!", pet->GetGUIDLow(), pet->GetTypeId());
        return;
    }

    count = (recvData.size() == 24) ? 2 : 1;

    uint32 position[2];
    uint32 data[2];

    for (uint8 i = 0; i < count; ++i)
    {
        recvData >> position[i];
        recvData >> data[i];
        //TC_LOG_ERROR("sql.sql", "count: %u, position: %u, data: %u", count, position[i], data[i]);
        //ignore invalid position
        if (position[i] >= MAX_UNIT_ACTION_BAR_INDEX)
            return;

        uint32 spell_id = uint32(UNIT_ACTION_BUTTON_ACTION(data[i]));
        uint8 act_state = uint8(UNIT_ACTION_BUTTON_TYPE(data[i]));
        UpdatePetActionBar(petguid, position[i], spell_id, act_state, data[i]);
    }
}

void WorldSession::UpdatePetActionBar(uint64 petguid, uint32 position, uint32 spell_id, uint8 act_state, uint32 data)
{

    if (auto pet = ObjectAccessor::GetUnit(*_player, petguid))
    if (auto charmInfo = pet->GetCharmInfo())
    {
        //TC_LOG_ERROR("network.opcode", "Player %s has changed pet spell action. Position: %u, Spell: %u, State: 0x%X",
            //_player->GetName().c_str(), position, spell_id, uint32(act_state));

        //if it's act for spell (en/disable/cast) and there is a spell given (0 = remove spell) which pet doesn't know, don't add
        if (act_state == ACTIONBAR_STATE_AURA_ACTIVE_OR_SPELL_AUTOCAST || act_state == ACTIONBAR_STATE_AURA_NOTACTIVE_OR_SPELL_NOTAUTO)
        {
            if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spell_id))
            {
                //sign for autocast
                if (act_state == ACTIONBAR_STATE_AURA_ACTIVE_OR_SPELL_AUTOCAST)
                {
                    if (pet->GetTypeId() == TYPEID_UNIT && pet->ToCreature()->isPet())
                        ((Pet*)pet)->ToggleAutocast(spellInfo, true);
                    else
                    {
                        if (auto p = GetPlayer())
                        for (auto itr : p->m_Controlled)
                            if (itr->GetEntry() == pet->GetEntry())
                                if (auto itr_info = itr->GetCharmInfo())
                                    itr_info->ToggleCreatureAutocast(spellInfo, bool(act_state == ACTIONBAR_STATE_AURA_ACTIVE_OR_SPELL_AUTOCAST));
                    }
                }
            }
        }
        //else TC_LOG_ERROR("sql.sql", "position: %u, data: %u", position, data);

        uint32 spell_id2 = uint32(UNIT_ACTION_BUTTON_ACTION(data));
        uint8 act_state2 = uint8(UNIT_ACTION_BUTTON_TYPE(data));
            charmInfo->SetActionBar(position, spell_id2, ActiveStates(act_state2));
    }
   
}
void WorldSession::HandlePetRename(WorldPacket& recvData)
{
    TC_LOG_INFO("network.opcode", "HandlePetRename. CMSG_PET_RENAME");

    uint64 petguid;
    uint8 isdeclined;

    std::string name;
    DeclinedName declinedname;

    recvData >> petguid;
    recvData >> name;
    recvData >> isdeclined;

    Pet* pet = ObjectAccessor::FindPet(petguid);
                                                            // check it!
    if (!pet || !pet->isPet() || ((Pet*)pet)->getPetType()!= HUNTER_PET ||
        !pet->HasByteFlag(UNIT_FIELD_BYTES_2, 2, UNIT_CAN_BE_RENAMED) ||
        pet->GetOwnerGUID() != _player->GetGUID() || !pet->GetCharmInfo())
        return;

    PetNameInvalidReason res = ObjectMgr::CheckPetName(name);
    if (res != PET_NAME_SUCCESS)
    {
        SendPetNameInvalid(res, name, NULL);
        return;
    }

    if (sObjectMgr->IsReservedName(name))
    {
        SendPetNameInvalid(PET_NAME_RESERVED, name, NULL);
        return;
    }

    pet->SetName(name);

    Unit* owner = pet->GetOwner();
    if (owner && (owner->GetTypeId() == TYPEID_PLAYER) && owner->ToPlayer()->GetGroup())
        owner->ToPlayer()->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_NAME);

    pet->RemoveByteFlag(UNIT_FIELD_BYTES_2, 2, UNIT_CAN_BE_RENAMED);

    if (isdeclined)
    {
        for (uint8 i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
        {
            recvData >> declinedname.name[i];
        }

        std::wstring wname;
        Utf8toWStr(name, wname);
        if (!ObjectMgr::CheckDeclinedNames(wname, declinedname))
        {
            SendPetNameInvalid(PET_NAME_DECLENSION_DOESNT_MATCH_BASE_NAME, name, &declinedname);
            return;
        }
    }

    SQLTransaction trans = CharacterDatabase.BeginTransaction();
    if (isdeclined)
    {
        PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_PET_DECLINEDNAME);
        stmt->setUInt32(0, pet->GetCharmInfo()->GetPetNumber());
        trans->Append(stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_ADD_CHAR_PET_DECLINEDNAME);
        stmt->setUInt32(0, _player->GetGUIDLow());

        for (uint8 i = 0; i < 5; i++)
            stmt->setString(i+1, declinedname.name[i]);

        trans->Append(stmt);
    }
    CharacterDatabase.CommitTransaction(trans);

    pet->SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, uint32(time(NULL))); // cast can't be helped
}

void WorldSession::HandlePetAbandon(WorldPacket& recvData)
{
    uint64 guid;
    recvData >> guid;                                      //pet guid
    TC_LOG_INFO("network.opcode", "HandlePetAbandon. CMSG_PET_ABANDON pet guid is %u", GUID_LOPART(guid));

    if (!_player->IsInWorld())
        return;

    // pet/charmed
    Creature* pet = ObjectAccessor::GetCreatureOrPetOrVehicle(*_player, guid);
    if (pet)
    {
        if (pet->isPet())
            _player->RemovePet((Pet*)pet, PET_SLOT_DELETED);
        else if (pet->GetGUID() == _player->GetCharmGUID())
            _player->StopCastingCharm();
    }
    SendStablePet(0);
}

void WorldSession::HandlePetSpellAutocastOpcode(WorldPacket& recvPacket)
{
    TC_LOG_INFO("network.opcode", "CMSG_PET_SPELL_AUTOCAST");
    uint64 guid;
    uint32 spellid;
    uint8  state;                                           //1 for on, 0 for off
    recvPacket >> guid >> spellid >> state;



    TC_LOG_ERROR("sql.sql", "guid: %u, spell: %u, state: %u", guid, spellid, state);

    if (!_player->GetGuardianPet() && !_player->GetCharm())
        return;

    if (ObjectAccessor::FindPlayer(guid))
        return;

    Creature* pet=ObjectAccessor::GetCreatureOrPetOrVehicle(*_player, guid);

    if (!pet || (pet != _player->GetGuardianPet() && pet != _player->GetCharm()))
    {
        TC_LOG_ERROR("network.opcode", "HandlePetSpellAutocastOpcode.Pet %u isn't pet of player %s .", uint32(GUID_LOPART(guid)), GetPlayer()->GetName().c_str());
        return;
    }

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellid);
    // do not add not learned spells/ passive spells
    if (!pet->HasSpell(spellid) || !spellInfo->IsAutocastable())
        return;

    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
    {
        TC_LOG_ERROR("network.opcode", "WorldSession::HandlePetSpellAutocastOpcod: object (GUID: %u TypeId: %u) is considered pet-like but doesn't have a charminfo!", pet->GetGUIDLow(), pet->GetTypeId());
        return;
    }

    if (pet->isPet())
        ((Pet*)pet)->ToggleAutocast(spellInfo, state);
    else
        pet->GetCharmInfo()->ToggleCreatureAutocast(spellInfo, state);

    charmInfo->SetSpellAutocast(spellInfo, state);
}

void WorldSession::HandlePetCastSpellOpcode(WorldPacket& recvPacket)
{
    TC_LOG_DEBUG("network.opcode", "WORLD: CMSG_PET_CAST_SPELL");

    uint64 guid;
    uint8  castCount;
    uint32 spellId;
    uint8  castFlags;

    recvPacket >> guid >> castCount >> spellId >> castFlags;

    TC_LOG_DEBUG("network.opcode", "WORLD: CMSG_PET_CAST_SPELL, guid: " UI64FMTD ", castCount: %u, spellId %u, castFlags %u", guid, castCount, spellId, castFlags);

    // This opcode is also sent from charmed and possessed units (players and creatures)
    if (!_player->GetGuardianPet() && !_player->GetCharm())
        return;

    Unit* caster = ObjectAccessor::GetUnit(*_player, guid);

    if (!caster || (caster != _player->GetGuardianPet() && caster != _player->GetCharm()))
    {
        TC_LOG_ERROR("network.opcode", "HandlePetCastSpellOpcode: Pet %u isn't pet of player %s .", uint32(GUID_LOPART(guid)), GetPlayer()->GetName().c_str());
        return;
    }

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
    {
        TC_LOG_ERROR("network.opcode", "WORLD: unknown PET spell id %i", spellId);
        return;
    }

    if (caster->GetMap() && (!caster->HasSpell(spellId) || spellInfo->IsPassive()) && caster->GetMap()->IsBattleground()) // hack fix for battlgrounds and SPELL_AURA_PERIODIC_TRIGGER_SPELL_BY_CLIENT
    {
        spellId = sSpellMgr->GetSpellIdForDifficulty(spellId, DUNGEON_DIFFICULTY_HEROIC);
        spellInfo = sSpellMgr->GetSpellInfo(spellId);
    }

    if (spellInfo->StartRecoveryCategory > 0) // Check if spell is affected by GCD
        if (caster->GetTypeId() == TYPEID_UNIT && caster->GetCharmInfo() && caster->GetCharmInfo()->GetGlobalCooldownMgr().HasGlobalCooldown(spellInfo))
        {
            caster->SendPetCastFail(castCount, spellInfo, SPELL_FAILED_NOT_READY);
            return;
        }

    bool trig = false;
    Unit::AuraEffectList const& auras = caster->GetAuraEffectsByType(SPELL_AURA_PERIODIC_TRIGGER_SPELL_BY_CLIENT);
    for (Unit::AuraEffectList::const_iterator i = auras.begin(); i != auras.end(); ++i)
        if (SpellInfo const *aura = (*i)->GetSpellInfo())
            if (aura->Effects[(*i)->GetEffIndex()].TriggerSpell == spellId)
            {
                trig = true;
                break;
            }

    // do not cast not learned spells
    if (!trig && (!caster->HasSpell(spellId) || spellInfo->IsPassive()))
        return;

    SpellCastTargets targets;
    targets.Read(recvPacket, caster);
    HandleClientCastFlags(recvPacket, castFlags, targets);

    caster->ClearUnitState(UNIT_STATE_FOLLOW);

    Spell* spell = new Spell(caster, spellInfo, trig ? TRIGGERED_FULL_MASK : TRIGGERED_NONE);
    spell->m_cast_count = castCount;                    // probably pending spell cast
    spell->m_targets = targets;

    // TODO: need to check victim?
    SpellCastResult result;
    if (caster->m_movedPlayer)
        result = spell->CheckPetCast(caster->m_movedPlayer->GetSelectedUnit());
    else
        result = spell->CheckPetCast(NULL);
    if (result == SPELL_CAST_OK)
    {
        if (caster->GetTypeId() == TYPEID_UNIT)
        {
            Creature* pet = caster->ToCreature();
            pet->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_CAST);
            if (pet->isPet())
            {
                Pet* p = (Pet*)pet;
                // 10% chance to play special pet attack talk, else growl
                // actually this only seems to happen on special spells, fire shield for imp, torment for voidwalker, but it's stupid to check every spell
                if (p->getPetType() == SUMMON_PET && (urand(0, 100) < 10))
                    pet->SendPetTalk((uint32)PET_TALK_SPECIAL_SPELL);
                else
                    pet->SendPetAIReaction(guid);
            }
        }

        spell->prepare(&(spell->m_targets));
    }
    else
    {
        caster->SendPetCastFail(castCount, spellInfo, result);
        if (caster->GetTypeId() == TYPEID_PLAYER)
        {
            if (!caster->ToPlayer()->HasSpellCooldown(spellId))
                GetPlayer()->SendClearCooldown(spellId, caster);
        }
        else
        {
            if (!caster->ToCreature()->HasSpellCooldown(spellId))
                GetPlayer()->SendClearCooldown(spellId, caster);
        }

        spell->finish(false);
        delete spell;
    }
}

void WorldSession::SendPetNameInvalid(uint32 error, const std::string& name, DeclinedName *declinedName)
{
    WorldPacket data(SMSG_PET_NAME_INVALID, 4 + name.size() + 1 + 1);
    data << uint32(error);
    data << name;
    data << uint8(declinedName ? 1 : 0);
    if (declinedName)
        for (uint32 i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
            data << declinedName->name[i];

    SendPacket(&data);
}

void WorldSession::HandlePetLearnTalent(WorldPacket& recvData)
{
    TC_LOG_DEBUG("network.opcode", "WORLD: CMSG_PET_LEARN_TALENT");

    uint64 guid;
    uint32 talentId, requestedRank;
    recvData >> guid >> talentId >> requestedRank;

    _player->LearnPetTalent(guid, talentId, requestedRank);
    _player->SendTalentsInfoData(true);
}

void WorldSession::HandleLearnPreviewTalentsPet(WorldPacket& recvData)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_LEARN_PREVIEW_TALENTS_PET");

    uint64 guid;
    recvData >> guid;

    uint32 talentsCount;
    recvData >> talentsCount;

    uint32 talentId, talentRank;

    for (uint32 i = 0; i < talentsCount; ++i)
    {
        recvData >> talentId >> talentRank;

        _player->LearnPetTalent(guid, talentId, talentRank);
    }

    _player->SendTalentsInfoData(true);
}
