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
#include "DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Opcodes.h"
#include "Log.h"
#include "UpdateMask.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Pet.h"
#include "Unit.h"
#include "Totem.h"
#include "Spell.h"
#include "DynamicObject.h"
#include "Group.h"
#include "UpdateData.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "CellImpl.h"
#include "SharedDefines.h"
#include "LootMgr.h"
#include "VMapFactory.h"
#include "Battleground.h"
#include "Util.h"
#include "TemporarySummon.h"
#include "Vehicle.h"
#include "SpellAuraEffects.h"
#include "ScriptMgr.h"
#include "ConditionMgr.h"
#include "DisableMgr.h"
#include "SpellScript.h"
#include "InstanceScript.h"
#include "SpellInfo.h"
#include "DB2Stores.h"
#include "Battlefield.h"
#include "BattlefieldMgr.h"
#include "Guild.h"
#include "GameObjectAI.h"
#include "ArenaTeam.h"
#include "ChallengeModeMgr.h"
#include "Transport.h"

extern pEffect SpellEffects[TOTAL_SPELL_EFFECTS];

SpellDestination::SpellDestination()
{
    _position.Relocate(0, 0, 0, 0);
    _transportGUID = 0;
    _transportOffset.Relocate(0, 0, 0, 0);
}

SpellDestination::SpellDestination(float x, float y, float z, float orientation, uint32 mapId)
{
    _position.Relocate(x, y, z, orientation);
    _transportGUID = 0;
    _position.m_mapId = mapId;
}

SpellDestination::SpellDestination(Position const& pos)
{
    _position.Relocate(pos);
    _transportGUID = 0;
}

SpellDestination::SpellDestination(WorldObject const& wObj)
{
    _transportGUID = 0;
    if (wObj.GetTransport())
        _transportGUID = wObj.GetTransport()->GetGUID();

    _transportOffset.Relocate(wObj.GetTransOffsetX(), wObj.GetTransOffsetY(), wObj.GetTransOffsetZ(), wObj.GetTransOffsetO());
    _position.Relocate(wObj);
}

SpellCastTargets::SpellCastTargets() : m_elevation(0), m_speed(0), m_strTarget()
{
    m_objectTarget = NULL;
    m_itemTarget = NULL;

    m_objectTargetGUID   = 0;
    m_itemTargetGUID   = 0;
    m_itemTargetEntry  = 0;

    m_targetMask = 0;
}

SpellCastTargets::~SpellCastTargets()
{
}

void SpellCastTargets::Read(ByteBuffer& data, Unit* caster)
{
    data >> m_targetMask;

    if (m_targetMask == TARGET_FLAG_NONE)
        return;

    if (m_targetMask & (TARGET_FLAG_UNIT | TARGET_FLAG_UNIT_MINIPET | TARGET_FLAG_GAMEOBJECT | TARGET_FLAG_CORPSE_ENEMY | TARGET_FLAG_CORPSE_ALLY))
        data.readPackGUID(m_objectTargetGUID);

    if (m_targetMask & (TARGET_FLAG_ITEM | TARGET_FLAG_TRADE_ITEM))
        data.readPackGUID(m_itemTargetGUID);

    if (m_targetMask & TARGET_FLAG_SOURCE_LOCATION)
    {
        data.readPackGUID(m_src._transportGUID);
        if (m_src._transportGUID)
            data >> m_src._transportOffset.PositionXYZStream();
        else
            data >> m_src._position.PositionXYZStream();
    }
    else
    {
        m_src._transportGUID = caster->GetTransGUID();
        if (m_src._transportGUID)
            m_src._transportOffset.Relocate(caster->GetTransOffsetX(), caster->GetTransOffsetY(), caster->GetTransOffsetZ(), caster->GetTransOffsetO());
        else
            m_src._position.Relocate(caster);
    }

    if (m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        data.readPackGUID(m_dst._transportGUID);
        if (m_dst._transportGUID)
            data >> m_dst._transportOffset.PositionXYZStream();
        else
            data >> m_dst._position.PositionXYZStream();
    }
    else
    {
        m_dst._transportGUID = caster->GetTransGUID();
        if (m_dst._transportGUID)
            m_dst._transportOffset.Relocate(caster->GetTransOffsetX(), caster->GetTransOffsetY(), caster->GetTransOffsetZ(), caster->GetTransOffsetO());
        else
            m_dst._position.Relocate(caster);
    }

    if (m_targetMask & TARGET_FLAG_STRING)
        data >> m_strTarget;

    Update(caster);
}

void SpellCastTargets::Write(ByteBuffer& data)
{
    data << uint32(m_targetMask);

    if (m_targetMask & (TARGET_FLAG_UNIT | TARGET_FLAG_CORPSE_ALLY | TARGET_FLAG_GAMEOBJECT | TARGET_FLAG_CORPSE_ENEMY | TARGET_FLAG_UNIT_MINIPET))
        data.appendPackGUID(m_objectTargetGUID);

    if (m_targetMask & (TARGET_FLAG_ITEM | TARGET_FLAG_TRADE_ITEM))
    {
        if (m_itemTarget)
            data.append(m_itemTarget->GetPackGUID());
        else
            data << uint8(0);
    }

    if (m_targetMask & TARGET_FLAG_SOURCE_LOCATION)
    {
        data.appendPackGUID(m_src._transportGUID); // relative position guid here - transport for example
        if (m_src._transportGUID)
            data << m_src._transportOffset.PositionXYZStream();
        else
            data << m_src._position.PositionXYZStream();
    }

    if (m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        data.appendPackGUID(m_dst._transportGUID); // relative position guid here - transport for example
        if (m_dst._transportGUID)
            data << m_dst._transportOffset.PositionXYZStream();
        else
            data << m_dst._position.PositionXYZStream();
    }

    if (m_targetMask & TARGET_FLAG_STRING)
        data << m_strTarget;
}

uint64 SpellCastTargets::GetUnitTargetGUID() const
{
    switch (GUID_HIPART(m_objectTargetGUID))
    {
        case HIGHGUID_PLAYER:
        case HIGHGUID_VEHICLE:
        case HIGHGUID_UNIT:
        case HIGHGUID_PET:
            return m_objectTargetGUID;
        default:
            return 0LL;
    }
}

Unit* SpellCastTargets::GetUnitTarget() const
{
    if (m_objectTarget)
        return m_objectTarget->ToUnit();
    return NULL;
}

void SpellCastTargets::SetUnitTarget(Unit* target)
{
    if (!target)
        return;

    m_objectTarget = target;
    m_objectTargetGUID = target->GetGUID();
    m_targetMask |= TARGET_FLAG_UNIT;
}

uint64 SpellCastTargets::GetGOTargetGUID() const
{
    switch (GUID_HIPART(m_objectTargetGUID))
    {
        case HIGHGUID_TRANSPORT:
        case HIGHGUID_GAMEOBJECT:
            return m_objectTargetGUID;
        default:
            return 0LL;
    }
}

GameObject* SpellCastTargets::GetGOTarget() const
{
    if (m_objectTarget)
        return m_objectTarget->ToGameObject();
    return NULL;
}


void SpellCastTargets::SetGOTarget(GameObject* target)
{
    if (!target)
        return;

    m_objectTarget = target;
    m_objectTargetGUID = target->GetGUID();
    m_targetMask |= TARGET_FLAG_GAMEOBJECT;
}

uint64 SpellCastTargets::GetCorpseTargetGUID() const
{
    switch (GUID_HIPART(m_objectTargetGUID))
    {
        case HIGHGUID_CORPSE:
            return m_objectTargetGUID;
        default:
            return 0LL;
    }
}

Corpse* SpellCastTargets::GetCorpseTarget() const
{
    if (m_objectTarget)
        return m_objectTarget->ToCorpse();
    return NULL;
}

WorldObject* SpellCastTargets::GetObjectTarget() const
{
    return m_objectTarget;
}

uint64 SpellCastTargets::GetObjectTargetGUID() const
{
    return m_objectTargetGUID;
}

void SpellCastTargets::RemoveObjectTarget()
{
    m_objectTarget = NULL;
    m_objectTargetGUID = 0LL;
    m_targetMask &= ~(TARGET_FLAG_UNIT_MASK | TARGET_FLAG_CORPSE_MASK | TARGET_FLAG_GAMEOBJECT_MASK);
}

void SpellCastTargets::SetItemTarget(Item* item)
{
    if (!item)
        return;

    m_itemTarget = item;
    m_itemTargetGUID = item->GetGUID();
    m_itemTargetEntry = item->GetEntry();
    m_targetMask |= TARGET_FLAG_ITEM;
}

void SpellCastTargets::SetTradeItemTarget(Player* caster)
{
    m_itemTargetGUID = uint64(TRADE_SLOT_NONTRADED);
    m_itemTargetEntry = 0;
    m_targetMask |= TARGET_FLAG_TRADE_ITEM;

    Update(caster);
}

void SpellCastTargets::UpdateTradeSlotItem()
{
    if (m_itemTarget && (m_targetMask & TARGET_FLAG_TRADE_ITEM))
    {
        m_itemTargetGUID = m_itemTarget->GetGUID();
        m_itemTargetEntry = m_itemTarget->GetEntry();
    }
}

SpellDestination const* SpellCastTargets::GetSrc() const
{
    return &m_src;
}

Position const* SpellCastTargets::GetSrcPos() const
{
    return &m_src._position;
}

void SpellCastTargets::SetSrc(float x, float y, float z)
{
    m_src = SpellDestination(x, y, z);
    m_targetMask |= TARGET_FLAG_SOURCE_LOCATION;
}

void SpellCastTargets::SetSrc(Position const& pos)
{
    m_src = SpellDestination(pos);
    m_targetMask |= TARGET_FLAG_SOURCE_LOCATION;
}

void SpellCastTargets::SetSrc(WorldObject const& wObj)
{
    m_src = SpellDestination(wObj);
    m_targetMask |= TARGET_FLAG_SOURCE_LOCATION;
}

void SpellCastTargets::ModSrc(Position const& pos)
{
    ASSERT(m_targetMask & TARGET_FLAG_SOURCE_LOCATION);

    if (m_src._transportGUID)
    {
        Position offset;
        m_src._position.GetPositionOffsetTo(pos, offset);
        m_src._transportOffset.RelocateOffset(offset);
    }
    m_src._position.Relocate(pos);
}

void SpellCastTargets::RemoveSrc()
{
    m_targetMask &= ~(TARGET_FLAG_SOURCE_LOCATION);
}

SpellDestination const* SpellCastTargets::GetDst() const
{
    return &m_dst;
}

WorldLocation const* SpellCastTargets::GetDstPos() const
{
    return &m_dst._position;
}

void SpellCastTargets::SetDst(float x, float y, float z, float orientation, uint32 mapId)
{
    m_dst = SpellDestination(x, y, z, orientation, mapId);
    m_targetMask |= TARGET_FLAG_DEST_LOCATION;
}

void SpellCastTargets::SetDst(Position const& pos)
{
    m_dst = SpellDestination(pos);
    m_targetMask |= TARGET_FLAG_DEST_LOCATION;
}

void SpellCastTargets::SetDst(WorldObject const& wObj)
{
    m_dst = SpellDestination(wObj);
    m_targetMask |= TARGET_FLAG_DEST_LOCATION;
}

void SpellCastTargets::SetDst(SpellCastTargets const& spellTargets)
{
    m_dst = spellTargets.m_dst;
    m_targetMask |= TARGET_FLAG_DEST_LOCATION;
}

void SpellCastTargets::ModDst(Position const& pos)
{
    ASSERT(m_targetMask & TARGET_FLAG_DEST_LOCATION);

    if (m_dst._transportGUID)
    {
        Position offset;
        m_dst._position.GetPositionOffsetTo(pos, offset);
        m_dst._transportOffset.RelocateOffset(offset);
    }
    m_dst._position.Relocate(pos);
}

void SpellCastTargets::RemoveDst()
{
    m_targetMask &= ~(TARGET_FLAG_DEST_LOCATION);
}

void SpellCastTargets::Update(Unit* caster)
{
    m_objectTarget = m_objectTargetGUID ? ((m_objectTargetGUID == caster->GetGUID()) ? caster : ObjectAccessor::GetWorldObject(*caster, m_objectTargetGUID)) : NULL;

    m_itemTarget = NULL;
    if (caster->GetTypeId() == TYPEID_PLAYER)
    {
        Player* player = caster->ToPlayer();
        if (m_targetMask & TARGET_FLAG_ITEM)
            m_itemTarget = player->GetItemByGuid(m_itemTargetGUID);
        else if (m_targetMask & TARGET_FLAG_TRADE_ITEM)
            if (m_itemTargetGUID == TRADE_SLOT_NONTRADED) // here it is not guid but slot. Also prevents hacking slots
                if (TradeData* pTrade = player->GetTradeData())
                    m_itemTarget = pTrade->GetTraderData()->GetItem(TRADE_SLOT_NONTRADED);

        if (m_itemTarget)
            m_itemTargetEntry = m_itemTarget->GetEntry();
    }

    // update positions by transport move
    if (HasSrc() && m_src._transportGUID)
    {
        if (WorldObject* transport = ObjectAccessor::GetWorldObject(*caster, m_src._transportGUID))
        {
            m_src._position.Relocate(transport);
            m_src._position.RelocateOffset(m_src._transportOffset);
        }
    }

    if (HasDst() && m_dst._transportGUID)
    {
        if (WorldObject* transport = ObjectAccessor::GetWorldObject(*caster, m_dst._transportGUID))
        {
            m_dst._position.Relocate(transport);
            m_dst._position.RelocateOffset(m_dst._transportOffset);
        }
    }
}

void SpellCastTargets::OutDebug() const
{
    if (!m_targetMask)
        TC_LOG_INFO("spells", "No targets");

    TC_LOG_INFO("spells", "target mask: %u", m_targetMask);
    if (m_targetMask & (TARGET_FLAG_UNIT_MASK | TARGET_FLAG_CORPSE_MASK | TARGET_FLAG_GAMEOBJECT_MASK))
        TC_LOG_INFO("spells", "Object target: " UI64FMTD, m_objectTargetGUID);
    if (m_targetMask & TARGET_FLAG_ITEM)
        TC_LOG_INFO("spells", "Item target: " UI64FMTD, m_itemTargetGUID);
    if (m_targetMask & TARGET_FLAG_TRADE_ITEM)
        TC_LOG_INFO("spells", "Trade item target: " UI64FMTD, m_itemTargetGUID);
    if (m_targetMask & TARGET_FLAG_SOURCE_LOCATION)
        TC_LOG_INFO("spells", "Source location: transport guid:" UI64FMTD " trans offset: %s position: %s", m_src._transportGUID, m_src._transportOffset.ToString().c_str(), m_src._position.ToString().c_str());
    if (m_targetMask & TARGET_FLAG_DEST_LOCATION)
        TC_LOG_INFO("spells", "Destination location: transport guid:" UI64FMTD " trans offset: %s position: %s", m_dst._transportGUID, m_dst._transportOffset.ToString().c_str(), m_dst._position.ToString().c_str());
    if (m_targetMask & TARGET_FLAG_STRING)
        TC_LOG_INFO("spells", "String: %s", m_strTarget.c_str());
    TC_LOG_INFO("spells", "speed: %f", m_speed);
    TC_LOG_INFO("spells", "elevation: %f", m_elevation);
}

SpellValue::SpellValue(SpellInfo const* proto)
{
    for (uint32 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        EffectBasePoints[i] = proto->Effects[i].BasePoints;
    MaxAffectedTargets = proto->MaxAffectedTargets;
    RadiusMod = 1.0f;
    AuraStackAmount = 1;
    Speed = proto->Speed;
    Duration = 0;
    MaxDuration = 0;
    CastTime = -1;
}

Spell::Spell(Unit* caster, SpellInfo const* info, TriggerCastFlags triggerFlags, uint64 originalCasterGUID, bool skipCheck) :
m_spellInfo(sSpellMgr->GetSpellForDifficultyFromSpell(info, caster)),
m_caster((info->AttributesEx6 & SPELL_ATTR6_CAST_BY_CHARMER && caster->GetCharmerOrOwner()) ? caster->GetCharmerOrOwner() : caster)
, m_spellValue(new SpellValue(m_spellInfo)), m_preGeneratedPath(PathGenerator(m_caster))
{
    m_customError = SPELL_CUSTOM_ERROR_NONE;
    m_skipCheck = skipCheck;
    m_selfContainer = NULL;
    m_referencedFromCurrentSpell = false;
    m_executedCurrently = false;
    m_needComboPoints = m_spellInfo->NeedsComboPoints();
    m_comboPointGain = 0;
    m_delayStart = 0;
    m_delayAtDamageCount = 0;

    m_applyMultiplierMask = 0;
    m_auraScaleMask = 0;

    m_levelCheck = false;

    // Get data for type of attack
    switch (m_spellInfo->DmgClass)
    {
        case SPELL_DAMAGE_CLASS_MELEE:
            if (m_spellInfo->AttributesEx3 & SPELL_ATTR3_REQ_OFFHAND)
                m_attackType = OFF_ATTACK;
            else
                m_attackType = BASE_ATTACK;
            break;
        case SPELL_DAMAGE_CLASS_RANGED:
            m_attackType = m_spellInfo->IsRangedWeaponSpell() ? RANGED_ATTACK : BASE_ATTACK;
            break;
        default:
                                                            // Wands
            if (m_spellInfo->AttributesEx2 & SPELL_ATTR2_AUTOREPEAT_FLAG)
                m_attackType = RANGED_ATTACK;
            else
                m_attackType = BASE_ATTACK;
            break;
    }

    m_spellSchoolMask = info->GetSchoolMask();           // Can be override for some spell (wand shoot for example)

    if (m_attackType == RANGED_ATTACK)
        // wand case
        if ((m_caster->getClassMask() & CLASSMASK_WAND_USERS) != 0 && m_caster->GetTypeId() == TYPEID_PLAYER)
            if (Item* pItem = m_caster->ToPlayer()->GetWeaponForAttack(RANGED_ATTACK))
                m_spellSchoolMask = SpellSchoolMask(1 << pItem->GetTemplate()->DamageType);

    if (originalCasterGUID)
        m_originalCasterGUID = originalCasterGUID;
    else
        m_originalCasterGUID = m_caster->GetGUID();

    if (m_originalCasterGUID == m_caster->GetGUID())
        m_originalCaster = m_caster;
    else
    {
        m_originalCaster = ObjectAccessor::GetUnit(*m_caster, m_originalCasterGUID);
        if (m_originalCaster && !m_originalCaster->IsInWorld())
            m_originalCaster = NULL;
    }

    m_spellState = SPELL_STATE_NULL;
    _triggeredCastFlags = triggerFlags;
    if (info->AttributesEx4 & SPELL_ATTR4_TRIGGERED)
        _triggeredCastFlags = TRIGGERED_FULL_MASK;

    m_CastItem = NULL;
    m_castItemGUID = 0;

    _refreshedAura = false;

    unitTarget = NULL;
    itemTarget = NULL;
    gameObjTarget = NULL;
    focusObject = NULL;
    m_cast_count = 0;
    m_glyphIndex = 0;
    m_preCastSpell = 0;
    m_triggeredByAuraSpell  = NULL;
    _interruptedSpell = NULL;
    m_spellAura = NULL;
    m_changeBySoulburn = false;
    _redirected = false;
    _isReflected = false;
    m_costModified = false;

    //Auto Shot & Shoot (wand)
    m_autoRepeat = m_spellInfo->IsAutoRepeatRangedSpell();

    m_runesState = 0;
    m_powerCost = 0;                                        // setup to correct value in Spell::prepare, must not be used before.
    m_casttime = 0;                                         // setup to correct value in Spell::prepare, must not be used before.
    m_timer = 0;                                            // will set to castime in prepare

    m_delaytime = 0;
    m_channelTargetEffectMask = 0;

    // Determine if spell can be reflected back to the caster
    // Patch 1.2 notes: Spell Reflection no longer reflects abilities
    // 49560 (Deathgrip), is the only known exception that should be reflectable even though it's not magic.
    m_canReflect = (m_spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MAGIC || m_spellInfo->Id == 49560) && !(m_spellInfo->Attributes & SPELL_ATTR0_ABILITY)
        && !(m_spellInfo->AttributesEx & SPELL_ATTR1_CANT_BE_REFLECTED) && !(m_spellInfo->Attributes & SPELL_ATTR0_UNAFFECTED_BY_INVULNERABILITY)
        && !m_spellInfo->IsPassive() && !m_spellInfo->IsPositive() && !m_spellInfo->IsTargetingArea();

    CleanupTargetList();
    memset(m_effectExecuteData, 0, MAX_SPELL_EFFECTS * sizeof(ByteBuffer*));

    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        m_destTargets[i] = SpellDestination(*m_caster);
}

Spell::~Spell()
{
    UnloadScripts();

    if (m_referencedFromCurrentSpell && m_selfContainer && *m_selfContainer == this)
    {
        // Clean the reference to avoid later crash.
        // If this error is repeating, we may have to add an ASSERT to better track down how we get into this case.
        TC_LOG_ERROR("spells", "SPELL: deleting spell for spell ID %u. However, spell still referenced.", m_spellInfo->Id);
        *m_selfContainer = NULL;
    }

    if (m_caster && m_caster->GetTypeId() == TYPEID_PLAYER)
        ASSERT(m_caster->ToPlayer()->m_spellModTakingSpell != this);

    delete m_spellValue;

    CheckEffectExecuteData();
}

void Spell::InitExplicitTargets(SpellCastTargets const& targets)
{
    m_targets = targets;
    // this function tries to correct spell explicit targets for spell
    // client doesn't send explicit targets correctly sometimes - we need to fix such spells serverside
    // this also makes sure that we correctly send explicit targets to client (removes redundant data)
    uint32 neededTargets = m_spellInfo->GetExplicitTargetMask();

    if (WorldObject* target = m_targets.GetObjectTarget())
    {
        if (m_originalCaster && m_spellInfo->AttributesEx & SPELL_ATTR1_CHANNEL_TRACK_TARGET) // im sure this is not the good way to place it...
            if (m_originalCaster != target && m_originalCaster->GetTypeId() == TYPEID_UNIT)
                m_originalCaster->SetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT, target->GetGUID());
        // check if object target is valid with needed target flags
        // for unit case allow corpse target mask because player with not released corpse is a unit target
        if ((target->ToUnit() && !(neededTargets & (TARGET_FLAG_UNIT_MASK | TARGET_FLAG_CORPSE_MASK)))
            || (target->ToGameObject() && !(neededTargets & TARGET_FLAG_GAMEOBJECT_MASK))
            || (target->ToCorpse() && !(neededTargets & TARGET_FLAG_CORPSE_MASK)))
        {
            m_targets.RemoveObjectTarget();
        }
    }
    else
    {
        // try to select correct unit target if not provided by client or by serverside cast
        if (neededTargets & (TARGET_FLAG_UNIT_MASK))
        {
            Unit* unit = NULL;
            // try to use player selection as a target
            if (Player* playerCaster = m_caster->ToPlayer())
            {
                // selection has to be found and to be valid target for the spell
                if (Unit* selectedUnit = ObjectAccessor::GetUnit(*m_caster, playerCaster->GetSelection()))
                    if (m_spellInfo->CheckExplicitTarget(m_caster, selectedUnit) == SPELL_CAST_OK)
                        unit = selectedUnit;
            }
            // try to use attacked unit as a target
            else if ((m_caster->GetTypeId() == TYPEID_UNIT) && neededTargets & (TARGET_FLAG_UNIT_ENEMY | TARGET_FLAG_UNIT))
                unit = m_caster->getVictim();

            // didn't find anything - let's use self as target
            if (m_caster && !unit && neededTargets & (TARGET_FLAG_UNIT_RAID | TARGET_FLAG_UNIT_PARTY | TARGET_FLAG_UNIT_ALLY))
                unit = m_caster;

            m_targets.SetUnitTarget(unit);

            if (unit && m_originalCaster && m_spellInfo->AttributesEx & SPELL_ATTR1_CHANNEL_TRACK_TARGET) // im sure this is not the good way to place it...
                if (m_originalCaster != target && m_originalCaster->GetTypeId() == TYPEID_UNIT)
                    m_originalCaster->SetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT, unit->GetGUID());
        }
    }

    // check if spell needs dst target
    if (neededTargets & TARGET_FLAG_DEST_LOCATION)
    {
        // and target isn't set
        if (!m_targets.HasDst())
        {
            // try to use unit target if provided
            if (WorldObject* target = targets.GetObjectTarget())
                m_targets.SetDst(*target);
        }
    }
    else
        m_targets.RemoveDst();

    if (neededTargets & TARGET_FLAG_SOURCE_LOCATION)
    {
        if (!targets.HasSrc())
            m_targets.SetSrc(*m_caster);
    }
    else
        m_targets.RemoveSrc();
}

void Spell::SelectExplicitTargets()
{
    // here go all explicit target changes made to explicit targets after spell prepare phase is finished
    if (Unit* target = m_targets.GetUnitTarget())
    {
        // check for explicit target redirection, for Grounding Totem for example
        if (m_spellInfo->GetExplicitTargetMask() & TARGET_FLAG_UNIT_ENEMY
            || (m_spellInfo->GetExplicitTargetMask() & TARGET_FLAG_UNIT && !m_spellInfo->IsPositive()))
        {
            Unit* redirect;
            switch (m_spellInfo->DmgClass)
            {
                case SPELL_DAMAGE_CLASS_MAGIC:
                    redirect = m_caster->GetMagicHitRedirectTarget(target, m_spellInfo);
                    break;
                case SPELL_DAMAGE_CLASS_MELEE:
                case SPELL_DAMAGE_CLASS_RANGED:
                    redirect = m_caster->GetMeleeHitRedirectTarget(target, m_spellInfo);
                    break;
                default:
                    redirect = NULL;
                    break;
            }
            if (redirect && (redirect != target))
            {
                m_targets.SetUnitTarget(redirect);
                _redirected = true;
            }
        }
    }
}

void Spell::SelectSpellTargets()
{
    // select targets for cast phase
    SelectExplicitTargets();

    uint32 processedAreaEffectsMask = 0;
    for (uint32 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        // not call for empty effect.
        // Also some spells use not used effect targets for store targets for dummy effect in triggered spells
        if (!m_spellInfo->Effects[i].IsEffect())
            continue;

        // set expected type of implicit targets to be sent to client
        uint32 implicitTargetMask = GetTargetFlagMask(m_spellInfo->Effects[i].TargetA.GetObjectType()) | GetTargetFlagMask(m_spellInfo->Effects[i].TargetB.GetObjectType());
        if (implicitTargetMask & TARGET_FLAG_UNIT)
            m_targets.SetTargetFlag(TARGET_FLAG_UNIT);
        if (implicitTargetMask & (TARGET_FLAG_GAMEOBJECT | TARGET_FLAG_GAMEOBJECT_ITEM))
            m_targets.SetTargetFlag(TARGET_FLAG_GAMEOBJECT);

        SelectEffectImplicitTargets(SpellEffIndex(i), m_spellInfo->Effects[i].TargetA, processedAreaEffectsMask);
        SelectEffectImplicitTargets(SpellEffIndex(i), m_spellInfo->Effects[i].TargetB, processedAreaEffectsMask);

        // Select targets of effect based on effect type
        // those are used when no valid target could be added for spell effect based on spell target type
        // some spell effects use explicit target as a default target added to target map (like SPELL_EFFECT_LEARN_SPELL)
        // some spell effects add target to target map only when target type specified (like SPELL_EFFECT_WEAPON)
        // some spell effects don't add anything to target map (confirmed with sniffs) (like SPELL_EFFECT_DESTROY_ALL_TOTEMS)
        SelectEffectTypeImplicitTargets(i);

        if (m_targets.HasDst())
            AddDestTarget(*m_targets.GetDst(), i);

        //if (m_spellInfo->IsChanneled())
        //{
        //    uint8 mask = (1 << i);
        //    for (auto ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
        //    {
        //        if (ihit->effectMask & mask)
        //        {
        //            m_channelTargetEffectMask |= mask;
        //            break;
        //        }
        //    }
        //}
        //else if (m_auraScaleMask)
        //{
        //    bool checkLvl = !m_UniqueTargetInfo.empty();
        //    for (auto ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end();)
        //    {
        //        // remove targets which did not pass min level check
        //        if (m_auraScaleMask && ihit->effectMask == m_auraScaleMask)
        //        {
        //            // Do not check for selfcast
        //            if (!ihit->scaleAura && ihit->targetGUID != m_caster->GetGUID())
        //            {
        //                 m_UniqueTargetInfo.erase(ihit++);
        //                 continue;
        //            }
        //        }
        //        ++ihit;
        //    }
        //    if (checkLvl && m_UniqueTargetInfo.empty())
        //    {
        //        SendCastResult(SPELL_FAILED_LOWLEVEL);
        //        finish(false);
        //    }
        //}
    }

    FilterTargets();

    if (m_levelCheck && m_UniqueTargetInfo.empty())
    {
        SendCastResult(SPELL_FAILED_LOWLEVEL);
        finish(false);
    }

    if (m_targets.HasDst() && (!(m_spellInfo->AttributesCu & SPELL_ATTR0_CU_AURA_CC) || IsTriggered()))
    {
        if (m_targets.HasTraj())
        {
            float speed = m_targets.GetSpeedXY();
            if (speed > 0.0f)
                m_delayMoment = uint64(floor(m_targets.GetDist2d() / speed * 1000.0f));
        }
        else if (m_spellValue->Speed > 0.0f)
        {
            float dist = m_caster->GetDistance(*m_targets.GetDstPos());
            if (!(m_spellInfo->AttributesEx9 & SPELL_ATTR9_SPECIAL_DELAY_CALCULATION))
                m_delayMoment = uint64(floor(dist / m_spellValue->Speed * 1000.0f));
            else
                m_delayMoment = uint64(m_spellValue->Speed * 1000.0f);
        }
    }
}

void Spell::SelectEffectImplicitTargets(SpellEffIndex effIndex, SpellImplicitTargetInfo const& targetType, uint32& processedEffectMask)
{
    if (!targetType.GetTarget())
        return;

    uint32 effectMask = 1 << effIndex;
    // set the same target list for all effects
    // some spells appear to need this, however this requires more research
    switch (targetType.GetSelectionCategory())
    {
        case TARGET_SELECT_CATEGORY_NEARBY:
        case TARGET_SELECT_CATEGORY_CONE:
        case TARGET_SELECT_CATEGORY_AREA:
            // targets for effect already selected
            if (effectMask & processedEffectMask)
                return;
            // choose which targets we can select at once
            for (uint32 j = effIndex + 1; j < MAX_SPELL_EFFECTS; ++j)
            {
                SpellEffectInfo const* effects = GetSpellInfo()->Effects;
                if (effects[j].IsEffect() &&
                    effects[effIndex].TargetA.GetTarget() == effects[j].TargetA.GetTarget() &&
                    effects[effIndex].TargetB.GetTarget() == effects[j].TargetB.GetTarget() &&
                    effects[effIndex].ImplicitTargetConditions == effects[j].ImplicitTargetConditions &&
                    effects[effIndex].CalcRadius(m_caster) == effects[j].CalcRadius(m_caster) &&
                    CheckScriptEffectImplicitTargets(effIndex, j))
                {
                    effectMask |= 1 << j;
                }
            }
            processedEffectMask |= effectMask;
            break;
        default:
            break;
    }

    switch (targetType.GetSelectionCategory())
    {
        case TARGET_SELECT_CATEGORY_CHANNEL:
            SelectImplicitChannelTargets(effIndex, targetType);
            break;
        case TARGET_SELECT_CATEGORY_NEARBY:
            SelectImplicitNearbyTargets(effIndex, targetType, effectMask);
            break;
        case TARGET_SELECT_CATEGORY_CONE:
            SelectImplicitConeTargets(effIndex, targetType, effectMask);
            break;
        case TARGET_SELECT_CATEGORY_AREA:
            SelectImplicitAreaTargets(effIndex, targetType, effectMask);
            break;
        case TARGET_SELECT_CATEGORY_DEFAULT:
            switch (targetType.GetObjectType())
            {
                case TARGET_OBJECT_TYPE_SRC:
                    switch (targetType.GetReferenceType())
                    {
                        case TARGET_REFERENCE_TYPE_CASTER:
                            m_targets.SetSrc(*m_caster);
                            break;
                        default:
                            ASSERT(false && "Spell::SelectEffectImplicitTargets: received not implemented select target reference type for TARGET_TYPE_OBJECT_SRC");
                            break;
                    }
                    break;
                case TARGET_OBJECT_TYPE_DEST:
                     switch (targetType.GetReferenceType())
                     {
                         case TARGET_REFERENCE_TYPE_CASTER:
                             SelectImplicitCasterDestTargets(effIndex, targetType);
                             break;
                         case TARGET_REFERENCE_TYPE_TARGET:
                             SelectImplicitTargetDestTargets(effIndex, targetType);
                             break;
                         case TARGET_REFERENCE_TYPE_DEST:
                             SelectImplicitDestDestTargets(effIndex, targetType);
                             break;
                         default:
                             ASSERT(false && "Spell::SelectEffectImplicitTargets: received not implemented select target reference type for TARGET_TYPE_OBJECT_DEST");
                             break;
                     }
                     break;
                default:
                    switch (targetType.GetReferenceType())
                    {
                        case TARGET_REFERENCE_TYPE_CASTER:
                            SelectImplicitCasterObjectTargets(effIndex, targetType);
                            break;
                        case TARGET_REFERENCE_TYPE_TARGET:
                            SelectImplicitTargetObjectTargets(effIndex, targetType);
                            break;
                        default:
                            ASSERT(false && "Spell::SelectEffectImplicitTargets: received not implemented select target reference type for TARGET_TYPE_OBJECT");
                            break;
                    }
                    break;
            }
            break;
        case TARGET_SELECT_CATEGORY_NYI:
            TC_LOG_DEBUG("spells", "SPELL: target type %u, found in spellID %u, effect %u is not implemented yet!", m_spellInfo->Id, effIndex, targetType.GetTarget());
            break;
        default:
            ASSERT(false && "Spell::SelectEffectImplicitTargets: received not implemented select target category");
            break;
    }
}

void Spell::SelectImplicitChannelTargets(SpellEffIndex effIndex, SpellImplicitTargetInfo const& targetType)
{
    if (targetType.GetReferenceType() != TARGET_REFERENCE_TYPE_CASTER)
    {
        ASSERT(false && "Spell::SelectImplicitChannelTargets: received not implemented target reference type");
        return;
    }

    Spell* channeledSpell = m_originalCaster->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
    if (!channeledSpell)
    {
        TC_LOG_DEBUG("spells", "Spell::SelectImplicitChannelTargets: cannot find channel spell for spell ID %u, effect %u", m_spellInfo->Id, effIndex);
        return;
    }

    switch (targetType.GetTarget())
    {
        case TARGET_UNIT_CHANNEL_TARGET:
        {

            WorldObject* target = ObjectAccessor::GetUnit(*m_caster, m_originalCaster->GetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT));
            CallScriptObjectTargetSelectHandlers(target, effIndex);
            // unit target may be no longer avalible - teleported out of map for example
            if (target && target->ToUnit())
                AddUnitTarget(target->ToUnit(), 1 << effIndex);
            else
                TC_LOG_DEBUG("spells", "SPELL: cannot find channel spell target for spell ID %u, effect %u", m_spellInfo->Id, effIndex);
            break;
        }
        case TARGET_DEST_CHANNEL_TARGET:
            if (channeledSpell->m_targets.HasDst())
                m_targets.SetDst(channeledSpell->m_targets);
            else if (WorldObject* target = ObjectAccessor::GetWorldObject(*m_caster, m_originalCaster->GetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT)))
            {
                CallScriptObjectTargetSelectHandlers(target, effIndex);
                if (target)
                    m_targets.SetDst(*target);
            }
            else
                TC_LOG_DEBUG("spells", "SPELL: cannot find channel spell destination for spell ID %u, effect %u", m_spellInfo->Id, effIndex);
            break;
        case TARGET_DEST_CHANNEL_CASTER:
            m_targets.SetDst(*channeledSpell->GetCaster());
            break;
        default:
            ASSERT(false && "Spell::SelectImplicitChannelTargets: received not implemented target type");
            break;
    }
}

void Spell::SelectImplicitNearbyTargets(SpellEffIndex effIndex, SpellImplicitTargetInfo const& targetType, uint32 effMask)
{
    if (targetType.GetReferenceType() != TARGET_REFERENCE_TYPE_CASTER)
    {
        ASSERT(false && "Spell::SelectImplicitNearbyTargets: received not implemented target reference type");
        return;
    }

    float range = 0.0f;
    switch (targetType.GetCheckType())
    {
        case TARGET_CHECK_ENEMY:
            range = m_spellInfo->GetMaxRange(false, m_caster, this);
            break;
        case TARGET_CHECK_ALLY:
        case TARGET_CHECK_PARTY:
        case TARGET_CHECK_RAID:
        case TARGET_CHECK_RAID_CLASS:
            range = m_spellInfo->GetMaxRange(true, m_caster, this);
            break;
        case TARGET_CHECK_ENTRY:
        case TARGET_CHECK_DEFAULT:
            range = m_spellInfo->GetMaxRange(m_spellInfo->IsPositive(), m_caster, this);
            break;
        default:
            ASSERT(false && "Spell::SelectImplicitNearbyTargets: received not implemented selection check type");
            break;
    }

    ConditionList* condList = m_spellInfo->Effects[effIndex].ImplicitTargetConditions;

    // handle emergency case - try to use other provided targets if no conditions provided
    if (targetType.GetCheckType() == TARGET_CHECK_ENTRY && (!condList || condList->empty()))
    {
        TC_LOG_DEBUG("spells", "Spell::SelectImplicitNearbyTargets: no conditions entry for target with TARGET_CHECK_ENTRY of spell ID %u, effect %u - selecting default targets", m_spellInfo->Id, effIndex);
        switch (targetType.GetObjectType())
        {
            case TARGET_OBJECT_TYPE_GOBJ:
                if (m_spellInfo->RequiresSpellFocus)
                {
                    if (focusObject)
                        AddGOTarget(focusObject, effMask);
                    return;
                }
                break;
            case TARGET_OBJECT_TYPE_DEST:
                if (m_spellInfo->RequiresSpellFocus)
                {
                    if (focusObject)
                        m_targets.SetDst(*focusObject);
                    return;
                }
                break;
            default:
                break;
        }
    }

    WorldObject* target = SearchNearbyTarget(range, targetType.GetObjectType(), targetType.GetCheckType(), condList);
    if (!target)
    {
        TC_LOG_DEBUG("spells", "Spell::SelectImplicitNearbyTargets: cannot find nearby target for spell ID %u, effect %u", m_spellInfo->Id, effIndex);
        SendCastResult(SPELL_FAILED_BAD_IMPLICIT_TARGETS);
        finish(false);
        return;
    }

    CallScriptObjectTargetSelectHandlers(target, effIndex);

    if (!target)
    {
        TC_LOG_DEBUG("spells", "Spell::SelectImplicitNearbyTargets: OnObjectTargetSelect script hook for spell Id %u set NULL target, effect %u", m_spellInfo->Id, effIndex);
        SendCastResult(SPELL_FAILED_BAD_IMPLICIT_TARGETS);
        finish(false);
        return;
    }

    switch (targetType.GetObjectType())
    {
        case TARGET_OBJECT_TYPE_UNIT:
        {
            if (Unit* unitTarget = target->ToUnit())
                AddUnitTarget(unitTarget, effMask, true, false);
            else
            {
                TC_LOG_DEBUG("spells", "Spell::SelectImplicitNearbyTargets: OnObjectTargetSelect script hook for spell Id %u set object of wrong type, expected unit, got %s, effect %u", m_spellInfo->Id, GetLogNameForGuid(target->GetGUID()), effMask);
                return;
            }
            break;
        }
        case TARGET_OBJECT_TYPE_GOBJ:
            if (GameObject* gobjTarget = target->ToGameObject())
                AddGOTarget(gobjTarget, effMask);
            else
            {
                TC_LOG_DEBUG("spells", "Spell::SelectImplicitNearbyTargets: OnObjectTargetSelect script hook for spell Id %u set object of wrong type, expected gameobject, got %s, effect %u", m_spellInfo->Id, GetLogNameForGuid(target->GetGUID()), effMask);
                return;
            }
            break;
        case TARGET_OBJECT_TYPE_DEST:
            m_targets.SetDst(*target);
            break;
        default:
            ASSERT(false && "Spell::SelectImplicitNearbyTargets: received not implemented target object type");
            break;
    }

    if ((m_casttime || m_spellInfo->IsChanneled()) && !(_triggeredCastFlags & TRIGGERED_IGNORE_SET_FACING))
        if (m_caster->GetGUID() != target->GetGUID() && m_caster->GetTypeId() == TYPEID_UNIT)
        {
            m_originalCaster->ReleaseFocus(this);
            m_caster->FocusTarget(this, target->GetGUID());
        }

    if (m_spellInfo->IsChanneled() && m_spellInfo->AttributesEx & SPELL_ATTR1_CHANNEL_TRACK_TARGET && m_originalCaster && m_originalCaster->GetTypeId() == TYPEID_UNIT)
    {
        if (m_originalCaster->GetGUID() != target->GetGUID())
        {
            m_originalCaster->AddUnitMovementFlag(MOVEMENTFLAG_TRACKING_TARGET);
            if (Unit *channelTrackTarget = target->ToUnit())
            {
                m_originalCaster->StopMoving();
                m_originalCaster->SetFacingToObject(channelTrackTarget);
                m_originalCaster->SendMeleeAttackStop();
            }
            m_originalCaster->SetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT, target->GetGUID());
        }
    }

    SelectImplicitChainTargets(effIndex, targetType, target, effMask);
}

void Spell::SelectImplicitConeTargets(SpellEffIndex effIndex, SpellImplicitTargetInfo const& targetType, uint32 effMask)
{
    if (targetType.GetReferenceType() != TARGET_REFERENCE_TYPE_CASTER)
    {
        ASSERT(false && "Spell::SelectImplicitConeTargets: received not implemented target reference type");
        return;
    }
    std::list<WorldObject*> targets;
    SpellTargetObjectTypes objectType = targetType.GetObjectType();
    SpellTargetCheckTypes selectionType = targetType.GetCheckType();
    ConditionList* condList = m_spellInfo->Effects[effIndex].ImplicitTargetConditions;

    float coneAngle;
    if (G3D::fuzzyEq(m_spellInfo->ConeAngle, 0.f))
        coneAngle = 90.f;
    else coneAngle = m_spellInfo->ConeAngle;
    coneAngle *= (2.f * float(M_PI) / 360.f);
    float radius = m_spellInfo->Effects[effIndex].CalcRadius(m_caster) * m_spellValue->RadiusMod;

    if (uint32 containerTypeMask = GetSearcherTypeMask(objectType, condList))
    {
        Trinity::WorldObjectSpellConeTargetCheck check(coneAngle, radius, m_caster, m_spellInfo, selectionType, condList);
        Trinity::WorldObjectListSearcher<Trinity::WorldObjectSpellConeTargetCheck> searcher(m_caster, targets, check, containerTypeMask);
        SearchTargets<Trinity::WorldObjectListSearcher<Trinity::WorldObjectSpellConeTargetCheck> >(searcher, containerTypeMask, m_caster, m_caster, radius);

        CallScriptObjectAreaTargetSelectHandlers(targets, effIndex);

        if (!targets.empty())
        {
            // Other special target selection goes here
            if (uint32 maxTargets = m_spellValue->MaxAffectedTargets)
                Trinity::Containers::RandomResizeList(targets, maxTargets);

            // for compability with older code - add only unit and go targets
            // TODO: remove this
            std::list<Unit*> unitTargets;
            std::list<GameObject*> gObjTargets;

            for (std::list<WorldObject*>::iterator itr = targets.begin(); itr != targets.end(); ++itr)
            {
                if (Unit* unitTarget = (*itr)->ToUnit())
                    unitTargets.push_back(unitTarget);
                else if (GameObject* gObjTarget = (*itr)->ToGameObject())
                    gObjTargets.push_back(gObjTarget);
            }

            for (std::list<Unit*>::iterator itr = unitTargets.begin(); itr != unitTargets.end(); ++itr)
                AddUnitTarget(*itr, effMask, false);

            for (std::list<GameObject*>::iterator itr = gObjTargets.begin(); itr != gObjTargets.end(); ++itr)
                AddGOTarget(*itr, effMask);
        }
    }
}

void Spell::SelectImplicitAreaTargets(SpellEffIndex effIndex, SpellImplicitTargetInfo const& targetType, uint32 effMask)
{
    Unit* referer = NULL;
    switch (targetType.GetReferenceType())
    {
        case TARGET_REFERENCE_TYPE_SRC:
        case TARGET_REFERENCE_TYPE_DEST:
        case TARGET_REFERENCE_TYPE_CASTER:
            referer = m_caster;
            break;
        case TARGET_REFERENCE_TYPE_TARGET:
            referer = m_targets.GetUnitTarget();
            break;
        case TARGET_REFERENCE_TYPE_LAST:
        {
            // find last added target for this effect
            for (std::list<TargetInfo>::reverse_iterator ihit = m_UniqueTargetInfo.rbegin(); ihit != m_UniqueTargetInfo.rend(); ++ihit)
            {
                if (ihit->effectMask & (1<<effIndex))
                {
                    referer = ObjectAccessor::GetUnit(*m_caster, ihit->targetGUID);
                    break;
                }
            }
            break;
        }
        default:
            ASSERT(false && "Spell::SelectImplicitAreaTargets: received not implemented target reference type");
            return;
    }
    if (!referer)
        return;

    Position const* center = NULL;
    switch (targetType.GetReferenceType())
    {
        case TARGET_REFERENCE_TYPE_SRC:
            center = m_targets.GetSrcPos();
            break;
        case TARGET_REFERENCE_TYPE_DEST:
            center = m_targets.GetDstPos();
            break;
        case TARGET_REFERENCE_TYPE_CASTER:
        case TARGET_REFERENCE_TYPE_TARGET:
        case TARGET_REFERENCE_TYPE_LAST:
            center = referer;
            break;
         default:
             ASSERT(false && "Spell::SelectImplicitAreaTargets: received not implemented target reference type");
             return;
    }
    std::list<WorldObject*> targets;
    float radius = m_spellInfo->Effects[effIndex].CalcRadius(m_caster) * m_spellValue->RadiusMod;
    SearchAreaTargets(targets, radius, center, referer, targetType.GetObjectType(), targetType.GetCheckType(), m_spellInfo->Effects[effIndex].ImplicitTargetConditions);

    // Custom entries
    // TODO: remove those
    switch (m_spellInfo->Id)
    {
        case 46584: // Raise Dead
        {
            if (Player* playerCaster = m_caster->ToPlayer())
            {
                for (std::list<WorldObject*>::iterator itr = targets.begin(); itr != targets.end(); ++itr)
                {
                    switch ((*itr)->GetTypeId())
                    {
                        case TYPEID_UNIT:
                        case TYPEID_PLAYER:
                        {
                            Unit* unitTarget = (*itr)->ToUnit();
                            if (unitTarget->isAlive() || !playerCaster->isHonorOrXPTarget(unitTarget)
                                || ((unitTarget->GetCreatureTypeMask() & (1 << (CREATURE_TYPE_HUMANOID-1))) == 0)
                                || (unitTarget->GetDisplayId() != unitTarget->GetNativeDisplayId()))
                                break;
                            AddUnitTarget(unitTarget, effMask, false);
                            // no break;
                        }
                        case TYPEID_CORPSE: // wont work until corpses are allowed in target lists, but at least will send dest in packet
                            m_targets.SetDst(*(*itr));
                            return; // nothing more to do here
                        default:
                            break;
                    }
                }
            }
            return; // don't add targets to target map
        }
        // Corpse Explosion
        case 49158:
        case 51325:
        case 51326:
        case 51327:
        case 51328:
            // check if our target is not valid (spell can target ghoul or dead unit)
            if (!(m_targets.GetUnitTarget() && m_targets.GetUnitTarget()->GetDisplayId() == m_targets.GetUnitTarget()->GetNativeDisplayId() &&
                ((m_targets.GetUnitTarget()->GetEntry() == 26125 && m_targets.GetUnitTarget()->GetOwnerGUID() == m_caster->GetGUID())
                || m_targets.GetUnitTarget()->isDead())))
            {
                // remove existing targets
                CleanupTargetList();

                for (std::list<WorldObject*>::iterator itr = targets.begin(); itr != targets.end(); ++itr)
                {
                    switch ((*itr)->GetTypeId())
                    {
                        case TYPEID_UNIT:
                        case TYPEID_PLAYER:
                            if (!(*itr)->ToUnit()->isDead())
                                break;
                            AddUnitTarget((*itr)->ToUnit(), 1 << effIndex, false);
                            return;
                        default:
                            break;
                    }
                }
                if (m_caster->GetTypeId() == TYPEID_PLAYER)
                    m_caster->ToPlayer()->RemoveSpellCooldown(m_spellInfo->Id, true);
                SendCastResult(SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW);
                finish(false);
            }
            return;
        default:
            break;
    }

    CallScriptObjectAreaTargetSelectHandlers(targets, effIndex);

    std::list<Unit*> unitTargets;
    std::list<GameObject*> gObjTargets;
    // for compability with older code - add only unit and go targets
    // TODO: remove this
    for (std::list<WorldObject*>::iterator itr = targets.begin(); itr != targets.end(); ++itr)
    {
        if (Unit* unitTarget = (*itr)->ToUnit())
            unitTargets.push_back(unitTarget);
        else if (GameObject* gObjTarget = (*itr)->ToGameObject())
            gObjTargets.push_back(gObjTarget);
    }

    if (!unitTargets.empty())
    {
        // Special target selection for smart heals and energizes
        uint32 maxSize = 0;
        int32 power = -1;
        switch (m_spellInfo->SpellFamilyName)
        {
            case SPELLFAMILY_GENERIC:
                switch (m_spellInfo->Id)
                {
                    case 52759: // Ancestral Awakening
                    case 71610: // Echoes of Light (Althor's Abacus normal version)
                    case 71641: // Echoes of Light (Althor's Abacus heroic version)
                    case 96966: // Eye of Blazing Power
                    case 97136: // Eye of Blazing Power
                    case 108000:// Windward Heart
                    case 109822:// Windward Heart
                    case 109825:// Windward Heart
                    case 54968: // Divine Flame
                    case 99152: // Cauterizing Flame
                        maxSize = 1;
                        power = POWER_HEALTH;
                        break;
                    case 57669: // Replenishment
                        // In arenas Replenishment may only affect the caster
                        if (m_caster->GetTypeId() == TYPEID_PLAYER && m_caster->ToPlayer()->InArena())
                        {
                            unitTargets.clear();
                            unitTargets.push_back(m_caster);
                            break;
                        }
                        maxSize = 10;
                        power = POWER_MANA;
                        break;
                    default:
                        break;
                }
                break;
            case SPELLFAMILY_PRIEST:
                {
                    bool onlyRaidTargets = true;
                    if (m_spellInfo->SpellFamilyFlags[0] == 0x10000000) // Circle of Healing
                    {
                        maxSize = m_caster->HasAura(55675) ? 6 : 5; // Glyph of Circle of Healing
                        power = POWER_HEALTH;
                    }
                    else if (m_spellInfo->Id == 64844) // Divine Hymn
                    {
                        maxSize = 5;
                        power = POWER_HEALTH;
                    }
                    else if (m_spellInfo->Id == 64904) // Hymn of Hope
                    {
                        maxSize = 3;
                        power = POWER_MANA;
                    }
                    else if (m_spellInfo->Id == 94472 || m_spellInfo->Id == 81751) // Atonement
                    {
                        onlyRaidTargets = false;
                        maxSize = 1;
                        power = POWER_HEALTH;
                    }
                    else
                        break;

                    // Remove targets outside caster's raid
                    if (onlyRaidTargets)
                        for (std::list<Unit*>::iterator itr = unitTargets.begin(); itr != unitTargets.end();)
                        {
                            if (!(*itr)->IsInRaidWith(m_caster))
                                itr = unitTargets.erase(itr);
                            else
                                ++itr;
                        }
                    break;
            }
            case SPELLFAMILY_DRUID:
                if (m_spellInfo->SpellFamilyFlags[1] == 0x04000000) // Wild Growth
                {
                    maxSize = 5;
                    // Glyph of Wild Growth
                    if (m_caster->HasAura(62970))
                        maxSize++;
                    // Tree of Life bonus
                    if (m_caster->HasAura(33891))
                        maxSize += 2;

                    power = POWER_HEALTH;
                }
                else if (m_spellInfo->Id == 44203) // Tranquility
                {
                    maxSize = 5;
                    power = POWER_HEALTH;
                }
                else if (m_spellInfo->Id == 99017) // Firebloom
                {
                    maxSize = 1;
                    power = POWER_HEALTH;
                }
                else
                    break;

                // Remove targets outside caster's raid
                for (std::list<Unit*>::iterator itr = unitTargets.begin(); itr != unitTargets.end();)
                    if (!(*itr)->IsInRaidWith(m_caster))
                        itr = unitTargets.erase(itr);
                    else
                        ++itr;
                break;
            default:
                break;
        }

        if (maxSize && power != -1)
        {
            if (Powers(power) == POWER_HEALTH)
            {
                if (unitTargets.size() > maxSize)
                {
                    unitTargets.sort(Trinity::HealthPctOrderPred());
                    unitTargets.resize(maxSize);
                }
            }
            else
            {
                for (std::list<Unit*>::iterator itr = unitTargets.begin(); itr != unitTargets.end();)
                    if ((*itr)->getPowerType() != (Powers)power)
                        itr = unitTargets.erase(itr);
                    else
                        ++itr;

                if (unitTargets.size() > maxSize)
                {
                    unitTargets.sort(Trinity::PowerPctOrderPred((Powers)power));
                    unitTargets.resize(maxSize);
                }
            }
        }

        // Other special target selection goes here
        if (uint32 maxTargets = m_spellValue->MaxAffectedTargets)
            Trinity::Containers::RandomResizeList(unitTargets, maxTargets);

        for (std::list<Unit*>::iterator itr = unitTargets.begin(); itr != unitTargets.end(); ++itr)
            AddUnitTarget(*itr, effMask, false);
    }

    if (!gObjTargets.empty())
    {
        if (uint32 maxTargets = m_spellValue->MaxAffectedTargets)
            Trinity::Containers::RandomResizeList(gObjTargets, maxTargets);

        for (std::list<GameObject*>::iterator itr = gObjTargets.begin(); itr != gObjTargets.end(); ++itr)
            AddGOTarget(*itr, effMask);
    }
}

void Spell::SelectImplicitCasterDestTargets(SpellEffIndex effIndex, SpellImplicitTargetInfo const& targetType)
{
    switch (targetType.GetTarget())
    {
        case TARGET_DEST_CASTER:
            m_targets.SetDst(*m_caster);
            return;
        case TARGET_DEST_HOME:
            if (Player* playerCaster = m_caster->ToPlayer())
                m_targets.SetDst(playerCaster->m_homebindX, playerCaster->m_homebindY, playerCaster->m_homebindZ, playerCaster->GetOrientation(), playerCaster->m_homebindMapId);
            return;
        case TARGET_DEST_DB:
            if (SpellTargetPosition const* st = sSpellMgr->GetSpellTargetPosition(m_spellInfo->Id))
            {
                // TODO: fix this check
                if (m_spellInfo->HasEffect(SPELL_EFFECT_TELEPORT_UNITS) || m_spellInfo->HasEffect(SPELL_EFFECT_BIND))
                    m_targets.SetDst(st->target_X, st->target_Y, st->target_Z, st->target_Orientation, (int32)st->target_mapId);
                else if (st->target_mapId == m_caster->GetMapId())
                    m_targets.SetDst(st->target_X, st->target_Y, st->target_Z, st->target_Orientation);
            }
            else
            {
                TC_LOG_DEBUG("spells", "SPELL: unknown target coordinates for spell ID %u", m_spellInfo->Id);
                WorldObject* target = m_targets.GetObjectTarget();
                m_targets.SetDst(target ? *target : *m_caster);
            }
            return;
        case TARGET_DEST_CASTER_FISHING:
        {
             float min_dis = m_spellInfo->GetMinRange(true);
             float max_dis = m_spellInfo->GetMaxRange(true);
             float dis = (float)rand_norm() * (max_dis - min_dis) + min_dis;
             float x, y, z, angle;
             angle = (float)rand_norm() * static_cast<float>(M_PI * 35.0f / 180.0f) - static_cast<float>(M_PI * 17.5f / 180.0f);
             m_caster->GetClosePoint(x, y, z, DEFAULT_WORLD_OBJECT_SIZE, dis, angle);
             m_targets.SetDst(x, y, z, m_caster->GetOrientation());
             return;
        }
        default:
            break;
    }

    float dist;
    float angle = targetType.CalcDirectionAngle();
    float objSize = m_caster->GetObjectSize();
    if (targetType.GetTarget() == TARGET_DEST_CASTER_SUMMON)
        dist = PET_FOLLOW_DIST;
    else
        dist = m_spellInfo->Effects[effIndex].CalcRadius(m_caster);

    if (dist < objSize)
        dist = objSize;
    else if (targetType.GetTarget() == TARGET_DEST_CASTER_RANDOM)
        dist = objSize + (dist - objSize) * (float)rand_norm();

    if (m_spellInfo->AttributesEx7 & SPELL_ATTR7_SUMMON_TOTEM)
        dist += m_caster->GetCombatReach();

    Position pos;
    if (targetType.GetTarget() == TARGET_DEST_CASTER_FRONT_LEAP)
        m_caster->MoveBlink(pos, dist, angle);
    else
    {
        if (!m_caster->m_movementInfo.HasMovementFlag(MOVEMENTFLAG_FALLING | MOVEMENTFLAG_FALLING_FAR))
            m_caster->GetFirstCollisionPosition(pos, dist, angle);
        else
            m_caster->GetNearPosition(pos, dist, angle, false);
        if (!m_caster->IsWithinLOS(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), m_spellInfo))
            m_caster->GetPosition(&pos);
    }
    m_targets.SetDst(*m_caster);
    m_targets.ModDst(pos);
}

void Spell::SelectImplicitTargetDestTargets(SpellEffIndex effIndex, SpellImplicitTargetInfo const& targetType)
{
    WorldObject* target = m_targets.GetObjectTarget();
    switch (targetType.GetTarget())
    {
        case TARGET_DEST_TARGET_ENEMY:
        case TARGET_DEST_TARGET_ANY:
            m_targets.SetDst(*target);
            return;
        default:
            break;
    }

    float angle = targetType.CalcDirectionAngle();

    float objSize = target->GetObjectSize();
    float dist = m_spellInfo->Effects[effIndex].CalcRadius(m_caster);
    if (dist < objSize)
        dist = objSize;
    else if (targetType.GetTarget() == TARGET_DEST_TARGET_RANDOM)
        dist = objSize + (dist - objSize) * (float)rand_norm();
    else if (targetType.GetTarget() == TARGET_DEST_TARGET_RADIUS)
    {
        if (m_spellInfo->Effects[effIndex].HasMaxRadius())
            dist = m_spellInfo->Effects[effIndex].MaxRadiusEntry->RadiusMax;
        else if (m_spellInfo->Effects[effIndex].HasRadius())
            dist = m_spellInfo->Effects[effIndex].RadiusEntry->RadiusMin;
    }

    Position pos;
    target->GetFirstCollisionPosition(pos, dist, angle);
    m_targets.SetDst(*target);
    m_targets.ModDst(pos);
}

void Spell::SelectImplicitDestDestTargets(SpellEffIndex effIndex, SpellImplicitTargetInfo const& targetType)
{
    // set destination to caster if no dest provided
    // can only happen if previous destination target could not be set for some reason
    // (not found nearby target, or channel target for example
    // maybe we should abort the spell in such case?
    if (!m_targets.HasDst())
    {
        if (targetType.GetTarget() == TARGET_DEST_DEST)
            return;
        else
            m_targets.SetDst(*m_caster);
    }

    switch (targetType.GetTarget())
    {
        case TARGET_DEST_DYNOBJ_ENEMY:
        case TARGET_DEST_DYNOBJ_ALLY:
        case TARGET_DEST_DYNOBJ_NONE:
        case TARGET_DEST_DEST:
            return;
        case TARGET_DEST_TRAJ:
            SelectImplicitTrajTargets();
            return;
        default:
            break;
    }

    float angle = targetType.CalcDirectionAngle();
    float dist = m_spellInfo->Effects[effIndex].CalcRadius(m_caster);
    if (targetType.GetTarget() == TARGET_DEST_DEST_RANDOM)
        dist *= (float)rand_norm();

    Position pos = *m_targets.GetDstPos();
    m_caster->MovePosition(pos, dist, angle);
    m_targets.ModDst(pos);
}

void Spell::SelectImplicitCasterObjectTargets(SpellEffIndex effIndex, SpellImplicitTargetInfo const& targetType)
{
    WorldObject* target = NULL;
    bool checkIfValid = true;

    switch (targetType.GetTarget())
    {
        case TARGET_UNIT_CASTER:
            target = m_caster;
            checkIfValid = false;
            break;
        case TARGET_UNIT_MASTER:
            target = m_caster->GetCharmerOrOwner();
            break;
        case TARGET_UNIT_PET:
            target = m_caster->GetGuardianPet();
            break;
        case TARGET_UNIT_SUMMONER:
            if (m_caster->isSummon())
                target = m_caster->ToTempSummon()->GetSummoner();
            break;
        case TARGET_UNIT_VEHICLE:
            target = m_caster->GetVehicleBase();
            break;
        case TARGET_UNIT_PASSENGER_0:
        case TARGET_UNIT_PASSENGER_1:
        case TARGET_UNIT_PASSENGER_2:
        case TARGET_UNIT_PASSENGER_3:
        case TARGET_UNIT_PASSENGER_4:
        case TARGET_UNIT_PASSENGER_5:
        case TARGET_UNIT_PASSENGER_6:
        case TARGET_UNIT_PASSENGER_7:
            if (m_caster->GetTypeId() == TYPEID_UNIT && m_caster->ToCreature()->IsVehicle())
                target = m_caster->GetVehicleKit()->GetPassenger(targetType.GetTarget() - TARGET_UNIT_PASSENGER_0);
            break;
        case TARGET_UNIT_TARGET_PASSENGER:
        case TARGET_UNIT_UNK_105:
            if (m_caster->IsVehicle())
            {
                if (Vehicle *veh = m_caster->GetVehicleKit())
                {
                    SeatMap::iterator itr;
                    int seatId = 0;
                    for (itr = veh->Seats.begin(); itr != veh->Seats.end(); ++itr)
                    {
                        if (target = m_caster->GetVehicleKit()->GetPassenger(seatId)) // take the first valid seat. Don't sure this is correct but maybe ? O maybe all units on seat ?
                            break;
                        seatId++;
                    }
                }
            }
            break;
        default:
            break;
    }

    CallScriptObjectTargetSelectHandlers(target, effIndex);

    if (target && target->ToUnit())
        AddUnitTarget(target->ToUnit(), 1 << effIndex, checkIfValid);
}

void Spell::SelectImplicitTargetObjectTargets(SpellEffIndex effIndex, SpellImplicitTargetInfo const& targetType)
{
    if (!m_targets.GetObjectTarget() && !m_targets.GetItemTarget())
    {
        SendCastResult(SPELL_FAILED_NO_VALID_TARGETS);
        finish(false);
    }

    WorldObject* target = m_targets.GetObjectTarget();

    CallScriptObjectTargetSelectHandlers(target, effIndex);

    if (target)
    {
        if (Unit* unit = target->ToUnit())
            AddUnitTarget(unit, 1 << effIndex, true, false);
        else if (GameObject* gobj = target->ToGameObject())
            AddGOTarget(gobj, 1 << effIndex);

        SelectImplicitChainTargets(effIndex, targetType, target, 1 << effIndex);
    }
    // Script hook can remove object target and we would wrongly land here
    else if (Item* item = m_targets.GetItemTarget())
        AddItemTarget(item, 1 << effIndex);
}

void Spell::SelectImplicitChainTargets(SpellEffIndex effIndex, SpellImplicitTargetInfo const& targetType, WorldObject* target, uint32 effMask)
{
    uint32 maxTargets = m_spellInfo->Effects[effIndex].ChainTarget;
    if (Player* modOwner = m_caster->GetSpellModOwner())
        modOwner->ApplySpellMod(m_spellInfo->Id, SPELLMOD_JUMP_TARGETS, maxTargets, this);

    if (maxTargets > 1)
    {
        // mark damage multipliers as used
        for (uint32 k = effIndex; k < MAX_SPELL_EFFECTS; ++k)
            if (effMask & (1 << k))
                m_damageMultipliers[k] = 1.0f;
        m_applyMultiplierMask |= effMask;

        std::list<WorldObject*> targets;
        SearchChainTargets(targets, maxTargets - 1, target, targetType.GetObjectType(), targetType.GetCheckType()
            , m_spellInfo->Effects[effIndex].ImplicitTargetConditions, targetType.GetTarget() == TARGET_UNIT_TARGET_CHAINHEAL_ALLY);

        // Chain primary target is added earlier
        CallScriptObjectAreaTargetSelectHandlers(targets, effIndex);

        // for backward compability
        std::list<Unit*> unitTargets;
        for (std::list<WorldObject*>::iterator itr = targets.begin(); itr != targets.end(); ++itr)
            if (Unit* unitTarget = (*itr)->ToUnit())
                unitTargets.push_back(unitTarget);

        for (std::list<Unit*>::iterator itr = unitTargets.begin(); itr != unitTargets.end(); ++itr)
            AddUnitTarget(*itr, effMask, false);
    }
}

float tangent(float x)
{
    x = tan(x);
    //if (x < std::numeric_limits<float>::max() && x > -std::numeric_limits<float>::max()) return x;
    //if (x >= std::numeric_limits<float>::max()) return std::numeric_limits<float>::max();
    //if (x <= -std::numeric_limits<float>::max()) return -std::numeric_limits<float>::max();
    if (x < 100000.0f && x > -100000.0f) return x;
    if (x >= 100000.0f) return 100000.0f;
    if (x <= 100000.0f) return -100000.0f;
    return 0.0f;
}

#define DEBUG_TRAJ(a) //a

void Spell::SelectImplicitTrajTargets()
{
    if (!m_targets.HasTraj())
        return;

    float dist2d = m_targets.GetDist2d();
    if (!dist2d)
        return;

    float srcToDestDelta = m_targets.GetDstPos()->m_positionZ - m_targets.GetSrcPos()->m_positionZ;

    std::list<WorldObject*> targets;
    Trinity::WorldObjectSpellTrajTargetCheck check(dist2d, m_targets.GetSrcPos(), m_caster, m_spellInfo);
    Trinity::WorldObjectListSearcher<Trinity::WorldObjectSpellTrajTargetCheck> searcher(m_caster, targets, check, GRID_MAP_TYPE_MASK_ALL);
    SearchTargets<Trinity::WorldObjectListSearcher<Trinity::WorldObjectSpellTrajTargetCheck> > (searcher, GRID_MAP_TYPE_MASK_ALL, m_caster, m_targets.GetSrcPos(), dist2d);
    if (targets.empty())
        return;

    targets.sort(Trinity::ObjectDistanceOrderPred(m_caster));

    float b = tangent(m_targets.GetElevation());
    float a = (srcToDestDelta - dist2d * b) / (dist2d * dist2d);
    if (a > -0.0001f)
        a = 0;
    DEBUG_TRAJ(TC_LOG_ERROR("spells", "Spell::SelectTrajTargets: a %f b %f", a, b);)

    float bestDist = m_spellInfo->GetMaxRange(false);

    auto itr = targets.begin();
    for (; itr != targets.end(); ++itr)
    {
        if (Unit* unitTarget = (*itr)->ToUnit())
            if (m_caster == *itr || m_caster->IsOnVehicle(unitTarget) || (unitTarget)->GetVehicle())//(*itr)->IsOnVehicle(m_caster))
                continue;

        const float size = std::max((*itr)->GetObjectSize() * 0.7f, 1.0f); // 1/sqrt(3)
        // TODO: all calculation should be based on src instead of m_caster
        const float objDist2d = m_targets.GetSrcPos()->GetExactDist2d(*itr) * std::cos(m_targets.GetSrcPos()->GetRelativeAngle(*itr));
        const float dz = (*itr)->GetPositionZ() - m_targets.GetSrcPos()->m_positionZ;

        DEBUG_TRAJ(TC_LOG_ERROR("spells", "Spell::SelectTrajTargets: check %u, dist between %f %f, height between %f %f.", (*itr)->GetEntry(), objDist2d - size, objDist2d + size, dz - size, dz + size);)

        float dist = objDist2d - size;
        float height = dist * (a * dist + b);
        DEBUG_TRAJ(TC_LOG_ERROR("spells", "Spell::SelectTrajTargets: dist %f, height %f.", dist, height);)
        if (dist < bestDist && height < dz + size && height > dz - size)
        {
            bestDist = dist > 0 ? dist : 0;
            break;
        }

#define CHECK_DIST {\
            DEBUG_TRAJ(TC_LOG_ERROR("spells", "Spell::SelectTrajTargets: dist %f, height %f.", dist, height);)\
            if (dist > bestDist)\
                continue;\
            if (dist < objDist2d + size && dist > objDist2d - size)\
            {\
                bestDist = dist;\
                break;\
            }\
        }

        if (!a)
        {
            height = dz - size;
            dist = height / b;
            CHECK_DIST;

            height = dz + size;
            dist = height / b;
            CHECK_DIST;

            continue;
        }

        height = dz - size;
        float sqrt1 = b * b + 4 * a * height;
        if (sqrt1 > 0)
        {
            sqrt1 = sqrt(sqrt1);
            dist = (sqrt1 - b) / (2 * a);
            CHECK_DIST;
        }

        height = dz + size;
        float sqrt2 = b * b + 4 * a * height;
        if (sqrt2 > 0)
        {
            sqrt2 = sqrt(sqrt2);
            dist = (sqrt2 - b) / (2 * a);
            CHECK_DIST;

            dist = (-sqrt2 - b) / (2 * a);
            CHECK_DIST;
        }

        if (sqrt1 > 0)
        {
            dist = (-sqrt1 - b) / (2 * a);
            CHECK_DIST;
        }
    }

    if (m_targets.GetSrcPos()->GetExactDist2d(m_targets.GetDstPos()) > bestDist)
    {
        float x = m_targets.GetSrcPos()->m_positionX + std::cos(m_caster->GetOrientation()) * bestDist;
        float y = m_targets.GetSrcPos()->m_positionY + std::sin(m_caster->GetOrientation()) * bestDist;
        float z = m_targets.GetSrcPos()->m_positionZ + bestDist * (a * bestDist + b);

        if (itr != targets.end())
        {
            float distSq = (*itr)->GetExactDistSq(x, y, z);
            float sizeSq = (*itr)->GetObjectSize();
            sizeSq *= sizeSq;
            DEBUG_TRAJ(TC_LOG_ERROR("spells", "Initial %f %f %f %f %f", x, y, z, distSq, sizeSq);)
            if (distSq > sizeSq)
            {
                float factor = 1 - sqrt(sizeSq / distSq);
                x += factor * ((*itr)->GetPositionX() - x);
                y += factor * ((*itr)->GetPositionY() - y);
                z += factor * ((*itr)->GetPositionZ() - z);

                distSq = (*itr)->GetExactDistSq(x, y, z);
                DEBUG_TRAJ(TC_LOG_ERROR("spells", "Initial %f %f %f %f %f", x, y, z, distSq, sizeSq);)
            }
        }

        Position trajDst;
        trajDst.Relocate(x, y, z, m_caster->GetOrientation());
        m_targets.ModDst(trajDst);
    }

    if (Vehicle* veh = m_caster->GetVehicleKit())
        veh->SetLastShootPos(*m_targets.GetDstPos());
}

void Spell::SelectEffectTypeImplicitTargets(uint8 effIndex)
{
    // special case for SPELL_EFFECT_SUMMON_RAF_FRIEND and SPELL_EFFECT_SUMMON_PLAYER
    // TODO: this is a workaround - target shouldn't be stored in target map for those spells
    switch (m_spellInfo->Effects[effIndex].Effect)
    {
        case SPELL_EFFECT_SUMMON_RAF_FRIEND:
        case SPELL_EFFECT_SUMMON_PLAYER:
            if (m_caster->GetTypeId() == TYPEID_PLAYER && m_caster->ToPlayer()->GetSelection())
            {
                WorldObject* target = ObjectAccessor::FindPlayer(m_caster->ToPlayer()->GetSelection());

                CallScriptObjectTargetSelectHandlers(target, SpellEffIndex(effIndex));

                if (target && target->ToPlayer())
                    AddUnitTarget(target->ToUnit(), 1 << effIndex, false);
            }
            // this is silly...but better than nothing...
            if (m_spellInfo->Id == 85592) // Have Group, Will Travel
            {
                if (Player* caster = m_caster->ToPlayer())
                    if (Group* group = caster->GetGroup())
                        for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
                            if (Unit* target = itr->getSource())
                                if (target != m_caster)
                                    if (target->GetMapId() != 654 && target->GetMapId() != 648) // Check for starter areas, since it's a workaround, this is okay here for now
                                        AddUnitTarget(target->ToUnit(), 1 << effIndex, false);
            }
            return;
        default:
            break;
    }

    // select spell implicit targets based on effect type
    if (!m_spellInfo->Effects[effIndex].GetImplicitTargetType())
        return;

    uint32 targetMask = m_spellInfo->Effects[effIndex].GetMissingTargetMask();

    if (!targetMask)
        return;

    WorldObject* target = NULL;

    switch (m_spellInfo->Effects[effIndex].GetImplicitTargetType())
    {
        // add explicit object target or self to the target map
        case EFFECT_IMPLICIT_TARGET_EXPLICIT:
            // player which not released his spirit is Unit, but target flag for it is TARGET_FLAG_CORPSE_MASK
            if (targetMask & (TARGET_FLAG_UNIT_MASK | TARGET_FLAG_CORPSE_MASK))
            {
                if (Unit* unitTarget = m_targets.GetUnitTarget())
                    target = unitTarget;
                else if (targetMask & TARGET_FLAG_CORPSE_MASK)
                {
                    if (Corpse* corpseTarget = m_targets.GetCorpseTarget())
                    {
                        // TODO: this is a workaround - corpses should be added to spell target map too, but we can't do that so we add owner instead
                        if (Player* owner = ObjectAccessor::FindPlayer(corpseTarget->GetOwnerGUID()))
                            target = owner;
                    }
                }
                else //if (targetMask & TARGET_FLAG_UNIT_MASK)
                    target = m_caster;
            }
            if (targetMask & TARGET_FLAG_ITEM_MASK)
            {
                if (Item* itemTarget = m_targets.GetItemTarget())
                    AddItemTarget(itemTarget, 1 << effIndex);
                return;
            }
            if (targetMask & TARGET_FLAG_GAMEOBJECT_MASK)
                target = m_targets.GetGOTarget();
            break;
        // add self to the target map
        case EFFECT_IMPLICIT_TARGET_CASTER:
            if (targetMask & TARGET_FLAG_UNIT_MASK)
                target = m_caster;
            break;
        default:
            break;
    }

    CallScriptObjectTargetSelectHandlers(target, SpellEffIndex(effIndex));

    if (target)
    {
        if (target->ToUnit())
            AddUnitTarget(target->ToUnit(), 1 << effIndex, false);
        else if (target->ToGameObject())
            AddGOTarget(target->ToGameObject(), 1 << effIndex);
    }
}

uint32 Spell::GetSearcherTypeMask(SpellTargetObjectTypes objType, ConditionList* condList)
{
    // this function selects which containers need to be searched for spell target
    uint32 retMask = GRID_MAP_TYPE_MASK_ALL;

    // filter searchers based on searched object type
    switch (objType)
    {
        case TARGET_OBJECT_TYPE_UNIT:
        case TARGET_OBJECT_TYPE_UNIT_AND_DEST:
        case TARGET_OBJECT_TYPE_CORPSE:
        case TARGET_OBJECT_TYPE_CORPSE_ENEMY:
        case TARGET_OBJECT_TYPE_CORPSE_ALLY:
            retMask &= GRID_MAP_TYPE_MASK_PLAYER | GRID_MAP_TYPE_MASK_CORPSE | GRID_MAP_TYPE_MASK_CREATURE;
            break;
        case TARGET_OBJECT_TYPE_GOBJ:
        case TARGET_OBJECT_TYPE_GOBJ_ITEM:
            retMask &= GRID_MAP_TYPE_MASK_GAMEOBJECT;
            break;
        default:
            break;
    }
    if (!(m_spellInfo->AttributesEx2 & SPELL_ATTR2_CAN_TARGET_DEAD))
        retMask &= ~GRID_MAP_TYPE_MASK_CORPSE;
    if (m_spellInfo->AttributesEx3 & SPELL_ATTR3_ONLY_TARGET_PLAYERS)
        retMask &= GRID_MAP_TYPE_MASK_CORPSE | GRID_MAP_TYPE_MASK_PLAYER;
    if (m_spellInfo->AttributesEx3 & SPELL_ATTR3_ONLY_TARGET_GHOSTS)
        retMask &= GRID_MAP_TYPE_MASK_PLAYER;

    if (condList)
        retMask &= sConditionMgr->GetSearcherTypeMaskForConditionList(*condList);
    return retMask;
}

template<class SEARCHER>
void Spell::SearchTargets(SEARCHER& searcher, uint32 containerMask, Unit* referer, Position const* pos, float radius)
{
    if (!containerMask)
        return;

    // search world and grid for possible targets
    bool searchInGrid = containerMask & (GRID_MAP_TYPE_MASK_CREATURE | GRID_MAP_TYPE_MASK_GAMEOBJECT);
    bool searchInWorld = containerMask & (GRID_MAP_TYPE_MASK_CREATURE | GRID_MAP_TYPE_MASK_PLAYER | GRID_MAP_TYPE_MASK_CORPSE);
    if (searchInGrid || searchInWorld)
    {
        float x, y;
        x = pos->GetPositionX();
        y = pos->GetPositionY();

        CellCoord p(Trinity::ComputeCellCoord(x, y));
        Cell cell(p);
        cell.SetNoCreate();

        Map& map = *(referer->GetMap());

        if (searchInWorld)
        {
            TypeContainerVisitor<SEARCHER, WorldTypeMapContainer> world_object_notifier(searcher);
            cell.Visit(p, world_object_notifier, map, radius, x, y);
        }
        if (searchInGrid)
        {
            TypeContainerVisitor<SEARCHER, GridTypeMapContainer >  grid_object_notifier(searcher);
            cell.Visit(p, grid_object_notifier, map, radius, x, y);
        }
    }
}

WorldObject* Spell::SearchNearbyTarget(float range, SpellTargetObjectTypes objectType, SpellTargetCheckTypes selectionType, ConditionList* condList)
{
    WorldObject* target = NULL;
    uint32 containerTypeMask = GetSearcherTypeMask(objectType, condList);
    if (!containerTypeMask)
        return NULL;
    Trinity::WorldObjectSpellNearbyTargetCheck check(range, m_caster, m_spellInfo, selectionType, condList);
    Trinity::WorldObjectLastSearcher<Trinity::WorldObjectSpellNearbyTargetCheck> searcher(m_caster, target, check, containerTypeMask);
    SearchTargets<Trinity::WorldObjectLastSearcher<Trinity::WorldObjectSpellNearbyTargetCheck> > (searcher, containerTypeMask, m_caster, m_caster, range);
    return target;
}

void Spell::SearchAreaTargets(std::list<WorldObject*>& targets, float range, Position const* position, Unit* referer, SpellTargetObjectTypes objectType, SpellTargetCheckTypes selectionType, ConditionList* condList)
{
    uint32 containerTypeMask = GetSearcherTypeMask(objectType, condList);
    if (!containerTypeMask)
        return;
    Trinity::WorldObjectSpellAreaTargetCheck check(range, position, m_caster, referer, m_spellInfo, selectionType, condList);
    Trinity::WorldObjectListSearcher<Trinity::WorldObjectSpellAreaTargetCheck> searcher(m_caster, targets, check, containerTypeMask);
    SearchTargets<Trinity::WorldObjectListSearcher<Trinity::WorldObjectSpellAreaTargetCheck> > (searcher, containerTypeMask, m_caster, position, range);
}

void Spell::SearchChainTargets(std::list<WorldObject*>& targets, uint32 chainTargets, WorldObject* target, SpellTargetObjectTypes objectType, SpellTargetCheckTypes selectType, ConditionList* condList, bool isChainHeal)
{
    // max dist for jump target selection
    float jumpRadius = 0.0f;
    switch (m_spellInfo->DmgClass)
    {
        case SPELL_DAMAGE_CLASS_RANGED:
            // 7.5y for multi shot
            jumpRadius = 7.5f;
            break;
        case SPELL_DAMAGE_CLASS_MELEE:
            // 5y for swipe, cleave and similar
            jumpRadius = 5.0f;
            break;
        case SPELL_DAMAGE_CLASS_NONE:
        case SPELL_DAMAGE_CLASS_MAGIC:
            // 12.5y for chain heal spell since 3.2 patch
            if (isChainHeal)
                jumpRadius = 12.5f;
            // 10y as default for magic chain spells
            else
                jumpRadius = 10.0f;
            break;
    }

    // chain lightning/heal spells and similar - allow to jump at larger distance and go out of los
    bool isBouncingFar = (m_spellInfo->AttributesEx4 & SPELL_ATTR4_AREA_TARGET_CHAIN
        || m_spellInfo->DmgClass == SPELL_DAMAGE_CLASS_NONE
        || m_spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MAGIC);

    switch (m_spellInfo->Id)
    {
        case 83282:
        case 92448:
        case 92449:
        case 92450:
        case 84529:
        case 92480:
        case 92481:
        case 92482:
            isBouncingFar = false;
            break;
        default:
            break;
    }

    // max dist which spell can reach
    float searchRadius = jumpRadius;
    if (isBouncingFar)
        searchRadius *= chainTargets;

    std::list<WorldObject*> tempTargets;
    SearchAreaTargets(tempTargets, searchRadius, target, m_caster, objectType, selectType, condList);
    tempTargets.remove(target);

    // remove targets which are always invalid for chain spells
    // for some spells allow only chain targets in front of caster (swipe for example)
    if (!isBouncingFar)
    {
        for (std::list<WorldObject*>::iterator itr = tempTargets.begin(); itr != tempTargets.end();)
        {
            std::list<WorldObject*>::iterator checkItr = itr++;
            if (!m_caster->HasInArc(static_cast<float>(M_PI), *checkItr))
                tempTargets.erase(checkItr);
        }
    }

    while (chainTargets)
    {
        // try to get unit for next chain jump
        std::list<WorldObject*>::iterator foundItr = tempTargets.end();
        // get unit with highest hp deficit in dist
        if (isChainHeal)
        {
            uint32 maxHPDeficit = 0;
            for (std::list<WorldObject*>::iterator itr = tempTargets.begin(); itr != tempTargets.end(); ++itr)
            {
                if (Unit* unitTarget = (*itr)->ToUnit())
                {
                    uint32 deficit = unitTarget->GetMaxHealth() - unitTarget->GetHealth();
                    if ((deficit > maxHPDeficit || foundItr == tempTargets.end()) && target->IsWithinDist(unitTarget, jumpRadius) && target->IsWithinLOSInMap(unitTarget))
                    {
                        foundItr = itr;
                        maxHPDeficit = deficit;
                    }
                }
            }
        }
        // get closest object
        else
        {
            for (std::list<WorldObject*>::iterator itr = tempTargets.begin(); itr != tempTargets.end(); ++itr)
            {
                if (foundItr == tempTargets.end())
                {
                    if ((!isBouncingFar || target->IsWithinDist(*itr, jumpRadius)) && target->IsWithinLOSInMap(*itr, m_spellInfo))
                        foundItr = itr;
                }
                else if (target->GetDistanceOrder(*itr, *foundItr) && target->IsWithinLOSInMap(*itr, m_spellInfo))
                    foundItr = itr;
            }
        }
        // not found any valid target - chain ends
        if (foundItr == tempTargets.end())
            break;
        target = *foundItr;
        tempTargets.erase(foundItr);
        targets.push_back(target);
        --chainTargets;
    }
}

void Spell::prepareDataForTriggerSystem(AuraEffect const* /*triggeredByAura*/)
{
    //==========================================================================================
    // Now fill data for trigger system, need know:
    // can spell trigger another or not (m_canTrigger)
    // Create base triggers flags for Attacker and Victim (m_procAttacker, m_procVictim and m_procEx)
    //==========================================================================================

    m_procVictim = m_procAttacker = 0;
    // Get data for type of attack and fill base info for trigger
    switch (m_spellInfo->DmgClass)
    {
        case SPELL_DAMAGE_CLASS_MELEE:
            m_procAttacker = PROC_FLAG_DONE_SPELL_MELEE_DMG_CLASS;
            if (m_attackType == OFF_ATTACK)
                m_procAttacker |= PROC_FLAG_DONE_OFFHAND_ATTACK;
            else
                m_procAttacker |= PROC_FLAG_DONE_MAINHAND_ATTACK;
            m_procVictim   = PROC_FLAG_TAKEN_SPELL_MELEE_DMG_CLASS;
            break;
        case SPELL_DAMAGE_CLASS_RANGED:
            // Auto attack
            if (m_spellInfo->AttributesEx2 & SPELL_ATTR2_AUTOREPEAT_FLAG)
            {
                m_procAttacker = PROC_FLAG_DONE_RANGED_AUTO_ATTACK;
                m_procVictim   = PROC_FLAG_TAKEN_RANGED_AUTO_ATTACK;
            }
            else // Ranged spell attack
            {
                m_procAttacker = PROC_FLAG_DONE_SPELL_RANGED_DMG_CLASS;
                m_procVictim   = PROC_FLAG_TAKEN_SPELL_RANGED_DMG_CLASS;
            }
            break;
        default:
            if (m_spellInfo->EquippedItemClass == ITEM_CLASS_WEAPON &&
                m_spellInfo->EquippedItemSubClassMask & (1<<ITEM_SUBCLASS_WEAPON_WAND)
                && m_spellInfo->AttributesEx2 & SPELL_ATTR2_AUTOREPEAT_FLAG) // Wands auto attack
            {
                m_procAttacker = PROC_FLAG_DONE_RANGED_AUTO_ATTACK;
                m_procVictim   = PROC_FLAG_TAKEN_RANGED_AUTO_ATTACK;
            }
            // For other spells trigger procflags are set in Spell::DoAllEffectOnTarget
            // Because spell positivity is dependant on target
    }
    m_procEx = PROC_EX_NONE;

    // Hunter trap spells - activation proc for Lock and Load, Entrapment and Misdirection
    if (m_spellInfo->SpellFamilyName == SPELLFAMILY_HUNTER &&
        (m_spellInfo->SpellFamilyFlags[0] & 0x18 ||     // Freezing and Frost Trap, Freezing Arrow
        m_spellInfo->Id == 63487 ||                     // Ice Trap trigger
        m_spellInfo->Id == 57879 ||                     // Snake Trap - done this way to avoid double proc
        m_spellInfo->SpellFamilyFlags[2] & 0x00024000)) // Explosive and Immolation Trap
    {
        m_procAttacker |= PROC_FLAG_DONE_TRAP_ACTIVATION;
        m_procVictim |= PROC_FLAG_TAKEN_SPELL_MAGIC_DMG_CLASS_NEG;
    }

    /* Effects which are result of aura proc from triggered spell cannot proc
        to prevent chain proc of these spells */

    // Hellfire Effect - trigger as DOT
    if (m_spellInfo->SpellFamilyName == SPELLFAMILY_WARLOCK && m_spellInfo->SpellFamilyFlags[0] & 0x00000040)
    {
        m_procAttacker = PROC_FLAG_DONE_PERIODIC;
        m_procVictim   = PROC_FLAG_TAKEN_PERIODIC;
    }

    // Ranged autorepeat attack is set as triggered spell - ignore it
    if (!(m_procAttacker & PROC_FLAG_DONE_RANGED_AUTO_ATTACK))
    {
        if (_triggeredCastFlags & TRIGGERED_DISALLOW_PROC_EVENTS &&
            (m_spellInfo->AttributesEx2 & SPELL_ATTR2_TRIGGERED_CAN_TRIGGER_PROC ||
            m_spellInfo->AttributesEx3 & SPELL_ATTR3_TRIGGERED_CAN_TRIGGER_PROC_2))
            m_procEx |= PROC_EX_INTERNAL_CANT_PROC;
        else if (_triggeredCastFlags & TRIGGERED_DISALLOW_PROC_EVENTS)
            m_procEx |= PROC_EX_INTERNAL_TRIGGERED;
    }
    // Totem casts require spellfamilymask defined in spell_proc_event to proc
    if (m_originalCaster && m_caster != m_originalCaster && m_caster->GetTypeId() == TYPEID_UNIT && m_caster->ToCreature()->isTotem() && m_caster->IsControlledByPlayer())
        m_procEx |= PROC_EX_INTERNAL_REQ_FAMILY;
}

void Spell::CleanupTargetList()
{
    m_UniqueTargetInfo.clear();
    m_UniqueGOTargetInfo.clear();
    m_UniqueItemInfo.clear();
    m_UniqueTargetInfoMap.clear();
    m_delayMoment = 0;
}

void Spell::AddUnitTarget(Unit* target, uint32 effectMask, bool checkIfValid /*= true*/, bool implicit /*= true*/)
{

    std::map<Unit*, int>::iterator iterTarget, iterEnd;
    iterEnd = m_UniqueTargetInfoMap.end();
    iterTarget = m_UniqueTargetInfoMap.find(target);


    auto iterList = m_UniqueTargetInfo.begin();

    if (iterTarget != iterEnd)
    {
        std::advance(iterList, iterTarget->second);
        iterList->effectMask |= effectMask;
        iterList->checkIfValid |= checkIfValid;
        iterList->implicit &= implicit;
    }
    else
    {
        TargetInfo targetInfo;
        targetInfo.target     = target;
        targetInfo.targetGUID = target->GetGUID();                  // Store target GUID
        targetInfo.timeDelay  = 0LL;
        targetInfo.missCondition = SPELL_MISS_NONE;
        targetInfo.reflectResult = SPELL_MISS_NONE;
        targetInfo.effectMask = effectMask;                         // Store all effects not immune
        //    targetInfo.checkMask  = effectMask;                   // Store all effects to check spell immunity
        targetInfo.processed  = false;                              // Effects not apply on target
        targetInfo.alive      = target->isAlive();
        targetInfo.damage     = 0;
        targetInfo.crit       = false;
        targetInfo.scaleAura  = false;
        targetInfo.checkIfValid = checkIfValid;
        targetInfo.implicit   = implicit;
        m_UniqueTargetInfo.push_back(targetInfo);
        m_UniqueTargetInfoMap[target] = m_UniqueTargetInfo.size() - 1;
    }
}

void Spell::FilterTargets()
{
    for (auto iterList = m_UniqueTargetInfo.begin(); iterList != m_UniqueTargetInfo.end();)
    {
        TargetInfo* targetInfo = &*iterList;
        Unit* target = targetInfo->target;

        for (uint32 effIndex = 0; effIndex < MAX_SPELL_EFFECTS; ++effIndex)
            if (!m_spellInfo->Effects[effIndex].IsEffect() || !CheckEffectTarget(target, effIndex))
                targetInfo->effectMask &= ~(1 << effIndex);

        // no effects left
        if (!targetInfo->effectMask)
        {
            iterList = m_UniqueTargetInfo.erase(iterList);
            continue;
        }

        if (targetInfo->checkIfValid && !_redirected)
            if (m_spellInfo->CheckTarget(m_caster, target, targetInfo->implicit) != SPELL_CAST_OK)
            {
                iterList = m_UniqueTargetInfo.erase(iterList);
                continue;
            }

            uint64 targetGUID = target->GetGUID();

            if (m_originalCaster)
                targetInfo->missCondition = m_originalCaster->SpellHitResult(target, m_spellInfo, m_canReflect, targetInfo->effectMask);
            else
                targetInfo->missCondition = SPELL_MISS_EVADE; //SPELL_MISS_NONE;

            if (targetInfo->missCondition != SPELL_MISS_REFLECT)
            {
                // Check for effect immune skip if immuned
                for (uint32 effIndex = 0; effIndex < MAX_SPELL_EFFECTS; ++effIndex)
                    if (targetInfo->effectMask & (1 << effIndex))
                        if (target->IsImmunedToSpellEffect(m_spellInfo, effIndex))
                            targetInfo->effectMask &= ~(1 << effIndex);
            }

            if (m_originalCaster)
            {
                if (targetInfo->missCondition != SPELL_MISS_REFLECT)
                {
                    targetInfo->missCondition = m_originalCaster->SpellHitResult(target, m_spellInfo, false, targetInfo->effectMask);
                    if (m_skipCheck && targetInfo->missCondition != SPELL_MISS_IMMUNE)
                        targetInfo->missCondition = SPELL_MISS_NONE;
                }
            }
            else
                targetInfo->missCondition = SPELL_MISS_EVADE; //SPELL_MISS_NONE;

            if (m_auraScaleMask && targetInfo->effectMask == m_auraScaleMask && m_caster != target)
            {
                SpellInfo const* auraSpell = sSpellMgr->GetSpellInfo(sSpellMgr->GetFirstSpellInChain(m_spellInfo->Id));
                if (uint32(target->getLevel() + 10) >= auraSpell->SpellLevel)
                    targetInfo->scaleAura = true;
            }

            // Spell have speed - need calculate incoming time
            // Incoming time is zero for self casts. At least I think so.
            // Spell have speed - need calculate incoming time
            float speed = m_spellValue->Speed;
            if (speed > 0.0f && m_caster != target)
            {
                // calculate spell incoming interval
                // TODO: this is a hack
                float dist = m_caster->GetDistance(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ());
                if (dist < 5.0f)
                    dist = 5.0f;

                if (!(m_spellInfo->AttributesEx9 & SPELL_ATTR9_SPECIAL_DELAY_CALCULATION))
                    targetInfo->timeDelay = uint64(floor(dist / speed * 1000.0f));
                else
                    targetInfo->timeDelay = uint64(speed * 1000.0f);

                // Calculate minimum incoming time
                if (m_delayMoment == 0 || m_delayMoment > targetInfo->timeDelay)
                    m_delayMoment = targetInfo->timeDelay;
            }
            else // targetInfo->timeDelay = 0LL;
            {
                targetInfo->timeDelay = GetCustomDelay(m_spellInfo);
                if (m_delayMoment == 0 || m_delayMoment > targetInfo->timeDelay)
                    m_delayMoment = targetInfo->timeDelay;
            }

            // If target reflect spell back to caster
            if (targetInfo->missCondition == SPELL_MISS_REFLECT)
            {
                targetInfo->reflectResult = target->SpellHitResult(m_caster, m_spellInfo, m_canReflect, targetInfo->effectMask);

                if (targetInfo->reflectResult != SPELL_MISS_REFLECT)
                {

                    // Check for effect immune skip if immuned
                    for (uint32 effIndex = 0; effIndex < MAX_SPELL_EFFECTS; ++effIndex)
                        if (targetInfo->effectMask & (1 << effIndex))
                            if (m_caster->IsImmunedToSpellEffect(m_spellInfo, effIndex))
                                targetInfo->effectMask &= ~(1 << effIndex);

                    //targetInfo.effectMask = effectMask;             // Immune effects removed from mask

                    // Calculate reflected spell result on caster
                    targetInfo->reflectResult = target->SpellHitResult(m_caster, m_spellInfo, false, targetInfo->effectMask);
                }

                if (targetInfo->reflectResult == SPELL_MISS_REFLECT)     // Impossible reflect again, so simply deflect spell
                    targetInfo->reflectResult = SPELL_MISS_PARRY;

                // Increase time interval for reflected spells by 1.5
                targetInfo->timeDelay += targetInfo->timeDelay >> 1;
            }
            else
                targetInfo->reflectResult = SPELL_MISS_NONE;

            if (m_spellInfo->IsChanneled())
            {
                m_channelTargetEffectMask |= targetInfo->effectMask;
            }
            else if (m_auraScaleMask)
            {
                // remove targets which did not pass min level check
                if (/*m_auraScaleMask && */targetInfo->effectMask == m_auraScaleMask)
                {
                    // Do not check for selfcast
                    if (!targetInfo->scaleAura && targetGUID != m_caster->GetGUID())
                    {
                        m_levelCheck = true;
                        //    m_UniqueTargetInfoMap.erase(iterTarget++);
                        iterList = m_UniqueTargetInfo.erase(iterList);
                        continue;
                    }
                }
            }
            ++iterList;
    }
}

void Spell::AddGOTarget(GameObject* go, uint32 effectMask)
{
    for (uint32 effIndex = 0; effIndex < MAX_SPELL_EFFECTS; ++effIndex)
    {
        if (!m_spellInfo->Effects[effIndex].IsEffect())
            effectMask &= ~(1 << effIndex);
        else
        {
            switch (m_spellInfo->Effects[effIndex].Effect)
            {
            case SPELL_EFFECT_GAMEOBJECT_DAMAGE:
            case SPELL_EFFECT_GAMEOBJECT_REPAIR:
            case SPELL_EFFECT_GAMEOBJECT_SET_DESTRUCTION_STATE:
                if (go->GetGoType() != GAMEOBJECT_TYPE_DESTRUCTIBLE_BUILDING)
                    effectMask &= ~(1 << effIndex);
                break;
            default:
                break;
            }
        }
    }

    if (!effectMask)
        return;

    uint64 targetGUID = go->GetGUID();

    // Lookup target in already in list
    for (auto ihit = m_UniqueGOTargetInfo.begin(); ihit != m_UniqueGOTargetInfo.end(); ++ihit)
    {
        if (targetGUID == ihit->targetGUID)                 // Found in list
        {
            ihit->effectMask |= effectMask;                 // Add only effect mask
            return;
        }
    }

    // This is new target calculate data for him

    GOTargetInfo target;
    target.targetGUID = targetGUID;
    target.effectMask = effectMask;
    target.processed  = false;                              // Effects not apply on target

    // Spell have speed - need calculate incoming time
    if (m_spellValue->Speed > 0.0f)
    {
        // calculate spell incoming interval
        float dist = m_caster->GetDistance(go->GetPositionX(), go->GetPositionY(), go->GetPositionZ());
        if (dist < 5.0f)
            dist = 5.0f;

        if (!(m_spellInfo->AttributesEx9 & SPELL_ATTR9_SPECIAL_DELAY_CALCULATION))
            target.timeDelay = uint64(floor(dist / m_spellValue->Speed * 1000.0f));
        else
            target.timeDelay = uint64(m_spellValue->Speed * 1000.0f);

        if (m_delayMoment == 0 || m_delayMoment > target.timeDelay)
            m_delayMoment = target.timeDelay;
    }
    else
        target.timeDelay = 0LL;

    // Add target to list
    m_UniqueGOTargetInfo.push_back(target);
}

void Spell::AddItemTarget(Item* item, uint32 effectMask)
{
    for (uint32 effIndex = 0; effIndex < MAX_SPELL_EFFECTS; ++effIndex)
        if (!m_spellInfo->Effects[effIndex].IsEffect())
            effectMask &= ~(1 << effIndex);

    // no effects left
    if (!effectMask)
        return;

    // Lookup target in already in list
    for (auto ihit = m_UniqueItemInfo.begin(); ihit != m_UniqueItemInfo.end(); ++ihit)
    {
        if (item == ihit->item)                            // Found in list
        {
            ihit->effectMask |= effectMask;                 // Add only effect mask
            return;
        }
    }

    // This is new target add data

    ItemTargetInfo target;
    target.item       = item;
    target.effectMask = effectMask;

    m_UniqueItemInfo.push_back(target);
}

void Spell::AddDestTarget(SpellDestination const& dest, uint32 effIndex)
{
    m_destTargets[effIndex] = dest;
}

void Spell::DoAllEffectOnTarget(TargetInfo* target)
{
    if (!target || target->processed)
        return;

    target->processed = true;                               // Target checked in apply effects procedure

    // Get mask of effects for target
    uint8 mask = target->effectMask;

    Unit* unit = m_caster->GetGUID() == target->targetGUID ? m_caster : ObjectAccessor::GetUnit(*m_caster, target->targetGUID);
    if (!unit)
    {
        uint8 farMask = 0;
        // create far target mask
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            if (m_spellInfo->Effects[i].IsFarUnitTargetEffect())
                if ((1 << i) & mask)
                    farMask |= (1 << i);

        if (!farMask)
            return;
        // find unit in world
        unit = ObjectAccessor::FindUnit(target->targetGUID);
        if (!unit)
            return;

        // do far effects on the unit
        // can't use default call because of threading, do stuff as fast as possible
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            if (farMask & (1 << i))
                HandleEffects(unit, NULL, NULL, i, SPELL_EFFECT_HANDLE_HIT_TARGET_FIRST);

        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            if (farMask & (1 << i))
                HandleEffects(unit, NULL, NULL, i, SPELL_EFFECT_HANDLE_HIT_TARGET);
        return;
    }

    if (unit->isAlive() != target->alive)
        return;

    if (getState() == SPELL_STATE_DELAYED && !m_spellInfo->IsPositive() && (getMSTime() - target->timeDelay) <= unit->m_lastSanctuaryTime)
        return;                                             // No missinfo in that case

    // Get original caster (if exist) and calculate damage/healing from him data
    Unit* caster = m_originalCaster ? m_originalCaster : m_caster;

    // Skip if m_originalCaster not avaiable
    if (!caster)
        return;

    SpellMissInfo missInfo = target->missCondition;

    // Need init unitTarget by default unit (can changed in code on reflect)
    // Or on missInfo != SPELL_MISS_NONE unitTarget undefined (but need in trigger subsystem)
    unitTarget = unit;

    // Reset damage/healing counter
    m_damage = target->damage;
    m_healing = -target->damage;

    // Fill base trigger info
    uint32 procAttacker = m_procAttacker;
    uint32 procVictim   = m_procVictim;
    uint32 procEx = m_procEx;

    m_spellAura = NULL; // Set aura to null for every target-make sure that pointer is not used for unit without aura applied

                            //Spells with this flag cannot trigger if effect is casted on self
    bool canEffectTrigger = !(m_spellInfo->AttributesEx3 & SPELL_ATTR3_CANT_TRIGGER_PROC) && unitTarget->CanProc() && CanExecuteTriggersOnHit(mask);
    Unit* spellHitTarget = NULL;

    if (missInfo == SPELL_MISS_NONE)                          // In case spell hit target, do all effect on that target
        spellHitTarget = unit;
    else if (missInfo == SPELL_MISS_REFLECT)                // In case spell reflect from target, do all effect on caster (if hit)
    {
        if (target->reflectResult == SPELL_MISS_NONE)       // If reflected spell hit caster -> do all effect on him
        {
            spellHitTarget = m_caster;
            if (m_caster->GetTypeId() == TYPEID_UNIT)
                m_caster->ToCreature()->LowerPlayerDamageReq(target->damage);
        }
        else
            if (target->reflectResult == SPELL_MISS_PARRY)
                return;
    }

    // Check for SPELL_ATTR7_INTERRUPT_ONLY_NONPLAYER
    if (m_spellInfo->HasAttribute(SPELL_ATTR7_INTERRUPT_ONLY_NONPLAYER) && unit->GetTypeId() != TYPEID_PLAYER)
        caster->CastSpell(unit, SPELL_INTERRUPT_NONPLAYER, true);

    if (spellHitTarget)
    {
        SpellMissInfo missInfo2 = DoSpellHitOnUnit(spellHitTarget, mask, target->scaleAura);
        if (missInfo2 != SPELL_MISS_NONE)
        {
            m_caster->SendSpellMiss(unit, m_spellInfo->Id, missInfo2);
            m_damage = 0;
            spellHitTarget = NULL;
        }
    }

    // Do not take combo points on dodge and miss
    if (missInfo != SPELL_MISS_NONE && m_needComboPoints &&
            m_targets.GetUnitTargetGUID() == target->targetGUID)
    {
        m_needComboPoints = false;
        // Restore spell mods for a miss/dodge/parry Cold Blood
        // TODO: check how broad this rule should be
        if (m_caster->GetTypeId() == TYPEID_PLAYER && (missInfo == SPELL_MISS_MISS ||
                missInfo == SPELL_MISS_DODGE || missInfo == SPELL_MISS_PARRY))
            m_caster->ToPlayer()->RestoreSpellMods(this, 14177);
    }

    // Trigger info was not filled in spell::preparedatafortriggersystem - we do it now
    if (canEffectTrigger && !procAttacker && !procVictim)
    {
        bool positive = true;
        if (m_damage > 0)
            positive = false;
        else if (!m_healing)
        {
            for (uint8 i = 0; i< MAX_SPELL_EFFECTS; ++i)
                // If at least one effect negative spell is negative hit
                if (mask & (1<<i) && !m_spellInfo->IsPositiveEffect(i))
                {
                    positive = false;
                    break;
                }
        }
        switch (m_spellInfo->DmgClass)
        {
            case SPELL_DAMAGE_CLASS_MAGIC:
                if (positive)
                {
                    procAttacker |= PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_POS;
                    procVictim   |= PROC_FLAG_TAKEN_SPELL_MAGIC_DMG_CLASS_POS;
                }
                else
                {
                    procAttacker |= PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_NEG;
                    procVictim   |= PROC_FLAG_TAKEN_SPELL_MAGIC_DMG_CLASS_NEG;
                }
            break;
            case SPELL_DAMAGE_CLASS_NONE:
                if (positive)
                {
                    procAttacker |= PROC_FLAG_DONE_SPELL_NONE_DMG_CLASS_POS;
                    procVictim   |= PROC_FLAG_TAKEN_SPELL_NONE_DMG_CLASS_POS;
                    if (!(m_spellSchoolMask & SPELL_SCHOOL_MASK_NORMAL))
                    {
                        procAttacker |= PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_POS;
                        procVictim   |= PROC_FLAG_TAKEN_SPELL_MAGIC_DMG_CLASS_POS;
                    }
                }
                else
                {
                    procAttacker |= PROC_FLAG_DONE_SPELL_NONE_DMG_CLASS_NEG;
                    procVictim   |= PROC_FLAG_TAKEN_SPELL_NONE_DMG_CLASS_NEG;
                    if (!(m_spellSchoolMask & SPELL_SCHOOL_MASK_NORMAL))
                    {
                        procAttacker |= PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_NEG;
                        procVictim   |= PROC_FLAG_TAKEN_SPELL_MAGIC_DMG_CLASS_NEG;
                    }
                }
            break;
        }
    }
    CallScriptOnHitHandlers();

    // All calculated do it!
    // Do healing and triggers
    if (m_healing > 0)
    {
        bool crit = caster->isSpellCrit(unitTarget, m_spellInfo, m_spellSchoolMask);

        //healing crits
        if (crit)
            setIsCriticalStrike(2);
        else
            setIsCriticalStrike(1);
        // Victory Rush, HACKFIX for healing effect
        if (m_spellInfo->Id == 34428)
            crit = false;

        uint32 addhealth = m_healing;
        if (crit)
        {
            procEx |= PROC_EX_CRITICAL_HIT;
            addhealth = caster->SpellCriticalHealingBonus(m_spellInfo, addhealth, NULL);
        }
        else
            procEx |= PROC_EX_NORMAL_HIT;

        // Do triggers for unit (reflect triggers passed on hit phase for correct drop charge)
        if (canEffectTrigger && missInfo != SPELL_MISS_REFLECT)
            caster->ProcDamageAndSpell(unitTarget, procAttacker, procVictim, procEx, addhealth, 0, 0, 0, 0, m_attackType, m_spellInfo, m_triggeredByAuraSpell, NULL, true);

        int32 gain = caster->HealBySpell(unitTarget, m_spellInfo, addhealth, crit);
        unitTarget->getHostileRefManager().threatAssist(caster, float(gain) * 0.5f, m_spellInfo);
        m_healing = gain;
    }
    // Do damage and triggers
    else if (m_damage > 0)
    {
        // Fill base damage struct (unitTarget - is real spell target)
        SpellNonMeleeDamage damageInfo(caster, unitTarget, m_spellInfo->Id, m_spellSchoolMask);

        // Add bonuses and fill damageInfo struct
        if (m_caster->GetEntry() == 27893) // Dancing Rune Weapon
        {
            if (Unit* owner = m_caster->GetOwner())
                owner->CalculateSpellDamageTaken(&damageInfo, m_damage, m_spellInfo, m_attackType, target->crit);
        }
        else
            caster->CalculateSpellDamageTaken(&damageInfo, m_damage, m_spellInfo, m_attackType, target->crit);
        caster->DealDamageMods(damageInfo.target, damageInfo.damage, &damageInfo.absorb);

        damageInfo.damage = uint32(sChallengeModeMgr->ApplyDamageMultiplier(caster, damageInfo.damage, GetSpellInfo()->Id));

        // Send log damage message to client
        caster->SendSpellNonMeleeDamageLog(&damageInfo);

        procEx |= createProcExtendMask(&damageInfo, missInfo);
        if (_interruptedSpell)
            procEx |= PROC_EX_INTERRUPT;
        else if (_refreshedAura)
            procEx |= PROC_EX_REFRESH_AURA;

        procVictim |= PROC_FLAG_TAKEN_DAMAGE;

        // Do triggers for unit (reflect triggers passed on hit phase for correct drop charge)
        if (canEffectTrigger && missInfo != SPELL_MISS_REFLECT)
        {
            caster->ProcDamageAndSpellForAttacker(unitTarget, procAttacker, procVictim, procEx, damageInfo.damage, damageInfo.absorb, damageInfo.resist, damageInfo.blocked, damageInfo.cleanDamage, m_attackType, m_spellInfo, m_triggeredByAuraSpell, NULL, false, false, mask);
            // Will have to see if this turns out to be correct
            // temporary commented out, cause of killing streak consuming own proc
            //if (caster->GetOwner() && caster->isHunterPet())
            //    caster->GetOwner()->ProcDamageAndSpell(unitTarget, procAttacker, procVictim, procEx, damageInfo.damage, damageInfo.absorb, damageInfo.resist, damageInfo.blocked, damageInfo.cleanDamage, m_attackType, m_spellInfo, m_triggeredByAuraSpell, _interruptedSpell);

            if (caster->GetTypeId() == TYPEID_PLAYER && (m_spellInfo->Attributes & SPELL_ATTR0_STOP_ATTACK_TARGET) == 0 &&
               (m_spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MELEE || m_spellInfo->DmgClass == SPELL_DAMAGE_CLASS_RANGED))
                caster->ToPlayer()->CastItemCombatSpell(unitTarget, m_attackType, procVictim, procEx);
        }

        m_damage = damageInfo.damage;
        m_true_damage = damageInfo.damage + damageInfo.absorb;

        caster->DealSpellDamage(&damageInfo, true);

        if (canEffectTrigger && missInfo != SPELL_MISS_REFLECT)
            caster->ProcDamageAndSpellForVictim(unitTarget, procAttacker, procVictim, procEx, damageInfo.damage, damageInfo.absorb, damageInfo.resist, damageInfo.blocked, damageInfo.cleanDamage, m_attackType, m_spellInfo, m_triggeredByAuraSpell, NULL, false, false, mask);
    }
    // Passive spell hits/misses or active spells only misses (only triggers)
    else
    {
        // Fill base damage struct (unitTarget - is real spell target)
        SpellNonMeleeDamage damageInfo(caster, unitTarget, m_spellInfo->Id, m_spellSchoolMask);
        procEx |= createProcExtendMask(&damageInfo, missInfo);
        if (_interruptedSpell)
            procEx |= PROC_EX_INTERRUPT;
        else if (_refreshedAura)
            procEx |= PROC_EX_REFRESH_AURA;

        // Do triggers for unit (reflect triggers passed on hit phase for correct drop charge)
        if (canEffectTrigger && missInfo != SPELL_MISS_REFLECT)
            caster->ProcDamageAndSpell(unit, procAttacker, procVictim, procEx, 0, 0, 0, 0, 0, m_attackType, m_spellInfo, m_triggeredByAuraSpell, _interruptedSpell);

        // Failed Pickpocket, reveal rogue
        if (missInfo == SPELL_MISS_RESIST && m_spellInfo->AttributesCu & SPELL_ATTR0_CU_PICKPOCKET && unitTarget->GetTypeId() == TYPEID_UNIT)
        {
            m_caster->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TALK);
            if (unitTarget->ToCreature()->IsAIEnabled)
                unitTarget->ToCreature()->AI()->AttackStart(m_caster);
        }
    }

    if (missInfo != SPELL_MISS_EVADE && !m_caster->IsFriendlyTo(unit) && (!m_spellInfo->IsPositive() || m_spellInfo->HasEffect(SPELL_EFFECT_DISPEL)))
    {
        bool combatStart = !(m_spellInfo->AttributesEx3 & SPELL_ATTR3_NO_INITIAL_AGGRO) && !(m_spellInfo->AttributesEx & SPELL_ATTR1_NO_THREAT)
            && (!(m_spellInfo->AttributesEx4 & SPELL_ATTR4_TRIGGERED) || (m_triggeredByAuraSpell && m_triggeredByAuraSpell->IsChanneled()));

        m_caster->CombatStart(unit, combatStart);
        // Log last spell that caused combat
        if (combatStart)
        {
            if (m_caster->GetTypeId() == TYPEID_PLAYER)
            {
                m_caster->ToPlayer()->m_lastCombatspell = m_spellInfo->Id;
                m_caster->ToPlayer()->m_lastCombatCaster = m_caster->GetName();
            }
            if (unit->GetTypeId() == TYPEID_PLAYER)
            {
                unit->ToPlayer()->m_lastCombatspell = m_spellInfo->Id;
                unit->ToPlayer()->m_lastCombatCaster = m_caster->GetName();
            }
        }

        if (m_spellInfo->AttributesCu & SPELL_ATTR0_CU_AURA_CC)
            if (!unit->IsStandState())
                unit->SetStandState(UNIT_STAND_STATE_STAND);
    }

    if (spellHitTarget)
    {
        //AI functions
        if (spellHitTarget->GetTypeId() == TYPEID_UNIT)
        {
            if (spellHitTarget->ToCreature()->IsAIEnabled)
                spellHitTarget->ToCreature()->AI()->SpellHit(m_caster, m_spellInfo);

            // cast at creature (or GO) quest objectives update at successful cast finished (+channel finished)
            // ignore pets or autorepeat/melee casts for speed (not exist quest for spells (hm...)
            if (m_originalCaster && m_originalCaster->IsControlledByPlayer() && !spellHitTarget->ToCreature()->isPet() && !IsAutoRepeat() && !IsNextMeleeSwingSpell() && !IsChannelActive())
                if (Player* p = m_originalCaster->GetCharmerOrOwnerPlayerOrPlayerItself())
                    p->CastedCreatureOrGO(spellHitTarget->GetEntry(), spellHitTarget->GetGUID(), m_spellInfo->Id);
        }

        if (m_caster->GetTypeId() == TYPEID_UNIT && m_caster->ToCreature()->IsAIEnabled)
            m_caster->ToCreature()->AI()->SpellHitTarget(spellHitTarget, m_spellInfo);

        // Needs to be called after dealing damage/healing to not remove breaking on damage auras
        DoTriggersOnSpellHit(spellHitTarget, mask);

        bool flag_for_pvp{ true };
        // if target is flaged for pvp also flag caster if a player
        if (unit->IsPvP() && m_caster->GetTypeId() == TYPEID_PLAYER && unit->GetTypeId() == TYPEID_PLAYER)
            if (m_caster->ToPlayer()->GetTeam() != unit->ToPlayer()->GetTeam())
            {
                if (m_caster->ToPlayer()->duel)
                    if (m_caster->ToPlayer()->duel->accepted)
                        if (m_caster->ToPlayer()->duel->opponent)
                            if (m_caster->ToPlayer()->duel->opponent->GetGUID() == unit->ToPlayer()->GetGUID())
                            {
                                //This spell is casted against a duel opponent from the opposing faction, do not flag them for pvp
                                //TC_LOG_ERROR("entities.player", "Spell victim is duel opponent of opposing faction.");
                                flag_for_pvp = false;
                            }

                if (flag_for_pvp)
                {
                    m_caster->ToPlayer()->UpdatePvP(true);
                    unit->ToPlayer()->UpdatePvP(true);
                }
            }

        CallScriptAfterHitHandlers();
    }
}

SpellMissInfo Spell::DoSpellHitOnUnit(Unit* unit, uint32 effectMask, bool scaleAura)
{
    if (!unit || !effectMask)
        return SPELL_MISS_EVADE;

    // For delayed spells immunity may be applied between missile launch and hit - check immunity for that case
    if ((m_spellValue->Speed || m_delayMoment) && (unit->IsImmunedToDamage(m_spellInfo, true, effectMask) || unit->IsImmunedToSpell(m_spellInfo, m_caster, effectMask)))
        return SPELL_MISS_IMMUNE;

    if (m_originalCaster && m_spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MAGIC)
    {
        SpellMissInfo missInfo = m_originalCaster->MagicSpellHitResult(unit, m_spellInfo, effectMask);
        if (missInfo == SPELL_MISS_MISS)
            return SPELL_MISS_MISS;
    }

    // disable effects to which unit is immune
    SpellMissInfo returnVal = SPELL_MISS_IMMUNE;
    for (uint32 effectNumber = 0; effectNumber < MAX_SPELL_EFFECTS; ++effectNumber)
        if (effectMask & (1 << effectNumber))
            if (unit->IsImmunedToSpellEffect(m_spellInfo, effectNumber))
                effectMask &= ~(1 << effectNumber);

    if (!effectMask)
        return returnVal;

    PrepareScriptHitHandlers();
    CallScriptBeforeHitHandlers();

    if (Player* player = unit->ToPlayer())
    {
        player->StartTimedAchievement(ACHIEVEMENT_TIMED_TYPE_SPELL_TARGET, m_spellInfo->Id);
        player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET, m_spellInfo->Id, 0, 0, m_caster);
        player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET2, m_spellInfo->Id, 0, 0, unit);
    }

    if (Player* player = m_caster->ToPlayer())
    {
        player->StartTimedAchievement(ACHIEVEMENT_TIMED_TYPE_SPELL_CASTER, m_spellInfo->Id);
        player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_CAST_SPELL2, m_spellInfo->Id, 0, 0, unit);
    }

    if (m_caster != unit)
    {
        // Recheck  UNIT_FLAG_NON_ATTACKABLE for delayed spells
        if ((m_spellValue->Speed || m_delayMoment) && unit->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE) && unit->GetCharmerOrOwnerGUID() != m_caster->GetGUID())
            return SPELL_MISS_EVADE;

        // There might be another attribute used for this (NO_INITIAL_AGGRO | NO_INITIAL_THREAT)
        if (m_caster->_IsValidAttackTarget(unit, m_spellInfo))
        {
            if (!(m_spellInfo->AttributesEx & SPELL_ATTR1_NOT_BREAK_STEALTH) || m_spellInfo->AttributesCu & SPELL_ATTR0_CU_AURA_CC)
                unit->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_HITBYSPELL);
        }
        else if (m_caster->IsFriendlyTo(unit) /*&& (!m_originalCaster || m_originalCaster->IsFriendlyTo(unit))*/  /*&& !m_spellInfo->IsAffectingArea(effectMask)*/)
        {
            // for delayed spells ignore negative spells (after duel end) for friendly targets
            // TODO: this cause soul transfer bugged
            if ((m_spellValue->Speed || m_delayMoment) && unit->GetTypeId() == TYPEID_PLAYER && !m_spellInfo->IsPositive(effectMask) && m_spellInfo->CheckTarget(m_caster, unit) != SPELL_CAST_OK)
                return SPELL_MISS_EVADE;

            // assisting case, healing and resurrection
            if (unit->HasUnitState(UNIT_STATE_ATTACK_PLAYER))
            {
                m_caster->SetContestedPvP();
                if (m_caster->GetTypeId() == TYPEID_PLAYER)
                    m_caster->ToPlayer()->UpdatePvP(true);
            }
            if (unit->isInCombat() && !(m_spellInfo->AttributesEx3 & SPELL_ATTR3_NO_INITIAL_AGGRO) && !(m_spellInfo->AttributesEx & SPELL_ATTR1_NO_THREAT)
                && (!(m_spellInfo->AttributesEx4 & SPELL_ATTR4_TRIGGERED) || (m_triggeredByAuraSpell && m_triggeredByAuraSpell->IsChanneled())))
            {
                m_caster->SetInCombatState(unit->GetCombatTimer() > 0, unit);
                unit->getHostileRefManager().threatAssist(m_caster, 0.0f);
                if (m_caster->GetTypeId() == TYPEID_PLAYER)
                {
                    m_caster->ToPlayer()->m_lastCombatspell = m_spellInfo->Id;
                    m_caster->ToPlayer()->m_lastCombatCaster = m_caster->GetName();
                }
            }
        }
    }

    uint8 aura_effmask = 0;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (effectMask & (1 << i) && m_spellInfo->Effects[i].IsUnitOwnedAuraEffect())
            aura_effmask |= 1 << i;

    if (aura_effmask)
    {
        // Get Data Needed for Diminishing Returns, some effects may have multiple auras, so this must be done on spell hit, not aura add
        m_diminishGroups = GetDiminishingReturnsGroupForSpell(m_spellInfo, m_triggeredByAuraSpell);
        if (m_diminishGroups.size())
        {
            DiminishingLevels highestLevel = DIMINISHING_LEVEL_1;
            for (std::list<DiminishingGroup>::iterator itr = m_diminishGroups.begin(); itr != m_diminishGroups.end(); itr++)
            {
                if (*itr == DIMINISHING_NONE)
                    continue;

                m_diminishLevel = unit->GetDiminishing(*itr);
                if (m_diminishLevel > highestLevel || !m_diminishGroup)
                {
                    highestLevel = m_diminishLevel;
                    m_diminishGroup = *itr;
                }
            }
            if (m_diminishGroup)
            {
                m_diminishLevel = highestLevel;
                DiminishingReturnsType type = GetDiminishingReturnsGroupType(m_diminishGroup);
                // Increase Diminishing on unit, current informations for actually casts will use values above
                if (m_spellInfo->Id == 605 && !(aura_effmask & EFFECT_1) || (type == DRTYPE_PLAYER &&
                    (unit->GetCharmerOrOwnerPlayerOrPlayerItself() || (unit->GetTypeId() == TYPEID_UNIT && unit->ToCreature()->GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_ALL_DIMINISH))) ||
                    type == DRTYPE_ALL)
                {
                    for (std::list<DiminishingGroup>::iterator itr = m_diminishGroups.begin(); itr != m_diminishGroups.end(); itr++)
                        unit->IncrDiminishing(*itr);
                }
            }
        }

        // Select rank for aura with level requirements only in specific cases
        // Unit has to be target only of aura effect, both caster and target have to be players, target has to be other than unit target
        SpellInfo const* aurSpellInfo = m_spellInfo;
        int32 basePoints[3];
        if (scaleAura)
        {
            aurSpellInfo = m_spellInfo->GetAuraRankForLevel(unitTarget->getLevel());
            ASSERT(aurSpellInfo);
            for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            {
                basePoints[i] = aurSpellInfo->Effects[i].BasePoints;
                if (m_spellInfo->Effects[i].Effect != aurSpellInfo->Effects[i].Effect)
                {
                    aurSpellInfo = m_spellInfo;
                    break;
                }
            }
        }

        if (m_originalCaster)
        {
            // Hackfix: Also signal Pala guardian and DK rune weapon to attack
            if (unit && unit->ToPlayer() && unit->getClass() == CLASS_PALADIN && unit->IsHostileTo(m_originalCaster->ToUnit()))
            {
                if (Unit* ancient = unit->ToPlayer()->GetPetSummoned(46506))
                {
                    if (!ancient->getVictim() && !unit->getVictim())
                    {
                        ancient->ToCreature()->SetReactState(REACT_ASSIST);
                        ancient->ToCreature()->AI()->AttackStart(m_originalCaster->ToUnit());
                    }
                }
                if (Unit* runeweapon = unit->ToPlayer()->GetPetSummoned(27893))
                {
                    if (!runeweapon->getVictim() && !unit->getVictim())
                    {
                        runeweapon->ToCreature()->SetReactState(REACT_ASSIST);
                        runeweapon->ToCreature()->AI()->AttackStart(m_originalCaster->ToUnit());
                    }
                }
            } // Hackfix end
            
            // special case
            // http://blue.mmo-champion.com/topic/5389/recent-in-game-fixes-june-2009#post6
            switch(GetSpellInfo()->Id)
            {
            case 22570:    // Maim
            case 853:      // Hammer of Justice
            case 5211:     // Bash
                if (unit && unit->ToCreature() && unit->isAlive() && (unit->ToCreature()->GetCreatureTemplate()->MechanicImmuneMask & (1 << (MECHANIC_STUN - 1))))
                {
                    if (unit->ToCreature()->GetCreatureTemplate()->MechanicImmuneMask & (1 << (MECHANIC_INTERRUPT - 1)))
                    {
                        TC_LOG_INFO("spells", "immune to stun and interrupt, should not happen");
                        return SPELL_MISS_IMMUNE;
                    }

                    for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_AUTOREPEAT_SPELL; ++i)
                    {
                        if (Spell* spell = unit->GetCurrentSpell(CurrentSpellTypes(i)))
                        {
                            SpellInfo const* curSpellInfo = spell->m_spellInfo;
                            // check if we can interrupt spell
                            if ((spell->getState() == SPELL_STATE_CASTING
                                || (spell->getState() == SPELL_STATE_PREPARING && spell->GetCastTime() > 0.0f))
                                && curSpellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE
                                && ((i == CURRENT_GENERIC_SPELL && curSpellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_INTERRUPT)
                                || (i == CURRENT_CHANNELED_SPELL && curSpellInfo->ChannelInterruptFlags & CHANNEL_INTERRUPT_FLAG_INTERRUPT)))
                            {
                                spell->cancel();
                            }
                        }
                    }
                    return SPELL_MISS_NONE;
                }
            }

            bool refresh = false;
            m_spellAura = Aura::TryRefreshStackOrCreate(aurSpellInfo, effectMask, unit,
                m_originalCaster, (aurSpellInfo == m_spellInfo)? &m_spellValue->EffectBasePoints[0] : &basePoints[0], m_CastItem, 0, &refresh);
            _refreshedAura = refresh;
            if (m_spellAura)
            {
                // Set aura stack amount to desired value
                if (m_spellValue->AuraStackAmount > 1)
                {
                    if (!refresh)
                        m_spellAura->SetStackAmount(m_spellValue->AuraStackAmount);
                    else
                        m_spellAura->ModStackAmount(m_spellValue->AuraStackAmount);
                }

                m_spellAura->SetChangeBySoulBurn(m_changeBySoulburn);

                // Now Reduce spell duration using data received at spell hit
                //
                int32 duration = m_spellAura->GetMaxDuration();
                int32 limitduration = GetDiminishingReturnsLimitDuration(m_diminishGroup, aurSpellInfo);
                if (GetSpellInfo()->Id == 605 && (m_originalCaster == unit))
                    if (Unit* charm = unit->GetCharm())
                        if (charm->GetTypeId() == TYPEID_UNIT)
                            limitduration = duration; // Mind Control self aura at NPC target shouldn't have limit
                float diminishMod = unit->ApplyDiminishingToDuration(m_diminishGroup, duration, m_originalCaster, m_diminishLevel, limitduration);

                // unit is immune to aura if it was diminished to 0 duration
                if (diminishMod == 0.0f)
                {
                    m_spellAura->Remove();
                    bool found = false;
                    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                        if (effectMask & (1 << i) && m_spellInfo->Effects[i].Effect != SPELL_EFFECT_APPLY_AURA)
                            found = true;
                    if (!found)
                        return SPELL_MISS_IMMUNE;
                }
                else
                {
                    for (std::list<DiminishingGroup>::iterator itr = m_diminishGroups.begin(); itr != m_diminishGroups.end(); itr++)
                        if (UnitAura *unitAura = (UnitAura*)m_spellAura)
                            unitAura->SetDiminishGroup(*itr);

                    bool positive = m_spellAura->GetSpellInfo()->IsPositive(m_spellAura->GetEffectMask());
                    if (AuraApplication* aurApp = m_spellAura->GetApplicationOfTarget(m_originalCaster->GetGUID()))
                        positive = aurApp->IsPositive();

                    duration = m_originalCaster->ModSpellDuration(aurSpellInfo, unit, duration, positive, effectMask);

                    if (duration > 0)
                    {
                        // Haste modifies duration of channeled spells
                        if (m_spellInfo->IsChanneled())
                        {
                            if (m_spellInfo->AttributesEx5 & SPELL_ATTR5_HASTE_AFFECT_DURATION)
                                m_originalCaster->ModSpellCastTime(aurSpellInfo, duration, this);
                        }
                        // Haste modifies duration of spells
                        else if (refresh || m_spellInfo->AttributesEx5 & SPELL_ATTR5_HASTE_AFFECT_DURATION)
                        {
                            int32 origDuration = duration;
                            int32 tempDuration = 0;
                            if (m_spellInfo->AttributesEx5 & SPELL_ATTR5_HASTE_AFFECT_DURATION)
                                duration = 0;
                            for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                                if (AuraEffect const* eff = m_spellAura->GetEffect(i))
                                    if (int32 amplitude = eff->GetAmplitude())  // amplitude is hastened by UNIT_MOD_CAST_SPEED
                                    {
                                        int32 ticks = origDuration / amplitude + (origDuration % amplitude > amplitude / 2 ? 1 : 0);
                                        if (m_spellInfo->AttributesEx5 & SPELL_ATTR5_HASTE_AFFECT_DURATION)
                                            duration = std::max(ticks * amplitude, duration);
                                        if (refresh && ticks > 1)
                                        {
                                            tempDuration = eff->GetPeriodicTimer();
                                            const_cast<AuraEffect *>(eff)->SetPeriodicTimer(tempDuration);
                                        }
                                        if (m_spellInfo->AttributesEx5 & SPELL_ATTR5_START_PERIODIC_AT_APPLY)
                                            const_cast<AuraEffect *>(eff)->SetExtraTick(true);
                                    }

                            // if there is no periodic effect
                            if (!duration && m_spellInfo->AttributesEx5 & SPELL_ATTR5_HASTE_AFFECT_DURATION)
                                duration = int32(origDuration * m_originalCaster->GetFloatValue(UNIT_MOD_CAST_SPEED) + tempDuration);
                            else if (refresh)
                                duration += tempDuration;
                        }
                    }

                    if (duration != m_spellAura->GetMaxDuration())
                    {
                        m_spellAura->SetMaxDuration(duration);
                        m_spellAura->SetDuration(duration);
                        // set duration for DynamicObject from spell (Efflorescence)
                        if (DynamicObject* dynObj = m_originalCaster->GetDynObject(m_spellInfo->Id))
                            dynObj->SetDuration(duration);
                    }
                    if (m_spellValue->Duration)
                        m_spellAura->SetDuration(m_spellValue->Duration);
                    if (m_spellValue->MaxDuration)
                        m_spellAura->SetMaxDuration(m_spellValue->MaxDuration);
                    m_spellAura->_RegisterForTargets();

                    if (m_spellInfo->IsChanneled() && !m_spellInfo->IsPositive())
                        for (int i = 0; i < MAX_SPELL_EFFECTS; ++i)
                            if (m_spellInfo->Effects[i].Effect == SPELL_EFFECT_APPLY_AURA)
                            {
                                SendChannelStart(duration);
                                break;
                            }
                }
            }
        }
    }

    for (uint32 effectNumber = 0; effectNumber < MAX_SPELL_EFFECTS; ++effectNumber)
        if (effectMask & (1 << effectNumber))
            HandleEffects(unit, NULL, NULL, effectNumber, SPELL_EFFECT_HANDLE_HIT_TARGET_FIRST);

    for (uint32 effectNumber = 0; effectNumber < MAX_SPELL_EFFECTS; ++effectNumber)
        if (effectMask & (1 << effectNumber))
            HandleEffects(unit, NULL, NULL, effectNumber, SPELL_EFFECT_HANDLE_HIT_TARGET);
    
    return SPELL_MISS_NONE;
}

void Spell::DoTriggersOnSpellHit(Unit* unit, uint8 effMask)
{
    // Apply additional spell effects to target
    // TODO: move this code to scripts
    if (m_preCastSpell)
    {
        // Paladin immunity shields
        if (m_preCastSpell == 61988)
            // Cast Forbearance
            m_caster->CastSpell(unit, 25771, true);

        if (sSpellMgr->GetSpellInfo(m_preCastSpell))
            // Blizz seems to just apply aura without bothering to cast
            m_caster->AddAura(m_preCastSpell, unit);
    }

    // handle SPELL_AURA_ADD_TARGET_TRIGGER auras
    // this is executed after spell proc spells on target hit
    // spells are triggered for each hit spell target
    // info confirmed with retail sniffs of permafrost and shadow weaving
    if (!m_hitTriggerSpells.empty())
    {
        int _duration = 0;
        for (auto i = m_hitTriggerSpells.begin(); i != m_hitTriggerSpells.end(); ++i)
        {
            if (CanExecuteTriggersOnHit(effMask, i->triggeredByAura) && roll_chance_i(i->chance))
            {
                TC_LOG_DEBUG("spells", "Spell %d triggered spell %d by SPELL_AURA_ADD_TARGET_TRIGGER aura", m_spellInfo->Id, i->triggeredSpell->Id);

                // SPELL_AURA_ADD_TARGET_TRIGGER auras shouldn't trigger auras without duration
                // set duration of current aura to the triggered spell
                if (i->triggeredSpell->GetDuration() == -1)
                {
                    if (Aura* aur = unit->GetAura(m_spellInfo->Id, m_caster->GetGUID()))
                    {
                        m_caster->CastSpell(unit, i->triggeredSpell, true);
                        if (Aura* triggeredAur = unit->GetAura(i->triggeredSpell->Id, m_caster->GetGUID()))
                        {
                            // get duration from aura-only once
                            if (!_duration)
                            {
                                if (i->triggeredSpell->Id == 30069 || i->triggeredSpell->Id == 30070)
                                {
                                    if (Aura* rend = unit->GetAura(94009, m_caster->GetGUID()))
                                        _duration = rend->GetDuration();

                                    if (Aura* deepWounds = unit->GetAura(12721, m_caster->GetGUID()))
                                        if (deepWounds->GetDuration() > _duration)
                                            _duration = deepWounds->GetDuration();
                                }

                                if (!_duration)
                                    _duration = aur->GetDuration();
                            }
                            triggeredAur->SetDuration(_duration);
                        }
                    }
                }
                else
                    m_caster->CastSpell(unit, i->triggeredSpell, true);
            }
        }
    }

    // trigger linked auras remove/apply
    // TODO: remove/cleanup this, as this table is not documented and people are doing stupid things with it
    if (std::vector<int32> const* spellTriggered = sSpellMgr->GetSpellLinked(m_spellInfo->Id + SPELL_LINK_HIT))
    {
        for (auto i = spellTriggered->begin(); i != spellTriggered->end(); ++i)
        {
            if (*i < 0)
                unit->RemoveAurasDueToSpell(-(*i));
            else
                unit->CastSpell(unit, *i, true, 0, 0, m_caster->GetGUID());
        }
    }

    if (m_spellInfo->AttributesEx & SPELL_ATTR1_DISPEL_AURAS_ON_IMMUNITY)
    {
        AuraStateType aState = m_spellInfo->GetAuraState();
        Unit::AuraApplicationMap& Auras = unit->GetAppliedAuras();
        for (auto iter = Auras.begin(); iter != Auras.end();)
        {
            SpellInfo const* spellInfo = iter->second->GetBase()->GetSpellInfo();
            AuraStateType bState = AuraStateType(spellInfo->CasterAuraStateNot);
            if (aState && bState && (bState == aState))
                unit->RemoveAura(iter);
            else
                ++iter;
        }
    }
}

void Spell::DoAllEffectOnTarget(GOTargetInfo* target)
{
    if (target->processed)                                  // Check target
        return;
    target->processed = true;                               // Target checked in apply effects procedure

    uint32 effectMask = target->effectMask;
    if (!effectMask)
        return;

    GameObject* go = m_caster->GetMap()->GetGameObject(target->targetGUID);
    if (!go)
        return;

    PrepareScriptHitHandlers();
    CallScriptBeforeHitHandlers();

    for (uint32 effectNumber = 0; effectNumber < MAX_SPELL_EFFECTS; ++effectNumber)
        if (effectMask & (1 << effectNumber))
            HandleEffects(NULL, NULL, go, effectNumber, SPELL_EFFECT_HANDLE_HIT_TARGET_FIRST);

    for (uint32 effectNumber = 0; effectNumber < MAX_SPELL_EFFECTS; ++effectNumber)
        if (effectMask & (1 << effectNumber))
            HandleEffects(NULL, NULL, go, effectNumber, SPELL_EFFECT_HANDLE_HIT_TARGET);

    CallScriptOnHitHandlers();

    // cast at creature (or GO) quest objectives update at successful cast finished (+channel finished)
    // ignore autorepeat/melee casts for speed (not exist quest for spells (hm...)
    if (m_originalCaster && m_originalCaster->IsControlledByPlayer() && !IsAutoRepeat() && !IsNextMeleeSwingSpell() && !IsChannelActive())
        if (Player* p = m_originalCaster->GetCharmerOrOwnerPlayerOrPlayerItself())
            p->CastedCreatureOrGO(go->GetEntry(), go->GetGUID(), m_spellInfo->Id);

    if (go->AI() && m_originalCaster)
        go->AI()->SpellHit(m_originalCaster, m_spellInfo);

    CallScriptAfterHitHandlers();
}

void Spell::DoAllEffectOnTarget(ItemTargetInfo* target)
{
    uint32 effectMask = target->effectMask;
    if (!target->item || !effectMask)
        return;

    PrepareScriptHitHandlers();
    CallScriptBeforeHitHandlers();

    for (uint32 effectNumber = 0; effectNumber < MAX_SPELL_EFFECTS; ++effectNumber)
        if (effectMask & (1 << effectNumber))
            HandleEffects(NULL, target->item, NULL, effectNumber, SPELL_EFFECT_HANDLE_HIT_TARGET_FIRST);

    for (uint32 effectNumber = 0; effectNumber < MAX_SPELL_EFFECTS; ++effectNumber)
        if (effectMask & (1 << effectNumber))
            HandleEffects(NULL, target->item, NULL, effectNumber, SPELL_EFFECT_HANDLE_HIT_TARGET);

    CallScriptOnHitHandlers();

    CallScriptAfterHitHandlers();
}

bool Spell::UpdateChanneledTargetList()
{
    // Not need check return true
    if (m_channelTargetEffectMask == 0)
        return true;

    uint8 channelTargetEffectMask = m_channelTargetEffectMask;
    uint8 channelAuraMask = 0;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (m_spellInfo->Effects[i].Effect == SPELL_EFFECT_APPLY_AURA)
            channelAuraMask |= 1<<i;

    channelAuraMask &= channelTargetEffectMask;

    float range = 0;
    if (channelAuraMask)
    {
        range = m_spellInfo->GetMaxRange(m_spellInfo->IsPositive());
        if (Player* modOwner = m_caster->GetSpellModOwner())
            modOwner->ApplySpellMod(m_spellInfo->Id, SPELLMOD_RANGE, range, this);

        // Active channeled Spells have a max range tolerance (around 3m). 
        // Verified on WoD Patch 6.2.3
        range += 3.00f;
    }

    for (auto ihit= m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
    {
        if (ihit->missCondition == SPELL_MISS_NONE && (channelTargetEffectMask & ihit->effectMask))
        {
            Unit* unit = m_caster->GetGUID() == ihit->targetGUID ? m_caster : ObjectAccessor::GetUnit(*m_caster, ihit->targetGUID);

            if (!unit)
                continue;

            // Negative channels should cause combat for caster and target.
            if (!m_spellInfo->IsPositive() && m_spellInfo->Id != 2096 && (!m_caster->IsFriendlyTo(unit) || unit->GetCharmer() == m_caster))
                m_caster->CombatStart(unit, true);

            if (IsValidDeadOrAliveTarget(unit))
            {
                if (channelAuraMask & ihit->effectMask)
                {
                    if (AuraApplication * aurApp = unit->GetAuraApplication(m_spellInfo->Id, m_originalCasterGUID))
                    {
                        if (m_caster != unit && !m_caster->IsWithinDistInMap(unit, range))
                        {
                            ihit->effectMask &= ~aurApp->GetEffectMask();
                            unit->RemoveAura(aurApp);
                            continue;
                        }
                    }
                    else // aura is dispelled
                        continue;
                }
                channelTargetEffectMask &= ~ihit->effectMask;   // remove from need alive mask effect that have alive target
            }
        }
    }

    // is all effects from m_needAliveTargetMask have alive targets
    return channelTargetEffectMask == 0;
}

void Spell::prepare(SpellCastTargets const* targets, AuraEffect const* triggeredByAura)
{
    if (m_CastItem)
        m_castItemGUID = m_CastItem->GetGUID();
    else
        m_castItemGUID = 0;

    InitExplicitTargets(*targets);

    // Fill aura scaling information
    if (m_caster->IsControlledByPlayer() && !m_spellInfo->IsPassive() && m_spellInfo->SpellLevel && !m_spellInfo->IsChanneled() && !(_triggeredCastFlags & TRIGGERED_IGNORE_AURA_SCALING))
    {
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            if (m_spellInfo->Effects[i].Effect == SPELL_EFFECT_APPLY_AURA)
            {
                // Change aura with ranks only if basepoints are taken from spellInfo and aura is positive
                if (m_spellInfo->IsPositiveEffect(i))
                {
                    m_auraScaleMask |= (1 << i);
                    if (m_spellValue->EffectBasePoints[i] != m_spellInfo->Effects[i].BasePoints)
                    {
                        m_auraScaleMask = 0;
                        break;
                    }
                }
            }
        }
    }

    m_spellState = SPELL_STATE_PREPARING;
    SetStartCastTime(getMSTime());

    if (triggeredByAura)
    {
        m_triggeredByAuraSpell = triggeredByAura->GetSpellInfo();
        m_changeBySoulburn = triggeredByAura->GetBase()->IsChangeBySoulBurn();
    }

    // create and add update event for this spell
    SpellEvent* Event = new SpellEvent(this);
    m_caster->m_Events.AddEvent(Event, m_caster->m_Events.CalculateTime(1));

    // Prevent casting at cast another spell (ServerSide check)
    // skip Auto Shot
    if (!(_triggeredCastFlags & TRIGGERED_IGNORE_CAST_IN_PROGRESS) && m_caster->IsNonMeleeSpellCasted(false, true, true, m_spellInfo->IsAutoshot()) && m_cast_count)
    {
        SendCastResult(SPELL_FAILED_SPELL_IN_PROGRESS);
        finish(false);
        return;
    }

    if (DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, m_spellInfo->Id, m_caster))
    {
        SendCastResult(SPELL_FAILED_SPELL_UNAVAILABLE);
        finish(false);
        return;
    }
    LoadScripts();

    if (m_caster->GetTypeId() == TYPEID_PLAYER)
        m_caster->ToPlayer()->SetSpellModTakingSpell(this, true);
    // Fill cost data (not use power for item casts
    m_powerCost = m_CastItem ? 0 : m_spellInfo->CalcPowerCost(m_caster, m_spellSchoolMask, this);
    if (m_caster->GetTypeId() == TYPEID_PLAYER)
        m_caster->ToPlayer()->SetSpellModTakingSpell(this, false);

    // Set combo point requirement
    if ((_triggeredCastFlags & TRIGGERED_IGNORE_COMBO_POINTS) || m_CastItem || !m_caster->m_movedPlayer)
        m_needComboPoints = false;

    SpellCastResult result = CheckCast(true);
    if (result != SPELL_CAST_OK && !IsAutoRepeat())          //always cast autorepeat dummy for triggering
    {
        // Periodic auras should be interrupted when aura triggers a spell which can't be cast
        // for example bladestorm aura should be removed on disarm as of patch 3.3.5
        // channeled periodic spells should be affected by this (arcane missiles, penance, etc)
        // a possible alternative sollution for those would be validating aura target on unit state change
        if (triggeredByAura && triggeredByAura->IsPeriodic() && !triggeredByAura->GetBase()->IsPassive())
        {
            SendChannelUpdate(0);
            triggeredByAura->GetBase()->SetDuration(0);
        }
        // Restore spellmods
        if (m_caster->GetTypeId() == TYPEID_PLAYER)
        {
            m_caster->ToPlayer()->RestoreSpellMods(this);
            m_caster->ToPlayer()->SetSpellModTakingSpell(this, false);
        }

        SendCastResult(result);
        finish(false);
        return;
    }

    // Prepare data for triggers
    prepareDataForTriggerSystem(triggeredByAura);

    if (m_caster->GetTypeId() == TYPEID_PLAYER)
        m_caster->ToPlayer()->SetSpellModTakingSpell(this, true);
    // calculate cast time (calculated after first CheckCast check to prevent charge counting for first CheckCast fail)
    m_casttime = m_spellInfo->CalcCastTime(m_caster, this);

    if (m_spellValue->CastTime >= 0)
        m_casttime = uint32(m_spellValue->CastTime);

    if (m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        m_caster->ToPlayer()->SetSpellModTakingSpell(this, false);

        // Set casttime to 0 if .cheat casttime is enabled.
        if (m_caster->ToPlayer()->GetCommandStatus(CHEAT_CASTTIME) || m_caster->ToPlayer()->HasAura(82170))
            m_casttime = 0;
    }

    // don't allow channeled spells / spells with cast time to be casted while moving
    // (even if they are interrupted on moving, spells with almost immediate effect get to have their effect processed before movement interrupter kicks in)
    // don't cancel spells which are affected by a SPELL_AURA_CAST_WHILE_WALKING effect
    if (((m_spellInfo->IsChanneled() || m_casttime) && m_caster->GetTypeId() == TYPEID_PLAYER && m_caster->isMoving() &&
        m_spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_MOVEMENT) && !m_caster->HasAuraTypeWithAffectMask(SPELL_AURA_CAST_WHILE_WALKING, m_spellInfo) &&
        !((_triggeredCastFlags & TRIGGERED_CAST_DIRECTLY) && (!m_spellInfo->IsChanneled() || !m_spellInfo->GetMaxDuration())))
    {
        SendCastResult(SPELL_FAILED_MOVING);
        finish(false);
        return;
    }

    // set timer base at cast time
    ReSetTimer();

    TC_LOG_DEBUG("spells", "Spell::prepare: spell id %u source %u caster %d customCastFlags %u mask %u", m_spellInfo->Id, m_caster->GetEntry(), m_originalCaster ? m_originalCaster->GetEntry() : -1, _triggeredCastFlags, m_targets.GetTargetMask());

    if (GetCaster() && GetSpellInfo())
        if (Player* tmpPlayer = GetCaster()->ToPlayer())
            if (tmpPlayer->HaveSpectators())
            {
                SpectatorAddonMsg msg;
                msg.SetPlayer(tmpPlayer->GetName());
                msg.CastSpell(GetSpellInfo()->Id, GetSpellInfo()->CastTimeEntry->CastTime);
                tmpPlayer->SendSpectatorAddonMsgToBG(msg);
            }
    
    //Containers for channeled spells have to be set
    //TODO:Apply this to all casted spells if needed
    // Why check duration? 29350: channelled triggers channelled
    if ((_triggeredCastFlags & TRIGGERED_CAST_DIRECTLY) && (!m_spellInfo->IsChanneled() || !m_spellInfo->GetMaxDuration()))
        cast(true);
    else
    {
        // stealth must be removed at cast starting (at show channel bar)
        // skip triggered spell (item equip spell casting and other not explicit character casts/item uses)
        if (!(_triggeredCastFlags & TRIGGERED_IGNORE_AURA_INTERRUPT_FLAGS) && m_spellInfo->IsBreakingStealth())
        {
            if (!m_caster->HasAuraType(SPELL_AURA_MOD_CAMOUFLAGE) || m_spellInfo->Id == 21651) // Opening
                m_caster->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_CAST);
            for (uint32 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                if (m_spellInfo->Effects[i].GetUsedTargetObjectType() == TARGET_OBJECT_TYPE_UNIT)
                {
                    m_caster->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_SPELL_ATTACK);
                    break;
                }
        }

        m_caster->SetCurrentCastedSpell(this);
        SendSpellStart();

        // set target for proper facing
        if ((m_casttime || m_spellInfo->IsChanneled()) && !(_triggeredCastFlags & TRIGGERED_IGNORE_SET_FACING))
            if (m_caster->GetGUID() != m_targets.GetObjectTargetGUID() && m_caster->GetTypeId() == TYPEID_UNIT)
                m_caster->FocusTarget(this, m_targets.GetObjectTargetGUID());

        if (m_spellInfo->IsChanneled() && m_spellInfo->AttributesEx & SPELL_ATTR1_CHANNEL_TRACK_TARGET && m_originalCaster && m_originalCaster->GetTypeId() == TYPEID_UNIT)
        {
            if (WorldObject* target = ObjectAccessor::GetUnit(*m_caster, m_originalCaster->GetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT)))
            {
                if (m_originalCaster->GetGUID() != target->GetGUID())
                {
                    m_originalCaster->AddUnitMovementFlag(MOVEMENTFLAG_TRACKING_TARGET);
                    if (Unit *channelTrackTarget = target->ToUnit())
                    {
                        m_originalCaster->StopMoving();
                        m_originalCaster->SetFacingToObject(channelTrackTarget);
                        m_originalCaster->SendMeleeAttackStop();
                        m_originalCaster->ReleaseFocus(this);
                        m_originalCaster->FocusTarget(this, m_originalCaster->GetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT));
                    }
                }
            }
        }

        if (!(_triggeredCastFlags & TRIGGERED_IGNORE_GCD))
            TriggerGlobalCooldown();

        // item: first cast may destroy item and second cast causes crash
        if (!m_casttime && !m_spellInfo->StartRecoveryTime && GetCurrentContainer() == CURRENT_GENERIC_SPELL)
            cast(true);
    }
}

void Spell::cancel(Spell* interruptSpell)
{
    if (m_spellState == SPELL_STATE_FINISHED)
        return;

    uint32 oldState = m_spellState;
    m_spellState = SPELL_STATE_FINISHED;

    m_autoRepeat = false;
    switch (oldState)
    {
        case SPELL_STATE_PREPARING:
            CancelGlobalCooldown();
            if (m_caster->GetTypeId() == TYPEID_PLAYER)
                m_caster->ToPlayer()->RestoreSpellMods(this);
        case SPELL_STATE_DELAYED:
            SendInterrupted(0);
            SendCastResult(SPELL_FAILED_INTERRUPTED);
            break;

        case SPELL_STATE_CASTING:
            for (auto ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
                if ((*ihit).missCondition == SPELL_MISS_NONE)
                    if (Unit* unit = m_caster->GetGUID() == ihit->targetGUID ? m_caster : ObjectAccessor::GetUnit(*m_caster, ihit->targetGUID))
                        unit->RemoveOwnedAura(m_spellInfo->Id, m_originalCasterGUID, 0, AURA_REMOVE_BY_CANCEL);

            SendChannelUpdate(0);
            SendInterrupted(0);
            SendCastResult(SPELL_FAILED_INTERRUPTED);

            // spell is canceled-take mods and clear list
            if (m_caster->GetTypeId() == TYPEID_PLAYER)
                m_caster->ToPlayer()->RemoveSpellMods(this);

            m_appliedMods.clear();
            break;

        default:
            break;
    }

    SetReferencedFromCurrent(false);
    if (m_selfContainer && *m_selfContainer == this)
        *m_selfContainer = NULL;

    m_caster->RemoveDynObject(m_spellInfo->Id);
    if (m_spellInfo->IsChanneled()) // if not channeled then the object for the current cast wasn't summoned yet
        m_caster->RemoveGameObject(m_spellInfo->Id, true);

    //set state back so finish will be processed
    m_spellState = oldState;

    finish(false, interruptSpell);
}

void Spell::cast(bool skipCheck)
{
    // update pointers base at GUIDs to prevent access to non-existed already object
    if (!UpdatePointers())
    {
        // cancel the spell if UpdatePointers() returned false, something wrong happened there
        cancel();
        return;
    }

    // cancel at lost explicit target during cast
    if (m_targets.GetObjectTargetGUID() && !m_targets.GetObjectTarget())
    {
        cancel();
        return;
    }

    if (m_caster->lastSpell)
    {
        // Scatter shots asks makes the client cast auto-shot. Check if our last spell was ment to make us stop attack.
        // Not pretty but it works
        if (m_spellInfo->IsAutoRepeatRangedSpell() && m_caster->lastSpell->Attributes & SPELL_ATTR0_STOP_ATTACK_TARGET)
        {
            m_caster->lastSpell = GetSpellInfo();
            cancel();
            return;
        }
    }

    // save last casted spell
    if (!IsTriggered())
        m_caster->lastSpell = GetSpellInfo();

    if (Player* playerCaster = m_caster->ToPlayer())
    {
        // now that we've done the basic check, now run the scripts
        // should be done before the spell is actually executed
        sScriptMgr->OnPlayerSpellCast(playerCaster, this, skipCheck);

        // As of 3.0.2 pets begin attacking their owner's target immediately
        // Let any pets know we've attacked something. Check DmgClass for harmful spells only
        // This prevents spells such as Hunter's Mark from triggering pet attack
        if (GetSpellInfo()->DmgClass != SPELL_DAMAGE_CLASS_NONE)
        {
            if (Pet* playerPet = playerCaster->GetPet())
                if (playerPet->isAlive() && playerPet->isControlled() && (m_targets.GetTargetMask() & TARGET_FLAG_UNIT))
                    playerPet->AI()->OwnerAttacked(m_targets.GetObjectTarget()->ToUnit());

            if (m_targets.GetTargetMask() & TARGET_FLAG_UNIT)
                for (Unit::ControlList::iterator itr = playerCaster->m_Controlled.begin(); itr != playerCaster->m_Controlled.end(); ++itr)
                    if ((*itr)->IsAIEnabled && (*itr)->isGuardian() && (*itr)->ToCreature()->HasReactState(REACT_ASSIST))
                        (*itr)->ToCreature()->AI()->AttackStart(m_targets.GetObjectTarget()->ToUnit());
        }
    }
    SetExecutedCurrently(true);

    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        if (auto t = m_targets.GetUnitTarget())
            if (t != m_caster)
                m_caster->SetInFront(t);

    // Should this be done for original caster?
    if (m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        // Set spell which will drop charges for triggered cast spells
        // if not successfully casted, will be remove in finish(false)
        m_caster->ToPlayer()->SetSpellModTakingSpell(this, true);
    }

    CallScriptBeforeCastHandlers();

    // skip check if done already (for instant cast spells for example)
    if (!skipCheck)
    {
        SpellCastResult castResult = CheckCast(false);
        if (castResult != SPELL_CAST_OK)
        {
            SendCastResult(castResult);
            SendInterrupted(0);
            //restore spell mods
            if (m_caster->GetTypeId() == TYPEID_PLAYER)
            {
                m_caster->ToPlayer()->RestoreSpellMods(this);
                // cleanup after mod system
                // triggered spell pointer can be not removed in some cases
                m_caster->ToPlayer()->SetSpellModTakingSpell(this, false);
            }
            finish(false);
            SetExecutedCurrently(false);
            return;
        }

        // additional check after cast bar completes (must not be in CheckCast)
        // if trade not complete then remember it in trade data
        if (m_targets.GetTargetMask() & TARGET_FLAG_TRADE_ITEM)
        {
            if (m_caster->GetTypeId() == TYPEID_PLAYER)
            {
                if (TradeData* my_trade = m_caster->ToPlayer()->GetTradeData())
                {
                    if (!my_trade->IsInAcceptProcess())
                    {
                        // Spell will be casted at completing the trade. Silently ignore at this place
                        my_trade->SetSpell(m_spellInfo->Id, m_CastItem);
                        SendCastResult(SPELL_FAILED_DONT_REPORT);
                        SendInterrupted(0);
                        m_caster->ToPlayer()->RestoreSpellMods(this);
                        // cleanup after mod system
                        // triggered spell pointer can be not removed in some cases
                        m_caster->ToPlayer()->SetSpellModTakingSpell(this, false);
                        finish(false);
                        SetExecutedCurrently(false);
                        return;
                    }
                }
            }
        }
    }

    SelectSpellTargets();

    if (!m_targets.HasDst() && m_spellInfo->GetExplicitTargetMask() & TARGET_FLAG_DEST_LOCATION)
    {
        SendCastResult(SPELL_FAILED_BAD_TARGETS);
        SendInterrupted(0);
        finish(false);
        SetExecutedCurrently(false);
        return;
    }

    // Spell may be finished after target map check
    if (m_spellState == SPELL_STATE_FINISHED)
    {
        SendInterrupted(0);
        //restore spell mods
        if (m_caster->GetTypeId() == TYPEID_PLAYER)
        {
            m_caster->ToPlayer()->RestoreSpellMods(this);
            // cleanup after mod system
            // triggered spell pointer can be not removed in some cases
            m_caster->ToPlayer()->SetSpellModTakingSpell(this, false);
        }
        finish(false);
        SetExecutedCurrently(false);
        return;
    }

    PrepareTriggersExecutedOnHit();

    CallScriptOnCastHandlers();

    // traded items have trade slot instead of guid in m_itemTargetGUID
    // set to real guid to be sent later to the client
    m_targets.UpdateTradeSlotItem();

    if (Player* player = m_caster->ToPlayer())
    {
        if (!(_triggeredCastFlags & TRIGGERED_IGNORE_CAST_ITEM) && m_CastItem)
        {
            player->StartTimedAchievement(ACHIEVEMENT_TIMED_TYPE_ITEM, m_CastItem->GetEntry());
            player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_USE_ITEM, m_CastItem->GetEntry());
        }

        player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_CAST_SPELL, m_spellInfo->Id);

        if (m_spellInfo->IsBattleResurrection())
            if (m_spellInfo->HasEffect(SPELL_EFFECT_RESURRECT) || m_spellInfo->HasEffect(SPELL_EFFECT_SELF_RESURRECT))
                if (InstanceScript* instance = player->GetInstanceScript())
                    instance->UseBattleResurrection();
    }

    if (!(_triggeredCastFlags & TRIGGERED_IGNORE_POWER_AND_REAGENT_COST))
    {
        // Powers have to be taken before SendSpellGo
        TakePower();
        TakeReagents();                                         // we must remove reagents before HandleEffects to allow place crafted item in same slot
    }
    else if (Item* targetItem = m_targets.GetItemTarget())
    {
        /// Not own traded item (in trader trade slot) req. reagents including triggered spell case
        if (targetItem->GetOwnerGUID() != m_caster->GetGUID())
            TakeReagents();
    }

    // CAST SPELL
    SendSpellCooldown();

    PrepareScriptHitHandlers();

    HandleLaunchPhase();

    // we must send smsg_spell_go packet before m_castItem delete in TakeCastItem()...
    SendSpellGo();

    if (!IsAutoRepeat() && !IsTriggered() && m_spellInfo->AttributesEx2 & ~SPELL_ATTR2_DISPLAY_IN_STANCE_BAR)
        m_caster->UpdateRowCasts(m_spellInfo->Id);

    Unit::AuraEffectList swaps = m_caster->GetAuraEffectsByType(SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS);
    Unit::AuraEffectList const& swaps2 = m_caster->GetAuraEffectsByType(SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS_2);
    if (!swaps2.empty())
        swaps.insert(swaps.end(), swaps2.begin(), swaps2.end());

    if (!swaps.empty())
        for (auto itr = swaps.begin(); itr != swaps.end(); ++itr)
            if ((*itr)->GetAmount() == m_spellInfo->Id)
                if (!(*itr)->GetSpellInfo()->IsPassive() && (*itr)->GetBase()->GetDuration() > 0)
                {
                    if ((*itr)->GetBase()->IsUsingCharges())
                        (*itr)->GetBase()->DropCharge();
                    else
                        m_caster->RemoveAuraFromStack((*itr)->GetId());
                }

    // Okay, everything is prepared. Now we need to distinguish between immediate and evented delayed spells
    // TODO: Find a better solution for combo point procs
    if (((m_spellValue->Speed || m_delayMoment) && !m_spellInfo->IsChanneled()) || m_spellInfo->Id == 14157 || m_spellInfo->Id == 79128 || m_spellInfo->Id == 109950)
    {
        // Remove used for cast item if need (it can be already NULL after TakeReagents call
        // in case delayed spell remove item at cast delay start
        TakeCastItem();

        // Okay, maps created, now prepare flags
        m_immediateHandled = false;
        m_spellState = SPELL_STATE_DELAYED;
        SetDelayStart(0);

        if (m_caster->HasUnitState(UNIT_STATE_CASTING) && !m_caster->IsNonMeleeSpellCasted(false, false, true))
            m_caster->ClearUnitState(UNIT_STATE_CASTING);
    }
    else
    {
        // Immediate spell, no big deal
        handle_immediate();
    }

    m_caster->ProcDamageAndSpell(m_targets.GetUnitTarget(), PROC_FLAG_ON_CAST, PROC_FLAG_NONE, PROC_EX_NONE, 0, 0, 0, 0, 0, MAX_ATTACK, GetSpellInfo());

    CallScriptAfterCastHandlers();

    if (const std::vector<int32> *spell_triggered = sSpellMgr->GetSpellLinked(m_spellInfo->Id))
    {
        for (auto i = spell_triggered->begin(); i != spell_triggered->end(); ++i)
            if (*i < 0)
                m_caster->RemoveAurasDueToSpell(-(*i));
            else
            {
                if (m_spellInfo->IsAffectingArea())
                {
                    SpellInfo const* triggeredSpell = sSpellMgr->GetSpellInfo(*i);
                    if (triggeredSpell && !triggeredSpell->IsAffectingArea())
                    {
                        for (auto ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
                        {
                            if (Unit* unit = m_caster->GetGUID() == ihit->targetGUID ? m_caster : ObjectAccessor::GetUnit(*m_caster, ihit->targetGUID))
                                m_caster->CastSpell(unit, *i, true);
                        }
                    }
                    else
                        m_caster->CastSpell(m_targets.GetUnitTarget() ? m_targets.GetUnitTarget() : m_caster, *i, true);
                }
                else
                    m_caster->CastSpell(m_targets.GetUnitTarget() ? m_targets.GetUnitTarget() : m_caster, *i, true);
            }
    }

    Unit::AuraEffectList const& stateAuras = m_caster->GetAuraEffectsByType(SPELL_AURA_ABILITY_IGNORE_AURASTATE);
    for (auto j = stateAuras.begin(); j != stateAuras.end();)
    {
        if ((*j)->IsAffectingSpell(m_spellInfo))
        {
            Aura* base = (*j)->GetBase();
            if (base->GetSpellInfo()->StackAmount)
            {
                j++;
                base->ModStackAmount(-1);
                continue;
            }
        }
        j++;
    }


    switch (m_spellInfo->Id)
    {
    case 13812://explosive trap damage
    case 13797://flaming trap damage
    case 34655://snake deadly poison
        if (auto aura = m_caster->GetAura(51755))
            aura->Remove(AURA_REMOVE_BY_DAMAGE);

        if (auto o = m_caster->GetOwner())
            if (auto p = o->ToPlayer())
            if (p->getClass() == CLASS_HUNTER)
                if (auto aura = o->GetAura(51755))
                    aura->Remove(AURA_REMOVE_BY_DAMAGE);


                if (auto petGUID = m_caster->GetPetGUID())
                    if (auto pet = Unit::GetCreature(*m_caster, petGUID))
                        if (auto aura = pet->GetAura(51755))
                            aura->Remove(AURA_REMOVE_BY_DAMAGE);
        break;
    default:
        break;
    }

    // Camouflage handling, the Spellclassmask included in the spell is really messy but this check seems to work
    if (m_caster->HasAuraType(SPELL_AURA_MOD_CAMOUFLAGE))
        if (AuraEffect* camouflage = m_caster->GetAuraEffect(51755, EFFECT_2, m_caster->GetGUID()))
            if (m_spellInfo->SpellFamilyFlags & camouflage->GetSpellInfo()->Effects[EFFECT_2].SpellClassMask)
            {
                if (m_spellInfo->AttributesEx & SPELL_ATTR1_MELEE_COMBAT_START || m_spellInfo->Attributes & SPELL_ATTR0_REQ_AMMO)
                    m_caster->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_CAST);

            }
            else if (!m_spellInfo->IsPositive() && m_spellInfo->SpellIconID != 20/*entrapment shouldn't break camouflage*/)
                m_caster->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_CAST);

    m_caster->ProcDamageAndSpell(unitTarget, m_procAttacker, m_procVictim, m_procEx | PROC_EX_NORMAL_HIT, m_damage, 0, 0, 0, 0, m_attackType, m_spellInfo, m_triggeredByAuraSpell, NULL, false, true);

    if (m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        // Remove spell mods after cast
        if ((m_spellValue->Speed || m_delayMoment) && !m_spellInfo->IsChanneled())
            m_caster->ToPlayer()->RemoveSpellMods(this);

        m_caster->ToPlayer()->SetSpellModTakingSpell(this, false);
        //Clear spell cooldowns after every spell is cast if .cheat cooldown is enabled.
        if (m_caster->ToPlayer()->GetCommandStatus(CHEAT_COOLDOWN))
            m_caster->ToPlayer()->RemoveSpellCooldown(m_spellInfo->Id, true);
    }

    // Item - Warrior T11 DPS 4P Bonus
    if (m_spellInfo->Id == 85288 && m_caster->HasAura(90295))
    {
        m_caster->AddAura(90294, m_caster);
    }

    // Item - Cunning of the Cruel Visual
    if (m_spellInfo->Id == 109800 || m_spellInfo->Id == 108005 || m_spellInfo->Id == 109798)
    {
        m_caster->SendPlaySpellVisualKit(179, 0);       // Emote on cast
    }


    if (Pet* pet = m_caster->ToPet())
    {
        if (m_targets.GetUnitTarget() && pet->GetCharmInfo() && pet->IsAIEnabled)
        {
            // If pet is passive or was sent to cast a spell on a friendly target.
            if ((pet->HasReactState(REACT_PASSIVE) || m_caster->IsFriendlyTo(m_targets.GetUnitTarget())) && pet->GetCharmInfo()->IsMovingForCast())
            {
                CharmInfo* charmInfo = pet->GetCharmInfo();
                pet->AttackStop();
                pet->InterruptNonMeleeSpells(false);
                pet->SendMeleeAttackStop(); // Should stop pet's attack button from flashing
                charmInfo->SetIsAtStay(false);
                charmInfo->SetIsCommandAttack(false);
                charmInfo->SetIsCommandFollow(false);
                charmInfo->SetIsFollowing(false);
                charmInfo->SetIsReturning(false);
            }
            pet->GetCharmInfo()->SetIsMovingForCast(false);
        }
    }

    // self casted spells with SPELL_ATTR0_DISABLED_WHILE_ACTIVE (mostly stealth spells) trigger with SMSG_SPELL_GO client side a infinitely cooldown if the caster is immune to this spell
    // This spells need a "custom" cooldown event to prevent the infinitely cooldown
    // Example: casting stealth during flare is active on the rogue
    if (m_spellInfo->Attributes & SPELL_ATTR0_DISABLED_WHILE_ACTIVE)
        if (Player* player = m_caster->ToPlayer())
            if (player->IsImmunedToSpell(m_spellInfo, player))
                player->SendCooldownEvent(m_spellInfo);

    SetExecutedCurrently(false);

    if (!m_originalCaster)
        return;

    if (Creature* caster = m_originalCaster->ToCreature())
        if (caster->IsAIEnabled)
            caster->AI()->OnSpellCastFinished(GetSpellInfo(), SPELL_FINISHED_SUCCESSFUL_CAST);
}

void Spell::handle_immediate()
{
    bool isApplyingAura = false;
    // start channeling if applicable
    if (m_spellInfo->IsChanneled())
    {
        int32 duration = m_spellInfo->GetDuration();
        if (duration > 0)
        {
            if (!m_spellInfo->IsPositive())
                for (int i = 0; i < MAX_SPELL_EFFECTS; ++i)
                    if (m_spellInfo->Effects[i].Effect == SPELL_EFFECT_APPLY_AURA)
                        isApplyingAura = true;
            // First mod_duration then haste - see Missile Barrage
            // Apply duration mod
            if (Player* modOwner = m_caster->GetSpellModOwner())
                modOwner->ApplySpellMod(m_spellInfo->Id, SPELLMOD_DURATION, duration);
            // Apply haste mods
            if (m_spellInfo->AttributesEx5 & SPELL_ATTR5_HASTE_AFFECT_DURATION)
                m_caster->ModSpellCastTime(m_spellInfo, duration, this);

            m_spellState = SPELL_STATE_CASTING;
            m_caster->AddInterruptMask(m_spellInfo->ChannelInterruptFlags);
            if (!isApplyingAura)
                SendChannelStart(duration);
        }
        else if (duration == -1)
        {
            m_spellState = SPELL_STATE_CASTING;
            m_caster->AddInterruptMask(m_spellInfo->ChannelInterruptFlags);
            SendChannelStart(duration);
        }
    }

    PrepareTargetProcessing();

    // process immediate effects (items, ground, etc.) also initialize some variables
    _handle_immediate_phase();

    for (auto ihit= m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
        DoAllEffectOnTarget(&(*ihit));

    for (auto ihit= m_UniqueGOTargetInfo.begin(); ihit != m_UniqueGOTargetInfo.end(); ++ihit)
        DoAllEffectOnTarget(&(*ihit));

    FinishTargetProcessing();

    // spell is finished, perform some last features of the spell here
    _handle_finish_phase();

    // Remove used for cast item if need (it can be already NULL after TakeReagents call
    TakeCastItem();

    if (m_spellState != SPELL_STATE_CASTING)
        finish(true);                                       // successfully finish spell cast (not last in case autorepeat or channel spell)
}

uint64 Spell::handle_delayed(uint64 t_offset)
{
    if (!UpdatePointers())
    {
        // finish the spell if UpdatePointers() returned false, something wrong happened there
        finish(false);
        return 0;
    }

    if (m_caster->GetTypeId() == TYPEID_PLAYER)
        m_caster->ToPlayer()->SetSpellModTakingSpell(this, true);

    uint64 next_time = 0;

    PrepareTargetProcessing();

    if (!m_immediateHandled)
    {
        _handle_immediate_phase();
        m_immediateHandled = true;
    }

    bool single_missile = (m_targets.HasDst());

    // now recheck units targeting correctness (need before any effects apply to prevent adding immunity at first effect not allow apply second spell effect and similar cases)
    for (auto ihit= m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
    {
        if (ihit->processed == false)
        {
            if (single_missile || ihit->timeDelay <= t_offset)
            {
                ihit->timeDelay = t_offset;
                DoAllEffectOnTarget(&(*ihit));
            }
            else if (next_time == 0 || ihit->timeDelay < next_time)
                next_time = ihit->timeDelay;
        }
    }

    // now recheck gameobject targeting correctness
    for (auto ighit= m_UniqueGOTargetInfo.begin(); ighit != m_UniqueGOTargetInfo.end(); ++ighit)
    {
        if (ighit->processed == false)
        {
            if (single_missile || ighit->timeDelay <= t_offset)
                DoAllEffectOnTarget(&(*ighit));
            else if (next_time == 0 || ighit->timeDelay < next_time)
                next_time = ighit->timeDelay;
        }
    }

    FinishTargetProcessing();

    if (m_caster->GetTypeId() == TYPEID_PLAYER)
        m_caster->ToPlayer()->SetSpellModTakingSpell(this, false);

    // All targets passed - need finish phase
    if (next_time == 0)
    {
        // spell is finished, perform some last features of the spell here
        _handle_finish_phase();

        finish(true);                                       // successfully finish spell cast

        // return zero, spell is finished now
        return 0;
    }
    else
    {
        // spell is unfinished, return next execution time
        return next_time;
    }
}

void Spell::_handle_immediate_phase()
{
    m_spellAura = NULL;
    // initialize Diminishing Returns Data
    m_diminishLevel = DIMINISHING_LEVEL_1;
    m_diminishGroup = DIMINISHING_NONE;

    // handle some immediate features of the spell here
    HandleThreatSpells();

    PrepareScriptHitHandlers();

    // handle effects with SPELL_EFFECT_HANDLE_HIT mode
    for (uint32 j = 0; j < MAX_SPELL_EFFECTS; ++j)
    {
        // don't do anything for empty effect
        if (!m_spellInfo->Effects[j].IsEffect())
            continue;

        // call effect handlers to handle destination hit
        HandleEffects(NULL, NULL, NULL, j, SPELL_EFFECT_HANDLE_HIT);
    }

    // process items
    for (auto ihit= m_UniqueItemInfo.begin(); ihit != m_UniqueItemInfo.end(); ++ihit)
        DoAllEffectOnTarget(&(*ihit));

    if (!m_originalCaster)
        return;
    // Handle procs on cast
    // TODO: finish new proc system:P
    if (m_UniqueTargetInfo.empty() && m_targets.HasDst())
    {
        uint32 procAttacker = m_procAttacker;
        if (!procAttacker)
            procAttacker |= PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_POS;

        // Proc the spells that have DEST target
        m_originalCaster->ProcDamageAndSpell(NULL, procAttacker, 0, 0, 0, 0, m_procEx | PROC_EX_NORMAL_HIT, 0, 0, BASE_ATTACK, m_spellInfo, m_triggeredByAuraSpell);
    }
}

void Spell::_handle_finish_phase()
{
    if (m_caster->m_movedPlayer)
    {
        // Take for real after all targets are processed
        if (m_needComboPoints)
            m_caster->m_movedPlayer->ClearComboPoints();

        // Real add combo points from effects
        if (m_comboPointGain)
            m_caster->m_movedPlayer->GainSpellComboPoints(m_comboPointGain);

        if (m_spellInfo->PowerType == POWER_HOLY_POWER && m_caster->m_movedPlayer->getClass() == CLASS_PALADIN)
           HandleHolyPower(m_caster->m_movedPlayer);
    }

    if (m_caster->m_extraAttacks && GetSpellInfo()->HasEffect(SPELL_EFFECT_ADD_EXTRA_ATTACKS))
        m_caster->HandleProcExtraAttackFor(m_caster->getVictim());

    // TODO: trigger proc phase finish here
}

void Spell::SendSpellCooldown()
{
    Player* _player = m_caster->ToPlayer();

    // mana/health/etc potions, disabled by client (until combat out as declarate)
    if (_player && m_CastItem && m_CastItem->IsPotion())
    {
        // need in some way provided data for Spell::finish SendCooldownEvent
        _player->SetLastPotionId(m_CastItem->GetEntry());
        return;
    }

    // have infinity cooldown but set at aura apply
    if (m_spellInfo->Attributes & (SPELL_ATTR0_DISABLED_WHILE_ACTIVE | SPELL_ATTR0_PASSIVE)
        || ((_triggeredCastFlags & TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD) && !(m_spellInfo->AttributesEx4 & SPELL_ATTR4_TRIGGERED)))
        return;

    m_caster->AddSpellAndCategoryCooldowns(m_spellInfo, m_CastItem ? m_CastItem->GetEntry() : 0, this);
}

void Spell::update(uint32 difftime)
{
    // update pointers based at it's GUIDs
    if (!UpdatePointers())
    {
        // cancel the spell if UpdatePointers() returned false, something wrong happened there
        cancel();
        return;
    }

    if (auto info = GetSpellInfo())
        if (auto caster = GetCaster())
            if (caster->HasSameTickQueueBlock(info->StartRecoveryCategory, true))
                caster->RemoveSameTickQueueBlock(info->StartRecoveryCategory);

    if (m_targets.GetUnitTargetGUID() && !m_targets.GetUnitTarget())
    {
        TC_LOG_DEBUG("spells", "Spell %u is cancelled due to removal of target.", m_spellInfo->Id);
        cancel();
        return;
    }

    // check if the player caster has moved before the spell finished
    // with the exception of spells affected with SPELL_AURA_CAST_WHILE_WALKING effect
    if ((m_caster->GetTypeId() == TYPEID_PLAYER && m_timer != 0) &&
        m_caster->isMoving() && (m_spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_MOVEMENT) &&
        (m_spellInfo->Effects[0].Effect != SPELL_EFFECT_STUCK || !m_caster->HasUnitMovementFlag(MOVEMENTFLAG_FALLING_FAR)) &&
        !m_caster->HasAuraTypeWithAffectMask(SPELL_AURA_CAST_WHILE_WALKING, m_spellInfo))
    {
        // don't cancel for melee, autorepeat, triggered and instant spells
        if (!IsNextMeleeSwingSpell() && !IsAutoRepeat() && !IsTriggered())
            cancel();
    }

    switch (m_spellState)
    {
        case SPELL_STATE_PREPARING:
        {
            if (m_timer > 0)
            {
                if (difftime >= (uint32)m_timer)
                    m_timer = 0;
                else
                    m_timer -= difftime;
            }

            if (m_timer == 0 && !IsNextMeleeSwingSpell() && !IsAutoRepeat())
                // don't CheckCast for instant spells - done in spell::prepare, skip duplicate checks, needed for range checks for example
                cast(!m_casttime);
            break;
        }
        case SPELL_STATE_CASTING:
        {
            if (m_timer)
            {
                // check if there are alive targets left
                if (!UpdateChanneledTargetList())
                {
                    TC_LOG_DEBUG("spells", "Channeled spell %d is removed due to lack of targets", m_spellInfo->Id);
                    SendChannelUpdate(0);
                    finish();
                }

                if (m_timer > 0)
                {
                    if (difftime >= (uint32)m_timer)
                        m_timer = 0;
                    else
                        m_timer -= difftime;
                }
            }

            if (m_timer == 0)
            {
                SendChannelUpdate(0);

                // channeled spell processed independently for quest targeting
                // cast at creature (or GO) quest objectives update at successful cast channel finished
                // ignore autorepeat/melee casts for speed (not exist quest for spells (hm...)
                if (!IsAutoRepeat() && !IsNextMeleeSwingSpell())
                {
                    if (Player* p = m_caster->GetCharmerOrOwnerPlayerOrPlayerItself())
                    {
                        for (auto ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
                        {
                            TargetInfo* target = &*ihit;
                            if (!IS_CRE_OR_VEH_GUID(target->targetGUID))
                                continue;

                            Unit* unit = m_caster->GetGUID() == target->targetGUID ? m_caster : ObjectAccessor::GetUnit(*m_caster, target->targetGUID);
                            if (unit == NULL)
                                continue;

                            p->CastedCreatureOrGO(unit->GetEntry(), unit->GetGUID(), m_spellInfo->Id);
                        }

                        for (auto ihit = m_UniqueGOTargetInfo.begin(); ihit != m_UniqueGOTargetInfo.end(); ++ihit)
                        {
                            GOTargetInfo* target = &*ihit;

                            GameObject* go = m_caster->GetMap()->GetGameObject(target->targetGUID);
                            if (!go)
                                continue;

                            p->CastedCreatureOrGO(go->GetEntry(), go->GetGUID(), m_spellInfo->Id);
                        }
                    }
                }


                if (Creature* creatureCaster = m_caster->ToCreature())
                    if (creatureCaster->IsAIEnabled)
                        creatureCaster->AI()->OnSpellCastFinished(m_spellInfo, SPELL_FINISHED_FINISHED);
                finish();
            }
            break;
        }
        default:
            break;
    }
}

void Spell::finish(bool ok, Spell* interruptSpell)
{
    if (!m_caster)
        return;

    if (m_spellState == SPELL_STATE_FINISHED)
        return;
    m_spellState = SPELL_STATE_FINISHED;

    if (m_spellInfo->IsChanneled())
    {
        if (m_spellInfo->AttributesEx & SPELL_ATTR1_CHANNEL_TRACK_TARGET && m_originalCaster && m_originalCaster->HasUnitMovementFlag(MOVEMENTFLAG_TRACKING_TARGET)
            && (!interruptSpell || !(interruptSpell->GetSpellInfo()->AttributesEx & SPELL_ATTR1_CHANNEL_TRACK_TARGET) || interruptSpell->GetOriginalCaster() != m_originalCaster))
                m_originalCaster->RemoveUnitMovementFlag(MOVEMENTFLAG_TRACKING_TARGET);

        if (!interruptSpell || !(interruptSpell->GetSpellInfo()->AttributesEx & SPELL_ATTR1_CHANNEL_TRACK_TARGET))
            m_caster->SetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT, 0);

        m_caster->UpdateInterruptMask();
    }

    if (!interruptSpell && m_caster->HasUnitState(UNIT_STATE_CASTING) && !m_caster->IsNonMeleeSpellCasted(false, false, true))
        m_caster->ClearUnitState(UNIT_STATE_CASTING);

    // Unsummon summon as possessed creatures on spell cancel
    if (m_spellInfo->IsChanneled() && m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        if (Unit* charm = m_caster->GetCharm())
            if (charm->GetTypeId() == TYPEID_UNIT
                && charm->ToCreature()->HasUnitTypeMask(UNIT_MASK_PUPPET)
                && charm->GetUInt32Value(UNIT_CREATED_BY_SPELL) == m_spellInfo->Id)
                ((Puppet*)charm)->UnSummon();
    }

    // interruptSpell could set the focusTarget, should we check again ? -- ille
    if (m_caster->GetTypeId() == TYPEID_UNIT)
        m_caster->ReleaseFocus(this);

    if (!ok)
        return;

    if (m_caster->GetTypeId() == TYPEID_UNIT && m_caster->ToCreature()->isSummon())
    {
        // Unsummon statue
        uint32 spell = m_caster->GetUInt32Value(UNIT_CREATED_BY_SPELL);
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spell);
        if (spellInfo && spellInfo->SpellIconID == 2056)
        {
            TC_LOG_DEBUG("spells", "Statue %d is unsummoned in spell %d finish", m_caster->GetGUIDLow(), m_spellInfo->Id);
            m_caster->setDeathState(JUST_DIED);
            return;
        }
    }

    if (IsAutoActionResetSpell())
    {
        bool found = false;
        Unit::AuraEffectList const& vIgnoreReset = m_caster->GetAuraEffectsByType(SPELL_AURA_IGNORE_MELEE_RESET);
        for (auto i = vIgnoreReset.begin(); i != vIgnoreReset.end(); ++i)
        {
            if ((*i)->IsAffectingSpell(m_spellInfo))
            {
                found = true;
                break;
            }
        }
        if (!found && !(m_spellInfo->AttributesEx2 & SPELL_ATTR2_NOT_RESET_AUTO_ACTIONS))
        {
            m_caster->resetAttackTimer(BASE_ATTACK);
            if (m_caster->haveOffhandWeapon())
                m_caster->resetAttackTimer(OFF_ATTACK);
            m_caster->resetAttackTimer(RANGED_ATTACK);
        }
    }

    // potions disabled by client, send event "not in combat" if need
    if (m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        if (!m_triggeredByAuraSpell)
            m_caster->ToPlayer()->UpdatePotionCooldown(this);

        // triggered spell pointer can be not set in some cases
        // this is needed for proper apply of triggered spell mods
        m_caster->ToPlayer()->SetSpellModTakingSpell(this, true);

        // Take mods after trigger spell (needed for 14177 to affect 48664)
        // mods are taken only on succesfull cast and independantly from targets of the spell
        m_caster->ToPlayer()->RemoveSpellMods(this);
        m_caster->ToPlayer()->SetSpellModTakingSpell(this, false);
    }
    // Certain spells are triggered by the pet but have to be dropped by owner (Cobra strikes)
    else if (m_caster->isPet() && m_caster->GetOwner())
    {
        if (Player* owner = m_caster->GetOwner()->ToPlayer())
        {
            owner->SetSpellModTakingSpell(this, true);
            owner->RemoveSpellMods(this);
            owner->SetSpellModTakingSpell(this, false);
        }
    }

    // Stop Attack for some spells
    if (m_spellInfo->Attributes & SPELL_ATTR0_STOP_ATTACK_TARGET)
        m_caster->AttackStop();

    //m_caster->ExecuteSortedCastRequests();
}

void Spell::SendCastResult(SpellCastResult result)
{
    if (result == SPELL_CAST_OK)
        return;

    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        return;

    if (m_caster->ToPlayer()->GetSession()->PlayerLoading())  // don't send cast results at loading time
        return;

    SendCastResult(m_caster->ToPlayer(), m_spellInfo, m_cast_count, result, m_customError);
}

void Spell::SendCastResult(Player* caster, SpellInfo const* spellInfo, uint8 cast_count, SpellCastResult result, SpellCustomErrors customError /*= SPELL_CUSTOM_ERROR_NONE*/, Opcodes opcode /*= SMSG_CAST_FAILED*/)
{
    if (result == SPELL_CAST_OK)
        return;

    WorldPacket data(opcode, (4+1+1));
    data << uint8(cast_count);                              // single cast or multi 2.3 (0/1)
    data << uint32(spellInfo->Id);
    data << uint8(result);                                  // problem
    switch (result)
    {
        case SPELL_FAILED_NOT_READY:
            data << uint32(0);                              // unknown (value 1 update cooldowns on client flag)
            break;
        case SPELL_FAILED_REQUIRES_SPELL_FOCUS:
            data << uint32(spellInfo->RequiresSpellFocus);  // SpellFocusObject.dbc id
            break;
        case SPELL_FAILED_REQUIRES_AREA:                    // AreaTable.dbc id
            // hardcode areas limitation case
            switch (spellInfo->Id)
            {
                case 41617:                                 // Cenarion Mana Salve
                case 41619:                                 // Cenarion Healing Salve
                    data << uint32(3905);
                    break;
                case 41618:                                 // Bottled Nethergon Energy
                case 41620:                                 // Bottled Nethergon Vapor
                    data << uint32(3842);
                    break;
                case 45373:                                 // Bloodberry Elixir
                    data << uint32(4075);
                    break;
                default:                                    // default case (don't must be)
                    data << uint32(0);
                    break;
            }
            break;
        case SPELL_FAILED_TOTEMS:
            if (spellInfo->Totem[0])
                data << uint32(spellInfo->Totem[0]);
            if (spellInfo->Totem[1])
                data << uint32(spellInfo->Totem[1]);
            break;
        case SPELL_FAILED_TOTEM_CATEGORY:
            if (spellInfo->TotemCategory[0])
                data << uint32(spellInfo->TotemCategory[0]);
            if (spellInfo->TotemCategory[1])
                data << uint32(spellInfo->TotemCategory[1]);
            break;
        case SPELL_FAILED_EQUIPPED_ITEM_CLASS:
        case SPELL_FAILED_EQUIPPED_ITEM_CLASS_MAINHAND:
        case SPELL_FAILED_EQUIPPED_ITEM_CLASS_OFFHAND:
            data << uint32(spellInfo->EquippedItemClass);
            data << uint32(spellInfo->EquippedItemSubClassMask);
            break;
        case SPELL_FAILED_TOO_MANY_OF_ITEM:
        {
             uint32 item = 0;
             for (int8 eff = 0; eff < MAX_SPELL_EFFECTS; eff++)
                 if (spellInfo->Effects[eff].ItemType)
                     item = spellInfo->Effects[eff].ItemType;
             ItemTemplate const* proto = sObjectMgr->GetItemTemplate(item);
             if (proto && proto->ItemLimitCategory)
                 data << uint32(proto->ItemLimitCategory);
             break;
        }
        case SPELL_FAILED_PREVENTED_BY_MECHANIC:
            data << uint32(spellInfo->GetAllEffectsMechanicMask());  // SpellMechanic.dbc id
            break;
        case SPELL_FAILED_NEED_EXOTIC_AMMO:
            data << uint32(spellInfo->EquippedItemSubClassMask); // seems correct...
            break;
        case SPELL_FAILED_NEED_MORE_ITEMS:
            data << uint32(0);                              // Item id
            data << uint32(0);                              // Item count?
            break;
        case SPELL_FAILED_MIN_SKILL:
            data << uint32(0);                              // SkillLine.dbc id
            data << uint32(0);                              // required skill value
            break;
        case SPELL_FAILED_FISHING_TOO_LOW:
            data << uint32(0);                              // required fishing skill
            break;
        case SPELL_FAILED_CUSTOM_ERROR:
            data << uint32(customError);
            break;
        case SPELL_FAILED_SILENCED:
            data << uint32(0);                              // Unknown
            break;
        case SPELL_FAILED_REAGENTS:
        {
            uint32 missingItem = 0;
            for (uint32 i = 0; i < MAX_SPELL_REAGENTS; i++)
            {
                if (spellInfo->Reagent[i] <= 0)
                    continue;

                uint32 itemid    = spellInfo->Reagent[i];
                uint32 itemcount = spellInfo->ReagentCount[i];

                if (!caster->HasItemCount(itemid, itemcount))
                {
                    missingItem = itemid;
                    break;
                }
            }

            data << uint32(missingItem);  // first missing item
            break;
        }
        // TODO: SPELL_FAILED_NOT_STANDING
        default:
            break;
    }
    caster->GetSession()->SendPacket(&data);
}

void Spell::SendSpellStart()
{
    if (!IsNeedSendToClient())
        return;

    //TC_LOG_DEBUG("spells", "Sending SMSG_SPELL_START id=%u", m_spellInfo->Id);

    uint32 castFlags = CAST_FLAG_HAS_TRAJECTORY;

    if ((m_caster->GetTypeId() == TYPEID_PLAYER ||
        (m_caster->GetTypeId() == TYPEID_UNIT && m_caster->ToCreature()->isPet()))
         && m_spellInfo->PowerType != POWER_HEALTH)
        castFlags |= CAST_FLAG_POWER_LEFT_SELF;


    /*
    uint32 schoolImmunityMask = m_casttime != 0 ? m_caster->GetSchoolImmunityMask() : 0;
    uint32 mechanicImmunityMask = m_casttime != 0 ? m_caster->GetMechanicImmunityMask() : 0;

    if (schoolImmunityMask || mechanicImmunityMask)
        castFlags |= CAST_FLAG_IMMUNITY;
    */

    if (m_spellInfo->RuneCostID && m_spellInfo->PowerType == POWER_RUNES)
        castFlags |= CAST_FLAG_NO_GCD; // not needed, but Blizzard sends it

    if (m_spellInfo->HasAttribute(SPELL_ATTR8_HEALING_SPELL))
        if (m_casttime || m_spellInfo->HasAura(SPELL_AURA_PERIODIC_HEAL))
            castFlags |= CAST_FLAG_HEAL_PREDICTION;

    WorldPacket data(SMSG_SPELL_START, (8+8+4+4+2));
    if (m_CastItem)
        data.append(m_CastItem->GetPackGUID());
    else
        data.append(m_caster->GetPackGUID());

    data.append(m_caster->GetPackGUID());
    data << uint8(m_cast_count);                            // pending spell cast?
    data << uint32(m_spellInfo->Id);                        // spellId
    data << uint32(castFlags);                              // cast flags
    //    data << uint32(m_timer);                                // delay?
    data << uint32(m_caster->ToPlayer() ? m_caster->ToPlayer()->GetSession()->GetLatency() + 100 : 0);                                      // seems to be always 0 in sniffs but ?
    data << uint32(m_casttime);

    m_targets.Write(data);

    if (castFlags & CAST_FLAG_POWER_LEFT_SELF)
        data << uint32(m_caster->GetPower((Powers)m_spellInfo->PowerType));

    if (castFlags & CAST_FLAG_RUNE_LIST)                   // rune cooldowns list
    {
        //TODO: There is a crash caused by a spell with CAST_FLAG_RUNE_LIST casted by a creature
        //The creature is the mover of a player, so HandleCastSpellOpcode uses it as the caster
        if (Player* player = m_caster->ToPlayer())
        {
            data << uint8(m_runesState);                     // runes state before
            data << uint8(player->GetRunesState());          // runes state after
            for (uint8 i = 0; i < MAX_RUNES; ++i)
            {
                // float casts ensure the division is performed on floats as we need float result
                float baseCd = float(player->GetRuneBaseCooldown(i));
                data << uint8((baseCd - float(player->GetRuneCooldown(i))) / baseCd * 255); // rune cooldown passed
            }
        }
        else
        {
            data << uint8(0);
            data << uint8(0);
            for (uint8 i = 0; i < MAX_RUNES; ++i)
                data << uint8(0);
        }
    }

    if (castFlags & CAST_FLAG_PROJECTILE)
    {
        data << uint32(0); // Ammo display ID
        data << uint32(0); // Inventory Type
    }

    if (castFlags & CAST_FLAG_IMMUNITY)
    {
        /*
        * //This is probably where you can make shields appear on casts.
            TC_LOG_ERROR("sql.sql", "immune to interrupts.");
            uint32 schoolImmunityMask = m_caster->GetSchoolImmunityMask();
            uint32 mechanicImmunityMask = m_caster->GetMechanicImmunityMask();

        
                //0   1   1   2
                //1   0   1
        
            data << uint32(2);
            data << uint32(1);
        */

        data << uint32(0);
        data << uint32(0);
    }

    if (castFlags & CAST_FLAG_HEAL_PREDICTION)
    {
        uint32 predicted_heal_value{ 0 };
        uint8 unknown_filler_value{ 1 };
        auto healing_base{ m_healing };

        if (auto target = m_targets.GetUnitTarget())
            if (m_casttime || (m_spellInfo->HasAura(SPELL_AURA_PERIODIC_HEAL)))
            {
                for (auto i = 0; i < MAX_SPELL_EFFECTS; i++)
                    if (m_spellInfo->Effects[i].Effect == SPELL_EFFECT_HEAL)
                        if (auto value = CalculateDamage(i, target))
                            if (value > healing_base)
                                healing_base = value;

                predicted_heal_value += healing_base;

                auto SpellBaseHealingBonusDone = m_caster->SpellBaseHealingBonusDone(m_spellInfo->GetSchoolMask());
                predicted_heal_value += SpellBaseHealingBonusDone;

                /*
                TC_LOG_ERROR("sql.sql", "calculation: %u + %u = %u",
                    healing_base,
                    SpellBaseHealingBonusDone,
                    predicted_heal_value);
                */
            }

        data << uint32(predicted_heal_value);
        data << uint8(unknown_filler_value);
    }

    m_caster->SendMessageToSet(&data, true);
}

void Spell::SendSpellGo()
{
    // not send invisible spell casting
    if (!IsNeedSendToClient())
        return;

    TC_LOG_DEBUG("spells", "Sending SMSG_SPELL_GO id=%u", m_spellInfo->Id);

    uint32 castFlags = CAST_FLAG_UNKNOWN_9;

    if ((m_caster->GetTypeId() == TYPEID_PLAYER ||
        (m_caster->GetTypeId() == TYPEID_UNIT && m_caster->ToCreature()->isPet()))
        && m_spellInfo->PowerType != POWER_HEALTH)
        castFlags |= CAST_FLAG_POWER_LEFT_SELF; // should only be sent to self, but the current messaging doesn't make that possible

    if ((m_caster->GetTypeId() == TYPEID_PLAYER)
        && (m_caster->getClass() == CLASS_DEATH_KNIGHT)
        && m_spellInfo->RuneCostID
        && m_spellInfo->PowerType == POWER_RUNES
        && !(_triggeredCastFlags & TRIGGERED_IGNORE_POWER_AND_REAGENT_COST))
    {
        castFlags |= CAST_FLAG_NO_GCD;                       // same as in SMSG_SPELL_START
        castFlags |= CAST_FLAG_RUNE_LIST;                    // rune cooldowns list
    }

    if (m_spellInfo->HasEffect(SPELL_EFFECT_ACTIVATE_RUNE))
        castFlags |= CAST_FLAG_RUNE_LIST;                    // rune cooldowns list

    if (m_targets.HasTraj())
        castFlags |= CAST_FLAG_ADJUST_MISSILE;

    if (!m_spellInfo->StartRecoveryTime)
        castFlags |= CAST_FLAG_NO_GCD;

    WorldPacket data(SMSG_SPELL_GO, 50);                    // guess size

    if (m_CastItem)
        data.append(m_CastItem->GetPackGUID());
    else
        data.append(m_caster->GetPackGUID());

    data.append(m_caster->GetPackGUID());
    data << uint8(m_cast_count);                            // pending spell cast?
    data << uint32(m_spellInfo->Id);                        // spellId
    data << uint32(castFlags);                              // cast flags
    data << uint32(m_timer);
    /*
        OLD BUG:
        - When chain-queueing multiple spells via gcd, the queued timeframe would not be accounted for client-side.
        This would cause spells to feel extremely laggy because spells were not displaying the proper GCD (affected by spell cast timestamps)
    */
    if (auto p = m_caster->ToPlayer())
        if (auto s = p->GetSession())
        {
            uint32 keypress_time{ (m_requestedTime > 100) ? (m_requestedTime) : m_StartCast_time  };

            data << uint32(m_requestedTime ? (keypress_time) : m_StartCast_time);                            // timestamp
        }
        else data << m_StartCast_time;
    else data << m_StartCast_time;

    WriteSpellGoTargets(&data);

    m_targets.Write(data);

    if (castFlags & CAST_FLAG_POWER_LEFT_SELF)
        data << uint32(m_caster->GetPower((Powers)m_spellInfo->PowerType));

    if (castFlags & CAST_FLAG_RUNE_LIST)                   // rune cooldowns list
    {
        //TODO: There is a crash caused by a spell with CAST_FLAG_RUNE_LIST casted by a creature
        //The creature is the mover of a player, so HandleCastSpellOpcode uses it as the caster
        if (Player* player = m_caster->ToPlayer())
        {
            data << uint8(m_runesState);                     // runes state before
            data << uint8(player->GetRunesState());          // runes state after
            for (uint8 i = 0; i < MAX_RUNES; ++i)
            {
                // float casts ensure the division is performed on floats as we need float result
                float baseCd = float(player->GetRuneBaseCooldown(i));
                data << uint8((baseCd - float(player->GetRuneCooldown(i))) / baseCd * 255); // rune cooldown passed
            }
        }
    }

    if (castFlags & CAST_FLAG_ADJUST_MISSILE)
    {
        data << m_targets.GetElevation();
        data << uint32(m_delayMoment);
    }

    if (castFlags & CAST_FLAG_PROJECTILE)
    {
        data << uint32(0); // Ammo display ID
        data << uint32(0); // Inventory Type
    }

    if (castFlags & CAST_FLAG_VISUAL_CHAIN)
    {
        data << uint32(0);
        data << uint32(0);
    }

    if (m_targets.GetTargetMask() & TARGET_FLAG_DEST_LOCATION)
    {
        data << uint8(0);
    }

    if (m_targets.GetTargetMask() & TARGET_FLAG_EXTRA_TARGETS)
    {
        data << uint32(0); // Extra targets count
        /*
        for (uint8 i = 0; i < count; ++i)
        {
            data << float(0);   // Target Position X
            data << float(0);   // Target Position Y
            data << float(0);   // Target Position Z
            data << uint64(0);  // Target Guid
        }
        */
    }

    m_caster->SendMessageToSet(&data, true);
}

/// Writes miss and hit targets for a SMSG_SPELL_GO packet
void Spell::WriteSpellGoTargets(WorldPacket* data)
{
    // This function also fill data for channeled spells:
    // m_needAliveTargetMask req for stop channelig if one target die
    for (auto ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
    {
        if ((*ihit).effectMask == 0)                  // No effect apply - all immuned add state
            // possibly SPELL_MISS_IMMUNE2 for this??
            ihit->missCondition = SPELL_MISS_IMMUNE2;
    }

    // Hit and miss target counts are both uint8, that limits us to 255 targets for each
    // sending more than 255 targets crashes the client (since count sent would be wrong)
    // Spells like 40647 (with a huge radius) can easily reach this limit (spell might need
    // target conditions but we still need to limit the number of targets sent and keeping
    // correct count for both hit and miss).

    uint32 hit = 0;
    size_t hitPos = data->wpos();
    *data << (uint8)0; // placeholder
    for (auto ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end() && hit < 255; ++ihit)
    {
        if ((*ihit).missCondition == SPELL_MISS_NONE)       // Add only hits
        {
            *data << uint64(ihit->targetGUID);
            m_channelTargetEffectMask |=ihit->effectMask;
            ++hit;
        }
    }

    for (auto ighit = m_UniqueGOTargetInfo.begin(); ighit != m_UniqueGOTargetInfo.end() && hit < 255; ++ighit)
    {
        *data << uint64(ighit->targetGUID);                 // Always hits
        ++hit;
    }

    uint32 miss = 0;
    size_t missPos = data->wpos();
    *data << (uint8)0; // placeholder
    for (auto ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end() && miss < 255; ++ihit)
    {
        if (ihit->missCondition != SPELL_MISS_NONE)        // Add only miss
        {
            *data << uint64(ihit->targetGUID);
            *data << uint8(ihit->missCondition);
            if (ihit->missCondition == SPELL_MISS_REFLECT)
                *data << uint8(ihit->reflectResult);
            ++miss;
        }
    }
    // Reset m_needAliveTargetMask for non channeled spell
    if (!m_spellInfo->IsChanneled())
        m_channelTargetEffectMask = 0;

    data->put<uint8>(hitPos, (uint8)hit);
    data->put<uint8>(missPos, (uint8)miss);
}

void Spell::SendLogExecute()
{
    WorldPacket data(SMSG_SPELLLOGEXECUTE, (8+4+4+4+4+8));

    data.append(m_caster->GetPackGUID());

    data << uint32(m_spellInfo->Id);

    uint8 effCount = 0;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (m_effectExecuteData[i])
            ++effCount;
    }

    if (!effCount)
        return;

    data << uint32(effCount);
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (!m_effectExecuteData[i])
            continue;

        data << uint32(m_spellInfo->Effects[i].Effect);             // spell effect

        data.append(*m_effectExecuteData[i]);

        delete m_effectExecuteData[i];
        m_effectExecuteData[i] = NULL;
    }
    m_caster->SendMessageToSet(&data, true);
}

void Spell::ExecuteLogEffectTakeTargetPower(uint8 effIndex, Unit* target, uint32 powerType, uint32 powerTaken, float gainMultiplier)
{
    InitEffectExecuteData(effIndex);
    m_effectExecuteData[effIndex]->append(target->GetPackGUID());
    *m_effectExecuteData[effIndex] << uint32(powerTaken);
    *m_effectExecuteData[effIndex] << uint32(powerType);
    *m_effectExecuteData[effIndex] << float(gainMultiplier);
}

void Spell::ExecuteLogEffectExtraAttacks(uint8 effIndex, Unit* victim, uint32 attCount)
{
    InitEffectExecuteData(effIndex);
    m_effectExecuteData[effIndex]->append(victim->GetPackGUID());
    *m_effectExecuteData[effIndex] << uint32(attCount);
}

void Spell::ExecuteLogEffectInterruptCast(uint8 /*effIndex*/, Unit* victim, Spell* inputspell)
{
    ObjectGuid casterGuid = m_caster->GetGUID();
    ObjectGuid targetGuid = victim->GetGUID();

    WorldPacket data(SMSG_SPELLINTERRUPTLOG, 8 + 8 + 4 + 4);
    data.WriteBit(targetGuid[4]);
    data.WriteBit(casterGuid[5]);
    data.WriteBit(casterGuid[6]);
    data.WriteBit(casterGuid[1]);
    data.WriteBit(casterGuid[3]);
    data.WriteBit(casterGuid[0]);
    data.WriteBit(targetGuid[3]);
    data.WriteBit(targetGuid[5]);
    data.WriteBit(targetGuid[1]);
    data.WriteBit(casterGuid[4]);
    data.WriteBit(casterGuid[7]);
    data.WriteBit(targetGuid[7]);
    data.WriteBit(targetGuid[6]);
    data.WriteBit(targetGuid[2]);
    data.WriteBit(casterGuid[2]);
    data.WriteBit(targetGuid[0]);

    data.WriteByteSeq(casterGuid[7]);
    data.WriteByteSeq(casterGuid[6]);
    data.WriteByteSeq(casterGuid[3]);
    data.WriteByteSeq(casterGuid[2]);
    data.WriteByteSeq(targetGuid[3]);
    data.WriteByteSeq(targetGuid[6]);
    data.WriteByteSeq(targetGuid[2]);
    data.WriteByteSeq(targetGuid[4]);
    data.WriteByteSeq(targetGuid[7]);
    data.WriteByteSeq(targetGuid[0]);
    data.WriteByteSeq(casterGuid[4]);
    data << uint32(m_spellInfo->Id);
    data.WriteByteSeq(targetGuid[1]);
    data.WriteByteSeq(casterGuid[0]);
    data.WriteByteSeq(casterGuid[5]);
    data.WriteByteSeq(casterGuid[1]);
    data << uint32(inputspell->GetSpellInfo()->Id);
    data.WriteByteSeq(targetGuid[5]);

    m_caster->SendMessageToSet(&data, true);
    Execute_PvPAction_kick_DatabaseLog(m_caster, inputspell, victim);
}

void Spell::Execute_PvPAction_kick_DatabaseLog(Unit* Interrupter, Spell* inputspell, Unit* victim)
{
    if (!Interrupter)
        return;
    if (!inputspell)
        return;
    if (!victim)
        return;
    if (Player* InterruptPlayer = (Interrupter->ToPlayer() ? Interrupter->ToPlayer() : (Interrupter->GetOwner() ? Interrupter->GetOwner()->ToPlayer() : NULL)))
        if (Battleground* thisBG = InterruptPlayer->GetBattleground())       
            if (thisBG->isArena())          
                if (uint32 interrupterTeamID = InterruptPlayer->GetArenaTeamId(thisBG->GetArenaSlot()))
                {
                    //Establish who a viable kill target could be.
                    Unit* KillTarget = NULL;
                    std::list<Unit*> targets;
                    Trinity::AnyFriendlyUnitInObjectRangeCheck u_check2(victim, victim, 100.0f);
                    Trinity::UnitListSearcher<Trinity::AnyFriendlyUnitInObjectRangeCheck> searcher2(victim, targets, u_check2);
                    victim->VisitNearbyObject(100.0f, searcher2);
                    if (targets.size())
                    {
                        for (auto itr = targets.begin(); itr != targets.end();)
                        {
                            Unit* temp = (*itr);
                            itr++;
                            if (temp != NULL)
                                if ((temp)->ToPlayer())
                                {
                                    if (!KillTarget)
                                        KillTarget = temp;

                                    if (KillTarget->GetGUID() != temp->GetGUID())
                                        if (temp->GetHealth() < KillTarget->GetHealth())
                                            KillTarget = temp;
                                }
                        }
                    }

                    //Establish the percent completion of this cast that got interrupted
                    uint32 castPCT;
                    if (inputspell->GetSpellInfo()->IsChanneled())
                    {
                        castPCT = (100 - (100 * int32(inputspell->GetSpellInfo()->GetDuration() - GetMSTimeDiffToNow(((inputspell->GetStartCastTime()))) - inputspell->GetDelaytime()) / int32(inputspell->GetSpellInfo()->GetDuration())));
                    }
                    else
                    {
                        //TC_LOG_ERROR("spells", "cast is normal, times: %u / %u", (GetMSTimeDiffToNow(inputspell->GetStartCastTime()) - inputspell->GetDelaytime()), int32(inputspell->GetCastTime()));
                        if (int32(GetMSTimeDiffToNow(inputspell->GetStartCastTime())) > int32(inputspell->GetDelaytime()))//This spell's total casting time is higher than the time it's been reduced by pushback.
                        {
                            //TC_LOG_ERROR("spells", "cast should work with normal calculation, times: %u - %u / %u", GetMSTimeDiffToNow(inputspell->GetStartCastTime()), inputspell->GetDelaytime(), int32(inputspell->GetCastTime()));
                            castPCT =
                                (
                                    100 * (GetMSTimeDiffToNow(inputspell->GetStartCastTime()) - inputspell->GetDelaytime())   //calculate normally
                                    /
                                    int32(inputspell->GetCastTime())
                                    );
                        }
                        else
                        {
                            TC_LOG_ERROR("spells", "PVP_DETECTION_DATA_LOG: This formula would have produced a negative numerator: (%u - %u) / %u\n spell %u interrupted by %u", int32(GetMSTimeDiffToNow(inputspell->GetStartCastTime())), inputspell->GetDelaytime(), int32(inputspell->GetCastTime()), (inputspell->GetSpellInfo()->Id), GetSpellInfo()->Id);
                            castPCT = int32(0);
                        }
                    }

                    PreparedStatement* stmt = nullptr;
                    stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_PVP_DETECTION_DATA);
                    stmt->setUInt64(0, InterruptPlayer->GetGUID());                                     //playerGUID
                    stmt->setUInt32(1, PVP_DETECTION_EVENT_KICK_ATTEMPT);                               //eventType
                                                                                                        //DateAndTime filled already
                    stmt->setUInt32(2, uint64(thisBG->GetGUID()));                                      //data1 = match GUID, NEEDS ACTUAL DB GUID
                    //TC_LOG_ERROR("spells", "match GUID:", uint64(thisBG->GetGUID()));
                    stmt->setUInt32(3, thisBG->GetArenaType());                                         //data2 = bracket size
                    stmt->setUInt32(4, interrupterTeamID);                                              //data3 = team of player who just interrupted a spell
                    if (victim->ToPlayer())
                    {
                        stmt->setUInt32(5, victim->ToPlayer()->GetArenaTeamId(thisBG->GetArenaSlot()));     //data4 = team of player who just got interrupted
                        stmt->setUInt32(6, victim->GetGUID());                                              //data5 = unit who just got interrupted, only set to a value if a player was interrupted.
                    }
                    else
                    {
                        stmt->setUInt32(5, victim->GetOwner()->ToPlayer()->GetArenaTeamId(thisBG->GetArenaSlot()));     //data4 = team of player who just got interrupted
                        stmt->setUInt32(6, victim->GetOwner()->GetGUID());                                              //data5 = unit who just got interrupted, only set to a value if a player was interrupted.
                    }
                    stmt->setUInt32(7, (inputspell->GetSpellInfo()->Id));                               //data6 = spell from victim which got interrupted
                    stmt->setUInt32(8, GetSpellInfo()->Id);                                             //data7 = spell used to interrupt victim's spell
                    stmt->setUInt32(9, 1);                                                              //data8 = true or false as to whether or not this interrupt succeeded. this function is for a successful interrupt so it is always 1.
                    stmt->setUInt32(10, GetMSTimeDiffToNow(inputspell->GetStartCastTime()));            //data9 = reaction time between server-side spell start and current time. NOTE: both of these time values reference the time of server startup as 0 and counts up in milliseconds.
                    stmt->setUInt32(11, castPCT);                                                       //data10 = cast completion percentage, something set by kickbots under certain conditions.
                    stmt->setUInt32(12, InterruptPlayer->GetSession()->GetLatency());                   //data11 = Interrupting Player's latency at this given moment
                    stmt->setUInt64(13, (KillTarget != NULL ? KillTarget->GetGUID() : 0));              //data12 = GUID of potential kill target player friendly to the interrupt victim
                    stmt->setUInt32(14, (KillTarget != NULL ? KillTarget->GetHealthPct() : 0));         //data13 = PCT health of potential kill target player friendly to the interrupt victim
                    stmt->setUInt32(15, (KillTarget != NULL ? KillTarget->GetHealth() : 0));            //data14 = Actual health of potential kill target player friendly to the interrupt victim
                    CharacterDatabase.Execute(stmt);                                                    //execution of query
                }                
}

void Spell::ExecuteLogEffectDurabilityDamage(uint8 effIndex, Unit* victim, int32 itemId, int32 slot)
{
    InitEffectExecuteData(effIndex);
    m_effectExecuteData[effIndex]->append(victim->GetPackGUID());
    *m_effectExecuteData[effIndex] << int32(itemId);
    *m_effectExecuteData[effIndex] << int32(slot);
}

void Spell::ExecuteLogEffectOpenLock(uint8 effIndex, Object* obj)
{
    InitEffectExecuteData(effIndex);
    m_effectExecuteData[effIndex]->append(obj->GetPackGUID());
}

void Spell::ExecuteLogEffectCreateItem(uint8 effIndex, uint32 entry)
{
    InitEffectExecuteData(effIndex);
    *m_effectExecuteData[effIndex] << uint32(entry);
}

void Spell::ExecuteLogEffectDestroyItem(uint8 effIndex, uint32 entry)
{
    InitEffectExecuteData(effIndex);
    *m_effectExecuteData[effIndex] << uint32(entry);
}

void Spell::ExecuteLogEffectSummonObject(uint8 effIndex, WorldObject* obj)
{
    InitEffectExecuteData(effIndex);
    m_effectExecuteData[effIndex]->append(obj->GetPackGUID());
}

void Spell::ExecuteLogEffectUnsummonObject(uint8 effIndex, WorldObject* obj)
{
    InitEffectExecuteData(effIndex);
    m_effectExecuteData[effIndex]->append(obj->GetPackGUID());
}

void Spell::ExecuteLogEffectResurrect(uint8 effIndex, Unit* target)
{
    InitEffectExecuteData(effIndex);
    m_effectExecuteData[effIndex]->append(target->GetPackGUID());
}

void Spell::SendInterrupted(uint8 result)
{
    WorldPacket data(SMSG_SPELL_FAILURE, (8+4+1));
    data.append(m_caster->GetPackGUID());
    data << uint8(m_cast_count);
    data << uint32(m_spellInfo->Id);
    data << uint8(result);
    m_caster->SendMessageToSet(&data, true);

    data.Initialize(SMSG_SPELL_FAILED_OTHER, (8+4));
    data.append(m_caster->GetPackGUID());
    data << uint8(m_cast_count);
    data << uint32(m_spellInfo->Id);
    data << uint8(result);
    m_caster->SendMessageToSet(&data, true);
}

void Spell::SendChannelUpdate(uint32 time)
{
    if (time == 0)
    {
        // don't re-init the UNIT_FIELD_CHANNEL_OBJECT here, somes triggered spell needs a TARGET_UNIT_CHANNEL_TARGET or precedent spell
        //        m_caster->SetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT, 0);
        m_caster->SetUInt32Value(UNIT_CHANNEL_SPELL, 0);
    }

    WorldPacket data(MSG_CHANNEL_UPDATE, 8+4);
    data.append(m_caster->GetPackGUID());
    data << uint32(time);

    m_caster->SendMessageToSet(&data, true);
}

void Spell::SendChannelStart(uint32 duration)
{
    uint64 channelTarget = m_targets.GetObjectTargetGUID();
    if (!channelTarget && !m_spellInfo->NeedsExplicitUnitTarget())
        if (m_UniqueTargetInfo.size() + m_UniqueGOTargetInfo.size() == 1)   // this is for TARGET_SELECT_CATEGORY_NEARBY
            channelTarget = !m_UniqueTargetInfo.empty() ? m_UniqueTargetInfo.front().targetGUID : m_UniqueGOTargetInfo.front().targetGUID;

    WorldPacket data(MSG_CHANNEL_START, (8+4+4));
    data.append(m_caster->GetPackGUID());
    data << uint32(m_spellInfo->Id);
    data << uint32(duration);
    data << uint8(0);                           // immunity (castflag & 0x04000000)
    /*
    if (immunity)
    {
        data << uint32();                       // CastSchoolImmunities
        data << uint32();                       // CastImmunities
    }
    */
    data << uint8(0);                           // healPrediction (castflag & 0x40000000)
    /*
    if (healPrediction)
    {
        data.appendPackGUID(channelTarget);     // target packguid
        data << uint32();                       // spellid
        data << uint8(0);                       // unk3
        if (unk3 == 2)
            data.append();                      // unk packed guid (unused ?)
    }
    */
    m_caster->SendMessageToSet(&data, true);

    m_timer = duration;
    // TYPEID_PLAYER because SPELL_ATTR1_CHANNEL_TRACK_TARGET is handle client side for players for unit the UNIT_FIELD_CHANNEL_OBJECT is set before the cast
    if (channelTarget && (m_originalCaster->GetTypeId() == TYPEID_PLAYER || !(m_spellInfo->AttributesEx & SPELL_ATTR1_CHANNEL_TRACK_TARGET)))
        m_caster->SetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT, channelTarget);

    m_caster->SetUInt32Value(UNIT_CHANNEL_SPELL, m_spellInfo->Id);
}

void Spell::SendResurrectRequest(Player* target)
{
    // get ressurector name for creature resurrections, otherwise packet will be not accepted
    // for player resurrections the name is looked up by guid
    std::string const sentName(m_caster->GetTypeId() == TYPEID_PLAYER
                               ? ""
                               : m_caster->GetNameForLocaleIdx(target->GetSession()->GetSessionDbLocaleIndex()));

    WorldPacket data(SMSG_RESURRECT_REQUEST, (8+4+sentName.size()+1+1+1+4));
    data << uint64(m_caster->GetGUID());
    data << uint32(sentName.size() + 1);

    data << sentName;
    data << uint8(0); // null terminator

    data << uint8(m_caster->GetTypeId() == TYPEID_PLAYER ? 0 : 1); // "you'll be afflicted with resurrection sickness"
    // override delay sent with SMSG_CORPSE_RECLAIM_DELAY, set instant resurrection for spells with this attribute
    // 4.2.2 edit : id of the spell used to resurect. (used client-side for Mass Resurect)
    data << uint32(m_spellInfo->Id);
    target->GetSession()->SendPacket(&data);
}

void Spell::TakeCastItem()
{
    if (!m_CastItem || m_caster->GetTypeId() != TYPEID_PLAYER)
        return;

    // not remove cast item at triggered spell (equipping, weapon damage, etc)
    if (_triggeredCastFlags & TRIGGERED_IGNORE_CAST_ITEM)
        return;

    ItemTemplate const* proto = m_CastItem->GetTemplate();

    if (!proto)
    {
        // This code is to avoid a crash
        // I'm not sure, if this is really an error, but I guess every item needs a prototype
        TC_LOG_ERROR("spells", "Cast item has no item prototype highId=%d, lowId=%d", m_CastItem->GetGUIDHigh(), m_CastItem->GetGUIDLow());
        return;
    }

    bool expendable = false;
    bool withoutCharges = false;

    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        if (proto->Spells[i].SpellId)
        {
            // item has limited charges
            if (proto->Spells[i].SpellCharges)
            {
                if (proto->Spells[i].SpellCharges < 0)
                    expendable = true;

                int32 charges = m_CastItem->GetSpellCharges(i);

                // item has charges left
                if (charges)
                {
                    (charges > 0) ? --charges : ++charges;  // abs(charges) less at 1 after use
                    if (proto->Stackable == 1)
                        m_CastItem->SetSpellCharges(i, charges);
                    m_CastItem->SetState(ITEM_CHANGED, (Player*)m_caster);
                }

                // all charges used
                withoutCharges = (charges == 0);
            }
        }
    }

    if (expendable && withoutCharges)
    {
        uint32 count = 1;
        m_caster->ToPlayer()->DestroyItemCount(m_CastItem, count, true);

        // prevent crash at access to deleted m_targets.GetItemTarget
        if (m_CastItem == m_targets.GetItemTarget())
            m_targets.SetItemTarget(NULL);

        m_CastItem = NULL;
        m_castItemGUID = 0;
    }
}

void Spell::TakePower()
{
    if (m_CastItem || m_triggeredByAuraSpell)
        return;

    //Don't take power if the spell is cast while .cheat power is enabled.
    if (m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        if (m_caster->ToPlayer()->GetCommandStatus(CHEAT_POWER))
            return;
    }

    Powers powerType = Powers(m_spellInfo->PowerType);
    // Holy power is already taken in HandleHolyPower
    if (powerType == POWER_HOLY_POWER)
        return;

    bool hit = true;
    if (m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        if (powerType == POWER_RAGE || powerType == POWER_ENERGY || powerType == POWER_RUNES)
            if (uint64 targetGUID = m_targets.GetUnitTargetGUID())
                for (auto ihit= m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
                    if (ihit->targetGUID == targetGUID)
                    {
                        if (ihit->missCondition != SPELL_MISS_NONE)
                        {
                            hit = false;
                            //lower spell cost on fail (by talent aura)
                            if (Player* modOwner = m_caster->ToPlayer()->GetSpellModOwner())
                                modOwner->ApplySpellMod(m_spellInfo->Id, SPELLMOD_SPELL_COST_REFUND_ON_FAIL, m_powerCost);
                        }
                        break;
                    }
    }

    if (powerType == POWER_RUNES)
    {
        if (m_caster->GetTypeId() == TYPEID_PLAYER && m_spellInfo->Id != 49998)
            if (uint64 targetGUID = m_targets.GetUnitTargetGUID())
                for (auto ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
                    if (ihit->targetGUID == targetGUID)
                        if (ihit->missCondition == SPELL_MISS_DODGE || ihit->missCondition == SPELL_MISS_PARRY || ihit->missCondition == SPELL_MISS_MISS)
                            return;

        TakeRunePower(hit);
        return;
    }

    if (!m_powerCost)
        return;

    // health as power used
    if (powerType == POWER_HEALTH)
    {
        m_caster->ModifyHealth(-(int32)m_powerCost);
        return;
    }

    if (powerType >= MAX_POWERS)
    {
        TC_LOG_ERROR("spells", "Spell::TakePower: Unknown power type '%d'", powerType);
        return;
    }

    // savage roar clearcasting
    if (m_spellInfo->Id == 52610)
        if (m_caster->HasAura(16870))
        {
            m_powerCost = 0;
            m_caster->RemoveAura(16870);
        }

    // Nourish
    if (m_spellInfo->Id == 50464 && powerType == POWER_MANA)
        if (m_caster->HasAura(16870))
        {
            m_powerCost = 0;
            m_caster->RemoveAura(16870);
        }

    if (m_caster->isHunterPet() && m_caster->GetPower(POWER_FOCUS) >= 50)
    {
        // Wild Hunt
        if (m_caster->HasSpell(62758))
        {
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(62758);
            AddPct(m_powerCost, spellInfo->Effects[EFFECT_1].BasePoints);
        }
        else if (m_caster->HasSpell(62762))
        {
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(62762);
            AddPct(m_powerCost, spellInfo->Effects[EFFECT_1].BasePoints);
        }
    }

    if (hit)
        m_caster->ModifyPower(powerType, -m_powerCost);
    else
        m_caster->ModifyPower(powerType, -irand(0, m_powerCost/4));
}

SpellCastResult Spell::CheckRuneCost(uint32 runeCostID)
{
    if (m_spellInfo->PowerType != POWER_RUNES || !runeCostID)
        return SPELL_CAST_OK;

    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        return SPELL_CAST_OK;

    Player* player = (Player*)m_caster;

    if (player->getClass() != CLASS_DEATH_KNIGHT)
        return SPELL_CAST_OK;

    SpellRuneCostEntry const* src = sSpellRuneCostStore.LookupEntry(runeCostID);

    if (!src)
        return SPELL_CAST_OK;

    if (src->NoRuneCost())
        return SPELL_CAST_OK;

    int32 runeCost[NUM_RUNE_TYPES];                         // blood, frost, unholy, death

    for (uint32 i = 0; i < RUNE_DEATH; ++i)
    {
        runeCost[i] = src->RuneCost[i];
        if (Player* modOwner = m_caster->GetSpellModOwner())
            modOwner->ApplySpellMod(m_spellInfo->Id, SPELLMOD_COST, runeCost[i], this);
    }

    runeCost[RUNE_DEATH] = MAX_RUNES;                       // calculated later

    for (uint32 i = 0; i < MAX_RUNES; ++i)
    {
        RuneType rune = player->GetCurrentRune(i);
        if ((player->GetRuneCooldown(i) == 0) && (runeCost[rune] > 0))
            runeCost[rune]--;
    }

    for (uint32 i = 0; i < RUNE_DEATH; ++i)
        if (runeCost[i] > 0)
            runeCost[RUNE_DEATH] += runeCost[i];

    if (runeCost[RUNE_DEATH] > MAX_RUNES)
        return SPELL_FAILED_NO_POWER;                       // not sure if result code is correct

    return SPELL_CAST_OK;
}

void Spell::TakeRunePower(bool didHit)
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER || m_caster->getClass() != CLASS_DEATH_KNIGHT)
        return;

    SpellRuneCostEntry const* runeCostData = sSpellRuneCostStore.LookupEntry(m_spellInfo->RuneCostID);
    if (!runeCostData || (runeCostData->NoRuneCost() && runeCostData->NoRunicPowerGain()))
        return;

    Player* player = m_caster->ToPlayer();
    m_runesState = player->GetRunesState();                 // store previous state

    int32 runeCost[NUM_RUNE_TYPES];                         // blood, frost, unholy, death
    player->ClearLastUsedRuneMask();

    // Always use base cooldown for death strike
    if (m_spellInfo->Id == 49998)
        didHit = true;

    for (uint32 i = 0; i < RUNE_DEATH; ++i)
    {
        runeCost[i] = runeCostData->RuneCost[i];
        if (Player* modOwner = m_caster->GetSpellModOwner())
            modOwner->ApplySpellMod(m_spellInfo->Id, SPELLMOD_COST, runeCost[i], this);
    }

    runeCost[RUNE_DEATH] = 0;                               // calculated later

    for (uint32 i = 0; i < MAX_RUNES; ++i)
    {
        RuneType rune = player->GetCurrentRune(i);
        if (!player->GetRuneCooldown(i) && runeCost[rune] > 0)
        {
            player->SetRuneCooldown(i, didHit ? player->GetRuneBaseCooldown(i) : uint32(RUNE_MISS_COOLDOWN));
            player->SetLastUsedRune(rune);
            player->SetLastUsedRuneIndex(i);
            runeCost[rune]--;
        }
    }

    runeCost[RUNE_DEATH] = runeCost[RUNE_BLOOD] + runeCost[RUNE_UNHOLY] + runeCost[RUNE_FROST];

    if (runeCost[RUNE_DEATH] > 0)
    {
        for (uint32 i = 0; i < MAX_RUNES; ++i)
        {
            RuneType rune = player->GetCurrentRune(i);
            if (!player->GetRuneCooldown(i) && rune == RUNE_DEATH)
            {
                player->SetRuneCooldown(i, didHit ? player->GetRuneBaseCooldown(i) : uint32(RUNE_MISS_COOLDOWN));
                player->SetLastUsedRune(rune);
                player->SetLastUsedRuneIndex(i);
                runeCost[rune]--;

                // keep Death Rune type if missed
                if (didHit)
                    player->RestoreBaseRune(i);

                if (runeCost[RUNE_DEATH] == 0)
                    break;
            }
        }
    }

    if (player->HasAura(59052)) // freezing fog - ToolTip: Your next Icy Touch or Howling Blast will consume no runes and generate no Runic Power.
        if (m_spellInfo->Id == 45477 || m_spellInfo->Id == 49184)
            didHit = false;

    // you can gain some runic power when use runes
    if (didHit)
        if (int32 rp = int32(runeCostData->runePowerGain * sWorld->getRate(RATE_POWER_RUNICPOWER_INCOME)))
        {
            Unit::AuraEffectList const& bonusPct = m_caster->GetAuraEffectsByType(SPELL_AURA_MOD_RUNE_REGEN_SPEED);
            for (auto i = bonusPct.begin(); i != bonusPct.end(); ++i)
            {
                if ((*i)->GetId() == 63621) // improved frost presence
                {
                    if (player->HasAura(48265) || player->HasAura(48263)) // add improved frost presence bonus only at blood or unholy presence
                        AddPct(rp, (*i)->GetAmount());
                    continue;
                }
                else
                    AddPct(rp, (*i)->GetAmount());
            }
            player->ModifyPower(POWER_RUNIC_POWER, int32(rp));
        }

    if (player->HasAura(96269) || player->HasAura(96270))
    {
        if (m_spellInfo->Id == 43265 || m_spellInfo->Id == 51052 || m_spellInfo->Id == 63560) // Death's Advance hack fix for Death and Decay, Anti-Magic Zone & Dark Transformation
        {
            uint8 unholyRunesOnCooldown = 0;
            for (uint32 i = 2; i < 4; ++i)
            if (player->GetRuneCooldown(i))
                unholyRunesOnCooldown++;

            if (unholyRunesOnCooldown == 2)
            {
                int32 bp0 = 0;
                if (player->HasAura(96269))
                    bp0 = 60;
                else
                    bp0 = 75;

                player->CastCustomSpell(player, 96268, &bp0, NULL, NULL, true);
            }
        }
    }
}

void Spell::TakeReagents()
{
    ItemTemplate const* castItemTemplate = m_CastItem ? m_CastItem->GetTemplate() : NULL;

    // do not take reagents for these item casts
    if (castItemTemplate && castItemTemplate->Flags & ITEM_PROTO_FLAG_TRIGGERED_CAST)
        return;

    Player* p_caster = nullptr;

    if (m_caster->GetTypeId() != TYPEID_PLAYER)
        if (m_caster->IsVehicle())
            if (Unit* charmer = m_caster->GetCharmer())
                if (Player* Pcharmer = charmer->ToPlayer())
                    p_caster = Pcharmer;

    if (p_caster == nullptr)
    {
        if (m_caster->GetTypeId() != TYPEID_PLAYER)
            return;
        else p_caster = (Player*)m_caster;
    }

    if (p_caster->CanNoReagentCast(m_spellInfo))
        return;

    for (uint32 x = 0; x < MAX_SPELL_REAGENTS; ++x)
    {
        if (m_spellInfo->Reagent[x] <= 0)
            continue;

        uint32 itemid = m_spellInfo->Reagent[x];
        uint32 itemcount = m_spellInfo->ReagentCount[x];

        // if CastItem is also spell reagent
        if (castItemTemplate && castItemTemplate->ItemId == itemid)
        {
            for (int s = 0; s < MAX_ITEM_PROTO_SPELLS; ++s)
            {
                // CastItem will be used up and does not count as reagent
                int32 charges = m_CastItem ? m_CastItem->GetSpellCharges(s) : 0;
                if (castItemTemplate->Spells[s].SpellCharges < 0 && abs(charges) < 2)
                {
                    ++itemcount;
                    break;
                }
            }

            m_CastItem = NULL;
            m_castItemGUID = 0;
        }

        // if GetItemTarget is also spell reagent
        if (m_targets.GetItemTargetEntry() == itemid)
            m_targets.SetItemTarget(NULL);

        p_caster->DestroyItemCount(itemid, itemcount, true);
    }
}

void Spell::HandleThreatSpells()
{
    if (m_UniqueTargetInfo.empty())
        return;

    if ((m_spellInfo->AttributesEx  & SPELL_ATTR1_NO_THREAT) ||
        (m_spellInfo->AttributesEx3 & SPELL_ATTR3_NO_INITIAL_AGGRO))
        return;

    float threat = 0.0f;
    if (SpellThreatEntry const* threatEntry = sSpellMgr->GetSpellThreatEntry(m_spellInfo->Id))
    {
        if (threatEntry->apPctMod != 0.0f)
            threat += threatEntry->apPctMod * m_caster->GetTotalAttackPowerValue(BASE_ATTACK);

        threat += threatEntry->flatMod;
    }
    else if ((m_spellInfo->AttributesCu & SPELL_ATTR0_CU_NO_INITIAL_THREAT) == 0)
        threat += m_spellInfo->SpellLevel;

    // past this point only multiplicative effects occur
    if (threat == 0.0f)
        return;

    // since 2.0.1 threat from positive effects also is distributed among all targets, so the overall caused threat is at most the defined bonus
    threat /= m_UniqueTargetInfo.size();

    for (auto ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
    {
        float threatToAdd = threat;
        if (ihit->missCondition != SPELL_MISS_NONE)
            threatToAdd = 0.0f;

        Unit* target = ObjectAccessor::GetUnit(*m_caster, ihit->targetGUID);
        if (!target)
            continue;

        // positive spells distribute threat among all units that are in combat with target, like healing
        if (m_spellInfo->_IsPositiveSpell())
            target->getHostileRefManager().threatAssist(m_caster, threatToAdd, m_spellInfo);
        // for negative spells threat gets distributed among affected targets
        else
        {
            if (!target->CanHaveThreatList())
                continue;

            target->AddThreat(m_caster, threatToAdd, m_spellInfo->GetSchoolMask(), m_spellInfo);
        }
    }
    TC_LOG_DEBUG("spells", "Spell %u, added an additional %f threat for %s %u target(s)", m_spellInfo->Id, threat, m_spellInfo->_IsPositiveSpell() ? "assisting" : "harming", uint32(m_UniqueTargetInfo.size()));
}

void Spell::HandleHolyPower(Player* caster)
{
    if (!caster)
        return;

    bool hit = true;
    Player* modOwner = caster->GetSpellModOwner();

    m_powerCost = caster->GetPower(POWER_HOLY_POWER); // Always use all the holy power we have
	
    switch (m_spellInfo->Id)
    {
        case 85696: // Zealotry
            m_powerCost = 0;
            break;
    }
	// Eternal Glory
	bool returnResources = false;
	if (AuraEffect* const _effect = caster->GetDummyAuraEffect(SPELLFAMILY_PALADIN, 2944, EFFECT_0))
		returnResources = (roll_chance_i(_effect->GetAmount()) && (m_spellInfo->Id == 85673));
		

	if (returnResources && !m_costModified && !caster->HasSpellCooldown(88676))
	{
		caster->CastCustomSpell(88676, SPELLVALUE_BASE_POINT0, -m_powerCost, caster);
			m_powerCost = 0;
	}
    if (!m_powerCost || !modOwner)
        return;

    if (uint64 targetGUID = m_targets.GetUnitTargetGUID())
    {
        for (auto ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
        {
            if (ihit->targetGUID == targetGUID)
            {
                if (ihit->missCondition != SPELL_MISS_NONE && ihit->missCondition != SPELL_MISS_MISS)
                    hit = false;

                break;
            }
        }

        // The spell did hit the target, apply aura cost mods if there are any.
        if (hit)
        {
            modOwner->ApplySpellMod(m_spellInfo->Id, SPELLMOD_COST, m_powerCost);
            m_caster->ModifyPower(POWER_HOLY_POWER, -m_powerCost);
        }
    }
    else if (m_spellInfo->IsPositive())
    {
        modOwner->ApplySpellMod(m_spellInfo->Id, SPELLMOD_COST, m_powerCost);
        m_caster->ModifyPower(POWER_HOLY_POWER, -m_powerCost);
    }
}

void Spell::HandleEffects(Unit* pUnitTarget, Item* pItemTarget, GameObject* pGOTarget, uint32 i, SpellEffectHandleMode mode)
{
    effectHandleMode = mode;
    unitTarget = pUnitTarget;
    itemTarget = pItemTarget;
    gameObjTarget = pGOTarget;
    destTarget = &m_destTargets[i]._position;
    uint8 eff = m_spellInfo->Effects[i].Effect;

    TC_LOG_DEBUG("spells", "Spell: %u Effect : %u", m_spellInfo->Id, eff);

    damage = CalculateDamage(i, unitTarget);

    bool preventDefault = CallScriptEffectHandlers((SpellEffIndex)i, mode);

    if (!preventDefault && eff < TOTAL_SPELL_EFFECTS)
    {
        (this->*SpellEffects[eff])((SpellEffIndex)i);
    }
}

SpellCastResult Spell::CheckCast(bool strict)
{
    // check death state
    if (!m_caster->isAlive() && !(m_spellInfo->Attributes & SPELL_ATTR0_PASSIVE) && !((m_spellInfo->Attributes & SPELL_ATTR0_CASTABLE_WHILE_DEAD) || (IsTriggered() && !m_triggeredByAuraSpell)))
        return SPELL_FAILED_CASTER_DEAD;

    // check cooldowns to prevent cheating
    if (!(m_spellInfo->Attributes & SPELL_ATTR0_PASSIVE))
    {
        //can cast triggered (by aura only?) spells while have this flag
        if (m_caster->GetTypeId() == TYPEID_PLAYER)
        {
            if (!(_triggeredCastFlags & TRIGGERED_IGNORE_CASTER_AURASTATE) && m_caster->ToPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_ALLOW_ONLY_ABILITY))
                return SPELL_FAILED_SPELL_IN_PROGRESS;

            // check if we are using a potion in combat for the 2nd+ time. Cooldown is added only after caster gets out of combat
            if (m_caster->ToPlayer()->GetLastPotionId() && m_CastItem && m_CastItem->IsPotion())
                return SPELL_FAILED_NOT_READY;
        }

        if (m_caster->GetTypeId() == TYPEID_UNIT && m_caster->GetEntry() == 17252)
            if (!(_triggeredCastFlags & TRIGGERED_IGNORE_CASTER_AURASTATE) && m_caster->HasAuraType(SPELL_AURA_ALLOW_ONLY_ABILITY))
                return SPELL_FAILED_SPELL_IN_PROGRESS;

        if (m_caster->HasSpellCooldown(m_spellInfo->Id))
        {
            if (m_triggeredByAuraSpell || IsTriggered())
                return SPELL_FAILED_DONT_REPORT;
            else
                return SPELL_FAILED_NOT_READY;
        }
    }


    if (auto p = m_caster->ToPet())
        if (!m_spellInfo->IsPassive())
        if (p->hasCrowdControlPreventingActions(true))
        {
            switch (m_spellInfo->Id)
            {
            case 53490://Bull headed should be allowed to cast despite a cc
                break;
            default:
                return SPELL_FAILED_DONT_REPORT;
            }
        }

    if (m_spellInfo->AttributesEx7 & SPELL_ATTR7_IS_CHEAT_SPELL && !m_caster->HasFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_ALLOW_CHEAT_SPELLS))
    {
        m_customError = SPELL_CUSTOM_ERROR_GM_ONLY;
        return SPELL_FAILED_CUSTOM_ERROR;
    }

    // Check global cooldown
    if (strict && !(_triggeredCastFlags & TRIGGERED_IGNORE_GCD) && HasGlobalCooldown())
        return SPELL_FAILED_NOT_READY;

    // only triggered spells can be processed an ended battleground
    if (!IsTriggered() && m_caster->GetTypeId() == TYPEID_PLAYER)
        if (Battleground* bg = m_caster->ToPlayer()->GetBattleground())
            if (bg->GetStatus() == STATUS_WAIT_LEAVE)
                return SPELL_FAILED_DONT_REPORT;

    if (m_caster->GetTypeId() == TYPEID_PLAYER && VMAP::VMapFactory::createOrGetVMapManager()->isLineOfSightCalcEnabled())
    {
        if (m_spellInfo->Attributes & SPELL_ATTR0_OUTDOORS_ONLY &&
                !m_caster->GetMap()->IsOutdoors(m_caster->GetPositionX(), m_caster->GetPositionY(), m_caster->GetPositionZ()))
            return SPELL_FAILED_ONLY_OUTDOORS;

        if (m_spellInfo->Attributes & SPELL_ATTR0_INDOORS_ONLY &&
                m_caster->GetMap()->IsOutdoors(m_caster->GetPositionX(), m_caster->GetPositionY(), m_caster->GetPositionZ()))
            return SPELL_FAILED_ONLY_INDOORS;
    }

    if (Player* tmpPlayer = m_caster->ToPlayer())
        if (tmpPlayer->IsSpectator() && m_spellInfo->Id != 5118)
            return SPELL_FAILED_SPELL_UNAVAILABLE;

    // only check at first call, Stealth auras are already removed at second call
    // for now, ignore triggered spells
    if (strict && !(_triggeredCastFlags & TRIGGERED_IGNORE_SHAPESHIFT))
    {
        bool checkForm = true;
        // Ignore form req aura
        Unit::AuraEffectList const& ignore = m_caster->GetAuraEffectsByType(SPELL_AURA_MOD_IGNORE_SHAPESHIFT);
        for (auto i = ignore.begin(); i != ignore.end(); ++i)
        {
            if (!(*i)->IsAffectingSpell(m_spellInfo))
                continue;
            checkForm = false;
            break;
        }
        if (checkForm)
        {
            // Cannot be used in this stance/form
            SpellCastResult shapeError = m_spellInfo->CheckShapeshift(m_caster->GetShapeshiftForm());
            if (shapeError != SPELL_CAST_OK)
                return shapeError;

            if ((m_spellInfo->Attributes & SPELL_ATTR0_ONLY_STEALTHED) && !(m_caster->HasStealthAura()))
                return SPELL_FAILED_ONLY_STEALTHED;
        }
    }

    Unit::AuraEffectList const& blockSpells = m_caster->GetAuraEffectsByType(SPELL_AURA_BLOCK_SPELL_FAMILY);
    for (auto blockItr = blockSpells.begin(); blockItr != blockSpells.end(); ++blockItr)
    {
        if (m_caster->GetTypeId() == TYPEID_PLAYER && !(*blockItr)->GetMiscValue())
        {
            switch (m_caster->getClass())
            {
                case CLASS_DRUID:
                    if (m_spellInfo->SpellFamilyName == SPELLFAMILY_DRUID)
                        return SPELL_FAILED_SPELL_UNAVAILABLE;
                    break;
                case CLASS_HUNTER:
                    if (m_spellInfo->SpellFamilyName == SPELLFAMILY_HUNTER)
                        return SPELL_FAILED_SPELL_UNAVAILABLE;
                    break;
                case CLASS_MAGE:
                    if (m_spellInfo->SpellFamilyName == SPELLFAMILY_MAGE)
                        return SPELL_FAILED_SPELL_UNAVAILABLE;
                    break;
                case CLASS_WARLOCK:
                    if (m_spellInfo->SpellFamilyName == SPELLFAMILY_WARLOCK)
                        return SPELL_FAILED_SPELL_UNAVAILABLE;
                    break;
                case CLASS_WARRIOR:
                    if (m_spellInfo->SpellFamilyName == SPELLFAMILY_WARRIOR)
                        return SPELL_FAILED_SPELL_UNAVAILABLE;
                    break;
                case CLASS_PALADIN:
                    if (m_spellInfo->SpellFamilyName == SPELLFAMILY_PALADIN)
                        return SPELL_FAILED_SPELL_UNAVAILABLE;
                    break;
                case CLASS_PRIEST:
                    if (m_spellInfo->SpellFamilyName == SPELLFAMILY_PRIEST)
                        return SPELL_FAILED_SPELL_UNAVAILABLE;
                    break;
                case CLASS_SHAMAN:
                    if (m_spellInfo->SpellFamilyName == SPELLFAMILY_SHAMAN)
                        return SPELL_FAILED_SPELL_UNAVAILABLE;
                    break;
                case CLASS_ROGUE:
                    if (m_spellInfo->SpellFamilyName == SPELLFAMILY_ROGUE)
                        return SPELL_FAILED_SPELL_UNAVAILABLE;
                    break;
                case CLASS_DEATH_KNIGHT:
                    if (m_spellInfo->SpellFamilyName == SPELLFAMILY_DEATHKNIGHT)
                        return SPELL_FAILED_SPELL_UNAVAILABLE;
                    break;
            }
        }
        else if (uint32((*blockItr)->GetMiscValue()) == m_spellInfo->SpellFamilyName)
            return SPELL_FAILED_SPELL_UNAVAILABLE;
    }
    bool reqCombat = true;
    Unit::AuraEffectList const& stateAuras = m_caster->GetAuraEffectsByType(SPELL_AURA_ABILITY_IGNORE_AURASTATE);
    for (auto j = stateAuras.begin(); j != stateAuras.end(); ++j)
    {
        if ((*j)->IsAffectingSpell(m_spellInfo))
        {
            m_needComboPoints = false;
            if ((*j)->GetMiscValue() == 1)
            {
                reqCombat=false;
                break;
            }
        }
    }

    // caster state requirements
    // not for triggered spells (needed by execute)
    if (!(_triggeredCastFlags & TRIGGERED_IGNORE_CASTER_AURASTATE))
    {
        if (m_spellInfo->CasterAuraState && !m_caster->HasAuraState(AuraStateType(m_spellInfo->CasterAuraState), m_spellInfo, m_caster))
            return SPELL_FAILED_CASTER_AURASTATE;
        if (m_spellInfo->CasterAuraStateNot && m_caster->HasAuraState(AuraStateType(m_spellInfo->CasterAuraStateNot), m_spellInfo, m_caster))
            return SPELL_FAILED_CASTER_AURASTATE;
        // Note: spell 62473 requres casterAuraSpell = triggering spell
        if (m_spellInfo->CasterAuraSpell && !m_caster->HasAura(sSpellMgr->GetSpellIdForDifficulty(m_spellInfo->CasterAuraSpell, m_caster)))
            return SPELL_FAILED_CASTER_AURASTATE;
        if (reqCombat && m_caster->isInCombat() && !m_spellInfo->CanBeUsedInCombat())
        {
            if (m_targets.GetUnitTarget())
            {
                if (!m_caster->HasAura(7922, m_targets.GetUnitTarget()->GetGUID()))
                    return SPELL_FAILED_AFFECTING_COMBAT;
            }
            else return SPELL_FAILED_AFFECTING_COMBAT;
        }         
    }
    // Pursuit of Justice should check ExcludeCasterAuraSpell even though it is triggered
    if (m_spellInfo->ExcludeCasterAuraSpell && m_caster->HasAura(sSpellMgr->GetSpellIdForDifficulty(m_spellInfo->ExcludeCasterAuraSpell, m_caster)))
        return IsTriggered() ? SPELL_FAILED_DONT_REPORT : SPELL_FAILED_CASTER_AURASTATE;

    // cancel autorepeat spells if cast start when moving
    // (not wand currently autorepeat cast delayed to moving stop anyway in spell update code)
    // Do not cancel spells which are affected by a SPELL_AURA_CAST_WHILE_WALKING effect
    if (m_caster->GetTypeId() == TYPEID_PLAYER && m_caster->ToPlayer()->isMoving() && !m_caster->HasAuraTypeWithAffectMask(SPELL_AURA_CAST_WHILE_WALKING, m_spellInfo))
    {
        // skip stuck spell to allow use it in falling case and apply spell limitations at movement
        if ((!m_caster->HasUnitMovementFlag(MOVEMENTFLAG_FALLING_FAR) || m_spellInfo->Effects[0].Effect != SPELL_EFFECT_STUCK) &&
            (IsAutoRepeat() || (m_spellInfo->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED) != 0))
            return SPELL_FAILED_MOVING;
    }


    // Check vehicle flags
    if (!(_triggeredCastFlags & TRIGGERED_IGNORE_CASTER_MOUNTED_OR_ON_VEHICLE))
    {
        SpellCastResult vehicleCheck = m_spellInfo->CheckVehicle(m_caster);
        if (vehicleCheck != SPELL_CAST_OK)
            return vehicleCheck;
    }

    // check spell cast conditions from database
    {
        ConditionSourceInfo condInfo = ConditionSourceInfo(m_caster);
        condInfo.mConditionTargets[1] = m_targets.GetObjectTarget();
        ConditionList conditions = sConditionMgr->GetConditionsForNotGroupedEntry(CONDITION_SOURCE_TYPE_SPELL, m_spellInfo->Id);
        if (!conditions.empty() && !sConditionMgr->IsObjectMeetToConditions(condInfo, conditions))
        {
            // mLastFailedCondition can be NULL if there was an error processing the condition in Condition::Meets (i.e. wrong data for ConditionTarget or others)
            if (condInfo.mLastFailedCondition && condInfo.mLastFailedCondition->ErrorType)
            {
                if (condInfo.mLastFailedCondition->ErrorType == SPELL_FAILED_CUSTOM_ERROR)
                    m_customError = SpellCustomErrors(condInfo.mLastFailedCondition->ErrorTextId);
                return SpellCastResult(condInfo.mLastFailedCondition->ErrorType);
            }
            if (!condInfo.mLastFailedCondition || !condInfo.mLastFailedCondition->ConditionTarget)
                return SPELL_FAILED_CASTER_AURASTATE;
            return SPELL_FAILED_BAD_TARGETS;
        }
    }

    // Don't check explicit target for passive spells (workaround) (check should be skipped only for learn case)
    // those spells may have incorrect target entries or not filled at all (for example 15332)
    // such spells when learned are not targeting anyone using targeting system, they should apply directly to caster instead
    // also, such casts shouldn't be sent to client
    if (!((m_spellInfo->Attributes & SPELL_ATTR0_PASSIVE) && (!m_targets.GetUnitTarget() || m_targets.GetUnitTarget() == m_caster)))
    {
        // Check explicit target for m_originalCaster - todo: get rid of such workarounds
        SpellCastResult castResult = m_spellInfo->CheckExplicitTarget(m_originalCaster && m_caster->GetGoType() == MAX_GAMEOBJECT_TYPE ? m_originalCaster : m_caster, m_targets.GetObjectTarget(), m_targets.GetItemTarget());
        if (castResult != SPELL_CAST_OK)
            return castResult;
    }

    if (Unit* target = m_targets.GetUnitTarget())
    {
        SpellCastResult castResult = m_spellInfo->CheckTarget(m_caster, target, false);
        if (castResult != SPELL_CAST_OK)
            return castResult;

        if (target != m_caster)
        {
            // TODO: Replace by a more general check
            if (!strict && m_spellInfo->AttributesCu & SPELL_ATTR0_CU_AURA_CC && m_spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MAGIC)
                if (Aura* aur = target->GetAura(m_spellInfo->Id))
                    if (aur->GetDuration() > (aur->GetMaxDuration() / 2))
                        if (target->GetTypeId() == TYPEID_PLAYER)
                            return SPELL_FAILED_MORE_POWERFUL;

            // Must be behind the target
            if ((m_spellInfo->AttributesCu & SPELL_ATTR0_CU_REQ_CASTER_BEHIND_TARGET) && target->HasInArc(static_cast<float>(M_PI), m_caster))
            {
                bool needsBehind = true;
                switch (target->GetEntry())
                {
                    case 56471:
                    case 56167:
                    case 56846:
                        needsBehind = false;
                        break;
                    default:
                        break;
                }

                if (needsBehind)
                    return SPELL_FAILED_NOT_BEHIND;
            }

            // Target must be facing you
            if ((m_spellInfo->AttributesCu & SPELL_ATTR0_CU_REQ_TARGET_FACING_CASTER) && !target->HasInArc(static_cast<float>(M_PI), m_caster))
            {
                bool needsFacing = true;
                switch (m_spellInfo->Id)
                {
                    case 1776: // Gouge
                        needsFacing = !m_caster->HasAura(56809);
                        break;
                    default:
                        break;
                }
                if (needsFacing)
                    return SPELL_FAILED_NOT_INFRONT;
            }

            if (!IsTriggered() && m_caster->GetEntry() != WORLD_TRIGGER) // Ignore LOS for gameobjects casts (wrongly casted by a trigger)
                if (!DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, m_spellInfo->Id, m_caster, SPELL_DISABLE_LOS) && !m_caster->IsWithinLOSInMap(target, m_spellInfo))
                    return IsTriggered() ? SPELL_FAILED_DONT_REPORT : SPELL_FAILED_LINE_OF_SIGHT;
        }
    }

    // Check for line of sight for spells with dest
    if (m_targets.HasDst())
    {
        float x, y, z;
        m_targets.GetDstPos()->GetPosition(x, y, z);

        if (!DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, m_spellInfo->Id, m_caster, SPELL_DISABLE_LOS) && !m_caster->IsWithinLOS(x, y, z, m_spellInfo))
            return SPELL_FAILED_LINE_OF_SIGHT;
    }

    // check pet presence
    for (int j = 0; j < MAX_SPELL_EFFECTS; ++j)
    {
        if (m_spellInfo->Effects[j].TargetA.GetTarget() == TARGET_UNIT_PET)
        {
            Unit* target = m_caster->GetGuardianPet();
            if (!target)
            {
                if (m_triggeredByAuraSpell)              // not report pet not existence for triggered spells
                    return SPELL_FAILED_DONT_REPORT;
                else
                    return SPELL_FAILED_NO_PET;
            }
            else if (!target->isAlive())
                return SPELL_FAILED_TARGETS_DEAD;
            else
            {
                if (!m_triggeredByAuraSpell)
                    if (!m_caster->IsWithinLOSInMap(target, m_spellInfo))
                        return SPELL_FAILED_LINE_OF_SIGHT;
            }
            break;
        }
    }

    // do not allow spells to be cast in specific places
    if (m_spellInfo->AttributesEx8 & SPELL_ATTR8_NOT_IN_BG_OR_ARENA)
        if (Battlefield* bf = sBattlefieldMgr->GetBattlefieldToZoneId(m_caster->GetZoneId()))
            if (bf->IsWarTime())
                return SPELL_FAILED_NOT_IN_BATTLEGROUND;

    SpellCastResult castResult = CheckBattlegroundCastRules();
    if (castResult != SPELL_CAST_OK)
        return castResult;

    if (Player* player = m_caster->ToPlayer())
    {
        if (InstanceScript* instance = player->GetInstanceScript())
        {
            if (m_spellInfo->IsBattleResurrection())
                if (!instance->IsBattleResurrectionAvailable())
                    return SPELL_FAILED_IN_COMBAT_RES_LIMIT_REACHED;

            if (!m_spellInfo->CanBeUsedInCombat() && (m_spellInfo->HasEffect(SPELL_EFFECT_RESURRECT) || m_spellInfo->HasEffect(SPELL_EFFECT_RESURRECT_WITH_AURA)
                || m_spellInfo->HasEffect(SPELL_EFFECT_RESURRECT_NEW)))
                if (instance->IsEncounterInProgress())
                    return SPELL_FAILED_IN_COMBAT_RES_LIMIT_REACHED;
        }
    }

    // zone check
    if (m_caster->GetTypeId() == TYPEID_UNIT || !m_caster->ToPlayer()->isGameMaster())
    {
        uint32 zone, area;
        m_caster->GetZoneAndAreaId(zone, area);

        SpellCastResult locRes= m_spellInfo->CheckLocation(m_caster->GetMapId(), zone, area,
            m_caster->GetTypeId() == TYPEID_PLAYER ? m_caster->ToPlayer() : NULL);
        if (locRes != SPELL_CAST_OK)
            return locRes;
    }

    // not let players cast spells at mount (and let do it to creatures)
    if (m_caster->IsMounted() && m_caster->GetTypeId() == TYPEID_PLAYER && !(_triggeredCastFlags & TRIGGERED_IGNORE_CASTER_MOUNTED_OR_ON_VEHICLE) &&
        !m_spellInfo->IsPassive() && !(m_spellInfo->Attributes & SPELL_ATTR0_CASTABLE_WHILE_MOUNTED))
    {
        if (m_caster->isInFlight())
            return SPELL_FAILED_NOT_ON_TAXI;
        else
            return SPELL_FAILED_NOT_MOUNTED;
    }

    castResult = SPELL_CAST_OK;

    // always (except passive spells) check items (focus object can be required for any type casts)
    if (!m_spellInfo->IsPassive())
    {
        castResult = CheckItems();
        if (castResult != SPELL_CAST_OK)
            return castResult;
    }

    // Triggered spells also have range check
    // TODO: determine if there is some flag to enable/disable the check
    castResult = CheckRange(strict);
    if (castResult != SPELL_CAST_OK)
        return castResult;

    if (!(_triggeredCastFlags & TRIGGERED_IGNORE_POWER_AND_REAGENT_COST))
    {
        castResult = CheckPower();
        if (castResult != SPELL_CAST_OK)
            return castResult;
    }

    if (!(_triggeredCastFlags & TRIGGERED_IGNORE_CASTER_AURAS))
    {
        castResult = CheckCasterAuras();
        if (castResult != SPELL_CAST_OK)
            return castResult;
    }

    // script hook
    castResult = CallScriptCheckCastHandlers();
    if (castResult != SPELL_CAST_OK)
        return castResult;

    bool hasDispellableAura = false;
    bool hasNonDispelEffect = false;
    uint32 dispelMask = 0;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (m_spellInfo->Effects[i].Effect == SPELL_EFFECT_DISPEL)
        {
            if (m_spellInfo->Effects[i].IsTargetingArea() || m_spellInfo->AttributesEx & SPELL_ATTR1_MELEE_COMBAT_START)
            {
                hasDispellableAura = true;
                break;
            }

            dispelMask |= SpellInfo::GetDispelMask(DispelType(m_spellInfo->Effects[i].MiscValue));
        }
        else if (m_spellInfo->Effects[i].IsEffect())
        {
            hasNonDispelEffect = true;
            break;
        }
    }

    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        // for effects of spells that have only one target

        /*
            Block Spells by GMs
        */
        if (auto c = GetCaster())
            if (auto p = c->ToPlayer())
            if (auto s = p->GetSession())
                if (s->GetSecurity() >= SEC_MODERATOR)
                if (!p->isGMVisible())
                {
                    bool allow{ false };

                    switch (m_spellInfo->Effects[i].Effect)
                    {
                    case SPELL_EFFECT_SCHOOL_DAMAGE:
                    case SPELL_EFFECT_INSTAKILL:
                    case SPELL_EFFECT_SUMMON:
                    case SPELL_EFFECT_SUMMON_CHANGE_ITEM:
                    case SPELL_EFFECT_SUMMON_OBJECT_SLOT1:
                    case SPELL_EFFECT_SUMMON_OBJECT_SLOT2:
                    case SPELL_EFFECT_SUMMON_OBJECT_SLOT4:
                    case SPELL_EFFECT_SUMMON_OBJECT_WILD:
                    case SPELL_EFFECT_SUMMON_PET:
                    case SPELL_EFFECT_SUMMON_PLAYER:
                    case SPELL_EFFECT_TRANS_DOOR:
                    case SPELL_EFFECT_TRIGGER_MISSILE:
                    case SPELL_EFFECT_THREAT:
                    case SPELL_EFFECT_THREAT_ALL:
                    case SPELL_EFFECT_ATTACK_ME:
                    case SPELL_EFFECT_ATTACK:
                    case SPELL_EFFECT_CHARGE:
                    case SPELL_EFFECT_DISTRACT:
                    case SPELL_EFFECT_OPEN_LOCK:
                        break;
                    default:
                        allow = true;
                        if (auto t = m_targets.GetUnitTarget())
                            if (t != GetCaster())
                                allow = false;
                        break;
                    }
                    if (!allow)
                        return SPELL_FAILED_BM_OR_INVISGOD;
                }

        switch (m_spellInfo->Effects[i].Effect)
        {
            case SPELL_EFFECT_APPLY_AURA:
            {
                switch (m_spellInfo->Id)
                {
                    case 82692: // Focus Fire
                    {
                        if (Player* caster = m_caster->ToPlayer())
                            if (!caster->GetPet())
                                return SPELL_FAILED_NO_PET;
                            else if (caster->GetPet() && !caster->GetPet()->HasAura(19615))                      
                                return SPELL_FAILED_DONT_REPORT;
                        break;
                    }
                }
                break;
            }
            case SPELL_EFFECT_ADD_COMBO_POINTS:
            {
                if (m_spellInfo->Id == 73981)          // Redirect
                {
                    if (Player* plrCaster = m_caster->ToPlayer())
                    {
                        if (!plrCaster->GetComboPoints())
                            return SPELL_FAILED_NO_COMBO_POINTS;
                        else if (m_targets.GetUnitTarget())
                        {
                            if (plrCaster->GetComboTarget() == m_targets.GetUnitTarget()->GetGUID())
                                return SPELL_FAILED_BAD_TARGETS;
                        }
                    }

                }
                break;
            }
            case SPELL_EFFECT_DUMMY:
            {
                switch (m_spellInfo->Id)
                {
                    case 19938: // Awaken Peon
                    {
                        Unit* unit = m_targets.GetUnitTarget();
                        if (!unit || !unit->HasAura(17743))
                            return SPELL_FAILED_BAD_TARGETS;
                        break;
                    }
                    case 31789: // Righteous Defense
                    {
                        if (m_caster->GetTypeId() != TYPEID_PLAYER)
                            return SPELL_FAILED_DONT_REPORT;

                        Unit* target = m_targets.GetUnitTarget();
                        if (!target || !target->IsFriendlyTo(m_caster) || target->getAttackers().empty())
                            return SPELL_FAILED_BAD_TARGETS;
                        break;

                    }
                    case 34026: // Kill Command
                    {
                        Pet* pet = m_caster->ToPlayer()->GetPet();
                        if (!pet || !pet->getVictim() || !pet->isInCombat() || !pet->IsWithinCombatRange(pet->getVictim(), 5.0f))
                            return SPELL_FAILED_OUT_OF_RANGE;
                        break;
                    }
                }
                break;
            }
            case SPELL_EFFECT_LEARN_SPELL:
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    return SPELL_FAILED_BAD_TARGETS;

                if (m_spellInfo->Effects[i].TargetA.GetTarget() != TARGET_UNIT_PET)
                    break;

                Pet* pet = m_caster->ToPlayer()->GetPet();

                if (!pet)
                    return SPELL_FAILED_NO_PET;

                SpellInfo const* learn_spellproto = sSpellMgr->GetSpellInfo(m_spellInfo->Effects[i].TriggerSpell);

                if (!learn_spellproto)
                    return SPELL_FAILED_NOT_KNOWN;

                if (m_spellInfo->SpellLevel > pet->getLevel())
                    return SPELL_FAILED_LOWLEVEL;

                break;
            }
            case SPELL_EFFECT_UNLOCK_GUILD_VAULT_TAB:
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    return SPELL_FAILED_BAD_TARGETS;
                if (Guild* guild = m_caster->ToPlayer()->GetGuild())
                {
                    if (guild->GetLeaderGUID() != m_caster->ToPlayer()->GetGUID())
                        return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;
                }
                else
                    return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;
                break;
            }
            case SPELL_EFFECT_LEARN_PET_SPELL:
            {
                // check target only for unit target case
                if (Unit* unitTarget = m_targets.GetUnitTarget())
                {
                    if (m_caster->GetTypeId() != TYPEID_PLAYER)
                        return SPELL_FAILED_BAD_TARGETS;

                    Pet* pet = unitTarget->ToPet();
                    if (!pet || pet->GetOwner() != m_caster)
                        return SPELL_FAILED_BAD_TARGETS;

                    SpellInfo const* learn_spellproto = sSpellMgr->GetSpellInfo(m_spellInfo->Effects[i].TriggerSpell);

                    if (!learn_spellproto)
                        return SPELL_FAILED_NOT_KNOWN;

                    if (m_spellInfo->SpellLevel > pet->getLevel())
                        return SPELL_FAILED_LOWLEVEL;
                }
                break;
            }
            case SPELL_EFFECT_APPLY_GLYPH:
            {
                uint32 glyphId = m_spellInfo->Effects[i].MiscValue;
                if (GlyphPropertiesEntry const* gp = sGlyphPropertiesStore.LookupEntry(glyphId))
                    if (m_caster->HasAura(gp->SpellId))
                        return SPELL_FAILED_UNIQUE_GLYPH;
                break;
            }
            case SPELL_EFFECT_FEED_PET:
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    return SPELL_FAILED_BAD_TARGETS;

                Item* foodItem = m_targets.GetItemTarget();
                if (!foodItem)
                    return SPELL_FAILED_BAD_TARGETS;

                Pet* pet = m_caster->ToPlayer()->GetPet();

                if (!pet)
                    return SPELL_FAILED_NO_PET;

                if (!pet->HaveInDiet(foodItem->GetTemplate()))
                    return SPELL_FAILED_WRONG_PET_FOOD;

                if (!pet->GetCurrentFoodBenefitLevel(foodItem->GetTemplate()->ItemLevel))
                    return SPELL_FAILED_FOOD_LOWLEVEL;

                if (m_caster->isInCombat() || pet->isInCombat())
                    return SPELL_FAILED_AFFECTING_COMBAT;

                break;
            }
            case SPELL_EFFECT_POWER_BURN:
            case SPELL_EFFECT_POWER_DRAIN:
            {
                // Can be area effect, Check only for players and not check if target - caster (spell can have multiply drain/burn effects)
                if (m_caster->GetTypeId() == TYPEID_PLAYER)
                    if (Unit* target = m_targets.GetUnitTarget())
                        if (target != m_caster && target->getPowerType() != Powers(m_spellInfo->Effects[i].MiscValue))
                            return SPELL_FAILED_BAD_TARGETS;
                break;
            }
            case SPELL_EFFECT_CHARGE:
            {
                if (m_spellInfo->SpellFamilyName == SPELLFAMILY_WARRIOR)
                {
                    // Warbringer - can't be handled in proc system - should be done before checkcast root check and charge effect process
                    if (strict && m_caster->IsScriptOverriden(m_spellInfo, 6953))
                        m_caster->RemoveMovementImpairingAuras();
                }

                if (m_caster->HasUnitState(UNIT_STATE_ROOT))
                    return SPELL_FAILED_ROOTED;

                if (GetSpellInfo()->NeedsExplicitUnitTarget())
                {
                    Unit* target = m_targets.GetUnitTarget();
                    if (!target)
                        return SPELL_FAILED_DONT_REPORT;

                    float objSize = target->GetObjectSize();
                    float range = m_spellInfo->GetMaxRange(true, m_caster, this) * 1.5f + objSize; // can't be overly strict

                    m_preGeneratedPath.SetPathLengthLimit(range);
                    // first try with raycast, if it fails fall back to normal path
                    PathGenerator reverse_test(target);

                    bool result = m_preGeneratedPath.CalculatePath(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ() + target->GetObjectSize(), false, true);

                    bool result_inverse = reverse_test.CalculatePath(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ() + target->GetObjectSize(), false, true, true);

                    result = (result == true && result_inverse == true);

                    if (m_preGeneratedPath.GetPathType() & PATHFIND_SHORT)
                        return SPELL_FAILED_OUT_OF_RANGE;
                    else if (!result || m_preGeneratedPath.GetPathType() & PATHFIND_NOPATH)
                    {
                        result = m_preGeneratedPath.CalculatePath(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ() + target->GetObjectSize(), false, false);
                        if (m_preGeneratedPath.GetPathType() & PATHFIND_SHORT)
                            return SPELL_FAILED_OUT_OF_RANGE;
                        else if (!result || m_preGeneratedPath.GetPathType() & PATHFIND_NOPATH)
                            return SPELL_FAILED_NOPATH;
                        else if (m_caster->GetMapId() == 618 && !(target->GetPositionZ() <= 28.50f))
                            return SPELL_FAILED_NOPATH;
                    }

                    m_preGeneratedPath.ReducePathLenghtByDist(objSize); // move back
                }
                break;
            }
            case SPELL_EFFECT_SKINNING:
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER || !m_targets.GetUnitTarget() || m_targets.GetUnitTarget()->GetTypeId() != TYPEID_UNIT)
                    return SPELL_FAILED_BAD_TARGETS;

                if (!(m_targets.GetUnitTarget()->GetUInt32Value(UNIT_FIELD_FLAGS) & UNIT_FLAG_SKINNABLE))
                    return SPELL_FAILED_TARGET_UNSKINNABLE;

                Creature* creature = m_targets.GetUnitTarget()->ToCreature();
                if (creature->GetCreatureType() != CREATURE_TYPE_CRITTER && !creature->loot.isLooted())
                    return SPELL_FAILED_TARGET_NOT_LOOTED;

                uint32 skill = creature->GetCreatureTemplate()->GetRequiredLootSkill();

                int32 skillValue = m_caster->ToPlayer()->GetSkillValue(skill);
                int32 TargetLevel = m_targets.GetUnitTarget()->getLevel();
                int32 ReqValue = (skillValue < 100 ? (TargetLevel-10) * 10 : TargetLevel * 5);
                if (ReqValue > skillValue)
                    return SPELL_FAILED_LOW_CASTLEVEL;

                // chance for fail at orange skinning attempt
                if ((m_selfContainer && (*m_selfContainer) == this) &&
                    skillValue < sWorld->GetConfigMaxSkillValue() &&
                    (ReqValue < 0 ? 0 : ReqValue) > irand(skillValue - 25, skillValue + 37))
                    return SPELL_FAILED_TRY_AGAIN;

                break;
            }
            case SPELL_EFFECT_OPEN_LOCK:
            {
                if (m_spellInfo->Effects[i].TargetA.GetTarget() != TARGET_GAMEOBJECT_TARGET &&
                    m_spellInfo->Effects[i].TargetA.GetTarget() != TARGET_GAMEOBJECT_ITEM_TARGET)
                    break;

                if (m_caster->GetTypeId() != TYPEID_PLAYER  // only players can open locks, gather etc.
                    // we need a go target in case of TARGET_GAMEOBJECT_TARGET
                    || (m_spellInfo->Effects[i].TargetA.GetTarget() == TARGET_GAMEOBJECT_TARGET && !m_targets.GetGOTarget()))
                    return SPELL_FAILED_BAD_TARGETS;

                Item* pTempItem = NULL;
                if (m_targets.GetTargetMask() & TARGET_FLAG_TRADE_ITEM)
                {
                    if (TradeData* pTrade = m_caster->ToPlayer()->GetTradeData())
                        pTempItem = pTrade->GetTraderData()->GetItem(TradeSlots(m_targets.GetItemTargetGUID()));
                }
                else if (m_targets.GetTargetMask() & TARGET_FLAG_ITEM)
                    pTempItem = m_caster->ToPlayer()->GetItemByGuid(m_targets.GetItemTargetGUID());

                // we need a go target, or an openable item target in case of TARGET_GAMEOBJECT_ITEM_TARGET
                if (m_spellInfo->Effects[i].TargetA.GetTarget() == TARGET_GAMEOBJECT_ITEM_TARGET &&
                    !m_targets.GetGOTarget() &&
                    (!pTempItem || !pTempItem->GetTemplate()->LockID || !pTempItem->IsLocked()))
                    return SPELL_FAILED_BAD_TARGETS;

                if (m_spellInfo->Id != 1842 || (m_targets.GetGOTarget() &&
                    m_targets.GetGOTarget()->GetGOInfo()->type != GAMEOBJECT_TYPE_TRAP))
                    if (m_caster->ToPlayer()->InBattleground() && // In Battleground players can use only flags and banners
                        !m_caster->ToPlayer()->CanUseBattlegroundObject(m_targets.GetGOTarget()))
                        return SPELL_FAILED_TRY_AGAIN;

                // get the lock entry
                uint32 lockId = 0;
                if (GameObject* go = m_targets.GetGOTarget())
                {
                    lockId = go->GetGOInfo()->GetLockId();
                    if (!lockId)
                        return SPELL_FAILED_BAD_TARGETS;
                }
                else if (Item* itm = m_targets.GetItemTarget())
                    lockId = itm->GetTemplate()->LockID;

                SkillType skillId = SKILL_NONE;
                int32 reqSkillValue = 0;
                int32 skillValue = 0;

                // check lock compatibility
                SpellCastResult res = CanOpenLock(i, lockId, skillId, reqSkillValue, skillValue);
                if (res != SPELL_CAST_OK)
                    return res;

                // chance for fail at orange mining/herb/LockPicking gathering attempt
                // second check prevent fail at rechecks
                if (skillId != SKILL_NONE && (!m_selfContainer || ((*m_selfContainer) != this)))
                {
                    bool canFailAtMax = skillId != SKILL_HERBALISM && skillId != SKILL_MINING && skillId != SKILL_ARCHAEOLOGY;

                    // chance for failure in orange gather / lockpick (gathering skill can't fail at maxskill)
                    if ((canFailAtMax || skillValue < sWorld->GetConfigMaxSkillValue()) && reqSkillValue > irand(skillValue - 25, skillValue + 37))
                        return SPELL_FAILED_TRY_AGAIN;
                }
                break;
            }
            case SPELL_EFFECT_RESURRECT_PET:
            {
                Creature* pet = m_caster->GetGuardianPet();

                if (pet && pet->isAlive())
                    return SPELL_FAILED_ALREADY_HAVE_SUMMON;

                break;
            }
            // This is generic summon effect
            case SPELL_EFFECT_SUMMON:
            {
                SummonPropertiesEntry const* SummonProperties = sSummonPropertiesStore.LookupEntry(m_spellInfo->Effects[i].MiscValueB);
                if (!SummonProperties)
                    break;
                switch (SummonProperties->Category)
                {
                    case SUMMON_CATEGORY_PET:
                        m_caster->RemoveAllMinionsByEntry(m_spellInfo->Effects[i].MiscValue);
                        break;
                    case SUMMON_CATEGORY_PUPPET:
                        if (m_caster->GetCharmGUID())
                            return SPELL_FAILED_ALREADY_HAVE_CHARM;
                        break;
                }
                break;
            }
            case SPELL_EFFECT_CREATE_TAMED_PET:
            {
                if (m_targets.GetUnitTarget())
                {
                    if (m_targets.GetUnitTarget()->GetTypeId() != TYPEID_PLAYER)
                        return SPELL_FAILED_BAD_TARGETS;
                    if (m_targets.GetUnitTarget()->GetPetGUID())
                        return SPELL_FAILED_ALREADY_HAVE_SUMMON;
                }
                break;
            }
            case SPELL_EFFECT_SUMMON_PET:
            {
                if (m_caster->GetPetGUID())                  //let warlock do a replacement summon
                {
                    if (m_caster->GetTypeId() == TYPEID_PLAYER && m_caster->getClass() == CLASS_WARLOCK)
                    {
                        if (strict)                         //starting cast, trigger pet stun (cast by pet so it doesn't attack player)
                            if (Pet* pet = m_caster->ToPlayer()->GetPet())
                                pet->CastSpell(pet, 32752, true, NULL, NULL, pet->GetGUID());
                    }
                    else
                        m_caster->RemoveAllMinionsByEntry(m_spellInfo->Effects[i].MiscValue);
                }

                if (m_caster->GetCharmGUID())
                    return SPELL_FAILED_ALREADY_HAVE_CHARM;
                break;
            }
            case SPELL_EFFECT_SUMMON_PLAYER:
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    return SPELL_FAILED_BAD_TARGETS;
                if (!m_caster->ToPlayer()->GetSelection())
                    return SPELL_FAILED_BAD_TARGETS;

                if (Battleground const* bg = m_caster->ToPlayer()->GetBattleground())
                    return SPELL_FAILED_NOT_IN_BATTLEGROUND;

                Player* target = ObjectAccessor::FindPlayer(m_caster->ToPlayer()->GetSelection());
                if (!target || m_caster->ToPlayer() == target || (!target->IsInSameRaidWith(m_caster->ToPlayer()) && m_spellInfo->Id != 48955)) // refer-a-friend spell
                    return SPELL_FAILED_BAD_TARGETS;

                // check if our map is dungeon
                MapEntry const* map = sMapStore.LookupEntry(m_caster->GetMapId());
                if (map->IsDungeon())
                {
                    uint32 mapId = m_caster->GetMap()->GetId();
                    Difficulty difficulty = m_caster->GetMap()->GetDifficulty();
                    if (map->IsRaid())
                        if (InstancePlayerBind* targetBind = target->GetBoundInstance(mapId, difficulty))
                            if (InstancePlayerBind* casterBind = m_caster->ToPlayer()->GetBoundInstance(mapId, difficulty))
                                if (targetBind->perm && targetBind->save != casterBind->save)
                                    return SPELL_FAILED_TARGET_LOCKED_TO_RAID_INSTANCE;

                    InstanceTemplate const* instance = sObjectMgr->GetInstanceTemplate(mapId);
                    if (!instance)
                        return SPELL_FAILED_TARGET_NOT_IN_INSTANCE;
                    if (!target->Satisfy(sObjectMgr->GetAccessRequirement(mapId, difficulty), mapId))
                        return SPELL_FAILED_BAD_TARGETS;
                }
                break;
            }
            // RETURN HERE
            case SPELL_EFFECT_SUMMON_RAF_FRIEND:
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    return SPELL_FAILED_BAD_TARGETS;

                Player* playerCaster = m_caster->ToPlayer();
                    //
                if (!(playerCaster->GetSelection()))
                    return SPELL_FAILED_BAD_TARGETS;

                Player* target = ObjectAccessor::FindPlayer(playerCaster->GetSelection());

                if (!target ||
                    !(target->GetSession()->GetRecruiterId() == playerCaster->GetSession()->GetAccountId() || target->GetSession()->GetAccountId() == playerCaster->GetSession()->GetRecruiterId()))
                    return SPELL_FAILED_BAD_TARGETS;

                break;
            }
            case SPELL_EFFECT_LEAP:
            case SPELL_EFFECT_TELEPORT_UNITS_FACE_CASTER:
            {
              //Do not allow to cast it before BG starts.
                if (m_caster->GetTypeId() == TYPEID_PLAYER)
                    if (Battleground const* bg = m_caster->ToPlayer()->GetBattleground())
                        if (bg->GetStatus() != STATUS_IN_PROGRESS)
                            return SPELL_FAILED_TRY_AGAIN;
                break;
            }
            case SPELL_EFFECT_STEAL_BENEFICIAL_BUFF:
            {
                if (m_targets.GetUnitTarget() == m_caster)
                    return SPELL_FAILED_BAD_TARGETS;
                break;
            }
            case SPELL_EFFECT_JUMP_DEST:
            case SPELL_EFFECT_LEAP_BACK:
            {
                if (m_caster->HasUnitState(UNIT_STATE_ROOT) && m_spellInfo->Id != 92832)
                {
                    if (m_caster->GetTypeId() == TYPEID_PLAYER)
                        return SPELL_FAILED_ROOTED;
                    else
                        return SPELL_FAILED_DONT_REPORT;
                }
                break;
            }
            case SPELL_EFFECT_TALENT_SPEC_SELECT:
                // can't change during already started arena/battleground
                if (m_caster->GetTypeId() == TYPEID_PLAYER)
                    if (Battleground const* bg = m_caster->ToPlayer()->GetBattleground())
                        if (bg->GetStatus() == STATUS_IN_PROGRESS)
                            return SPELL_FAILED_NOT_IN_BATTLEGROUND;
                break;
            default:
                break;
        }
    }

    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        switch (m_spellInfo->Effects[i].ApplyAuraName)
        {
            case SPELL_AURA_MOD_POSSESS_PET:
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    return SPELL_FAILED_NO_PET;

                Pet* pet = m_caster->ToPlayer()->GetPet();
                if (!pet)
                    return SPELL_FAILED_NO_PET;

                if (pet->GetCharmerGUID())
                    return SPELL_FAILED_CHARMED;
                break;
            }
            case SPELL_AURA_MOD_POSSESS:
            case SPELL_AURA_MOD_CHARM:
            case SPELL_AURA_AOE_CHARM:
            {
                if (m_caster->GetCharmerGUID())
                    return SPELL_FAILED_CHARMED;

                if (m_spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOD_CHARM
                    || m_spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOD_POSSESS)
                {
                    /*if (m_spellInfo->Id == 605 && m_caster->GetPetGUID()) // tested on retail - if the priest casts mind control the shadowfiend is despawned
                        if (Unit* pet = m_caster->GetGuardianPet())
                            if (pet->GetTypeId() == TYPEID_UNIT && pet->GetEntry() == 19668)
                                pet->ToCreature()->DespawnOrUnsummon();

                    if (m_caster->GetPetGUID())
                        return SPELL_FAILED_ALREADY_HAVE_SUMMON;*/ // Moved to spell_pri_mind_control_SpellScript

                    if (m_caster->GetCharmGUID())
                        return SPELL_FAILED_ALREADY_HAVE_CHARM;
                }

                if (Unit* target = m_targets.GetUnitTarget())
                {
                    if (target->GetTypeId() == TYPEID_UNIT && target->ToCreature()->IsVehicle())
                        return SPELL_FAILED_BAD_IMPLICIT_TARGETS;

                    if (target->GetTypeId() == TYPEID_UNIT && target->IsMounted())
                        return SPELL_FAILED_CANT_BE_CHARMED;

                    if (target->GetCharmerGUID())
                        return SPELL_FAILED_CHARMED;

                    if (target->GetOwner() && target->GetOwner()->GetTypeId() == TYPEID_PLAYER)
                        return SPELL_FAILED_TARGET_IS_PLAYER_CONTROLLED;

                    int32 damage = CalculateDamage(i, target);
                    if (damage && int32(target->getLevel()) > damage)
                        return SPELL_FAILED_HIGHLEVEL;
                }

                break;
            }
            case SPELL_AURA_MOUNTED:
            {
                bool allowInWaterMount = false;

                if (MountCapabilityEntry const* mountCapability = sMountCapabilityStore.LookupEntry(m_spellInfo->Effects[i].MiscValueB))
                    if (mountCapability->Flags & MOUNT_FLAG_CAN_SWIM)
                        allowInWaterMount = true;

                // @hack for poseidon
                if (m_caster->IsInWater() && !allowInWaterMount && m_spellInfo->Effects[i].MiscValue != 53270) // hackfix for poseidon
                    return SPELL_FAILED_ONLY_ABOVEWATER;

                // Ignore map check if spell have AreaId. AreaId already checked and this prevent special mount spells
                bool allowMount = !m_caster->GetMap()->IsDungeon() || m_caster->GetMap()->IsBattlegroundOrArena();
                InstanceTemplate const* it = sObjectMgr->GetInstanceTemplate(m_caster->GetMapId());
                
                if (it)
                    allowMount = it->AllowMount;
                if (m_caster->GetTypeId() == TYPEID_PLAYER && !allowMount && !m_spellInfo->AreaGroupId)
                    return SPELL_FAILED_NO_MOUNTS_ALLOWED;

                if (it && m_caster->HasAura(102994)) // Stealth in WoE
                    return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;

                if (m_caster->IsInDisallowedMountForm())
                    return SPELL_FAILED_NOT_SHAPESHIFT;

                // disable mounting for stealthed legendary rogue quests
                if (m_caster->HasAura(106015) || m_caster->HasAura(109156) || m_caster->HasAura(68481))
                    return SPELL_FAILED_NO_MOUNTS_ALLOWED;

                break;
            }
            case SPELL_AURA_RANGED_ATTACK_POWER_ATTACKER_BONUS:
            {
                if (!m_targets.GetUnitTarget())
                    return SPELL_FAILED_BAD_IMPLICIT_TARGETS;

                // can be casted at non-friendly unit or own pet/charm
                if (m_caster->IsFriendlyTo(m_targets.GetUnitTarget()))
                    return SPELL_FAILED_TARGET_FRIENDLY;

                break;
            }
            case SPELL_AURA_FLY:
            case SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED:
            {
                // not allow cast fly spells if not have req. skills  (all spells is self target)
                // allow always ghost flight spells
                if (m_originalCaster && m_originalCaster->GetTypeId() == TYPEID_PLAYER && m_originalCaster->isAlive())
                {
                    Battlefield* Bf = sBattlefieldMgr->GetBattlefieldToZoneId(m_originalCaster->GetZoneId());
                    if (AreaTableEntry const* area = GetAreaEntryByAreaID(m_originalCaster->GetAreaId()))
                        if (area->flags & AREA_FLAG_NO_FLY_ZONE  || (Bf && !Bf->CanFlyIn()))
                            return (_triggeredCastFlags & TRIGGERED_DONT_REPORT_CAST_ERROR) ? SPELL_FAILED_DONT_REPORT : SPELL_FAILED_NOT_HERE;
                }
                break;
            }
            case SPELL_AURA_PERIODIC_MANA_LEECH:
            {
                if (m_spellInfo->Effects[i].IsTargetingArea())
                    break;

                if (!m_targets.GetUnitTarget())
                    return SPELL_FAILED_BAD_IMPLICIT_TARGETS;

                if (m_caster->GetTypeId() != TYPEID_PLAYER || m_CastItem)
                    break;

                if (m_targets.GetUnitTarget()->getPowerType() != POWER_MANA)
                    return SPELL_FAILED_BAD_TARGETS;

                break;
            }
            default:
                break;
        }
    }

    // check trade slot case (last, for allow catch any another cast problems)
    if (m_targets.GetTargetMask() & TARGET_FLAG_TRADE_ITEM)
    {
        if (m_CastItem)
            return SPELL_FAILED_ITEM_ENCHANT_TRADE_WINDOW;

        if (m_caster->GetTypeId() != TYPEID_PLAYER)
            return SPELL_FAILED_NOT_TRADING;

        TradeData* my_trade = m_caster->ToPlayer()->GetTradeData();

        if (!my_trade)
            return SPELL_FAILED_NOT_TRADING;

        TradeSlots slot = TradeSlots(m_targets.GetItemTargetGUID());
        if (slot != TRADE_SLOT_NONTRADED)
            return SPELL_FAILED_BAD_TARGETS;

        if (!IsTriggered())
            if (my_trade->GetSpell())
                return SPELL_FAILED_ITEM_ALREADY_ENCHANTED;
    }

    // check if caster has at least 1 combo point for spells that require combo points
    if (m_needComboPoints)
        if (Player* plrCaster = m_caster->ToPlayer())
            if (!plrCaster->GetComboPoints())
                return SPELL_FAILED_NO_COMBO_POINTS;

    // Spells with charge effect shall not be usable while rooted
    if (m_spellInfo->AttributesEx7 & SPELL_ATTR7_HAS_CHARGE_EFFECT && m_caster->HasUnitState(UNIT_STATE_ROOT))
        return SPELL_FAILED_ROOTED;

    // 55694 - Enraged Regeneration -> "....and new Enrage effects may not be gained while active."
    if (m_spellInfo->Id == 12292 && m_caster->HasAura(55694))
        return SPELL_FAILED_DONT_REPORT;

    // Berserk (Cat or Bear Form)
    if (m_spellInfo->Id == 50334 && !m_caster->IsInFeralForm())
        return SPELL_FAILED_DONT_REPORT;

    if (m_caster->HasAura(51514)) // shaman hex
        if (m_spellInfo->Id == 1130 || m_spellInfo->Id == 781 || m_spellInfo->Id == 8143 || m_spellInfo->SpellIconID == 455) // this spells shouldn't work on hex...
            return SPELL_FAILED_DONT_REPORT;

    if (m_spellInfo->RequiresSpellFocus)
    {
        CellCoord p(Trinity::ComputeCellCoord(m_caster->GetPositionX(), m_caster->GetPositionY()));
        Cell cell(p);

        GameObject* ok = NULL;
        Trinity::GameObjectFocusCheck go_check(m_caster, m_spellInfo->RequiresSpellFocus);
        Trinity::GameObjectSearcher<Trinity::GameObjectFocusCheck> checker(m_caster, ok, go_check);

        TypeContainerVisitor<Trinity::GameObjectSearcher<Trinity::GameObjectFocusCheck>, GridTypeMapContainer > object_checker(checker);
        Map& map = *m_caster->GetMap();
        cell.Visit(p, object_checker, map, *m_caster, m_caster->GetVisibilityRange());

        if (!ok)
            return SPELL_FAILED_REQUIRES_SPELL_FOCUS;
    }

    // all ok
    return SPELL_CAST_OK;
}

SpellCastResult Spell::CheckPetCast(Unit* target)
{
    if (m_caster->HasUnitState(UNIT_STATE_CASTING) && !(_triggeredCastFlags & TRIGGERED_IGNORE_CAST_IN_PROGRESS))              //prevent spellcast interruption by another spellcast
        return SPELL_FAILED_SPELL_IN_PROGRESS;

    // dead owner (pets still alive when owners ressed?)
    if (Unit* owner = m_caster->GetCharmerOrOwner())
        if (!owner->isAlive())
            return SPELL_FAILED_CASTER_DEAD;

    if (!target && m_targets.GetUnitTarget())
        target = m_targets.GetUnitTarget();

    if (m_spellInfo->NeedsExplicitUnitTarget())
    {
        if (!target)
            return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
        m_targets.SetUnitTarget(target);
    }

    return CheckCast(true);
}

uint32 Spell::GetCustomDelay(SpellInfo const* _spell)
{
    if (sWorld->getBoolConfig(CONFIG_SPELL_DELAY_ENABLE))
    {
        const uint32 delayValueSpell = sWorld->getIntConfig(CONFIG_SPELL_DELAY_SPELLS);
        const uint32 delayValueSlow = sWorld->getIntConfig(CONFIG_SPELL_DELAY_SLOWS);

        switch (_spell->Id)
        {
        case 118: // Polymorph
        case 2094: // Blind
        case 1776: // Gouge
        case 5484: // Howl of Terror
        case 6343: // Fear Ward
        case 6770: // Sap
        case 6789: // Death coil (warlock)
        case 8122: // Psychic Scream
        case 19503: // Scatter Shot
        case 20066: // Repentance
        case 31661: // Dragon's Breath
        case 32379: // Shadow Word: Death
        case 49576: // Death Grip
        case 64044: // Psychic Horror
        case 88625: // Holy Word: Chastise
            return delayValueSpell;
            break;
        case 7321: // Chilled
            return delayValueSlow;
            break;
        }
    }

    return 0;
}

SpellCastResult Spell::CheckCasterAuras() const
{
    // spells totally immuned to caster auras (wsg flag drop, give marks etc)
    if (m_spellInfo->AttributesEx6 & SPELL_ATTR6_IGNORE_CASTER_AURAS)
        return SPELL_CAST_OK;

    uint8 school_immune = 0;
    uint32 mechanic_immune = 0;
    uint32 dispel_immune = 0;

    // Check if the spell grants school or mechanic immunity.
    // We use bitmasks so the loop is done only once and not on every aura check below.
    if (m_spellInfo->AttributesEx & SPELL_ATTR1_DISPEL_AURAS_ON_IMMUNITY)
    {
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            if (m_spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_SCHOOL_IMMUNITY)
                school_immune |= uint32(m_spellInfo->Effects[i].MiscValue);
            else if (m_spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MECHANIC_IMMUNITY)
                mechanic_immune |= 1 << uint32(m_spellInfo->Effects[i].MiscValue);
            else if (m_spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_DISPEL_IMMUNITY)
                dispel_immune |= SpellInfo::GetDispelMask(DispelType(m_spellInfo->Effects[i].MiscValue));
        }

        // Custom mechanic immunitys - TODO: Find a better way to handle this
        // Tremor totem for example has USABLE_WHILE_FEARED - This should most likely include MECHANIC_FEAR, MECHANIC_SLEEP & MECHANIC_CHARM
        switch (m_spellInfo->Id)
        {
            // Bullheaded, PvP Trinkets & Bestial wrath
            case 53490:
            case 42292:
            case 59752:
            case 19574:
            case 70029:
                mechanic_immune = IMMUNE_TO_MOVEMENT_IMPAIRMENT_AND_LOSS_CONTROL_MASK;
                break;
            // Tremor Totem
            case 8143:
                mechanic_immune = (1 << MECHANIC_FEAR) | (1 << MECHANIC_SLEEP) | (1 << MECHANIC_CHARM);
                break;
        }
    }

    bool usableInStun = m_spellInfo->AttributesEx5 & SPELL_ATTR5_USABLE_WHILE_STUNNED;

    // Glyph of Pain Suppression
    // Allow Pain Suppression and Guardian Spirit to be cast while stunned
    // there is no other way to handle it
    if ((m_spellInfo->Id == 33206 || m_spellInfo->Id == 47788) && !m_caster->HasAura(63248))
        usableInStun = false;

    // Check whether the cast should be prevented by any state you might have.
    SpellCastResult prevented_reason = SPELL_CAST_OK;
    // Have to check if there is a stun aura. Otherwise will have problems with ghost aura apply while logging out
    uint32 unitflag = m_caster->GetUInt32Value(UNIT_FIELD_FLAGS);     // Get unit state
    if (unitflag & UNIT_FLAG_STUNNED)
    {
        // spell is usable while stunned, check if caster has only mechanic stun auras, another stun types must prevent cast spell
        if (usableInStun)
        {
            bool foundNotStun = false;
            Unit::AuraEffectList const& stunAuras = m_caster->GetAuraEffectsByType(SPELL_AURA_MOD_STUN);
            for (auto i = stunAuras.begin(); i != stunAuras.end(); ++i)
            {
                if ((*i)->GetSpellInfo()->GetAllEffectsMechanicMask()
                    && (*i)->GetSpellInfo()->GetAllEffectsMechanicMask() & ((1 << MECHANIC_BANISH) | (1<<MECHANIC_KNOCKOUT) | (1<<MECHANIC_FEAR) | (1<<MECHANIC_SAPPED)| (1<<MECHANIC_CHARM)))
                {
                    foundNotStun = true;
                    break;
                }
            }
            if (foundNotStun && m_spellInfo->Id != 22812)
                prevented_reason = SPELL_FAILED_STUNNED;
        }
        else
        {
            if (m_targets.GetUnitTarget())
            {
                if ((!m_caster->HasAura(7922, m_targets.GetUnitTarget()->GetGUID())) && (!m_caster->HasAura(20253, m_targets.GetUnitTarget()->GetGUID())))
                    prevented_reason = SPELL_FAILED_STUNNED;
            }
            else prevented_reason = SPELL_FAILED_STUNNED;
        }
    }
    else if (unitflag & UNIT_FLAG_CONFUSED && !(m_spellInfo->AttributesEx5 & SPELL_ATTR5_USABLE_WHILE_CONFUSED))
        prevented_reason = SPELL_FAILED_CONFUSED;
    else if (unitflag & UNIT_FLAG_FLEEING && !(m_spellInfo->AttributesEx5 & SPELL_ATTR5_USABLE_WHILE_FEARED))
        prevented_reason = SPELL_FAILED_FLEEING;
    else if (unitflag & UNIT_FLAG_SILENCED && m_spellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)
        prevented_reason = SPELL_FAILED_SILENCED;
    else if (unitflag & UNIT_FLAG_PACIFIED && m_spellInfo->PreventionType == SPELL_PREVENTION_TYPE_PACIFY)
        prevented_reason = SPELL_FAILED_PACIFIED;
    else if (m_caster->HasAuraType(SPELL_AURA_MOD_PACIFY_SILENCE) && m_spellInfo->PreventionType == SPELL_PREVENTION_TYPE_UNK2)
        prevented_reason = SPELL_FAILED_PACIFIED;

    // Attr must make flag drop spell totally immune from all effects
    if (prevented_reason != SPELL_CAST_OK)
    {
        if (school_immune || mechanic_immune || dispel_immune)
        {
            //Checking auras is needed now, because you are prevented by some state but the spell grants immunity.
            Unit::AuraApplicationMap const& auras = m_caster->GetAppliedAuras();
            for (auto itr = auras.begin(); itr != auras.end(); ++itr)
            {
                Aura const* aura = itr->second->GetBase();
                SpellInfo const* auraInfo = aura->GetSpellInfo();
                if (auraInfo->GetAllEffectsMechanicMask() & mechanic_immune)
                    continue;
                if (auraInfo->GetSchoolMask() & school_immune && !(auraInfo->AttributesEx & SPELL_ATTR1_UNAFFECTED_BY_SCHOOL_IMMUNE))
                    continue;
                if (auraInfo->GetDispelMask() & dispel_immune)
                    continue;

                //Make a second check for spell failed so the right SPELL_FAILED message is returned.
                //That is needed when your casting is prevented by multiple states and you are only immune to some of them.
                for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                {
                    if (AuraEffect* part = aura->GetEffect(i))
                    {
                        switch (part->GetAuraType())
                        {
                            case SPELL_AURA_MOD_STUN:
                                if (!usableInStun || !(auraInfo->GetAllEffectsMechanicMask() & (1<<MECHANIC_STUN)))
                                    return SPELL_FAILED_STUNNED;
                                break;
                            case SPELL_AURA_MOD_CONFUSE:
                                if (!(m_spellInfo->AttributesEx5 & SPELL_ATTR5_USABLE_WHILE_CONFUSED))
                                    return SPELL_FAILED_CONFUSED;
                                break;
                            case SPELL_AURA_MOD_FEAR:
                                if (!(m_spellInfo->AttributesEx5 & SPELL_ATTR5_USABLE_WHILE_FEARED))
                                    return SPELL_FAILED_FLEEING;
                                break;
                            case SPELL_AURA_MOD_SILENCE:
                            case SPELL_AURA_MOD_PACIFY:
                            case SPELL_AURA_MOD_PACIFY_SILENCE:
                                if (m_spellInfo->PreventionType == SPELL_PREVENTION_TYPE_PACIFY)
                                    return SPELL_FAILED_PACIFIED;
                                else if (m_spellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)
                                    return SPELL_FAILED_SILENCED;
                                break;
                            default: break;
                        }
                    }
                }
            }
        }
        // You are prevented from casting and the spell casted does not grant immunity. Return a failed error.
        else
            return prevented_reason;
    }
    return SPELL_CAST_OK;
}

SpellCastResult Spell::CheckBattlegroundCastRules()
{
    Map* map = m_caster->GetMap();
    bool isBattleground = map->IsBattleground();
    bool isArena = map->IsBattleArena();
    bool isRatedBattleground = false;

    if (BattlegroundMap* bgMap = isBattleground ? reinterpret_cast<BattlegroundMap*>(map) : nullptr)
        if (bgMap->GetBG())
            isRatedBattleground = bgMap->GetBG()->isRatedBattleground();

    // Spell casted only on battleground
    if (!isBattleground && (m_spellInfo->AttributesEx3 & SPELL_ATTR3_BATTLEGROUND) != 0)
         return SPELL_FAILED_ONLY_BATTLEGROUNDS;

    if ((isBattleground || isArena) && (m_spellInfo->AttributesEx8 & SPELL_ATTR8_NOT_IN_BG_OR_ARENA) != 0)
        return SPELL_FAILED_NOT_IN_BATTLEGROUND;

    // check USABLE attributes
    // USABLE takes precedence over NOT_USABLE
    if (isRatedBattleground && (m_spellInfo->AttributesEx9 & SPELL_ATTR9_USABLE_IN_RATED_BATTLEGROUNDS) != 0)
        return SPELL_CAST_OK;

    if (isArena && (m_spellInfo->AttributesEx4 & SPELL_ATTR4_USABLE_IN_ARENA) != 0)
        return SPELL_CAST_OK;

    // check NOT_USABLE attributes
    if ((isArena || isRatedBattleground) && (m_spellInfo->AttributesEx4 & SPELL_ATTR4_NOT_USABLE_IN_ARENA_OR_RATED_BG) != 0)
        return isArena ? SPELL_FAILED_NOT_IN_ARENA : SPELL_FAILED_NOT_IN_RATED_BATTLEGROUND;

    if (isArena && (m_spellInfo->AttributesEx9 & SPELL_ATTR9_NOT_USABLE_IN_ARENA) != 0)
        return SPELL_FAILED_NOT_IN_ARENA;

    // check cooldowns
    uint32 spellCooldown = m_spellInfo->GetRecoveryTime();

    if (isArena && spellCooldown > 10 * MINUTE * IN_MILLISECONDS) // not sure if still needed
        return SPELL_FAILED_NOT_IN_ARENA;

    if (isRatedBattleground && spellCooldown > 15 * MINUTE * IN_MILLISECONDS)
        return SPELL_FAILED_NOT_IN_RATED_BATTLEGROUND;

    return SPELL_CAST_OK;
}

bool Spell::CanAutoCast(Unit* target)
{
    uint64 targetguid = target->GetGUID();

    for (uint32 j = 0; j < MAX_SPELL_EFFECTS; ++j)
    {
        if (m_spellInfo->Effects[j].Effect == SPELL_EFFECT_APPLY_AURA)
        {
            if (m_spellInfo->StackAmount <= 1)
            {
                if (target->HasAuraEffect(m_spellInfo->Id, j))
                    return false;
            }
            else
            {
                if (AuraEffect* aureff = target->GetAuraEffect(m_spellInfo->Id, j))
                    if (aureff->GetBase()->GetStackAmount() >= m_spellInfo->StackAmount)
                        return false;
            }
        }
        else if (m_spellInfo->Effects[j].IsAreaAuraEffect())
        {
            if (target->HasAuraEffect(m_spellInfo->Id, j))
                return false;
        }
    }

    SpellCastResult result = CheckPetCast(target);

    if (result == SPELL_CAST_OK || result == SPELL_FAILED_UNIT_NOT_INFRONT)
    {
        LoadScripts();
        SelectSpellTargets();
        //check if among target units, our WANTED target is as well (->only self cast spells return false)
        for (auto ihit= m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
            if (ihit->targetGUID == targetguid)
            {
                CleanupTargetList();
                UnloadScripts();
                return true;
            }
    }
    CleanupTargetList();
    UnloadScripts();
    return false;                                           //target invalid
}

SpellCastResult Spell::CheckRange(bool strict)
{
    // Don't check for instant cast spells
    if (!strict && m_casttime == 0)
        return SPELL_CAST_OK;

    uint32 range_type = 0;

    if (m_spellInfo->RangeEntry)
    {
        // check needed by 68766 51693 - both spells are cast on enemies and have 0 max range
        // these are triggered by other spells - possibly we should omit range check in that case?
        if (m_spellInfo->RangeEntry->ID == 1)
            return SPELL_CAST_OK;

        range_type = m_spellInfo->RangeEntry->type;
    }

    if (!m_caster)
        return SPELL_FAILED_DONT_REPORT;

    Unit* target = m_targets.GetUnitTarget();
    float max_range = m_caster->GetSpellMaxRangeForTarget(target, m_spellInfo);
    float min_range = m_caster->GetSpellMinRangeForTarget(target, m_spellInfo);

    if (Player* modOwner = m_caster->GetSpellModOwner())
        modOwner->ApplySpellMod(m_spellInfo->Id, SPELLMOD_RANGE, max_range, this);

    // If the cast is done there is a distance tolerance (around 3m). 
    // Verified on WoD Patch 6.2.3
    if (!strict)
        max_range += 3.00f;

    if (target && target != m_caster)
    {
        if (range_type == SPELL_RANGE_MELEE)
        {
            // Because of lag, we can not check too strictly here.
            if (!m_caster->IsWithinMeleeRange(target, max_range))
                return !(_triggeredCastFlags & TRIGGERED_DONT_REPORT_CAST_ERROR) ? SPELL_FAILED_OUT_OF_RANGE : SPELL_FAILED_DONT_REPORT;
        }
        else if (!m_caster->IsWithinCombatRange(target, max_range))
            return !(_triggeredCastFlags & TRIGGERED_DONT_REPORT_CAST_ERROR) ? SPELL_FAILED_OUT_OF_RANGE : SPELL_FAILED_DONT_REPORT; //0x5A;

        if (range_type == SPELL_RANGE_RANGED)
        {
            if (m_caster->IsWithinMeleeRange(target) && target->GetTypeId() == TYPEID_PLAYER)
                return !(_triggeredCastFlags & TRIGGERED_DONT_REPORT_CAST_ERROR) ? SPELL_FAILED_TOO_CLOSE : SPELL_FAILED_DONT_REPORT;
        }
        else if (min_range && m_caster->IsWithinCombatRange(target, min_range)) // skip this check if min_range = 0
            return !(_triggeredCastFlags & TRIGGERED_DONT_REPORT_CAST_ERROR) ? SPELL_FAILED_TOO_CLOSE : SPELL_FAILED_DONT_REPORT;

        if (m_caster->GetTypeId() == TYPEID_PLAYER &&
            (m_spellInfo->FacingCasterFlags & SPELL_FACING_FLAG_INFRONT) && !m_caster->HasInArc(static_cast<float>(M_PI), target))
            return !(_triggeredCastFlags & TRIGGERED_DONT_REPORT_CAST_ERROR) ? SPELL_FAILED_UNIT_NOT_INFRONT : SPELL_FAILED_DONT_REPORT;

        if (!(_triggeredCastFlags & TRIGGERED_IGNORE_AURA_INTERFERE_TARGETING) && m_caster->IsVisionObscured(target, range_type != SPELL_RANGE_MELEE))
            return !(_triggeredCastFlags & TRIGGERED_DONT_REPORT_CAST_ERROR) ? SPELL_FAILED_VISION_OBSCURED : SPELL_FAILED_DONT_REPORT;
    }

    if (m_targets.HasDst() && !m_targets.HasTraj())
    {
        if (!m_caster->IsWithinDist3d(m_targets.GetDstPos(), max_range))
            return SPELL_FAILED_OUT_OF_RANGE;
        if (min_range && m_caster->IsWithinDist3d(m_targets.GetDstPos(), min_range))
            return SPELL_FAILED_TOO_CLOSE;
    }

    return SPELL_CAST_OK;
}

SpellCastResult Spell::CheckPower()
{
    // item cast not used power
    if (m_CastItem)
        return SPELL_CAST_OK;

    // health as power used - need check health amount
    if (m_spellInfo->PowerType == POWER_HEALTH)
    {
        if (int32(m_caster->GetHealth()) <= m_powerCost)
            return SPELL_FAILED_CASTER_AURASTATE;
        return SPELL_CAST_OK;
    }
    // Check valid power type
    if (m_spellInfo->PowerType >= MAX_POWERS)
    {
        TC_LOG_ERROR("spells", "Spell::CheckPower: Unknown power type '%d'", m_spellInfo->PowerType);
        return SPELL_FAILED_UNKNOWN;
    }

    //check rune cost only if a spell has PowerType == POWER_RUNES
    if (m_spellInfo->PowerType == POWER_RUNES)
    {
        SpellCastResult failReason = CheckRuneCost(m_spellInfo->RuneCostID);
        if (failReason != SPELL_CAST_OK)
            return failReason;
    }

    bool skipPowerCheck = false;
    // Dark simulacrum should allows us to cast any spell regardless of the power cost
    // Maybe this should be implemented generally for all SPELL_AURA_OVERRIDES
    if (AuraEffect* simulacrum = m_caster->GetAuraEffect(SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS_2, SPELLFAMILY_DEATHKNIGHT, 2206, EFFECT_0))
        skipPowerCheck = simulacrum->GetAmount() == m_spellInfo->Id;

    // Check power amount
    Powers powerType = Powers(m_spellInfo->PowerType);
    if (!skipPowerCheck && int32(m_caster->GetPower(powerType)) < m_powerCost)
        return SPELL_FAILED_NO_POWER;
    else
        return SPELL_CAST_OK;
}

SpellCastResult Spell::CheckItems()
{
    Player* p_caster = nullptr;

    if (m_caster->GetTypeId() != TYPEID_PLAYER)  
        if (m_caster->IsVehicle())      
            if (Unit* charmer = m_caster->GetCharmer())
                if (Player* Pcharmer = charmer->ToPlayer())
                    p_caster = Pcharmer;
         
    if (p_caster == nullptr)
    {
        if (m_caster->GetTypeId() != TYPEID_PLAYER)
            return SPELL_CAST_OK;
        else p_caster = (Player*)m_caster;
    }
       
    if (!m_CastItem)
    {
        if (m_castItemGUID)
            return SPELL_FAILED_ITEM_NOT_READY;
    }
    else
    {
        uint32 itemid = m_CastItem->GetEntry();
        if (!p_caster->HasItemCount(itemid))
            return SPELL_FAILED_ITEM_NOT_READY;

        ItemTemplate const* proto = m_CastItem->GetTemplate();
        if (!proto)
            return SPELL_FAILED_ITEM_NOT_READY;

        for (int i = 0; i < MAX_ITEM_SPELLS; ++i)
            if (proto->Spells[i].SpellCharges)
                if (m_CastItem->GetSpellCharges(i) == 0)
                    return SPELL_FAILED_NO_CHARGES_REMAIN;

        // consumable cast item checks
        if (proto->Class == ITEM_CLASS_CONSUMABLE && m_targets.GetUnitTarget())
        {
            // such items should only fail if there is no suitable effect at all - see Rejuvenation Potions for example
            SpellCastResult failReason = SPELL_CAST_OK;
            for (int i = 0; i < MAX_SPELL_EFFECTS; i++)
            {
                    // skip check, pet not required like checks, and for TARGET_UNIT_PET m_targets.GetUnitTarget() is not the real target but the caster
                    if (m_spellInfo->Effects[i].TargetA.GetTarget() == TARGET_UNIT_PET)
                    continue;

                if (m_spellInfo->Effects[i].Effect == SPELL_EFFECT_HEAL)
                {
                    if (m_targets.GetUnitTarget()->IsFullHealth())
                    {
                        failReason = SPELL_FAILED_ALREADY_AT_FULL_HEALTH;
                        continue;
                    }
                    else
                    {
                        failReason = SPELL_CAST_OK;
                        break;
                    }
                }

                // Mana Potion, Rage Potion, Thistle Tea(Rogue), ...
                if (m_spellInfo->Effects[i].Effect == SPELL_EFFECT_ENERGIZE)
                {
                    if (m_spellInfo->Effects[i].MiscValue < 0 || m_spellInfo->Effects[i].MiscValue >= int8(MAX_POWERS))
                    {
                        failReason = SPELL_FAILED_ALREADY_AT_FULL_POWER;
                        continue;
                    }

                    Powers power = Powers(m_spellInfo->Effects[i].MiscValue);
                    if (m_targets.GetUnitTarget()->GetPower(power) == m_targets.GetUnitTarget()->GetMaxPower(power))
                    {
                        failReason = SPELL_FAILED_ALREADY_AT_FULL_POWER;
                        continue;
                    }
                    else
                    {
                        failReason = SPELL_CAST_OK;
                        break;
                    }
                }
            }
            if (failReason != SPELL_CAST_OK)
                return failReason;
        }
    }

    // check target item
    if (m_targets.GetItemTargetGUID())
    {
        if (m_caster->GetTypeId() != TYPEID_PLAYER)
            return SPELL_FAILED_BAD_TARGETS;

        if (!m_targets.GetItemTarget())
            return SPELL_FAILED_ITEM_GONE;

        if (!m_targets.GetItemTarget()->IsFitToSpellRequirements(m_spellInfo))
            return SPELL_FAILED_EQUIPPED_ITEM_CLASS;
    }
    // if not item target then required item must be equipped
    else
    {
        //if (!(_triggeredCastFlags & TRIGGERED_IGNORE_EQUIPPED_ITEM_REQUIREMENT))
        if (m_caster->GetTypeId() == TYPEID_PLAYER && !m_caster->ToPlayer()->HasItemFitToSpellRequirements(m_spellInfo))
            return SPELL_FAILED_EQUIPPED_ITEM_CLASS;
    }

    // check spell focus object
    if (m_spellInfo->RequiresSpellFocus)
    {
        CellCoord p(Trinity::ComputeCellCoord(m_caster->GetPositionX(), m_caster->GetPositionY()));
        Cell cell(p);

        GameObject* ok = NULL;
        Trinity::GameObjectFocusCheck go_check(m_caster, m_spellInfo->RequiresSpellFocus);
        Trinity::GameObjectSearcher<Trinity::GameObjectFocusCheck> checker(m_caster, ok, go_check);

        TypeContainerVisitor<Trinity::GameObjectSearcher<Trinity::GameObjectFocusCheck>, GridTypeMapContainer > object_checker(checker);
        Map& map = *m_caster->GetMap();
        cell.Visit(p, object_checker, map, *m_caster, m_caster->GetVisibilityRange());

        if (!ok)
            return SPELL_FAILED_REQUIRES_SPELL_FOCUS;

        focusObject = ok;                                   // game object found in range
    }

    // do not take reagents for these item casts
    if (!(m_CastItem && m_CastItem->GetTemplate()->Flags & ITEM_PROTO_FLAG_TRIGGERED_CAST))
    {
        bool checkReagents = !(_triggeredCastFlags & TRIGGERED_IGNORE_POWER_AND_REAGENT_COST) && !p_caster->CanNoReagentCast(m_spellInfo);
        // Not own traded item (in trader trade slot) requires reagents even if triggered spell
        if (!checkReagents)
            if (Item* targetItem = m_targets.GetItemTarget())
                if (targetItem->GetOwnerGUID() != m_caster->GetGUID())
                    checkReagents = true;

        // check reagents (ignore triggered spells with reagents processed by original spell) and special reagent ignore case.
        if (checkReagents)
        {
            for (uint32 i = 0; i < MAX_SPELL_REAGENTS; i++)
            {
                if (m_spellInfo->Reagent[i] <= 0)
                    continue;

                uint32 itemid    = m_spellInfo->Reagent[i];
                uint32 itemcount = m_spellInfo->ReagentCount[i];

                // if CastItem is also spell reagent
                if (m_CastItem && m_CastItem->GetEntry() == itemid)
                {
                    ItemTemplate const* proto = m_CastItem->GetTemplate();
                    if (!proto)
                        return SPELL_FAILED_ITEM_NOT_READY;
                    for (int s=0; s < MAX_ITEM_PROTO_SPELLS; ++s)
                    {
                        // CastItem will be used up and does not count as reagent
                        int32 charges = m_CastItem->GetSpellCharges(s);
                        if (proto->Spells[s].SpellCharges < 0 && abs(charges) < 2)
                        {
                            ++itemcount;
                            break;
                        }
                    }
                }
                if (!p_caster->HasItemCount(itemid, itemcount))
                    return SPELL_FAILED_REAGENTS;
            }
        }

        // check totem-item requirements (items presence in inventory)
        uint32 totems = 2;
        for (int i = 0; i < 2; ++i)
        {
            if (m_spellInfo->Totem[i] != 0)
            {
                if (p_caster->HasItemCount(m_spellInfo->Totem[i]))
                {
                    totems -= 1;
                    continue;
                }
            }else
            totems -= 1;
        }
        if (totems != 0)
            return SPELL_FAILED_TOTEMS;
    }

    // special checks for spell effects
    for (int i = 0; i < MAX_SPELL_EFFECTS; i++)
    {
        switch (m_spellInfo->Effects[i].Effect)
        {
            case SPELL_EFFECT_CREATE_ITEM:
            case SPELL_EFFECT_CREATE_ITEM_2:
            {
                if (!IsTriggered() && m_spellInfo->Effects[i].ItemType)
                {
                    ItemPosCountVec dest;
                    InventoryResult msg = p_caster->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, m_spellInfo->Effects[i].ItemType, 1);
                    if (msg != EQUIP_ERR_OK)
                    {
                        ItemTemplate const* pProto = sObjectMgr->GetItemTemplate(m_spellInfo->Effects[i].ItemType);
                        // TODO: Needs review
                        if (pProto && !(pProto->ItemLimitCategory))
                        {
                            p_caster->SendEquipError(msg, NULL, NULL, m_spellInfo->Effects[i].ItemType);
                            return SPELL_FAILED_DONT_REPORT;
                        }
                        else
                        {
                            if (!(m_spellInfo->SpellFamilyName == SPELLFAMILY_MAGE && (m_spellInfo->SpellFamilyFlags[0] & 0x40000000)))
                                return SPELL_FAILED_TOO_MANY_OF_ITEM;
                            else if (!(p_caster->HasItemCount(m_spellInfo->Effects[i].ItemType)))
                                return SPELL_FAILED_TOO_MANY_OF_ITEM;
                            else
                                p_caster->CastSpell(m_caster, m_spellInfo->Effects[EFFECT_1].CalcValue(), false);        // move this to anywhere
                            return SPELL_FAILED_DONT_REPORT;
                        }
                    }
                }
                break;
            }
            case SPELL_EFFECT_ENCHANT_ITEM:
                if (m_spellInfo->Effects[i].ItemType && m_targets.GetItemTarget()
                    && (m_targets.GetItemTarget()->IsVellum()))
                {
                    // cannot enchant vellum for other player
                    if (m_targets.GetItemTarget()->GetOwner() != m_caster)
                        return SPELL_FAILED_NOT_TRADEABLE;
                    // do not allow to enchant vellum from scroll made by vellum-prevent exploit
                    if (m_CastItem && m_CastItem->GetTemplate()->Flags & ITEM_PROTO_FLAG_TRIGGERED_CAST)
                        return SPELL_FAILED_TOTEM_CATEGORY;
                    ItemPosCountVec dest;
                    InventoryResult msg = p_caster->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, m_spellInfo->Effects[i].ItemType, 1);
                    if (msg != EQUIP_ERR_OK)
                    {
                        p_caster->SendEquipError(msg, NULL, NULL, m_spellInfo->Effects[i].ItemType);
                        return SPELL_FAILED_DONT_REPORT;
                    }
                }
            case SPELL_EFFECT_ENCHANT_ITEM_PRISMATIC:
            {
                Item* targetItem = m_targets.GetItemTarget();
                if (!targetItem)
                    return SPELL_FAILED_ITEM_NOT_FOUND;

                if (targetItem->GetTemplate()->ItemLevel < m_spellInfo->BaseLevel)
                    return SPELL_FAILED_LOWLEVEL;

                bool isItemUsable = false;
                for (uint8 e = 0; e < MAX_ITEM_PROTO_SPELLS; ++e)
                {
                    ItemTemplate const* proto = targetItem->GetTemplate();
                    if (proto->Spells[e].SpellId && (
                        proto->Spells[e].SpellTrigger == ITEM_SPELLTRIGGER_ON_USE ||
                        proto->Spells[e].SpellTrigger == ITEM_SPELLTRIGGER_ON_NO_DELAY_USE))
                    {
                        isItemUsable = true;
                        break;
                    }
                }

                SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(m_spellInfo->Effects[i].MiscValue);
                // do not allow adding usable enchantments to items that have use effect already
                if (pEnchant && isItemUsable)
                    for (uint8 s = 0; s < MAX_ITEM_ENCHANTMENT_EFFECTS; ++s)
                        if (pEnchant->type[s] == ITEM_ENCHANTMENT_TYPE_USE_SPELL)
                            return SPELL_FAILED_ON_USE_ENCHANT;

                // Not allow enchant in trade slot for some enchant type
                if (targetItem->GetOwner() != m_caster)
                {
                    if (!pEnchant)
                        return SPELL_FAILED_ERROR;
                    if (pEnchant->slot & ENCHANTMENT_CAN_SOULBOUND)
                        return SPELL_FAILED_NOT_TRADEABLE;
                }
                break;
            }
            case SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY:
            {
                Item* item = m_targets.GetItemTarget();
                if (!item)
                    return SPELL_FAILED_ITEM_NOT_FOUND;
                // Not allow enchant in trade slot for some enchant type
                if (item->GetOwner() != m_caster)
                {
                    uint32 enchant_id = m_spellInfo->Effects[i].MiscValue;
                    SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
                    if (!pEnchant)
                        return SPELL_FAILED_ERROR;
                    if (pEnchant->slot & ENCHANTMENT_CAN_SOULBOUND)
                        return SPELL_FAILED_NOT_TRADEABLE;
                }
                break;
            }
            case SPELL_EFFECT_ENCHANT_HELD_ITEM:
                // check item existence in effect code (not output errors at offhand hold item effect to main hand for example
                break;
            case SPELL_EFFECT_DISENCHANT:
            {
                if (!m_targets.GetItemTarget())
                    return SPELL_FAILED_CANT_BE_DISENCHANTED;

                // prevent disenchanting in trade slot
                if (m_targets.GetItemTarget()->GetOwnerGUID() != m_caster->GetGUID())
                    return SPELL_FAILED_CANT_BE_DISENCHANTED;

                ItemTemplate const* itemProto = m_targets.GetItemTarget()->GetTemplate();
                if (!itemProto)
                    return SPELL_FAILED_CANT_BE_DISENCHANTED;

                uint32 item_quality = itemProto->Quality;
                // 2.0.x addon: Check player enchanting level against the item disenchanting requirements
                uint32 item_disenchantskilllevel = itemProto->RequiredDisenchantSkill;
                if (item_disenchantskilllevel == uint32(-1))
                    return SPELL_FAILED_CANT_BE_DISENCHANTED;
                if (item_disenchantskilllevel > p_caster->GetSkillValue(SKILL_ENCHANTING))
                    return SPELL_FAILED_LOW_CASTLEVEL;
                if (item_quality > 4 || item_quality < 2)
                    return SPELL_FAILED_CANT_BE_DISENCHANTED;
                if (itemProto->Class != ITEM_CLASS_WEAPON && itemProto->Class != ITEM_CLASS_ARMOR)
                    return SPELL_FAILED_CANT_BE_DISENCHANTED;
                if (!itemProto->DisenchantID)
                    return SPELL_FAILED_CANT_BE_DISENCHANTED;
                break;
            }
            case SPELL_EFFECT_PROSPECTING:
            {
                if (!m_targets.GetItemTarget())
                    return SPELL_FAILED_CANT_BE_PROSPECTED;
                //ensure item is a prospectable ore
                if (!(m_targets.GetItemTarget()->GetTemplate()->Flags & ITEM_PROTO_FLAG_PROSPECTABLE))
                    return SPELL_FAILED_CANT_BE_PROSPECTED;
                //prevent prospecting in trade slot
                if (m_targets.GetItemTarget()->GetOwnerGUID() != m_caster->GetGUID())
                    return SPELL_FAILED_CANT_BE_PROSPECTED;
                //Check for enough skill in jewelcrafting
                uint32 item_prospectingskilllevel = m_targets.GetItemTarget()->GetTemplate()->RequiredSkillRank;
                if (item_prospectingskilllevel >p_caster->GetSkillValue(SKILL_JEWELCRAFTING))
                    return SPELL_FAILED_LOW_CASTLEVEL;
                //make sure the player has the required ores in inventory
                if (m_targets.GetItemTarget()->GetCount() < 5)
                    return SPELL_FAILED_NEED_MORE_ITEMS;

                if (!LootTemplates_Prospecting.HaveLootFor(m_targets.GetItemTargetEntry()))
                    return SPELL_FAILED_CANT_BE_PROSPECTED;

                break;
            }
            case SPELL_EFFECT_MILLING:
            {
                if (!m_targets.GetItemTarget())
                    return SPELL_FAILED_CANT_BE_MILLED;
                //ensure item is a millable herb
                if (!(m_targets.GetItemTarget()->GetTemplate()->Flags & ITEM_PROTO_FLAG_MILLABLE))
                    return SPELL_FAILED_CANT_BE_MILLED;
                //prevent milling in trade slot
                if (m_targets.GetItemTarget()->GetOwnerGUID() != m_caster->GetGUID())
                    return SPELL_FAILED_CANT_BE_MILLED;
                //Check for enough skill in inscription
                uint32 item_millingskilllevel = m_targets.GetItemTarget()->GetTemplate()->RequiredSkillRank;
                if (item_millingskilllevel >p_caster->GetSkillValue(SKILL_INSCRIPTION))
                    return SPELL_FAILED_LOW_CASTLEVEL;
                //make sure the player has the required herbs in inventory
                if (m_targets.GetItemTarget()->GetCount() < 5)
                    return SPELL_FAILED_NEED_MORE_ITEMS;

                if (!LootTemplates_Milling.HaveLootFor(m_targets.GetItemTargetEntry()))
                    return SPELL_FAILED_CANT_BE_MILLED;

                break;
            }
            case SPELL_EFFECT_WEAPON_DAMAGE:
            case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    return SPELL_FAILED_TARGET_NOT_PLAYER;

                if (m_attackType != RANGED_ATTACK)
                    break;

                Item* pItem = m_caster->ToPlayer()->GetWeaponForAttack(m_attackType);
                if (!pItem || pItem->IsBroken())
                    return SPELL_FAILED_EQUIPPED_ITEM;

                switch (pItem->GetTemplate()->SubClass)
                {
                    case ITEM_SUBCLASS_WEAPON_THROWN:
                    {
                        uint32 ammo = pItem->GetEntry();
                        if (!m_caster->ToPlayer()->HasItemCount(ammo))
                            return SPELL_FAILED_NO_AMMO;
                    };
                    break;
                    case ITEM_SUBCLASS_WEAPON_GUN:
                    case ITEM_SUBCLASS_WEAPON_BOW:
                    case ITEM_SUBCLASS_WEAPON_CROSSBOW:
                    case ITEM_SUBCLASS_WEAPON_WAND:
                        break;
                    default:
                        break;
                }
                break;
            }
            case SPELL_EFFECT_CREATE_MANA_GEM:
            {
                 uint32 item_id = m_spellInfo->Effects[i].ItemType;
                 ItemTemplate const* pProto = sObjectMgr->GetItemTemplate(item_id);

                 if (!pProto)
                     return SPELL_FAILED_ITEM_AT_MAX_CHARGES;

                 if (Item* pitem = p_caster->GetItemByEntry(item_id))
                 {
                     for (int x = 0; x < MAX_ITEM_PROTO_SPELLS; ++x)
                         if (pProto->Spells[x].SpellCharges != 0 && pitem->GetSpellCharges(x) == pProto->Spells[x].SpellCharges)
                             return SPELL_FAILED_ITEM_AT_MAX_CHARGES;
                 }
                 break;
            }
            default:
                break;
        }
    }

    // check weapon presence in slots for main/offhand weapons
    if (/*!(_triggeredCastFlags & TRIGGERED_IGNORE_EQUIPPED_ITEM_REQUIREMENT) && */
        m_spellInfo->EquippedItemClass >=0)
    {
        // main hand weapon required
        if (m_spellInfo->AttributesEx3 & SPELL_ATTR3_MAIN_HAND)
        {
            Item* item = m_caster->ToPlayer()->GetWeaponForAttack(BASE_ATTACK);

            // skip spell if no weapon in slot or broken
            if (!item || item->IsBroken())
                return (_triggeredCastFlags & TRIGGERED_DONT_REPORT_CAST_ERROR) ? SPELL_FAILED_DONT_REPORT : SPELL_FAILED_EQUIPPED_ITEM_CLASS;

            // skip spell if weapon not fit to triggered spell
            if (!item->IsFitToSpellRequirements(m_spellInfo))
                return (_triggeredCastFlags & TRIGGERED_DONT_REPORT_CAST_ERROR) ? SPELL_FAILED_DONT_REPORT : SPELL_FAILED_EQUIPPED_ITEM_CLASS;
        }

        // offhand hand weapon required
        if (m_spellInfo->AttributesEx3 & SPELL_ATTR3_REQ_OFFHAND)
        {
            Item* item = m_caster->ToPlayer()->GetWeaponForAttack(OFF_ATTACK);

            // skip spell if no weapon in slot or broken
            if (!item || item->IsBroken())
                return (_triggeredCastFlags & TRIGGERED_DONT_REPORT_CAST_ERROR) ? SPELL_FAILED_DONT_REPORT : SPELL_FAILED_EQUIPPED_ITEM_CLASS;

            // skip spell if weapon not fit to triggered spell
            if (!item->IsFitToSpellRequirements(m_spellInfo))
                return (_triggeredCastFlags & TRIGGERED_DONT_REPORT_CAST_ERROR) ? SPELL_FAILED_DONT_REPORT : SPELL_FAILED_EQUIPPED_ITEM_CLASS;
        }
    }

    return SPELL_CAST_OK;
}

void Spell::Delayed() // only called in DealDamage()
{
    if (!m_caster)// || m_caster->GetTypeId() != TYPEID_PLAYER)
        return;

    //if (m_spellState == SPELL_STATE_DELAYED)
    //    return;                                             // spell is active and can't be time-backed

    if (isDelayableNoMore())                                 // Spells may only be delayed twice
        return;

    // spells not loosing casting time (slam, dynamites, bombs..)
    //if (!(m_spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_DAMAGE))
    //    return;

    //check pushback reduce
    int32 delaytime = 500;                                  // spellcasting delay is normally 500ms
    int32 delayReduce = 100;                                // must be initialized to 100 for percent modifiers
    m_caster->ToPlayer()->ApplySpellMod(m_spellInfo->Id, SPELLMOD_NOT_LOSE_CASTING_TIME, delayReduce, this);
    delayReduce += m_caster->GetTotalAuraModifier(SPELL_AURA_REDUCE_PUSHBACK) - 100;
    if (delayReduce >= 100)
        return;

    AddPct(delaytime, -delayReduce);

    if (m_timer + delaytime > m_casttime)
    {
        delaytime = m_casttime - m_timer;
        m_timer = m_casttime;
    }
    else
        m_timer += delaytime;

    if (delaytime <= int32(GetMSTimeDiffToNow(GetStartCastTime())))
        m_delaytime += delaytime;
    else
        m_delaytime += GetMSTimeDiffToNow(GetStartCastTime());

    TC_LOG_INFO("spells", "Spell %u partially interrupted for (%d) ms at damage", m_spellInfo->Id, delaytime);

    WorldPacket data(SMSG_SPELL_DELAYED, 8+4);
    data.append(m_caster->GetPackGUID());
    data << uint32(delaytime);

    m_caster->SendMessageToSet(&data, true);
}

void Spell::DelayedChannel()
{
    if (!m_caster || m_caster->GetTypeId() != TYPEID_PLAYER || getState() != SPELL_STATE_CASTING)
        return;

    if (isDelayableNoMore())                                    // Spells may only be delayed twice
        return;

    //check pushback reduce
    int32 delaytime = CalculatePct(m_spellInfo->GetDuration(), 25); // channeling delay is normally 25% of its time per hit
    int32 delayReduce = 100;                                    // must be initialized to 100 for percent modifiers
    m_caster->ToPlayer()->ApplySpellMod(m_spellInfo->Id, SPELLMOD_NOT_LOSE_CASTING_TIME, delayReduce, this);
    delayReduce += m_caster->GetTotalAuraModifier(SPELL_AURA_REDUCE_PUSHBACK) - 100;
    if (delayReduce >= 100)
        return;

    AddPct(delaytime, -delayReduce);

    if (m_timer <= delaytime)
    {
        delaytime = m_timer;
        m_timer = 0;
    }
    else
        m_timer -= delaytime;

    TC_LOG_DEBUG("spells", "Spell %u partially interrupted for %i ms, new duration: %u ms", m_spellInfo->Id, delaytime, m_timer);

    for (auto ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
        if ((*ihit).missCondition == SPELL_MISS_NONE)
            if (Unit* unit = (m_caster->GetGUID() == ihit->targetGUID) ? m_caster : ObjectAccessor::GetUnit(*m_caster, ihit->targetGUID))
                unit->DelayOwnedAuras(m_spellInfo->Id, m_originalCasterGUID, delaytime);

    // partially interrupt persistent area auras
    if (DynamicObject* dynObj = m_caster->GetDynObject(m_spellInfo->Id))
        dynObj->Delay(delaytime);

    if (delaytime <= int32(GetMSTimeDiffToNow(GetStartCastTime())))
        m_delaytime += delaytime;
    else
        m_delaytime += GetMSTimeDiffToNow(GetStartCastTime());

    SendChannelUpdate(m_timer);
}

bool Spell::UpdatePointers()
{
    if (m_originalCasterGUID == m_caster->GetGUID())
        m_originalCaster = m_caster;
    else
    {
        m_originalCaster = ObjectAccessor::GetUnit(*m_caster, m_originalCasterGUID);
        if (m_originalCaster && !m_originalCaster->IsInWorld())
            m_originalCaster = NULL;
    }

    if (m_castItemGUID && m_caster->GetTypeId() == TYPEID_PLAYER)
    {        
         if (auto p = m_caster->ToPlayer())
             if (auto found_item = p->GetItemByGuid(m_castItemGUID))
                 m_CastItem = found_item;// cast item not found, somehow the item is no longer where we expected
             else return false;
         else return false;
    }

    m_targets.Update(m_caster);

    // further actions done only for dest targets
    if (!m_targets.HasDst())
        return true;

    // cache last transport
    WorldObject* transport = NULL;

    // update effect destinations (in case of moved transport dest target)
    for (uint8 effIndex = 0; effIndex < MAX_SPELL_EFFECTS; ++effIndex)
    {
        SpellDestination& dest = m_destTargets[effIndex];
        if (!dest._transportGUID)
            continue;

        if (!transport || transport->GetGUID() != dest._transportGUID)
            transport = ObjectAccessor::GetWorldObject(*m_caster, dest._transportGUID);

        if (transport)
        {
            dest._position.Relocate(transport);
            dest._position.RelocateOffset(dest._transportOffset);
        }
    }

    return true;
}

CurrentSpellTypes Spell::GetCurrentContainer() const
{
    if (IsNextMeleeSwingSpell())
        return(CURRENT_MELEE_SPELL);
    else if (IsAutoRepeat())
        return(CURRENT_AUTOREPEAT_SPELL);
    else if (m_spellInfo->IsChanneled())
        return(CURRENT_CHANNELED_SPELL);
    else
        return(CURRENT_GENERIC_SPELL);
}

bool Spell::CheckEffectTarget(Unit const* target, uint32 eff) const
{
    switch (m_spellInfo->Effects[eff].ApplyAuraName)
    {
        case SPELL_AURA_MOD_POSSESS:
        case SPELL_AURA_MOD_CHARM:
        case SPELL_AURA_MOD_POSSESS_PET:
        case SPELL_AURA_AOE_CHARM:
            if (target->GetTypeId() == TYPEID_UNIT && target->IsVehicle())
                return false;
            if (target->GetTypeId() == TYPEID_UNIT && target->IsMounted())
                return false;
            if (target->GetCharmerGUID())
                return false;
            if (int32 damage = CalculateDamage(eff, target))
                if ((int32)target->getLevel() > damage)
                    return false;
            break;
        default:
            break;
    }

    // A redirected spell should always hit
    if (_redirected)
        return true;

    bool alreadyChecked = false;
    // Spells like Shadowfury
    if (m_targets.HasDst() && !m_targets.HasTraj())
    {
        alreadyChecked = true;
        if (target->IsWithinLOS(m_targets.GetDstPos()->GetPositionX(), m_targets.GetDstPos()->GetPositionY(), m_targets.GetDstPos()->GetPositionZ(), m_spellInfo))
            return true;
    }

    if (!alreadyChecked)
    {
        if (IsTriggered() && !m_spellInfo->Effects[eff].TriggeredNeedsLosChecks())
            return true;

        if (m_spellInfo->AttributesEx2 & SPELL_ATTR2_CAN_TARGET_NOT_IN_LOS)
            return true;
    }

    // todo: shit below shouldn't be here, but it's temporary
    //Check targets for LOS visibility (except spells without range limitations)
    switch (m_spellInfo->Effects[eff].Effect)
    {
        case SPELL_EFFECT_RESURRECT:
        case SPELL_EFFECT_RESURRECT_NEW:
            // player far away, maybe his corpse near?
            if (target != m_caster && !target->IsWithinLOSInMap(m_caster, m_spellInfo))
            {
                if (!m_targets.GetCorpseTargetGUID())
                    return false;

                Corpse* corpse = ObjectAccessor::GetCorpse(*m_caster, m_targets.GetCorpseTargetGUID());
                if (!corpse)
                    return false;

                if (target->GetGUID() != corpse->GetOwnerGUID())
                    return false;

                /*if (!corpse->IsWithinLOSInMap(m_caster))
                    return false;*/
            }

            // all ok by some way or another, skip normal check
            break;
        default:                                            // normal case
            // Get GO cast coordinates if original caster -> GO
            WorldObject* caster = NULL;
            if (IS_GAMEOBJECT_GUID(m_originalCasterGUID))
                caster = m_caster->GetMap()->GetGameObject(m_originalCasterGUID);
            if (!caster)
                caster = m_caster;
            // Glyph of Concussive Shot
            if (m_spellInfo->Id == 5116)
            {
                if (eff == EFFECT_1 && !m_caster->HasAura(56851))
                    return false;
            }
            
            // Should not just return false, only if its AoE spells. Return false will "eat" spells
            if (target != m_caster && !target->IsWithinLOSInMap(caster, m_spellInfo))
            {
                for (uint32 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                {
                    if (m_spellInfo->Effects[i].IsTargetingArea())
                        return false;
                }
            }
            break;
    }

    return true;
}

float Spell::GetCastTimeRemaining(bool force_zero_minimum)
{
    auto info = GetSpellInfo();

    float duration{ info->IsChanneled() ? float(info->GetDuration()) : float(GetCastTime()) };
    float pct_done{ GetCastCompletionPercent() };
    float time_remaining{ (duration - (duration * (pct_done * .01f))) };

    if (force_zero_minimum)
    if (time_remaining < 0.f)
        time_remaining = 0.f;

    //TC_LOG_ERROR("sql.sql", "Spell %u is %u percent complete of %u mS is %u mS remaining", info->Id, uint32(pct_done), duration, uint32(time_remaining));


    return time_remaining;
}

float Spell::GetCastCompletionPercent()
{
    float castPCT{ 0.f };
    if (GetSpellInfo()->IsChanneled())
    {
        castPCT = float(100.f - (100.f * float(GetSpellInfo()->GetDuration() - GetMSTimeDiffToNow(GetStartCastTime()) - GetDelaytime()) / float(GetSpellInfo()->GetDuration())));
    }
    else
    {
        //TC_LOG_ERROR("spells", "cast is normal, times: %u / %u", (GetMSTimeDiffToNow(inputspell->GetStartCastTime()) - inputspell->GetDelaytime()), int32(inputspell->GetCastTime()));
        if (float(GetMSTimeDiffToNow(GetStartCastTime())) > float(GetDelaytime()))//This spell's total casting time is higher than the time it's been reduced by pushback.
        {
            //TC_LOG_ERROR("spells", "cast should work with normal calculation, times: %u - %u / %u", GetMSTimeDiffToNow(inputspell->GetStartCastTime()), inputspell->GetDelaytime(), int32(inputspell->GetCastTime()));
            castPCT =
                float(
                    100.f * (GetMSTimeDiffToNow(GetStartCastTime()) - GetDelaytime())   //calculate normally
                    /
                    float(GetCastTime())
                    );
        }
        else
        {
            //TC_LOG_ERROR("spells", "Spell::GetCastCompletionPercent: This formula would have produced a negative numerator: (%u - %u) / %u\n spell %u interrupted by %u", uint32(GetMSTimeDiffToNow(GetStartCastTime())), GetDelaytime(), int32(GetCastTime()), (GetSpellInfo()->Id), GetSpellInfo()->Id);
            castPCT = float(0.f);
        }
    }
    return castPCT;
}

bool Spell::IsNextMeleeSwingSpell() const
{
    return m_spellInfo->Attributes & SPELL_ATTR0_ON_NEXT_SWING;
}

bool Spell::IsAutoActionResetSpell() const
{
    // TODO: changed SPELL_INTERRUPT_FLAG_AUTOATTACK -> SPELL_INTERRUPT_FLAG_INTERRUPT to fix compile - is this check correct at all?
    return !IsTriggered() && (m_spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_INTERRUPT);
}

bool Spell::IsNeedSendToClient() const
{
    return m_spellInfo->SpellVisual[0] || m_spellInfo->SpellVisual[1] || m_spellInfo->IsChanneled() ||
        (m_spellInfo->AttributesEx8 & SPELL_ATTR8_AURA_SEND_AMOUNT) || m_spellValue->Speed > 0.0f || (!m_triggeredByAuraSpell && !IsTriggered());
}

bool Spell::HaveTargetsForEffect(uint8 effect) const
{
    for (auto itr = m_UniqueTargetInfo.begin(); itr != m_UniqueTargetInfo.end(); ++itr)
        if (itr->effectMask & (1 << effect))
            return true;

    for (auto itr = m_UniqueGOTargetInfo.begin(); itr != m_UniqueGOTargetInfo.end(); ++itr)
        if (itr->effectMask & (1 << effect))
            return true;

    for (auto itr = m_UniqueItemInfo.begin(); itr != m_UniqueItemInfo.end(); ++itr)
        if (itr->effectMask & (1 << effect))
            return true;

    return false;
}

SpellEvent::SpellEvent(Spell* spell) : BasicEvent()
{
    m_Spell = spell;
}

SpellEvent::~SpellEvent()
{
    if (m_Spell->getState() != SPELL_STATE_FINISHED)
        m_Spell->cancel();

    if (m_Spell->IsDeletable())
    {
        delete m_Spell;
    }
    else
    {
        TC_LOG_ERROR("spells", "~SpellEvent: %s %u tried to delete non-deletable spell %u. Was not deleted, causes memory leak.",
            (m_Spell->GetCaster()->GetTypeId() == TYPEID_PLAYER ? "Player" : "Creature"), m_Spell->GetCaster()->GetGUIDLow(), m_Spell->m_spellInfo->Id);
        ASSERT(false);
    }
}

bool SpellEvent::Execute(uint64 e_time, uint32 p_time)
{
    // update spell if it is not finished
    if (m_Spell->getState() != SPELL_STATE_FINISHED)
        m_Spell->update(p_time);

    // check spell state to process
    switch (m_Spell->getState())
    {
        case SPELL_STATE_FINISHED:
        {
            // spell was finished, check deletable state
            if (m_Spell->IsDeletable())
            {
                // check, if we do have unfinished triggered spells
                return true;                                // spell is deletable, finish event
            }
            // event will be re-added automatically at the end of routine)
        } break;

        case SPELL_STATE_DELAYED:
        {
            // first, check, if we have just started
            if (m_Spell->GetDelayStart() != 0)
            {
                // no, we aren't, do the typical update
                // check, if we have channeled spell on our hands
                /*
                if (m_Spell->m_spellInfo->IsChanneled())
                {
                    // evented channeled spell is processed separately, casted once after delay, and not destroyed till finish
                    // check, if we have casting anything else except this channeled spell and autorepeat
                    if (m_Spell->GetCaster()->IsNonMeleeSpellCasted(false, true, true))
                    {
                        // another non-melee non-delayed spell is casted now, abort
                        m_Spell->cancel();
                    }
                    else
                    {
                        // Set last not triggered spell for apply spellmods
                        ((Player*)m_Spell->GetCaster())->SetSpellModTakingSpell(m_Spell, true);
                        // do the action (pass spell to channeling state)
                        m_Spell->handle_immediate();

                        // And remove after effect handling
                        ((Player*)m_Spell->GetCaster())->SetSpellModTakingSpell(m_Spell, false);
                    }
                    // event will be re-added automatically at the end of routine)
                }
                else
                */
                {
                    // run the spell handler and think about what we can do next
                    uint64 t_offset = e_time - m_Spell->GetDelayStart();
                    uint64 n_offset = m_Spell->handle_delayed(t_offset);
                    if (n_offset)
                    {
                        // re-add us to the queue
                        m_Spell->GetCaster()->m_Events.AddEvent(this, m_Spell->GetDelayStart() + n_offset, false);
                        return false;                       // event not complete
                    }
                    // event complete
                    // finish update event will be re-added automatically at the end of routine)
                }
            }
            else
            {
                // delaying had just started, record the moment
                m_Spell->SetDelayStart(e_time);
                // re-plan the event for the delay moment
                m_Spell->GetCaster()->m_Events.AddEvent(this, e_time + m_Spell->GetDelayMoment(), false);
                return false;                               // event not complete
            }
        } break;

        default:
        {
            // all other states
            // event will be re-added automatically at the end of routine)
        } break;
    }

    // spell processing not complete, plan event on the next update interval
    m_Spell->GetCaster()->m_Events.AddEvent(this, e_time + 1, false);
    return false;                                           // event not complete
}

void SpellEvent::Abort(uint64 /*e_time*/)
{
    // oops, the spell we try to do is aborted
    if (m_Spell->getState() != SPELL_STATE_FINISHED)
        m_Spell->cancel();
}

bool SpellEvent::IsDeletable() const
{
    return m_Spell->IsDeletable();
}

bool Spell::IsValidDeadOrAliveTarget(Unit const* target) const
{
    if (target->isAlive())
        return !m_spellInfo->IsRequiringDeadTarget();
    if (m_spellInfo->IsAllowingDeadTarget())
        return true;
    return false;
}

void Spell::HandleLaunchPhase()
{
    // handle effects with SPELL_EFFECT_HANDLE_LAUNCH mode
    for (uint32 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        // don't do anything for empty effect
        if (!m_spellInfo->Effects[i].IsEffect())
            continue;

        HandleEffects(NULL, NULL, NULL, i, SPELL_EFFECT_HANDLE_LAUNCH);
    }

    float multiplier[MAX_SPELL_EFFECTS];
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (m_applyMultiplierMask & (1 << i))
            multiplier[i] = m_spellInfo->Effects[i].CalcDamageMultiplier(m_originalCaster, this);

    for (auto ihit= m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
    {
        TargetInfo& target = *ihit;

        uint32 mask = target.effectMask;
        if (!mask)
            continue;

        DoAllEffectOnLaunchTarget(target, multiplier);
    }
}

void Spell::DoAllEffectOnLaunchTarget(TargetInfo& targetInfo, float* multiplier)
{
    Unit* unit = NULL;
    // In case spell hit target, do all effect on that target
    if (targetInfo.missCondition == SPELL_MISS_NONE || targetInfo.missCondition == SPELL_MISS_REFLECT)
        unit = m_caster->GetGUID() == targetInfo.targetGUID ? m_caster : ObjectAccessor::GetUnit(*m_caster, targetInfo.targetGUID);
    if (!unit)
        return;

    for (uint32 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (targetInfo.effectMask & (1<<i))
        {
            m_damage = 0;
            m_healing = 0;

            HandleEffects(unit, NULL, NULL, i, SPELL_EFFECT_HANDLE_LAUNCH_TARGET);

            if (m_damage > 0)
            {
                if (m_spellInfo->Effects[i].IsTargetingArea())
                {
                    m_damage = int32(float(m_damage) * unit->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_AOE_DAMAGE_AVOIDANCE, m_spellInfo->SchoolMask));
                    if (m_caster->GetTypeId() == TYPEID_UNIT)
                        m_damage = int32(float(m_damage) * unit->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_CREATURE_AOE_DAMAGE_AVOIDANCE, m_spellInfo->SchoolMask));

                    if (m_caster->GetTypeId() == TYPEID_PLAYER)
                    {
                        uint32 targetAmount = m_UniqueTargetInfo.size();
                        if (targetAmount > 10)
                            m_damage = m_damage * 10/targetAmount;
                    }
                }
            }

            if (m_applyMultiplierMask & (1 << i))
            {
                m_damage = int32(m_damage * m_damageMultipliers[i]);
                m_damageMultipliers[i] *= multiplier[i];
            }
            targetInfo.damage += m_damage;
        }
    }

   
    // totem's inherit owner crit chance and dancing rune weapon
    Unit* caster = m_caster;
    if (m_caster->isTotem() || m_caster->GetEntry() == 27893)
    {
        if (Unit* owner = m_caster->GetOwner())
            caster = owner;
    }
    else if (m_originalCaster)
        caster = m_originalCaster;

    targetInfo.crit = caster->isSpellCrit(unit, m_spellInfo, m_spellSchoolMask, m_attackType, this);

    //Harmful crits
    if (targetInfo.crit)
        setIsCriticalStrike(2);
    else
        setIsCriticalStrike(1);
}

SpellCastResult Spell::CanOpenLock(uint32 effIndex, uint32 lockId, SkillType& skillId, int32& reqSkillValue, int32& skillValue)
{
    if (!lockId)                                             // possible case for GO and maybe for items.
        return SPELL_CAST_OK;

    // Get LockInfo
    LockEntry const* lockInfo = sLockStore.LookupEntry(lockId);

    if (!lockInfo)
        return SPELL_FAILED_BAD_TARGETS;

    bool reqKey = false;                                    // some locks not have reqs

    for (int j = 0; j < MAX_LOCK_CASE; ++j)
    {
        switch (lockInfo->Type[j])
        {
            // check key item (many fit cases can be)
            case LOCK_KEY_ITEM:
                if (lockInfo->Index[j] && m_CastItem && m_CastItem->GetEntry() == lockInfo->Index[j])
                    return SPELL_CAST_OK;
                reqKey = true;
                break;
                // check key skill (only single first fit case can be)
            case LOCK_KEY_SKILL:
            {
                reqKey = true;

                // wrong locktype, skip
                if (uint32(m_spellInfo->Effects[effIndex].MiscValue) != lockInfo->Index[j])
                    continue;

                skillId = SkillByLockType(LockType(lockInfo->Index[j]));

                if (skillId != SKILL_NONE)
                {
                    reqSkillValue = lockInfo->Skill[j];

                    // castitem check: rogue using skeleton keys. the skill values should not be added in this case.
                    skillValue = m_CastItem || m_caster->GetTypeId()!= TYPEID_PLAYER ?
                        0 : m_caster->ToPlayer()->GetSkillValue(skillId);

                    // skill bonus provided by casting spell (mostly item spells)
                    // add the effect base points modifier from the spell casted (cheat lock / skeleton key etc.)
                    if (m_spellInfo->Effects[effIndex].TargetA.GetTarget() == TARGET_GAMEOBJECT_ITEM_TARGET || m_spellInfo->Effects[effIndex].TargetB.GetTarget() == TARGET_GAMEOBJECT_ITEM_TARGET)
                        skillValue += m_spellInfo->Effects[effIndex].CalcValue(m_caster);

                    if (skillValue < reqSkillValue)
                        return SPELL_FAILED_LOW_CASTLEVEL;
                }

                return SPELL_CAST_OK;
            }
        }
    }

    if (reqKey)
        return SPELL_FAILED_BAD_TARGETS;

    return SPELL_CAST_OK;
}

void Spell::SetSpellValue(SpellValueMod mod, int32 value)
{
    switch (mod)
    {
        case SPELLVALUE_BASE_POINT0:
            m_spellValue->EffectBasePoints[0] = m_spellInfo->Effects[EFFECT_0].CalcBaseValue(value);
            break;
        case SPELLVALUE_BASE_POINT1:
            m_spellValue->EffectBasePoints[1] = m_spellInfo->Effects[EFFECT_1].CalcBaseValue(value);
            break;
        case SPELLVALUE_BASE_POINT2:
            m_spellValue->EffectBasePoints[2] = m_spellInfo->Effects[EFFECT_2].CalcBaseValue(value);
            break;
        case SPELLVALUE_RADIUS_MOD:
            m_spellValue->RadiusMod = (float)value / 10000.0f;
            break;
        case SPELLVALUE_MAX_TARGETS:
            m_spellValue->MaxAffectedTargets = (uint32)value;
            break;
        case SPELLVALUE_AURA_STACK:
            m_spellValue->AuraStackAmount = uint32(value);
            break;
        case SPELLVALUE_SPEED:
            m_spellValue->Speed = float(value);
            break;
        case SPELLVALUE_DURATION:
            m_spellValue->Duration = value;
            break;
        case SPELLVALUE_CAST_TIME:
            m_spellValue->CastTime = value;
            break;
        case SPELLVALUE_MAXDURATION:
            m_spellValue->MaxDuration = value;
            break;
    }
}

void Spell::PrepareTargetProcessing()
{
    CheckEffectExecuteData();
}

void Spell::FinishTargetProcessing()
{
    SendLogExecute();
}

void Spell::InitEffectExecuteData(uint8 effIndex)
{
    ASSERT(effIndex < MAX_SPELL_EFFECTS);
    if (!m_effectExecuteData[effIndex])
    {
        m_effectExecuteData[effIndex] = new ByteBuffer(0x20);
        // first dword - target counter
        *m_effectExecuteData[effIndex] << uint32(1);
    }
    else
    {
        // increase target counter by one
        uint32 count = (*m_effectExecuteData[effIndex]).read<uint32>(0);
        (*m_effectExecuteData[effIndex]).put<uint32>(0, ++count);
    }
}

void Spell::CheckEffectExecuteData()
{
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        ASSERT(!m_effectExecuteData[i]);
}

void Spell::LoadScripts()
{
    sScriptMgr->CreateSpellScripts(m_spellInfo->Id, m_loadedScripts);
    for (auto itr = m_loadedScripts.begin(); itr != m_loadedScripts.end();)
    {
        if (!(*itr)->_Load(this))
        {
            auto bitr = itr;
            ++itr;
            delete (*bitr);
            m_loadedScripts.erase(bitr);
            continue;
        }
        TC_LOG_DEBUG("spells", "Spell::LoadScripts: Script `%s` for spell `%u` is loaded now", (*itr)->_GetScriptName()->c_str(), m_spellInfo->Id);
        (*itr)->Register();
        ++itr;
    }
}

void Spell::UnloadScripts()
{
    while (!m_loadedScripts.empty())
    {
        auto itr = m_loadedScripts.begin();
        (*itr)->_Unload();
        delete (*itr);
        m_loadedScripts.erase(itr);
    }
}

void Spell::CallScriptBeforeCastHandlers()
{
    for (auto scritr = m_loadedScripts.begin(); scritr != m_loadedScripts.end(); ++scritr)
    {
        (*scritr)->_PrepareScriptCall(SPELL_SCRIPT_HOOK_BEFORE_CAST);
        auto hookItrEnd = (*scritr)->BeforeCast.end(), hookItr = (*scritr)->BeforeCast.begin();
        for (; hookItr != hookItrEnd; ++hookItr)
            (*hookItr).Call(*scritr);

        (*scritr)->_FinishScriptCall();
    }
}

void Spell::CallScriptOnCastHandlers()
{
    for (auto scritr = m_loadedScripts.begin(); scritr != m_loadedScripts.end(); ++scritr)
    {
        (*scritr)->_PrepareScriptCall(SPELL_SCRIPT_HOOK_ON_CAST);
        auto hookItrEnd = (*scritr)->OnCast.end(), hookItr = (*scritr)->OnCast.begin();
        for (; hookItr != hookItrEnd; ++hookItr)
            (*hookItr).Call(*scritr);

        (*scritr)->_FinishScriptCall();
    }
}

void Spell::CallScriptAfterCastHandlers()
{
    for (auto scritr = m_loadedScripts.begin(); scritr != m_loadedScripts.end(); ++scritr)
    {
        (*scritr)->_PrepareScriptCall(SPELL_SCRIPT_HOOK_AFTER_CAST);
        auto hookItrEnd = (*scritr)->AfterCast.end(), hookItr = (*scritr)->AfterCast.begin();
        for (; hookItr != hookItrEnd; ++hookItr)
            (*hookItr).Call(*scritr);

        (*scritr)->_FinishScriptCall();
    }
}

SpellCastResult Spell::CallScriptCheckCastHandlers()
{
    SpellCastResult retVal = SPELL_CAST_OK;
    for (auto scritr = m_loadedScripts.begin(); scritr != m_loadedScripts.end(); ++scritr)
    {
        (*scritr)->_PrepareScriptCall(SPELL_SCRIPT_HOOK_CHECK_CAST);
        auto hookItrEnd = (*scritr)->OnCheckCast.end(), hookItr = (*scritr)->OnCheckCast.begin();
        for (; hookItr != hookItrEnd; ++hookItr)
        {
            SpellCastResult tempResult = (*hookItr).Call(*scritr);
            if (retVal == SPELL_CAST_OK)
                retVal = tempResult;
        }

        (*scritr)->_FinishScriptCall();
    }
    return retVal;
}

void Spell::PrepareScriptHitHandlers()
{
    for (auto scritr = m_loadedScripts.begin(); scritr != m_loadedScripts.end(); ++scritr)
        (*scritr)->_InitHit();
}

bool Spell::CallScriptEffectHandlers(SpellEffIndex effIndex, SpellEffectHandleMode mode)
{
    // execute script effect handler hooks and check if effects was prevented
    bool preventDefault = false;
    for (auto scritr = m_loadedScripts.begin(); scritr != m_loadedScripts.end(); ++scritr)
    {
        std::list<SpellScript::EffectHandler>::iterator effItr, effEndItr;
        SpellScriptHookType hookType;
        switch (mode)
        {
            case SPELL_EFFECT_HANDLE_LAUNCH:
                effItr = (*scritr)->OnEffectLaunch.begin();
                effEndItr = (*scritr)->OnEffectLaunch.end();
                hookType = SPELL_SCRIPT_HOOK_EFFECT_LAUNCH;
                break;
            case SPELL_EFFECT_HANDLE_LAUNCH_TARGET:
                effItr = (*scritr)->OnEffectLaunchTarget.begin();
                effEndItr = (*scritr)->OnEffectLaunchTarget.end();
                hookType = SPELL_SCRIPT_HOOK_EFFECT_LAUNCH_TARGET;
                break;
            case SPELL_EFFECT_HANDLE_HIT:
                effItr = (*scritr)->OnEffectHit.begin();
                effEndItr = (*scritr)->OnEffectHit.end();
                hookType = SPELL_SCRIPT_HOOK_EFFECT_HIT;
                break;
            case SPELL_EFFECT_HANDLE_HIT_TARGET:
                effItr = (*scritr)->OnEffectHitTarget.begin();
                effEndItr = (*scritr)->OnEffectHitTarget.end();
                hookType = SPELL_SCRIPT_HOOK_EFFECT_HIT_TARGET;
                break;
            case SPELL_EFFECT_HANDLE_HIT_TARGET_FIRST:
                return false;
            default:
                ASSERT(false);
                return false;
        }
        (*scritr)->_PrepareScriptCall(hookType);
        for (; effItr != effEndItr; ++effItr)
            // effect execution can be prevented
            if (!(*scritr)->_IsEffectPrevented(effIndex) && (*effItr).IsEffectAffected(m_spellInfo, effIndex))
                (*effItr).Call(*scritr, effIndex);

        if (!preventDefault)
            preventDefault = (*scritr)->_IsDefaultEffectPrevented(effIndex);

        (*scritr)->_FinishScriptCall();
    }
    return preventDefault;
}

void Spell::CallScriptBeforeHitHandlers()
{
    for (auto scritr = m_loadedScripts.begin(); scritr != m_loadedScripts.end(); ++scritr)
    {
        (*scritr)->_PrepareScriptCall(SPELL_SCRIPT_HOOK_BEFORE_HIT);
        std::list<SpellScript::HitHandler>::iterator hookItrEnd = (*scritr)->BeforeHit.end(), hookItr = (*scritr)->BeforeHit.begin();
        for (; hookItr != hookItrEnd; ++hookItr)
            (*hookItr).Call(*scritr);

        (*scritr)->_FinishScriptCall();
    }
}

void Spell::CallScriptOnHitHandlers()
{
    for (auto scritr = m_loadedScripts.begin(); scritr != m_loadedScripts.end(); ++scritr)
    {
        (*scritr)->_PrepareScriptCall(SPELL_SCRIPT_HOOK_HIT);
        std::list<SpellScript::HitHandler>::iterator hookItrEnd = (*scritr)->OnHit.end(), hookItr = (*scritr)->OnHit.begin();
        for (; hookItr != hookItrEnd; ++hookItr)
            (*hookItr).Call(*scritr);

        (*scritr)->_FinishScriptCall();
    }
}

void Spell::CallScriptAfterHitHandlers()
{
    for (auto scritr = m_loadedScripts.begin(); scritr != m_loadedScripts.end(); ++scritr)
    {
        (*scritr)->_PrepareScriptCall(SPELL_SCRIPT_HOOK_AFTER_HIT);
        std::list<SpellScript::HitHandler>::iterator hookItrEnd = (*scritr)->AfterHit.end(), hookItr = (*scritr)->AfterHit.begin();
        for (; hookItr != hookItrEnd; ++hookItr)
            (*hookItr).Call(*scritr);

        (*scritr)->_FinishScriptCall();
    }
}

void Spell::CallScriptDispel()
{
    for (auto scritr = m_loadedScripts.begin(); scritr != m_loadedScripts.end(); ++scritr)
    {
        (*scritr)->_PrepareScriptCall(SPELL_SCRIPT_HOOK_AFTER_SUCCESSFUL_DISPEL);
        std::list<SpellScript::DispelHandler>::iterator hookItrEnd = (*scritr)->OnSuccessfulDispel.end(), hookItr = (*scritr)->OnSuccessfulDispel.begin();
        for (; hookItr != hookItrEnd ; ++hookItr)
            (*hookItr).Call(*scritr);
        (*scritr)->_FinishScriptCall();
    }
}

void Spell::CallScriptObjectAreaTargetSelectHandlers(std::list<WorldObject*>& targets, SpellEffIndex effIndex)
{
    for (auto scritr = m_loadedScripts.begin(); scritr != m_loadedScripts.end(); ++scritr)
    {
        (*scritr)->_PrepareScriptCall(SPELL_SCRIPT_HOOK_OBJECT_AREA_TARGET_SELECT);
        std::list<SpellScript::ObjectAreaTargetSelectHandler>::iterator hookItrEnd = (*scritr)->OnObjectAreaTargetSelect.end(), hookItr = (*scritr)->OnObjectAreaTargetSelect.begin();
        for (; hookItr != hookItrEnd; ++hookItr)
            if ((*hookItr).IsEffectAffected(m_spellInfo, effIndex))
                (*hookItr).Call(*scritr, targets);

        (*scritr)->_FinishScriptCall();
    }
}

void Spell::CallScriptObjectTargetSelectHandlers(WorldObject*& target, SpellEffIndex effIndex)
{
    for (auto scritr = m_loadedScripts.begin(); scritr != m_loadedScripts.end(); ++scritr)
    {
        (*scritr)->_PrepareScriptCall(SPELL_SCRIPT_HOOK_OBJECT_TARGET_SELECT);
        std::list<SpellScript::ObjectTargetSelectHandler>::iterator hookItrEnd = (*scritr)->OnObjectTargetSelect.end(), hookItr = (*scritr)->OnObjectTargetSelect.begin();
        for (; hookItr != hookItrEnd; ++hookItr)
            if ((*hookItr).IsEffectAffected(m_spellInfo, effIndex))
                (*hookItr).Call(*scritr, target);

        (*scritr)->_FinishScriptCall();
    }
}

bool Spell::CheckScriptEffectImplicitTargets(uint32 effIndex, uint32 effIndexToCheck)
{
    // Skip if there are not any script
    if (!m_loadedScripts.size())
        return true;

    for (auto itr = m_loadedScripts.begin(); itr != m_loadedScripts.end(); ++itr)
    {
        std::list<SpellScript::ObjectTargetSelectHandler>::iterator targetSelectHookEnd = (*itr)->OnObjectTargetSelect.end(), targetSelectHookItr = (*itr)->OnObjectTargetSelect.begin();
        for (; targetSelectHookItr != targetSelectHookEnd; ++targetSelectHookItr)
            if (((*targetSelectHookItr).IsEffectAffected(m_spellInfo, effIndex) && !(*targetSelectHookItr).IsEffectAffected(m_spellInfo, effIndexToCheck)) ||
                (!(*targetSelectHookItr).IsEffectAffected(m_spellInfo, effIndex) && (*targetSelectHookItr).IsEffectAffected(m_spellInfo, effIndexToCheck)))
                return false;

        std::list<SpellScript::ObjectAreaTargetSelectHandler>::iterator areaTargetSelectHookEnd = (*itr)->OnObjectAreaTargetSelect.end(), areaTargetSelectHookItr = (*itr)->OnObjectAreaTargetSelect.begin();
        for (; areaTargetSelectHookItr != areaTargetSelectHookEnd; ++areaTargetSelectHookItr)
            if (((*areaTargetSelectHookItr).IsEffectAffected(m_spellInfo, effIndex) && !(*areaTargetSelectHookItr).IsEffectAffected(m_spellInfo, effIndexToCheck)) ||
                (!(*areaTargetSelectHookItr).IsEffectAffected(m_spellInfo, effIndex) && (*areaTargetSelectHookItr).IsEffectAffected(m_spellInfo, effIndexToCheck)))
                return false;
    }
    return true;
}

bool Spell::CanExecuteTriggersOnHit(uint8 effMask, SpellInfo const* triggeredByAura) const
{
    bool only_on_caster = (triggeredByAura && (triggeredByAura->AttributesEx4 & SPELL_ATTR4_PROC_ONLY_ON_CASTER));
    // If triggeredByAura has SPELL_ATTR4_PROC_ONLY_ON_CASTER then it can only proc on a casted spell with TARGET_UNIT_CASTER
    for (uint8 i = 0;i < MAX_SPELL_EFFECTS; ++i)
    {
        if ((effMask & (1 << i)) && (!only_on_caster || (m_spellInfo->Effects[i].TargetA.GetTarget() == TARGET_UNIT_CASTER)))
            return true;
    }
    return false;
}

void Spell::PrepareTriggersExecutedOnHit()
{
    // todo: move this to scripts
    if (m_spellInfo->SpellFamilyName)
    {
        // There are some new ExcludeCasterAuraSpell entrys like Killing Spree for Fan of knives which makes this
        // incorrect. TODO: Find broad rule
        SpellInfo const* excludeCasterSpellInfo = sSpellMgr->GetSpellInfo(m_spellInfo->ExcludeCasterAuraSpell);
        if (m_spellInfo->IsPositive() && excludeCasterSpellInfo && !excludeCasterSpellInfo->IsPositive())
            m_preCastSpell = m_spellInfo->ExcludeCasterAuraSpell;
        SpellInfo const* excludeTargetSpellInfo = sSpellMgr->GetSpellInfo(m_spellInfo->ExcludeTargetAuraSpell);
        if (excludeTargetSpellInfo && !excludeTargetSpellInfo->IsPositive())
            m_preCastSpell = m_spellInfo->ExcludeTargetAuraSpell;
    }

    // todo: move this to scripts
    switch (m_spellInfo->SpellFamilyName)
    {
        case SPELLFAMILY_MAGE:
        {
             // Permafrost
             if (m_spellInfo->SpellFamilyFlags[1] & 0x00001000 ||  m_spellInfo->SpellFamilyFlags[0] & 0x00100220)
                 m_preCastSpell = 68391;
             break;
        }
    }

    // handle SPELL_AURA_ADD_TARGET_TRIGGER auras:
    // save auras which were present on spell caster on cast, to prevent triggered auras from affecting caster
    // and to correctly calculate proc chance when combopoints are present
    Unit::AuraEffectList const& targetTriggers = m_caster->GetAuraEffectsByType(SPELL_AURA_ADD_TARGET_TRIGGER);
    for (auto i = targetTriggers.begin(); i != targetTriggers.end(); ++i)
    {
        if (!(*i)->IsAffectingSpell(m_spellInfo))
            continue;
        SpellInfo const* auraSpellInfo = (*i)->GetSpellInfo();
        uint32 auraSpellIdx = (*i)->GetEffIndex();
        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(auraSpellInfo->Effects[auraSpellIdx].TriggerSpell))
        {
            // calculate the chance using spell base amount, because aura amount is not updated on combo-points change
            // this possibly needs fixing
            int32 auraBaseAmount = (*i)->GetBaseAmount();
            // proc chance is stored in effect amount
            int32 chance = m_caster->CalculateSpellDamage(NULL, auraSpellInfo, auraSpellIdx, &auraBaseAmount);
            // build trigger and add to the list
            HitTriggerSpell spellTriggerInfo;
            spellTriggerInfo.triggeredSpell = spellInfo;
            spellTriggerInfo.triggeredByAura = auraSpellInfo;
            spellTriggerInfo.chance = chance * (*i)->GetBase()->GetStackAmount();
            m_hitTriggerSpells.push_back(spellTriggerInfo);
        }
    }
}

// Global cooldowns management
enum GCDLimits
{
    MIN_GCD = 1000,
    MAX_GCD_MIND_CONTROLLED_NPC = 3000,
    MAX_GCD = 1500
};

bool Spell::HasGlobalCooldown() const
{
    // Only player or controlled units have global cooldown
    if (m_caster->GetCharmInfo())
        return m_caster->GetCharmInfo()->GetGlobalCooldownMgr().HasGlobalCooldown(m_spellInfo);
    else if (m_caster->GetTypeId() == TYPEID_PLAYER)
        return m_caster->ToPlayer()->GetGlobalCooldownMgr().HasGlobalCooldown(m_spellInfo);
    else
        return false;
}

void Spell::TriggerGlobalCooldown()
{
    int32 gcd = m_spellInfo->StartRecoveryTime;
    bool is_mind_controlled_npc = false;

    if (m_caster->ToCreature())
        if (m_caster->HasAura(605))
        {
            is_mind_controlled_npc = true;
        }

    if (!is_mind_controlled_npc)
    if (!gcd)
        return;

    if (is_mind_controlled_npc)
        gcd = MAX_GCD_MIND_CONTROLLED_NPC;

    if (m_caster->GetTypeId() == TYPEID_PLAYER)
        if (m_caster->ToPlayer()->GetCommandStatus(CHEAT_COOLDOWN))
            return;

    // Global cooldown can't leave range 1..1.5 secs
    // There are some spells (mostly not casted directly by player) that have < 1 sec and > 1.5 sec global cooldowns
    // but as tests show are not affected by any spell mods.
    if (m_spellInfo->StartRecoveryTime >= uint32(MIN_GCD) && m_spellInfo->StartRecoveryTime <= uint32(is_mind_controlled_npc ? MAX_GCD_MIND_CONTROLLED_NPC : MAX_GCD))
    {
        // Apply haste rating
        gcd = int32(float(gcd) * m_caster->GetFloatValue(UNIT_MOD_CAST_SPEED));
        if (gcd < MIN_GCD)
            gcd = MIN_GCD;
        else if (gcd > (is_mind_controlled_npc ? MAX_GCD_MIND_CONTROLLED_NPC : MAX_GCD))
            gcd = (is_mind_controlled_npc ? MAX_GCD_MIND_CONTROLLED_NPC : MAX_GCD);
    }

    // gcd modifier auras are applied only to own spells and only players have such mods
    if (m_caster->GetTypeId() == TYPEID_PLAYER)
        m_caster->ToPlayer()->ApplySpellMod(m_spellInfo->Id, SPELLMOD_GLOBAL_COOLDOWN, gcd, this);

    if (gcd < 0)
        gcd = 0;

    // Only players or controlled units have global cooldown
    if (m_caster->GetCharmInfo())
        m_caster->GetCharmInfo()->GetGlobalCooldownMgr().AddGlobalCooldown(m_spellInfo, gcd);
    else if (m_caster->GetTypeId() == TYPEID_PLAYER)
        m_caster->ToPlayer()->GetGlobalCooldownMgr().AddGlobalCooldown(m_spellInfo, gcd);
}

void Spell::CancelGlobalCooldown()
{
    if (!m_spellInfo->StartRecoveryTime)
        return;

    // Cancel global cooldown when interrupting current cast
    if (m_caster->GetCurrentSpell(CURRENT_GENERIC_SPELL) != this)
        return;

    // Only players or controlled units have global cooldown
    if (m_caster->GetCharmInfo())
        m_caster->GetCharmInfo()->GetGlobalCooldownMgr().CancelGlobalCooldown(m_spellInfo);
    else if (m_caster->GetTypeId() == TYPEID_PLAYER)
        m_caster->ToPlayer()->GetGlobalCooldownMgr().CancelGlobalCooldown(m_spellInfo);
}

namespace Trinity
{

WorldObjectSpellTargetCheck::WorldObjectSpellTargetCheck(Unit* caster, Unit* referer, SpellInfo const* spellInfo,
            SpellTargetCheckTypes selectionType, ConditionList* condList) : _caster(caster), _referer(referer), _spellInfo(spellInfo),
    _targetSelectionType(selectionType), _condList(condList)
{
    if (condList)
        _condSrcInfo = new ConditionSourceInfo(NULL, caster);
    else
        _condSrcInfo = NULL;
}

WorldObjectSpellTargetCheck::~WorldObjectSpellTargetCheck()
{
    if (_condSrcInfo)
        delete _condSrcInfo;
}

bool WorldObjectSpellTargetCheck::operator()(WorldObject* target)
{
    if (_spellInfo->CheckTarget(_caster, target, true) != SPELL_CAST_OK)
        return false;
    Unit* unitTarget = target->ToUnit();
    if (Corpse* corpseTarget = target->ToCorpse())
    {
        // use ofter for party/assistance checks
        if (Player* owner = ObjectAccessor::FindPlayer(corpseTarget->GetOwnerGUID()))
            unitTarget = owner;
        else
            return false;
    }
    if (unitTarget)
    {
        switch (_targetSelectionType)
        {
            case TARGET_CHECK_ENEMY:
                if (unitTarget->isTotem())
                    return false;
                if (!_caster->_IsValidAttackTarget(unitTarget, _spellInfo))
                    return false;
                break;
            case TARGET_CHECK_ALLY:
                if (unitTarget->isTotem())
                    return false;
                if (_spellInfo->HasAttribute(SPELL_ATTR7_CONSOLIDATED_RAID_BUFF) || _spellInfo->HasAttribute(SPELL_ATTR5_CAN_NOT_TARGET_PETS))
                    if (unitTarget->isPet() || unitTarget->isGuardian())
                        return false;
                if (!_caster->_IsValidAssistTarget(unitTarget, _spellInfo))
                    return false;
                break;
            case TARGET_CHECK_PARTY:
                if (unitTarget->isTotem())
                    return false;
                if (_spellInfo->HasAttribute(SPELL_ATTR7_CONSOLIDATED_RAID_BUFF) || _spellInfo->HasAttribute(SPELL_ATTR5_CAN_NOT_TARGET_PETS))
                    if (unitTarget->isPet() || unitTarget->isGuardian())
                        return false;
                if (!_caster->_IsValidAssistTarget(unitTarget, _spellInfo))
                    return false;
                if (!_referer->IsInPartyWith(unitTarget))
                    return false;
                break;
            case TARGET_CHECK_RAID_CLASS:
                if (_referer->getClass() != unitTarget->getClass())
                    return false;
                // nobreak;
            case TARGET_CHECK_RAID:
                if (unitTarget->isTotem())
                    return false;
                if (_spellInfo->HasAttribute(SPELL_ATTR7_CONSOLIDATED_RAID_BUFF) || _spellInfo->HasAttribute(SPELL_ATTR5_CAN_NOT_TARGET_PETS))
                    if (unitTarget->isPet() || unitTarget->isGuardian())
                        return false;
                if (!_caster->_IsValidAssistTarget(unitTarget, _spellInfo))
                    return false;
                if (!_referer->IsInRaidWith(unitTarget))
                    return false;
                break;
            default:
                break;
        }
    }
    if (!_condSrcInfo)
        return true;
    _condSrcInfo->mConditionTargets[0] = target;
    return sConditionMgr->IsObjectMeetToConditions(*_condSrcInfo, *_condList);
}

WorldObjectSpellNearbyTargetCheck::WorldObjectSpellNearbyTargetCheck(float range, Unit* caster, SpellInfo const* spellInfo,
    SpellTargetCheckTypes selectionType, ConditionList* condList)
    : WorldObjectSpellTargetCheck(caster, caster, spellInfo, selectionType, condList), _range(range), _position(caster)
{
}

bool WorldObjectSpellNearbyTargetCheck::operator()(WorldObject* target)
{
    float dist = target->GetDistance(*_position);
    if (dist < _range && WorldObjectSpellTargetCheck::operator ()(target))
    {
        _range = dist;
        return true;
    }
    return false;
}

WorldObjectSpellAreaTargetCheck::WorldObjectSpellAreaTargetCheck(float range, Position const* position, Unit* caster,
    Unit* referer, SpellInfo const* spellInfo, SpellTargetCheckTypes selectionType, ConditionList* condList)
    : WorldObjectSpellTargetCheck(caster, referer, spellInfo, selectionType, condList), _range(range), _position(position)
{
}

bool WorldObjectSpellAreaTargetCheck::operator()(WorldObject* target)
{
    float new_dist = _range;
    if (target && _range > target->GetObjectSize())
        new_dist -= target->GetObjectSize();
    if (!target->IsWithinDist3d(_position, _range) && !(target->ToGameObject() && target->ToGameObject()->IsInRange(_position->GetPositionX(), _position->GetPositionY(), _position->GetPositionZ(), _range)))
        return false;
    return WorldObjectSpellTargetCheck::operator ()(target);
}

WorldObjectSpellConeTargetCheck::WorldObjectSpellConeTargetCheck(float coneAngle, float range, Unit* caster,
    SpellInfo const* spellInfo, SpellTargetCheckTypes selectionType, ConditionList* condList)
    : WorldObjectSpellAreaTargetCheck(range, caster, caster, caster, spellInfo, selectionType, condList), _coneAngle(coneAngle)
{
}

bool WorldObjectSpellConeTargetCheck::operator()(WorldObject* target)
{
    if (_spellInfo->AttributesCu & SPELL_ATTR0_CU_CONE_BACK)
    {
        if (!_caster->isInBack(target, _coneAngle))
            return false;
    }
    else if (_spellInfo->AttributesCu & SPELL_ATTR0_CU_CONE_LINE)
    {
        if (!_caster->HasInLine(target, _caster->GetObjectSize()))
            return false;
    }
    else
    {
        if (!_caster->isInFront(target, _coneAngle))
            return false;
    }
    return WorldObjectSpellAreaTargetCheck::operator ()(target);
}

WorldObjectSpellTrajTargetCheck::WorldObjectSpellTrajTargetCheck(float range, Position const* position, Unit* caster, SpellInfo const* spellInfo)
    : WorldObjectSpellAreaTargetCheck(range, position, caster, caster, spellInfo, TARGET_CHECK_DEFAULT, NULL)
{
}

bool WorldObjectSpellTrajTargetCheck::operator()(WorldObject* target)
{
    // return all targets on missile trajectory (0 - size of a missile)
    if (!_caster->HasInLine(target, 0))
        return false;
    return WorldObjectSpellAreaTargetCheck::operator ()(target);
}

} //namespace Trinity
