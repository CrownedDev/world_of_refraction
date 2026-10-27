// WoREffectSpecBuilder.h
// GAS stage 3 (B-042b, step 6): turns an enum-path effect (FActiveSkillEffect) into a GameplayEffect
// spec: the family class for its type, SetByCaller.Magnitude = its stacked value, its identity in
// the FWoREffectContext, Immunity.* / Effect.Origin.Gear as dynamic tags, and a tick spec for a
// DOT or HOT. Apply replaces the target's GE of the same identity (Q-044 A: replace, fresh span)
// and hands a timed effect to the target's UWoRTurnEffectTracker. The enum path still decides
// stacking, does-not-stack and fires-once; the facade (B-042c) mirrors its result through here.
// Unported types (GetEffectClass null) and instant effects build nothing yet (B-042d).
// Debug: GetDebugString (every GE on an ASC with its identity and magnitude).

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayEffectTypes.h"
#include "GAS/WoREffectContext.h"
#include "Templates/SubclassOf.h"

class AActor;
class UAbilitySystemComponent;
class UGameplayEffect;
struct FActiveSkillEffect;

/** What Build makes: the lasting spec, its optional tick spec, and how long it lasts. */
struct FWoRBuiltEffect
{
	FGameplayEffectSpecHandle Spec;
	FGameplayEffectSpecHandle TickSpec;
	int32 Turns = 0;
	bool bPermanent = false;

	bool IsValid() const { return Spec.IsValid(); }
};

struct WORLD_OF_REFRACTION_API FWoREffectSpecBuilder
{
	/** The family GE class for an effect type; null when the type isn't ported yet. */
	static TSubclassOf<UGameplayEffect> GetEffectClass(ESkillEffectType Type);

	static FWoREffectIdentity MakeIdentity(const FActiveSkillEffect &Effect);

	/** Build Effect's spec on the target's ASC, with Source as instigator. Invalid when the type
	 *  isn't ported or the effect is instant. bAlreadyStored: Effect is held on the enum path, so it
	 *  lasts even when its turns read 0 (a holder-turn span that counted its landing turn). */
	static FWoRBuiltEffect Build(UAbilitySystemComponent &TargetASC, const FActiveSkillEffect &Effect, AActor *Source,
								 bool bAlreadyStored = false);

	/** Build and apply to Target, replacing its GE of the same identity. Timed effects go through
	 *  the target's tracker, permanent ones straight onto the ASC. Invalid handle when nothing applied. */
	static FActiveGameplayEffectHandle Apply(AActor *Target, const FActiveSkillEffect &Effect, AActor *Source);

	/** The active GE on ASC with this identity (FWoREffectIdentity::IsSameIdentity), or invalid. */
	static FActiveGameplayEffectHandle FindByIdentity(const UAbilitySystemComponent &ASC, const FWoREffectIdentity &Identity);

	/** Sum of SetByCaller.Magnitude over the active GEs of this type: the GAS twin of
	 *  USkillEffectManager::GetTotalStatModifier. */
	static float SumMagnitude(const UAbilitySystemComponent &ASC, ESkillEffectType Type);

	/** The same, over the GEs of exactly this element: the twin of GetTotalStatModifierForElement. */
	static float SumMagnitudeForElement(const UAbilitySystemComponent &ASC, ESkillEffectType Type, ESpellElement Element);

	/** One line per active GE: class, identity, magnitude. */
	static FString GetDebugString(const UAbilitySystemComponent &ASC);
};
