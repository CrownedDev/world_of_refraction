// FieldMovementAdvanced.cpp
// B-214e (Design/FieldAbilities.md): UFieldMovementComponent's advanced moves, unlocked at rank 7 (B-214b).
//   Teleport (Void only, by the unlock rule): along the aim, stopping short of a wall. TODO: the long-vanish VFX.
//   Phase step: a short step ahead through whatever is in between (thin walls, bars), refused with no room there.
//   Mark and recall: the first use marks the spot (free), the next snaps back to it (paid) and clears the mark.
//   Super launch: a blast straight up, then a free glide from the top (FieldMovementMoves' step).
//   Regroup: to the farthest other party member on the map, behind it; never in a fight.
// Server side; a client reaches them through ActivateFieldSkill.

#include "Field/FieldMovementComponent.h"

#include "Character/CharacterDataComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Party/PartySessionSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "WoRLog.h"

namespace FieldAdvancedLog
{
	const TCHAR *const TAG = TEXT("[FieldAdvanced]");
}

bool UFieldMovementComponent::IsFightRunning() const
{
	const UWorld *World = GetWorld();
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	return PartySession && PartySession->IsFightRunning();
}

EFieldSkillRefusal UFieldMovementComponent::UseAdvancedMove(EFieldMovementType Type)
{
	ACharacter *Character = Cast<ACharacter>(GetOwner());
	if (!Character || !Character->GetCharacterMovement() || !HasAuthority())
	{
		return EFieldSkillRefusal::NoMovement; // a client uses ActivateFieldSkill (its Server RPC)
	}
	EFieldSkillRefusal Refusal = CheckActiveChoiceStillPickable(Type);
	const bool bPlacingMark = Type == EFieldMovementType::MarkAndRecall && !bHasRecallMark;
	if (Refusal == EFieldSkillRefusal::None && !bPlacingMark)
	{
		const double *LastUse = LastAdvancedUseTime.Find(Type);
		const double SinceLastUse = LastUse ? GetWorld()->GetTimeSeconds() - *LastUse : TNumericLimits<double>::Max();
		const UCharacterDataComponent *CharComp = GetCharacterData();
		Refusal = FieldSkillRules::CheckAdvancedMove(Type, CharComp ? CharComp->CurrentEP : 0, SinceLastUse, IsFightRunning());
	}
	if (Refusal == EFieldSkillRefusal::None)
	{
		if (bPlacingMark)
		{
			bHasRecallMark = true;
			RecallMark = Character->GetActorLocation();
			UE_LOG(LogWoR, Log, TEXT("%s %s: recall mark placed at %s"), FieldAdvancedLog::TAG, *GetNameSafe(Character),
				   *RecallMark.ToCompactString());
			return EFieldSkillRefusal::None;
		}
		switch (Type)
		{
		case EFieldMovementType::Teleport:
			Refusal = DoTeleport(*Character);
			break;
		case EFieldMovementType::PhaseStep:
			Refusal = DoPhaseStep(*Character);
			break;
		case EFieldMovementType::MarkAndRecall:
			Refusal = DoRecall(*Character);
			break;
		case EFieldMovementType::SuperLaunch:
			Refusal = DoSuperLaunch(*Character);
			break;
		case EFieldMovementType::Regroup:
			Refusal = DoRegroup(*Character);
			break;
		default:
			Refusal = EFieldSkillRefusal::NotActiveSkill;
			break;
		}
	}
	if (Refusal != EFieldSkillRefusal::None)
	{
		UE_LOG(LogWoR, Log, TEXT("%s %s: %s refused (%s)"), FieldAdvancedLog::TAG, *GetNameSafe(Character),
			   *FieldSkillRules::TypeName(Type), *FieldSkillRules::RefusalName(Refusal));
		return Refusal;
	}
	// Paid only once it happened (a blocked step costs nothing).
	SpendFieldEP(FieldSkillRules::GetAdvancedMoveEPCost(Type));
	LastAdvancedUseTime.Add(Type, GetWorld()->GetTimeSeconds());
	UE_LOG(LogWoR, Log, TEXT("%s %s: %s"), FieldAdvancedLog::TAG, *GetNameSafe(Character), *FieldSkillRules::TypeName(Type));
	return EFieldSkillRefusal::None;
}

EFieldSkillRefusal UFieldMovementComponent::DoTeleport(ACharacter &Character)
{
	// Level with the ground: a third-person camera looks a little down, which would sweep into the floor at once.
	const FVector Aim = (Character.GetController() ? Character.GetControlRotation().Vector() : Character.GetActorForwardVector()).GetSafeNormal2D();
	const FVector Start = Character.GetActorLocation();
	Character.SetActorLocation(Start + Aim * FieldSkillRules::TELEPORT_DISTANCE, true); // swept: stops short of a wall
	if (FVector::Dist(Start, Character.GetActorLocation()) < FieldSkillRules::TELEPORT_MIN_DISTANCE)
	{
		Character.SetActorLocation(Start); // a wall right ahead: no move, not paid
		return EFieldSkillRefusal::Blocked;
	}
	return EFieldSkillRefusal::None;
}

EFieldSkillRefusal UFieldMovementComponent::DoPhaseStep(ACharacter &Character)
{
	const FVector Ahead = Character.GetActorForwardVector().GetSafeNormal2D();
	const FVector Destination = Character.GetActorLocation() + Ahead * FieldSkillRules::PHASE_STEP_DISTANCE;
	// TeleportTo checks the destination (not the way there), so a thin wall or bars in between don't matter.
	return Character.TeleportTo(Destination, Character.GetActorRotation()) ? EFieldSkillRefusal::None : EFieldSkillRefusal::Blocked;
}

EFieldSkillRefusal UFieldMovementComponent::DoRecall(ACharacter &Character)
{
	if (!Character.TeleportTo(RecallMark, Character.GetActorRotation()))
	{
		return EFieldSkillRefusal::Blocked;
	}
	bHasRecallMark = false;
	Character.GetCharacterMovement()->Velocity = FVector::ZeroVector;
	return EFieldSkillRefusal::None;
}

EFieldSkillRefusal UFieldMovementComponent::DoSuperLaunch(ACharacter &Character)
{
	UCharacterMovementComponent *Movement = Character.GetCharacterMovement();
	if (Movement->MovementMode == MOVE_None)
	{
		return EFieldSkillRefusal::Blocked; // nothing moves the pawn with no movement mode
	}
	StopFlight();
	SetGliding(false);
	// As a jump does, not LaunchCharacter: a pending launch only lands on the pawn's next move, so a pawn already
	// falling would start the glide before it rose.
	Movement->Velocity.Z = FieldSkillRules::SUPER_LAUNCH_SPEED;
	Movement->SetMovementMode(MOVE_Falling);
	bSuperLaunchGlidePending = true;
	UpdateMoveStepTimer();
	return EFieldSkillRefusal::None;
}

AActor *UFieldMovementComponent::FindRegroupTarget() const
{
	const AActor *Owner = GetOwner();
	AActor *Farthest = nullptr;
	double FarthestDistSq = -1.0;
	for (TObjectIterator<UFieldMovementComponent> It; It; ++It)
	{
		AActor *Other = It->GetOwner();
		if (It->IsTemplate() || It->GetWorld() != GetWorld() || !Other || Other == Owner)
		{
			continue;
		}
		const APawn *OtherPawn = Cast<APawn>(Other);
		const UCharacterDataComponent *OtherData = Other->FindComponentByClass<UCharacterDataComponent>();
		if (!(OtherPawn && OtherPawn->IsPlayerControlled()) && !(OtherData && OtherData->PartyMemberId.IsValid()))
		{
			continue; // not a party member's pawn
		}
		const double DistSq = FVector::DistSquared(Owner->GetActorLocation(), Other->GetActorLocation());
		if (DistSq > FarthestDistSq)
		{
			FarthestDistSq = DistSq;
			Farthest = Other;
		}
	}
	return Farthest;
}

EFieldSkillRefusal UFieldMovementComponent::DoRegroup(ACharacter &Character)
{
	const AActor *Target = FindRegroupTarget();
	if (!Target)
	{
		return EFieldSkillRefusal::NoTarget;
	}
	const FVector Behind = Target->GetActorLocation() - Target->GetActorForwardVector().GetSafeNormal2D() * FieldSkillRules::REGROUP_OFFSET;
	if (!Character.TeleportTo(Behind, Target->GetActorRotation()))
	{
		return EFieldSkillRefusal::Blocked;
	}
	Character.GetCharacterMovement()->Velocity = FVector::ZeroVector;
	return EFieldSkillRefusal::None;
}
