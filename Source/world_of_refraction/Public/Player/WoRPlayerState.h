// WoRPlayerState.h
// The player's own state (D-046, B-151): one wallet per player, shared by every member of the
// player's party (Gold, Prisms, Diamond, all essences). Set as PlayerStateClass by the C++ game
// modes; a Blueprint game mode must pick it too (its Player State Class).
//
// B-227: on a listen server, levels change by seamless travel and CopyProperties carries every
// player's wallet to the next level's PlayerState.
// B-090: the banked balances ride across OpenLevel and into the save through USaveSubsystem:
// the local player's PlayerState banks them at EndPlay and takes them back at BeginPlay. Gold rides
// along as the save's carried run Gold (CarriedGold) and is cleared at run end.
// B-279b: each player's form (skin + Manny / Quinn base) replicates here; the local player's comes
// from UCharacterFormSubsystem (saved there) and every copy dresses its pawn.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Character/Forms/CharacterFormData.h"
#include "WoRPlayerState.generated.h"

class UCurrencyComponent;
class UCharacterData;

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
	virtual void ClientInitialize(AController *Controller) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const override;

	UCharacterFormData *GetWornForm() const { return WornForm; }
	EPlayerBaseForm GetBaseForm() const { return BaseForm; }

	/** B-279b: the server sets the form every player sees; a client asks through ServerSetForm. */
	void SetForm(UCharacterFormData *InWornForm, EPlayerBaseForm InBaseForm);

	/** B-232 (Q-21-audit): this player's own characters; it commands them (URunStateComponent seats). The server
	 *  claims the player's pawn's character when the pawn is set; seamless travel carries the claims. */
	bool ClaimsCharacter(const UCharacterData *Character) const;
	/** Server: Character becomes this player's (no-op for null or one it already claims; refused, with a warning, when
	 *  another player here already claims it). */
	void ClaimCharacter(UCharacterData *Character);
	/** Server: Character is no longer this player's (a pawn swap released it). */
	void ReleaseCharacter(UCharacterData *Character);
	const TArray<TObjectPtr<UCharacterData>> &GetClaimedCharacters() const { return ClaimedCharacters; }

private:
	/** Only the local player's wallet is banked (an AI or remote PlayerState has none to keep). */
	bool IsLocalPlayerWallet() const;

	UFUNCTION(Server, Reliable)
	void ServerSetForm(UCharacterFormData *InWornForm, EPlayerBaseForm InBaseForm);

	/** The local player's form changed in UCharacterFormSubsystem: send it on. */
	UFUNCTION()
	void HandleLocalFormChanged(UCharacterFormData *Form);

	UFUNCTION()
	void HandlePawnSet(APlayerState *Player, APawn *NewPawn, APawn *OldPawn);

	UFUNCTION()
	void OnRep_Form();

	/** Dresses the pawn: the skin's mesh and materials, else the base body. The default (no skin, Manny)
	 *  leaves the authored pawn as it is. // TODO: combat actors too, and V's lighter material (B-143). */
	void ApplyFormLook();

	/** Binds to the local player's form (once the owner is known) and sends it on. */
	void FollowLocalForm();

	/** The pawn this PlayerState last dressed; the default look restores only that one. */
	TWeakObjectPtr<const ACharacter> DressedPawn;

	UPROPERTY(ReplicatedUsing = OnRep_Form)
	TObjectPtr<UCharacterFormData> WornForm;

	UPROPERTY(ReplicatedUsing = OnRep_Form)
	EPlayerBaseForm BaseForm = EPlayerBaseForm::Manny;

	/** B-232: the characters this player commands. */
	UPROPERTY(Replicated)
	TArray<TObjectPtr<UCharacterData>> ClaimedCharacters;

	UPROPERTY(VisibleAnywhere, Category = "Economy")
	TObjectPtr<UCurrencyComponent> Wallet;
};
