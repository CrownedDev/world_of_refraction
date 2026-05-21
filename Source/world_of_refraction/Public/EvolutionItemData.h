// EvolutionItemData.h
// Placeholder UCLASS for the commit-3 rename in the crystal/evolution refactor
// sequence. Inherits UItemData unchanged — exists only as a forward-declarable
// symbol that future code can target. Commit 3 swaps the inheritance: this
// class becomes standalone and UItemData is deleted.

#pragma once

#include "CoreMinimal.h"
#include "ItemData.h"
#include "EvolutionItemData.generated.h"

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UEvolutionItemData : public UItemData
{
    GENERATED_BODY()

public:
    UEvolutionItemData();
};
