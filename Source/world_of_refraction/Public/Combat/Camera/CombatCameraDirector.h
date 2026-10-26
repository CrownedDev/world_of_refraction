// CombatCameraDirector.h
// Picks which combat camera the viewer sees and in which mode (Design/CombatCamera.md §1-3).
// Lives on the combat orchestrator; driven by turn start and action start. Plays the
// skill's Level Sequence (or its type's template) bound to the caster and targets.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Camera/CombatCameraTypes.h"
#include "CombatCameraDirector.generated.h"

class UCombatCameraComponent;
class UActionExecutor;
class ULevelSequence;
class ULevelSequencePlayer;
class ALevelSequenceActor;
class APlayerController;
struct FAction;

UCLASS(ClassGroup = (Combat))
class WORLD_OF_REFRACTION_API UCombatCameraDirector : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Gives every combatant a camera and starts following turns. Call before the first turn. */
	void BeginCombat(const TArray<AActor *> &LocalParty, const TArray<AActor *> &OpposingParty, UActionExecutor *Executor);

	/** Stops any sequence, removes the cameras and gives the view back. Safe to call twice. */
	void EndCombat();

	// TODO: settings menu (D-024 CommonUI). For now: Details panel or wor.Camera.Style / wor.Camera.MovementOff.
	void SetTurnViewStyle(ETurnViewStyle NewStyle);
	void SetCameraMovementOff(bool bOff);

	bool IsInCombat() const { return bInCombat; }
	ETurnViewStyle GetTurnViewStyle() const { return TurnViewStyle; }
	bool IsCameraMovementOff() const { return bCameraMovementOff; }
	ECombatCameraMode GetCurrentMode() const { return CurrentMode; }
	AActor *GetViewedActor() const { return ViewedActor.Get(); }
	const ULevelSequence *GetPlayingSequence() const { return PlayingSequence; }
	int32 GetCameraCount() const { return Cameras.Num(); }

	/** Turn-view style (D-074 (2)). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Settings")
	ETurnViewStyle TurnViewStyle = ETurnViewStyle::CentredWide;

	/** Motion-sickness setting: one static wide view, no sequences. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Settings")
	bool bCameraMovementOff = false;

	/** Sequence a skill without its own CameraSequence plays, by skill type (B-157 authors them). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Templates")
	TMap<ECameraTemplateKind, TSoftObjectPtr<ULevelSequence>> TemplateSequences;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleActorTurnStarted(AActor *Actor, int32 TurnNumber);

	UFUNCTION()
	void HandleActionStarted(AActor *Executor, const FAction &Action, int32 EnergyCost);

	UFUNCTION()
	void HandleSequenceFinished();

	/** Shows the turn / not-your-turn / static view for the actor whose turn it is. */
	void ShowTurnView();
	ULevelSequence *ResolveSequence(const FAction &Action) const;
	void PlaySequence(ULevelSequence *Sequence, AActor *Caster, const TArray<AActor *> &Targets);
	/** bDeferDestroy: called from the player's own OnFinished, so the actor goes next tick. */
	void StopSequence(bool bDeferDestroy = false);
	FCombatCameraFrame BuildFrame(AActor *Focus) const;
	bool IsViewersCharacter(const AActor *Actor) const;
	APlayerController *GetViewer() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCombatCameraComponent>> Cameras;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> LocalCombatants;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> OpposingCombatants;

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> SequencePlayer;

	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> SequenceActor;

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequence> PlayingSequence;

	TWeakObjectPtr<UActionExecutor> BoundExecutor;
	TWeakObjectPtr<AActor> ViewedActor;
	TWeakObjectPtr<AActor> ViewTargetBeforeCombat;
	ECombatCameraMode CurrentMode = ECombatCameraMode::None;
	bool bInCombat = false;
};
