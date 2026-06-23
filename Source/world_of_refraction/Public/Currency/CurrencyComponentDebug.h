// CurrencyComponentDebug.h
// Debug formatting for UCurrencyComponent wallets (Resources_Design.md). Matches the
// project debug-tool pattern (CharacterDataDebug): a UBlueprintFunctionLibrary of static
// helpers. This file holds the FORMATTING; the component owns the CallInEditor "Print
// Wallet" button, which calls in here — so the split stays clean (Debug = formatting,
// component = the button + log/on-screen surfacing).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CurrencyComponentDebug.generated.h"

class UCurrencyComponent;

/**
 * Debug utilities for UCurrencyComponent.
 */
UCLASS()
class WORLD_OF_REFRACTION_API UCurrencyComponentDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** One-line wallet dump. The scalar currencies (Gold / Prisms / Diamond / GearEssence) are
     *  ALWAYS shown, including zero balances; the typed Essence list shows only NON-zero entries
     *  to keep the line readable. e.g.
     *  "Gold: 0 | Prisms: 120 | Diamond: 5 | GearEssence: 40 | Essence[Fire:50, Mind:12]".
     *  Returns a "<null>" marker for a null component. */
    UFUNCTION(BlueprintPure, Category = "Currency Debug")
    static FString GetWalletString(const UCurrencyComponent *Wallet);
};
