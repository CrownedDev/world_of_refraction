// ImpactVisuals.cpp
// B-369: see ImpactVisuals.h.

#include "Combat/Presentation/ImpactVisuals.h"
#include "Combat/CombatPresentationComponent.h"
#include "Character/CharacterData.h"
#include "Character/CosmeticsData.h"
#include "Combat/Presentation/ImpactVisualDefaults.h"
#include "Combat/Defense/GuardRules.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "Loadout/LoadoutComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "WoRLog.h"

namespace
{
	TAutoConsoleVariable<bool> CVarImpactVFXLog(
		TEXT("wor.ImpactVFX.Log"),
		false,
		TEXT("DEBUG (B-369): log the visual chosen for every impact (outcome, source, asset, scale)."));

	const TCHAR *OutcomeName(EImpactOutcome Outcome)
	{
		switch (Outcome)
		{
		case EImpactOutcome::Hit:
			return TEXT("Hit");
		case EImpactOutcome::Crit:
			return TEXT("Crit");
		case EImpactOutcome::Guard:
			return TEXT("Guard");
		case EImpactOutcome::PerfectGuard:
			return TEXT("Perfect Guard");
		case EImpactOutcome::Dodge:
			return TEXT("Dodge");
		}
		return TEXT("?");
	}

	const TCHAR *SourceName(EImpactVisualSource Source)
	{
		switch (Source)
		{
		case EImpactVisualSource::None:
			return TEXT("none");
		case EImpactVisualSource::SkillEntry:
			return TEXT("skill Impact entry");
		case EImpactVisualSource::SkillCritEntry:
			return TEXT("skill CritImpact entry");
		case EImpactVisualSource::ElementDefault:
			return TEXT("element default");
		case EImpactVisualSource::GenericDefault:
			return TEXT("generic default");
		case EImpactVisualSource::GuardSpark:
			return TEXT("guard spark");
		case EImpactVisualSource::DodgeWhiff:
			return TEXT("dodge whiff");
		case EImpactVisualSource::PerfectGuardFlash:
			return TEXT("Perfect Guard flash (DefenseSystem)");
		case EImpactVisualSource::ProjectileArrival:
			return TEXT("projectile arrival (the projectile draws it)");
		}
		return TEXT("?");
	}

	FImpactVisualChoice Make(const TSoftObjectPtr<UNiagaraSystem> &VFX, EImpactVisualSource Source, float Scale = 1.0f)
	{
		FImpactVisualChoice Choice;
		Choice.VFX = VFX;
		Choice.Source = Source;
		Choice.Scale = Scale;
		return Choice;
	}

	const UCosmeticsData *GetCosmetics(const AActor *Actor)
	{
		const ULoadoutComponent *Loadout = Actor ? Actor->FindComponentByClass<ULoadoutComponent>() : nullptr;
		const UCharacterData *CharData = Loadout ? Loadout->GetOwnerCharacterData() : nullptr;
		return CharData ? CharData->Cosmetics : nullptr;
	}
}

namespace ImpactVisuals
{
	EImpactOutcome OutcomeFor(EDefenseType DefenseType, bool bSuccess, bool bPerfect, bool bCritical)
	{
		if (bSuccess && DefenseType == EDefenseType::Dodge)
		{
			return EImpactOutcome::Dodge;
		}
		if (bSuccess && GuardRules::IsGuard(DefenseType))
		{
			return bPerfect ? EImpactOutcome::PerfectGuard : EImpactOutcome::Guard;
		}
		return bCritical ? EImpactOutcome::Crit : EImpactOutcome::Hit;
	}

	FImpactVisualChoice Choose(EImpactOutcome Outcome, const USkillDataBase *Skill, ESpellElement Element,
							   const UImpactVisualDefaults *Defaults, const UCosmeticsData *DefenderCosmetics, bool bIsProjectileArrival)
	{
		// Layer 3: the defence outcome is the defender's cosmetic slot.
		switch (Outcome)
		{
		case EImpactOutcome::PerfectGuard:
			return Make(TSoftObjectPtr<UNiagaraSystem>(), EImpactVisualSource::PerfectGuardFlash);
		case EImpactOutcome::Guard:
			return DefenderCosmetics ? Make(DefenderCosmetics->GuardSparkVFX, EImpactVisualSource::GuardSpark) : FImpactVisualChoice();
		case EImpactOutcome::Dodge:
			return DefenderCosmetics ? Make(DefenderCosmetics->DodgeWhiffVFX, EImpactVisualSource::DodgeWhiff) : FImpactVisualChoice();
		case EImpactOutcome::Hit:
		case EImpactOutcome::Crit:
			break;
		}

		// Layer 2: the impact is on the skill.
		if (bIsProjectileArrival)
		{
			return Make(TSoftObjectPtr<UNiagaraSystem>(), EImpactVisualSource::ProjectileArrival);
		}
		const bool bCrit = Outcome == EImpactOutcome::Crit;
		const float CritScale = bCrit ? (Defaults ? Defaults->CritImpactVFXScale : FALLBACK_CRIT_SCALE) : 1.0f;
		if (Skill)
		{
			const FSkillVFXEntry *ImpactEntry = nullptr;
			for (const FSkillVFXEntry &Entry : Skill->VFXArray)
			{
				if (Entry.VFX.IsNull())
				{
					continue;
				}
				if (bCrit && Entry.Role == EVFXRole::CritImpact)
				{
					return Make(Entry.VFX, EImpactVisualSource::SkillCritEntry);
				}
				if (!ImpactEntry && Entry.Role == EVFXRole::Impact)
				{
					ImpactEntry = &Entry;
				}
			}
			if (ImpactEntry)
			{
				return Make(ImpactEntry->VFX, EImpactVisualSource::SkillEntry, CritScale);
			}
		}
		// The skill has no Impact entry: its element's default, then a generic one.
		if (Defaults)
		{
			if (const TSoftObjectPtr<UNiagaraSystem> *ElementVFX = Defaults->ElementHitVFX.Find(Element); ElementVFX && !ElementVFX->IsNull())
			{
				return Make(*ElementVFX, EImpactVisualSource::ElementDefault, CritScale);
			}
			if (!Defaults->GenericHitVFX.IsNull())
			{
				return Make(Defaults->GenericHitVFX, EImpactVisualSource::GenericDefault, CritScale);
			}
		}
		return FImpactVisualChoice();
	}

	FVector GetSpawnLocation(const AActor *Target)
	{
		if (!Target)
		{
			return FVector::ZeroVector;
		}
		// The character's body mesh, not a weapon mesh that FindComponentByClass may find first (review).
		const ACharacter *Character = Cast<ACharacter>(Target);
		const USkeletalMeshComponent *Mesh = Character ? Character->GetMesh() : Target->FindComponentByClass<USkeletalMeshComponent>();
		if (Mesh && Mesh->DoesSocketExist(HIT_POINT_SOCKET))
		{
			return Mesh->GetSocketLocation(HIT_POINT_SOCKET);
		}
		return Target->GetActorLocation();
	}

	void SpawnForImpact(AActor *Target, EImpactOutcome Outcome, const USkillDataBase *Skill, ESpellElement Element, bool bIsProjectileArrival)
	{
		if (!IsValid(Target)) // runs after the hit, which can kill the target
		{
			return;
		}
		const FImpactVisualChoice Choice = Choose(Outcome, Skill, Element, UImpactVisualDefaults::Get(), GetCosmetics(Target), bIsProjectileArrival);
		if (CVarImpactVFXLog.GetValueOnGameThread())
		{
			UE_LOG(LogWoR, Display, TEXT("[ImpactVFX] %s on %s"), *GetChoiceString(Outcome, Choice), *GetNameSafe(Target));
		}
		if (!Choice.ShouldSpawn())
		{
			return;
		}
		// Small assets, loaded on first use (as the Perfect Guard flash). TODO: preload with the fight's cosmetics if it hitches.
		if (UNiagaraSystem *System = Choice.VFX.LoadSynchronous())
		{
			const FVector Location = GetSpawnLocation(Target);
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(Target, System, Location, FRotator::ZeroRotator, FVector(Choice.Scale));
			// B-231c: the same burst on the clients (the impact resolves on the server only).
			if (UCombatPresentationComponent *Presentation = UCombatPresentationComponent::Find(Target))
			{
				FCosmeticVFX ForClients;
				ForClients.System = System;
				ForClients.Location = Location;
				ForClients.Scale = Choice.Scale;
				Presentation->SendVFX(ForClients);
			}
		}
	}

	FString GetChoiceString(EImpactOutcome Outcome, const FImpactVisualChoice &Choice)
	{
		return FString::Printf(TEXT("%s: %s %s x%.2f"), OutcomeName(Outcome), SourceName(Choice.Source),
							   Choice.VFX.IsNull() ? TEXT("(nothing)") : *Choice.VFX.GetAssetName(), Choice.Scale);
	}
}
