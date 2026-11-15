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
// B-214f (FieldMovementAttack.cpp): the field attack: a swing ahead; hitting an encounter starts its fight with first
// strike and the hit's buildup (the element and physical type of the entry in use, Ice held with Water).
// B-503 (D-215): the entry in use is one stop of a cycle through the whole loadout (weapons, rings, the innate element,
// evolutions), replacing B-214g's Innate / Weapon / Ring switch; a weapon swings, every other entry throws the pulse.
// B-214h: the evolution on the character's own slot adds an always-on sprint bonus and augments the field skill (cheaper
// EP, flight limits it lifts).
// A client's calls go to the server (it spends the EP and owns the record); the on / off state and the choice replicate.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/HitResult.h"
#include "Engine/TimerHandle.h"
#include "Field/FieldSkillTypes.h"
#include "FieldMovementComponent.generated.h"

class UAnimMontage;
class UCharacterDataComponent;
struct FPartyMember;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnhancedSprintChanged, bool, bActive);

UCLASS(Config = Game, ClassGroup = (WoR), meta = (BlueprintSpawnableComponent))
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

	/** Enhanced Sprint's element multiplier while on (else NO_SPRINT_BOOST), times the seated evolution's always-on
	 *  bonus (B-214h). */
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

	/** Up (+1) or down (-1) while flying: movement input along the world's up. B-214g binds it to IA_Field_FlyVertical
	 *  (a 1D axis, Q-214g); the pawn's own move input stays flat. */
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

	// ---- B-214f: the field attack ----

	/** A swing ahead (bHeld: the attack was held). Striking an encounter starts its fight (UEncounterComponent::
	 *  StrikeFromField); a miss is just a swing. Refused on cooldown and while a swing's hit is still to land.
	 *  B-503b: a weapon entry with a swing montage plays it on every machine and hits at its Impact notify
	 *  (FieldSkillRules::GetSwingImpactSeconds); with none the hit lands at once.
	 *  B-503c: every other entry throws the pulse: FieldPulseMontage the same way, then a shot
	 *  FieldSkillRules::FIELD_PULSE_RANGE ahead in the strike's colour. */
	UFUNCTION(BlueprintCallable, Category = "Field")
	EFieldSkillRefusal FieldAttack(bool bHeld);

	/** B-503k (D-217): the stops of the field attack's cycle, its infusions. A weapon user (Generic) swings the
	 *  loadout's active weapon: plain first, then infused with each element its combat infusion sources give
	 *  (UActionExecutor::GetAvailableInfusionSources). A Refractor or Resonator throws the pulse in each of those
	 *  elements, no plain stop. One stop per element. Empty for a caster with no element.
	 *  B-503l (Q-503l): a Reality stop (innate or evolution) or a Broken Darkness character's Darkness stop is followed
	 *  by its hold steps (FFieldAttackEntry::bHoldStep), one per element of the other stops: "(R) Fire", "Dark Fire". */
	UFUNCTION(BlueprintPure, Category = "Field")
	TArray<FFieldAttackEntry> GetFieldAttackEntries() const;

	/** The stop in use, within GetFieldAttackEntries (FieldSkillRules::ClampFieldAttackIndex). */
	UFUNCTION(BlueprintPure, Category = "Field")
	int32 GetFieldAttackIndex() const;

	/** The entry in use; the plain swing (no name, no element) with nothing to cycle. */
	UFUNCTION(BlueprintPure, Category = "Field")
	FFieldAttackEntry GetFieldAttackEntry() const;

	/** Steps to the next stop, wrapping (IA_Field_SwitchSource: LB / R), and returns it. Kept on the party record
	 *  when the pawn has one. B-503l (D-217): a press (bHold false) steps the source, skipping hold steps; a hold the
	 *  element within it (FieldSkillRules::NextFieldAttackHoldIndex; no change for a one-element source).
	 *  B-503m3: the keys' hold opens the wheel now (SelectFieldAttack); bHold is left for wor.Field.CycleAttack. */
	UFUNCTION(BlueprintCallable, Category = "Field")
	FFieldAttackEntry CycleFieldAttack(bool bHold = false);

	/** B-503m3 (D-217): the wheel's pick: makes that stop (an index into GetFieldAttackEntries, clamped) the one in use,
	 *  kept as CycleFieldAttack keeps it, and returns it. */
	UFUNCTION(BlueprintCallable, Category = "Field")
	FFieldAttackEntry SelectFieldAttack(int32 Index);

	/** What a field hit now would carry: the entry in use's element and physical type
	 *  (FieldSkillRules::MakeFieldStrike). */
	UFUNCTION(BlueprintPure, Category = "Field")
	FFieldStrike GetFieldStrike(bool bHeld) const;

	/** B-503c (D-215): the pulse's cast animation, one for every entry that isn't a weapon; its first Impact notify is
	 *  the release. Unset (DefaultGame.ini): the pulse leaves at once with no animation. Public for the spec. */
	UPROPERTY(Config)
	TSoftObjectPtr<UAnimMontage> FieldPulseMontage;

	/** B-503c: the pulse's burst, spawned at the hand facing the end point, with the user parameters PulseColor
	 *  (linear colour) and PulseEnd (world position) set. Unset: a plain line and ball in the strike's colour. */
	UPROPERTY(Config)
	TSoftObjectPtr<class UNiagaraSystem> FieldPulseVFX;

	// ---- B-214h: evolutions ----

	/** The evolution on the character's own slot, unless broken (null with none). */
	const class UEvolutionItemData *GetSeatedEvolution() const;

	/** A field EP cost after the seated evolution's discount (FieldSkillRules::ApplyFieldEPDiscount). */
	UFUNCTION(BlueprintPure, Category = "Field")
	int32 GetFieldEPCost(int32 BaseCost) const;

	/** The EP to show a start check for a BaseCost move: enough when the discounted cost is affordable (the checks
	 *  compare against the full cost), else the real EP. */
	int32 GetEPForCostCheck(int32 BaseCost) const;

	/** " | evolution <name> (sprint +15%, field EP -25%)" for the debug line. */
	FString GetEvolutionStateString() const;

	/** Flight's limits: the ones effects set (SetFlightLimits) and the seated evolution's, combined. */
	UFUNCTION(BlueprintPure, Category = "Field")
	FFieldFlightLimits GetEffectiveFlightLimits() const;

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
	void ServerFieldAttack(bool bHeld);

	/** B-503b: the swing, or the pulse's cast, on every machine (a look only: the host times the hit itself). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayFieldSwing(UAnimMontage *Montage);

	/** B-503c: the pulse's look on every machine, from the hand to End in Colour: FieldPulseVFX, or a plain line and
	 *  ball until it is authored. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayFieldPulse(FVector_NetQuantize Start, FVector_NetQuantize End, FLinearColor Colour);

	/** The hit: the sweep ahead (a swing 2 m, a pulse 7 m), engaging the first encounter body with Strike (server). */
	void ResolveFieldAttack(FFieldStrike Strike, bool bSwing);

	UFUNCTION(Server, Reliable)
	void ServerSetFieldAttackIndex(int32 Index);

	/** The host kept a different index than the owning client asked for (it clamps to its own reading of the loadout). */
	UFUNCTION(Client, Reliable)
	void ClientSetFieldAttackIndex(int32 Index);

	/** Keeps Index (clamped to the entries now): on the party record when the pawn has one, and in the mirror. */
	void StoreFieldAttackIndex(int32 Index);

	/** The index as kept (the record's on the host, else the mirror), not yet clamped to the entries. */
	int32 GetKeptFieldAttackIndex() const;

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

	/** The farthest other party member's field pawn (a pawn stamped with a party member id, or player-controlled), or
	 *  null: Regroup's target. */
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
	UPROPERTY(Replicated)
	bool bHasRecallMark = false;
	FVector RecallMark = FVector::ZeroVector;

	/** Super launch glides from the top of its rise (no glide EP). */
	bool bSuperLaunchGlidePending = false;

	/** B-503 (D-215): the field attack's stop for a pawn with no party record, and the replicated mirror of the
	 *  record's (FPartyMember::FieldAttackIndex) for the owning client, which sets it at once when it cycles. */
	UPROPERTY(Replicated)
	int32 FieldAttackIndex = FieldSkillRules::FIRST_FIELD_ATTACK_INDEX;

	double LastFieldAttackTime = -FieldSkillRules::FIELD_ATTACK_COOLDOWN_SECONDS;
	/** B-503b, server: running from the swing's (or the cast's) start to its hit. */
	FTimerHandle FieldSwingTimer;

	/** When each advanced move was last used (its cooldown). */
	TMap<EFieldMovementType, double> LastAdvancedUseTime;

	double LastDashTime = -FieldSkillRules::DASH_COOLDOWN_SECONDS;
	FTimerHandle DrainTimer;
	mutable TWeakObjectPtr<UCharacterDataComponent> CachedCharacterData;
	mutable TWeakObjectPtr<class ULoadoutComponent> CachedLoadout; // B-214h: the seated evolution's lookup
};
