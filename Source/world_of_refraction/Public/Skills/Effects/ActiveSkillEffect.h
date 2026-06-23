// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Effects/ESkillEffectTiming.h"
#include "Skills/Effects/ESkillTrigger.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Combat/Damage/EPhysicalDamageType.h"
#include "Skills/Effects/FSkillEffect.h"
#include "Skills/Effects/EffectIdentity.h"
#include "ActiveSkillEffect.generated.h"

/**
 * FActiveSkillEffect
 * Runtime status effect instance applied to actors during combat
 *
 * Supports:
 * - Multiple timing modes (immediate, start/end of turn, conditional, persistent)
 * - Stacking with configurable limits
 * - Source tracking for effect ownership
 * - Conditional triggers via ESkillTrigger
 * - Duration tracking per affected actor's turn (not global)
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FActiveSkillEffect
{
	GENERATED_BODY()

	// ========================================
	// IDENTITY
	// ========================================

	/** Display name for UI and logs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FString EffectName = TEXT("Unnamed Effect");

	/** Unique identifier for stacking/removal purposes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	int32 EffectID = 0;

	/** Optional description for UI tooltips */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FString Description = TEXT("");

	// ========================================
	// TIMING
	// ========================================

	/** When this effect processes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	ESkillEffectTiming ProcessTiming = ESkillEffectTiming::StartOfOwnTurn;

	/** If ProcessTiming = OnTrigger, what condition activates it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
			  meta = (EditCondition = "ProcessTiming == ESkillEffectTiming::OnTrigger"))
	ESkillTrigger TriggerCondition = ESkillTrigger::None;

	/** Threshold for HP/Energy percentage triggers (source side). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
			  meta = (ClampMin = "0", ClampMax = "100",
					  EditCondition = "ProcessTiming == ESkillEffectTiming::OnTrigger"))
	float TriggerThreshold = 30.0f;

	/** Target-side condition mirrored from FSkillEffect::TargetCondition.
	 *  Effects with a target condition only apply when the target's state
	 *  satisfies it (HP/Energy threshold checks evaluated against the target). */
	UPROPERTY(BlueprintReadWrite, Category = "Timing")
	ESkillTrigger TargetTriggerCondition = ESkillTrigger::None;

	/** Threshold for the target-side trigger (0..100). */
	UPROPERTY(BlueprintReadWrite, Category = "Timing",
			  meta = (ClampMin = "0", ClampMax = "100"))
	float TargetTriggerThreshold = 100.0f;

	/** Secondary source-side trigger condition, mirrored from FSkillEffect::SecondaryCondition.
	 *  Combined with TriggerCondition via bRequireBothTriggers (AND / OR). */
	UPROPERTY(BlueprintReadWrite, Category = "Timing")
	ESkillTrigger SecondaryTriggerCondition = ESkillTrigger::None;

	/** Threshold for the secondary source-side trigger (0..100). */
	UPROPERTY(BlueprintReadWrite, Category = "Timing",
			  meta = (ClampMin = "0", ClampMax = "100"))
	float SecondaryTriggerThreshold = 30.0f;

	/** True = both primary and secondary triggers must be met (AND). False = either suffices (OR). */
	UPROPERTY(BlueprintReadWrite, Category = "Timing")
	bool bRequireBothTriggers = true;

	/** Whether this trigger condition is currently active (for conditional effects) */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	bool bTriggerActive = false;

	/** N-condition trigger group (Cluster C), carried from FSkillEffect::Conditions at
	 *  conversion and shared across every payload of the source effect. When non-empty
	 *  this is the source of truth for trigger gating (IsTriggerConditionMet); when empty
	 *  the legacy TriggerCondition/SecondaryTriggerCondition/bRequireBothTriggers fields
	 *  are used, so the synthetic factories (CreateFromInfusion / CreateFromWeaponInfusion /
	 *  CreateFromPhysicalDamageType) — which never populate this — stay byte-identical. */
	UPROPERTY(BlueprintReadWrite, Category = "Timing")
	TArray<FSkillCondition> Conditions;

	// ========================================
	// DURATION
	// ========================================

	/** Turns remaining (ticks on affected actor's turn, not global) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duration")
	int32 RemainingTurns = 3;

	/** Initial duration (for UI display of "X/Y turns remaining") */
	UPROPERTY(BlueprintReadOnly, Category = "Duration")
	int32 InitialDuration = 3;

	/** Effect never expires (equipment bonuses, auras, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duration")
	bool bPermanent = false;

	// ========================================
	// CHARGES
	// ========================================

	/** Charge count — a SEPARATE governor from RemainingTurns (Option b: not overloading duration).
	 *  0 = unlimited (no charge system; existing effects are byte-identical — every consume site
	 *  no-ops at 0). >0 = the effect fires/triggers up to Charges times, decrementing per fire, and
	 *  is removed at 0 via ConsumeCharge — its OWN removal path, independent of TickDurations'
	 *  turn-expiry. ⚠️ Footgun: a charged effect MUST be bPermanent OR RemainingTurns > 0. A
	 *  duration-0 non-permanent effect is IsInstant() → takes the instant lane → never stored → its
	 *  charges never fire. (A permanent charged effect — e.g. phoenix — is NEVER reaped by
	 *  TickDurations, which skips bPermanent; ConsumeCharge is its only removal path.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Charges")
	int32 Charges = 0;

	// ========================================
	// EFFECT DATA
	// ========================================

	/** Type of effect (buff, debuff, DOT, etc.) - uses unified enum */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	ESkillEffectType EffectType = ESkillEffectType::None;

	/** Effect magnitude (damage amount, buff percentage, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	float EffectValue = 0.0f;

	/** Element for elemental DOTs and resistance calculations; None = non-elemental */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	ESpellElement Element = ESpellElement::None;

	/** Physical-damage type this effect keys on (for physical-type status
	 *  resistance). None = effect keys on Element instead. A resistance effect
	 *  targets EITHER an element OR a physical type, never both. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	EPhysicalDamageType PhysicalType = EPhysicalDamageType::None;

	/** Status-bar buildup contributed per damage event. Set by the effect's
	 *  source (e.g. Garnet) so each DOT tick mirrors the buildup of the
	 *  immediate hit. Left 0 for DOTs that don't build status (weapon Bleed,
	 *  ability infusions) — a 0 value contributes no buildup on tick. */
	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	float BuildupPerTick = 0.0f;

	// ========================================
	// STACKING
	// ========================================

	/** Can multiple instances of this effect stack on same target? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stacking")
	bool bCanStack = false;

	/** Current stack count */
	UPROPERTY(BlueprintReadOnly, Category = "Stacking")
	int32 CurrentStacks = 1;

	/** Maximum stacks allowed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stacking",
			  meta = (EditCondition = "bCanStack", ClampMin = "1", ClampMax = "99"))
	int32 MaxStacks = 3;

	/** If true, reapplying refreshes duration instead of adding stacks */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stacking")
	bool bRefreshDurationOnReapply = true;

	/** Applies at most once per combat (keyed on EffectID). Carried from
	 *  FSkillEffect::bFiresOncePerMatch; checked by ApplyEffect against the manager's
	 *  per-match fired-set. Default false = unrestricted. */
	UPROPERTY(BlueprintReadWrite, Category = "Stacking")
	bool bFiresOncePerMatch = false;

	// ========================================
	// SOURCE TRACKING
	// ========================================

	/** Actor who applied this effect (for damage attribution, cleanse targeting) */
	UPROPERTY(BlueprintReadOnly, Category = "Source")
	TWeakObjectPtr<AActor> SourceActor;

	/** Name of spell/ability that applied this effect */
	UPROPERTY(BlueprintReadOnly, Category = "Source")
	FString SourceAbilityName = TEXT("");

	/** Team index of source (for friendly fire checks, cleanse logic) */
	UPROPERTY(BlueprintReadOnly, Category = "Source")
	int32 SourceTeamIndex = -1;

	// ========================================
	// RUNTIME FLAGS
	// ========================================

	/** Has this effect been processed this turn? (prevents double-processing) */
	UPROPERTY(BlueprintReadOnly, Category = "Runtime")
	bool bProcessedThisTurn = false;

	/** Is this effect marked for removal at end of processing? */
	UPROPERTY(BlueprintReadOnly, Category = "Runtime")
	bool bPendingRemoval = false;

	/** Opt out of fire-on-application. Default false = the effect executes its logic on apply
	 *  (turn 0) and then ticks on its ProcessTiming windows. True = skip the apply execution and
	 *  only tick on windows (the pre-fire-on-apply behaviour, now opt-in). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	bool bDelayFirstExecution = false;

	// ========================================
	// CONSTRUCTORS
	// ========================================

	FActiveSkillEffect()
	{
		InitialDuration = RemainingTurns;
	}

	// ========================================
	// FACTORY METHODS - FROM SPELL/ABILITY DATA
	// ========================================

	// CreateFromSpellEffect was DELETED (effect-build-unification Cluster 2). Its dur-0 clamp
	// and silently-dropped bPermanent/Charges/bDelayFirstExecution drift are gone — the spell
	// cast path (ActionExecutor) and the defender-trigger path (SkillEffectManager) now build
	// through the shared BuildRuntimeFromPayload core (structural fields) + a post-build
	// EffectValue override (cast EffMagMult / defender pre-rounding). See that core below.

	/**
	 * Single-payload runtime-effect builder — the SHARED CORE the gear/skill/evolution factory
	 * (CreateAllFromSkillEffect), the spell cast path (ActionExecutor::ApplySkillEffects), and the
	 * defender-trigger path (SkillEffectManager) all route through. Lifted verbatim from
	 * CreateAllFromSkillEffect's inner Build lambda so gear output stays byte-identical; it is the
	 * single choke point where an authored payload becomes a runtime effect — which is what killed
	 * the old spell-path drift (the now-deleted CreateFromSpellEffect silently dropped
	 * bPermanent/Charges/bDelayFirstExecution and clamped dur-0 → 1).
	 *
	 * @param Name          Resolved display name (caller applies any "Source-or-fallback" choice).
	 * @param PackedEffectID Caller-packed EffectID (EffectIdentity::PackEffectID(...)).
	 * @param P             Authored payload — source of type/value/duration/charges/permanent/delay/timing.
	 * @param Conditions    Shared condition group (drives OnTrigger promotion); pass {} to omit.
	 * @param bStackable / MaxStacks / bFiresOnce  Per-effect stacking, shared across payloads.
	 * @param Element       Effect element (spell threads the resolved element; gear/defender = None).
	 *                      Default None matches FActiveSkillEffect::Element's struct default, which the
	 *                      reference Build lambda left untouched — so gear stays byte-identical.
	 * @param bAutoDetectTimingByType  OPT-IN type-based ProcessTiming auto-detect (DOT→EndOfOwnTurn,
	 *                      Health/EnergyRestore→StartOfOwnTurn, EnergyDrain→EndOfOwnTurn). DEFAULT OFF
	 *                      so gear keeps authored P.ProcessTiming (byte-identical). The spell paths
	 *                      pass true to preserve the type-based auto-detect the old CreateFromSpellEffect had.
	 */
	static FActiveSkillEffect BuildRuntimeFromPayload(
		const FString &Name,
		int32 PackedEffectID,
		const FSkillEffectPayload &P,
		const TArray<FSkillCondition> &Conditions,
		bool bStackable,
		int32 MaxStacks,
		bool bFiresOnce,
		ESpellElement Element = ESpellElement::None,
		bool bAutoDetectTimingByType = false)
	{
		FActiveSkillEffect Effect;
		Effect.EffectName = Name;
		Effect.EffectID = PackedEffectID;
		Effect.EffectType = P.EffectType;
		Effect.EffectValue = (P.Value != 0) ? static_cast<float>(P.Value) : (P.Magnitude * 100.0f);
		// Permanent is an EXPLICIT authored toggle now — NOT derived from duration-0.
		//   bPermanent    -> never expires (Persistent timing; RemainingTurns irrelevant)
		//   Duration == 0 -> INSTANT (RemainingTurns 0, NOT clamped; IsInstant() catches it)
		//   Duration  > 0 -> lingering (RemainingTurns = Duration)
		Effect.bPermanent = P.bPermanent;
		Effect.bDelayFirstExecution = P.bDelayFirstExecution;
		Effect.RemainingTurns = P.Duration;
		Effect.InitialDuration = Effect.RemainingTurns;
		// Charges: SEPARATE governor from duration. 0 (default) = no charge system, untouched.
		Effect.Charges = P.Charges;
		Effect.Element = Element;

		// ⚠️ Footgun guard: a charged effect that is duration-0 AND not permanent is IsInstant()
		// → ApplyEffect runs it down the instant lane and NEVER stores it, so its charges can
		// never fire. Warn at this single conversion choke point. Fix: set bPermanent OR Duration > 0.
		if (P.Charges > 0 && P.Duration == 0 && !P.bPermanent)
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("[FActiveSkillEffect] '%s' has Charges=%d but Duration=0 and not permanent — "
						"charges will NEVER fire (instant lane, never stored). Set bPermanent or Duration > 0."),
				   *Effect.EffectName, P.Charges);
		}

		// Authored timing is the base. The OPT-IN type auto-detect (spell parity) runs BEFORE the
		// OnTrigger promotion so a triggered effect still promotes correctly. OFF for gear → the
		// block is skipped and ProcessTiming == authored P.ProcessTiming (byte-identical to Build).
		Effect.ProcessTiming = P.ProcessTiming;
		if (bAutoDetectTimingByType)
		{
			if (Effect.IsDOT())
			{
				Effect.ProcessTiming = ESkillEffectTiming::EndOfOwnTurn;
			}
			else if (Effect.EffectType == ESkillEffectType::HealthRestore ||
					 Effect.EffectType == ESkillEffectType::EnergyRestore)
			{
				Effect.ProcessTiming = ESkillEffectTiming::StartOfOwnTurn;
			}
			else if (Effect.EffectType == ESkillEffectType::EnergyDrain)
			{
				Effect.ProcessTiming = ESkillEffectTiming::EndOfOwnTurn;
			}
		}

		// Shared N-condition group: every payload gates on the same conditions.
		Effect.Conditions = Conditions;

		// Authored stacking / fires-once (D2). Per-effect, shared across payloads.
		Effect.bCanStack = bStackable;
		Effect.MaxStacks = MaxStacks;
		Effect.bFiresOncePerMatch = bFiresOnce;

		// Promote to OnTrigger when any owner-side condition is non-trivial so the manager's
		// trigger evaluator fires. Reads the new Conditions[] (primary in Conditions[0]).
		for (const FSkillCondition &C : Conditions)
		{
			if (IsOwnerSide(C.Subject) && C.Trigger != ESkillTrigger::Always && C.Trigger != ESkillTrigger::None)
			{
				Effect.ProcessTiming = ESkillEffectTiming::OnTrigger;
				break;
			}
		}

		return Effect;
	}

	/**
	 * Create a runtime status-effect instance from an authored FSkillEffect.
	 * Used for evolution-crystal effects AND equipment (weapon/ring) effects —
	 * both now share the FSkillEffect type after the Phase 3 merge.
	 *
	 * EffectID is packed as SourceID*100 + EffectIndex so removal scans a
	 * contiguous window. Source callers must pass distinct SourceIDs to avoid
	 * collisions (e.g. evolution-ID vs weapon/ring-instance-ID ranges).
	 *
	 * @param SourceName    Display prefix when Source.EffectName is empty
	 * @param SourceID      Source's stable identifier (evolution/weapon/ring ID)
	 * @param Source        Authored skill-effect data to instantiate from
	 * @param EffectIndex   Index within the source's Effects array (for ID packing)
	 */
	static TArray<FActiveSkillEffect> CreateAllFromSkillEffect(
		const FString &SourceName,
		int32 SourceID,
		const FSkillEffect &Source,
		int32 EffectIndex)
	{
		TArray<FActiveSkillEffect> Out;

		// Thin wrapper: resolve the shared display name + per-payload packed ID, then route every
		// non-None payload through the shared BuildRuntimeFromPayload core. Gear/skill/evolution
		// keep authored timing (Element None, bAutoDetectTimingByType=false) — byte-identical to the
		// former inline Build lambda this replaced. The condition group + stacking are shared across
		// payloads (the trigger gates the whole bundle).
		const FString ResolvedName = Source.EffectName.IsEmpty() ? (SourceName + TEXT(" Effect")) : Source.EffectName;

		if (Source.Payloads.Num() > 0)
		{
			for (int32 p = 0; p < Source.Payloads.Num(); ++p)
			{
				const FSkillEffectPayload &P = Source.Payloads[p];
				if (P.EffectType == ESkillEffectType::None)
				{
					continue;
				}
				// SubIndex p keeps multi-payload IDs distinct; SubIndex 0 reproduces the legacy ID exactly.
				Out.Add(BuildRuntimeFromPayload(
					ResolvedName,
					EffectIdentity::PackEffectID(SourceID, EffectIndex, p),
					P,
					Source.Conditions,
					Source.bStackable,
					Source.MaxStacks,
					Source.bFiresOncePerMatch));
			}
		}

		return Out;
	}

	/**
	 * TODO(Cluster D): deprecate — single-payload convenience wrapper kept so callers
	 * not yet repointed to CreateAllFromSkillEffect still compile. Returns the FIRST
	 * produced runtime effect (or a default when the source yields none). Callers that
	 * may carry multi-payload effects MUST use CreateAllFromSkillEffect instead.
	 */
	static FActiveSkillEffect CreateFromSkillEffect(
		const FString &SourceName,
		int32 SourceID,
		const FSkillEffect &Source,
		int32 EffectIndex)
	{
		TArray<FActiveSkillEffect> All = CreateAllFromSkillEffect(SourceName, SourceID, Source, EffectIndex);
		return All.Num() > 0 ? All[0] : FActiveSkillEffect();
	}

	/**
	 * Create DOT effect from ability infusion
	 * When ability is infused, it gains elemental DOT
	 */
	static FActiveSkillEffect CreateFromInfusion(
		const FString &AbilityName,
		int32 AbilityID,
		ESpellElement InfusedElement,
		float DOTDamage,
		int32 Duration)
	{
		FActiveSkillEffect Effect = CreateDOT(
			AbilityName + TEXT(" Infusion"),
			AbilityID * 10 + 5, // +5 offset for infusion effects
			DOTDamage,
			Duration,
			InfusedElement);
		return Effect;
	}

	// ========================================
	// FACTORY METHODS - WEAPON SYSTEM
	// ========================================

	/**
	 * Create status effect from physical damage type (Generic character weapon attacks)
	 * Slash → Bleed DOT, Pierce → Armor Break, Impact → Stun
	 *
	 * @param WeaponName Name of weapon
	 * @param WeaponID Unique weapon identifier
	 * @param PhysicalType The physical damage type (0=Slash, 1=Pierce, 2=Impact)
	 * @param StatusBuildup Base buildup value from WeaponAttackData
	 * @param InfusionMultiplier Weapon's InfusionStatusMultiplier (1.0 = normal)
	 * @param HitCount Number of hits (multiplies buildup)
	 */
	static FActiveSkillEffect CreateFromPhysicalDamageType(
		const FString &WeaponName,
		int32 WeaponID,
		uint8 PhysicalType,
		int32 StatusBuildup,
		float InfusionMultiplier,
		int32 HitCount,
		int32 DurationOverride = 0)
	{
		// Canonical per-status durations — PASSIVE-proc territory only (the
		// weapon-attack caller, ApplyPhysicalDamageEffect, has no authored spec
		// and passes no override). The AUTHORED ability-DoT path always passes a
		// >0 override (max(1, authored Duration)), so these defaults can never
		// mask an authoring mistake there.
		constexpr int32 BLEED_DEFAULT_DURATION = 3;
		constexpr int32 ARMOR_BREAK_DEFAULT_DURATION = 2;
		constexpr int32 STUN_DEFAULT_DURATION = 1;

		FActiveSkillEffect Effect;
		Effect.EffectID = WeaponID * 10 + 7; // +7 offset for physical status

		// Calculate actual buildup: base × multiplier × hits
		float TotalBuildup = static_cast<float>(StatusBuildup) * InfusionMultiplier * static_cast<float>(HitCount);

		// Physical damage types: 0=Slash, 1=Pierce, 2=Impact
		switch (PhysicalType)
		{
		case 0: // Slash → Bleed DOT
			Effect.EffectName = WeaponName + TEXT(" Bleed");
			Effect.EffectType = ESkillEffectType::DOT;
			Effect.Element = ESpellElement::None;  // Physical damage — non-elemental
			Effect.EffectValue = TotalBuildup * 0.5f; // DOT damage = half of buildup
			Effect.RemainingTurns = BLEED_DEFAULT_DURATION;
			Effect.ProcessTiming = ESkillEffectTiming::EndOfOwnTurn;
			Effect.bCanStack = true;
			break;

		case 1: // Pierce → Armor Break (defense debuff)
			Effect.EffectName = WeaponName + TEXT(" Armor Break");
			Effect.EffectType = ESkillEffectType::DefenseDebuff;
			Effect.EffectValue = TotalBuildup * 0.3f; // Defense reduction
			Effect.RemainingTurns = ARMOR_BREAK_DEFAULT_DURATION;
			Effect.ProcessTiming = ESkillEffectTiming::Persistent;
			Effect.bCanStack = true;
			Effect.MaxStacks = 3;
			break;

		case 2: // Impact → Stun (skip turn - uses special handling)
			Effect.EffectName = WeaponName + TEXT(" Stun");
			Effect.EffectType = ESkillEffectType::Stun;
			Effect.EffectValue = 1.0f;					// Stun duration multiplier
			Effect.RemainingTurns = STUN_DEFAULT_DURATION;
			Effect.ProcessTiming = ESkillEffectTiming::StartOfOwnTurn;
			Effect.bCanStack = false; // Stun doesn't stack, refreshes
			Effect.bRefreshDurationOnReapply = true;
			break;

		default:
			Effect.EffectName = TEXT("Unknown Physical Effect");
			Effect.EffectType = ESkillEffectType::None;
			break;
		}

		// Authored duration wins over the canonical default; only the duration —
		// the per-status value shaping above is untouched.
		if (DurationOverride > 0 && Effect.EffectType != ESkillEffectType::None)
		{
			Effect.RemainingTurns = DurationOverride;
		}

		Effect.InitialDuration = Effect.RemainingTurns;
		return Effect;
	}

	/**
	 * Create weapon infusion DOT with weapon's multiplier applied
	 * For Generic characters using infused abilities through weapons
	 */
	static FActiveSkillEffect CreateFromWeaponInfusion(
		const FString &AbilityName,
		const FString &WeaponName,
		int32 AbilityID,
		ESpellElement InfusedElement,
		float BaseDOTDamage,
		int32 Duration,
		float WeaponInfusionMultiplier)
	{
		float AdjustedDamage = BaseDOTDamage * WeaponInfusionMultiplier;
		FActiveSkillEffect Effect = CreateDOT(
			AbilityName + TEXT(" (") + WeaponName + TEXT(" Infusion)"),
			AbilityID * 10 + 8, // +8 offset for weapon-infused abilities
			AdjustedDamage,
			Duration,
			InfusedElement);
		return Effect;
	}

	/** Quick constructor for common buff/debuff creation */
	static FActiveSkillEffect CreateBuff(const FString &Name, int32 ID, ESkillEffectType Type, float Value, int32 Duration)
	{
		FActiveSkillEffect Effect;
		Effect.EffectName = Name;
		Effect.EffectID = ID;
		Effect.EffectType = Type;
		Effect.EffectValue = Value;
		Effect.RemainingTurns = Duration;
		Effect.InitialDuration = Duration;
		Effect.ProcessTiming = ESkillEffectTiming::StartOfOwnTurn;
		return Effect;
	}

	/** Quick constructor for DOT effects */
	static FActiveSkillEffect CreateDOT(const FString &Name, int32 ID, float DamagePerTurn, int32 Duration, ESpellElement InElement)
	{
		FActiveSkillEffect Effect;
		Effect.EffectName = Name;
		Effect.EffectID = ID;
		Effect.EffectType = ESkillEffectType::DOT; // Generic DOT type
		Effect.EffectValue = DamagePerTurn;
		Effect.RemainingTurns = Duration;
		Effect.InitialDuration = Duration;
		Effect.Element = InElement;
		Effect.ProcessTiming = ESkillEffectTiming::EndOfOwnTurn;

		// No element-specific switch needed - display name handled by SkillEffectDisplayNames

		return Effect;
	}

	/** Quick constructor for persistent stat modifiers */
	static FActiveSkillEffect CreatePersistent(const FString &Name, int32 ID, ESkillEffectType Type, float Value)
	{
		FActiveSkillEffect Effect;
		Effect.EffectName = Name;
		Effect.EffectID = ID;
		Effect.EffectType = Type;
		Effect.EffectValue = Value;
		Effect.bPermanent = true;
		Effect.ProcessTiming = ESkillEffectTiming::Persistent;
		return Effect;
	}

	// ========================================
	// HELPER FUNCTIONS
	// ========================================

	/** Check if this is a buff (positive effect)? Delegates to the shared classifier. */
	bool IsBuff() const
	{
		return SkillEffectClassification::IsBuff(EffectType, EffectValue);
	}

	/** Check if this is a debuff (negative effect)? Delegates to the shared classifier. */
	bool IsDebuff() const
	{
		return SkillEffectClassification::IsDebuff(EffectType, EffectValue);
	}

	/** Check if this is DOT damage */
	bool IsDOT() const
	{
		return EffectType == ESkillEffectType::DOT;
	}

	/** Check if this effect deals damage */
	bool DealsDamage() const
	{
		return IsDOT() ||
			   EffectType == ESkillEffectType::RetaliationDamage ||
			   EffectType == ESkillEffectType::SelfDamage ||
			   EffectType == ESkillEffectType::BurstDamage;
	}

	/** Get effective value considering stacks */
	float GetStackedValue() const
	{
		return EffectValue * static_cast<float>(CurrentStacks);
	}

	/** Single-application strength for the re-apply policy: magnitude, sign-agnostic.
	 *  Abs() so the one signed type (ModifyStatusResist, where sign = direction) compares
	 *  by how strong, not which way. NOT stack-scaled — the policy gates on per-application
	 *  strength, not the stacked total. */
	float Strength() const
	{
		return FMath::Abs(EffectValue);
	}

	/** True instant: applies via its Server* primitive through ApplyEffect's gates, then
	 *  returns WITHOUT being stored. Duration-0 is the authoritative, SOLE signal — a
	 *  zero-duration, non-permanent effect is instant. Permanent is now an explicit toggle
	 *  (bPermanent), fully decoupled from duration-0, so the old Persistent guard is gone.
	 *  (ESkillEffectTiming::Instant is now vestigial — duration-0 supersedes it; flagged for
	 *  later removal, not deleted mid-arc.) */
	bool IsInstant() const
	{
		return RemainingTurns == 0 && !bPermanent;
	}

	/** Check if can add another stack */
	bool CanAddStack() const
	{
		return bCanStack && CurrentStacks < MaxStacks;
	}

	/** Get duration display string (e.g., "2/3 turns") */
	FString GetDurationString() const
	{
		if (bPermanent)
		{
			return TEXT("Permanent");
		}
		return FString::Printf(TEXT("%d/%d turns"), RemainingTurns, InitialDuration);
	}

	/** Get stack display string (e.g., "x3") */
	FString GetStackString() const
	{
		if (!bCanStack || CurrentStacks <= 1)
		{
			return TEXT("");
		}
		return FString::Printf(TEXT("x%d"), CurrentStacks);
	}

	/** Equality check by EffectID (for finding existing effects) */
	bool operator==(const FActiveSkillEffect &Other) const
	{
		return EffectID == Other.EffectID;
	}

	/** Reset turn-based processing flags */
	void ResetTurnFlags()
	{
		bProcessedThisTurn = false;
	}
};

/** Hash function for TMap usage */
FORCEINLINE uint32 GetTypeHash(const FActiveSkillEffect &Effect)
{
	return GetTypeHash(Effect.EffectID);
}