// WoREffectContext.h
// GAS stage 3 (B-042b): who an effect belongs to travels in the effect context (the standard GAS
// extension, as Lyra does), so the tracker, cleanse and UI can read it from any active effect.
// FWoREffectIdentity mirrors FActiveSkillEffect's identity (B-062: EffectID + SourceKey; D-012,
// D-053) plus its type, element and gear origin (D-015), so both paths can be compared until the
// enum storage retires (B-042g). UWoRAbilitySystemGlobals makes every context an FWoREffectContext
// (set in DefaultGame.ini, [/Script/GameplayAbilities.AbilitySystemGlobals]).
// Debug: FWoREffectIdentity::ToString, FWoREffectContext::ToString.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffectTypes.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "WoREffectContext.generated.h"

struct FWoRBuildupRequest;
struct FWoRDamageRequest;

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FWoREffectIdentity
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	ESkillEffectType EffectType = ESkillEffectType::None;

	/** The effect's element; None when it has none. */
	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	ESpellElement Element = ESpellElement::None;

	/** The enum path's EffectID (EffectIdentity.h), kept for parity until B-042g. */
	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	int32 EffectID = 0;

	/** Who gave it (EffectIdentity::MakeSourceKey); 0 = not stamped. */
	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	int32 SourceKey = 0;

	/** Came from worn gear: a cleanse leaves it (D-015). */
	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	bool bFromGear = false;

	/** Same identity as the enum path's (USkillEffectManager::FindEffectByIdentity): EffectID and source. */
	bool IsSameIdentity(const FWoREffectIdentity &Other) const
	{
		return EffectID == Other.EffectID && SourceKey == Other.SourceKey;
	}

	void Serialize(FArchive &Ar);

	FString ToString() const;
};

USTRUCT()
struct WORLD_OF_REFRACTION_API FWoREffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()

	FWoREffectIdentity Identity;

	/** A damage spec's gathered inputs (B-042e, UWoRDamageExecution). Authority only: not net serialized. */
	TSharedPtr<const FWoRDamageRequest> DamageRequest;

	/** A buildup spec's gathered inputs (B-042l, UWoRBuildupExecution). Authority only: not net serialized. */
	TSharedPtr<const FWoRBuildupRequest> BuildupRequest;

	/** The WoR context behind a handle, or null when the handle holds another kind (or none). */
	static const FWoREffectContext *Get(const FGameplayEffectContextHandle &Handle);
	static FWoREffectContext *GetMutable(FGameplayEffectContextHandle &Handle);

	virtual UScriptStruct *GetScriptStruct() const override { return StaticStruct(); }
	virtual FWoREffectContext *Duplicate() const override;
	virtual bool NetSerialize(FArchive &Ar, UPackageMap *Map, bool &bOutSuccess) override;
	virtual FString ToString() const override;
};

template <>
struct TStructOpsTypeTraits<FWoREffectContext> : public TStructOpsTypeTraitsBase2<FWoREffectContext>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRAbilitySystemGlobals : public UAbilitySystemGlobals
{
	GENERATED_BODY()

public:
	virtual FGameplayEffectContext *AllocGameplayEffectContext() const override;
};
