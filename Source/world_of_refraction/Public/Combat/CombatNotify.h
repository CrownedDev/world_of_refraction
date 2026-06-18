// CombatNotify.h
// Production combat notify (Stage 2): self-identifying <Family><Index> montage
// notify for the four-array model (§13 index-by-position). The runner wires it
// at Stage 12; until then nothing consumes it.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Skills/Definitions/SkillVFXEntry.h"
#include "CombatNotify.generated.h"

/** The four tag-matched skill arrays (four-array model): Impact / Cast / VFX / SFX.
 *  ("Impact" was "Hit" pre-defense-rework — unified with projectile arrivals; an
 *  EnumRedirect in DefaultEngine.ini maps authored "Hit" notifies to "Impact".) */
UENUM(BlueprintType)
enum class ECombatNotifyFamily : uint8
{
	Impact,
	Cast,
	VFX,
	SFX
};

/**
 * Self-identifying montage notify: carries its family + index as picked fields
 * (index-by-position authoring — nothing typed, array position IS identity).
 * Notify() broadcasts through UCombatAnimInstance::OnCombatNotify. Coexists
 * with the engine Montage-Notify path (OnActionNotify) until Stage 12 replaces
 * the spike's authoring.
 */
UCLASS(meta = (DisplayName = "Combat Notify"))
class WORLD_OF_REFRACTION_API UCombatNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	UCombatNotify();

	/** Which skill array this notify indexes into. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	ECombatNotifyFamily Family = ECombatNotifyFamily::Impact;

	/** Position within the family's array — array position IS identity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = 0))
	int32 Index = 0;

	/** Telegraph "incoming!" tell — a VFX played TelegraphLeadSeconds (wall-clock) BEFORE this notify
	 *  fires. PURELY cosmetic: opens no window, gates no input, touches no difficulty. Authored inline
	 *  on the notify (one per notify, not an array). An unset VFX means no telegraph. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Telegraph")
	FSkillVFXEntry Telegraph;

	/** Wall-clock seconds before this notify to play Telegraph. 0 = no telegraph. Rate-independent:
	 *  the scheduler divides the notify's montage-local time by the play rate then subtracts this, so
	 *  the warning lead stays constant regardless of montage speed. A lead longer than the time-to-notify
	 *  fires the tell at montage start. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Telegraph", meta = (ClampMin = "0.0"))
	float TelegraphLeadSeconds = 0.0f;

	virtual void Notify(USkeletalMeshComponent *MeshComp, UAnimSequenceBase *Animation,
						const FAnimNotifyEventReference &EventReference) override;

	/** Montage-editor label: "<Family><Index>" — "Impact0", "Cast1" (locked
	 *  authoring convention, no underscore). */
	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
	/** Keep the track color in sync when the Family field is re-picked. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
#endif

private:
	static const TCHAR *FamilyToString(ECombatNotifyFamily InFamily);

#if WITH_EDITORONLY_DATA
	/** Distinct track color per family — authoring aid in the montage editor. */
	void RefreshNotifyColor();
#endif
};
