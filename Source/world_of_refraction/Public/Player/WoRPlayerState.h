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
#include "Save/CoopCheckpoint.h"
#include "Memory/MemoryJournalSubsystem.h" // EMemorySource (B-235d)
#include "Save/WoRSaveTypes.h" // FSavedQuestProgress (B-377b)
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
	/** B-234b: the server sets a joining player's id after AddPlayerState; the join order (AWoRGameState) takes it here. */
	virtual void OnSetUniqueId() override;
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

	/** B-360b (D-150 point 2): the same by party member (FPartyMember::MemberId), so two players holding copies of one
	 *  character each command their own. A battle pawn's member is the id stamped on it; the claims by character above
	 *  stay for a member the party does not know (OBSOLETE once every seat is a party member; delete in the D-124 sweep). */
	bool ClaimsMember(const FGuid &MemberId) const;
	/** Server: the member becomes this player's (no-op for an invalid id or one it already claims; refused, with a
	 *  warning, when another player here already claims it). */
	void ClaimMember(const FGuid &MemberId);
	void ReleaseMember(const FGuid &MemberId);
	const TArray<FGuid> &GetClaimedMembers() const { return ClaimedMembers; }
	/** True when this player commands any member or character (it has a seat). */
	bool HasAnyClaim() const { return ClaimedMembers.Num() > 0 || ClaimedCharacters.Num() > 0; }

	/** D-150 (B-360): ask the host for a seat in its run with Character (your own, picked before joining). On the
	 *  server it goes straight to the run state; a client sends ServerRequestJoin. Console: `wor.Party.RequestJoin`. */
	void RequestJoin(UCharacterData *Character);

	/** B-232e: a client asks the host to recruit the character RecruitOwner plays (a found character in a run; the
	 *  host's UPartyInteractionComponent::InviteForClient runs it, invites only). */
	UFUNCTION(Server, Reliable)
	void ServerRecruit(AActor *RecruitOwner);
	/** B-235b: a map feature's message for this player alone (a Prism pickup), shown on their own screen. */
	UFUNCTION(Client, Reliable)
	void Client_ShowFeatureMessage(const FText &Message);

	/** B-187d: this player's rest press reached the host (ServerInteractFeature), which rested the party; their own
	 *  rest menu opens on their screen. B-187e: with what the host knows of the spot's mini-boss (Revive shows when
	 *  bCanRevive, labelled RevivalNumber). */
	UFUNCTION(Client, Reliable)
	void Client_OpenRestMenu(bool bCanRevive, int32 RevivalNumber);

	/** B-187e: a client pressed Revive in its rest menu. Q-187e (Crown, 2026-11-08): it starts the revive vote
	 *  (AWoRGameState::StartReviveVote) if the mini-boss of the run's last rest spot can be revived and this player stands
	 *  at the rest spot, outside a fight. */
	UFUNCTION(Server, Reliable)
	void ServerReviveMiniBoss();

	/** B-187e (Q-187e): this player's answer to the open revive vote. */
	UFUNCTION(Server, Reliable)
	void ServerCastReviveVote(bool bYes);

	/** B-235c: a client uses a map feature (a chest, a memory spot, the Prism pickup): the host runs it for this
	 *  player's pawn, if it stands in the feature's trigger. */
	UFUNCTION(Server, Reliable)
	void ServerInteractFeature(class ATrialMapFeature *Feature);

	/** B-235d: the host's run unlocked Memory and it qualifies for this player: their own journal records it and only
	 *  they see the notice (D-103: hidden unlocks are private). */
	UFUNCTION(Client, Reliable)
	void Client_UnlockMemory(class UMemoryData *Memory, EMemorySource Source);

	/** B-379: one of this player's characters added to a hidden counter in a fight the host ran; their own save counts it. */
	UFUNCTION(Client, Reliable)
	void Client_AddCounter(FName Counter, class UCharacterData *Character);
	const TArray<TObjectPtr<UCharacterData>> &GetClaimedCharacters() const { return ClaimedCharacters; }
	/** B-235d (Q-235d, Crown): the characters this player plays: the claimed characters and the characters of the
	 *  claimed party members (the host's party knows them). A memory's conditions are read against these for them. */
	TArray<UCharacterData *> GetOwnCharacters() const;
	/** B-235d / B-235e (server): the memories this player has: what their journal reported on joining
	 *  (ServerReportJournal), what they found themselves (ServerReportFoundMemory) and what the host sent them. Each
	 *  memory goes to a player once; their PriorMemory conditions read this. Seamless travel carries it. */
	bool HasMemoryId(FName MemoryId) const { return KnownMemoryIds.Contains(MemoryId); }
	void AddMemoryId(FName MemoryId) { KnownMemoryIds.Add(MemoryId); }
	const TSet<FName> &GetKnownMemoryIds() const { return KnownMemoryIds; }

	/** B-235e: a client tells the host which memories its own journal holds (sent once it owns this state).
	 *  TODO: trusted co-op only: no WithValidation and no cap on the list's size (add both before public matchmaking). */
	UFUNCTION(Server, Reliable)
	void ServerReportJournal(const TArray<FName> &MemoryIds);

	/** B-377b: a client's hidden quest progress, for the host to check its steps (sent on joining and on each change). */
	UFUNCTION(Server, Reliable)
	void ServerReportQuests(const TArray<FSavedQuestProgress> &Quests);

	/** B-377b (server): this player's unfinished quests: quest id -> steps done. */
	const TMap<FName, int32> &GetReportedQuests() const { return ReportedQuests; }

	/** B-377b (server): a gear quest offered to this player once (until they report it started). */
	bool WasQuestOffered(FName QuestId) const { return OfferedQuests.Contains(QuestId); }
	void MarkQuestOffered(FName QuestId) { OfferedQuests.Add(QuestId); }

	/** B-377b: the host saw this player's own part of an event meet QuestId's current step. */
	UFUNCTION(Client, Reliable)
	void Client_QuestStepMet(FName QuestId, int32 StepIndex);

	/** B-377b: this player's worn gear started Quest at an event the host ran. */
	UFUNCTION(Client, Reliable)
	void Client_StartQuest(class UHiddenQuestData *Quest);

	/** B-235e: a client found Memory itself (its own map spot, fragment or event): the host notes it, and shares one for
	 *  every character (it lists none) with itself and the other friends (Q-235d rule 3). */
	UFUNCTION(Server, Reliable)
	void ServerReportFoundMemory(class UMemoryData *Memory, EMemorySource Source);

	/** B-235d debug (`wor.Memory.Sent`): this player's own characters and the memories they have. */
	FString GetSharedMemoriesString() const;

	/** B-234 (host): sends this remote player the host's compressed save as checkpoint CheckpointId, in chunks. */
	void SendCheckpoint(int32 CheckpointId, const TArray<uint8> &Compressed, int32 UncompressedSize);

	/** B-234 debug: the last checkpoint copy this client received and kept (wor.Coop.PrintCheckpoint). */
	FString GetCheckpointString() const;

	/** B-234c review: the host is handing over at a rest spot. Sent on this PlayerState after the checkpoint's chunks
	 *  (reliable calls keep their order on one actor only), so the copy is complete before the client leaves. */
	UFUNCTION(Client, Reliable)
	void Client_BeginPlannedHandOver();

	/** B-234d (server): this player's place in the join order (AWoRGameState::UpdateHostOrder gives it once;
	 *  WoRHostSuccession::NO_JOIN_SEQUENCE until then). CopyProperties carries it across a seamless travel. */
	int32 GetJoinSequence() const { return JoinSequence; }
	void SetJoinSequence(int32 InJoinSequence) { JoinSequence = InJoinSequence; }

private:
	/** B-234: one chunk of the host's save; the last one of a checkpoint writes the copy to WoRCheckpoint::SLOT_NAME. */
	UFUNCTION(Client, Reliable)
	void Client_ReceiveCheckpointChunk(int32 CheckpointId, int32 ChunkIndex, int32 ChunkCount, int32 UncompressedSize, const TArray<uint8> &Chunk);

	FCoopCheckpointAssembler CheckpointAssembler;
	int32 KeptCheckpointId = INDEX_NONE;
	int32 KeptCheckpointBytes = 0;
	FDateTime KeptCheckpointTime;

	/** B-234d: server only, never replicated (clients read the order from AWoRGameState::HostOrder). INDEX_NONE is
	 *  WoRHostSuccession::NO_JOIN_SEQUENCE. */
	int32 JoinSequence = INDEX_NONE;

	/** Only the local player's wallet is banked (an AI or remote PlayerState has none to keep). */
	bool IsLocalPlayerWallet() const;

	UFUNCTION(Server, Reliable)
	void ServerSetForm(UCharacterFormData *InWornForm, EPlayerBaseForm InBaseForm);

	UFUNCTION(Server, Reliable)
	void ServerRequestJoin(UCharacterData *Character);

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

	/** B-360b: the party members this player commands, by FPartyMember::MemberId. */
	UPROPERTY(Replicated)
	TArray<FGuid> ClaimedMembers;

	UPROPERTY(VisibleAnywhere, Category = "Economy")
	TObjectPtr<UCurrencyComponent> Wallet;

	/** B-377b (server only): see GetReportedQuests / WasQuestOffered. */
	TMap<FName, int32> ReportedQuests;
	TSet<FName> OfferedQuests;

	/** B-235d / B-235e (server only): the memory ids this player has (see HasMemoryId). */
	TSet<FName> KnownMemoryIds;
};
