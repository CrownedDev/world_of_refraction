// CombatGridSlotComponent.cpp

#include "Combat/Grid/CombatGridSlotComponent.h"
#include "Combat/Grid/ECombatRow.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "WoRLog.h"

UCombatGridSlotComponent::UCombatGridSlotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UCombatGridSlotComponent *UCombatGridSlotComponent::Find(const AActor *Actor)
{
	return Actor ? Actor->FindComponentByClass<UCombatGridSlotComponent>() : nullptr;
}

void UCombatGridSlotComponent::SetSlot(const FCombatGridPosition &Position)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) // review fix: replicated state is written on the server only
	{
		return;
	}
	Slot = Position;
	bOnGrid = true;
	OnSlotChanged.Broadcast(GetOwner()); // the server's own readers; clients get OnRep_Slot
}

void UCombatGridSlotComponent::ClearSlot()
{
	if (!bOnGrid || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	bOnGrid = false;
	OnSlotChanged.Broadcast(GetOwner());
}

bool UCombatGridSlotComponent::GetSlot(FCombatGridPosition &OutPosition) const
{
	if (!bOnGrid)
	{
		return false;
	}
	OutPosition = Slot;
	return true;
}

void UCombatGridSlotComponent::OnRep_Slot()
{
	OnSlotChanged.Broadcast(GetOwner());
}

FString UCombatGridSlotComponent::GetDebugString() const
{
	return bOnGrid ? FString::Printf(TEXT("%s: %s"), *GetNameSafe(GetOwner()), *Slot.ToString())
				   : FString::Printf(TEXT("%s: off the grid"), *GetNameSafe(GetOwner()));
}

void UCombatGridSlotComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCombatGridSlotComponent, Slot);
	DOREPLIFETIME(UCombatGridSlotComponent, bOnGrid);
}

static FAutoConsoleCommandWithWorld GGridPrintSlotsCommand(
	TEXT("wor.Grid.PrintSlots"),
	TEXT("B-230b: every combatant's replicated grid slot (on a client: what the server sent)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!World)
		{
			return;
		}
		int32 Count = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (const UCombatGridSlotComponent *GridSlot = UCombatGridSlotComponent::Find(*It))
			{
				UE_LOG(LogWoR, Display, TEXT("[GridSlots] %s"), *GridSlot->GetDebugString());
				++Count;
			}
		}
		UE_LOG(LogWoR, Display, TEXT("[GridSlots] %d combatant(s)"), Count);
	}));
