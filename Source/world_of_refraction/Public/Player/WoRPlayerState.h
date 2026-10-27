// WoRPlayerState.h
// The player's own state (D-046, B-151): one wallet per player, shared by every member of the
// player's party (Gold, Prisms, Diamond, all essences). Set as PlayerStateClass by the C++ game
// modes; a Blueprint game mode must pick it too (its Player State Class).
//
// B-227: on a listen server, levels change by seamless travel and CopyProperties carries every
// player's wallet to the next level's PlayerState.
// B-090: the banked balances ride across OpenLevel and into the save through USaveSubsystem:
// the local player's PlayerState banks them at EndPlay and takes them back at BeginPlay. Gold is
// never banked, so it still resets on a level change.

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

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** B-227: seamless travel hands the wallet to the next level's PlayerState (every player, the
	 *  server's copy of a client's included). */
	virtual void CopyProperties(APlayerState *PlayerState) override;

private:
	/** Only the local player's wallet is banked (an AI or remote PlayerState has none to keep). */
	bool IsLocalPlayerWallet() const;

	UPROPERTY(VisibleAnywhere, Category = "Economy")
	TObjectPtr<UCurrencyComponent> Wallet;
};
