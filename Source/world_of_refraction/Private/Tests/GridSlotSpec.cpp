// GridSlotSpec.cpp
// B-230b: the grid subsystem mirrors each slot change onto the actor's replicated slot, and its position reads fall
// back to that slot for an actor it doesn't hold (a client's empty grid). The two-machine path is the PIE sheet.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Grid/CombatGridSlotComponent.h"
#include "Combat/Grid/CombatGridSubsystem.h"
#include "WoRTestWorld.h"

BEGIN_DEFINE_SPEC(FGridSlotSpec, "WoR.Net.GridSlot", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AActor *Fighter = nullptr;
	UCombatGridSlotComponent *GridSlot = nullptr;
	UCombatGridSubsystem *Grid = nullptr;
END_DEFINE_SPEC(FGridSlotSpec)

void FGridSlotSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
		Fighter = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<AActor>() : nullptr;
		GridSlot = Fighter ? NewObject<UCombatGridSlotComponent>(Fighter) : nullptr;
		if (GridSlot)
		{
			GridSlot->RegisterComponent();
		}
		Grid = TestWorld->GetSubsystem<UCombatGridSubsystem>();
	});
	AfterEach([this]()
	{
		if (Grid)
		{
			Grid->ClearAllPositions();
		}
		Fighter = nullptr;
		GridSlot = nullptr;
		Grid = nullptr;
		TestWorld.Reset();
	});

	It("holds a slot and clears it", [this]()
	{
		if (!TestNotNull(TEXT("setup"), GridSlot))
		{
			return;
		}
		FCombatGridPosition Out;
		TestFalse(TEXT("off the grid at first"), GridSlot->GetSlot(Out));
		GridSlot->SetSlot(FCombatGridPosition(0, ECombatRow::Front, 1));
		TestTrue(TEXT("on the grid"), GridSlot->GetSlot(Out) && Out.Row == ECombatRow::Front);
		GridSlot->ClearSlot();
		TestFalse(TEXT("cleared"), GridSlot->GetSlot(Out));
	});

	It("follows the grid: placed, moved by a row, removed", [this]()
	{
		if (!TestTrue(TEXT("setup"), GridSlot && Grid))
		{
			return;
		}
		FCombatGridPosition Out;
		TestTrue(TEXT("placed"), Grid->AssignPosition(Fighter, FCombatGridPosition(0, ECombatRow::Back, 0)));
		TestTrue(TEXT("the slot shows the back row"), GridSlot->GetSlot(Out) && Out.Row == ECombatRow::Back);
		TestTrue(TEXT("moved"), Grid->MoveActorToRow(Fighter, ECombatRow::Middle, FVector::ZeroVector));
		TestTrue(TEXT("the slot shows the middle row"), GridSlot->GetSlot(Out) && Out.Row == ECombatRow::Middle);
		Grid->RemoveFromGrid(Fighter);
		TestFalse(TEXT("removed: off the grid"), GridSlot->GetSlot(Out));
	});

	It("reads replicated slots only on a client; the server's grid stays the truth (review)", [this]()
	{
		TestTrue(TEXT("a client reads the slots"), UCombatGridSubsystem::ReadsReplicatedSlots(NM_Client));
		TestFalse(TEXT("a listen server doesn't"), UCombatGridSubsystem::ReadsReplicatedSlots(NM_ListenServer));
		TestFalse(TEXT("standalone doesn't"), UCombatGridSubsystem::ReadsReplicatedSlots(NM_Standalone));
		if (!TestTrue(TEXT("setup"), GridSlot && Grid))
		{
			return;
		}
		GridSlot->SetSlot(FCombatGridPosition(1, ECombatRow::Front, 2)); // a slot the grid doesn't hold
		FCombatGridPosition Out;
		TestFalse(TEXT("standalone: not on this grid"), Grid->GetActorPosition(Fighter, Out));
	});
}

#endif
