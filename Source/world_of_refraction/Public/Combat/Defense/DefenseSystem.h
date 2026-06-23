// DefenseSystem.h
// Real-time defense system for Block/Parry/Dodge mechanics

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Defense/EDefenseDirection.h"
#include "Combat/Defense/DefenseDifficulty.h"
#include "Character/CharacterData.h"
#include "Combat/Actions/EActionType.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Character.h"
#include "DefenseSystem.generated.h"

class UCharacterDataComponent;

// ========================================
// STRUCTS
// ========================================

/**
 * Result of a defense attempt
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FDefenseResult
{
	GENERATED_BODY()

	/** Was defense window active when input received? */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	bool bWasInWindow = false;

	/** Type of defense attempted */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	EDefenseType DefenseType = EDefenseType::None;

	/** Was the defense successful? */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	bool bSuccess = false;

	/** Damage after defense reduction */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	int32 FinalDamage = 0;

	/** Damage reflected back to attacker (Parry only) */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	int32 ReflectedDamage = 0;

	/** Why dodge failed (if applicable) */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	FString FailureReason;
};

/**
 * One timestamped defense input (Stage 3). The buffer replaces the one-shot
 * DefenseChosen/bInputReceived model: each press APPENDS an entry, and each impact
 * frame match-and-consumes the latest unconsumed entry whose timing falls in window.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FTimestampedDefenseInput
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	EDefenseType Type = EDefenseType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	EDefenseDirection Direction = EDefenseDirection::None;

	/** FPlatformTime::Seconds() at the moment of the press. */
	double InputTime = 0.0;

	/** Set once an impact frame has matched-and-consumed this entry. */
	bool bConsumed = false;
};

/**
 * Result of matching one impact frame against the input buffer (Stage 3).
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FDefenseInputMatch
{
	GENERATED_BODY()

	/** Did an unconsumed in-window input match this impact? */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	bool bMatched = false;

	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	EDefenseType Type = EDefenseType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	EDefenseDirection Direction = EDefenseDirection::None;

	/** True when the match landed inside PerfectThreshold of the impact. */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	bool bPerfect = false;
};

/**
 * Active defense state for an actor
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FDefenseState
{
	GENERATED_BODY()

	/** Actor being defended */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	TWeakObjectPtr<AActor> Defender;

	/** Actor attacking */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	TWeakObjectPtr<AActor> Attacker;

	/** Is defense window currently open? */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	bool bWindowOpen = false;

	/** Size of incoming attack (affects dodge viability) */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	float AttackSize = 1.0f;

	/** Base damage before defense */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	int32 BaseDamage = 0;

	/** Defense chosen by player (None until input) */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	EDefenseType DefenseChosen = EDefenseType::None;

	/** Dodge direction chosen */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	EDefenseDirection DodgeDirection = EDefenseDirection::None;

	/** Time window opened */
	double WindowOpenTime = 0.0;

	/** Duration of window */
	float WindowDuration = 0.3f;

	/** Has player already submitted defense input? */
	bool bInputReceived = false;

	/**
	 * Stage 3 timestamped input buffer. SubmitDefenseInput appends here; each impact
	 * frame match-and-consumes from it (MatchAndConsumeInput). The single fields above
	 * (DefenseChosen/DodgeDirection/bInputReceived) are now LEGACY — read only by the
	 * non-melee/tail close path; the per-impact melee path reads this buffer.
	 */
	TArray<FTimestampedDefenseInput> InputBuffer;

	/**
	 * True for count-based (melee, bManualClose) windows. Gates the close-time parry
	 * reflect OFF — melee resolves reflect PER IMPACT (ResolveImpactDefense), so the
	 * lumped close must not reflect again on the legacy first-input decision.
	 */
	bool bCountBasedClose = false;
};

/**
 * Defense window configuration (embedded in attack data)
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FDefenseWindowConfig
{
	GENERATED_BODY()

	/** When in animation does window open? (seconds from start) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense")
	float WindowStartTime = 0.5f;

	/** How long is window open? (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense")
	float WindowDuration = 0.3f;

	/** Visual/audio cue timing (seconds before window opens) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense")
	float CueLeadTime = 0.1f;
};

// ========================================
// DELEGATES
// ========================================

/** Broadcast when defense window opens */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDefenseWindowOpened, AActor *, Defender, float, AttackSize, float, WindowDuration);

/** Broadcast when defense window closes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDefenseWindowClosed, AActor *, Defender, const FDefenseResult &, Result);

/** Broadcast when player submits defense input */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDefenseInputReceived, AActor *, Defender, EDefenseType, DefenseType, EDefenseDirection, Direction);

/** Broadcast when visual cue should show (before window opens) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDefenseCueTriggered, AActor *, Defender, float, TimeUntilWindow);

/** Broadcast when parry reflects damage */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnParryReflect, AActor *, Defender, AActor *, Attacker, int32, ReflectedDamage);

/** Broadcast per impact once its defense is resolved (Stage 3). Payoffs are content (later).
 *  AttackEnergyCost is the attack's BASE (pre-efficiency, raw authored) energy cost — the inherent
 *  worth, NOT the post-efficiency paid cost — so a reactive effect can scale off it (e.g. "on parry,
 *  gain EP = X% of the attack's cost"). 0 when the cost can't be resolved. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams(FOnDefenseResolved, AActor *, Defender, AActor *, Attacker, EDefenseType, DefenseType, bool, bPerfect, int32, ImpactIndex, float, AttackEnergyCost);

/** Broadcast when an impact's defense lands inside the perfect-timing window (Stage 3). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnDefensePerfect, AActor *, Defender, AActor *, Attacker, EDefenseType, DefenseType, bool, bPerfect, int32, ImpactIndex);

// ========================================
// DEFENSE SYSTEM
// ========================================

/**
 * UDefenseSystem
 *
 * GameInstanceSubsystem that handles real-time defense mechanics.
 * Players can Block, Parry, or Dodge incoming attacks during defense windows.
 *
 * Defense Types:
 * - Block: 50% damage reduction (always works)
 * - Parry: 70% reduction + 30% reflect (tight timing required)
 * - Dodge: 100% avoidance IF attack size < threshold (left/right only)
 *
 * Usage:
 *   UDefenseSystem* Defense = GetGameInstance()->GetSubsystem<UDefenseSystem>();
 *   Defense->OpenDefenseWindow(Attacker, Defender, AttackSize, BaseDamage, WindowDuration);
 *   // Player presses button
 *   Defense->SubmitDefenseInput(Defender, EDefenseType::Block);
 *   // Window closes, damage applied
 */
UCLASS()
class WORLD_OF_REFRACTION_API UDefenseSystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	// ========================================
	// DEFENSE WINDOW MANAGEMENT
	// ========================================

	/**
	 * Open a defense window for a defender
	 * @param Attacker The actor attacking
	 * @param Defender The actor defending
	 * @param AttackSize Size of attack (affects dodge viability)
	 * @param BaseDamage Damage before defense reduction
	 * @param WindowDuration How long window stays open (auto-close timer duration)
	 * @param bManualClose Melee count-based close (Stage 1): when true the window is
	 *        closed externally by the count-based trigger (last landed hit), so the
	 *        auto-close timer is armed at MaxWindowDuration as a FAILSAFE only — not as
	 *        the closer. WindowDuration is still the AI reaction-delay seed regardless.
	 */
	UFUNCTION(BlueprintCallable, Category = "Defense System")
	void OpenDefenseWindow(
		AActor *Attacker,
		AActor *Defender,
		float AttackSize,
		int32 BaseDamage,
		float WindowDuration = 0.3f,
		bool bManualClose = false);

	/**
	 * Close defense window and calculate result
	 * Called automatically when window expires, or manually
	 */
	UFUNCTION(BlueprintCallable, Category = "Defense System")
	FDefenseResult CloseDefenseWindow(AActor *Defender);

	/**
	 * Submit player's defense input
	 * @param Defender The defending actor
	 * @param DefenseType Block/Parry/Dodge
	 * @param Direction Dodge direction (only for Dodge, left/right only)
	 * @param InputTime Optional explicit press timestamp (FPlatformTime::Seconds clock) for the
	 *        buffered entry. Default 0.0 → stamp now() (player path). A synthesized AI press passes
	 *        an impact-anchored time so MatchAndConsumeInput judges it for that impact.
	 */
	UFUNCTION(BlueprintCallable, Category = "Defense System")
	void SubmitDefenseInput(AActor *Defender, EDefenseType DefenseType, EDefenseDirection Direction = EDefenseDirection::None, double InputTime = 0.0);
	/**
	 * Play defense animation on defender
	 * @param Defender The defending actor
	 * @param DefenseType Block/Parry/Dodge
	 * @param Direction Dodge direction (only for Dodge)
	 */
	UFUNCTION(BlueprintCallable, Category = "Defense System")
	void PlayDefenseAnimation(AActor *Defender, EDefenseType DefenseType, EDefenseDirection Direction = EDefenseDirection::None);

	/**
	 * Check if defense window is currently open for actor
	 */
	UFUNCTION(BlueprintPure, Category = "Defense System")
	bool IsDefenseWindowOpen(AActor *Defender) const;

	/**
	 * Resolve the local player's active defender — the actor with an OPEN window
	 * whose CharacterData is NOT AI-controlled (the player's character currently
	 * under attack). Returns nullptr if none (callers no-op safely).
	 *
	 * SINGLE-TARGET: returns the FIRST matching open-window player defender. This is
	 * the seed for the multi-target model (solo sequential / MP simultaneous) — see
	 * docs/Design/RealTimeDefenseRework.md §13. Multi-target routing is Stage 6.
	 */
	UFUNCTION(BlueprintPure, Category = "Defense System")
	AActor *GetActiveDefenderForLocalPlayer() const;

	/**
	 * Match-and-consume one impact against the input buffer (Stage 3 mutator).
	 *
	 * Finds the LATEST unconsumed buffer entry whose delta = ImpactTime - InputTime
	 * falls in [0, EffectiveWindow], marks it consumed, and returns the match
	 * (bPerfect when delta <= PerfectThreshold). No in-window entry → {bMatched=false}.
	 * EffectiveWindow is the attacker→defender duel (GetEffectiveDefenseInputWindow): the
	 * attacker is read from the live state, AttackType selects the attacker's speed stat.
	 *
	 * Mutates the LIVE ActiveDefenseStates entry (GetDefenseState returns a copy and
	 * cannot consume), so callers at the impact frame get an authoritative per-impact result.
	 *
	 * Window = duel × per-impact DIFFICULTY (cluster 4): each candidate press is tested against
	 * EffectiveWindow × DefenseDifficultyMultiplier(Difficulty.<press's DefenseType>), re-floored at
	 * MINIMUM_DEFENSE_WINDOW. Difficulty carries CONCRETE tiers resolved by the caller (DefenseSystem
	 * stays asset-free); an all-Inherit triple resolves to Easy ×1.0 — unchanged, the empty/guard case.
	 */
	UFUNCTION(BlueprintCallable, Category = "Defense System")
	FDefenseInputMatch MatchAndConsumeInput(AActor *Defender, double ImpactTime, EActionType AttackType,
											const FDefenseDifficultyTriple &Difficulty);

	/**
	 * Apply a parry's reflected damage to the attacker and broadcast OnParryReflect.
	 * Public so the per-impact resolver (ActionExecutor::ResolveImpactDefense) can reflect
	 * each parried impact's own slice (Stage 3) instead of the lumped close.
	 */
	UFUNCTION(BlueprintCallable, Category = "Defense System")
	void ApplyParryReflect(AActor *Attacker, AActor *Defender, int32 ReflectedDamage);

	/**
	 * Get current defense state for actor
	 */
	UFUNCTION(BlueprintPure, Category = "Defense System")
	FDefenseState GetDefenseState(AActor *Defender) const;

	/**
	 * Get remaining time in defense window
	 */
	UFUNCTION(BlueprintPure, Category = "Defense System")
	float GetRemainingWindowTime(AActor *Defender) const;

	// ========================================
	// DEFENSE WINDOW DUEL
	// ========================================

	/**
	 * Effective defense input window for this attacker→defender duel: the tuned base
	 * DefenseInputWindow WIDENED by the defender's Reflex (CalculateReflexWindowBonus) and
	 * NARROWED by the attacker's speed (CalculateSpeedWindowPenalty, type-aware: physical →
	 * ActionSpeed/Body, spell → SpellSpeed/Mind), floored at MINIMUM_DEFENSE_WINDOW.
	 *   window = max(MINIMUM_DEFENSE_WINDOW, base + defenderReflexBonus − attackerSpeedPenalty)
	 * Base on the subsystem, per-character terms through CharacterData.
	 * Null-safe on BOTH sides: a missing attacker / component / CharacterData simply drops that
	 * side's term (no crash). Equal Reflex/ActionSpeed points + equal Body world level cancel to base.
	 */
	UFUNCTION(BlueprintPure, Category = "Defense System")
	float GetEffectiveDefenseInputWindow(AActor *Defender, AActor *Attacker, EActionType AttackType) const;

	// ========================================
	// DAMAGE CALCULATION
	// ========================================

	/**
	 * Calculate final damage after defense. Dodge succeeds on TIMING alone (the attack-size
	 * gate was removed) — a successful Dodge fully avoids regardless of attack size.
	 * @param BaseDamage Original damage
	 * @param DefenseType Defense used
	 * @param bDefenseSuccessful Was timing correct?
	 */
	UFUNCTION(BlueprintPure, Category = "Defense System|Damage")
	FDefenseResult CalculateDefenseResult(
		int32 BaseDamage,
		EDefenseType DefenseType,
		bool bDefenseSuccessful);

	// ========================================
	// EVENTS
	// ========================================

	UPROPERTY(BlueprintAssignable, Category = "Defense System|Events")
	FOnDefenseWindowOpened OnDefenseWindowOpened;

	UPROPERTY(BlueprintAssignable, Category = "Defense System|Events")
	FOnDefenseWindowClosed OnDefenseWindowClosed;

	UPROPERTY(BlueprintAssignable, Category = "Defense System|Events")
	FOnDefenseInputReceived OnDefenseInputReceived;

	UPROPERTY(BlueprintAssignable, Category = "Defense System|Events")
	FOnDefenseCueTriggered OnDefenseCueTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Defense System|Events")
	FOnParryReflect OnParryReflect;

	UPROPERTY(BlueprintAssignable, Category = "Defense System|Events")
	FOnDefenseResolved OnDefenseResolved;

	UPROPERTY(BlueprintAssignable, Category = "Defense System|Events")
	FOnDefensePerfect OnDefensePerfect;

	// ========================================
	// CONFIGURATION
	// ========================================

	/** Block damage reduction (0.5 = 50% reduction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense System|Config")
	float BlockReduction = 0.5f;

	/** Parry damage reduction (0.7 = 70% reduction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense System|Config")
	float ParryReduction = 0.7f;

	/** Parry reflect percentage (0.3 = 30% reflected) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense System|Config")
	float ParryReflect = 0.3f;

	/** Default defense window duration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense System|Config")
	float DefaultWindowDuration = 0.3f;

	/** Stage 3: lead-in window — an input counts for an impact up to this many seconds
	 *  before the impact frame (delta = ImpactTime - InputTime must fall in [0, this]). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense System|Config")
	float DefenseInputWindow = 0.5f;

	/** Stage 3: perfect-timing threshold — a match with delta <= this is PERFECT. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense System|Config")
	float PerfectThreshold = 0.1f;

	/** Defense window duration for AOE attacks (longer than default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense System|Config")
	float AoeWindowDuration = 0.5f;

	/** Max-duration FAILSAFE for manual-close (count-based) windows. Armed instead
	 *  of DefaultWindowDuration when bManualClose is true: longer than any montage so
	 *  the last-hit close pre-empts it on the happy path, shorter than the 10s action
	 *  timeout so the window heals first if a Hit notify is missing/miscounted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense System|Config")
	float MaxWindowDuration = 8.0f;

private:
	// ========================================
	// INTERNAL STATE
	// ========================================

	/** Active defense states per defender */
	TMap<TWeakObjectPtr<AActor>, FDefenseState> ActiveDefenseStates;

	/** Timer handles for auto-closing windows */
	TMap<TWeakObjectPtr<AActor>, FTimerHandle> WindowTimerHandles;

	// ========================================
	// INTERNAL METHODS
	// ========================================

	/** Called when window timer expires */
	void OnWindowTimerExpired(AActor *Defender);

	/** Apply reflected damage from parry */
	void ApplyReflectedDamage(AActor *Attacker, int32 Damage);

	/** Get CharacterDataComponent from actor */
	UCharacterDataComponent *GetCharacterDataComponent(AActor *Actor) const;
};
