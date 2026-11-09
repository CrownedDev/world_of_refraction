// ActionsChangedRecorder.h
// Test-only listener for UCharacterDataComponent::OnActionsChanged (B-170a). Dynamic delegates need a
// UFUNCTION target, so ActionPipsSpec binds this and reads back each broadcast.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ActionsChangedRecorder.generated.h"

UCLASS(Transient)
class UActionsChangedRecorder : public UObject
{
	GENERATED_BODY()

public:
	/** Each broadcast's (current, max), in arrival order. */
	TArray<TPair<int32, int32>> Broadcasts;

	UFUNCTION()
	void Record(int32 NewCurrent, int32 NewMax)
	{
		Broadcasts.Add(TPair<int32, int32>(NewCurrent, NewMax));
	}
};
