// SevenBalancesSpec.cpp
// B-333 (D-125): seven balances — Gold, Prisms, Diamond, Gear / Skill / Elemental / Pillar Essence.
// Elemental pays for anything with an element, Pillar for stat stones, Skill for spells, abilities and
// ability stones; a spell costs Skill + Elemental, a non-elemental ability Skill only; gear levels on Gear.
// Q-333: a crystal's buy price adds its essence at tier; a None spell is Skill only; the upgrade co-cost
// is half Elemental on items with an element. The save version bump is pinned in SaveSpec.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyService.h"
#include "Currency/EconomyYield.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Merchant/MerchantData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	constexpr EItemTier SEVEN_BALANCES_TIER = EItemTier::D_Tier;
	/** Today's numbers, as literals so a changed ladder fails here on purpose. */
	constexpr int32 SEVEN_BALANCES_ESSENCE_D = 17; // TYPED_ESSENCE_BUY_D
	constexpr int32 SEVEN_BALANCES_EVOLUTION_ELEMENTAL_D = 25; // 17 + half (8)
	constexpr int32 SEVEN_BALANCES_STONE_YIELD_D = 9; // TYPED_ESSENCE_YIELD_D
	constexpr int32 SEVEN_BALANCES_STEP_GEAR_F = 10; // F to E
	constexpr int32 SEVEN_BALANCES_BALANCE_COUNT = 7;
	constexpr ECrystalType SEVEN_BALANCES_STAT_STONE = ECrystalType::DefenseStone;
	constexpr ECrystalType SEVEN_BALANCES_GEM = ECrystalType::Garnet;
}

BEGIN_DEFINE_SPEC(FSevenBalancesSpec, "WoR.Economy.SevenBalances",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UEconomyService *Economy = nullptr;

	FPurchaseCost CostOfAsset(UPrimaryDataAsset *Asset) const
	{
		FMerchantStockEntry Entry;
		Entry.Asset = Asset;
		return Economy->PreviewCartCost({Entry});
	}

	FPurchaseCost CostOfCrystal(ECrystalType Type) const
	{
		FMerchantStockEntry Entry;
		Entry.Crystal = FCrystalId(Type, SEVEN_BALANCES_TIER);
		return Economy->PreviewCartCost({Entry});
	}

	FPurchaseCost CostOfSpell(ESpellElement Element) const
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Element = Element;
		Spell->Tier = SEVEN_BALANCES_TIER;
		return CostOfAsset(Spell);
	}

	/** An owner with every economy component (null with a failed check). */
	AActor *SpawnOwner()
	{
		AActor *Owner = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("Owner"), Owner))
		{
			return nullptr;
		}
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Owner);
		FWoRTestWorld::AddComponent<UInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Owner);
		return Owner;
	}
END_DEFINE_SPEC(FSevenBalancesSpec)

void FSevenBalancesSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Economy = TestWorld->GetSubsystem<UEconomyService>();
		TestNotNull(TEXT("EconomyService"), Economy);
	});

	AfterEach([this]()
	{
		Economy = nullptr;
		TestWorld.Reset();
	});

	It("has seven balances", [this]()
	{
		TestEqual(TEXT("Seven"), static_cast<int32>(UE_ARRAY_COUNT(CurrencyBalances::SEVEN)), SEVEN_BALANCES_BALANCE_COUNT);
	});

	It("prices an elemental spell in Skill + Elemental essence", [this]()
	{
		if (!Economy) return;
		const FPurchaseCost Cost = CostOfSpell(ESpellElement::Fire);
		TestEqual(TEXT("Skill"), Cost.SkillEssence, SEVEN_BALANCES_ESSENCE_D);
		TestEqual(TEXT("Elemental"), Cost.ElementalEssence, SEVEN_BALANCES_ESSENCE_D);
		TestEqual(TEXT("No Pillar"), Cost.PillarEssence, 0);
	});

	It("prices a non-elemental (None) spell in Skill only (Q-333)", [this]()
	{
		if (!Economy) return;
		const FPurchaseCost Cost = CostOfSpell(ESpellElement::None);
		TestEqual(TEXT("Skill"), Cost.SkillEssence, SEVEN_BALANCES_ESSENCE_D);
		TestEqual(TEXT("No Elemental"), Cost.ElementalEssence, 0);
	});

	It("prices a non-elemental ability in Skill only", [this]()
	{
		if (!Economy) return;
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->Tier = SEVEN_BALANCES_TIER;
		const FPurchaseCost Cost = CostOfAsset(Ability);
		TestEqual(TEXT("Skill"), Cost.SkillEssence, SEVEN_BALANCES_ESSENCE_D);
		TestEqual(TEXT("No Elemental"), Cost.ElementalEssence, 0);
		TestEqual(TEXT("No Pillar"), Cost.PillarEssence, 0);
	});

	It("prices a stat stone in Pillar essence, and an ability stone in Skill", [this]()
	{
		if (!Economy) return;
		const FPurchaseCost Stone = CostOfCrystal(SEVEN_BALANCES_STAT_STONE);
		TestEqual(TEXT("Stat stone: Pillar"), Stone.PillarEssence, SEVEN_BALANCES_ESSENCE_D);
		TestEqual(TEXT("Stat stone: no Elemental"), Stone.ElementalEssence, 0);
		TestEqual(TEXT("Stat stone: Prisms at tier"), Stone.Prisms, EconomyYield::GetPrismsBaseForTier(SEVEN_BALANCES_TIER));
		const FPurchaseCost AbilityStone = CostOfCrystal(ECrystalType::AbilityStone);
		TestEqual(TEXT("Ability stone: Skill"), AbilityStone.SkillEssence, SEVEN_BALANCES_ESSENCE_D);
		TestEqual(TEXT("Ability stone: no Pillar"), AbilityStone.PillarEssence, 0);
	});

	It("prices elemental items in Elemental essence: a gem, Quartz, an evolution", [this]()
	{
		if (!Economy) return;
		TestEqual(TEXT("Gem"), CostOfCrystal(SEVEN_BALANCES_GEM).ElementalEssence, SEVEN_BALANCES_ESSENCE_D);
		TestEqual(TEXT("Quartz folds into Elemental"), CostOfCrystal(ECrystalType::Quartz).ElementalEssence, SEVEN_BALANCES_ESSENCE_D);
		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		Evolution->Tier = SEVEN_BALANCES_TIER;
		const FPurchaseCost Cost = CostOfAsset(Evolution);
		TestEqual(TEXT("Evolution: Elemental at tier + half"), Cost.ElementalEssence, SEVEN_BALANCES_EVOLUTION_ELEMENTAL_D);
		TestEqual(TEXT("Evolution: no typed essence (obsolete)"), Cost.Typed.Num(), 0);
	});

	It("prices an element-agnostic (Quartz) evolution in Prisms only (Q-333)", [this]()
	{
		if (!Economy) return;
		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		Evolution->Tier = SEVEN_BALANCES_TIER;
		Evolution->CrystalType = ECrystalType::Quartz; // Quartz carries no element
		const FPurchaseCost Cost = CostOfAsset(Evolution);
		TestEqual(TEXT("No Elemental"), Cost.ElementalEssence, 0);
		TestTrue(TEXT("Still costs Prisms"), Cost.Prisms > 0);
	});

	It("pays a dismantled stat stone in Pillar essence", [this]()
	{
		if (!Economy) return;
		AActor *Owner = SpawnOwner();
		if (!Owner) return;
		const FCrystalId Stone(SEVEN_BALANCES_STAT_STONE, SEVEN_BALANCES_TIER);
		Owner->FindComponentByClass<UInventoryComponent>()->AddCrystal(Stone, 1);
		TestTrue(TEXT("Dismantled"), Economy->DismantleCrystal(Owner, Stone, 1));
		const UCurrencyComponent *Wallet = Owner->FindComponentByClass<UCurrencyComponent>();
		TestEqual(TEXT("Pillar"), Wallet->GetPillarEssence(), SEVEN_BALANCES_STONE_YIELD_D);
		TestEqual(TEXT("No Elemental"), Wallet->GetElementalEssence(), 0);
	});

	It("levels gear on Gear essence alone", [this]()
	{
		if (!Economy) return;
		AActor *Owner = SpawnOwner();
		if (!Owner) return;
		UInventoryComponent *Inventory = Owner->FindComponentByClass<UInventoryComponent>();
		UCurrencyComponent *Wallet = Owner->FindComponentByClass<UCurrencyComponent>();
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->Tier = EItemTier::F_Tier;
		if (!TestTrue(TEXT("Weapon added"), Inventory->AddWeapon(Weapon)) || Inventory->Weapons.Num() == 0) return;
		Wallet->AddGearEssence(SEVEN_BALANCES_STEP_GEAR_F);
		TestTrue(TEXT("Levelled"), Economy->LevelUpWeapon(Owner, Inventory->Weapons.Last().PersistentID));
		TestEqual(TEXT("Gear spent"), Wallet->GetGearEssence(), 0);
		TestEqual(TEXT("A weapon has no element: no Elemental co-cost (Q-333)"), Wallet->GetElementalEssence(), 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
