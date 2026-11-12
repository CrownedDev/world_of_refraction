// ParryVisuals.cpp
// B-393: see ParryVisuals.h.

#include "Combat/Presentation/ParryVisuals.h"
#include "Combat/Presentation/ImpactVisuals.h"
#include "Combat/CombatPresentationComponent.h"
#include "Character/CharacterData.h"
#include "Character/CosmeticsData.h"
#include "Character/ECharacterClass.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Infusion/ElementColors.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/Entries/FRingLoadoutEntry.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

namespace
{
	const TCHAR *ParryElementSourceName(EParryElementSource Source)
	{
		switch (Source)
		{
		case EParryElementSource::None:
			return TEXT("none");
		case EParryElementSource::Weapon:
			return TEXT("Weapon");
		case EParryElementSource::Magic:
			return TEXT("Magic");
		}
		return TEXT("?");
	}

	FString ParryElementName(ESpellElement Element)
	{
		return StaticEnum<ESpellElement>()->GetNameStringByValue(static_cast<int64>(Element));
	}

	FString ParryAssetName(const TSoftObjectPtr<UNiagaraSystem> &VFX)
	{
		return VFX.IsNull() ? FString(TEXT("(none)")) : VFX.GetAssetName();
	}

	/** The in-hand weapon's crystal element; None with no weapon, an empty socket or a broken crystal. */
	ESpellElement WeaponCrystalElement(const ULoadoutComponent &Loadout)
	{
		const FRuntimeAttachedItem *Attachment = Loadout.GetActiveWeapon() ? Loadout.GetActiveWeaponAttachment() : nullptr;
		return Attachment && !Attachment->IsEmpty() && !Attachment->IsBroken() ? Attachment->GetElement() : ESpellElement::None;
	}

	/** A Refractor's element (D-170: its source element), else the adapter's crystal: a Resonator's active adapter,
	 *  another class's primary one. FRingInventoryEntry::GetElement already reads a broken crystal as None. */
	ESpellElement OwnMagicElement(const ULoadoutComponent &Loadout)
	{
		const UCharacterData *CharData = Loadout.GetOwnerCharacterData();
		if (CharData && CharData->CharacterClass == ECharacterClass::Caster && ParryVisuals::IsColouringElement(CharData->GetElement()))
		{
			return CharData->GetElement();
		}
		const FRingLoadoutEntry *Adapter = CharData && CharData->CharacterClass == ECharacterClass::Resonator
											   ? Loadout.GetActiveRingLoadout()
											   : Loadout.GetPrimaryRingLoadout();
		return Adapter ? Adapter->RingEntry.GetElement() : ESpellElement::None;
	}

	/** Spawns System at Location (tinted when bTinted) and sends the same burst to the clients (B-231c). */
	void SpawnParryBurst(AActor *Defender, UNiagaraSystem *System, const FVector &Location, bool bTinted, const FLinearColor &Tint)
	{
		UNiagaraComponent *Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(Defender, System, Location);
		if (Spawned && bTinted)
		{
			Spawned->SetColorParameter(ParryVisuals::TINT_CORE_PARAM, Tint);
			Spawned->SetColorParameter(ParryVisuals::TINT_EDGE_PARAM, Tint);
		}
		if (UCombatPresentationComponent *Presentation = UCombatPresentationComponent::Find(Defender))
		{
			FCosmeticVFX ForClients;
			ForClients.System = System;
			ForClients.Location = Location;
			ForClients.bTinted = bTinted;
			ForClients.CoreColor = Tint;
			ForClients.EdgeColor = Tint;
			Presentation->SendVFX(ForClients);
		}
	}
}

namespace ParryVisuals
{
	bool IsColouringElement(ESpellElement Element)
	{
		return Element != ESpellElement::None && Element != ESpellElement::Generic;
	}

	EParryElementSource PickElementSource(const FParryVisualInputs &Inputs)
	{
		// Q-393: an unarmed guard has put the weapon away, so only the weapon guard reads the weapon's crystal.
		const bool bWeapon = Inputs.Guard == EGuardStyle::Weapon && IsColouringElement(Inputs.WeaponElement);
		const bool bMagic = IsColouringElement(Inputs.MagicElement);
		if (bWeapon && bMagic)
		{
			return Inputs.Priority == EParryElementPriority::Magic ? EParryElementSource::Magic : EParryElementSource::Weapon;
		}
		if (bWeapon)
		{
			return EParryElementSource::Weapon;
		}
		return bMagic ? EParryElementSource::Magic : EParryElementSource::None;
	}

	FParryVisualChoice Resolve(const FParryVisualInputs &Inputs)
	{
		FParryVisualChoice Choice;
		// D-170: weapons always spark; a Perfect Guard's flash takes the spark's place (B-202, DefenseSystem).
		if (Inputs.Guard == EGuardStyle::Weapon && !Inputs.bPerfect)
		{
			Choice.Spark = Inputs.Spark;
		}
		Choice.ElementSource = PickElementSource(Inputs);
		if (Choice.ElementSource == EParryElementSource::None)
		{
			return Choice; // no element behind it: the spark only (weapon) or nothing (unarmed)
		}
		Choice.Element = Choice.ElementSource == EParryElementSource::Weapon ? Inputs.WeaponElement : Inputs.MagicElement;
		Choice.Tint = ElementColors::GetColorForElement(Choice.Element);
		Choice.Effect = Inputs.Guard == EGuardStyle::Weapon ? Inputs.WeaponGuardEffect : Inputs.UnarmedGuardEffect;
		return Choice;
	}

	FParryVisualInputs GatherInputs(const AActor *Defender, bool bPerfect)
	{
		FParryVisualInputs Inputs;
		Inputs.bPerfect = bPerfect;
		const ULoadoutComponent *Loadout = Defender ? Defender->FindComponentByClass<ULoadoutComponent>() : nullptr;
		if (!Loadout)
		{
			return Inputs;
		}
		Inputs.Guard = Loadout->GetGuardStyle();
		Inputs.WeaponElement = WeaponCrystalElement(*Loadout);
		Inputs.MagicElement = OwnMagicElement(*Loadout);
		Inputs.Priority = Loadout->GetParryElementPriority();
		const UCharacterData *CharData = Loadout->GetOwnerCharacterData();
		if (const UCosmeticsData *Cosmetics = CharData ? CharData->Cosmetics : nullptr)
		{
			Inputs.Spark = Cosmetics->GuardSparkVFX;
			Inputs.WeaponGuardEffect = Cosmetics->WeaponGuardParryEffectVFX;
			Inputs.UnarmedGuardEffect = Cosmetics->UnarmedGuardParryEffectVFX;
		}
		return Inputs;
	}

	FParryVisualChoice ResolveForDefender(const AActor *Defender, bool bPerfect)
	{
		return Resolve(GatherInputs(Defender, bPerfect));
	}

	FParryVisualChoice SpawnForLandedParry(AActor *Defender, bool bPerfect)
	{
		if (!IsValid(Defender))
		{
			return FParryVisualChoice();
		}
		const FParryVisualChoice Choice = ResolveForDefender(Defender, bPerfect);
		if (!Choice.ShouldSpawnSpark() && !Choice.ShouldSpawnEffect())
		{
			return Choice;
		}
		const FVector Location = ImpactVisuals::GetSpawnLocation(Defender);
		// Small assets, loaded on first use (as the Perfect Guard flash). TODO: preload with the fight's cosmetics if it hitches.
		if (UNiagaraSystem *Spark = Choice.ShouldSpawnSpark() ? Choice.Spark.LoadSynchronous() : nullptr)
		{
			SpawnParryBurst(Defender, Spark, Location, false, FLinearColor::White);
		}
		if (UNiagaraSystem *Effect = Choice.ShouldSpawnEffect() ? Choice.Effect.LoadSynchronous() : nullptr)
		{
			SpawnParryBurst(Defender, Effect, Location, true, Choice.Tint);
		}
		return Choice;
	}

	FString GetInputsString(const FParryVisualInputs &Inputs)
	{
		return FString::Printf(TEXT("guard %s | weapon %s, magic %s, priority %s"),
							   *StaticEnum<EGuardStyle>()->GetNameStringByValue(static_cast<int64>(Inputs.Guard)),
							   *ParryElementName(Inputs.WeaponElement), *ParryElementName(Inputs.MagicElement),
							   *StaticEnum<EParryElementPriority>()->GetNameStringByValue(static_cast<int64>(Inputs.Priority)));
	}

	FString GetChoiceString(const FParryVisualChoice &Choice)
	{
		const FString Spark = Choice.ShouldSpawnSpark() ? FString::Printf(TEXT("spark %s"), *ParryAssetName(Choice.Spark)) : FString(TEXT("no spark"));
		const FString Effect = Choice.ElementSource == EParryElementSource::None
								   ? FString(TEXT("no effect (no element)"))
								   : FString::Printf(TEXT("effect %s tinted %s (%s)"), *ParryAssetName(Choice.Effect), *ParryElementName(Choice.Element),
													 ParryElementSourceName(Choice.ElementSource));
		return FString::Printf(TEXT("%s + %s"), *Spark, *Effect);
	}
}
