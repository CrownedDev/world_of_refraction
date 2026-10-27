// PoolSourcesSpec.cpp
// GAS stage 3 (B-042u, Q-042u): MaxHealth / MaxEnergy on GAS. The attribute's base is the stat portion of the
// last RecomputeMaxPools, a UWoRGE_PoolSources effect adds the gear's flat amount and multiplies the summed stone
// + buff percent, the attribute set rounds it, and the current value equals the component's MaxHP / MaxEP. A max
// set another way (a direct write) falls back to the mirrored value with no pool effect.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterDataComponent.h"
#include "Debug/GAS/WoRAttributeDebug.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "GAS/WoRGameplayEffects.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 POOL_SOURCES_BUFF_ID = 4821;
	constexpr int32 POOL_SOURCES_TURNS = 3;
	/** A percent that leaves a fraction before the round. */
	constexpr float POOL_SOURCES_BUFF_PERCENT = 33.0f;
	/** The equipped weapon's gear MaxHP points. */
	constexpr int32 POOL_SOURCES_GEAR_POINTS = 7;
	/** A max no recompute would give. */
	constexpr int32 POOL_SOURCES_DIRECT_MAX_HP = 1234;
	constexpr float POOL_SOURCES_TOLERANCE = 1.0e-3f;
}

BEGIN_DEFINE_SPEC(FPoolSourcesSpec, "WoR.GAS.PoolSources",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AActor *Hero = nullptr;
	UAbilitySystemComponent *ASC = nullptr;
	UCharacterDataComponent *Char = nullptr;
	UWoRAttributeSyncComponent *Sync = nullptr;

	int32 CountPoolSources(const FGameplayAttribute &Attribute) const
	{
		FGameplayEffectQuery Query;
		Query.EffectDefinition = UWoRGE_PoolSources::GetClassFor(Attribute);
		return ASC->GetActiveEffects(Query).Num();
	}

	TArray<FActiveGameplayEffectHandle> MaxHealthSources() const
	{
		FGameplayEffectQuery Query;
		Query.EffectDefinition = UWoRGE_PoolSources::GetClassFor(UWoRAttributeSet_Vitals::GetMaxHealthAttribute());
		return ASC->GetActiveEffects(Query);
	}

	float MaxHealth() const { return ASC->GetNumericAttribute(UWoRAttributeSet_Vitals::GetMaxHealthAttribute()); }
END_DEFINE_SPEC(FPoolSourcesSpec)

void FPoolSourcesSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		// The weapon carries the gear's MaxHP points.
		const ULoadoutComponent *Loadout =
			TestWorld->SpawnCombatantWithLoadout(FWoRTestWorld::NewCharacterData(), NewObject<UWeaponData>(GetTransientPackage()));
		Hero = Loadout ? Loadout->GetOwner() : nullptr;
		if (Hero)
		{
			ASC = FWoRTestWorld::AddComponent<UAbilitySystemComponent>(Hero);
			Sync = FWoRTestWorld::AddComponent<UWoRAttributeSyncComponent>(Hero);
			Char = Hero->FindComponentByClass<UCharacterDataComponent>();
		}
		if (!TestNotNull(TEXT("ASC"), ASC) || !TestNotNull(TEXT("Sync"), Sync) || !TestNotNull(TEXT("Character data"), Char) ||
			!TestNotNull(TEXT("Skill effect manager"), TestWorld->GetSubsystem<USkillEffectManager>()))
		{
			ASC = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		Hero = nullptr;
		ASC = nullptr;
		Char = nullptr;
		Sync = nullptr;
		TestWorld.Reset();
	});

	It("holds the stat portion as the base and the max as the current value", [this]()
	{
		if (!ASC) return;
		FWoRPoolLayers HP;
		FWoRPoolLayers EP;
		TestTrue(TEXT("MaxHP layers"), Char->GetMaxHPLayers(HP));
		TestTrue(TEXT("MaxEP layers"), Char->GetMaxEPLayers(EP));
		const FGameplayAttribute MaxEnergy = UWoRAttributeSet_Vitals::GetMaxEnergyAttribute();
		TestEqual(TEXT("MaxHealth base = stat portion"), ASC->GetNumericAttributeBase(UWoRAttributeSet_Vitals::GetMaxHealthAttribute()),
				  HP.StatPortion, POOL_SOURCES_TOLERANCE);
		TestEqual(TEXT("MaxEnergy base = stat portion"), ASC->GetNumericAttributeBase(MaxEnergy), EP.StatPortion, POOL_SOURCES_TOLERANCE);
		TestEqual(TEXT("MaxHealth = MaxHP"), MaxHealth(), static_cast<float>(Char->MaxHP));
		TestEqual(TEXT("MaxEnergy = MaxEP"), ASC->GetNumericAttribute(MaxEnergy), static_cast<float>(Char->MaxEP));
		TestEqual(TEXT("One MaxHealth sources effect"), CountPoolSources(UWoRAttributeSet_Vitals::GetMaxHealthAttribute()), 1);
		TestEqual(TEXT("One MaxEnergy sources effect"), CountPoolSources(MaxEnergy), 1);
		TestTrue(TEXT("The compare sees no mismatch"), UWoRAttributeDebug::GetAttributeMismatchString(Hero).IsEmpty());
	});

	It("rounds a pool buff in by updating its sources effect in place", [this]()
	{
		if (!ASC) return;
		const int32 Before = Char->MaxHP;
		const TArray<FActiveGameplayEffectHandle> HandlesBefore = MaxHealthSources();
		TestTrue(TEXT("Sources applied"), HandlesBefore.Num() == 1);
		// The component recomputes its max on a pool effect (HandlePoolEffectChanged), and the sync follows.
		TestWorld->GetSubsystem<USkillEffectManager>()->ApplyEffect(
			Hero, FActiveSkillEffect::CreateBuff(TEXT("Pool sources test"), POOL_SOURCES_BUFF_ID, ESkillEffectType::MaxHPBuff,
												 POOL_SOURCES_BUFF_PERCENT, POOL_SOURCES_TURNS), Hero);
		Sync->SyncAttributes();
		TestTrue(TEXT("The buff raised MaxHP"), Char->MaxHP > Before);
		TestEqual(TEXT("MaxHealth = MaxHP"), MaxHealth(), static_cast<float>(Char->MaxHP));
		TestEqual(TEXT("A whole number"), FMath::Frac(MaxHealth()), 0.0f);
		TestTrue(TEXT("Same effect, updated in place"), MaxHealthSources() == HandlesBefore);
	});

	It("adds the gear's flat amount before the percent", [this]()
	{
		if (!ASC) return;
		UInventoryComponent *Inventory = Hero->FindComponentByClass<UInventoryComponent>();
		if (!TestNotNull(TEXT("Inventory"), Inventory) ||
			!TestTrue(TEXT("An active loadout"), Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex)))
		{
			return;
		}
		const int32 Before = Char->MaxHP;
		Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].PrimaryWeapon.WeaponEntry.StatBonus.BonusMaxHP = POOL_SOURCES_GEAR_POINTS;
		Char->RecomputeMaxPools(); // what an equipment change runs
		Sync->SyncAttributes();
		FWoRPoolLayers HP;
		TestTrue(TEXT("MaxHP layers"), Char->GetMaxHPLayers(HP));
		TestTrue(TEXT("The gear is the flat amount"), HP.GearAmount > 0.0f);
		TestTrue(TEXT("The gear raised MaxHP"), Char->MaxHP > Before);
		TestEqual(TEXT("MaxHealth = MaxHP"), MaxHealth(), static_cast<float>(Char->MaxHP));
	});

	It("mirrors a max set another way, with no pool effect", [this]()
	{
		if (!ASC) return;
		Char->MaxHP = POOL_SOURCES_DIRECT_MAX_HP;
		Sync->SyncAttributes();
		FWoRPoolLayers HP;
		TestFalse(TEXT("The layers no longer give the max"), Char->GetMaxHPLayers(HP));
		TestEqual(TEXT("MaxHealth = the written max"), MaxHealth(), static_cast<float>(POOL_SOURCES_DIRECT_MAX_HP));
		TestEqual(TEXT("No MaxHealth sources effect"), CountPoolSources(UWoRAttributeSet_Vitals::GetMaxHealthAttribute()), 0);
		TestTrue(TEXT("The compare sees no mismatch"), UWoRAttributeDebug::GetAttributeMismatchString(Hero).IsEmpty());
	});
}

#endif
