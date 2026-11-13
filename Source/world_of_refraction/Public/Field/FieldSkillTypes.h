// FieldSkillTypes.h
// B-214a (D-099, Design/FieldAbilities.md): field movement on the map. One active field skill per character: a source
// (innate / weapon / ring) and a movement type. The source's element styles it and sets its strength; the normal dash
// is everyone's and needs no source. FieldSkillRules holds the pure pick / start / speed rules the component applies. B-214b adds the unlock rules
// (an aura spell of the source's element at rank 3 or 7 unlocks a type; UFieldSkillUnlockSubsystem keeps the account's ranks).

#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemTier.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Templates/Function.h"
#include "FieldSkillTypes.generated.h"

/** The 11 movement types (FieldAbilities "Movement types", Crown 2026-10-27). B-214a builds Dash and Enhanced Sprint;
 *  the rest are refused as not built until their slice (B-214c..e). Append only: saved as a byte (FSavedPartyMember). */
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
	NotUnlocked           // B-214b: no aura spell of the source's element has reached the rank yet
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

	/** A real element: not Generic ("inherit") and not None. */
	WORLD_OF_REFRACTION_API bool IsElemental(ESpellElement Element);

	/** B-214a builds Dash and Enhanced Sprint. */
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
