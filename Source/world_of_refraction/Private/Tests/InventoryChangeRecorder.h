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
