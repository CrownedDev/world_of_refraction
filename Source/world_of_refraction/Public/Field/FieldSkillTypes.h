// FieldSkillTypes.h
// B-214a (D-099, Design/FieldAbilities.md): field movement on the map. One active field skill per character: a source
// (innate / weapon / ring) and a movement type. The source's element styles it and sets its strength; the normal dash
// is everyone's and needs no source. FieldSkillRules holds the pure pick / start / speed rules the component applies. B-214b adds the unlock rules
// (an aura spell of the source's element at rank 3 or 7 unlocks a type; UFieldSkillUnlockSubsystem keeps the account's ranks).

#pragma once

#include "CoreMinimal.h"
#include "Combat/Damage/EPhysicalDamageType.h"
#include "Inventory/ItemTier.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Templates/Function.h"
#include "FieldSkillTypes.generated.h"

/** The 11 movement types (FieldAbilities "Movement types", Crown 2026-10-27). B-214a builds Dash and Enhanced Sprint,
 *  B-214c Glide, High jump and Air dash, B-214d Flight, B-214e the rest (all built). Append only: saved as a byte (FSavedPartyMember). */
UENUM(BlueprintType)
enum class EFieldMovementType : uint8
{
	Dash,
	EnhancedSprint,
	Glide,
	Flight,
	Teleport,
	HighJump,
	AirDash,
	SuperLaunch,
	PhaseStep,
	MarkAndRecall,
	Regroup
};

/** Where a field skill draws its element from. Append only: saved as a byte (FSavedPartyMember). */
UENUM(BlueprintType)
enum class EFieldSkillSource : uint8
{
	Innate, // a Caster's innate element (UCharacterData::GetElement; Generic for other classes)
	Weapon, // the crystal in the weapon in hand (none when broken)
	Ring    // a Resonator's active adapter, another class's primary ring
};

/** Why a pick or a start was refused. None = allowed. */
UENUM(BlueprintType)
enum class EFieldSkillRefusal : uint8
{
	None,
	NotBuiltYet,          // its slice (B-214c..e) hasn't landed
	NeedsElementalSource, // the source has no element (Generic, None, empty or broken crystal)
	NotEnoughEP,
	OnCooldown,
	NoMovement,           // the pawn isn't a character (no movement to dash with)
	NotUnlocked,          // B-214b: no aura spell of the source's element has reached the rank yet
	NeedsAir,             // B-214c: glide and air dash start only in the air
	AlreadyUsedInAir,     // B-214c: the double jump or air dash was used since the last landing
	NotActiveSkill,       // B-214c: a move other than the character's active field skill was called
	NoFlyZone,            // B-214d: inside a Zone.NoFly volume
	InFight,              // B-214e: Regroup isn't used in or into a fight
	NoTarget,             // B-214e: Regroup with no other party member on the map
	Blocked               // B-214e: no room at the destination
};

/** B-214b (FieldAbilities "Movement -> aura"): the three elemental enhancements whose spells unlock movement types. A
 *  spell is marked with its aura on the asset (USpellData::FieldAura, Crown's table); its element is the variant.
 *  Append only: saved as a byte (FSavedFieldUnlocks). */
UENUM(BlueprintType)
enum class EFieldAura : uint8
{
	None,
	Elemental,
	Enhancement,
	Manipulation
};

/** B-214d (FieldAbilities choice 4, Crown 2026-10-27): what lifts flight's limits. Set by effects (an evolution's
 *  unlimited flight, B-214h; one character's unlimited energy later) through UFieldMovementComponent::SetFlightLimits. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FFieldFlightLimits
{
	GENERATED_BODY()

	/** Flight costs no EP. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field")
	bool bUnlimitedEP = false;

	/** No height ceiling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field")
	bool bNoCeiling = false;

	/** Zone.NoFly volumes don't stop it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field")
	bool bIgnoreNoFly = false;

	/** Raises the ceiling (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field", meta = (ClampMin = 0))
	float ExtraCeiling = 0.0f;
};

/** B-214f (Metaphor: ReFantazio's field strike): what a field hit carries into the fight it starts: the party strikes
 *  first, and every enemy of the struck team takes this status buildup at the start. Carried across the level change by
 *  UTrialRunSubsystem (as the first strike is). */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FFieldStrike
{
	GENERATED_BODY()

	/** The attack source's element; None for a source with no element (the hit is physical only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field")
	ESpellElement Element = ESpellElement::None;

	/** The weapon in hand's attack type (Slash / Pierce / Impact); None with no weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field")
	EPhysicalDamageType Physical = EPhysicalDamageType::None;

	/** Held with a Water source: the hit deals Ice (D-029's Water-deals-Ice rule). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field")
	bool bIce = false;

	/** Buildup each enemy takes at the start; 0 = no field strike. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field")
	float Buildup = 0.0f;

	bool IsSet() const { return Buildup > 0.0f; }
};

/** A character's active field skill. Its home is FPartyMember::FieldSkill (saved, v36). */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FFieldSkillChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field")
	EFieldSkillSource Source = EFieldSkillSource::Innate;

	/** Dash by default: the one movement type everyone has (FieldAbilities "all locked except the normal dash"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field")
	EFieldMovementType Type = EFieldMovementType::Dash;

	bool operator==(const FFieldSkillChoice &Other) const { return Source == Other.Source && Type == Other.Type; }
	bool operator!=(const FFieldSkillChoice &Other) const { return !(*this == Other); }
};

namespace FieldSkillRules
{
	constexpr float DASH_COOLDOWN_SECONDS = 1.0f;  // TODO: tune
	constexpr float DASH_LAUNCH_SPEED = 1800.0f;   // cm/s along the ground. TODO: tune

	constexpr int32 ENHANCED_SPRINT_EP_PER_SECOND = 1;     // TODO: tune
	constexpr float ENHANCED_SPRINT_DRAIN_INTERVAL = 1.0f; // seconds per EP charge

	/** Q-214a (Crown, 2026-11-12): a second is charged only while the pawn moves at sprint speed. Faster than the pawn
	 *  Blueprint's walk (500 cm/s), slower than the slowest sprint (UHubMovementLibrary::GetSprintSpeedMin, 800). */
	constexpr float ENHANCED_SPRINT_MIN_MOVING_SPEED = 600.0f; // cm/s along the ground. TODO: tune

	/** Enhanced Sprint speed over the normal sprint, by the source's element (Crown: Lightning fastest, Wind second). */
	constexpr float ENHANCED_SPRINT_LIGHTNING_MULTIPLIER = 1.5f; // TODO: tune
	constexpr float ENHANCED_SPRINT_WIND_MULTIPLIER = 1.4f;      // TODO: tune
	constexpr float ENHANCED_SPRINT_OTHER_MULTIPLIER = 1.25f;    // TODO: tune
	constexpr float NO_SPRINT_BOOST = 1.0f;

	// ---- B-214c: ground and air moves (FieldAbilities; all numbers TODO: tune, Q-214c) ----

	/** Glide: a slow fall while on (double tap in the air), off on a second tap or on landing; paid once at the start. */
	constexpr int32 GLIDE_EP_COST = 1;
	constexpr float GLIDE_GRAVITY_SCALE = 0.15f;
	constexpr float GLIDE_MAX_FALL_SPEED = 250.0f; // cm/s
	constexpr float GLIDE_AIR_CONTROL = 0.8f;

	/** High jump from the ground; once in the air, one double jump until landing. Each paid. */
	constexpr int32 HIGH_JUMP_EP_COST = 1;
	constexpr float HIGH_JUMP_SPEED = 1200.0f;   // cm/s up
	constexpr float DOUBLE_JUMP_SPEED = 900.0f;  // cm/s up

	/** Air dash: once per airtime, along the aim (the camera, else facing); Lightning's is a chain of blinks. */
	constexpr int32 AIR_DASH_EP_COST = 2;
	constexpr float AIR_DASH_SPEED = 2000.0f;          // cm/s
	constexpr int32 AIR_DASH_BLINK_COUNT = 3;
	constexpr float AIR_DASH_BLINK_DISTANCE = 250.0f;  // cm per blink

	/** Wall-run: during Enhanced Sprint, off the ground at sprint speed beside a wall; holds height along the wall for
	 *  a while, once per airtime (Prototype / Warframe parkour). Free: Enhanced Sprint's drain pays for it. */
	constexpr float WALL_RUN_TRACE_DISTANCE = 80.0f; // cm to each side
	constexpr float WALL_RUN_GRAVITY_SCALE = 0.0f;
	constexpr float WALL_RUN_MAX_SECONDS = 1.5f;

	/** How often a running glide or Enhanced Sprint steps (glide's fall cap, the wall-run check). */
	constexpr float MOVE_STEP_INTERVAL = 0.05f;

	// ---- B-214d: flight (FieldAbilities choice 4; numbers TODO: tune, Q-214d) ----

	constexpr int32 FLIGHT_EP_PER_SECOND = 2;
	constexpr float FLIGHT_DRAIN_INTERVAL = 1.0f;
	constexpr float FLIGHT_MAX_HEIGHT = 1500.0f;          // cm above the ground at take-off
	constexpr float FLIGHT_SPEED = 900.0f;                // cm/s (MaxFlySpeed)
	constexpr float FLIGHT_HOVER_BRAKING = 4000.0f;       // cm/s^2: stops quickly with no input, so it hovers
	constexpr float FLIGHT_GROUND_TRACE_DISTANCE = 10000.0f;
	constexpr float FLIGHT_TAKEOFF_LIFT_SPEED = 600.0f;   // cm/s up on take-off, so it leaves the ground
	constexpr float FLIGHT_VERTICAL_NUDGE_SPEED = 500.0f; // cm/s for wor.Field.FlyVertical (until B-214g's keys)
	constexpr float FLIGHT_CEILING_SNAP_MARGIN = 100.0f;  // cm over the ceiling before it is put back (else its rise stops)
	/** The actor tag that makes a volume a no-fly zone (boss arenas, some secrets). */
	inline constexpr const TCHAR *NO_FLY_ZONE_TAG = TEXT("Zone.NoFly"); // TCHAR, not FName: no FName at static init

	/** Flight may start: outside a no-fly zone (unless lifted) with one second's EP (unless unlimited). */
	WORLD_OF_REFRACTION_API EFieldSkillRefusal CheckFlightStart(bool bInNoFlyZone, int32 CurrentEP, const FFieldFlightLimits &Limits);

	/** The highest the flyer may go (world Z); no limit when lifted. */
	WORLD_OF_REFRACTION_API double GetFlightCeilingZ(double TakeOffGroundZ, const FFieldFlightLimits &Limits);

	// ---- B-214e: advanced moves (FieldAbilities; numbers TODO: tune, Q-214e) ----

	constexpr int32 TELEPORT_EP_COST = 3;
	constexpr float TELEPORT_DISTANCE = 1500.0f;      // cm along the aim, stopping short of a wall (Void only)
	constexpr float TELEPORT_COOLDOWN_SECONDS = 2.0f;
	constexpr float TELEPORT_MIN_DISTANCE = 100.0f;   // cm: a shorter move (a wall right ahead) is Blocked, not paid
	constexpr int32 PHASE_STEP_EP_COST = 2;
	constexpr float PHASE_STEP_DISTANCE = 400.0f;     // cm ahead, through what's in between (thin walls, bars)
	constexpr float PHASE_STEP_COOLDOWN_SECONDS = 1.0f;
	constexpr int32 RECALL_EP_COST = 3;               // placing the mark is free
	constexpr float RECALL_COOLDOWN_SECONDS = 1.0f;
	constexpr int32 SUPER_LAUNCH_EP_COST = 3;
	constexpr float SUPER_LAUNCH_SPEED = 2500.0f;     // cm/s up, then a free glide from the top
	constexpr float SUPER_LAUNCH_COOLDOWN_SECONDS = 2.0f;
	constexpr int32 REGROUP_EP_COST = 3;
	constexpr float REGROUP_OFFSET = 150.0f;          // cm behind the party member
	constexpr float REGROUP_COOLDOWN_SECONDS = 5.0f;
	constexpr float NO_COOLDOWN_SECONDS = 0.0f;

	/** The EP a B-214e move costs (the recall's, for Mark and recall). 0 for other types. */
	WORLD_OF_REFRACTION_API int32 GetAdvancedMoveEPCost(EFieldMovementType Type);

	/** Its cooldown in seconds. NO_COOLDOWN_SECONDS for other types. */
	WORLD_OF_REFRACTION_API float GetAdvancedMoveCooldown(EFieldMovementType Type);

	/** A B-214e move may be used: Regroup not in a fight, off cooldown, with its EP. */
	WORLD_OF_REFRACTION_API EFieldSkillRefusal CheckAdvancedMove(EFieldMovementType Type, int32 CurrentEP,
																double SecondsSinceLastUse, bool bInFight);

	// ---- B-214f: the field attack (Q-214f; numbers TODO: tune) ----

	constexpr float FIELD_ATTACK_RANGE = 200.0f;          // cm ahead
	constexpr float FIELD_ATTACK_RADIUS = 80.0f;          // cm, the swept sphere
	constexpr float FIELD_ATTACK_COOLDOWN_SECONDS = 0.6f;
	constexpr float FIELD_STRIKE_BUILDUP = 25.0f;         // status buildup each enemy takes at the start
	constexpr int32 FIELD_ATTACK_SOURCE_COUNT = 3;        // Innate, Weapon, Ring (EFieldSkillSource)

	/** The strike a field hit carries: the element if it is a real one, the weapon's physical type, Ice when held with
	 *  a Water source, FIELD_STRIKE_BUILDUP. */
	WORLD_OF_REFRACTION_API FFieldStrike MakeFieldStrike(ESpellElement SourceElement, EPhysicalDamageType Physical, bool bHeld);

	/** The next field attack source: Innate -> Weapon -> Ring -> Innate. */
	WORLD_OF_REFRACTION_API EFieldSkillSource NextSource(EFieldSkillSource Source);

	/** Glide may start: in the air with its EP. */
	WORLD_OF_REFRACTION_API EFieldSkillRefusal CheckGlideStart(bool bFalling, int32 CurrentEP);

	/** High jump on the ground; in the air the one double jump. */
	WORLD_OF_REFRACTION_API EFieldSkillRefusal CheckJump(bool bOnGround, bool bAirJumpUsed, int32 CurrentEP);

	/** Air dash: in the air, once per airtime. */
	WORLD_OF_REFRACTION_API EFieldSkillRefusal CheckAirDash(bool bFalling, bool bAirDashUsed, int32 CurrentEP);

	/** A wall-run step may hold: Enhanced Sprint on, in the air, at sprint speed, a wall beside, time left. */
	WORLD_OF_REFRACTION_API bool CanWallRun(bool bEnhancedSprintActive, bool bFalling, float GroundSpeed, bool bWallBeside,
											float SecondsOnWall);

	/** Lightning's air dash is a chain of short blinks (FieldAbilities element styles). */
	WORLD_OF_REFRACTION_API bool IsBlinkAirDash(ESpellElement Element);

	/** A real element: not Generic ("inherit") and not None. */
	WORLD_OF_REFRACTION_API bool IsElemental(ESpellElement Element);

	/** B-214a builds Dash and Enhanced Sprint; B-214c Glide, High jump and Air dash; B-214d Flight; B-214e the rest. */
	WORLD_OF_REFRACTION_API bool IsBuilt(EFieldMovementType Type);

	/** Every type but the normal dash needs an elemental source (a Generic can't fly without one, FieldAbilities). */
	WORLD_OF_REFRACTION_API bool NeedsElementalSource(EFieldMovementType Type);

	/** Can a character pick Type with a source of SourceElement? bUnlocked: the account has unlocked Type for that
	 *  element (UFieldSkillUnlockSubsystem::IsUnlocked); the dash ignores it. */
	WORLD_OF_REFRACTION_API EFieldSkillRefusal CheckPick(EFieldMovementType Type, ESpellElement SourceElement, bool bUnlocked);

	/** Can Enhanced Sprint start now? The pick rule again (gear may have changed), then the first second's EP. */
	WORLD_OF_REFRACTION_API EFieldSkillRefusal CheckEnhancedSprintStart(ESpellElement SourceElement, int32 CurrentEP, bool bUnlocked);

	// ---- B-214b: unlocks (FieldAbilities "Rank needed", Crown 2026-10-27) ----

	constexpr int32 SIMPLE_UNLOCK_RANK = 3;   // D tier: Dash, Enhanced Sprint, Glide, High jump, Air dash
	constexpr int32 ADVANCED_UNLOCK_RANK = 7; // S tier (max): every other type
	constexpr ESpellElement TELEPORT_ELEMENT = ESpellElement::Void; // Teleport is Void only (Crown, 2026-10-27)

	/** A spell tier as the design's rank: F = 1 ... D = 3 ... S = 7. */
	WORLD_OF_REFRACTION_API int32 TierToRank(EItemTier Tier);

	/** Every type but the normal dash (everyone's) has to be unlocked. */
	WORLD_OF_REFRACTION_API bool NeedsUnlock(EFieldMovementType Type);

	/** The auras whose spells unlock Type; where several are listed, all are needed (FieldAbilities table). */
	WORLD_OF_REFRACTION_API TArray<EFieldAura> GetUnlockAuras(EFieldMovementType Type);

	/** SIMPLE_UNLOCK_RANK or ADVANCED_UNLOCK_RANK. */
	WORLD_OF_REFRACTION_API int32 GetUnlockRank(EFieldMovementType Type);

	/** Can a variant of this element unlock Type at all? Only real elements (a Generic spell is the standard version,
	 *  not a variant), and Teleport only for Void. */
	WORLD_OF_REFRACTION_API bool CanElementUnlock(EFieldMovementType Type, ESpellElement Element);

	/** Type is unlocked for Element when each of its auras has reached its rank in that element. BestRank gives the
	 *  best rank the account has reached for an aura in Element (0 = never). */
	WORLD_OF_REFRACTION_API bool IsUnlockedByRanks(EFieldMovementType Type, ESpellElement Element,
												   TFunctionRef<int32(EFieldAura)> BestRank);

	/** Is a pawn moving this fast along the ground sprinting (so an Enhanced Sprint second is charged)? */
	WORLD_OF_REFRACTION_API bool IsMovingAtSprintSpeed(float GroundSpeed);

	/** Sprint speed multiplier while Enhanced Sprint runs with this element; NO_SPRINT_BOOST for a non-element. */
	WORLD_OF_REFRACTION_API float GetEnhancedSprintMultiplier(ESpellElement Element);

	/** Enum display names for logs and the debug print. */
	WORLD_OF_REFRACTION_API FString TypeName(EFieldMovementType Type);
	WORLD_OF_REFRACTION_API FString SourceName(EFieldSkillSource Source);
	WORLD_OF_REFRACTION_API FString RefusalName(EFieldSkillRefusal Refusal);
	WORLD_OF_REFRACTION_API FString AuraName(EFieldAura Aura);
}
