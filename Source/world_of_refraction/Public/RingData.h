// RingData.h
// Ring data asset for Resonator class spell casting

#pragma once

#include "CoreMinimal.h"
#include "EquipmentDataBase.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "RingData.generated.h"

class UStaticMesh;

/**
 * Ring Data Asset - Resonator's primary spell source
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API URingData : public UEquipmentDataBase
{
	GENERATED_BODY()

public:
	// ==================== RING ====================

	// If true, spells cannot be customized (conjured-ring equivalent of bAbilitiesLocked)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ring")
	bool bSpellsLocked = false;

	// ==================== MESH ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	UStaticMesh *RingStaticMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	FRotator MeshRotation = FRotator::ZeroRotator;

	// ==================== UTILITY ====================

	virtual int32 GetMaxSpells() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};
