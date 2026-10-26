// TurnSwitchSpec.cpp
// ET-19 (Crown, 2026-10-26; B-175): one free weapon or ring switch per turn. A second switch in
// the same turn is refused with a reason; the next turn gives it back.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/CombatOrchestrator.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"

namespace
{
	/** Test-world ticks are 0.01s; enough for a turn advance to settle. */
	constexpr int32 TURN_SWITCH_SETTLE_FRAMES = 10;
	/** Normal turns to play through for the switcher's next turn to come up. */
	constexpr int32 TURN_SWITCH_MAX_TURNS = 4;

	template <class TComponent>
	TComponent *TurnSwitchAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}
}

BEGIN_DEFINE_SPEC(FTurnSwitchSpec, "WoR.Loadout.TurnSwitch",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ULoadoutComponent *Loadout = nullptr;

	AActor *SpawnWithLoadout()
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		AActor *Actor = TestWorld->SpawnCombatant(Data);
		if (!Actor)
		{
			return nullptr;
		}
		UInventoryComponent *Inventory = TurnSwitchAddComponent<UInventoryComponent>(Actor);
		Loadout = TurnSwitchAddComponent<ULoadoutComponent>(Actor);
		Loadout->InitializeFromCharacterData(Data, Inventory);
		return Actor;
	}
END_DEFINE_SPEC(FTurnSwitchSpec)

void FTurnSwitchSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		Loadout = nullptr;
		TestWorld.Reset();
	});

	It("refuses a second switch in the same turn, with a reason", [this]()
	{
		if (!TestNotNull(TEXT("Switcher"), SpawnWithLoadout()))
		{
			return;
		}
		FText Reason;
		TestTrue(TEXT("first switch allowed"), Loadout->TrySwitchWeaponThisTurn(Reason));
		TestTrue(TEXT("no reason for the first"), Reason.IsEmpty());
		TestFalse(TEXT("second weapon switch refused"), Loadout->TrySwitchWeaponThisTurn(Reason));
		TestFalse(TEXT("with a reason"), Reason.IsEmpty());
		TestFalse(TEXT("a ring switch shares the one free switch"), Loadout->TrySwitchRingThisTurn(0, Reason));
		TestFalse(TEXT("the menu reads the same reason"), Loadout->GetTurnSwitchRefusedReason().IsEmpty());
	});

	It("only flips the weapon once", [this]()
	{
		if (!TestNotNull(TEXT("Switcher"), SpawnWithLoadout()))
		{
			return;
		}
		const bool bPrimaryBefore = Loadout->GetActiveLoadout().bShowPrimary;
		FText Reason;
		Loadout->TrySwitchWeaponThisTurn(Reason);
		Loadout->TrySwitchWeaponThisTurn(Reason);
		TestEqual(TEXT("switched once"), Loadout->GetActiveLoadout().bShowPrimary, !bPrimaryBefore);
	});

	It("keeps the switch when a ring switch changes nothing", [this]()
	{
		if (!TestNotNull(TEXT("Switcher"), SpawnWithLoadout()))
		{
			return;
		}
		FText Reason;
		TestFalse(TEXT("no ring at that index"), Loadout->TrySwitchRingThisTurn(INDEX_NONE, Reason));
		TestTrue(TEXT("switch still free"), Loadout->GetTurnSwitchRefusedReason().IsEmpty());
	});

	It("allows a switch again on the next turn", [this]()
	{
		ACombatOrchestrator *Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		AActor *Hero = SpawnWithLoadout();
		AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("Fight set up"), Orchestrator && Hero && Foe))
		{
			return;
		}
		// Equal stats: the tie-break gives the local party (Hero) turn 1.
		Orchestrator->StartCombat({Hero}, {Foe});
		if (!TestTrue(TEXT("Hero acts first"), Orchestrator->GetCurrentActor() == Hero))
		{
			Orchestrator->ForceEndCombat();
			return;
		}
		FText Reason;
		Loadout->TrySwitchWeaponThisTurn(Reason);
		TestFalse(TEXT("used this turn"), Loadout->GetTurnSwitchRefusedReason().IsEmpty());

		for (int32 Turn = 0; Turn < TURN_SWITCH_MAX_TURNS; ++Turn)
		{
			Orchestrator->OnActionCompleted();
			TestWorld->Tick(TURN_SWITCH_SETTLE_FRAMES);
			if (Orchestrator->GetCurrentActor() == Hero)
			{
				break;
			}
		}
		TestTrue(TEXT("Hero's turn again"), Orchestrator->GetCurrentActor() == Hero);
		TestTrue(TEXT("switch allowed again"), Loadout->TrySwitchWeaponThisTurn(Reason));
		Orchestrator->ForceEndCombat();
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
