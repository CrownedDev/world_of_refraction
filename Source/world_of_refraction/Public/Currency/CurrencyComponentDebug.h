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
    /** One-line wallet dump. The scalar currencies (Prismas / Prisms / Diamond) are ALWAYS
     *  shown, including zero balances; Dust and Essence list only NON-zero entries to keep
     *  the line readable. e.g.
     *  "Prismas: 0 | Prisms: 120 | Diamond: 5 | Dust[Fire:50, Mind:12] | Essence[Weapon:30, Crystal:75]".
     *  Returns a "<null>" marker for a null component. */
    UFUNCTION(BlueprintPure, Category = "Currency Debug")
    static FString GetWalletString(const UCurrencyComponent *Wallet);
};
