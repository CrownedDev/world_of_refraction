// InfusionLook.h
// B-626 (D-277, Crown 2026-11-25): an infusion's look depends on its source. A weapon crystal shows the weapon glow, an
// adapter (ring) a flame on a finger, Innate or Evolution (and Raw, Q-626) an aura on the body. Stacked sources (B-435)
// each show their own kind at once. Only the defaults exist now (UImpactVisualDefaults); a later look picker may
// replace a kind's look with another of the same kind only (CanWear).

#pragma once

#include "CoreMinimal.h"
#include "Infusion/EInfusionSourceOption.h"
#include "Skills/Definitions/ESpellElement.h"
#include "InfusionLook.generated.h"

class UNiagaraSystem;
struct FLoadoutSlot;

/** Where an infusion's look shows: what kind of look a source wears. */
UENUM(BlueprintType)
enum class EInfusionLookKind : uint8
{
	None UMETA(DisplayName = "None"),
	/** The weapon glow, on the weapon in hand (a weapon crystal). */
	Weapon UMETA(DisplayName = "Weapon Glow"),
	/** A flame on the finger the adapter is worn on. */
	Ring UMETA(DisplayName = "Ring Flame"),
	/** An aura around the body (Innate, Evolution, Raw). */
	Aura UMETA(DisplayName = "Body Aura"),
};

/** One kind's looks: each element's, then a default (a missing element row, Raw). Tinted by ElementColor and the
 *  SoftTofu User.Color_. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FInfusionLookSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion Looks")
	TMap<ESpellElement, TSoftObjectPtr<UNiagaraSystem>> ElementVFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion Looks")
	TSoftObjectPtr<UNiagaraSystem> DefaultVFX;

	/** Element's row, else the default; empty when neither is set. */
	TSoftObjectPtr<UNiagaraSystem> GetVFX(ESpellElement Element) const;
};

namespace InfusionLook
{
	/** The right hand's finger bones, index first (Q-626: the reference default; Crown may change the hand). */
	WORLD_OF_REFRACTION_API TArray<FName> GetDefaultRingFingerSockets();

	/** The look kind Source wears: WeaponCrystal a weapon glow, ActiveRing / PrimaryRing a ring flame, Innate,
	 *  Evolution and Raw an aura; None for no infusion. */
	WORLD_OF_REFRACTION_API EInfusionLookKind KindForSource(EInfusionSourceOption Source);

	/** A later look picker's rule: a look may replace only one of its own kind. */
	inline bool CanWear(EInfusionLookKind Look, EInfusionLookKind Place) { return Look != EInfusionLookKind::None && Look == Place; }

	/** The finger the adapter in InUse is worn on: 0 for the first adapter in the loadout's slot order (the index
	 *  finger), 1 for the second, and so on; weapons don't count. INDEX_NONE when InUse isn't an adapter of Slots. */
	WORLD_OF_REFRACTION_API int32 RingFingerIndex(const TArray<FLoadoutSlot> &Slots, const FLoadoutSlot *InUse);

	/** Sockets[FingerIndex], the last finger past the end; NAME_None with no sockets or no finger. */
	WORLD_OF_REFRACTION_API FName FingerSocket(const TArray<FName> &Sockets, int32 FingerIndex);

	/** The kinds a stack shows, each once, in the stack's order (two aura sources show one aura). */
	WORLD_OF_REFRACTION_API TArray<EInfusionLookKind> KindsForStack(const TArray<EInfusionSourceOption> &Stack);
}
