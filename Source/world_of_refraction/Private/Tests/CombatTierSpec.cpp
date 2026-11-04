// CombatTierSpec.cpp
// B-325 (D-118, Q-325): a combatant's Combat Tier is the average tier of its gear and skills, rounded down; it
// follows a gear change; with no gear or skills it falls back to UCharacterData::EnemyTier; an enemy's tier-power
// HP reads it.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Damage/TierPowerConstants.h"
#include "Combat/Tier/CombatTier.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Trial/TrialGate.h"

namespace
{
	constexpr float COMBAT_TIER_TOLERANCE = 1.0e-4f;
}

BEGIN_DEFINE_SPEC(FCombatTierSpec, "WoR.Combat.CombatTier",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FCombatTierSpec)

void FCombatTierSpec::Define()
{
	It("averages tiers and rounds down (Q-325)", [this]()
	{
		const TArray<EItemTier> ThirdAboveC = {EItemTier::C_Tier, EItemTier::C_Tier, EItemTier::B_Tier};
		TestEqual(TEXT("C C B is C"), WoRCombatTier::GetAverageTier(ThirdAboveC, EItemTier::F_Tier), EItemTier::C_Tier);
		const TArray<EItemTier> HalfAboveB = {EItemTier::B_Tier, EItemTier::A_Tier};
		TestEqual(TEXT("B A is B, a half rounds down"), WoRCombatTier::GetAverageTier(HalfAboveB, EItemTier::F_Tier), EItemTier::B_Tier);
		const TArray<EItemTier> Spread = {EItemTier::F_Tier, EItemTier::S_Tier};
		TestEqual(TEXT("F S is C, the exact middle"), WoRCombatTier::GetAverageTier(Spread, EItemTier::F_Tier), EItemTier::C_Tier);
		const TArray<EItemTier> AllS = {EItemTier::S_Tier, EItemTier::S_Tier, EItemTier::S_Tier};
		TestEqual(TEXT("S S S is S"), WoRCombatTier::GetAverageTier(AllS, EItemTier::F_Tier), EItemTier::S_Tier);
		TestEqual(TEXT("nothing is the fallback"), WoRCombatTier::GetAverageTier(TConstArrayView<EItemTier>(), EItemTier::D_Tier), EItemTier::D_Tier);
	});

	Describe("on a combatant", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("Test world created"), TestWorld->Create());
		});

		AfterEach([this]()
		{
			TestWorld.Reset();
		});

		It("falls back to EnemyTier with no gear or skills, and the enemy's HP tier reads it", [this]()
		{
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->EnemyTier = EItemTier::D_Tier;
			const AActor *Enemy = TestWorld->SpawnCombatant(Data, true);
			TestEqual(TEXT("fallback"), WoRCombatTier::GetCombatTier(Enemy), EItemTier::D_Tier);
			TestEqual(TEXT("HP tier power"), TrialGate::GetEnemyTierHealthMultiplier(Enemy),
					  TierPowerScaling::GetTierPowerMultiplier(EItemTier::D_Tier), COMBAT_TIER_TOLERANCE);
		});

		It("reads the weapon in hand over EnemyTier, and follows a gear change", [this]()
		{
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->EnemyTier = EItemTier::F_Tier;
			UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
			ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, Weapon, true);
			UInventoryComponent *Inventory = Loadout ? Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
			if (!TestNotNull(TEXT("loadout"), Loadout) || !TestNotNull(TEXT("inventory"), Inventory))
			{
				return;
			}
			FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
			Active.PrimaryWeapon.WeaponEntry.Tier = EItemTier::A_Tier;
			Loadout->NotifyActiveWeaponChanged();
			TestEqual(TEXT("the A weapon"), WoRCombatTier::GetCombatTier(Loadout->GetOwner()), EItemTier::A_Tier);

			Active.PrimaryWeapon.WeaponEntry.Tier = EItemTier::S_Tier;
			Loadout->NotifyActiveWeaponChanged();
			TestEqual(TEXT("upgraded to S"), WoRCombatTier::GetCombatTier(Loadout->GetOwner()), EItemTier::S_Tier);
			TestEqual(TEXT("the HP tier follows the weapon in hand"), TrialGate::GetEnemyTierHealthMultiplier(Loadout->GetOwner()),
					  TierPowerScaling::GetTierPowerMultiplier(EItemTier::S_Tier), COMBAT_TIER_TOLERANCE);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
