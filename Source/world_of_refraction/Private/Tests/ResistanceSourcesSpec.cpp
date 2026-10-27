// ResistanceSourcesSpec.cpp
// GAS stage 3 (B-042w): the resistances on GAS. Each type resistance and StatusResistBonus has a 0 base and one
// UWoRGE_ResistanceSources_* effect per source (typed buffs, class table, gear rolls, boss rising resistance) adds
// its bucket, so the current value equals WoRResistance's sum. General status resistance is on the stat-sources
// shape: the capped Spirit stat as the base, gear points and stones multiplied in, clamped to [0, RESISTANCE_MAX].

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterData.h"
#include "Combat/Boss/BossPhaseComponent.h"
#include "Combat/CombatConstants.h"
#include "Combat/Resistance/EWoRDamageType.h"
#include "Combat/Resistance/ResistanceQuery.h"
#include "Debug/GAS/WoRAttributeDebug.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "GAS/WoRGameplayEffects.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Tags/WoRGameplayTags.h"

namespace
{
	constexpr int32 RESISTANCE_SOURCES_FIRE_BUFF_ID = 4841;
	constexpr int32 RESISTANCE_SOURCES_CURSE_ID = 4842;
	constexpr int32 RESISTANCE_SOURCES_TURNS = 3;
	constexpr float RESISTANCE_SOURCES_FIRE_BUFF = 20.0f;
	constexpr float RESISTANCE_SOURCES_CURSE = -30.0f;
	constexpr float RESISTANCE_SOURCES_GEAR_FIRE = 6.0f;
	constexpr float RESISTANCE_SOURCES_GEAR_FIRE_CHANGED = 10.0f;
	constexpr int32 RESISTANCE_SOURCES_SPIRIT_LEVEL = 5;
	constexpr int32 RESISTANCE_SOURCES_RESIST_POINTS = 4;
	/** Far past the ceiling, so only the attribute set's clamp holds it. */
	constexpr float RESISTANCE_SOURCES_OVER_CEILING_FACTOR = 10.0f;
	constexpr float RESISTANCE_SOURCES_TOLERANCE = 1.0e-5f;
}

BEGIN_DEFINE_SPEC(FResistanceSourcesSpec, "WoR.GAS.ResistanceSources",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AActor *Hero = nullptr;
	UAbilitySystemComponent *ASC = nullptr;
	UWoRAttributeSyncComponent *Sync = nullptr;

	FCombatLoadout *ActiveLoadout() const
	{
		UInventoryComponent *Inventory = Hero->FindComponentByClass<UInventoryComponent>();
		return Inventory && Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex)
				   ? &Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex]
				   : nullptr;
	}

	TArray<FActiveGameplayEffectHandle> SourceEffects(TSubclassOf<UGameplayEffect> SourceClass) const
	{
		FGameplayEffectQuery Query;
		Query.EffectDefinition = SourceClass;
		return ASC->GetActiveEffects(Query);
	}

	void Give(int32 ID, ESkillEffectType Type, float Value, ESpellElement Element) const
	{
		FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(TEXT("Resistance sources test"), ID, Type, Value, RESISTANCE_SOURCES_TURNS);
		Effect.Element = Element;
		Effect.bDelayFirstExecution = true;
		TestWorld->GetSubsystem<USkillEffectManager>()->ApplyEffect(Hero, Effect, Hero);
		Sync->SyncAttributes();
	}

	/** Every resistance attribute equals its getter, over a 0 base, with each source effect applied once. */
	void TestMatchesGetters(const FString &What)
	{
		for (const EWoRDamageType DamageType : WoRDamageType::All())
		{
			const FGameplayAttribute Attribute = UWoRAttributeSet_Resistance::GetTypeAttribute(DamageType);
			const FString Name = What + TEXT(": ") + Attribute.GetName();
			TestEqual(Name + TEXT(" base 0"), ASC->GetNumericAttributeBase(Attribute), 0.0f);
			TestEqual(Name + TEXT(" = getter"), ASC->GetNumericAttribute(Attribute), WoRResistance::GetTypeResistance(Hero, DamageType),
					  RESISTANCE_SOURCES_TOLERANCE);
		}
		const FGameplayAttribute Bonus = UWoRAttributeSet_Resistance::GetStatusResistBonusAttribute();
		TestEqual(What + TEXT(": StatusResistBonus base 0"), ASC->GetNumericAttributeBase(Bonus), 0.0f);
		TestEqual(What + TEXT(": StatusResistBonus = getter"), ASC->GetNumericAttribute(Bonus), WoRResistance::GetStatusResistBonus(Hero),
				  RESISTANCE_SOURCES_TOLERANCE);
		TestEqual(What + TEXT(": StatusResistance = getter"),
				  ASC->GetNumericAttribute(UWoRAttributeSet_Resistance::GetStatusResistanceAttribute()),
				  WoRResistance::GetGeneralStatusResistance(Hero), RESISTANCE_SOURCES_TOLERANCE);
		for (const TSubclassOf<UGameplayEffect> &SourceClass : UWoRGE_ResistanceSources::GetSourceClasses())
		{
			TestEqual(What + TEXT(": one ") + SourceClass->GetName(), SourceEffects(SourceClass).Num(), 1);
		}
		TestEqual(What + TEXT(": the compare sees no mismatch"), UWoRAttributeDebug::GetAttributeMismatchString(Hero), FString());
	}
END_DEFINE_SPEC(FResistanceSourcesSpec)

void FResistanceSourcesSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		// A Fire Caster (class table), Spirit and resistance points (general), a Fire gear roll.
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Caster;
		Data->InnateElement = ESpellElement::Fire;
		Data->WorldSpiritLevel = RESISTANCE_SOURCES_SPIRIT_LEVEL;
		Data->Resistance = RESISTANCE_SOURCES_RESIST_POINTS;
		const ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, NewObject<UWeaponData>(GetTransientPackage()));
		Hero = Loadout ? Loadout->GetOwner() : nullptr;
		if (Hero && ActiveLoadout())
		{
			ActiveLoadout()->PrimaryWeapon.WeaponEntry.ResistanceBonus.Fire = RESISTANCE_SOURCES_GEAR_FIRE;
			ASC = FWoRTestWorld::AddComponent<UAbilitySystemComponent>(Hero);
			Sync = FWoRTestWorld::AddComponent<UWoRAttributeSyncComponent>(Hero);
		}
		if (!TestNotNull(TEXT("ASC"), ASC) || !TestNotNull(TEXT("Sync"), Sync) ||
			!TestNotNull(TEXT("Skill effect manager"), TestWorld->GetSubsystem<USkillEffectManager>()) ||
			!TestNotNull(TEXT("Resistance set"), ASC->GetSet<UWoRAttributeSet_Resistance>()))
		{
			ASC = nullptr; // each case returns early
			return;
		}
		Sync->SyncAttributes();
	});

	AfterEach([this]()
	{
		Hero = nullptr;
		ASC = nullptr;
		Sync = nullptr;
		TestWorld.Reset();
	});

	It("adds every source over a 0 base, one effect each", [this]()
	{
		if (!ASC) return;
		const WoRResistance::FTypeResistanceSources Fire = WoRResistance::GetTypeResistanceSources(Hero, EWoRDamageType::Fire);
		TestTrue(TEXT("The class table gives Fire"), !FMath::IsNearlyZero(Fire.Class));
		TestTrue(TEXT("The gear roll gives Fire"), !FMath::IsNearlyZero(Fire.Gear));
		TestTrue(TEXT("General status resistance is above 0"), WoRResistance::GetGeneralStatusResistance(Hero) > 0.0f);
		TestMatchesGetters(TEXT("bare"));
	});

	It("updates the effect source in place for a typed buff and a curse", [this]()
	{
		if (!ASC) return;
		const FGameplayAttribute FireAttribute = UWoRAttributeSet_Resistance::GetTypeAttribute(EWoRDamageType::Fire);
		const float FireBefore = ASC->GetNumericAttribute(FireAttribute);
		const TArray<FActiveGameplayEffectHandle> Before = SourceEffects(UWoRGE_ResistanceSources_Effect::StaticClass());
		Give(RESISTANCE_SOURCES_FIRE_BUFF_ID, ESkillEffectType::ResistanceBuff, RESISTANCE_SOURCES_FIRE_BUFF, ESpellElement::Fire);
		Give(RESISTANCE_SOURCES_CURSE_ID, ESkillEffectType::ModifyStatusResist, RESISTANCE_SOURCES_CURSE, ESpellElement::None);
		TestEqual(TEXT("Fire rose by the buff"), ASC->GetNumericAttribute(FireAttribute) - FireBefore,
				  RESISTANCE_SOURCES_FIRE_BUFF / CombatConstants::STAT_PERCENT_DIVISOR, RESISTANCE_SOURCES_TOLERANCE);
		TestTrue(TEXT("The curse lowered the bonus"), ASC->GetNumericAttribute(UWoRAttributeSet_Resistance::GetStatusResistBonusAttribute()) < 0.0f);
		TestTrue(TEXT("Same effect, updated in place"), SourceEffects(UWoRGE_ResistanceSources_Effect::StaticClass()) == Before);
		TestMatchesGetters(TEXT("buffed"));
	});

	It("follows a gear roll change and boss rising resistance", [this]()
	{
		if (!ASC) return;
		ActiveLoadout()->PrimaryWeapon.WeaponEntry.ResistanceBonus.Fire = RESISTANCE_SOURCES_GEAR_FIRE_CHANGED;
		UBossPhaseComponent *Boss = FWoRTestWorld::AddComponent<UBossPhaseComponent>(Hero);
		if (!TestNotNull(TEXT("Boss phases"), Boss)) return;
		Boss->RecordStatusBreak();
		Sync->SyncAttributes();
		TestTrue(TEXT("Boss rising resistance is in"), WoRResistance::GetStatusResistBonusSources(Hero).Boss > 0.0f);
		TestMatchesGetters(TEXT("new roll, one break"));
	});

	It("re-applies a source effect that something removed", [this]()
	{
		if (!ASC) return;
		for (const FActiveGameplayEffectHandle &Handle : SourceEffects(UWoRGE_ResistanceSources_Gear::StaticClass()))
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
		TestTrue(TEXT("Missing source reported"), UWoRAttributeDebug::GetAttributeMismatchString(Hero).Contains(TEXT("expected 1")));
		Sync->SyncAttributes();
		TestMatchesGetters(TEXT("re-applied"));
	});

	It("is seen by the attribute compare when a source carries the wrong bucket", [this]()
	{
		if (!ASC) return;
		const TArray<FActiveGameplayEffectHandle> Gear = SourceEffects(UWoRGE_ResistanceSources_Gear::StaticClass());
		if (!TestEqual(TEXT("One gear source"), Gear.Num(), 1)) return;
		// The class bucket on the gear effect: the sum moves, and the compare names the source.
		const float ClassFire = WoRResistance::GetTypeResistanceSources(Hero, EWoRDamageType::Fire).Class;
		ASC->UpdateActiveGameplayEffectSetByCallerMagnitude(Gear[0], WoRTags::DamageTypeToTag(EWoRDamageType::Fire), ClassFire);
		TestTrue(TEXT("Wrong bucket reported"), UWoRAttributeDebug::GetAttributeMismatchString(Hero).Contains(TEXT("carries")));
	});

	It("clamps general status resistance at the ceiling", [this]()
	{
		if (!ASC) return;
		const FGameplayAttribute StatusResistance = UWoRAttributeSet_Resistance::GetStatusResistanceAttribute();
		ASC->SetNumericAttributeBase(StatusResistance, CombatConstants::RESISTANCE_MAX * RESISTANCE_SOURCES_OVER_CEILING_FACTOR);
		TestEqual(TEXT("Held at the ceiling"), ASC->GetNumericAttribute(StatusResistance), CombatConstants::RESISTANCE_MAX,
				  RESISTANCE_SOURCES_TOLERANCE);
		Sync->SyncAttributes();
		TestMatchesGetters(TEXT("re-synced"));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
