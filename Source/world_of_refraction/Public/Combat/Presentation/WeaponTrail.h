// WeaponTrail.h
// Crown 2026-11-24: a weapon's swing trail on attacks and weapon skills. One universal trail (WeaponTrailVFX on
// UImpactVisualDefaults, empty until the art lands); bows, guns and shields have none. It plays inside a Weapon Trail window on the
// attack montage (UWeaponTrailWindow); a montage with none runs it from the first Impact's wind-up to the last Impact
// (ScheduleFallback). The ribbon runs between the TrailStart and TrailEnd sockets on the weapon mesh, else along the
// mesh's long axis, and is tinted by the infusion's element (plain with none, Physical or Raw). Impact VFX are B-369's.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Equipment/Weapons/EWeaponType.h"
#include "Skills/Definitions/ESpellElement.h"
#include "WeaponTrail.generated.h"

class UAnimMontage;
class UMeshComponent;
class UNiagaraComponent;

namespace WeaponTrail
{
	/** Sockets on the weapon mesh the ribbon runs between. */
	inline const FName START_SOCKET = TEXT("TrailStart");
	inline const FName END_SOCKET = TEXT("TrailEnd");

	/** The trail system's user parameters: the two ends (the weapon mesh's local space; author the system in Local
	 *  Space) and the tint (the same name as UInfusionVFXComponent::ColorParameterName). */
	inline const FName START_PARAM = TEXT("TrailStart");
	inline const FName END_PARAM = TEXT("TrailEnd");
	inline const FName COLOR_PARAM = TEXT("ElementColor");

	/** Tag on a playing trail, so Stop finds it. */
	inline const FName COMPONENT_TAG = TEXT("WoRWeaponTrail");

	/** Every weapon type trails except bows, guns and shields. */
	bool HasTrail(EWeaponType Type);

	/** The element's tint; None and Generic (no infusion, Physical, Raw) take Plain. */
	FLinearColor ColorFor(ESpellElement Element, const FLinearColor &Plain);

	/** The fallback window from the Impact times (seconds, sorted): the first Impact minus WindUp (not before 0) to the
	 *  last Impact. False with no Impact or an empty window. */
	bool FindFallbackWindow(const TArray<float> &ImpactTimes, float WindUp, float &OutStart, float &OutEnd);

	/** The montage's Impact notify times (montage seconds), sorted. */
	TArray<float> GetImpactTimes(const UAnimMontage &Montage);

	/** The montage has its own Weapon Trail window. */
	bool HasTrailWindow(const UAnimMontage &Montage);

	/** The two ends of a box's longest axis, through its centre. */
	void LongAxisPoints(const FBox &Box, FVector &OutStart, FVector &OutEnd);

	/** The element of Owner's running action's infusion; None when it isn't acting or isn't infused. */
	ESpellElement GetInfusedElement(AActor *Owner);

	/** Play the trail on Owner's weapon in hand, tinted by Element; null with no weapon shown, no trail kind or no
	 *  trail system authored. */
	UNiagaraComponent *Start(AActor *Owner, ESpellElement Element);
	UNiagaraComponent *Start(AActor *Owner);

	/** Fade out Owner's playing trails. */
	void Stop(AActor *Owner);

	/** A montage with no window of its own: trail timers from the first Impact's wind-up to the last Impact. The start
	 *  timer goes in OutHandles (cleared with the action); the stop runs on its own so a trail never lingers.
	 *  shortcut: runs where the executor runs (the server); a co-op client sees a trail only from an authored window. */
	void ScheduleFallback(AActor *Owner, const UAnimMontage *Montage, float PlayRate, TArray<FTimerHandle> &OutHandles);

	/** Debug: Owner's weapon, its trail kind, the slot's system, the ends and whether they came from sockets. */
	FString GetTrailString(AActor *Owner);
}

/** The window a weapon's trail plays in, placed on an attack or weapon-skill montage. */
UCLASS(meta = (DisplayName = "Weapon Trail"))
class WORLD_OF_REFRACTION_API UWeaponTrailWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent *MeshComp, UAnimSequenceBase *Animation, float TotalDuration,
							 const FAnimNotifyEventReference &EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent *MeshComp, UAnimSequenceBase *Animation,
						   const FAnimNotifyEventReference &EventReference) override;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("WeaponTrail"); }
};
