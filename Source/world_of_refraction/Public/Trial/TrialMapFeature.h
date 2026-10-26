// TrialMapFeature.h
// B-188 (RunStructure §4): something on the trial map you walk up to and use (Press E), like the
// trial door and the merchant: a chest, a lore object, the boss arena. The exploration controller
// interacts with the nearest one in range. What it found is recorded on the run
// (UTrialRunSubsystem::RecordFound, shown by wor.Trial.Print).

#pragma once

#include "CoreMinimal.h"
#include "AI/EAIDifficulty.h"
#include "GameFramework/Actor.h"
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

	/** The nearest feature whose trigger Pawn stands in, or null. */
	static ATrialMapFeature *FindNearestInRange(APawn *Pawn);

	/** The key the run records it under: the placed actor's path, PIE prefix removed (stable across
	 *  the trial level's reloads). */
	FName GetFeatureId() const;

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

	/** The pool this chest rolls: its own when set, else the zone it stands in (AEnemyZone). */
	FGameplayTag GetLootPool() const;

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

	/** The boss this arena fights now, or null. */
	UCharacterData *GetBoss() const;

protected:
	virtual FString GetHintText() const override;

	/** Fought when the run has no variation boss (a trial without variations). The fight needs an
	 *  active trial, so enter through the trial door. */
	UPROPERTY(EditInstanceOnly, Category = "Boss")
	TObjectPtr<UCharacterData> FallbackBoss;

	UPROPERTY(EditInstanceOnly, Category = "Boss")
	EAIDifficulty Difficulty = EAIDifficulty::Hard;
};
