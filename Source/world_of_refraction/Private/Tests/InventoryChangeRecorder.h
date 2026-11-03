// InventoryChangeRecorder.h
// Test-only listener for UInventoryComponent::OnInventoryChanged: a dynamic delegate needs a
// UFUNCTION target, so specs bind this and read back the change types it saw.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Inventory/InventoryComponent.h"
#include "InventoryChangeRecorder.generated.h"

UCLASS(Transient)
class UInventoryChangeRecorder : public UObject
{
	GENERATED_BODY()

public:
	TArray<EInventoryChangeType> Changes;

	void Listen(UInventoryComponent *Inventory)
	{
		Inventory->OnInventoryChanged.AddDynamic(this, &UInventoryChangeRecorder::Record);
	}

	UFUNCTION()
	void Record(EInventoryChangeType ChangeType) { Changes.Add(ChangeType); }
};

/** B-323: listener for UInventoryComponent::OnDuplicateGearReceived (D-115), the same way. */
UCLASS(Transient)
class UDuplicateGearRecorder : public UObject
{
	GENERATED_BODY()

public:
	TArray<FGuid> NewIDs;
	TArray<FGuid> OwnedIDs;

	void Listen(UInventoryComponent *Inventory)
	{
		Inventory->OnDuplicateGearReceived.AddDynamic(this, &UDuplicateGearRecorder::Record);
	}

	UFUNCTION()
	void Record(FGuid NewID, FGuid OwnedID)
	{
		NewIDs.Add(NewID);
		OwnedIDs.Add(OwnedID);
	}
};

