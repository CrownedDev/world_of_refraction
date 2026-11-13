// FieldMovementMoves.cpp
// B-214c: UFieldMovementComponent's ground and air moves (glide, high / double jump, air dash, wall-run). Server side:
// a client reaches them through ActivateFieldSkill (its Server RPC); the glide / wall-run state replicates so the
// owning client's gravity matches.

#include "Field/FieldMovementComponent.h"

#include "Character/CharacterDataComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "WoRLog.h"

namespace FieldMovesLog
{
	const TCHAR *const TAG = TEXT("[FieldMoves]");
}

namespace FieldMovesHelpers
{
	UCharacterMovementComponent *MovementOf(const AActor *Owner)
	{
		const ACharacter *Character = Cast<ACharacter>(Owner);
		return Character ? Character->GetCharacterMovement() : nullptr;
	}

	/** The aim: the controller's view (the camera), else the pawn's facing. */
	FVector AimOf(const ACharacter *Character)
	{
		return Character->GetController() ? Character->GetControlRotation().Vector() : Character->GetActorForwardVector();
	}
}

bool UFieldMovementComponent::SpendFieldEP(int32 Amount)
{
	UCharacterDataComponent *CharComp = GetCharacterData();
	if (!CharComp || CharComp->CurrentEP < Amount)
	{
		return false;
	}
	CharComp->ServerSpendEnergy(Amount);
	return true;
}

EFieldSkillRefusal UFieldMovementComponent::ToggleGlide()
{
	const UCharacterMovementComponent *Movement = FieldMovesHelpers::MovementOf(GetOwner());
	if (!Movement || !HasAuthority())
	{
		return EFieldSkillRefusal::NoMovement; // a client uses ActivateFieldSkill (its Server RPC)
	}
	if (bGliding)
	{
		SetGliding(false);
		return EFieldSkillRefusal::None;
	}
	EFieldSkillRefusal Refusal = CheckActiveChoiceStillPickable(EFieldMovementType::Glide);
	if (Refusal == EFieldSkillRefusal::None)
	{
		const UCharacterDataComponent *CharComp = GetCharacterData();
		Refusal = FieldSkillRules::CheckGlideStart(Movement->IsFalling(), CharComp ? CharComp->CurrentEP : 0);
	}
	if (Refusal != EFieldSkillRefusal::None)
	{
		UE_LOG(LogWoR, Log, TEXT("%s %s: glide refused (%s)"), FieldMovesLog::TAG, *GetNameSafe(GetOwner()),
			   *FieldSkillRules::RefusalName(Refusal));
		return Refusal;
	}
	SpendFieldEP(FieldSkillRules::GLIDE_EP_COST);
	SetGliding(true);
	StepMoves(); // the fall is capped at once
	return EFieldSkillRefusal::None;
}

EFieldSkillRefusal UFieldMovementComponent::HighJump()
{
	ACharacter *Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent *Movement = FieldMovesHelpers::MovementOf(Character);
	if (!Movement || !HasAuthority())
	{
		return EFieldSkillRefusal::NoMovement;
	}
	const bool bOnGround = Movement->IsMovingOnGround();
	EFieldSkillRefusal Refusal = CheckActiveChoiceStillPickable(EFieldMovementType::HighJump);
	if (Refusal == EFieldSkillRefusal::None)
	{
		const UCharacterDataComponent *CharComp = GetCharacterData();
		Refusal = FieldSkillRules::CheckJump(bOnGround, bAirJumpUsed, CharComp ? CharComp->CurrentEP : 0);
	}
	if (Refusal != EFieldSkillRefusal::None)
	{
		UE_LOG(LogWoR, Log, TEXT("%s %s: jump refused (%s)"), FieldMovesLog::TAG, *GetNameSafe(Character),
			   *FieldSkillRules::RefusalName(Refusal));
		return Refusal;
	}
	SpendFieldEP(FieldSkillRules::HIGH_JUMP_EP_COST);
	bAirJumpUsed = bAirJumpUsed || !bOnGround;
	SetGliding(false);
	const float Speed = bOnGround ? FieldSkillRules::HIGH_JUMP_SPEED : FieldSkillRules::DOUBLE_JUMP_SPEED;
	Character->LaunchCharacter(FVector(0.0f, 0.0f, Speed), false, true);
	UE_LOG(LogWoR, Log, TEXT("%s %s: %s"), FieldMovesLog::TAG, *GetNameSafe(Character),
		   bOnGround ? TEXT("high jump") : TEXT("double jump"));
	return EFieldSkillRefusal::None;
}

EFieldSkillRefusal UFieldMovementComponent::AirDash()
{
	ACharacter *Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent *Movement = FieldMovesHelpers::MovementOf(Character);
	if (!Movement || !HasAuthority())
	{
		return EFieldSkillRefusal::NoMovement;
	}
	EFieldSkillRefusal Refusal = CheckActiveChoiceStillPickable(EFieldMovementType::AirDash);
	if (Refusal == EFieldSkillRefusal::None)
	{
		const UCharacterDataComponent *CharComp = GetCharacterData();
		Refusal = FieldSkillRules::CheckAirDash(Movement->IsFalling(), bAirDashUsed, CharComp ? CharComp->CurrentEP : 0);
	}
	if (Refusal != EFieldSkillRefusal::None)
	{
		UE_LOG(LogWoR, Log, TEXT("%s %s: air dash refused (%s)"), FieldMovesLog::TAG, *GetNameSafe(Character),
			   *FieldSkillRules::RefusalName(Refusal));
		return Refusal;
	}
	SpendFieldEP(FieldSkillRules::AIR_DASH_EP_COST);
	bAirDashUsed = true;
	const FVector Aim = FieldMovesHelpers::AimOf(Character).GetSafeNormal();
	if (FieldSkillRules::IsBlinkAirDash(GetSourceElement(GetActiveFieldSkill().Source)))
	{
		// Lightning: short swept blinks (stopping at a wall), then a hover: no fall speed carried. TODO: VFX per blink.
		int32 Blinks = 0;
		for (; Blinks < FieldSkillRules::AIR_DASH_BLINK_COUNT; ++Blinks)
		{
			FHitResult Hit;
			Character->SetActorLocation(Character->GetActorLocation() + Aim * FieldSkillRules::AIR_DASH_BLINK_DISTANCE, true, &Hit);
			if (Hit.bBlockingHit)
			{
				++Blinks;
				break;
			}
		}
		Movement->Velocity = FVector::ZeroVector;
		UE_LOG(LogWoR, Log, TEXT("%s %s: air dash, %d blink(s)"), FieldMovesLog::TAG, *GetNameSafe(Character), Blinks);
		return EFieldSkillRefusal::None;
	}
	Character->LaunchCharacter(Aim * FieldSkillRules::AIR_DASH_SPEED, true, true);
	UE_LOG(LogWoR, Log, TEXT("%s %s: air dash"), FieldMovesLog::TAG, *GetNameSafe(Character));
	return EFieldSkillRefusal::None;
}

void UFieldMovementComponent::NotifyLanded()
{
	bAirJumpUsed = false;
	bAirDashUsed = false;
	bSuperLaunchGlidePending = false; // B-214e: landed before the top
	bWallRunSpent = false;
	WallRunSeconds = 0.0f;
	SetGliding(false);
	SetWallRunning(false);
}

void UFieldMovementComponent::HandleLanded(const FHitResult &Hit)
{
	if (HasAuthority())
	{
		NotifyLanded();
	}
}

void UFieldMovementComponent::ApplyWallRunStep(bool bWallBeside, const FVector &WallNormal)
{
	UCharacterMovementComponent *Movement = FieldMovesHelpers::MovementOf(GetOwner());
	if (!Movement)
	{
		return;
	}
	const bool bCan = !bWallRunSpent && FieldSkillRules::CanWallRun(bEnhancedSprintActive, Movement->IsFalling(),
																   Movement->Velocity.Size2D(), bWallBeside, WallRunSeconds);
	if (!bCan)
	{
		if (bWallRunning)
		{
			bWallRunSpent = true; // once per airtime: off the wall, or out of time
			SetWallRunning(false);
		}
		return;
	}
	if (!bWallRunning)
	{
		SetWallRunning(true);
	}
	WallRunSeconds += FieldSkillRules::MOVE_STEP_INTERVAL;
	// Along the wall at the same speed, height held.
	const FVector::FReal Speed = Movement->Velocity.Size2D();
	const FVector Along = FVector::VectorPlaneProject(FVector(Movement->Velocity.X, Movement->Velocity.Y, 0.0f), WallNormal).GetSafeNormal2D();
	Movement->Velocity = Along * Speed;
}

void UFieldMovementComponent::StepMoves()
{
	ACharacter *Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent *Movement = FieldMovesHelpers::MovementOf(Character);
	if (!Movement)
	{
		return;
	}
	if (bFlying)
	{
		ApplyFlightStep(); // B-214d
	}
	if (bSuperLaunchGlidePending && Movement->IsFalling() && Movement->Velocity.Z <= 0.0)
	{
		bSuperLaunchGlidePending = false; // B-214e: the top of the launch: a free glide down
		SetGliding(true);
	}
	if (bGliding && !Movement->IsFalling())
	{
		SetGliding(false); // out of the air without a landing event (water, a teleport, a scripted mode)
	}
	if (bGliding)
	{
		Movement->Velocity.Z = FMath::Max<FVector::FReal>(Movement->Velocity.Z, -FieldSkillRules::GLIDE_MAX_FALL_SPEED);
	}
	if (bEnhancedSprintActive && Movement->IsFalling())
	{
		// A wall to the left or right of the facing.
		const FVector Start = Character->GetActorLocation();
		const FVector Side = Character->GetActorRightVector();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(FieldWallRun), false, Character);
		FHitResult Hit;
		const bool bWall =
			GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Side * FieldSkillRules::WALL_RUN_TRACE_DISTANCE, ECC_Visibility, Params) ||
			GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - Side * FieldSkillRules::WALL_RUN_TRACE_DISTANCE, ECC_Visibility, Params);
		ApplyWallRunStep(bWall, Hit.ImpactNormal);
	}
	else if (bWallRunning)
	{
		ApplyWallRunStep(false, FVector::ZeroVector);
	}
}

void UFieldMovementComponent::UpdateMoveStepTimer()
{
	UWorld *World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}
	FTimerManager &Timers = World->GetTimerManager();
	const bool bNeeded = bGliding || bEnhancedSprintActive || bFlying || bSuperLaunchGlidePending;
	if (bNeeded && !Timers.IsTimerActive(MoveStepTimer))
	{
		Timers.SetTimer(MoveStepTimer, this, &UFieldMovementComponent::StepMoves, FieldSkillRules::MOVE_STEP_INTERVAL, true);
	}
	else if (!bNeeded)
	{
		Timers.ClearTimer(MoveStepTimer);
	}
}

void UFieldMovementComponent::SetGliding(bool bOn)
{
	if (bGliding == bOn)
	{
		return;
	}
	bGliding = bOn;
	UE_LOG(LogWoR, Log, TEXT("%s %s: glide %s"), FieldMovesLog::TAG, *GetNameSafe(GetOwner()), bOn ? TEXT("on") : TEXT("off"));
	ApplyAirMoveState();
	UpdateMoveStepTimer();
}

void UFieldMovementComponent::SetWallRunning(bool bOn)
{
	if (bWallRunning == bOn)
	{
		return;
	}
	bWallRunning = bOn;
	UE_LOG(LogWoR, Log, TEXT("%s %s: wall-run %s"), FieldMovesLog::TAG, *GetNameSafe(GetOwner()), bOn ? TEXT("on") : TEXT("off"));
	if (bOn)
	{
		if (UCharacterMovementComponent *Movement = FieldMovesHelpers::MovementOf(GetOwner()))
		{
			Movement->Velocity.Z = 0.0f; // the height is held from here
		}
	}
	ApplyAirMoveState();
}

void UFieldMovementComponent::OnRep_AirMoveState()
{
	ApplyAirMoveState();
}

void UFieldMovementComponent::ApplyAirMoveState()
{
	UCharacterMovementComponent *Movement = FieldMovesHelpers::MovementOf(GetOwner());
	if (!Movement || !bHasBaseMovement)
	{
		return;
	}
	Movement->GravityScale = bWallRunning ? FieldSkillRules::WALL_RUN_GRAVITY_SCALE
							 : bGliding	  ? FieldSkillRules::GLIDE_GRAVITY_SCALE
										  : BaseGravityScale;
	Movement->AirControl = bGliding ? FieldSkillRules::GLIDE_AIR_CONTROL : BaseAirControl;
}
