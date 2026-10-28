// EconomyPreambleSpec.cpp
// Every UEconomyService entry point refuses a null owner and an owner missing its components,
// and a well-formed owner gets through to the action (B-025). Written against the hand-copied
// preambles first, so the shared helper must give the same results. Not covered: the "no
// authority" refusal (a standalone test world always has authority).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/CombatOrchestrator.h"
#include "WoRTestWorld.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyService.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	template <class TComponent>
	TComponent *EconomyTestAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}

	const FCrystalId ECONOMY_TEST_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	constexpr int32 ECONOMY_TEST_GEM_COUNT = 2;
}

BEGIN_DEFINE_SPEC(FEconomyPreambleSpec, "WoR.Economy.Preamble",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> TestWorld;
UEconomyService *Economy = nullptr;

/** Runs every mutating entry point on Owner and expects each to refuse. */
void ExpectAllRefuse(AActor *Owner, const FString &Context)
{
	const FGuid AnyId = FGuid::NewGuid();
	// Real arguments, so the entry points with their own guards (null spell/ability, empty
	// cart) reach the owner and component checks.
	USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
	UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
	const TArray<FMerchantStockEntry> Cart = {FMerchantStockEntry()};
	const TPair<const TCHAR *, bool> Results[] = {
		{TEXT("DismantleCrystal"), Economy->DismantleCrystal(Owner, ECONOMY_TEST_GEM, 1)},
		{TEXT("MergeCrystals"), Economy->MergeCrystals(Owner, ECrystalType::Garnet, EItemTier::E_Tier)},
		{TEXT("DismantleWeapon"), Economy->DismantleWeapon(Owner, AnyId)},
		{TEXT("DismantleRing"), Economy->DismantleRing(Owner, AnyId)},
		{TEXT("DismantleEvolution"), Economy->DismantleEvolution(Owner, AnyId)},
		{TEXT("RemovePrimaryEvolution"), Economy->RemovePrimaryEvolution(Owner)},
		{TEXT("DismantleSpell"), Economy->DismantleSpell(Owner, Spell)},
		{TEXT("DismantleAbility"), Economy->DismantleAbility(Owner, Ability)},
		{TEXT("Purchase"), Economy->Purchase(Owner, Cart)},
		{TEXT("BuyWorldStat"), Economy->BuyWorldStat(Owner, EWorldPillar(0))},
		{TEXT("LevelUpWeapon"), Economy->LevelUpWeapon(Owner, AnyId)},
		{TEXT("LevelUpRing"), Economy->LevelUpRing(Owner, AnyId)},
		{TEXT("LevelUpEvolution"), Economy->LevelUpEvolution(Owner, AnyId)},
		{TEXT("LevelUpSpell"), Economy->LevelUpSpell(Owner, Spell)},
		{TEXT("LevelUpAbility"), Economy->LevelUpAbility(Owner, Ability)},
		{TEXT("DowngradeWeapon"), Economy->DowngradeWeapon(Owner, AnyId)},
		{TEXT("DowngradeRing"), Economy->DowngradeRing(Owner, AnyId)},
		{TEXT("DowngradeEvolution"), Economy->DowngradeEvolution(Owner, AnyId)},
		{TEXT("DowngradeSpell"), Economy->DowngradeSpell(Owner, Spell)},
		{TEXT("DowngradeAbility"), Economy->DowngradeAbility(Owner, Ability)},
	};
	for (const TPair<const TCHAR *, bool> &Result : Results)
	{
		TestFalse(*FString::Printf(TEXT("%s: %s refused"), *Context, Result.Key), Result.Value);
	}
}

/** An owner with every component the economy reads. */
AActor *SpawnWellFormedOwner()
{
	AActor *Owner = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
	EconomyTestAddComponent<UCrystalInventoryComponent>(Owner);
	EconomyTestAddComponent<UEvolutionInventoryComponent>(Owner);
	EconomyTestAddComponent<UCurrencyComponent>(Owner);
	EconomyTestAddComponent<UInventoryComponent>(Owner);
	EconomyTestAddComponent<ULoadoutComponent>(Owner);
	return Owner;
}
END_DEFINE_SPEC(FEconomyPreambleSpec)

void FEconomyPreambleSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		Economy = TestWorld->GetSubsystem<UEconomyService>();
		TestNotNull(TEXT("economy service"), Economy);
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("refuses a null owner everywhere", [this]()
	{
		ExpectAllRefuse(nullptr, TEXT("null owner"));
	});

	It("refuses an owner without inventory or currency components everywhere", [this]()
	{
		ExpectAllRefuse(TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData()), TEXT("bare owner"));
	});

	It("lets a well-formed owner dismantle a crystal", [this]()
	{
		AActor *Owner = SpawnWellFormedOwner();
		UInventoryComponent *Inventory = Owner->FindComponentByClass<UInventoryComponent>();
		UCrystalInventoryComponent *Crystals = Owner->FindComponentByClass<UCrystalInventoryComponent>();
		TestTrue(TEXT("gems added"), Inventory->AddCrystal(ECONOMY_TEST_GEM, ECONOMY_TEST_GEM_COUNT));
		TestTrue(TEXT("dismantled"), Economy->DismantleCrystal(Owner, ECONOMY_TEST_GEM, 1));
		TestEqual(TEXT("one gem left"), Crystals->GetCount(ECONOMY_TEST_GEM), ECONOMY_TEST_GEM_COUNT - 1);
	});

	It("refuses dismantle, level-up and revert during a fight, and allows them after (Q-252)", [this]()
	{
		AActor *Owner = SpawnWellFormedOwner();
		AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		ACombatOrchestrator *Orchestrator = TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>();
		UInventoryComponent *Inventory = Owner->FindComponentByClass<UInventoryComponent>();
		UCrystalInventoryComponent *Crystals = Owner->FindComponentByClass<UCrystalInventoryComponent>();
		if (!TestNotNull(TEXT("Orchestrator"), Orchestrator) || !TestNotNull(TEXT("Foe"), Foe)) return;
		TestTrue(TEXT("gems added"), Inventory->AddCrystal(ECONOMY_TEST_GEM, ECONOMY_TEST_GEM_COUNT));
		TestTrue(TEXT("weapon added"), Inventory->AddWeapon(NewObject<UWeaponData>(GetTransientPackage())));
		const FGuid WeaponId = Inventory->Weapons.Num() > 0 ? Inventory->Weapons[0].PersistentID : FGuid();

		Orchestrator->StartCombat({Owner}, {Foe});
		TestFalse(TEXT("mid-fight: dismantle a crystal refused"), Economy->DismantleCrystal(Owner, ECONOMY_TEST_GEM, 1));
		TestEqual(TEXT("mid-fight: gems kept"), Crystals->GetCount(ECONOMY_TEST_GEM), ECONOMY_TEST_GEM_COUNT);
		TestFalse(TEXT("mid-fight: level-up refused"), Economy->LevelUpWeapon(Owner, WeaponId));
		TestFalse(TEXT("mid-fight: revert refused"), Economy->DowngradeWeapon(Owner, WeaponId));
		TestFalse(TEXT("mid-fight: weapon dismantle refused"), Economy->DismantleWeapon(Owner, WeaponId));

		Orchestrator->ForceEndCombat();
		TestTrue(TEXT("after the fight: dismantle a crystal"), Economy->DismantleCrystal(Owner, ECONOMY_TEST_GEM, 1));
	});

	It("lets a well-formed owner dismantle a weapon", [this]()
	{
		AActor *Owner = SpawnWellFormedOwner();
		UInventoryComponent *Inventory = Owner->FindComponentByClass<UInventoryComponent>();
		TestTrue(TEXT("weapon added"), Inventory->AddWeapon(NewObject<UWeaponData>(GetTransientPackage())));
		if (!TestEqual(TEXT("one weapon"), Inventory->Weapons.Num(), 1))
		{
			return;
		}
		TestTrue(TEXT("dismantled"), Economy->DismantleWeapon(Owner, Inventory->Weapons[0].PersistentID));
		TestEqual(TEXT("weapon gone"), Inventory->Weapons.Num(), 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
