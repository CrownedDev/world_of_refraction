// EncounterComponent.h
// World-space encounter trigger (Cluster T-C1, battle-stage shape): drop on an
// enemy pawn in a trial level. A player pawn entering the trigger sphere opens
// a join window — a Void-tinted ArenaSphere spawns at the encounter midpoint
// and further player pawns entering it during JoinWindowSeconds join LocalParty
// (the arena shrinking per joiner). On expiry the component extracts every
// combatant's authored UCharacterData and hands the roster to
// UTrialRunSubsystem::EnterEncounter — which OpenLevels to the trial's battle
// stage. ABattleGameMode spawns fresh pawns and starts combat over there.
//
// This component spawns NO orchestrator and binds NO combat-end delegate: the
// level swap destroys it (and the arena, and both pawns) — combat teardown and
// the return trip are ABattleGameMode / UTrialRunSubsystem concerns. Retry
// after defeat comes free: the trial level reloads fresh from the map asset.
//
// ⚠️ Requires AUTHORED CharacterData assets on every combatant — the roster
// survives the OpenLevel only as hard asset refs on the GameInstance-scoped
// subsystem (see TrialRunSubsystem.h).
//
// ⚠️ MULTI-INSTANCE: single-combat is structurally enforced here — OpenLevel is
// global, one battle stage at a time. The T-C2 concerns are co-op (seamless
// travel / listen-server instead of hard OpenLevel) and the GI-singleton
// combat services (TurnManager, ActionExecutor, CombatGridSubsystem,
// WeatherStateManager) which hold single-combat state.

#pragma once

#include "AI/EAIDifficulty.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "EncounterComponent.generated.h"

class AStaticMeshActor;
class UMaterialInterface;
class USphereComponent;

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UEncounterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEncounterComponent();

	/** Radius of the runtime-created encounter trigger sphere. */
	UPROPERTY(EditAnywhere, Category = "Encounter", meta = (ClampMin = 50.0))
	float TriggerRadius = 200.0f;

	/** Co-op join window: seconds between first contact and the level swap. */
	UPROPERTY(EditAnywhere, Category = "Encounter", meta = (ClampMin = 0.0))
	float JoinWindowSeconds = 2.0f;

	/** ArenaSphere radius at first contact (one combatant pending). */
	UPROPERTY(EditAnywhere, Category = "Encounter|Arena", meta = (ClampMin = 100.0))
	float ArenaSphereMaxRadius = 800.0f;

	/** Radius lost per additional joiner. */
	UPROPERTY(EditAnywhere, Category = "Encounter|Arena", meta = (ClampMin = 0.0))
	float ArenaSphereShrinkPerCombatant = 100.0f;

	/** Radius floor regardless of joiner count. */
	UPROPERTY(EditAnywhere, Category = "Encounter|Arena", meta = (ClampMin = 100.0))
	float ArenaSphereMinRadius = 400.0f;

	/** Authored translucent arena material (Crown). Unset = engine
	 *  BasicShapeMaterial MID tinted Void — opaque, so only visible as a dome
	 *  from OUTSIDE (backface culling hides it from combatants within). */
	UPROPERTY(EditAnywhere, Category = "Encounter|Arena")
	TSoftObjectPtr<UMaterialInterface> ArenaSphereMaterial;

	/** AI difficulty carried to the battle stage — per-encounter, not per-CharacterData. */
	UPROPERTY(EditAnywhere, Category = "Encounter")
	EAIDifficulty Difficulty = EAIDifficulty::Medium;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Trigger-sphere contact: first player pawn opens the join window. */
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor,
					   UPrimitiveComponent *OtherComp, int32 OtherBodyIndex,
					   bool bFromSweep, const FHitResult &SweepResult);

	/** ArenaSphere contact during the join window: extra player pawns join LocalParty. */
	UFUNCTION()
	void HandleJoin(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor,
					UPrimitiveComponent *OtherComp, int32 OtherBodyIndex,
					bool bFromSweep, const FHitResult &SweepResult);

	/** Extract CharacterData from the pending roster + owner and hand off to
	 *  UTrialRunSubsystem::EnterEncounter (the level swap). */
	void OnJoinWindowExpired();

	/** True if Actor carries a non-AI UCharacterDataComponent (player combatant). */
	static bool IsPlayerCombatant(const AActor *Actor);

	/** Spawn the placeholder arena shell at Center with MaxRadius. */
	void SpawnArenaSphere(const FVector &Center);

	/** Tear down arena + pending state and reopen the trigger (abort path only —
	 *  a successful handoff ends in OpenLevel, which destroys everything). */
	void RearmEncounter();

	/** Runtime-created trigger. Weak: owned by the actor's component tree, not us. */
	TWeakObjectPtr<USphereComponent> Sphere;

	/** Placeholder arena shell (engine sphere mesh, overlap-only while joining). */
	TWeakObjectPtr<AStaticMeshActor> ArenaSphere;

	/** Trial-side player pawns awaiting the swap (co-op joiners during the window). */
	TArray<TWeakObjectPtr<AActor>> PendingLocalParty;

	FTimerHandle JoinWindowTimer;

	/** Latch while the join window runs; cleared only on abort. */
	bool bTriggered = false;
};
