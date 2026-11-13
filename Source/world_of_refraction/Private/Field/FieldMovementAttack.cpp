// FieldMovementAttack.cpp
// B-214f (Design/FieldAbilities.md; Metaphor: ReFantazio's field strike): UFieldMovementComponent's field attack. A swing
// sweeps a sphere ahead; the first encounter it strikes engages with the party striking first and carries the hit's
// buildup into the fight (UEncounterComponent::StrikeFromField -> ABattleGameMode::ApplyFieldStrike). The source is
// switchable (innate / weapon / ring); the weapon in hand gives the physical type; held with Water it is Ice. Server
// side; a client's calls go by Server RPC. TODO: the swing's animation and hit VFX.

#include "Field/FieldMovementComponent.h"

#include "CollisionQueryParams.h"
#include "Combat/Encounter/EncounterComponent.h"
#include "Engine/World.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "WoRLog.h"

namespace FieldAttackLog
{
	const TCHAR *const TAG = TEXT("[FieldAttack]");
}

EPhysicalDamageType UFieldMovementComponent::GetFieldAttackPhysicalType() const
{
	const ULoadoutComponent *Loadout = GetOwner() ? GetOwner()->FindComponentByClass<ULoadoutComponent>() : nullptr;
	const UWeaponData *Weapon = Loadout ? Loadout->GetActiveWeapon() : nullptr;
	const UAbilityData *Attack = Weapon ? Cast<UAbilityData>(Weapon->WeaponAttack) : nullptr;
	return Attack ? Attack->PhysicalDamageType : EPhysicalDamageType::None;
}

FFieldStrike UFieldMovementComponent::GetFieldStrike(bool bHeld) const
{
	return FieldSkillRules::MakeFieldStrike(GetSourceElement(FieldAttackSource), GetFieldAttackPhysicalType(), bHeld);
}

void UFieldMovementComponent::SwitchFieldAttackSource()
{
	if (!HasAuthority())
	{
		ServerSwitchFieldAttackSource();
		return;
	}
	FieldAttackSource = FieldSkillRules::NextSource(FieldAttackSource);
	UE_LOG(LogWoR, Log, TEXT("%s %s: attack source %s (%s)"), FieldAttackLog::TAG, *GetNameSafe(GetOwner()),
		   *FieldSkillRules::SourceName(FieldAttackSource),
		   *StaticEnum<ESpellElement>()->GetNameStringByValue(static_cast<int64>(GetSourceElement(FieldAttackSource))));
}

void UFieldMovementComponent::ServerSwitchFieldAttackSource_Implementation()
{
	SwitchFieldAttackSource();
}

EFieldSkillRefusal UFieldMovementComponent::FieldAttack(bool bHeld)
{
	AActor *Owner = GetOwner();
	UWorld *World = GetWorld();
	if (!Owner || !World)
	{
		return EFieldSkillRefusal::NoMovement;
	}
	if (World->GetTimeSeconds() - LastFieldAttackTime < FieldSkillRules::FIELD_ATTACK_COOLDOWN_SECONDS)
	{
		return EFieldSkillRefusal::OnCooldown;
	}
	LastFieldAttackTime = World->GetTimeSeconds();
	if (!HasAuthority())
	{
		ServerFieldAttack(bHeld);
		return EFieldSkillRefusal::None;
	}

	const FVector Start = Owner->GetActorLocation();
	const FVector End = Start + Owner->GetActorForwardVector().GetSafeNormal2D() * FieldSkillRules::FIELD_ATTACK_RANGE;
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FieldAttack), false, Owner);
	World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Pawn,
							   FCollisionShape::MakeSphere(FieldSkillRules::FIELD_ATTACK_RADIUS), Params);
	const FFieldStrike Strike = GetFieldStrike(bHeld);
	for (const FHitResult &Hit : Hits)
	{
		AActor *Struck = Hit.GetActor();
		UEncounterComponent *Encounter = Struck ? Struck->FindComponentByClass<UEncounterComponent>() : nullptr;
		if (Encounter && Encounter->StrikeFromField(Owner, Strike))
		{
			UE_LOG(LogWoR, Log, TEXT("%s %s struck %s"), FieldAttackLog::TAG, *GetNameSafe(Owner), *GetNameSafe(Struck));
			return EFieldSkillRefusal::None;
		}
	}
	UE_LOG(LogWoR, Verbose, TEXT("%s %s: a swing, nothing struck"), FieldAttackLog::TAG, *GetNameSafe(Owner));
	return EFieldSkillRefusal::None;
}

void UFieldMovementComponent::ServerFieldAttack_Implementation(bool bHeld)
{
	LastFieldAttackTime = -FieldSkillRules::FIELD_ATTACK_COOLDOWN_SECONDS; // the client already timed it
	FieldAttack(bHeld);
}
