// FieldMovementComponent.h
// B-214a (D-099, Design/FieldAbilities.md): field movement on the map pawn. AWoRHubController adds it on possess
// (server) and it replicates to the owning client.
//   - The normal dash: everyone, free, on a cooldown.
//   - The active field skill (FFieldSkillChoice), whose home is the party record (FPartyMember::FieldSkill, saved);
//     a pawn with no record keeps it here. Double-tap sprint (IA_Field_MovementSkill) or wor.Field.Activate uses it.
//   - Enhanced Sprint: a faster sprint while on (by the source's element), EP per second while moving at sprint speed
//     (Q-214a, Crown), off at 0 EP or on a second double-tap. UHubMovementLibrary::ComputeHubSprintSpeed reads GetSprintSpeedMultiplier.
// B-214c (moves in FieldMovementMoves.cpp): Glide (a slow fall, toggled in the air), High jump (and one double jump per
// airtime), Air dash (once per airtime along the aim; Lightning blinks), and wall-run while Enhanced Sprint is on.
// B-214d (FieldMovementFlight.cpp): Flight, toggled; EP per second (hovering when still), a height ceiling above the
// take-off ground and Zone.NoFly volumes, each liftable by effects (FFieldFlightLimits).
// B-214e (FieldMovementAdvanced.cpp): Teleport (Void), Phase step, Mark and recall, Super launch, Regroup.
// A client's calls go to the server (it spends the EP and owns the record); the on / off state and the choice replicate.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/HitResult.h"
#include "Engine/TimerHandle.h"
#include "Field/FieldSkillTypes.h"
#include "FieldMovementComponent.generated.h"

class UCharacterDataComponent;
struct FPartyMember;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnhancedSprintChanged, bool, bActive);

UCLASS(ClassGroup = (WoR), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UFieldMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFieldMovementComponent();

	/** The normal dash along the ground (velocity, else facing). Refused on cooldown or on a non-character pawn. */
	UFUNCTION(BlueprintCallable, Category = "Field")
	EFieldSkillRefusal Dash();

	/** Uses the active field skill: Dash dashes; Enhanced Sprint toggles. */
	UFUNCTION(BlueprintCallable, Category = "Field")
	EFieldSkillRefusal ActivateFieldSkill();

	/** Picks the active field skill (FieldSkillRules::CheckPick against the source's element now). Saved on the party
	 *  record when the pawn has one. Stops Enhanced Sprint when the choice changes. */
	UFUNCTION(BlueprintCallable, Category = "Field")
	EFieldSkillRefusal SetActiveFieldSkill(EFieldSkillSource Source, EFieldMovementType Type);

	UFUNCTION(BlueprintPure, Category = "Field")
	FFieldSkillChoice GetActiveFieldSkill() const;

	/** The element Source gives this pawn now (None when it has none). */
	UFUNCTION(BlueprintPure, Category = "Field")
	ESpellElement GetSourceElement(EFieldSkillSource Source) const;

	UFUNCTION(BlueprintPure, Category = "Field")
	bool IsEnhancedSprintActive() const { return bEnhancedSprintActive; }

	/** FieldSkillRules::NO_SPRINT_BOOST unless Enhanced Sprint is on. */
	UFUNCTION(BlueprintPure, Category = "Field")
	float GetSprintSpeedMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Field")
	bool IsDashReady() const;

	UFUNCTION(BlueprintCallable, Category = "Field")
	void StopEnhancedSprint();

	/** One Enhanced Sprint EP charge (the drain timer's step; public for the spec), charged only while the pawn moves at
	 *  sprint speed (Q-214a, Crown). Stops it below one charge. */
	void DrainEnhancedSprint();

	/** The pawn's real ground speed is a sprint (FieldSkillRules::IsMovingAtSprintSpeed). */
	UFUNCTION(BlueprintPure, Category = "Field")
	bool IsMovingAtSprintSpeed() const;

	// ---- B-214c: ground and air moves ----

	/** Glide on (in the air, paid once) or off. */
	UFUNCTION(BlueprintCallable, Category = "Field")
	EFieldSkillRefusal ToggleGlide();

	UFUNCTION(BlueprintPure, Category = "Field")
	bool IsGliding() const { return bGliding; }

	/** A high jump from the ground, or the one double jump in the air. */
	UFUNCTION(BlueprintCallable, Category = "Field")
	EFieldSkillRefusal HighJump();

	/** The air dash along the aim (Lightning: blinks), once per airtime. */
	UFUNCTION(BlueprintCallable, Category = "Field")
	EFieldSkillRefusal AirDash();

	UFUNCTION(BlueprintPure, Category = "Field")
	bool IsWallRunning() const { return bWallRunning; }

	/** Landing: the double jump, air dash and wall-run come back; glide and wall-run end. Public for the spec. */
	void NotifyLanded();

	/** One wall-run step with the wall test given (StepMoves traces for it). Public for the spec. */
	void ApplyWallRunStep(bool bWallBeside, const FVector &WallNormal);

	// ---- B-214d: flight ----

	/** Flight on (paid one second, outside a no-fly zone) or off (it falls). */
	UFUNCTION(BlueprintCallable, Category = "Field")
	EFieldSkillRefusal ToggleFlight();

	UFUNCTION(BlueprintPure, Category = "Field")
	bool IsFlying() const { return bFlying; }

	/** What effects lift (unlimited flight, no ceiling, no-fly ignored). Server. */
	UFUNCTION(BlueprintCallable, Category = "Field")
	void SetFlightLimits(const FFieldFlightLimits &NewLimits);

	UFUNCTION(BlueprintPure, Category = "Field")
	FFieldFlightLimits GetFlightLimits() const { return FlightLimits; }

	/** Up (+1) or down (-1) while flying: movement input along the world's up. B-214g binds it to keys
	 *  (IA_Field_FlyUp / IA_Field_FlyDown, InputMap rows with that job); the pawn's own move input stays flat. */
	UFUNCTION(BlueprintCallable, Category = "Field")
	void AddFlightVerticalInput(float Value);

	/** Debug (wor.Field.FlyVertical, until the keys): a vertical push while flying. Server. */
	void NudgeFlightVertical(float Direction);

	/** Inside a volume tagged Zone.NoFly. */
	UFUNCTION(BlueprintPure, Category = "Field")
	bool IsInNoFlyZone() const;

	/** One flight EP charge (the drain timer's step; public for the spec). Lands it below one charge. */
	void DrainFlight();

	/** One flight step: out of a no-fly zone and under the ceiling (StepMoves calls it; public for the spec). */
	void ApplyFlightStep();

	// ---- B-214e: advanced moves ----

	/** Uses Type if it is the active field skill: Teleport, PhaseStep, MarkAndRecall (mark, then recall), SuperLaunch,
	 *  Regroup. Server; a client uses ActivateFieldSkill. */
	UFUNCTION(BlueprintCallable, Category = "Field")
	EFieldSkillRefusal UseAdvancedMove(EFieldMovementType Type);

	UFUNCTION(BlueprintPure, Category = "Field")
	bool HasRecallMark() const { return bHasRecallMark; }

	/** Server: copies the party record's choice into the replicated mirror (after the leader is rebound). */
	void RefreshFromRecord();

	/** "Hero: skill Innate EnhancedSprint (Lightning) | sprint ON x1.50 | EP 7/10 | dash ready | record yes" */
	FString GetFieldStateString() const;

	UPROPERTY(BlueprintAssignable, Category = "Field")
	FOnEnhancedSprintChanged OnEnhancedSprintChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION(Server, Reliable)
	void ServerDash();

	UFUNCTION(Server, Reliable)
	void ServerActivateFieldSkill();

	UFUNCTION(Server, Reliable)
	void ServerSetActiveFieldSkill(EFieldSkillSource Source, EFieldMovementType Type);

	UFUNCTION()
	void OnRep_EnhancedSprintActive();

	UFUNCTION()
	void OnRep_AirMoveState();

	UFUNCTION()
	void OnRep_Flying();

	void StopFlight();

	EFieldSkillRefusal DoTeleport(ACharacter &Character);
	EFieldSkillRefusal DoPhaseStep(ACharacter &Character);
	EFieldSkillRefusal DoRecall(ACharacter &Character);
	EFieldSkillRefusal DoSuperLaunch(ACharacter &Character);
	EFieldSkillRefusal DoRegroup(ACharacter &Character);

	/** The other party member's field pawn farthest away (Regroup's target), or null. */
	AActor *FindRegroupTarget() const;

	/** A fight is running (UPartySessionSubsystem::IsFightRunning). */
	bool IsFightRunning() const;

	/** Flying movement on / off with the base speed and braking put back (server, and the owning client by OnRep). */
	void ApplyFlightMode();

	UFUNCTION()
	void HandleLanded(const FHitResult &Hit);

	/** Type is the active choice and may still be used (its source's element now, the unlock): CheckPick again. */
	EFieldSkillRefusal CheckActiveChoiceStillPickable(EFieldMovementType Type) const;

	/** Glide's fall cap and the wall-run check, every MOVE_STEP_INTERVAL while gliding or Enhanced Sprint is on. */
	void StepMoves();
	void UpdateMoveStepTimer();

	/** Gravity and air control for the glide / wall-run state (server, and the owning client by OnRep). */
	void ApplyAirMoveState();

	void SetGliding(bool bOn);
	void SetWallRunning(bool bOn);

	/** True when the pawn has its EP: spends it (server). */
	bool SpendFieldEP(int32 Amount);

	EFieldSkillRefusal StartEnhancedSprint();
	void SetEnhancedSprintActive(bool bActive);

	/** Re-reads the sprint speed when the pawn is sprinting (the pawn Blueprint sets it on sprint press). */
	void ReapplySprintSpeed() const;

	bool HasAuthority() const;
	UCharacterDataComponent *GetCharacterData() const;
	const FPartyMember *FindRecord() const;

	/** The choice for a pawn with no party record, and the replicated mirror of the record's for the owning client. */
	UPROPERTY(Replicated)
	FFieldSkillChoice ActiveChoice;

	UPROPERTY(ReplicatedUsing = OnRep_EnhancedSprintActive)
	bool bEnhancedSprintActive = false;

	/** The element Enhanced Sprint started with (its speed); replicated so the client's sprint speed matches. */
	UPROPERTY(Replicated)
	ESpellElement EnhancedSprintElement = ESpellElement::None;

	UPROPERTY(ReplicatedUsing = OnRep_AirMoveState)
	bool bGliding = false;

	UPROPERTY(ReplicatedUsing = OnRep_AirMoveState)
	bool bWallRunning = false;

	/** Server: used since the last landing. */
	bool bAirJumpUsed = false;
	bool bAirDashUsed = false;
	bool bWallRunSpent = false;
	float WallRunSeconds = 0.0f;

	/** The movement's own gravity and air control, put back when glide / wall-run end (read at BeginPlay). */
	float BaseGravityScale = 1.0f;
	float BaseAirControl = 0.0f;
	bool bHasBaseMovement = false;

	FTimerHandle MoveStepTimer;

	UPROPERTY(ReplicatedUsing = OnRep_Flying)
	bool bFlying = false;

	UPROPERTY(Replicated)
	FFieldFlightLimits FlightLimits;
	/** The ground under the take-off: the ceiling counts from it (FieldSkillRules::GetFlightCeilingZ). */
	double FlightTakeOffGroundZ = 0.0;
	float BaseMaxFlySpeed = 0.0f;
	float BaseBrakingFlying = 0.0f;
	FTimerHandle FlightDrainTimer;

	/** B-214e, server: Mark and recall's mark (one at a time, this level only). */
	bool bHasRecallMark = false;
	FVector RecallMark = FVector::ZeroVector;

	/** Super launch glides from the top of its rise (no glide EP). */
	bool bSuperLaunchGlidePending = false;

	/** When each advanced move was last used (its cooldown). */
	TMap<EFieldMovementType, double> LastAdvancedUseTime;

	double LastDashTime = -FieldSkillRules::DASH_COOLDOWN_SECONDS;
	FTimerHandle DrainTimer;
	mutable TWeakObjectPtr<UCharacterDataComponent> CachedCharacterData;
};
