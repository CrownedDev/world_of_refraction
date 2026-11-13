// TrialMapFeature.h
// B-188 (RunStructure §4): something on the trial map you walk up to and use (Press E), like the
// trial door and the merchant: a chest, a lore object, the boss arena. The exploration controller
// interacts with the nearest one in range. What it found is recorded on the run
// (UTrialRunSubsystem::RecordFound, shown by wor.Trial.Print).

#pragma once

#include "CoreMinimal.h"
#include "AI/EAIDifficulty.h"
#include "GameFramework/Actor.h"
#include "Hub/InteractPrompt.h"
#include "GameplayTagContainer.h"
#include "TrialMapFeature.generated.h"

class UBoxComponent;
class UCharacterData;
class UStaticMeshComponent;
class UTrialRunSubsystem;

UCLASS(Abstract)
class WORLD_OF_REFRACTION_API ATrialMapFeature : public AActor
{
	GENERATED_BODY()

public:
	ATrialMapFeature();

	/** Use the feature (the controller's interact key). */
	virtual void Interact(APawn *Pawn) PURE_VIRTUAL(ATrialMapFeature::Interact, );

	/** B-443: what the floating prompt says while the player stands in range. None by default: a feature says
	 *  nothing unless it means to (a memory spot stays unannounced). */
	virtual FInteractPrompt GetPrompt() const { return FInteractPrompt(); }

	/** The nearest feature whose trigger Pawn stands in, or null. */
	static ATrialMapFeature *FindNearestInRange(APawn *Pawn);

	/** A line on screen for a few seconds (what a feature gave or why it refused). Public (B-235b): a player's own
	 *  message arrives through AWoRPlayerState::Client_ShowFeatureMessage. */
	static void ShowMessage(const FText &Message);

	/** B-235c: Message on Pawn's player's own screen (the host's here, a client's by Client RPC). */
	static void ShowMessageTo(const APawn *Pawn, const FText &Message);

	/** The key the run records it under: the placed actor's path, PIE prefix removed (stable across
	 *  the trial level's reloads). */
	FName GetFeatureId() const;

	/** B-232d (client): the run's progress arrived (URunStateComponent): a feature already found takes its found
	 *  state (spent, removed, opened hint). Default: redraw the hint. */
	virtual void RefreshFromRun() { RefreshHint(); }
	/** B-235c (server): Pawn stands in this feature's trigger (any player's pawn; the host's overlap). */
	bool IsWithinReach(const APawn *Pawn) const;

	/** B-235c (D-103: personal loot): the key a per-player feature records for Pawn's player: the feature id for
	 *  the host's (or solo) player, so solo and saves are unchanged; "<id>@<player key>" for another player. */
	FName GetFeatureIdFor(const APawn *Pawn) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The on-screen hint while the player stands in range. */
	virtual FString GetHintText() const PURE_VIRTUAL(ATrialMapFeature::GetHintText, return FString(););

	/** Refresh or clear the hint after the feature's state changed. */
	void RefreshHint();

	UTrialRunSubsystem *GetTrialRun() const;


	UPROPERTY(VisibleAnywhere, Category = "Feature")
	TObjectPtr<UStaticMeshComponent> FeatureMesh;

	UPROPERTY(VisibleAnywhere, Category = "Feature")
	TObjectPtr<UBoxComponent> InteractionTrigger;

private:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor,
							   UPrimitiveComponent *OtherComp, int32 OtherBodyIndex,
							   bool bFromSweep, const FHitResult &SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor,
							 UPrimitiveComponent *OtherComp, int32 OtherBodyIndex);

	TWeakObjectPtr<APawn> PawnInRange;
};

/** Opens once per run and gives one item rolled from its loot pool (Q-188). */
UCLASS()
class WORLD_OF_REFRACTION_API ATrialChest : public ATrialMapFeature
{
	GENERATED_BODY()

public:
	virtual void Interact(APawn *Pawn) override;
	virtual FInteractPrompt GetPrompt() const override;

	/** The pool this chest rolls: its own when set, else the zone it stands in (AEnemyZone). */
	FGameplayTag GetLootPool() const;

	/** This machine's player has opened it this run (a client reads its own copy's record). */
	bool IsOpenedForLocalPlayer() const;

protected:
	virtual FString GetHintText() const override;

	/** Optional: a pool of its own, overriding the zone's (a special chest). */
	UPROPERTY(EditInstanceOnly, Category = "Chest", meta = (Categories = "Source"))
	FGameplayTag LootPool;
};

/** Read once and its entry goes into the run's found list: the placeholder journal (Q-188). */
UCLASS()
class WORLD_OF_REFRACTION_API ATrialLoreObject : public ATrialMapFeature
{
	GENERATED_BODY()

public:
	ATrialLoreObject();

	virtual void Interact(APawn *Pawn) override;
	virtual FInteractPrompt GetPrompt() const override;

protected:
	virtual FString GetHintText() const override;

	UPROPERTY(EditInstanceOnly, Category = "Lore")
	FText Title;

	UPROPERTY(EditInstanceOnly, Category = "Lore", meta = (MultiLine = true))
	FText Body;
};

/** The way into the boss fight: pressing interact is the player's choice to start it (Q-188).
 *  Fights the chosen variation's boss (FTrialVariation::Boss), else FallbackBoss. Beaten for
 *  the rest of the run once won. */
UCLASS()
class WORLD_OF_REFRACTION_API ABossArenaTrigger : public ATrialMapFeature
{
	GENERATED_BODY()

public:
	virtual void Interact(APawn *Pawn) override;
	virtual FInteractPrompt GetPrompt() const override;

	/** The boss this arena fights now, or null. */
	UCharacterData *GetBoss() const;

protected:
	virtual FString GetHintText() const override;

	/** Fought when the run has no variation boss (a trial without variations). The fight needs an
	 *  active trial, so enter through the trial door. */
	UPROPERTY(EditInstanceOnly, Category = "Boss")
	TObjectPtr<UCharacterData> FallbackBoss;

	/** B-344 (D-136, Q-344): set to give this boss its own AI tier; otherwise the trial difficulty picks it (bosses
	 *  still harden by phase, B-193). */
	UPROPERTY(EditInstanceOnly, Category = "Boss")
	bool bOverrideAIDifficulty = false;

	UPROPERTY(EditInstanceOnly, Category = "Boss", meta = (EditCondition = "bOverrideAIDifficulty"))
	EAIDifficulty Difficulty = EAIDifficulty::Hard;
};
