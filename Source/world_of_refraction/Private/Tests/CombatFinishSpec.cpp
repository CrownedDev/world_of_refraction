// CombatFinishSpec.cpp
// B-246 (DRY audit A7, Q-246): every way a fight ends runs the same end steps. A fight the TurnManager ends
// with a winner destroys the broken crystals and repairs the rest (ER-35), as a win through the action path
// does; a draw keeps its wear (Q-115b). The turn number and async wait reset either way.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/TurnManager.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Durability/DurabilityConstants.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"

namespace
{
	constexpr int32 FINISH_WORN_DURABILITY = 1;
	constexpr int32 FINISH_BROKEN_DURABILITY = 0;
	/** More than any test combatant's HP. */
	constexpr int32 FINISH_LETHAL_DAMAGE = 100000;
	const FCrystalId FINISH_GEM(ECrystalType::Garnet, EItemTier::F_Tier);

	FRuntimeAttachedItem FinishGem(int32 Durability)
	{
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::Crystal;
		Attached.Crystal = FCrystalAttachment(FINISH_GEM, Durability);
		return Attached;
	}
}

BEGIN_DEFINE_SPEC(FCombatFinishSpec, "WoR.Combat.FinishPath", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	UTurnManager *TurnManager = nullptr;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;
	UInventoryComponent *Inventory = nullptr;

	/** A Resonator hero with a worn gem on its weapon and a broken gem on its active ring. */
	void SpawnHero()
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Resonator;
		Hero = TestWorld->SpawnCombatant(Data);
		Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Hero);
		ULoadoutComponent *Loadout = FWoRTestWorld::AddComponent<ULoadoutComponent>(Hero);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Hero);
		Loadout->InitializeFromCharacterData(Data, Inventory);
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.PrimarySlotType = EPrimarySlotType::Weapon;
		Active.PrimaryWeapon.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Active.PrimaryWeapon.WeaponEntry.AttachedItem = FinishGem(FINISH_WORN_DURABILITY);
		FRingLoadoutEntry RingEntry;
		RingEntry.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
		RingEntry.RingEntry.AttachedItem = FinishGem(FINISH_BROKEN_DURABILITY);
		Active.RingLoadout = {RingEntry};
		Active.ActiveRingIndex = 0;
	}

	const FCombatLoadout &ActiveLoadout() const { return Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex]; }

	static void Kill(AActor *Actor) { Actor->FindComponentByClass<UCharacterDataComponent>()->ServerTakeDamage(FINISH_LETHAL_DAMAGE); }
END_DEFINE_SPEC(FCombatFinishSpec)

void FCombatFinishSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		TurnManager = TestWorld->GetSubsystem<UTurnManager>();
		SpawnHero();
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
	});

	AfterEach([this]() { TestWorld.Reset(); });

	It("a TurnManager-ended win destroys the broken gem and repairs the worn one", [this]()
	{
		if (!TestTrue(TEXT("setup"), Orchestrator && TurnManager && Hero && Foe && Inventory)) return;
		const int32 Repaired = FMath::Min(FINISH_WORN_DURABILITY + DurabilityConstants::REPAIR_PER_BATTLE,
										  ActiveLoadout().PrimaryWeapon.WeaponEntry.AttachedItem.GetMaxDurability());
		Orchestrator->StartCombat({Hero}, {Foe});
		Kill(Foe);
		if (TurnManager->IsCombatActive())
		{
			TurnManager->EndCombat(); // the TurnManager ends the fight (HandleCombatEnded)
		}

		TestEqual(TEXT("idle"), Orchestrator->GetCombatState(), ECombatState::Idle);
		TestEqual(TEXT("turn number reset"), Orchestrator->GetCurrentTurnNumber(), 0);
		TestFalse(TEXT("no async wait"), Orchestrator->IsWaitingForAsyncAction());
		TestTrue(TEXT("broken ring gem destroyed"), ActiveLoadout().RingLoadout[0].RingEntry.AttachedItem.IsEmpty());
		TestEqual(TEXT("worn weapon gem repaired"), ActiveLoadout().PrimaryWeapon.WeaponEntry.AttachedItem.GetCurrentDurability(), Repaired);
	});

	It("a draw keeps the wear", [this]()
	{
		if (!TestTrue(TEXT("setup"), Orchestrator && TurnManager && Hero && Foe && Inventory)) return;
		Orchestrator->StartCombat({Hero}, {Foe});
		Kill(Foe);
		Kill(Hero);
		if (TurnManager->IsCombatActive())
		{
			TurnManager->EndCombat();
		}
		TestEqual(TEXT("idle"), Orchestrator->GetCombatState(), ECombatState::Idle);
		TestFalse(TEXT("broken gem kept"), ActiveLoadout().RingLoadout[0].RingEntry.AttachedItem.IsEmpty());
		TestEqual(TEXT("worn gem not repaired"), ActiveLoadout().PrimaryWeapon.WeaponEntry.AttachedItem.GetCurrentDurability(),
				  FINISH_WORN_DURABILITY);
	});
}

#endif
