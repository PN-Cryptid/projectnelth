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
#include "DBCStores.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "ObjectMgr.h"
#include "GuildMgr.h"
#include "SpellMgr.h"
#include "Log.h"
#include "Opcodes.h"
#include "Spell.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "SpellAuras.h"
#include "CreatureAI.h"
#include "ScriptMgr.h"
#include "GameObjectAI.h"
#include "SpellAuraEffects.h"
#include "Player.h"

void WorldSession::HandleClientCastFlags(WorldPacket& recvPacket, uint8 castFlags, SpellCastTargets& targets)
{
    // some spell cast packet including more data (for projectiles?)
    if (castFlags & 0x02)
    {
        // not sure about these two
        float elevation, speed;
        recvPacket >> elevation;
        recvPacket >> speed;

        targets.SetElevation(elevation);
        targets.SetSpeed(speed);

        uint8 hasMovementData;
        recvPacket >> hasMovementData;
        if (hasMovementData)
        {
            recvPacket.SetOpcode(Opcodes(recvPacket.read<uint32>()));
            HandleMovementOpcodes(recvPacket);
        }
    }
    else if (castFlags & 0x8)   // Archaeology
    {
        uint32 count, entry, usedCount;
        uint8 type;
        recvPacket >> count;
        for (uint32 i = 0; i < count; ++i)
        {
            recvPacket >> type;
            switch (type)
            {
                case 2: // Keystones
                    recvPacket >> entry;        // Item id
                    recvPacket >> usedCount;    // Item count
                    break;
                case 1: // Fragments
                    recvPacket >> entry;        // Currency id
                    recvPacket >> usedCount;    // Currency count
                    break;
            }
        }
    }
}

void WorldSession::HandleUseItemOpcode(WorldPacket& recvPacket)
{
    uint32 oldMSTime = getMSTime();
    // TODO: add targets.read() check
    Player* pUser = _player;
    // ignore for remote control state
    if (pUser->m_mover != pUser)
        return;
    uint8 bagIndex, slot, castFlags;
    uint8 castCount;                                       // next cast if exists (single or not)
    uint64 itemGUID;
    uint32 glyphIndex;                                      // something to do with glyphs?
    uint32 spellId;                                         // casted spell id
    recvPacket >> bagIndex >> slot >> castCount >> spellId >> itemGUID >> glyphIndex >> castFlags;

    if (glyphIndex >= MAX_GLYPH_SLOT_INDEX)
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, NULL, NULL);
        return;
    }
    Item* pItem = pUser->GetUseableItemByPos(bagIndex, slot);
    if (!pItem)
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, NULL, NULL);
        return;
    }
    if (pItem->GetGUID() != itemGUID)
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, NULL, NULL);
        return;
    }
    TC_LOG_DEBUG("network.opcode", "WORLD: CMSG_USE_ITEM packet, bagIndex: %u, slot: %u, castCount: %u, spellId: %u, Item: %u, glyphIndex: %u, data length = %i", bagIndex, slot, castCount, spellId, pItem->GetEntry(), glyphIndex, (uint32)recvPacket.size());

    ItemTemplate const* proto = pItem->GetTemplate();
    if (!proto)
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pItem, NULL);
        return;
    }
    // some item classes can be used only in equipped state
    if (proto->InventoryType != INVTYPE_NON_EQUIP && !pItem->IsEquipped())
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pItem, NULL);
        return;
    }
    InventoryResult msg = pUser->CanUseItem(pItem);
    if (msg != EQUIP_ERR_OK)
    {
        pUser->SendEquipError(msg, pItem, NULL);
        return;
    }
    // only allow conjured consumable, bandage, poisons (all should have the 2^21 item flag set in DB)
    if (proto->Class == ITEM_CLASS_CONSUMABLE && !(proto->Flags & ITEM_PROTO_FLAG_USEABLE_IN_ARENA) && pUser->InArena())
    {
        pUser->SendEquipError(EQUIP_ERR_NOT_DURING_ARENA_MATCH, pItem, NULL);
        return;
    }
    // don't allow items banned in arena
    if (proto->Flags & ITEM_PROTO_FLAG_NOT_USEABLE_IN_ARENA && pUser->InArena())
    {
        pUser->SendEquipError(EQUIP_ERR_NOT_DURING_ARENA_MATCH, pItem, NULL);
        return;
    }
    if (pUser->isInCombat())
    {
        for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(proto->Spells[i].SpellId))
            {
                if (!spellInfo->CanBeUsedInCombat())
                {
                    pUser->SendEquipError(EQUIP_ERR_NOT_IN_COMBAT, pItem, NULL);
                    return;
                }
            }
        }
    }
    // check also  BIND_WHEN_PICKED_UP and BIND_QUEST_ITEM for .additem or .additemset case by GM (not binded at adding to inventory)
    if (pItem->GetTemplate()->Bonding == BIND_WHEN_USE || pItem->GetTemplate()->Bonding == BIND_WHEN_PICKED_UP || pItem->GetTemplate()->Bonding == BIND_QUEST_ITEM)
    {
        if (!pItem->IsSoulBound())
        {
            pItem->SetState(ITEM_CHANGED, pUser);
            pItem->SetBinding(true);
        }
    }
    SpellCastTargets targets;
    targets.Read(recvPacket, pUser);
    HandleClientCastFlags(recvPacket, castFlags, targets);

    // Note: If script stop casting it must send appropriate data to client to prevent stuck item in gray state.
    if (!sScriptMgr->OnItemUse(pUser, pItem, targets))
    {
        // no script or script not process request by self
        pUser->CastItemUseSpell(pItem, targets, castCount, glyphIndex);
    }

    if (GetMSTimeDiffToNow(oldMSTime) >= 100)
        TC_LOG_INFO("network.opcode", "OPCODE INFO: Item: %u, need time: %u", pItem->GetTemplate()->ItemId, GetMSTimeDiffToNow(oldMSTime));
}

void WorldSession::HandleOpenItemOpcode(WorldPacket& recvPacket)
{
    TC_LOG_DEBUG("network.opcode", "WORLD: CMSG_OPEN_ITEM packet, data length = %i", (uint32)recvPacket.size());

    Player* pUser = _player;

    // ignore for remote control state
    if (pUser->m_mover != pUser)
        return;

    uint8 bagIndex, slot;

    recvPacket >> bagIndex >> slot;

    TC_LOG_INFO("network.opcode", "bagIndex: %u, slot: %u", bagIndex, slot);

    Item* item = pUser->GetItemByPos(bagIndex, slot);
    if (!item)
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, NULL, NULL);
        return;
    }

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, item, NULL);
        return;
    }

    // Verify that the bag is an actual bag or wrapped item that can be used "normally"
    if (!(proto->Flags & ITEM_PROTO_FLAG_OPENABLE) && !item->HasFlag(ITEM_FIELD_FLAGS, ITEM_FLAG_WRAPPED))
    {
        pUser->SendEquipError(EQUIP_ERR_CLIENT_LOCKED_OUT, item, NULL);
        TC_LOG_ERROR("network.opcode", "Possible hacking attempt: Player %s [guid: %u] tried to open item [guid: %u, entry: %u] which is not openable!",
                pUser->GetName().c_str(), pUser->GetGUIDLow(), item->GetGUIDLow(), proto->ItemId);
        return;
    }

    // locked item
    uint32 lockId = proto->LockID;
    if (lockId)
    {
        LockEntry const* lockInfo = sLockStore.LookupEntry(lockId);

        if (!lockInfo)
        {
            pUser->SendEquipError(EQUIP_ERR_ITEM_LOCKED, item, NULL);
            TC_LOG_ERROR("network.opcode", "WORLD::OpenItem: item [guid = %u] has an unknown lockId: %u!", item->GetGUIDLow(), lockId);
            return;
        }

        // was not unlocked yet
        if (item->IsLocked())
        {
            pUser->SendEquipError(EQUIP_ERR_ITEM_LOCKED, item, NULL);
            return;
        }
    }

    if (item->HasFlag(ITEM_FIELD_FLAGS, ITEM_FLAG_WRAPPED))// wrapped?
    {
        PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_GIFT_BY_ITEM);

        stmt->setUInt32(0, item->GetGUIDLow());

        PreparedQueryResult result = CharacterDatabase.Query(stmt);

        if (result)
        {
            Field* fields = result->Fetch();
            uint32 entry = fields[0].GetUInt32();
            uint32 flags = fields[1].GetUInt32();

            item->SetUInt64Value(ITEM_FIELD_GIFTCREATOR, 0);
            item->SetEntry(entry);
            item->SetUInt32Value(ITEM_FIELD_FLAGS, flags);
            item->SetState(ITEM_CHANGED, pUser);
        }
        else
        {
            TC_LOG_ERROR("network.opcode", "Wrapped item %u don't have record in character_gifts table and will deleted", item->GetGUIDLow());
            pUser->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
            return;
        }

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_GIFT);

        stmt->setUInt32(0, item->GetGUIDLow());

        CharacterDatabase.Execute(stmt);
    }
    else
        pUser->SendLoot(item->GetGUID(), LOOT_CORPSE);
}

void WorldSession::HandleGameObjectUseOpcode(WorldPacket& recvData)
{
    uint64 guid;

    recvData >> guid;

    TC_LOG_DEBUG("network.opcode", "WORLD: Recvd CMSG_GAMEOBJ_USE Message [guid=%u]", GUID_LOPART(guid));

    // ignore for remote control state
    if (_player->m_mover != _player)
        return;

    if (GameObject* obj = GetPlayer()->GetMap()->GetGameObject(guid))
        obj->Use(_player);
}

void WorldSession::HandleGameobjectReportUse(WorldPacket& recvPacket)
{
    uint64 guid;
    recvPacket >> guid;

    TC_LOG_DEBUG("network.opcode", "WORLD: Recvd CMSG_GAMEOBJ_REPORT_USE Message [in game guid: %u]", GUID_LOPART(guid));

    // ignore for remote control state
    if (_player->m_mover != _player)
        return;

    GameObject* go = GetPlayer()->GetMap()->GetGameObject(guid);
    if (!go)
        return;

    if (!go->IsWithinDistInMap(_player, INTERACTION_DISTANCE))
        return;

    /*if (go->AI()->GossipHello(_player))
        return;*/

    _player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_USE_GAMEOBJECT, go->GetEntry());
}

void WorldSession::HandleCastSpellOpcode(WorldPacket& recvPacket)
{
    uint32 spellId, glyphIndex;
    uint8  castCount, castFlags;

    if (recvPacket.empty())
        return;


    WorldPacket const copy_packet = recvPacket;

    recvPacket >> castCount;    //This is the Spell's "GUID-Per-Session". An ability will stay lit unless it's castcount and ID are replied to.
    recvPacket >> spellId;
    recvPacket >> glyphIndex;
    recvPacket >> castFlags;

    
        if (auto u = _player->ToUnit())
        if (u->m_pendingCasts.size())
            if (auto info = sSpellMgr->GetSpellInfo(spellId))
            {
                auto category = info->StartRecoveryCategory;
                auto request = u->GetCastRequest(info->StartRecoveryCategory);
                if (request)
                {
                    if (request->cancel_in_progress)
                        if (request->spell_id == spellId)
                        {
                            Spell* spell = new Spell(u, info, TRIGGERED_NONE, 0, false);
                            spell->m_cast_count = castCount;                       // set count of casts
                            spell->m_glyphIndex = glyphIndex;
                            spell->SendCastResult(SPELL_FAILED_DONT_REPORT);
                            spell->finish(false);
                            recvPacket.rfinish();
                            return;
                        }
                }
            }

    if (_player)
    if (auto u = _player->ToUnit())
        if (auto info = sSpellMgr->GetSpellInfo(spellId))
            if (!u->CanExecutePendingSpellCastRequest(info, true))
                if (u->CanRequestSpellCast(info))
                {
                    PendingSpellCastRequest new_request
                    {
                        /*                        
                            uint32      spell_id;
                            uint32      time_requested;
                            bool        active;
                            WorldPacket request_packet;
                            bool        cancel_in_progress = false;
                            uint32      cast_count;
                        */
                        {spellId},
                        {recvPacket.ReadPackedTime()},
                        {true},
                        {copy_packet},
                        {false},
                        {castCount}
                    };

                    u->RequestSpellCast(new_request, info);
                    return;
                }


    //TC_LOG_DEBUG("network.opcode", "WORLD: got cast spell packet, castCount: %u, spellId: %u, castFlags: %u, data length = %u", castCount, spellId, castFlags, (uint32)recvPacket.size());

    // ignore for remote control state (for player case)
    Unit* mover = _player->m_mover;
    if (mover != _player && mover->GetTypeId() == TYPEID_PLAYER)
    {
        recvPacket.rfinish(); // prevent spam at ignore packet
        return;
    }

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
    {
        TC_LOG_ERROR("network.opcode", "WORLD: unknown spell id %u", spellId);
        recvPacket.rfinish(); // prevent spam at ignore packet
        return;
    }

    if (spellInfo->IsPassive())
    {
        recvPacket.rfinish(); // prevent spam at ignore packet
        return;
    }

    Unit* caster = mover;

    if (caster->GetTypeId() == TYPEID_UNIT && !caster->ToCreature()->HasSpell(spellId))
    {
        // If the vehicle creature does not have the spell but it allows the passenger to cast own spells
        // change caster to player and let him cast
        if (!_player->IsOnVehicle(caster) || spellInfo->CheckVehicle(_player) != SPELL_CAST_OK)
        {
            recvPacket.rfinish(); // prevent spam at ignore packet
            return;
        }

        caster = _player;
    }

    bool trig = false;
    Unit::AuraEffectList const& auras = caster->GetAuraEffectsByType(SPELL_AURA_PERIODIC_TRIGGER_SPELL_BY_CLIENT);
    for (auto i = auras.begin(); i != auras.end(); ++i)
    {
        if ((*i)->GetSpellInfo()->Effects[(*i)->GetEffIndex()].TriggerSpell == spellId)
        {
            trig = true;
            break;
        }
    }

    bool forceCast = false;
    Unit::AuraEffectList const& aur = caster->GetAuraEffectsByType(SPELL_AURA_MOD_NEXT_SPELL);
    for (auto i = aur.begin(); i != aur.end(); ++i)
    {
        if ((*i)->GetSpellInfo()->Effects[(*i)->GetEffIndex()].TriggerSpell == spellId)
        {
            forceCast = true;
            break;
        }
    }

    if (caster->GetTypeId() == TYPEID_PLAYER && !caster->ToPlayer()->HasActiveSpell(spellId) && !(spellInfo->AttributesEx8 & SPELL_ATTR8_RAID_MARKER) && !trig &&
        !forceCast)
    {
        // Archeology craft artifacts
        if (Player* player = mover->ToPlayer())
        {
            if (player->HasSkill(SKILL_ARCHAEOLOGY))
                for (uint32 i = 9; i < sResearchProjectStore.GetNumRows(); i++)
                    if (ResearchProjectEntry* rp = sResearchProjectStore.LookupRow(i))
                        if (rp->spellId == spellId)
                        {
                            player->GetArcheologyMgr().CompleteArtifact(rp->id, rp->spellId, recvPacket);
                            recvPacket.rfinish();
                            return;
                        }
        }

        // not have spell in spellbook
        recvPacket.rfinish(); // prevent spam at ignore packet
        return;
    }

    Unit::AuraEffectList swaps = mover->GetAuraEffectsByType(SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS);
    Unit::AuraEffectList const& swaps2 = mover->GetAuraEffectsByType(SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS_2);
    if (!swaps2.empty())
        swaps.insert(swaps.end(), swaps2.begin(), swaps2.end());

    if (!swaps.empty())
    {
        for (auto itr = swaps.begin(); itr != swaps.end(); ++itr)
        {
            if ((*itr)->IsAffectingSpell(spellInfo))
            {
                if (SpellInfo const* newInfo = sSpellMgr->GetSpellInfo((*itr)->GetAmount()))
                {
                    spellInfo = newInfo;
                    spellId = newInfo->Id;
                }
                break;
            }
        }
    }

    // Client is resending autoshot cast opcode when other spell is casted during shoot rotation
    // Skip it to prevent "interrupt" message
    if (spellInfo->IsAutoRepeatRangedSpell() && caster->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL)
        && caster->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL)->m_spellInfo == spellInfo)
    {
        recvPacket.rfinish();
        return;
    }

    // can't use our own spells when we're in possession of another unit,
    if (_player->isPossessing())
    {
        recvPacket.rfinish(); // prevent spam at ignore packet
        return;
    }

    if (spellInfo->Id == 605 && caster->HasUnitState(UNIT_STATE_CASTING))
    {
        recvPacket.rfinish();
        return; // hackfix to wierd recast while casting bug
    }

    // client provided targets
    SpellCastTargets targets;
    targets.Read(recvPacket, caster);
    HandleClientCastFlags(recvPacket, castFlags, targets);

    // auto-selection buff level base at target level (in spellInfo)
    if (targets.GetUnitTarget())
    {
        SpellInfo const* actualSpellInfo = spellInfo->GetAuraRankForLevel(targets.GetUnitTarget()->getLevel());

        // if rank not found then function return NULL but in explicit cast case original spell can be casted and later failed with appropriate error message
        if (actualSpellInfo)
            spellInfo = actualSpellInfo;
    }

    Spell* spell = new Spell(caster, spellInfo, trig ? TRIGGERED_FULL_MASK : TRIGGERED_NONE, 0, false);
    spell->m_cast_count = castCount;                       // set count of casts

    if (auto p = _player)
        if (auto info = sSpellMgr->GetSpellInfo(spellId))
            if (auto req = p->GetCastRequest(info->StartRecoveryCategory))
            {
                //TC_LOG_ERROR("sql.sql", "req count: %u, cast count: %u", req->cast_count, castCount);
                if (req->cast_count == castCount)
                    spell->m_requestedTime = req->time_requested;
            }
            else
            {
                //TC_LOG_ERROR("sql.sql", "request category %u does not exist.", info->StartRecoveryCategory);
            }

    spell->m_glyphIndex = glyphIndex;
    spell->prepare(&targets);
}

void WorldSession::HandleCancelCastOpcode(WorldPacket& recvPacket)
{
    uint32 spellId;

    recvPacket.read_skip<uint8>();                          // counter, increments with every CANCEL packet, don't use for now
    recvPacket >> spellId;

    if (_player->IsNonMeleeSpellCasted(false))
        _player->InterruptNonMeleeSpells(false, spellId, false);
}

void WorldSession::HandleCancelAuraOpcode(WorldPacket& recvPacket)
{
    uint32 spellId;
    recvPacket >> spellId;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return;

    // not allow remove spells with attr SPELL_ATTR0_CANT_CANCEL
    if (spellInfo->Attributes & SPELL_ATTR0_CANT_CANCEL)
        return;

    // channeled spell case (it currently casted then)
    if (spellInfo->IsChanneled())
    {
        if (Spell* curSpell = _player->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            if (curSpell->m_spellInfo->Id == spellId)
                _player->InterruptSpell(CURRENT_CHANNELED_SPELL);
        return;
    }

    // non channeled case:
    // don't allow remove non positive spells
    // don't allow cancelling passive auras (some of them are visible)
    if (!spellInfo->IsPositive() || spellInfo->IsPassive())
        return;

    // maybe should only remove one buff when there are multiple?
    _player->RemoveOwnedAura(spellId, 0, 0, AURA_REMOVE_BY_CANCEL);
}

void WorldSession::HandlePetCancelAuraOpcode(WorldPacket& recvPacket)
{
    uint64 guid;
    uint32 spellId;

    recvPacket >> guid;
    recvPacket >> spellId;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
    {
        TC_LOG_ERROR("network.opcode", "WORLD: unknown PET spell id %u", spellId);
        return;
    }

    Creature* pet=ObjectAccessor::GetCreatureOrPetOrVehicle(*_player, guid);

    if (!pet)
    {
        TC_LOG_ERROR("network.opcode", "HandlePetCancelAura: Attempt to cancel an aura for non-existant pet %u by player '%s'", uint32(GUID_LOPART(guid)), GetPlayer()->GetName().c_str());
        return;
    }

    if (pet != GetPlayer()->GetGuardianPet() && pet != GetPlayer()->GetCharm())
    {
        TC_LOG_ERROR("network.opcode", "HandlePetCancelAura: Pet %u is not a pet of player '%s'", uint32(GUID_LOPART(guid)), GetPlayer()->GetName().c_str());
        return;
    }

    if (!pet->isAlive())
    {
        pet->SendPetActionFeedback(FEEDBACK_PET_DEAD);
        return;
    }

    pet->RemoveOwnedAura(spellId, 0, 0, AURA_REMOVE_BY_CANCEL);

    pet->AddSpellAndCategoryCooldowns(spellInfo, 0);
}

void WorldSession::HandleCancelGrowthAuraOpcode(WorldPacket& /*recvPacket*/)
{
}
void WorldSession::HandleCancelQueuedSpellOpcode(WorldPacket& /*recvPacket*/)
{
    if (auto p = GetPlayer())
    {
        if (_player->m_pendingCasts.size())
            for (auto i = _player->m_pendingCasts.begin(); i != _player->m_pendingCasts.end(); ++i)
                if (auto category = i->first)
                {
                    _player->CancelPendingCastRequest(category);
                }

        p->m_pendingCasts.clear();
    }
}
void WorldSession::HandleCancelAutoRepeatSpellOpcode(WorldPacket& /*recvPacket*/)
{
    // may be better send SMSG_CANCEL_AUTO_REPEAT?
    // cancel and prepare for deleting
    _player->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
}

void WorldSession::HandleCancelChanneling(WorldPacket& recvData)
{
    uint32 spellid;
    recvData >> spellid;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellid);
    if (!spellInfo)
        return;

    // not allow remove spells with attr SPELL_ATTR0_CANT_CANCEL
    if (spellInfo->Attributes & SPELL_ATTR0_CANT_CANCEL)
        return;

    // ignore for remote control state (for player case)
    Unit* mover = _player->m_mover;
    if (mover != _player && mover->GetTypeId() == TYPEID_PLAYER)
        return;

    mover->InterruptSpell(CURRENT_CHANNELED_SPELL);
}

void WorldSession::HandleTotemDestroyed(WorldPacket& recvPacket)
{
    // ignore for remote control state
    if (_player->m_mover != _player)
        return;

    uint8 slotId;
    uint64 guid;
    recvPacket >> slotId;
    recvPacket >> guid;

    ++slotId;
    if (slotId >= MAX_TOTEM_SLOT)
        return;

    if (!_player->m_SummonSlot[slotId])
        return;

    Creature* totem = GetPlayer()->GetMap()->GetCreature(_player->m_SummonSlot[slotId]);
    if (totem && totem->isTotem() && totem->GetGUID() == guid)
        totem->ToTotem()->UnSummon();
}

void WorldSession::HandleSelfResOpcode(WorldPacket& /*recvData*/)
{
    TC_LOG_DEBUG("network.opcode", "WORLD: CMSG_SELF_RES");                  // empty opcode

    if (_player->HasAuraType(SPELL_AURA_PREVENT_RESURRECTION))
        return; // silent return, client should display error by itself and not send this opcode

    if (_player->GetUInt32Value(PLAYER_SELF_RES_SPELL))
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(_player->GetUInt32Value(PLAYER_SELF_RES_SPELL));
        if (spellInfo)
            _player->CastSpell(_player, spellInfo, false, 0);

        _player->SetUInt32Value(PLAYER_SELF_RES_SPELL, 0);
    }
}

void WorldSession::HandleSpellClick(WorldPacket& recvData)
{
    uint64 guid;
    recvData >> guid;

    // this will get something not in world. crash
    Creature* unit = ObjectAccessor::GetCreatureOrPetOrVehicle(*_player, guid);

    if (!unit)
        return;

    // TODO: Unit::SetCharmedBy: 28782 is not in world but 0 is trying to charm it! -> crash
    if (!unit->IsInWorld())
        return;

    unit->HandleSpellClick(_player);
}

void WorldSession::HandleMirrorImageDataRequest(WorldPacket& recvData)
{
    TC_LOG_DEBUG("network.opcode", "WORLD: CMSG_GET_MIRRORIMAGE_DATA");
    uint64 guid;
    recvData >> guid;
    recvData.read_skip<uint32>(); // DisplayId ?

    // Get unit for which data is needed by client
    Unit* unit = ObjectAccessor::GetObjectInWorld(guid, (Unit*)NULL);
    if (!unit)
        return;

    if (!unit->HasAuraType(SPELL_AURA_CLONE_CASTER))
        return;

    // Get creator of the unit (SPELL_AURA_CLONE_CASTER does not stack)
    Unit* creator = unit->GetAuraEffectsByType(SPELL_AURA_CLONE_CASTER).front()->GetCaster();
    if (!creator)
        return;

    WorldPacket data(SMSG_MIRRORIMAGE_DATA, 68);
    data << uint64(guid);
    data << uint32(creator->GetDisplayId());
    data << uint8(creator->getRace());
    data << uint8(creator->getGender());
    data << uint8(creator->getClass());

    if (creator->GetTypeId() == TYPEID_PLAYER)
    {
        Player* player = creator->ToPlayer();
        Guild* guild = NULL;

        if (uint32 guildId = player->GetGuildId())
            guild = sGuildMgr->GetGuildById(guildId);

        data << uint8(player->GetByteValue(PLAYER_BYTES, 0));   // skin
        data << uint8(player->GetByteValue(PLAYER_BYTES, 1));   // face
        data << uint8(player->GetByteValue(PLAYER_BYTES, 2));   // hair
        data << uint8(player->GetByteValue(PLAYER_BYTES, 3));   // haircolor
        data << uint8(player->GetByteValue(PLAYER_BYTES_2, 0)); // facialhair
        data << uint64(guild ? guild->GetGUID() : 0);

        static EquipmentSlots const itemSlots[] =
        {
            EQUIPMENT_SLOT_HEAD,
            EQUIPMENT_SLOT_SHOULDERS,
            EQUIPMENT_SLOT_BODY,
            EQUIPMENT_SLOT_CHEST,
            EQUIPMENT_SLOT_WAIST,
            EQUIPMENT_SLOT_LEGS,
            EQUIPMENT_SLOT_FEET,
            EQUIPMENT_SLOT_WRISTS,
            EQUIPMENT_SLOT_HANDS,
            EQUIPMENT_SLOT_BACK,
            EQUIPMENT_SLOT_TABARD,
            EQUIPMENT_SLOT_END
        };

        // Display items in visible slots
        for (EquipmentSlots const* itr = &itemSlots[0]; *itr != EQUIPMENT_SLOT_END; ++itr)
        {
            if (*itr == EQUIPMENT_SLOT_HEAD && player->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM))
                data << uint32(0);
            else if (*itr == EQUIPMENT_SLOT_BACK && player->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_CLOAK))
                data << uint32(0);
            else if (Item const* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, *itr))
            {
                if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(item->GetVisibleEntry()))
                    data << uint32(itemTemplate->DisplayInfoID);
            }
            else
                data << uint32(0);
        }
    }
    else
    {
        // Skip player data for creatures
        data << uint8(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
    }

    SendPacket(&data);
}

void WorldSession::HandleUpdateProjectilePosition(WorldPacket& recvPacket)
{
    TC_LOG_DEBUG("network.opcode", "WORLD: CMSG_UPDATE_PROJECTILE_POSITION");

    uint64 casterGuid;
    uint32 spellId;
    uint8 castCount;
    float x, y, z;    // Position of missile hit

    recvPacket >> casterGuid;
    recvPacket >> spellId;
    recvPacket >> castCount;
    recvPacket >> x;
    recvPacket >> y;
    recvPacket >> z;

    Unit* caster = ObjectAccessor::GetUnit(*_player, casterGuid);
    if (!caster)
        return;

    Spell* spell = caster->FindCurrentSpellBySpellId(spellId);
    if (!spell || !spell->m_targets.HasDst())
        return;

    Position pos = *spell->m_targets.GetDstPos();
    pos.Relocate(x, y, z);
    spell->m_targets.ModDst(pos);

    WorldPacket data(SMSG_SET_PROJECTILE_POSITION, 21);
    data << uint64(casterGuid);
    data << uint8(castCount);
    data << float(x);
    data << float(y);
    data << float(z);
    caster->SendMessageToSet(&data, true);
}

void WorldSession::HandleRequestCategoryCooldown(WorldPacket& recvPacket)
{
    std::map<uint32, int32> categoryMods;
    Unit::AuraEffectList const& categoryCooldownAuras = _player->GetAuraEffectsByType(SPELL_AURA_MOD_SPELL_CATEGORY_COOLDOWN);
    for (auto itr = categoryCooldownAuras.begin(); itr != categoryCooldownAuras.end(); ++itr)
    {
        std::map<uint32, int32>::iterator cItr = categoryMods.find((*itr)->GetMiscValue());
        if (cItr == categoryMods.end())
            categoryMods[(*itr)->GetMiscValue()] = (*itr)->GetAmount();
        else
            cItr->second += (*itr)->GetAmount();
    }

    WorldPacket data(SMSG_SPELL_CATEGORY_COOLDOWN, 11);
    data.WriteBits(categoryMods.size(), 23);
    data.FlushBits();
    for (auto itr = categoryMods.begin(); itr != categoryMods.end(); ++itr)
    {
        data << uint32(itr->first);
        data << int32(-itr->second);
    }

    SendPacket(&data);
    recvPacket.rfinish();
}
