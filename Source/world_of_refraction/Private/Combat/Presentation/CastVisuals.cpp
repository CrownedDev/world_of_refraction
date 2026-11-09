// CastVisuals.cpp
// B-394: see CastVisuals.h.

#include "Combat/Presentation/CastVisuals.h"
#include "Combat/Actions/ActionStructs.h"
#include "Combat/Presentation/ImpactVisualDefaults.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "HAL/IConsoleManager.h"
#include "Loadout/LoadoutComponent.h"
#include "NiagaraSystem.h"
#include "UObject/UObjectIterator.h"
#include "WoRLog.h"

namespace
{
	const TCHAR *CastSourceName(ECastVFXSource Source)
	{
		switch (Source)
		{
		case ECastVFXSource::Weapon:
			return TEXT("Weapon");
		case ECastVFXSource::AdapterDefault:
			return TEXT("AdapterDefault");
		case ECastVFXSource::Skill:
			return TEXT("Skill");
		}
		return TEXT("?");
	}

	FString CastAssetName(const TSoftObjectPtr<UNiagaraSystem> &VFX)
	{
		return VFX.IsNull() ? FString(TEXT("(none)")) : VFX.GetAssetName();
	}

	/** The asset Source names, or a null pointer when it has none authored. */
	TSoftObjectPtr<UNiagaraSystem> CastAssetFor(ECastVFXSource Source, const FCastVisualInputs &Inputs, EAdapterForm Form,
											const UImpactVisualDefaults *Defaults)
	{
		switch (Source)
		{
		case ECastVFXSource::Weapon:
			return Inputs.Weapon ? Inputs.Weapon->CastVFX : TSoftObjectPtr<UNiagaraSystem>();
		case ECastVFXSource::AdapterDefault:
			if (Defaults)
			{
				if (const TSoftObjectPtr<UNiagaraSystem> *Found = Defaults->AdapterCastVFX.Find(Form))
				{
					return *Found;
				}
			}
			return TSoftObjectPtr<UNiagaraSystem>();
		case ECastVFXSource::Skill:
			return Inputs.SkillVFX;
		}
		return TSoftObjectPtr<UNiagaraSystem>();
	}
}

ECastVFXSource CastVisuals::PickSource(const FCastVisualInputs &Inputs, ECastVFXPreference GenericPreference)
{
	if (!Inputs.bHasAction)
	{
		return ECastVFXSource::Skill;
	}
	// D-170: a weapon's attack VFX are fixed; attacks and abilities are the weapon's.
	if (!Inputs.bIsSpell)
	{
		return ECastVFXSource::Weapon;
	}
	if (Inputs.SpellSource == ESpellSource::Item)
	{
		return ECastVFXSource::Skill;
	}
	// D-170: a Generic with both a weapon and an adapter shows the class preference, whichever one the spell came from.
	if (Inputs.CharacterClass == ECharacterClass::Generic && Inputs.Weapon && Inputs.Adapter)
	{
		return GenericPreference == ECastVFXPreference::Weapon ? ECastVFXSource::Weapon : ECastVFXSource::AdapterDefault;
	}
	if (Inputs.SpellSource == ESpellSource::WeaponCrystal)
	{
		return ECastVFXSource::Weapon;
	}
	// With no adapter worn, only a Refractor's spells take the Ring default (Q-394): an enemy or a Generic / Resonator
	// casting an innate spell keeps the skill's own visual, so a filled Ring row doesn't reskin every monster's cast.
	if (!Inputs.Adapter && Inputs.CharacterClass != ECharacterClass::Caster)
	{
		return ECastVFXSource::Skill;
	}
	// An adapter's spell, a Refractor's innate spell and an evolution's spell: the adapter type's default (Q-394).
	return ECastVFXSource::AdapterDefault;
}

FCastVisualChoice CastVisuals::Resolve(const FCastVisualInputs &Inputs, const UImpactVisualDefaults *Defaults)
{
	const ECastVFXPreference Preference = Defaults ? Defaults->GenericCastVFXPreference : FALLBACK_GENERIC_PREFERENCE;

	FCastVisualChoice Choice;
	Choice.Source = PickSource(Inputs, Preference);
	Choice.AdapterForm = Inputs.Adapter ? Inputs.Adapter->Form : DEFAULT_ADAPTER_FORM;
	Choice.VFX = CastAssetFor(Choice.Source, Inputs, Choice.AdapterForm, Defaults);
	if (Choice.VFX.IsNull() && Choice.Source != ECastVFXSource::Skill)
	{
		// Nothing authored for the picked source yet: today's visual, so play is unchanged until the data is set.
		Choice.VFX = Inputs.SkillVFX;
		Choice.bFellBackToSkill = true;
	}
	return Choice;
}

FCastVisualInputs CastVisuals::GatherInputs(const AActor *Caster, const FAction *Action, bool bIsSpell,
											 const TSoftObjectPtr<UNiagaraSystem> &SkillVFX)
{
	FCastVisualInputs Inputs;
	Inputs.bHasAction = Action != nullptr;
	Inputs.bIsSpell = bIsSpell;
	Inputs.SpellSource = Action ? Action->SpellSource : ESpellSource::Innate;
	Inputs.SkillVFX = SkillVFX;

	const ULoadoutComponent *Loadout = Caster ? Caster->FindComponentByClass<ULoadoutComponent>() : nullptr;
	if (Loadout)
	{
		Inputs.CharacterClass = Loadout->GetCharacterClass();
		Inputs.Weapon = Loadout->GetActiveWeapon();
		// GetActiveRing answers for a Resonator only, GetPrimaryRing for the other classes (B-304).
		const URingData *Active = Loadout->GetActiveRing();
		Inputs.Adapter = Active ? Active : Loadout->GetPrimaryRing();
	}
	return Inputs;
}

FCastVisualChoice CastVisuals::ResolveForCaster(const AActor *Caster, const FAction *Action, bool bIsSpell,
												const TSoftObjectPtr<UNiagaraSystem> &SkillVFX)
{
	return Resolve(GatherInputs(Caster, Action, bIsSpell, SkillVFX), UImpactVisualDefaults::Get());
}

FString CastVisuals::GetChoiceString(const FCastVisualChoice &Choice)
{
	const FString Source = Choice.Source == ECastVFXSource::AdapterDefault
							   ? FString::Printf(TEXT("AdapterDefault (%s)"), *UEnum::GetDisplayValueAsText(Choice.AdapterForm).ToString())
							   : FString(CastSourceName(Choice.Source));
	return FString::Printf(TEXT("%s%s: %s"), *Source, Choice.bFellBackToSkill ? TEXT(" -> skill fallback") : TEXT(""),
						   *CastAssetName(Choice.VFX));
}

namespace
{
	struct FCastKindProbe
	{
		const TCHAR *Label;
		bool bIsSpell;
		ESpellSource Source;
	};

	const FCastKindProbe CAST_KIND_PROBES[] = {
		{TEXT("attack / ability"), false, ESpellSource::Innate},
		{TEXT("weapon-crystal spell"), true, ESpellSource::WeaponCrystal},
		{TEXT("adapter spell"), true, ESpellSource::RingCrystal},
		{TEXT("innate spell"), true, ESpellSource::Innate},
		{TEXT("evolution spell"), true, ESpellSource::Evolution},
		{TEXT("spell item"), true, ESpellSource::Item},
	};

	FAutoConsoleCommandWithWorld GCastVisualsPrintCommand(
		TEXT("wor.VFX.PrintCastSource"),
		TEXT("B-394 (D-170): every combatant's cast visual source for each kind of cast (skill VFX shown as (none))."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			const UImpactVisualDefaults *Defaults = UImpactVisualDefaults::Get();
			UE_LOG(LogWoR, Display, TEXT("[CastVFX] defaults %s, Generic preference %s, %d adapter default(s)"),
				   Defaults ? *Defaults->GetName() : TEXT("(none)"),
				   *UEnum::GetDisplayValueAsText(Defaults ? Defaults->GenericCastVFXPreference : CastVisuals::FALLBACK_GENERIC_PREFERENCE).ToString(),
				   Defaults ? Defaults->AdapterCastVFX.Num() : 0);
			for (TObjectIterator<ULoadoutComponent> It; It; ++It)
			{
				if (It->IsTemplate() || It->GetWorld() != World)
				{
					continue;
				}
				const AActor *Owner = It->GetOwner();
				FAction Probe;
				const FCastVisualInputs Base = CastVisuals::GatherInputs(Owner, &Probe, false, TSoftObjectPtr<UNiagaraSystem>());
				UE_LOG(LogWoR, Display, TEXT("[CastVFX] %s (%s): weapon %s (cast VFX %s), adapter %s"),
					   *GetNameSafe(Owner), *UEnum::GetDisplayValueAsText(Base.CharacterClass).ToString(),
					   *GetNameSafe(Base.Weapon), Base.Weapon ? *CastAssetName(Base.Weapon->CastVFX) : TEXT("-"),
					   Base.Adapter ? *UEnum::GetDisplayValueAsText(Base.Adapter->Form).ToString() : TEXT("(none)"));
				for (const FCastKindProbe &Row : CAST_KIND_PROBES)
				{
					Probe.SpellSource = Row.Source;
					const FCastVisualChoice Choice = CastVisuals::Resolve(
						CastVisuals::GatherInputs(Owner, &Probe, Row.bIsSpell, TSoftObjectPtr<UNiagaraSystem>()), Defaults);
					UE_LOG(LogWoR, Display, TEXT("  %s: %s"), Row.Label, *CastVisuals::GetChoiceString(Choice));
				}
			}
		}));
}
