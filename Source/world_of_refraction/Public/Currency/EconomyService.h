// EconomyService.h
// Cross-system orchestrator for the dismantle / purchase economy. A GameInstanceSubsystem
// (matches the DamageCalculator / ActionExecutor pattern per CLAUDE.md) so it can be reached
// globally and coordinate an owner's inventory <-> wallet WITHOUT coupling those components to
// each other — it resolves both off the owner actor via FindComponentByClass and drives them.
//
// Yield math + essence-type resolution live in the stateless EconomyYield helper; this service
// owns the orchestration (authority gate, availability check, remove-then-grant ordering).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "EconomyService.generated.h"

class AActor;
class UCrystalInventoryComponent;
class UCurrencyComponent;

UCLASS()
class WORLD_OF_REFRACTION_API UEconomyService : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /**
     * Dismantle Count crystals/stones of Id from Owner's chosen pool (Item vs Refined per
     * bRefined), granting typed essence per EconomyYield. Server-authoritative.
     *
     * Order is REMOVE-then-GRANT: a failed/insufficient removal never grants phantom essence,
     * and the grant is sized to what was actually removed.
     *
     * Returns false if: Owner null / Count<=0; no authority; CrystalInventoryComponent or
     * CurrencyComponent missing; insufficient count in the chosen pool; or removal returned 0.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool DismantleCrystal(AActor *Owner, const FCrystalId &Id, int32 Count = 1, bool bRefined = false);
};
