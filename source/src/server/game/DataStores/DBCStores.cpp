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

#include "DBCStores.h"
#include "Log.h"
#include "SharedDefines.h"
#include "SpellMgr.h"
#include "DBCfmt.h"
#include "ItemPrototype.h"

#include <map>

typedef std::map<uint16, uint32> AreaFlagByAreaID;
typedef std::map<uint32, uint32> AreaFlagByMapID;

struct WMOAreaTableTripple
{
    WMOAreaTableTripple(int32 r, int32 a, int32 g) :  groupId(g), rootId(r), adtId(a)
    {
    }

    bool operator <(const WMOAreaTableTripple& b) const
    {
        return memcmp(this, &b, sizeof(WMOAreaTableTripple))<0;
    }

    // ordered by entropy; that way memcmp will have a minimal medium runtime
    int32 groupId;
    int32 rootId;
    int32 adtId;
};

typedef std::map<WMOAreaTableTripple, WMOAreaTableEntry const*> WMOAreaInfoByTripple;

DBCStorage <AreaTableEntry> sAreaStore(AreaTableEntryfmt);
DBCStorage <AreaGroupEntry> sAreaGroupStore(AreaGroupEntryfmt);
DBCStorage <AreaPOIEntry> sAreaPOIStore(AreaPOIEntryfmt);
static AreaFlagByAreaID sAreaFlagByAreaID;
static AreaFlagByMapID sAreaFlagByMapID;                    // for instances without generated *.map files

static WMOAreaInfoByTripple sWMOAreaInfoByTripple;

DBCStorage <AchievementEntry> sAchievementStore(Achievementfmt);
DBCStorage <AchievementCriteriaEntry> sAchievementCriteriaStore(AchievementCriteriafmt);
DBCStorage <AreaTriggerEntry> sAreaTriggerStore(AreaTriggerEntryfmt);
DBCStorage <ArmorLocationEntry> sArmorLocationStore(ArmorLocationfmt);
DBCStorage <AuctionHouseEntry> sAuctionHouseStore(AuctionHouseEntryfmt);
DBCStorage <BankBagSlotPricesEntry> sBankBagSlotPricesStore(BankBagSlotPricesEntryfmt);
DBCStorage <BattlemasterListEntry> sBattlemasterListStore(BattlemasterListEntryfmt);
DBCStorage <BarberShopStyleEntry> sBarberShopStyleStore(BarberShopStyleEntryfmt);
DBCStorage <CharStartOutfitEntry> sCharStartOutfitStore(CharStartOutfitEntryfmt);
std::map<uint32, CharStartOutfitEntry const*> sCharStartOutfitMap;
DBCStorage <CharTitlesEntry> sCharTitlesStore(CharTitlesEntryfmt);
DBCStorage <ChatChannelsEntry> sChatChannelsStore(ChatChannelsEntryfmt);
DBCStorage <ChrClassesEntry> sChrClassesStore(ChrClassesEntryfmt);
DBCStorage <ChrRacesEntry> sChrRacesStore(ChrRacesEntryfmt);
DBCStorage <ChrPowerTypesEntry> sChrPowerTypesStore(ChrClassesXPowerTypesfmt);
DBCStorage <CinematicSequencesEntry> sCinematicSequencesStore(CinematicSequencesEntryfmt);
DBCStorage <CreatureDisplayInfoEntry> sCreatureDisplayInfoStore(CreatureDisplayInfofmt);
DBCStorage <CreatureFamilyEntry> sCreatureFamilyStore(CreatureFamilyfmt);
DBCStorage <CreatureModelDataEntry> sCreatureModelDataStore(CreatureModelDatafmt);
DBCStorage <CreatureSpellDataEntry> sCreatureSpellDataStore(CreatureSpellDatafmt);
DBCStorage <CreatureTypeEntry> sCreatureTypeStore(CreatureTypefmt);
DBCStorage <CurrencyTypesEntry> sCurrencyTypesStore(CurrencyTypesfmt);
uint32 PowersByClass[MAX_CLASSES][MAX_POWERS];

DBCStorage <DestructibleModelDataEntry> sDestructibleModelDataStore(DestructibleModelDatafmt);
DBCStorage <DungeonEncounterEntry> sDungeonEncounterStore(DungeonEncounterfmt);
DBCStorage <DurabilityQualityEntry> sDurabilityQualityStore(DurabilityQualityfmt);
DBCStorage <DurabilityCostsEntry> sDurabilityCostsStore(DurabilityCostsfmt);

DBCStorage <EmotesEntry> sEmotesStore(EmotesEntryfmt);
DBCStorage <EmotesTextEntry> sEmotesTextStore(EmotesTextEntryfmt);

typedef std::map<uint32, SimpleFactionsList> FactionTeamMap;
static FactionTeamMap sFactionTeamMap;
DBCStorage <FactionEntry> sFactionStore(FactionEntryfmt);
DBCStorage <FactionTemplateEntry> sFactionTemplateStore(FactionTemplateEntryfmt);

DBCStorage <GameObjectDisplayInfoEntry> sGameObjectDisplayInfoStore(GameObjectDisplayInfofmt);
DBCStorage <GemPropertiesEntry> sGemPropertiesStore(GemPropertiesEntryfmt);
DBCStorage <GlyphPropertiesEntry> sGlyphPropertiesStore(GlyphPropertiesfmt);
DBCStorage <GlyphSlotEntry> sGlyphSlotStore(GlyphSlotfmt);

DBCStorage <GtBarberShopCostBaseEntry>    sGtBarberShopCostBaseStore(GtBarberShopCostBasefmt);
DBCStorage <GtCombatRatingsEntry>         sGtCombatRatingsStore(GtCombatRatingsfmt);
DBCStorage <GtChanceToMeleeCritBaseEntry> sGtChanceToMeleeCritBaseStore(GtChanceToMeleeCritBasefmt);
DBCStorage <GtChanceToMeleeCritEntry>     sGtChanceToMeleeCritStore(GtChanceToMeleeCritfmt);
DBCStorage <GtChanceToSpellCritBaseEntry> sGtChanceToSpellCritBaseStore(GtChanceToSpellCritBasefmt);
DBCStorage <GtChanceToSpellCritEntry>     sGtChanceToSpellCritStore(GtChanceToSpellCritfmt);
DBCStorage <GtNPCManaCostScalerEntry>     sGtNPCManaCostScalerStore(GtNPCManaCostScalerfmt);
DBCStorage <GtOCTClassCombatRatingScalarEntry> sGtOCTClassCombatRatingScalarStore(GtOCTClassCombatRatingScalarfmt);
//DBCStorage <GtOCTRegenHPEntry>            sGtOCTRegenHPStore(GtOCTRegenHPfmt);
//DBCStorage <GtOCTRegenMPEntry>            sGtOCTRegenMPStore(GtOCTRegenMPfmt);  -- not used currently
DBCStorage <gtOCTHpPerStaminaEntry>       sGtOCTHpPerStaminaStore(GtOCTHpPerStaminafmt);
DBCStorage <GtRegenMPPerSptEntry>         sGtRegenMPPerSptStore(GtRegenMPPerSptfmt);
DBCStorage <GtSpellScalingEntry>          sGtSpellScalingStore(GtSpellScalingfmt);
DBCStorage <GtOCTBaseHPByClassEntry>      sGtOCTBaseHPByClassStore(GtOCTBaseHPByClassfmt);
DBCStorage <GtOCTBaseMPByClassEntry>      sGtOCTBaseMPByClassStore(GtOCTBaseMPByClassfmt);
DBCStorage <GuildPerkSpellsEntry>         sGuildPerkSpellsStore(GuildPerkSpellsfmt);

DBCStorage <HolidaysEntry>                sHolidaysStore(Holidaysfmt);

DBCStorage <ImportPriceArmorEntry>        sImportPriceArmorStore(ImportPriceArmorfmt);
DBCStorage <ImportPriceQualityEntry>      sImportPriceQualityStore(ImportPriceQualityfmt);
DBCStorage <ImportPriceShieldEntry>       sImportPriceShieldStore(ImportPriceShieldfmt);
DBCStorage <ImportPriceWeaponEntry>       sImportPriceWeaponStore(ImportPriceWeaponfmt);
DBCStorage <ItemPriceBaseEntry>           sItemPriceBaseStore(ItemPriceBasefmt);
DBCStorage <ItemReforgeEntry>             sItemReforgeStore(ItemReforgefmt);
DBCStorage <ItemArmorQualityEntry>        sItemArmorQualityStore(ItemArmorQualityfmt);
DBCStorage <ItemArmorShieldEntry>         sItemArmorShieldStore(ItemArmorShieldfmt);
DBCStorage <ItemArmorTotalEntry>          sItemArmorTotalStore(ItemArmorTotalfmt);
DBCStorage <ItemClassEntry>               sItemClassStore(ItemClassfmt);
DBCStorage <ItemBagFamilyEntry>           sItemBagFamilyStore(ItemBagFamilyfmt);
DBCStorage <ItemDamageEntry>              sItemDamageAmmoStore(ItemDamagefmt);
DBCStorage <ItemDamageEntry>              sItemDamageOneHandStore(ItemDamagefmt);
DBCStorage <ItemDamageEntry>              sItemDamageOneHandCasterStore(ItemDamagefmt);
DBCStorage <ItemDamageEntry>              sItemDamageRangedStore(ItemDamagefmt);
DBCStorage <ItemDamageEntry>              sItemDamageThrownStore(ItemDamagefmt);
DBCStorage <ItemDamageEntry>              sItemDamageTwoHandStore(ItemDamagefmt);
DBCStorage <ItemDamageEntry>              sItemDamageTwoHandCasterStore(ItemDamagefmt);
DBCStorage <ItemDamageEntry>              sItemDamageWandStore(ItemDamagefmt);
DBCStorage <ItemDisenchantLootEntry>      sItemDisenchantLootStore(ItemDisenchantLootfmt);
//DBCStorage <ItemDisplayInfoEntry> sItemDisplayInfoStore(ItemDisplayTemplateEntryfmt); -- not used currently
DBCStorage <ItemLimitCategoryEntry> sItemLimitCategoryStore(ItemLimitCategoryEntryfmt);
DBCStorage <ItemRandomPropertiesEntry> sItemRandomPropertiesStore(ItemRandomPropertiesfmt);
DBCStorage <ItemRandomSuffixEntry> sItemRandomSuffixStore(ItemRandomSuffixfmt);
DBCStorage <ItemSetEntry> sItemSetStore(ItemSetEntryfmt);

DBCStorage <LFGDungeonEntry> sLFGDungeonStore(LFGDungeonEntryfmt);
DBCStorage <LiquidTypeEntry> sLiquidTypeStore(LiquidTypefmt);
DBCStorage <LockEntry> sLockStore(LockEntryfmt);

DBCStorage <MailTemplateEntry> sMailTemplateStore(MailTemplateEntryfmt);
DBCStorage <MapEntry> sMapStore(MapEntryfmt);

// DBC used only for initialization sMapDifficultyMap at startup.
DBCStorage <MapDifficultyEntry> sMapDifficultyStore(MapDifficultyEntryfmt); // only for loading
MapDifficultyMap sMapDifficultyMap;

DBCStorage <MovieEntry> sMovieStore(MovieEntryfmt);
DBCStorage <MountCapabilityEntry> sMountCapabilityStore(MountCapabilityfmt);
DBCStorage <MountTypeEntry> sMountTypeStore(MountTypefmt);

DBCStorage <NameGenEntry> sNameGenStore(NameGenfmt);
NameGenVectorArraysMap sGenNameVectoArraysMap;
DBCStorage <NumTalentsAtLevelEntry> sNumTalentsAtLevelStore(NumTalentsAtLevelfmt);

DBCStorage <OverrideSpellDataEntry> sOverrideSpellDataStore(OverrideSpellDatafmt);

DBCStorage <PvPDifficultyEntry> sPvPDifficultyStore(PvPDifficultyfmt);

DBCStorage <QuestSortEntry> sQuestSortStore(QuestSortEntryfmt);
DBCStorage <QuestXPEntry>   sQuestXPStore(QuestXPfmt);
DBCStorage <QuestFactionRewEntry>  sQuestFactionRewardStore(QuestFactionRewardfmt);
DBCStorage <RandomPropertiesPointsEntry> sRandomPropertiesPointsStore(RandomPropertiesPointsfmt);
DBCStorage <ResearchSiteEntry> sResearchSiteStore(ResearchSitefmt);
DBCStorage <ResearchProjectEntry> sResearchProjectStore(ResearchProjectfmt);
DBCStorage <ResearchSitePoints> sResearchSitePoints(ResearchSitePointsfmt);
DBCStorage <ScalingStatDistributionEntry> sScalingStatDistributionStore(ScalingStatDistributionfmt);
DBCStorage <ScalingStatValuesEntry> sScalingStatValuesStore(ScalingStatValuesfmt);

DBCStorage <SkillLineEntry> sSkillLineStore(SkillLinefmt);
DBCStorage <SkillLineAbilityEntry> sSkillLineAbilityStore(SkillLineAbilityfmt);

DBCStorage <SoundEntriesEntry> sSoundEntriesStore(SoundEntriesfmt);

DBCStorage <SpellItemEnchantmentEntry> sSpellItemEnchantmentStore(SpellItemEnchantmentfmt);
DBCStorage <SpellItemEnchantmentConditionEntry> sSpellItemEnchantmentConditionStore(SpellItemEnchantmentConditionfmt);
DBCStorage <SpellEntry> sSpellStore(SpellEntryfmt);
SpellCategoryStore sSpellCategoryStore;
PetFamilySpellsStore sPetFamilySpellsStore;


DBCStorage <SpellReagentsEntry> sSpellReagentsStore(SpellReagentsEntryfmt);
DBCStorage <SpellScalingEntry> sSpellScalingStore(SpellScalingEntryfmt);
DBCStorage <SpellTotemsEntry> sSpellTotemsStore(SpellTotemsEntryfmt);
DBCStorage <SpellTargetRestrictionsEntry> sSpellTargetRestrictionsStore(SpellTargetRestrictionsEntryfmt);
DBCStorage <SpellPowerEntry> sSpellPowerStore(SpellPowerEntryfmt);
DBCStorage <SpellLevelsEntry> sSpellLevelsStore(SpellLevelsEntryfmt);
DBCStorage <SpellInterruptsEntry> sSpellInterruptsStore(SpellInterruptsEntryfmt);
DBCStorage <SpellEquippedItemsEntry> sSpellEquippedItemsStore(SpellEquippedItemsEntryfmt);
DBCStorage <SpellClassOptionsEntry> sSpellClassOptionsStore(SpellClassOptionsEntryfmt);
DBCStorage <SpellCooldownsEntry> sSpellCooldownsStore(SpellCooldownsEntryfmt);
DBCStorage <SpellAuraOptionsEntry> sSpellAuraOptionsStore(SpellAuraOptionsEntryfmt);
DBCStorage <SpellAuraRestrictionsEntry> sSpellAuraRestrictionsStore(SpellAuraRestrictionsEntryfmt);
DBCStorage <SpellCastingRequirementsEntry> sSpellCastingRequirementsStore(SpellCastingRequirementsEntryfmt);
DBCStorage <SpellCastTimesEntry> sSpellCastTimesStore(SpellCastTimefmt);
DBCStorage <SpellCategoriesEntry> sSpellCategoriesStore(SpellCategoriesEntryfmt);
DBCStorage <SpellEffectEntry> sSpellEffectStore(SpellEffectEntryfmt);
DBCStorage <SpellDifficultyEntry> sSpellDifficultyStore(SpellDifficultyfmt);
DBCStorage <SpellDurationEntry> sSpellDurationStore(SpellDurationfmt);
DBCStorage <SpellFocusObjectEntry> sSpellFocusObjectStore(SpellFocusObjectfmt);
DBCStorage <SpellRadiusEntry> sSpellRadiusStore(SpellRadiusfmt);
DBCStorage <SpellRangeEntry> sSpellRangeStore(SpellRangefmt);
DBCStorage <SpellRuneCostEntry> sSpellRuneCostStore(SpellRuneCostfmt);
DBCStorage <SpellShapeshiftEntry> sSpellShapeshiftStore(SpellShapeshiftEntryfmt);
DBCStorage <SpellShapeshiftFormEntry> sSpellShapeshiftFormStore(SpellShapeshiftFormfmt);
DBCStorage <SpellVisualEntry> sSpellVisualStore(SpellVisualfmt);
DBCStorage <StableSlotPricesEntry> sStableSlotPricesStore(StableSlotPricesfmt);
DBCStorage <SummonPropertiesEntry> sSummonPropertiesStore(SummonPropertiesfmt);
DBCStorage <TalentEntry> sTalentStore(TalentEntryfmt);
TalentSpellPosMap sTalentSpellPosMap;
DBCStorage <TalentTabEntry> sTalentTabStore(TalentTabEntryfmt);
DBCStorage <TalentTreePrimarySpellsEntry> sTalentTreePrimarySpellsStore(TalentTreePrimarySpellsfmt);
typedef std::map<uint32, std::vector<uint32> > TalentTreePrimarySpellsMap;
TalentTreePrimarySpellsMap sTalentTreePrimarySpellsMap;
typedef std::map<uint32, uint32> SpellVisualMap;
SpellVisualMap sSpellVisualMap;

// store absolute bit position for first rank for talent inspect
static uint32 sTalentTabPages[MAX_CLASSES][3];

DBCStorage <TaxiNodesEntry> sTaxiNodesStore(TaxiNodesEntryfmt);
TaxiMask sTaxiNodesMask;
TaxiMask sOldContinentsNodesMask;
TaxiMask sHordeTaxiNodesMask;
TaxiMask sAllianceTaxiNodesMask;
TaxiMask sDeathKnightTaxiNodesMask;

// DBC used only for initialization sTaxiPathSetBySource at startup.
TaxiPathSetBySource sTaxiPathSetBySource;
DBCStorage <TaxiPathEntry> sTaxiPathStore(TaxiPathEntryfmt);

// DBC used only for initialization sTaxiPathNodeStore at startup.
TaxiPathNodesByPath sTaxiPathNodesByPath;
static DBCStorage <TaxiPathNodeEntry> sTaxiPathNodeStore(TaxiPathNodeEntryfmt);

DBCStorage <TotemCategoryEntry> sTotemCategoryStore(TotemCategoryEntryfmt);
DBCStorage <UnitPowerBarEntry> sUnitPowerBarStore(UnitPowerBarfmt);
DBCStorage <VehicleEntry> sVehicleStore(VehicleEntryfmt);
DBCStorage <VehicleSeatEntry> sVehicleSeatStore(VehicleSeatEntryfmt);
DBCStorage <WMOAreaTableEntry> sWMOAreaTableStore(WMOAreaTableEntryfmt);
DBCStorage <WorldMapAreaEntry> sWorldMapAreaStore(WorldMapAreaEntryfmt);
DBCStorage <WorldMapOverlayEntry> sWorldMapOverlayStore(WorldMapOverlayEntryfmt);
DBCStorage <WorldSafeLocsEntry> sWorldSafeLocsStore(WorldSafeLocsEntryfmt);
DBCStorage <PhaseEntry> sPhaseStores(PhaseEntryfmt);

DBCStorage <TransportAnimationEntry> sTransportAnimationStore(TransportAnimationEntryfmt);
DBCStorage <TransportRotationEntry> sTransportRotationStore(TransportRotationEntryfmt);
TransportAnimationContainer sTransportAnimationContainer;

typedef std::list<std::string> StoreProblemList;

uint32 DBCFileCount = 0;

static bool LoadDBC_assert_print(uint32 fsize, uint32 rsize, const std::string& filename)
{
    TC_LOG_ERROR("misc", "Size of '%s' set by format string (%u) not equal size of C++ structure (%u).", filename.c_str(), fsize, rsize);

    // ASSERT must fail after function call
    return false;
}

template<class T>
inline void LoadDBC(uint32& availableDbcLocales, StoreProblemList& errors, DBCStorage<T>& storage, std::string const& dbcPath, std::string const& filename, bool force = false, std::string const* customFormat = NULL, std::string const* customIndexName = NULL)
{
    // compatibility format and C++ structure sizes
    ASSERT(
        DBCFileLoader::GetFormatRecordSize(storage.GetFormat()) == sizeof(T) 
        || 
        LoadDBC_assert_print(DBCFileLoader::GetFormatRecordSize(storage.GetFormat()), sizeof(T), filename));

    ++DBCFileCount;
    std::string dbcFilename = dbcPath + filename;
    SqlDbc * sql = NULL;
    if (customFormat)
        sql = new SqlDbc(&filename, customFormat, customIndexName, storage.GetFormat());

    if (storage.Load(dbcFilename.c_str(), sql, force))
    {
        for (uint8 i = 0; i < TOTAL_LOCALES; ++i)
        {
            if (!(availableDbcLocales & (1 << i)))
                continue;

            std::string localizedName(dbcPath);
            localizedName.append(localeNames[i]);
            localizedName.push_back('/');
            localizedName.append(filename);

            if (!storage.LoadStringsFrom(localizedName.c_str()))
                availableDbcLocales &= ~(1<<i);             // mark as not available for speedup next checks
        }
    }
    else
    {
        // sort problematic dbc to (1) non compatible and (2) non-existed
        if (FILE* f = fopen(dbcFilename.c_str(), "rb"))
        {
            // storage.GetFormat
            std::ostringstream stream;
            stream << dbcFilename << " exists, and has " << storage.GetFieldCount() << " field(s) (expected " << strlen(storage.GetFormat()) << "). Extracted file might be from wrong client version or a database-update has been forgotten.";
            std::string buf = stream.str();
            errors.push_back(buf);
            fclose(f);
        }
        else
            errors.push_back(dbcFilename);
    }

    delete sql;
}

void LoadDBCStores(const std::string& dataPath)
{
    uint32 oldMSTime = getMSTime();

    std::string dbcPath = dataPath+"dbc/";

    StoreProblemList bad_dbc_files;
    uint32 availableDbcLocales = 0xFFFFFFFF;

    LoadDBC(availableDbcLocales, bad_dbc_files, sAreaStore,                   dbcPath, "AreaTable.dbc");

    // must be after sAreaStore loading
    for (uint32 i = 0; i < sAreaStore.GetNumRows(); ++i)           // areaflag numbered from 0
    {
        if (AreaTableEntry const* area = sAreaStore.LookupEntry(i))
        {
            // fill AreaId->DBC records
            sAreaFlagByAreaID.insert(AreaFlagByAreaID::value_type(uint16(area->ID), area->exploreFlag));

            // fill MapId->DBC records (skip sub zones and continents)
            if (area->zone == 0 && area->mapid != 0 && area->mapid != 1 && area->mapid != 530 && area->mapid != 571)
                sAreaFlagByMapID.insert(AreaFlagByMapID::value_type(area->mapid, area->exploreFlag));
        }
    }

    LoadDBC(availableDbcLocales, bad_dbc_files, sAchievementStore,            dbcPath, "Achievement.dbc", false, &CustomAchievementfmt, &CustomAchievementIndex);//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sAchievementCriteriaStore,    dbcPath, "Achievement_Criteria.dbc", true, &CustomAchievementCriteriafmt, &CustomAchievementCriteriaIndex);//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sAreaTriggerStore,            dbcPath, "AreaTrigger.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sAreaGroupStore,              dbcPath, "AreaGroup.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sAreaPOIStore,                dbcPath, "AreaPOI.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sAuctionHouseStore,           dbcPath, "AuctionHouse.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sArmorLocationStore,          dbcPath, "ArmorLocation.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sBankBagSlotPricesStore,      dbcPath, "BankBagSlotPrices.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sBattlemasterListStore,       dbcPath, "BattlemasterList.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sBarberShopStyleStore,        dbcPath, "BarberShopStyle.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sCharStartOutfitStore,        dbcPath, "CharStartOutfit.dbc");//14545
    for (uint32 i = 0; i < sCharStartOutfitStore.GetNumRows(); ++i)
        if (CharStartOutfitEntry const* outfit = sCharStartOutfitStore.LookupEntry(i))
            sCharStartOutfitMap[outfit->Race | (outfit->Class << 8) | (outfit->Gender << 16)] = outfit;

    LoadDBC(availableDbcLocales, bad_dbc_files, sCharTitlesStore,             dbcPath, "CharTitles.dbc");//14545
    for (uint32 i = 0; i < sCharTitlesStore.GetNumRows(); ++i)
        if (CharTitlesEntry const* titleEntry = sCharTitlesStore.LookupEntry(i))
            sTitleByIndexStore[titleEntry->bit_index] = titleEntry;

    LoadDBC(availableDbcLocales, bad_dbc_files, sChatChannelsStore,           dbcPath, "ChatChannels.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sChrClassesStore,             dbcPath, "ChrClasses.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sChrRacesStore,               dbcPath, "ChrRaces.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sChrPowerTypesStore,          dbcPath, "ChrClassesXPowerTypes.dbc");//14545
    for (uint32 i = 0; i < MAX_CLASSES; ++i)
        for (uint32 j = 0; j < MAX_POWERS; ++j)
            PowersByClass[i][j] = MAX_POWERS;

    for (uint32 i = 0; i < sChrPowerTypesStore.GetNumRows(); ++i)
    {
        if (ChrPowerTypesEntry const* power = sChrPowerTypesStore.LookupEntry(i))
        {
            uint32 index = 0;
            for (uint32 j = 0; j < MAX_POWERS; ++j)
                if (PowersByClass[power->classId][j] != MAX_POWERS)
                    ++index;

            PowersByClass[power->classId][power->power] = index;
        }
    }

    LoadDBC(availableDbcLocales, bad_dbc_files, sCinematicSequencesStore,     dbcPath, "CinematicSequences.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sCreatureDisplayInfoStore,    dbcPath, "CreatureDisplayInfo.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sCreatureFamilyStore,         dbcPath, "CreatureFamily.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sCreatureModelDataStore,      dbcPath, "CreatureModelData.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sCreatureSpellDataStore,      dbcPath, "CreatureSpellData.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sCreatureTypeStore,           dbcPath, "CreatureType.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sCurrencyTypesStore,          dbcPath, "CurrencyTypes.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sDestructibleModelDataStore,  dbcPath, "DestructibleModelData.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sDungeonEncounterStore,       dbcPath, "DungeonEncounter.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sDurabilityCostsStore,        dbcPath, "DurabilityCosts.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sDurabilityQualityStore,      dbcPath, "DurabilityQuality.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sEmotesStore,                 dbcPath, "Emotes.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sEmotesTextStore,             dbcPath, "EmotesText.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sFactionStore,                dbcPath, "Faction.dbc");//14545
    for (uint32 i=0; i<sFactionStore.GetNumRows(); ++i)
    {
        FactionEntry const* faction = sFactionStore.LookupEntry(i);
        if (faction && faction->team)
        {
            SimpleFactionsList &flist = sFactionTeamMap[faction->team];
            flist.push_back(i);
        }
    }

    LoadDBC(availableDbcLocales, bad_dbc_files, sFactionTemplateStore,        dbcPath, "FactionTemplate.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sGameObjectDisplayInfoStore,  dbcPath, "GameObjectDisplayInfo.dbc");//14545
    for (uint32 i = 0; i < sGameObjectDisplayInfoStore.GetNumRows(); ++i)
    {
        if (GameObjectDisplayInfoEntry const* info = sGameObjectDisplayInfoStore.LookupEntry(i))
        {
            if (info->maxX < info->minX)
                std::swap(*(float*)(&info->maxX), *(float*)(&info->minX));
            if (info->maxY < info->minY)
                std::swap(*(float*)(&info->maxY), *(float*)(&info->minY));
            if (info->maxZ < info->minZ)
                std::swap(*(float*)(&info->maxZ), *(float*)(&info->minZ));
        }
    }

    LoadDBC(availableDbcLocales, bad_dbc_files, sGemPropertiesStore,          dbcPath, "GemProperties.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sGlyphPropertiesStore,        dbcPath, "GlyphProperties.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sGlyphSlotStore,              dbcPath, "GlyphSlot.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sGtBarberShopCostBaseStore,   dbcPath, "gtBarberShopCostBase.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sGtCombatRatingsStore,        dbcPath, "gtCombatRatings.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sGtChanceToMeleeCritBaseStore, dbcPath, "gtChanceToMeleeCritBase.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sGtChanceToMeleeCritStore,    dbcPath, "gtChanceToMeleeCrit.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sGtChanceToSpellCritBaseStore, dbcPath, "gtChanceToSpellCritBase.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sGtChanceToSpellCritStore,    dbcPath, "gtChanceToSpellCrit.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sGtNPCManaCostScalerStore,    dbcPath, "gtNPCManaCostScaler.dbc");
    LoadDBC(availableDbcLocales, bad_dbc_files, sGtOCTClassCombatRatingScalarStore,    dbcPath, "gtOCTClassCombatRatingScalar.dbc");//14545
    //LoadDBC(availableDbcLocales, bad_dbc_files, sGtOCTRegenHPStore,           dbcPath, "gtOCTRegenHP.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sGtOCTHpPerStaminaStore,      dbcPath, "gtOCTHpPerStamina.dbc");//14545
    //LoadDBC(dbcCount, availableDbcLocales, bad_dbc_files, sGtOCTRegenMPStore,           dbcPath, "gtOCTRegenMP.dbc");       -- not used currently
    LoadDBC(availableDbcLocales, bad_dbc_files, sGtRegenMPPerSptStore,        dbcPath, "gtRegenMPPerSpt.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sGtSpellScalingStore,        dbcPath, "gtSpellScaling.dbc");//15595
    LoadDBC(availableDbcLocales, bad_dbc_files, sGtOCTBaseHPByClassStore,        dbcPath, "gtOCTBaseHPByClass.dbc");//15595
    LoadDBC(availableDbcLocales, bad_dbc_files, sGtOCTBaseMPByClassStore,        dbcPath, "gtOCTBaseMPByClass.dbc");//15595
    LoadDBC(availableDbcLocales, bad_dbc_files, sGuildPerkSpellsStore,        dbcPath, "GuildPerkSpells.dbc");//15595

    LoadDBC(availableDbcLocales, bad_dbc_files, sHolidaysStore,               dbcPath, "Holidays.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sImportPriceArmorStore,       dbcPath, "ImportPriceArmor.dbc"); // 15595
    LoadDBC(availableDbcLocales, bad_dbc_files, sImportPriceQualityStore,     dbcPath, "ImportPriceQuality.dbc"); // 15595
    LoadDBC(availableDbcLocales, bad_dbc_files, sImportPriceShieldStore,      dbcPath, "ImportPriceShield.dbc"); // 15595
    LoadDBC(availableDbcLocales, bad_dbc_files, sImportPriceWeaponStore,      dbcPath, "ImportPriceWeapon.dbc"); // 15595
    LoadDBC(availableDbcLocales, bad_dbc_files, sItemPriceBaseStore,          dbcPath, "ItemPriceBase.dbc"); // 15595
    LoadDBC(availableDbcLocales, bad_dbc_files, sItemReforgeStore,            dbcPath, "ItemReforge.dbc"); // 15595
    LoadDBC(availableDbcLocales, bad_dbc_files, sItemBagFamilyStore,          dbcPath, "ItemBagFamily.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sItemClassStore,              dbcPath, "ItemClass.dbc"); // 15595
    //LoadDBC(dbcCount, availableDbcLocales, bad_dbc_files, sItemDisplayInfoStore,        dbcPath, "ItemDisplayInfo.dbc");     -- not used currently
    LoadDBC(availableDbcLocales, bad_dbc_files, sItemLimitCategoryStore,      dbcPath, "ItemLimitCategory.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sItemRandomPropertiesStore,   dbcPath, "ItemRandomProperties.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sItemRandomSuffixStore,       dbcPath, "ItemRandomSuffix.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sItemSetStore,                dbcPath, "ItemSet.dbc");//14545

    LoadDBC(availableDbcLocales, bad_dbc_files, sItemArmorQualityStore,       dbcPath, "ItemArmorQuality.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sItemArmorShieldStore,        dbcPath, "ItemArmorShield.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sItemArmorTotalStore,         dbcPath, "ItemArmorTotal.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sItemDamageAmmoStore,         dbcPath, "ItemDamageAmmo.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sItemDamageOneHandStore,      dbcPath, "ItemDamageOneHand.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sItemDamageOneHandCasterStore, dbcPath, "ItemDamageOneHandCaster.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sItemDamageRangedStore,       dbcPath, "ItemDamageRanged.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sItemDamageThrownStore,       dbcPath, "ItemDamageThrown.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sItemDamageTwoHandStore,      dbcPath, "ItemDamageTwoHand.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sItemDamageTwoHandCasterStore, dbcPath, "ItemDamageTwoHandCaster.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sItemDamageWandStore,         dbcPath, "ItemDamageWand.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sItemDisenchantLootStore,     dbcPath, "ItemDisenchantLoot.dbc");

    LoadDBC(availableDbcLocales, bad_dbc_files, sLFGDungeonStore,             dbcPath, "LFGDungeons.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sLiquidTypeStore,             dbcPath, "LiquidType.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sLockStore,                   dbcPath, "Lock.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sPhaseStores,                 dbcPath, "Phase.dbc");//14545

    LoadDBC(availableDbcLocales, bad_dbc_files, sMailTemplateStore,           dbcPath, "MailTemplate.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sMapStore,                    dbcPath, "Map.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sMapDifficultyStore,          dbcPath, "MapDifficulty.dbc");//14545
    // fill data
    sMapDifficultyMap[MAKE_PAIR32(0, 0)] = MapDifficulty(0, 0, false, "");//map 0 is missingg from MapDifficulty.dbc use this till its ported to sql
    for (uint32 i = 0; i < sMapDifficultyStore.GetNumRows(); ++i)
        if (MapDifficultyEntry const* entry = sMapDifficultyStore.LookupEntry(i))
            sMapDifficultyMap[MAKE_PAIR32(entry->MapId, entry->Difficulty)] = MapDifficulty(entry->resetTime, entry->maxPlayers, entry->areaTriggerText[0] > 0, entry->areaTriggerText);
    sMapDifficultyStore.Clear();

    LoadDBC(availableDbcLocales, bad_dbc_files, sMountCapabilityStore,        dbcPath, "MountCapability.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sMountTypeStore,              dbcPath, "MountType.dbc");//14545

    LoadDBC(availableDbcLocales, bad_dbc_files, sNameGenStore,                dbcPath, "NameGen.dbc");//14545
    for (uint32 i = 0; i < sNameGenStore.GetNumRows(); ++i)
        if (NameGenEntry const* entry = sNameGenStore.LookupEntry(i))
            sGenNameVectoArraysMap[entry->race].stringVectorArray[entry->gender].push_back(std::string(entry->name));
    sNameGenStore.Clear();
    LoadDBC(availableDbcLocales, bad_dbc_files, sNumTalentsAtLevelStore,      dbcPath, "NumTalentsAtLevel.dbc");//14545

    LoadDBC(availableDbcLocales, bad_dbc_files, sMovieStore,                  dbcPath, "Movie.dbc");//14545

    LoadDBC(availableDbcLocales, bad_dbc_files, sOverrideSpellDataStore,      dbcPath, "OverrideSpellData.dbc");//14545

    LoadDBC(availableDbcLocales, bad_dbc_files, sPvPDifficultyStore,          dbcPath, "PvpDifficulty.dbc");//14545
    for (uint32 i = 0; i < sPvPDifficultyStore.GetNumRows(); ++i)
        if (PvPDifficultyEntry const* entry = sPvPDifficultyStore.LookupEntry(i))
            if (entry->bracketId > MAX_BATTLEGROUND_BRACKETS)
                ASSERT(false && "Need update MAX_BATTLEGROUND_BRACKETS by DBC data");

    LoadDBC(availableDbcLocales, bad_dbc_files, sQuestXPStore,                dbcPath, "QuestXP.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sQuestFactionRewardStore,     dbcPath, "QuestFactionReward.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sQuestSortStore,              dbcPath, "QuestSort.dbc");//14545

    LoadDBC(availableDbcLocales, bad_dbc_files, sRandomPropertiesPointsStore, dbcPath, "RandPropPoints.dbc");//14545

    LoadDBC(availableDbcLocales, bad_dbc_files, sResearchSiteStore,        dbcPath, "ResearchSite.dbc");
    LoadDBC(availableDbcLocales, bad_dbc_files, sResearchProjectStore,     dbcPath, "ResearchProject.dbc");
    LoadDBC(availableDbcLocales, bad_dbc_files, sResearchSitePoints,       dbcPath, "QuestPOIPoint.dbc");

    LoadDBC(availableDbcLocales, bad_dbc_files, sScalingStatDistributionStore, dbcPath, "ScalingStatDistribution.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sScalingStatValuesStore,      dbcPath, "ScalingStatValues.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSkillLineStore,              dbcPath, "SkillLine.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSkillLineAbilityStore,       dbcPath, "SkillLineAbility.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSoundEntriesStore,           dbcPath, "SoundEntries.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellStore,                  dbcPath, "Spell.dbc", false, &CustomSpellEntryfmt, &CustomSpellEntryIndex);//15595
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellCategoriesStore,        dbcPath, "SpellCategories.dbc");//14545
    for (uint32 i = 1; i < sSpellStore.GetNumRows(); ++i)
    {
        SpellEntry const* spell = sSpellStore.LookupEntry(i);
        if (!spell)
            continue;

        if (SpellCategoriesEntry const* category = sSpellCategoriesStore.LookupEntry(spell->SpellCategoriesId))
            sSpellCategoryStore[category->Category].insert(i);
    }
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellReagentsStore,          dbcPath,"SpellReagents.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellScalingStore,           dbcPath,"SpellScaling.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellTotemsStore,            dbcPath,"SpellTotems.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellTargetRestrictionsStore, dbcPath,"SpellTargetRestrictions.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellPowerStore,             dbcPath,"SpellPower.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellLevelsStore,            dbcPath,"SpellLevels.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellInterruptsStore,        dbcPath,"SpellInterrupts.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellEquippedItemsStore,     dbcPath,"SpellEquippedItems.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellClassOptionsStore,      dbcPath,"SpellClassOptions.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellCooldownsStore,         dbcPath,"SpellCooldowns.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellAuraOptionsStore,       dbcPath,"SpellAuraOptions.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellAuraRestrictionsStore,  dbcPath,"SpellAuraRestrictions.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellCastingRequirementsStore, dbcPath,"SpellCastingRequirements.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellEffectStore,            dbcPath,"SpellEffect.dbc", false, &CustomSpellEffectEntryfmt, &CustomSpellEffectEntryIndex);//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellCastTimesStore,         dbcPath, "SpellCastTimes.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellDifficultyStore,        dbcPath, "SpellDifficulty.dbc", false, &CustomSpellDifficultyfmt, &CustomSpellDifficultyIndex);//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellDurationStore,          dbcPath, "SpellDuration.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellFocusObjectStore,       dbcPath, "SpellFocusObject.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellItemEnchantmentStore,   dbcPath, "SpellItemEnchantment.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellItemEnchantmentConditionStore, dbcPath, "SpellItemEnchantmentCondition.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellRadiusStore,            dbcPath, "SpellRadius.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellRangeStore,             dbcPath, "SpellRange.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellRuneCostStore,          dbcPath, "SpellRuneCost.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellShapeshiftStore,        dbcPath, "SpellShapeshift.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellShapeshiftFormStore,    dbcPath, "SpellShapeshiftForm.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sSpellVisualStore,            dbcPath, "SpellVisual.dbc");//14545
    //LoadDBC(availableDbcLocales, bad_dbc_files, sStableSlotPricesStore,       dbcPath, "StableSlotPrices.dbc");
    LoadDBC(availableDbcLocales, bad_dbc_files, sSummonPropertiesStore,       dbcPath, "SummonProperties.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sTransportAnimationStore,     dbcPath, "TransportAnimation.dbc");
    for (uint32 i = 0; i < sTransportAnimationStore.GetNumRows(); ++i)
    {
        TransportAnimationEntry const* anim = sTransportAnimationStore.LookupEntry(i);
        if (!anim)
            continue;

        TransportAnimation& animNode = sTransportAnimationContainer[anim->TransportEntry];
        if (animNode.TotalTime < anim->TimeSeg)
            animNode.TotalTime = anim->TimeSeg;

        animNode.Path[anim->TimeSeg] = anim;
    }

    LoadDBC(availableDbcLocales, bad_dbc_files, sTransportRotationStore,      dbcPath, "TransportRotation.dbc");
    for (uint32 i = 0; i < sTransportRotationStore.GetNumRows(); ++i)
    {
        TransportRotationEntry const* rot = sTransportRotationStore.LookupEntry(i);
        if (!rot)
            continue;

        sTransportAnimationContainer[rot->TransportEntry].Rotations[rot->TimeSeg] = rot;
    }

    for (uint32 i = 0; i < sSpellVisualStore.GetNumRows(); ++i)
        if (SpellVisualEntry const* visual = sSpellVisualStore.LookupEntry(i))
            sSpellVisualMap[visual->ID] = visual->alternativeVisual;

    // Must be done when sSkillLineAbilityStore, sSpellStore, sSpellLevelsStore and sCreatureFamilyStore are all loaded
    for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
    {
        SkillLineAbilityEntry const* skillLine = sSkillLineAbilityStore.LookupEntry(j);
        if (!skillLine)
            continue;

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(skillLine->spellId);
        if (!spellInfo)
            continue;

        SpellLevelsEntry const* levels = sSpellLevelsStore.LookupEntry(spellInfo->SpellLevelsId);
        if (spellInfo->SpellLevelsId && (!levels || levels->spellLevel))
            continue;

        if (spellInfo && spellInfo->Attributes & SPELL_ATTR0_PASSIVE)
        {
            for (uint32 i = 1; i < sCreatureFamilyStore.GetNumRows(); ++i)
            {
                CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(i);
                if (!cFamily)
                    continue;

                if (skillLine->skillId != cFamily->skillLine[0] && skillLine->skillId != cFamily->skillLine[1])
                    continue;

                if (skillLine->learnOnGetSkill != ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL)
                    continue;

                sPetFamilySpellsStore[i].insert(spellInfo->Id);
            }
        }
    }

    LoadDBC(availableDbcLocales, bad_dbc_files, sTalentStore,                 dbcPath, "Talent.dbc");//14545

    // Create Spelldifficulty searcher
    for (uint32 i = 0; i < sSpellDifficultyStore.GetNumRows(); ++i)
    {
        SpellDifficultyEntry const* spellDiff = sSpellDifficultyStore.LookupEntry(i);
        if (!spellDiff)
            continue;

        SpellDifficultyEntry newEntry;
        memset(newEntry.SpellID, 0, 4*sizeof(uint32));
        for (uint32 x = 0; x < MAX_DIFFICULTY; ++x)
        {
            if (spellDiff->SpellID[x] <= 0 || !sSpellStore.LookupEntry(spellDiff->SpellID[x]))
            {
                if (spellDiff->SpellID[x] > 0)//don't show error if spell is <= 0, not all modes have spells and there are unknown negative values
                    TC_LOG_ERROR("sql.sql", "spelldifficulty_dbc: spell %i at field id:%u at spellid%i does not exist in SpellStore (spell.dbc), loaded as 0", spellDiff->SpellID[x], spellDiff->ID, x);
                newEntry.SpellID[x] = 0;//spell was <= 0 or invalid, set to 0
            }
            else
                newEntry.SpellID[x] = spellDiff->SpellID[x];
        }
        if (newEntry.SpellID[0] <= 0 || newEntry.SpellID[1] <= 0)//id0-1 must be always set!
            continue;

        for (uint32 x = 0; x < MAX_DIFFICULTY; ++x)
            sSpellMgr->SetSpellDifficultyId(uint32(newEntry.SpellID[x]), spellDiff->ID);
    }

    // create talent spells set
    for (unsigned int i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);
        if (!talentInfo)
            continue;

        for (int j = 0; j < MAX_TALENT_RANK; j++)
            if (talentInfo->RankID[j])
                sTalentSpellPosMap[talentInfo->RankID[j]] = TalentSpellPos(i, j);
    }

    LoadDBC(availableDbcLocales, bad_dbc_files, sTalentTabStore,              dbcPath, "TalentTab.dbc");//14545

    // prepare fast data access to bit pos of talent ranks for use at inspecting
    {
        // now have all max ranks (and then bit amount used for store talent ranks in inspect)
        for (uint32 talentTabId = 1; talentTabId < sTalentTabStore.GetNumRows(); ++talentTabId)
        {
            TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentTabId);
            if (!talentTabInfo)
                continue;

            // prevent memory corruption; otherwise cls will become 12 below
            if ((talentTabInfo->ClassMask & CLASSMASK_ALL_PLAYABLE) == 0)
                continue;

            // store class talent tab pages
            for (uint32 cls = 1; cls < MAX_CLASSES; ++cls)
                if (talentTabInfo->ClassMask & (1 << (cls - 1)))
                    sTalentTabPages[cls][talentTabInfo->tabpage] = talentTabId;
        }
    }

    LoadDBC(availableDbcLocales, bad_dbc_files, sTalentTreePrimarySpellsStore, dbcPath, "TalentTreePrimarySpells.dbc");
    for (uint32 i = 0; i < sTalentTreePrimarySpellsStore.GetNumRows(); ++i)
        if (TalentTreePrimarySpellsEntry const* talentSpell = sTalentTreePrimarySpellsStore.LookupEntry(i))
            sTalentTreePrimarySpellsMap[talentSpell->TalentTree].push_back(talentSpell->SpellId);
    sTalentTreePrimarySpellsStore.Clear();

    LoadDBC(availableDbcLocales, bad_dbc_files, sTaxiNodesStore,              dbcPath, "TaxiNodes.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sTaxiPathStore,               dbcPath, "TaxiPath.dbc");//14545
    for (uint32 i = 1; i < sTaxiPathStore.GetNumRows(); ++i)
        if (TaxiPathEntry const* entry = sTaxiPathStore.LookupEntry(i))
            sTaxiPathSetBySource[entry->from][entry->to] = TaxiPathBySourceAndDestination(entry->ID, entry->price);
    uint32 pathCount = sTaxiPathStore.GetNumRows();

    //## TaxiPathNode.dbc ## Loaded only for initialization different structures
    LoadDBC(availableDbcLocales, bad_dbc_files, sTaxiPathNodeStore,           dbcPath, "TaxiPathNode.dbc");//14545
    // Calculate path nodes count
    std::vector<uint32> pathLength;
    pathLength.resize(pathCount);                           // 0 and some other indexes not used
    for (uint32 i = 1; i < sTaxiPathNodeStore.GetNumRows(); ++i)
        if (TaxiPathNodeEntry const* entry = sTaxiPathNodeStore.LookupEntry(i))
        {
            if (pathLength[entry->path] < entry->index + 1)
                pathLength[entry->path] = entry->index + 1;
        }
    // Set path length
    sTaxiPathNodesByPath.resize(pathCount);                 // 0 and some other indexes not used
    for (uint32 i = 1; i < sTaxiPathNodesByPath.size(); ++i)
        sTaxiPathNodesByPath[i].resize(pathLength[i]);
    // fill data
    for (uint32 i = 1; i < sTaxiPathNodeStore.GetNumRows(); ++i)
        if (TaxiPathNodeEntry const* entry = sTaxiPathNodeStore.LookupEntry(i))
            sTaxiPathNodesByPath[entry->path].set(entry->index, entry);

    // Initialize global taxinodes mask
    // include existed nodes that have at least single not spell base (scripted) path
    {
        std::set<uint32> spellPaths;
        for (uint32 i = 1; i < sSpellEffectStore.GetNumRows(); ++i)
            if (SpellEffectEntry const* sInfo = sSpellEffectStore.LookupEntry (i))
                if (sInfo->Effect == SPELL_EFFECT_SEND_TAXI)
                    spellPaths.insert(sInfo->EffectMiscValue);

        memset(sTaxiNodesMask, 0, sizeof(sTaxiNodesMask));
        memset(sOldContinentsNodesMask, 0, sizeof(sOldContinentsNodesMask));
        memset(sHordeTaxiNodesMask, 0, sizeof(sHordeTaxiNodesMask));
        memset(sAllianceTaxiNodesMask, 0, sizeof(sAllianceTaxiNodesMask));
        memset(sDeathKnightTaxiNodesMask, 0, sizeof(sDeathKnightTaxiNodesMask));
        for (uint32 i = 1; i < sTaxiNodesStore.GetNumRows(); ++i)
        {
            TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(i);
            if (!node)
                continue;

            TaxiPathSetBySource::const_iterator src_i = sTaxiPathSetBySource.find(i);
            if (src_i != sTaxiPathSetBySource.end() && !src_i->second.empty())
            {
                bool ok = false;
                for (TaxiPathSetForSource::const_iterator dest_i = src_i->second.begin(); dest_i != src_i->second.end(); ++dest_i)
                {
                    // not spell path
                    if (dest_i->second.price || spellPaths.find(dest_i->second.ID) == spellPaths.end())
                    {
                        ok = true;
                        break;
                    }
                }

                if (!ok)
                    continue;
            }

            // valid taxi network node
            uint8  field   = (uint8)((i - 1) / 8);
            uint32 submask = 1 << ((i-1) % 8);

            sTaxiNodesMask[field] |= submask;
            if (node->MountCreatureID[0] && node->MountCreatureID[0] != 32981)
                sHordeTaxiNodesMask[field] |= submask;
            if (node->MountCreatureID[1] && node->MountCreatureID[1] != 32981)
                sAllianceTaxiNodesMask[field] |= submask;
            if (node->MountCreatureID[0] == 32981 || node->MountCreatureID[1] == 32981)
                sDeathKnightTaxiNodesMask[field] |= submask;

            // old continent node (+ nodes virtually at old continents, check explicitly to avoid loading map files for zone info)
            if (node->map_id < 2 || i == 82 || i == 83 || i == 93 || i == 94)
                sOldContinentsNodesMask[field] |= submask;

            // fix DK node at Ebon Hold and Shadow Vault flight master
            if (i == 315 || i == 333)
                ((TaxiNodesEntry*)node)->MountCreatureID[1] = 32981;
        }
    }

    //LoadDBC(availableDbcLocales, bad_dbc_files, sTeamContributionPointsStore, dbcPath, "TeamContributionPoints.dbc");
    LoadDBC(availableDbcLocales, bad_dbc_files, sTotemCategoryStore,          dbcPath, "TotemCategory.dbc");//14545

    LoadDBC(availableDbcLocales, bad_dbc_files, sUnitPowerBarStore,           dbcPath, "UnitPowerBar.dbc");//15595

    LoadDBC(availableDbcLocales, bad_dbc_files, sVehicleStore,                dbcPath, "Vehicle.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sVehicleSeatStore,            dbcPath, "VehicleSeat.dbc");//14545

    LoadDBC(availableDbcLocales, bad_dbc_files, sWMOAreaTableStore,           dbcPath, "WMOAreaTable.dbc");//14545
    for (uint32 i = 0; i < sWMOAreaTableStore.GetNumRows(); ++i)
        if (WMOAreaTableEntry const* entry = sWMOAreaTableStore.LookupEntry(i))
            sWMOAreaInfoByTripple.insert(WMOAreaInfoByTripple::value_type(WMOAreaTableTripple(entry->rootId, entry->adtId, entry->groupId), entry));
    LoadDBC(availableDbcLocales, bad_dbc_files, sWorldMapAreaStore,           dbcPath, "WorldMapArea.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sWorldMapOverlayStore,        dbcPath, "WorldMapOverlay.dbc");//14545
    LoadDBC(availableDbcLocales, bad_dbc_files, sWorldSafeLocsStore,          dbcPath, "WorldSafeLocs.dbc", false, &CustomWorldSafeLocsEntryfmt, &CustomWorldSafeLocsEntryIndex);

    // error checks
    if (bad_dbc_files.size() >= DBCFileCount)
    {
        TC_LOG_ERROR("misc", "Incorrect DataDir value in worldserver.conf or ALL required *.dbc files (%d) not found by path: %sdbc", DBCFileCount, dataPath.c_str());
        exit(1);
    }
    else if (!bad_dbc_files.empty())
    {
        std::string str;
        for (StoreProblemList::iterator i = bad_dbc_files.begin(); i != bad_dbc_files.end(); ++i)
            str += *i + "\n";

        TC_LOG_ERROR("misc", "Some required *.dbc files (%u from %d) not found or not compatible:\n%s", (uint32)bad_dbc_files.size(), DBCFileCount, str.c_str());
        exit(1);
    }

    // Check loaded DBC files proper version
    if (!sAreaStore.LookupEntry(4713)          ||     // last area (areaflag) added in 4.3.4 (15595)
        !sCharTitlesStore.LookupEntry(287)     ||     // last char title added in 4.3.4 (15595)
        !sGemPropertiesStore.LookupEntry(2250) ||     // last gem property added in 4.3.4 (15595)
        !sMapStore.LookupEntry(980)            ||     // last map added in 4.3.4 (15595)
        !sSpellStore.LookupEntry(121820)       )      // last spell added in 4.3.4 (15595)
    {
        TC_LOG_ERROR("misc", "You have _outdated_ DBC files. Please extract correct versions from current using client.");
        exit(1);
    }

    TC_LOG_INFO("server.loading", ">> Initialized %d DBC data stores in %u ms", DBCFileCount, GetMSTimeDiffToNow(oldMSTime));
}

const std::string* GetRandomCharacterName(uint8 race, uint8 gender)
{
    uint32 size = sGenNameVectoArraysMap[race].stringVectorArray[gender].size();
    uint32 randPos = urand(0, size-1);

    return &sGenNameVectoArraysMap[race].stringVectorArray[gender][randPos];
}

SimpleFactionsList const* GetFactionTeamList(uint32 faction)
{
    FactionTeamMap::const_iterator itr = sFactionTeamMap.find(faction);
    if (itr != sFactionTeamMap.end())
        return &itr->second;

    return NULL;
}

char const* GetPetName(uint32 petfamily, uint32 /*dbclang*/)
{
    if (!petfamily)
        return NULL;
    CreatureFamilyEntry const* pet_family = sCreatureFamilyStore.LookupEntry(petfamily);
    if (!pet_family)
        return NULL;
    return pet_family->Name ? pet_family->Name : NULL;
}

TalentSpellPos const* GetTalentSpellPos(uint32 spellId)
{
    TalentSpellPosMap::const_iterator itr = sTalentSpellPosMap.find(spellId);
    if (itr == sTalentSpellPosMap.end())
        return NULL;

    return &itr->second;
}

uint32 GetTalentSpellCost(uint32 spellId)
{
    if (TalentSpellPos const* pos = GetTalentSpellPos(spellId))
        return pos->rank+1;

    return 0;
}

int32 GetAreaFlagByAreaID(uint32 area_id)
{
    AreaFlagByAreaID::iterator i = sAreaFlagByAreaID.find(area_id);
    if (i == sAreaFlagByAreaID.end())
        return -1;

    return i->second;
}

WMOAreaTableEntry const* GetWMOAreaTableEntryByTripple(int32 rootid, int32 adtid, int32 groupid)
{
    WMOAreaInfoByTripple::iterator i = sWMOAreaInfoByTripple.find(WMOAreaTableTripple(rootid, adtid, groupid));
        if (i == sWMOAreaInfoByTripple.end())
            return NULL;
        return i->second;
}

AreaTableEntry const* GetAreaEntryByAreaID(uint32 area_id)
{
    int32 areaflag = GetAreaFlagByAreaID(area_id);
    if (areaflag < 0)
        return NULL;

    return sAreaStore.LookupEntry(areaflag);
}

AreaTableEntry const* GetAreaEntryByAreaFlagAndMap(uint32 area_flag, uint32 map_id)
{
    if (area_flag)
        return sAreaStore.LookupEntry(area_flag);

    if (MapEntry const* mapEntry = sMapStore.LookupEntry(map_id))
        return GetAreaEntryByAreaID(mapEntry->linked_zone);

    return NULL;
}

uint32 GetAreaFlagByMapId(uint32 mapid)
{
    AreaFlagByMapID::iterator i = sAreaFlagByMapID.find(mapid);
    if (i == sAreaFlagByMapID.end())
        return 0;
    else
        return i->second;
}

uint32 GetVirtualMapForMapAndZone(uint32 mapid, uint32 zoneId)
{
    if (mapid != 530 && mapid != 571 && mapid != 732)   // speed for most cases
        return mapid;

    if (WorldMapAreaEntry const* wma = sWorldMapAreaStore.LookupEntry(zoneId))
        return wma->virtual_map_id >= 0 ? wma->virtual_map_id : wma->map_id;

    return mapid;
}

uint32 GetMaxLevelForExpansion(uint32 expansion)
{
    switch (expansion)
    {
        case CONTENT_1_60:
            return 60;
        case CONTENT_61_70:
            return 70;
        case CONTENT_71_80:
            return 80;
        case CONTENT_81_85:
            return 85;
        default:
            break;
    }
    return 0;
}

/*
Used only for calculate xp gain by content lvl.
Calculation on Gilneas and group maps of LostIslands calculated as CONTENT_1_60.
*/
ContentLevels GetContentLevelsForMapAndZone(uint32 mapid, uint32 zoneId)
{
    mapid = GetVirtualMapForMapAndZone(mapid, zoneId);
    if (mapid < 2)
        return CONTENT_1_60;

    MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);
    if (!mapEntry)
        return CONTENT_1_60;

    // no need enum all maps from phasing
    if (mapEntry->rootPhaseMap >= 0)
        mapid = mapEntry->rootPhaseMap;

    switch (mapid)
    {
        case 648:   //LostIslands
        case 654:   //Gilneas
            return CONTENT_1_60;
        default:
            return ContentLevels(mapEntry->Expansion());
    }
}

bool IsTotemCategoryCompatiableWith(uint32 itemTotemCategoryId, uint32 requiredTotemCategoryId)
{
    if (requiredTotemCategoryId == 0)
        return true;
    if (itemTotemCategoryId == 0)
        return false;

    TotemCategoryEntry const* itemEntry = sTotemCategoryStore.LookupEntry(itemTotemCategoryId);
    if (!itemEntry)
        return false;
    TotemCategoryEntry const* reqEntry = sTotemCategoryStore.LookupEntry(requiredTotemCategoryId);
    if (!reqEntry)
        return false;

    if (itemEntry->categoryType != reqEntry->categoryType)
        return false;

    return (itemEntry->categoryMask & reqEntry->categoryMask) == reqEntry->categoryMask;
}

void Zone2MapCoordinates(float& x, float& y, uint32 zone)
{
    WorldMapAreaEntry const* maEntry = sWorldMapAreaStore.LookupEntry(zone);

    // if not listed then map coordinates (instance)
    if (!maEntry)
        return;

    std::swap(x, y);                                         // at client map coords swapped
    x = x*((maEntry->x2-maEntry->x1)/100)+maEntry->x1;
    y = y*((maEntry->y2-maEntry->y1)/100)+maEntry->y1;      // client y coord from top to down
}

void Map2ZoneCoordinates(float& x, float& y, uint32 zone)
{
    WorldMapAreaEntry const* maEntry = sWorldMapAreaStore.LookupEntry(zone);

    // if not listed then map coordinates (instance)
    if (!maEntry)
        return;

    x = (x-maEntry->x1)/((maEntry->x2-maEntry->x1)/100);
    y = (y-maEntry->y1)/((maEntry->y2-maEntry->y1)/100);    // client y coord from top to down
    std::swap(x, y);                                         // client have map coords swapped
}

bool IsMapDifficultySwitchable(uint32 mapId)
{
    switch (mapId)
    {
        case 967: // dragonsoul
        case 720: // firelands
        case 754: // throne of the four winds
        case 671: // bastion of twilight
        case 669: // blackwingDescent
        case 649: // trial of the crusader
        case 631: // icecrown citadel
        case 724: // rubis sanctum
            return true;
    }
    return false;
}

uint32 GetMapDifficultySwitchAchievement(uint32 mapId, Difficulty difficulty)
{
    switch (mapId)
    {
        case 967: // dragonsoul
            if (difficulty == RAID_DIFFICULTY_10MAN_HEROIC || difficulty == RAID_DIFFICULTY_25MAN_HEROIC)
                return 6106;
        case 720: // firelands
            if (difficulty == RAID_DIFFICULTY_10MAN_HEROIC || difficulty == RAID_DIFFICULTY_25MAN_HEROIC)
                return 5802;
        case 754: // throne of the four winds
            if (difficulty == RAID_DIFFICULTY_10MAN_HEROIC || difficulty == RAID_DIFFICULTY_25MAN_HEROIC)
                return 4851;
        case 671: // bastion of twilight
            if (difficulty == RAID_DIFFICULTY_10MAN_HEROIC || difficulty == RAID_DIFFICULTY_25MAN_HEROIC)
                return 4850;
        case 669: // blackwingDescent
            if (difficulty == RAID_DIFFICULTY_10MAN_HEROIC || difficulty == RAID_DIFFICULTY_25MAN_HEROIC)
                return 4842;
        case 649: // trial of the crusader
        {
            if (difficulty == RAID_DIFFICULTY_10MAN_HEROIC)
                return 3917;
            if (difficulty == RAID_DIFFICULTY_25MAN_HEROIC)
                return 3916;
        }
        case 631: // icecrown citadel
        {
            if (difficulty == RAID_DIFFICULTY_10MAN_HEROIC)
                return 4532;
            if (difficulty == RAID_DIFFICULTY_25MAN_HEROIC)
                return 4608;
        }
        case 724: // rubis sanctum
        {
            if (difficulty == RAID_DIFFICULTY_10MAN_HEROIC)
                return 4817;
            if (difficulty == RAID_DIFFICULTY_25MAN_HEROIC)
                return 4815;
        }
    }
    return 0;
}

MapDifficulty const* GetMapDifficultyData(uint32 mapId, Difficulty difficulty)
{
    MapDifficultyMap::const_iterator itr = sMapDifficultyMap.find(MAKE_PAIR32(mapId, difficulty));
    return itr != sMapDifficultyMap.end() ? &itr->second : NULL;
}

MapDifficulty const* GetDownscaledMapDifficultyData(uint32 mapId, Difficulty &difficulty)
{
    uint32 tmpDiff = difficulty;
    MapDifficulty const* mapDiff = GetMapDifficultyData(mapId, Difficulty(tmpDiff));
    if (!mapDiff)
    {
        if (tmpDiff > RAID_DIFFICULTY_25MAN_NORMAL) // heroic, downscale to normal
            tmpDiff -= 2;
        else
            tmpDiff -= 1;   // any non-normal mode for raids like tbc (only one mode)

        // pull new data
        mapDiff = GetMapDifficultyData(mapId, Difficulty(tmpDiff)); // we are 10 normal or 25 normal
        if (!mapDiff)
        {
            tmpDiff -= 1;
            mapDiff = GetMapDifficultyData(mapId, Difficulty(tmpDiff)); // 10 normal
        }
    }

    difficulty = Difficulty(tmpDiff);
    return mapDiff;
}

WorldMapAreaEntry const* GetMapAreaEntryForArea(uint32 area_id)
{
        for (uint32 i = 0; i < sWorldMapAreaStore.GetNumRows(); ++i)
            if (WorldMapAreaEntry const* entry = sWorldMapAreaStore.LookupEntry(i))
                if (entry->area_id == area_id)
                    return entry;
        return NULL;
}

PvPDifficultyEntry const* GetBattlegroundBracketByLevel(uint32 mapid, uint32 level)
{
    PvPDifficultyEntry const* maxEntry = NULL;              // used for level > max listed level case
    for (uint32 i = 0; i < sPvPDifficultyStore.GetNumRows(); ++i)
    {
        if (PvPDifficultyEntry const* entry = sPvPDifficultyStore.LookupEntry(i))
        {
            // skip unrelated and too-high brackets
            if (entry->mapId != mapid || entry->minLevel > level)
                continue;

            // exactly fit
            if (entry->maxLevel >= level)
                return entry;

            // remember for possible out-of-range case (search higher from existed)
            if (!maxEntry || maxEntry->maxLevel < entry->maxLevel)
                maxEntry = entry;
        }
    }

    return maxEntry;
}

PvPDifficultyEntry const* GetBattlegroundBracketById(uint32 mapid, BattlegroundBracketId id)
{
    for (uint32 i = 0; i < sPvPDifficultyStore.GetNumRows(); ++i)
        if (PvPDifficultyEntry const* entry = sPvPDifficultyStore.LookupEntry(i))
            if (entry->mapId == mapid && entry->GetBracketId() == id)
                return entry;

    return NULL;
}

uint32 const* GetTalentTabPages(uint8 cls)
{
    return sTalentTabPages[cls];
}

std::vector<uint32> const* GetTalentTreePrimarySpells(uint32 talentTree)
{
    TalentTreePrimarySpellsMap::const_iterator itr = sTalentTreePrimarySpellsMap.find(talentTree);
    if (itr == sTalentTreePrimarySpellsMap.end())
        return NULL;

    return &itr->second;
}

uint32 GetAlternativeVisual(uint32 visualId)
{
    SpellVisualMap::const_iterator itr = sSpellVisualMap.find(visualId);
    if (itr == sSpellVisualMap.end())
        return 0;

    return itr->second;
}

uint32 GetLiquidFlags(uint32 liquidType)
{
    if (LiquidTypeEntry const* liq = sLiquidTypeStore.LookupEntry(liquidType))
        return 1 << liq->Type;

    return 0;
}

CharStartOutfitEntry const* GetCharStartOutfitEntry(uint8 race, uint8 class_, uint8 gender)
{
    std::map<uint32, CharStartOutfitEntry const*>::const_iterator itr = sCharStartOutfitMap.find(race | (class_ << 8) | (gender << 16));
    if (itr == sCharStartOutfitMap.end())
        return NULL;

    return itr->second;
}
uint32 GetPowerIndexByClass(uint32 powerType, uint32 classId)
{
    return PowersByClass[classId][powerType];
}

uint32 ScalingStatValuesEntry::GetStatMultiplier(uint32 inventoryType) const
{
    if (inventoryType < MAX_INVTYPE)
    {
        switch (inventoryType)
        {
            case INVTYPE_NON_EQUIP:
            case INVTYPE_BODY:
            case INVTYPE_BAG:
            case INVTYPE_TABARD:
            case INVTYPE_AMMO:
            case INVTYPE_QUIVER:
                return 0;
            case INVTYPE_HEAD:
            case INVTYPE_CHEST:
            case INVTYPE_LEGS:
            case INVTYPE_2HWEAPON:
            case INVTYPE_ROBE:
                return StatMultiplier[0];
            case INVTYPE_SHOULDERS:
            case INVTYPE_WAIST:
            case INVTYPE_FEET:
            case INVTYPE_HANDS:
            case INVTYPE_TRINKET:
                return StatMultiplier[1];
            case INVTYPE_NECK:
            case INVTYPE_WRISTS:
            case INVTYPE_FINGER:
            case INVTYPE_SHIELD:
            case INVTYPE_CLOAK:
            case INVTYPE_HOLDABLE:
                return StatMultiplier[2];
            case INVTYPE_RANGED:
            case INVTYPE_THROWN:
            case INVTYPE_RANGEDRIGHT:
            case INVTYPE_RELIC:
                return StatMultiplier[3];
            case INVTYPE_WEAPON:
            case INVTYPE_WEAPONMAINHAND:
            case INVTYPE_WEAPONOFFHAND:
                return StatMultiplier[4];
            default:
                break;
        }
    }
    return 0;
}

uint32 ScalingStatValuesEntry::GetArmor(uint32 inventoryType, uint32 armorType) const
{
    if (inventoryType <= INVTYPE_ROBE && armorType < 4)
    {
        switch (inventoryType)
        {
            case INVTYPE_NON_EQUIP:
            case INVTYPE_NECK:
            case INVTYPE_BODY:
            case INVTYPE_FINGER:
            case INVTYPE_TRINKET:
            case INVTYPE_WEAPON:
            case INVTYPE_SHIELD:
            case INVTYPE_RANGED:
            case INVTYPE_2HWEAPON:
            case INVTYPE_BAG:
            case INVTYPE_TABARD:
                break;
            case INVTYPE_SHOULDERS:
                return Armor[0][armorType];
            case INVTYPE_CHEST:
            case INVTYPE_ROBE:
                return Armor[1][armorType];
            case INVTYPE_HEAD:
                return Armor[2][armorType];
            case INVTYPE_LEGS:
                return Armor[3][armorType];
            case INVTYPE_FEET:
                return Armor[4][armorType];
            case INVTYPE_WAIST:
                return Armor[5][armorType];
            case INVTYPE_HANDS:
                return Armor[6][armorType];
            case INVTYPE_WRISTS:
                return Armor[7][armorType];
            case INVTYPE_CLOAK:
                return CloakArmor;
            default:
                break;
        }
    }
    return 0;
}

uint32 ScalingStatValuesEntry::GetDPSAndDamageMultiplier(uint32 subClass, bool isCasterWeapon, float* damageMultiplier) const
{
    if (!isCasterWeapon)
    {
        switch (subClass)
        {
            case ITEM_SUBCLASS_WEAPON_AXE:
            case ITEM_SUBCLASS_WEAPON_MACE:
            case ITEM_SUBCLASS_WEAPON_SWORD:
            case ITEM_SUBCLASS_WEAPON_DAGGER:
            case ITEM_SUBCLASS_WEAPON_THROWN:
                *damageMultiplier = 0.3f;
                return dpsMod[0];
            case ITEM_SUBCLASS_WEAPON_AXE2:
            case ITEM_SUBCLASS_WEAPON_MACE2:
            case ITEM_SUBCLASS_WEAPON_POLEARM:
            case ITEM_SUBCLASS_WEAPON_SWORD2:
            case ITEM_SUBCLASS_WEAPON_STAFF:
            case ITEM_SUBCLASS_WEAPON_FISHING_POLE:
                *damageMultiplier = 0.2f;
                return dpsMod[1];
            case ITEM_SUBCLASS_WEAPON_BOW:
            case ITEM_SUBCLASS_WEAPON_GUN:
            case ITEM_SUBCLASS_WEAPON_CROSSBOW:
                *damageMultiplier = 0.3f;
                return dpsMod[4];
            case ITEM_SUBCLASS_WEAPON_Obsolete:
            case ITEM_SUBCLASS_WEAPON_EXOTIC:
            case ITEM_SUBCLASS_WEAPON_EXOTIC2:
            case ITEM_SUBCLASS_WEAPON_FIST_WEAPON:
            case ITEM_SUBCLASS_WEAPON_MISCELLANEOUS:
            case ITEM_SUBCLASS_WEAPON_SPEAR:
            case ITEM_SUBCLASS_WEAPON_WAND:
                break;
        }
    }
    else
    {
        if (subClass <= ITEM_SUBCLASS_WEAPON_WAND)
        {
            uint32 mask = 1 << subClass;
            // two-handed weapons
            if (mask & 0x562)
            {
                *damageMultiplier = 0.2f;
                return dpsMod[3];
            }

            if (mask & (1 << ITEM_SUBCLASS_WEAPON_WAND))
            {
                *damageMultiplier = 0.3f;
                return dpsMod[5];
            }
        }
        *damageMultiplier = 0.3f;
        return dpsMod[2];
    }
    return 0;
}

TransportAnimation const* TransportAnimation::GetTransportAnimation(uint32 TransportEntry)
{
    auto itr = sTransportAnimationContainer.find(TransportEntry);
    if (itr != sTransportAnimationContainer.end())
        return &itr->second;

    TC_LOG_FATAL("sql.sql", "Transport Entry %u could not get animation.", TransportEntry);
    return NULL;
}

TransportAnimationEntry const* TransportAnimation::GetAnimNode(uint32 time) const
{
    if (Path.empty())
    {
        TC_LOG_FATAL("sql.sql", "Transport could not get animation for time %u", time);
        return NULL;
    }

    for (auto itr = Path.rbegin(); itr != Path.rend(); ++itr)
        if (time >= itr->first)
            return itr->second;

    return Path.begin()->second;
}

G3D::Quat TransportAnimation::GetAnimRotation(uint32 time) const
{
    if (Rotations.empty())
        return G3D::Quat(0.0f, 0.0f, 0.0f, 1.0f);

    TransportRotationEntry const* rot = Rotations.begin()->second;
    for (auto itr2 = Rotations.rbegin(); itr2 != Rotations.rend(); ++itr2)
    {
        if (time >= itr2->first)
        {
            rot = itr2->second;
            break;
        }
    }

    return G3D::Quat(rot->X, rot->Y, rot->Z, rot->W);
}

bool TransportAnimation::GetAnimNode(uint32 time, TransportAnimationEntry const*& curr, TransportAnimationEntry const*& next, float& percPos) const
{
    if (Path.empty())
        return false;

    for (auto itr = Path.rbegin(); itr != Path.rend(); ++itr)
        if (time >= itr->first)
        {
            curr = itr->second;

            if (itr == Path.rbegin())
                return false;

            --itr;
            next = itr->second;

            percPos = float(time - curr->TimeSeg) / float(next->TimeSeg - curr->TimeSeg);

            return true;
        }

    return false;
}

CharTitlesEntry const* GetTitleEntryByIndex(uint32 index)
{
    TitleByIndexStore::const_iterator itr = sTitleByIndexStore.find(index);
    if (itr != sTitleByIndexStore.end())
        return itr->second;

    return nullptr;
}