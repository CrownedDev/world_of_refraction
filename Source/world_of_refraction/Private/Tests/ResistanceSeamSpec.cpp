// ResistanceSeamSpec.cpp
// B-216: resistance per damage type gives every buildup resistance the old composition gave
// (UResistanceQueryDebug::GetLegacyTotalStatusResistance), for each class x element x physical
// type, with typed buffs, an element-None buff on a physical hit, gear rolls and two stones.
// Also: the explicit damage-type conversions, the DamageType.* tags, and the profile data asset
// starting from the C++ table.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/CombatConstants.h"
#include "Combat/Resistance/ClassInnateResistanceTable.h"
#include "Combat/Resistance/EWoRDamageType.h"
#include "Combat/Resistance/ResistanceProfileData.h"
#include "Combat/Resistance/ResistanceQuery.h"
#include "Debug/Combat/ResistanceQueryDebug.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Tags/WoRGameplayTags.h"

namespace
{
	constexpr int32 SEAM_TEST_DURATION_TURNS = 3;
	constexpr int32 SEAM_TEST_FIRE_BUFF_ID = 9601;
	constexpr int32 SEAM_TEST_SLASH_DEBUFF_ID = 9602;
	constexpr int32 SEAM_TEST_NONELEMENTAL_BUFF_ID = 9603;
	constexpr int32 SEAM_TEST_CURSE_ID = 9604;
	constexpr float SEAM_TEST_FIRE_BUFF = 20.0f;
	constexpr float SEAM_TEST_SLASH_DEBUFF = 12.0f;
	constexpr float SEAM_TEST_NONELEMENTAL_BUFF = 25.0f;
	constexpr float SEAM_TEST_CURSE = -30.0f;
	constexpr float SEAM_TEST_GEAR_FIRE = 6.0f;
	constexpr float SEAM_TEST_GEAR_WATER = -6.0f;
	constexpr float SEAM_TEST_GEAR_PIERCE = 4.0f;
	constexpr int32 SEAM_TEST_SPIRIT_LEVEL = 5;
	constexpr int32 SEAM_TEST_RESIST_POINTS = 4;
	constexpr int32 SEAM_TEST_FUSION_DURABILITY = 10;
	constexpr float SEAM_TEST_TOLERANCE = 1.0e-5f;

	const ESpellElement SEAM_TEST_CASTER_ELEMENTS[] = {
		ESpellElement::Fire, ESpellElement::Water, ESpellElement::Earth, ESpellElement::Wind, ESpellElement::Light,
		ESpellElement::Darkness, ESpellElement::Lightning, ESpellElement::Void, ESpellElement::Reality};

	FRuntimeAttachedItem SeamResistanceStone()
	{
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::AugmentStone;
		Attached.Crystal = FCrystalAttachment(FCrystalId(ECrystalType::ResistanceStone, EItemTier::F_Tier), 0);
		return Attached;
	}

	/** ResistanceStone + Garnet: the stat-stone half counts on a ring (B-082). */
	FRuntimeAttachedItem SeamRingResistanceFusion()
	{
		FFusionId Id;
		Id.HalfA = FCrystalId(ECrystalType::ResistanceStone, EItemTier::F_Tier);
		Id.HalfB = FCrystalId(ECrystalType::Garnet, EItemTier::F_Tier);
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::Fusion;
		Attached.Fusion = FFusionAttachment(Id, SEAM_TEST_FUSION_DURABILITY);
		return Attached;
	}
}

BEGIN_DEFINE_SPEC(FResistanceSeamSpec, "WoR.Resistance.Seam",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *Effects = nullptr;

	/** A combatant of Class (and innate element) with a weapon, its gear roll and Spirit points. */
	AActor *SpawnFighter(ECharacterClass Class, ESpellElement InnateElement)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = Class;
		Data->InnateElement = InnateElement;
		Data->WorldSpiritLevel = SEAM_TEST_SPIRIT_LEVEL;
		Data->Resistance = SEAM_TEST_RESIST_POINTS;
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, NewObject<UWeaponData>(GetTransientPackage()));
		if (!Loadout)
		{
			return nullptr;
		}
		UInventoryComponent *Inventory = Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>();
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.PrimaryWeapon.WeaponEntry.ResistanceBonus.Fire = SEAM_TEST_GEAR_FIRE;
		Active.PrimaryWeapon.WeaponEntry.ResistanceBonus.Water = SEAM_TEST_GEAR_WATER;
		Active.PrimaryWeapon.WeaponEntry.ResistanceBonus.Pierce = SEAM_TEST_GEAR_PIERCE;
		return Loadout->GetOwner();
	}

	void Give(AActor *Target, int32 ID, ESkillEffectType Type, float Value, ESpellElement Element, EPhysicalDamageType Physical)
	{
		FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(TEXT("Seam test"), ID, Type, Value, SEAM_TEST_DURATION_TURNS);
		Effect.Element = Element;
		Effect.PhysicalType = Physical;
		Effect.bDelayFirstExecution = true;
		Effects->ApplyEffect(Target, Effect, Target);
	}

	/** A Fire buff, a Slash debuff, an element-None buff and a blanket curse. */
	void GiveMixedEffects(AActor *Target)
	{
		Give(Target, SEAM_TEST_FIRE_BUFF_ID, ESkillEffectType::ResistanceBuff, SEAM_TEST_FIRE_BUFF, ESpellElement::Fire, EPhysicalDamageType::None);
		Give(Target, SEAM_TEST_SLASH_DEBUFF_ID, ESkillEffectType::ResistanceDebuff, SEAM_TEST_SLASH_DEBUFF, ESpellElement::None, EPhysicalDamageType::Slash);
		Give(Target, SEAM_TEST_NONELEMENTAL_BUFF_ID, ESkillEffectType::ResistanceBuff, SEAM_TEST_NONELEMENTAL_BUFF, ESpellElement::None, EPhysicalDamageType::None);
		Give(Target, SEAM_TEST_CURSE_ID, ESkillEffectType::ModifyStatusResist, SEAM_TEST_CURSE, ESpellElement::None, EPhysicalDamageType::None);
	}

	void TestSameAsLegacy(const FString &What, AActor *Target)
	{
		TestEqual(FString::Printf(TEXT("%s: hits that differ from the old composition"), *What),
				  UResistanceQueryDebug::CompareActorResistance(Target, true), 0);
	}
END_DEFINE_SPEC(FResistanceSeamSpec)

void FResistanceSeamSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		TestNotNull(TEXT("SkillEffectManager"), Effects);
	});

	AfterEach([this]()
	{
		Effects = nullptr;
		TestWorld.Reset();
	});

	Describe("damage type", [this]()
	{
		It("converts explicitly, never by number", [this]()
		{
			using namespace WoRDamageType;
			TestEqual(TEXT("element None is not Slash"), ElementToDamageType(ESpellElement::None), EWoRDamageType::None);
			TestEqual(TEXT("leftover Generic is non-elemental (Q-216)"), ElementToDamageType(ESpellElement::Generic), EWoRDamageType::None);
			TestEqual(TEXT("physical Slash (0) is Slash"), PhysicalToDamageType(EPhysicalDamageType::Slash), EWoRDamageType::Slash);
			TestEqual(TEXT("uninfused swing"), ToDamageType(ESpellElement::None, EPhysicalDamageType::Slash), EWoRDamageType::Slash);
			TestEqual(TEXT("Fire-infused swing"), ToDamageType(ESpellElement::Fire, EPhysicalDamageType::Slash), EWoRDamageType::Fire);
			TestEqual(TEXT("pure effect"), ToDamageType(ESpellElement::None, EPhysicalDamageType::None), EWoRDamageType::None);
			TestEqual(TEXT("Reality keeps its number"), static_cast<int32>(ElementToDamageType(ESpellElement::Reality)),
					  static_cast<int32>(ESpellElement::Reality));
			TestTrue(TEXT("Impact is physical"), IsPhysical(EWoRDamageType::Impact));
			TestFalse(TEXT("Ice is not physical"), IsPhysical(EWoRDamageType::Ice));
		});

		It("has one DamageType tag per type, physical ones under DamageType.Physical", [this]()
		{
			for (const EWoRDamageType DamageType : WoRDamageType::All())
			{
				const FGameplayTag Tag = WoRTags::DamageTypeToTag(DamageType);
				EWoRDamageType Back = EWoRDamageType::None;
				TestTrue(FString::Printf(TEXT("type %d has a tag"), static_cast<int32>(DamageType)), Tag.IsValid());
				TestTrue(FString::Printf(TEXT("type %d round-trips"), static_cast<int32>(DamageType)),
						 WoRTags::TagToDamageType(Tag, Back) && Back == DamageType);
				TestEqual(FString::Printf(TEXT("type %d under Physical"), static_cast<int32>(DamageType)),
						  Tag.MatchesTag(WoRTags::DamageType_Physical), WoRDamageType::IsPhysical(DamageType));
			}
		});
	});

	It("starts a new profile data asset from the C++ table", [this]()
	{
		const UResistanceProfileData *Asset = NewObject<UResistanceProfileData>(GetTransientPackage());
		const ECharacterClass Classes[] = {ECharacterClass::Generic, ECharacterClass::Caster, ECharacterClass::Resonator};
		for (const ECharacterClass Class : Classes)
		{
			for (const ESpellElement Innate : SEAM_TEST_CASTER_ELEMENTS)
			{
				for (const bool bBrokenDarkness : {false, true})
				{
					const ClassInnateResistanceTable::FResistanceRow A = Asset->ResolveRow(Class, Innate, bBrokenDarkness);
					const ClassInnateResistanceTable::FResistanceRow C = ClassInnateResistanceTable::ResolveRow(Class, Innate, bBrokenDarkness);
					const bool bSame = FMemory::Memcmp(&A, &C, sizeof(A)) == 0;
					TestTrue(FString::Printf(TEXT("class %d innate %d BD %d"), static_cast<int32>(Class), static_cast<int32>(Innate), bBrokenDarkness), bSame);
				}
			}
		}
	});

	Describe("buildup resistance is unchanged", [this]()
	{
		It("for Generic and Resonator fighters with mixed effects and gear", [this]()
		{
			for (const ECharacterClass Class : {ECharacterClass::Generic, ECharacterClass::Resonator})
			{
				AActor *Fighter = SpawnFighter(Class, ESpellElement::Generic);
				if (!TestNotNull(TEXT("fighter"), Fighter))
				{
					return;
				}
				TestSameAsLegacy(TEXT("bare"), Fighter);
				GiveMixedEffects(Fighter);
				TestSameAsLegacy(TEXT("with effects"), Fighter);
			}
		});

		It("for a Caster of every innate element", [this]()
		{
			for (const ESpellElement Innate : SEAM_TEST_CASTER_ELEMENTS)
			{
				AActor *Fighter = SpawnFighter(ECharacterClass::Caster, Innate);
				if (!TestNotNull(TEXT("caster"), Fighter))
				{
					return;
				}
				GiveMixedEffects(Fighter);
				TestSameAsLegacy(FString::Printf(TEXT("Caster innate %d"), static_cast<int32>(Innate)), Fighter);
			}
		});

		It("counts an element-None buff on a physical hit through the non-elemental bucket", [this]()
		{
			AActor *Fighter = SpawnFighter(ECharacterClass::Generic, ESpellElement::Generic);
			if (!TestNotNull(TEXT("fighter"), Fighter))
			{
				return;
			}
			GiveMixedEffects(Fighter);
			const float Bucket = WoRResistance::GetTypeResistance(Fighter, EWoRDamageType::None);
			TestEqual(TEXT("bucket = the element-None buff"), Bucket, SEAM_TEST_NONELEMENTAL_BUFF / CombatConstants::STAT_PERCENT_DIVISOR, SEAM_TEST_TOLERANCE);
			const float Hit = WoRResistance::GetHitTypeResistance(Fighter, ESpellElement::None, EPhysicalDamageType::Slash);
			TestEqual(TEXT("an uninfused Slash hit reads the bucket plus Slash"), Hit,
					  Bucket + WoRResistance::GetTypeResistance(Fighter, EWoRDamageType::Slash), SEAM_TEST_TOLERANCE);
			TestEqual(TEXT("and the total matches the old composition"),
					  WoRResistance::GetTotalStatusResistance(Fighter, ESpellElement::None, EPhysicalDamageType::Slash),
					  UResistanceQueryDebug::GetLegacyTotalStatusResistance(Fighter, ESpellElement::None, EPhysicalDamageType::Slash),
					  SEAM_TEST_TOLERANCE);
			TestEqual(TEXT("a Fire hit doesn't read the bucket"),
					  WoRResistance::GetHitTypeResistance(Fighter, ESpellElement::Fire, EPhysicalDamageType::None),
					  WoRResistance::GetTypeResistance(Fighter, EWoRDamageType::Fire), SEAM_TEST_TOLERANCE);
		});

		It("sums two attached stones before multiplying", [this]()
		{
			AActor *Fighter = SpawnFighter(ECharacterClass::Resonator, ESpellElement::Generic);
			if (!TestNotNull(TEXT("fighter"), Fighter))
			{
				return;
			}
			UInventoryComponent *Inventory = Fighter->FindComponentByClass<UInventoryComponent>();
			ULoadoutComponent *Loadout = Fighter->FindComponentByClass<ULoadoutComponent>();
			FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
			Active.PrimaryWeapon.WeaponEntry.AttachedItem = SeamResistanceStone();
			FRingLoadoutEntry Ring;
			Ring.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
			Ring.RingEntry.AttachedItem = SeamRingResistanceFusion();
			Active.RingLoadout = {Ring};
			Active.ActiveRingIndex = 0;

			const float WeaponStone = CrystalEffectTable::GetAttachedStonePercent(SeamResistanceStone(), ESubStat::Resistance);
			const float RingStone = CrystalEffectTable::GetAttachedStonePercent(SeamRingResistanceFusion(), ESubStat::Resistance);
			TestTrue(TEXT("both stones give resistance"), WeaponStone > 0.0f && RingStone > 0.0f);
			TestEqual(TEXT("both stones count"), Loadout->GetAttachedStonePercent(ESubStat::Resistance), WeaponStone + RingStone, SEAM_TEST_TOLERANCE);

			const UCharacterDataComponent *Char = Fighter->FindComponentByClass<UCharacterDataComponent>();
			const float Capped = Char->GetCrystalResistanceStatCapped();
			TestTrue(TEXT("the fighter has general resistance"), Capped > 0.0f);
			const float GearPoints = Loadout->GetActiveStatBonus(Fighter).BonusResistance;
			const float Expected = FMath::Clamp(Capped * (1.0f + GearPoints * CombatConstants::RESISTANCE_PER_POINT) *
													(1.0f + (WeaponStone + RingStone) / CombatConstants::STAT_PERCENT_DIVISOR),
												0.0f, CombatConstants::RESISTANCE_MAX);
			TestEqual(TEXT("general = capped stat x gear x (1 + summed stones)"), WoRResistance::GetGeneralStatusResistance(Fighter), Expected, SEAM_TEST_TOLERANCE);
			TestSameAsLegacy(TEXT("two stones"), Fighter);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
