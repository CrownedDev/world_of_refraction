// WoRPlayerState.h
// The player's own state (D-046, B-151): one wallet per player, shared by every member of the
// player's party (Gold, Prisms, Diamond, all essences). Set as PlayerStateClass by the C++ game
// modes; a Blueprint game mode must pick it too (its Player State Class).
//
// Not carried across OpenLevel yet: a level change wipes the wallet, as it wiped each
// character's before (TrialData.h). Carrying it is the Pool persistence arc.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "WoRPlayerState.generated.h"

class UCurrencyComponent;

UCLASS()
class WORLD_OF_REFRACTION_API AWoRPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AWoRPlayerState();

	UCurrencyComponent *GetWallet() const { return Wallet; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Economy")
	TObjectPtr<UCurrencyComponent> Wallet;
};
