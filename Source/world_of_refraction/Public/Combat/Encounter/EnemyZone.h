// EnemyZone.h
// B-186 (Enemies §4, Q-186): a box on the trial map that sets how strong the enemies fought
// inside it are (its tier) and the loot pool its enemies and chests roll from. Nearer the boss is
// tougher; nothing scales to the party. Overlapping zones: the highest tier wins.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "EnemyZone.generated.h"

class UBoxComponent;

/** What a zone hands to a fight started inside it. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FEncounterZoneInfo
{
	GENERATED_BODY()

	/** 0 = outside every zone (base strength); up to EnemyZoneConstants::MAX_ZONE_TIER. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone", meta = (ClampMin = 0, ClampMax = 4))
	int32 Tier = 0;

	/** Source tag (Resources_Design §7.9) the zone's drops and chests roll from.
	 *  TODO: read by chests (B-188) and enemy drops (B-073). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	FGameplayTag LootPool;
};

namespace EnemyZoneConstants
{
	constexpr int32 MAX_ZONE_TIER = 4;
}

UCLASS()
class WORLD_OF_REFRACTION_API AEnemyZone : public AActor
{
	GENERATED_BODY()

public:
	AEnemyZone();

	UPROPERTY(VisibleAnywhere, Category = "Zone")
	TObjectPtr<UBoxComponent> Area;

	/** Tier (0-4) and loot pool of the fights started inside. */
	UPROPERTY(EditAnywhere, Category = "Zone", meta = (ShowOnlyInnerProperties))
	FEncounterZoneInfo Zone;

	/** True when Location is inside the box. */
	bool Contains(const FVector &Location) const;

	/** The zone a fight at Location takes: the highest-tier zone containing it, or tier 0. */
	static FEncounterZoneInfo FindZoneAt(const UWorld *World, const FVector &Location);

	/** Enemies' max HP and damage, × this, for a zone tier (clamped to 0..MAX_ZONE_TIER). */
	static float GetTierStrengthMultiplier(int32 Tier);

	FString GetEnemyZoneString() const;
};
