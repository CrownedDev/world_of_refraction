// FieldMovementFlight.cpp
// B-214d (FieldAbilities choice 4, Crown 2026-10-27): UFieldMovementComponent's flight. Toggled by the double tap;
// EP each second (hovering costs the same: it is still flying), a ceiling above the ground it took off from, and
// Zone.NoFly volumes (any volume with that actor tag) that refuse it and bring a flyer down. FFieldFlightLimits lifts
// each limit. Earth's magnetism and Fire's Phoenix Rising are looks only (later). Server side; bFlying replicates so
// the owning client flies too.

#include "Field/FieldMovementComponent.h"

#include "Character/CharacterDataComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Volume.h"
#include "TimerManager.h"
#include "WoRLog.h"

namespace FieldFlightLog
{
	const TCHAR *const TAG = TEXT("[FieldFlight]");
}

bool UFieldMovementComponent::IsInNoFlyZone() const
{
	const UWorld *World = GetWorld();
	const AActor *Owner = GetOwner();
	if (!World || !Owner)
	{
		return false;
	}
	const FName NoFlyTag(FieldSkillRules::NO_FLY_ZONE_TAG);
	const FVector Location = Owner->GetActorLocation();
	for (TActorIterator<AVolume> It(World); It; ++It)
	{
		if (It->ActorHasTag(NoFlyTag) && It->EncompassesPoint(Location))
		{
			return true;
		}
	}
	return false;
}

EFieldSkillRefusal UFieldMovementComponent::ToggleFlight()
{
	ACharacter *Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent *Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement || !HasAuthority())
	{
		return EFieldSkillRefusal::NoMovement; // a client uses ActivateFieldSkill (its Server RPC)
	}
	if (bFlying)
	{
		StopFlight();
		return EFieldSkillRefusal::None;
	}
	EFieldSkillRefusal Refusal = CheckActiveChoiceStillPickable(EFieldMovementType::Flight);
	if (Refusal == EFieldSkillRefusal::None)
	{
		Refusal = FieldSkillRules::CheckFlightStart(IsInNoFlyZone(), GetEPForCostCheck(FieldSkillRules::FLIGHT_EP_PER_SECOND),
													GetEffectiveFlightLimits());
	}
	if (Refusal != EFieldSkillRefusal::None)
	{
		UE_LOG(LogWoR, Log, TEXT("%s %s: flight refused (%s)"), FieldFlightLog::TAG, *GetNameSafe(Character),
			   *FieldSkillRules::RefusalName(Refusal));
		return Refusal;
	}

	// The ceiling counts from the ground under the take-off (the actor's own height when none is in reach).
	const FVector Start = Character->GetActorLocation();
	FHitResult Ground;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FieldFlightGround), false, Character);
	const bool bGround = GetWorld()->LineTraceSingleByChannel(
		Ground, Start, Start - FVector(0.0f, 0.0f, FieldSkillRules::FLIGHT_GROUND_TRACE_DISTANCE), ECC_Visibility, Params);
	FlightTakeOffGroundZ = bGround ? Ground.ImpactPoint.Z : Start.Z;

	SetGliding(false);
	bFlying = true;
	ApplyFlightMode();
	DrainFlight(); // the first second is paid up front, as Enhanced Sprint's
	if (bFlying)
	{
		Character->LaunchCharacter(FVector(0.0f, 0.0f, FieldSkillRules::FLIGHT_TAKEOFF_LIFT_SPEED), false, true);
		GetWorld()->GetTimerManager().SetTimer(FlightDrainTimer, this, &UFieldMovementComponent::DrainFlight,
											   FieldSkillRules::FLIGHT_DRAIN_INTERVAL, true);
		UpdateMoveStepTimer();
		UE_LOG(LogWoR, Log, TEXT("%s %s: flight on (ceiling %s)"), FieldFlightLog::TAG, *GetNameSafe(Character),
			   GetEffectiveFlightLimits().bNoCeiling ? TEXT("none") : *FString::Printf(TEXT("%.0f"), FieldSkillRules::GetFlightCeilingZ(FlightTakeOffGroundZ, GetEffectiveFlightLimits())));
	}
	return EFieldSkillRefusal::None;
}

void UFieldMovementComponent::DrainFlight()
{
	if (!bFlying || GetEffectiveFlightLimits().bUnlimitedEP)
	{
		return;
	}
	if (CheckActiveChoiceStillPickable(EFieldMovementType::Flight) != EFieldSkillRefusal::None)
	{
		UE_LOG(LogWoR, Log, TEXT("%s %s: flight lost its source"), FieldFlightLog::TAG, *GetNameSafe(GetOwner()));
		StopFlight();
		return;
	}
	if (!SpendFieldEP(FieldSkillRules::FLIGHT_EP_PER_SECOND))
	{
		UE_LOG(LogWoR, Log, TEXT("%s %s: flight out of EP"), FieldFlightLog::TAG, *GetNameSafe(GetOwner()));
		StopFlight();
	}
}

void UFieldMovementComponent::ApplyFlightStep()
{
	if (!bFlying)
	{
		return;
	}
	const ACharacter *Flyer = Cast<ACharacter>(GetOwner());
	if (Flyer && Flyer->GetCharacterMovement() && Flyer->GetCharacterMovement()->MovementMode != MOVE_Flying)
	{
		// Something else took the movement (water, a script): flight ends rather than draining for nothing.
		UE_LOG(LogWoR, Log, TEXT("%s %s: no longer in flying mode"), FieldFlightLog::TAG, *GetNameSafe(GetOwner()));
		StopFlight();
		return;
	}
	const FFieldFlightLimits Limits = GetEffectiveFlightLimits(); // B-214h: effects' and the evolution's
	if (!Limits.bIgnoreNoFly && IsInNoFlyZone())
	{
		UE_LOG(LogWoR, Log, TEXT("%s %s: entered a no-fly zone"), FieldFlightLog::TAG, *GetNameSafe(GetOwner()));
		StopFlight();
		return;
	}
	AActor *Owner = GetOwner();
	ACharacter *Character = Cast<ACharacter>(Owner);
	UCharacterMovementComponent *Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement || Limits.bNoCeiling)
	{
		return;
	}
	const double CeilingZ = FieldSkillRules::GetFlightCeilingZ(FlightTakeOffGroundZ, Limits);
	FVector Location = Owner->GetActorLocation();
	if (Location.Z < CeilingZ)
	{
		return;
	}
	// At the ceiling the rise stops (the owning client's own move may overshoot a little); only well over it is the
	// pawn put back, so a co-op guest isn't pulled back every step.
	Movement->Velocity.Z = FMath::Min<FVector::FReal>(Movement->Velocity.Z, 0.0);
	if (Location.Z > CeilingZ + FieldSkillRules::FLIGHT_CEILING_SNAP_MARGIN)
	{
		Location.Z = CeilingZ;
		Owner->SetActorLocation(Location, true);
	}
}

void UFieldMovementComponent::AddFlightVerticalInput(float Value)
{
	APawn *Pawn = Cast<APawn>(GetOwner());
	if (bFlying && Pawn && !FMath::IsNearlyZero(Value))
	{
		Pawn->AddMovementInput(FVector::UpVector, Value);
	}
}

void UFieldMovementComponent::NudgeFlightVertical(float Direction)
{
	ACharacter *Character = Cast<ACharacter>(GetOwner());
	if (bFlying && Character && HasAuthority())
	{
		Character->LaunchCharacter(FVector(0.0f, 0.0f, FMath::Sign(Direction) * FieldSkillRules::FLIGHT_VERTICAL_NUDGE_SPEED), false, true);
	}
}

void UFieldMovementComponent::SetFlightLimits(const FFieldFlightLimits &NewLimits)
{
	FlightLimits = NewLimits;
	UE_LOG(LogWoR, Log, TEXT("%s %s: flight limits: EP %s, ceiling %s (+%.0f), no-fly %s"), FieldFlightLog::TAG,
		   *GetNameSafe(GetOwner()), NewLimits.bUnlimitedEP ? TEXT("unlimited") : TEXT("drains"),
		   NewLimits.bNoCeiling ? TEXT("none") : TEXT("on"), NewLimits.ExtraCeiling,
		   NewLimits.bIgnoreNoFly ? TEXT("ignored") : TEXT("on"));
}

void UFieldMovementComponent::StopFlight()
{
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlightDrainTimer);
	}
	if (!bFlying)
	{
		return;
	}
	bFlying = false;
	ApplyFlightMode();
	UpdateMoveStepTimer();
	UE_LOG(LogWoR, Log, TEXT("%s %s: flight off"), FieldFlightLog::TAG, *GetNameSafe(GetOwner()));
}

void UFieldMovementComponent::OnRep_Flying()
{
	ApplyFlightMode();
}

void UFieldMovementComponent::ApplyFlightMode()
{
	const ACharacter *Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent *Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement || !bHasBaseMovement)
	{
		return;
	}
	if (bFlying)
	{
		Movement->MaxFlySpeed = FieldSkillRules::FLIGHT_SPEED;
		Movement->BrakingDecelerationFlying = FieldSkillRules::FLIGHT_HOVER_BRAKING; // still: hover in place
		Movement->SetMovementMode(MOVE_Flying);
		return;
	}
	Movement->MaxFlySpeed = BaseMaxFlySpeed;
	Movement->BrakingDecelerationFlying = BaseBrakingFlying;
	if (Movement->MovementMode == MOVE_Flying)
	{
		Movement->SetMovementMode(MOVE_Falling);
	}
}
