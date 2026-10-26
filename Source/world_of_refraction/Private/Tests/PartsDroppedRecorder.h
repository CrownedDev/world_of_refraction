// PartsDroppedRecorder.h
// Test-only listener for ULoadoutComponent::OnPartsDropped (B-159).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PartsDroppedRecorder.generated.h"

UCLASS(Transient)
class UPartsDroppedRecorder : public UObject
{
	GENERATED_BODY()

public:
	TArray<int32> LoadoutIndices;
	TArray<FText> Messages;

	UFUNCTION()
	void Record(int32 LoadoutIndex, const TArray<FText> &InMessages)
	{
		LoadoutIndices.Add(LoadoutIndex);
		Messages.Append(InMessages);
	}
};
