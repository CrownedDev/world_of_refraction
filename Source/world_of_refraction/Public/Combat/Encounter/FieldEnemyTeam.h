// FieldEnemyTeam.h
// B-184 (Enemies §2-3): an enemy team placed on a trial map. It stands still or walks a waypoint
// patrol; touching it starts the fight with its team (UEncounterComponent, whose EncounterData
// holds the team, B-074). A team beaten in this run stays gone when the trial level reloads.
// Placeholder look: an engine cylinder until enemy art lands.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FieldEnemyTeam.generated.h"

class UEncounterComponent;
class UInterpToMovementComponent;
class UStaticMeshComponent;

UCLASS()
class WORLD_OF_REFRACTION_API AFieldEnemyTeam : public AActor
{
	GENERATED_BODY()

public:
	AFieldEnemyTeam();

	/** The team, its AI level and rank: set them on this component (Encounter > Encounter Data). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter")
	TObjectPtr<UEncounterComponent> Encounter;

	/** Patrol waypoints, relative to where the team is placed. Empty = it stands still.
	 *  It walks them in order and back. */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (MakeEditWidget = true))
	TArray<FVector> PatrolPoints;

	/** Walking speed along the patrol, in units per second. */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = 1.0))
	float PatrolSpeed = 150.0f;

	/** Debug: team, patrol and whether it is beaten this run. */
	FString GetFieldEnemyTeamString() const;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> TeamMesh;

	UPROPERTY(VisibleAnywhere, Category = "Patrol")
	TObjectPtr<UInterpToMovementComponent> Patrol;

	/** Feed PatrolPoints to the movement component; idle when there are none. */
	void StartPatrol();
};
