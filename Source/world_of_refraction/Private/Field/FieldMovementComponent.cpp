// FieldMovementComponent.cpp

#include "Field/FieldMovementComponent.h"

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/ECharacterClass.h"
#include "Engine/World.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Hub/HubMovementLibrary.h"
#include "Loadout/Entries/FRingLoadoutEntry.h"
#include "Loadout/LoadoutComponent.h"
#include "Net/UnrealNetwork.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "TimerManager.h"
#include "WoRLog.h"

namespace FieldMovementLog
{
	const TCHAR *const TAG = TEXT("[FieldMovement]");
}

UFieldMovementComponent::UFieldMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UFieldMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UFieldMovementComponent, ActiveChoice, COND_OwnerOnly);
	DOREPLIFETIME(UFieldMovementComponent, bEnhancedSprintActive);
	DOREPLIFETIME(UFieldMovementComponent, EnhancedSprintElement);
}

void UFieldMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DrainTimer);
	}
	Super::EndPlay(EndPlayReason);
}

bool UFieldMovementComponent::HasAuthority() const
{
	return GetOwner() && GetOwner()->HasAuthority();
}

UCharacterDataComponent *UFieldMovementComponent::GetCharacterData() const
{
	if (!CachedCharacterData.IsValid() && GetOwner())
	{
		CachedCharacterData = GetOwner()->FindComponentByClass<UCharacterDataComponent>();
	}
	return CachedCharacterData.Get();
}

const FPartyMember *UFieldMovementComponent::FindRecord() const
{
	// B-348b's rule: only a pawn stamped with its member id reads and writes the record.
	const UCharacterDataComponent *CharComp = GetCharacterData();
	return CharComp && CharComp->PartyMemberId.IsValid() ? CharComp->FindCarriedPartyMember() : nullptr;
}

FFieldSkillChoice UFieldMovementComponent::GetActiveFieldSkill() const
{
	if (HasAuthority())
	{
		if (const FPartyMember *Record = FindRecord())
		{
			return Record->FieldSkill;
		}
	}
	return ActiveChoice;
}

void UFieldMovementComponent::RefreshFromRecord()
{
	if (const FPartyMember *Record = HasAuthority() ? FindRecord() : nullptr)
	{
		ActiveChoice = Record->FieldSkill;
	}
}

ESpellElement UFieldMovementComponent::GetSourceElement(EFieldSkillSource Source) const
{
	const ULoadoutComponent *Loadout = GetOwner() ? GetOwner()->FindComponentByClass<ULoadoutComponent>() : nullptr;
	switch (Source)
	{
	case EFieldSkillSource::Innate:
	{
		const UCharacterDataComponent *CharComp = GetCharacterData();
		const UCharacterData *Data = CharComp ? CharComp->CharacterData : nullptr;
		return Data ? Data->GetElement() : ESpellElement::None;
	}
	case EFieldSkillSource::Weapon:
	{
		const FRuntimeAttachedItem *Crystal = Loadout && Loadout->GetActiveWeapon() ? Loadout->GetActiveWeaponAttachment() : nullptr;
		return Crystal && !Crystal->IsEmpty() && !Crystal->IsBroken() ? Crystal->GetElement() : ESpellElement::None;
	}
	case EFieldSkillSource::Ring:
	{
		// As the parry colour reads a character's ring (ParryVisuals OwnMagicElement): a Resonator's active adapter,
		// another class's primary ring. FRingInventoryEntry::GetElement reads a broken crystal as None.
		const UCharacterData *Data = Loadout ? Loadout->GetOwnerCharacterData() : nullptr;
		const FRingLoadoutEntry *Ring = Data && Data->CharacterClass == ECharacterClass::Resonator ? Loadout->GetActiveRingLoadout()
																								: (Loadout ? Loadout->GetPrimaryRingLoadout() : nullptr);
		return Ring ? Ring->RingEntry.GetElement() : ESpellElement::None;
	}
	}
	return ESpellElement::None;
}

EFieldSkillRefusal UFieldMovementComponent::SetActiveFieldSkill(EFieldSkillSource Source, EFieldMovementType Type)
{
	const EFieldSkillRefusal Refusal = FieldSkillRules::CheckPick(Type, GetSourceElement(Source));
	if (Refusal != EFieldSkillRefusal::None)
	{
		UE_LOG(LogWoR, Log, TEXT("%s %s: %s %s refused (%s)"), FieldMovementLog::TAG, *GetNameSafe(GetOwner()),
			   *FieldSkillRules::SourceName(Source), *FieldSkillRules::TypeName(Type), *FieldSkillRules::RefusalName(Refusal));
		return Refusal;
	}
	if (!HasAuthority())
	{
		ServerSetActiveFieldSkill(Source, Type); // the host owns the record and re-checks the pick
		return EFieldSkillRefusal::None;
	}

	FFieldSkillChoice Choice;
	Choice.Source = Source;
	Choice.Type = Type;
	if (Choice != GetActiveFieldSkill() && bEnhancedSprintActive)
	{
		StopEnhancedSprint();
	}
	ActiveChoice = Choice;
	const UCharacterDataComponent *CharComp = GetCharacterData();
	const UGameInstance *GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	if (PartySession && FindRecord())
	{
		PartySession->SetMemberFieldSkill(PartySession->GetMemberSlotOf(CharComp), Choice);
	}
	UE_LOG(LogWoR, Log, TEXT("%s %s: field skill %s %s"), FieldMovementLog::TAG, *GetNameSafe(GetOwner()),
		   *FieldSkillRules::SourceName(Source), *FieldSkillRules::TypeName(Type));
	return EFieldSkillRefusal::None;
}

void UFieldMovementComponent::ServerSetActiveFieldSkill_Implementation(EFieldSkillSource Source, EFieldMovementType Type)
{
	if (StaticEnum<EFieldSkillSource>()->IsValidEnumValue(static_cast<int64>(Source)) &&
		StaticEnum<EFieldMovementType>()->IsValidEnumValue(static_cast<int64>(Type)))
	{
		SetActiveFieldSkill(Source, Type);
	}
}

bool UFieldMovementComponent::IsDashReady() const
{
	const UWorld *World = GetWorld();
	return World && World->GetTimeSeconds() - LastDashTime >= FieldSkillRules::DASH_COOLDOWN_SECONDS;
}

EFieldSkillRefusal UFieldMovementComponent::Dash()
{
	ACharacter *Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return EFieldSkillRefusal::NoMovement;
	}
	if (!IsDashReady())
	{
		return EFieldSkillRefusal::OnCooldown;
	}
	if (!HasAuthority())
	{
		ServerDash(); // the server moves the pawn; the client follows its correction
		return EFieldSkillRefusal::None;
	}
	LastDashTime = GetWorld()->GetTimeSeconds();
	FVector Direction = Character->GetVelocity().GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = Character->GetActorForwardVector().GetSafeNormal2D();
	}
	Character->LaunchCharacter(Direction * FieldSkillRules::DASH_LAUNCH_SPEED, true, false);
	UE_LOG(LogWoR, Verbose, TEXT("%s %s: dash"), FieldMovementLog::TAG, *GetNameSafe(Character));
	return EFieldSkillRefusal::None;
}

void UFieldMovementComponent::ServerDash_Implementation()
{
	Dash();
}

EFieldSkillRefusal UFieldMovementComponent::ActivateFieldSkill()
{
	if (!HasAuthority())
	{
		ServerActivateFieldSkill();
		return EFieldSkillRefusal::None;
	}
	const FFieldSkillChoice Choice = GetActiveFieldSkill();
	switch (Choice.Type)
	{
	case EFieldMovementType::Dash:
		return Dash();
	case EFieldMovementType::EnhancedSprint:
		if (bEnhancedSprintActive)
		{
			StopEnhancedSprint();
			return EFieldSkillRefusal::None;
		}
		return StartEnhancedSprint();
	default:
		UE_LOG(LogWoR, Log, TEXT("%s %s: %s isn't built yet (B-214c..e)"), FieldMovementLog::TAG, *GetNameSafe(GetOwner()),
			   *FieldSkillRules::TypeName(Choice.Type));
		return EFieldSkillRefusal::NotBuiltYet;
	}
}

void UFieldMovementComponent::ServerActivateFieldSkill_Implementation()
{
	ActivateFieldSkill();
}

EFieldSkillRefusal UFieldMovementComponent::StartEnhancedSprint()
{
	UCharacterDataComponent *CharComp = GetCharacterData();
	const ESpellElement Element = GetSourceElement(GetActiveFieldSkill().Source);
	const EFieldSkillRefusal Refusal = FieldSkillRules::CheckEnhancedSprintStart(Element, CharComp ? CharComp->CurrentEP : 0);
	if (Refusal != EFieldSkillRefusal::None)
	{
		UE_LOG(LogWoR, Log, TEXT("%s %s: Enhanced Sprint refused (%s, %s, EP %d)"), FieldMovementLog::TAG,
			   *GetNameSafe(GetOwner()), *FieldSkillRules::RefusalName(Refusal),
			   *StaticEnum<ESpellElement>()->GetNameStringByValue(static_cast<int64>(Element)), CharComp ? CharComp->CurrentEP : 0);
		return Refusal;
	}
	EnhancedSprintElement = Element;
	SetEnhancedSprintActive(true);
	DrainEnhancedSprint(); // the first second is paid up front, so tapping it on and off is never free
	if (bEnhancedSprintActive)
	{
		GetWorld()->GetTimerManager().SetTimer(DrainTimer, this, &UFieldMovementComponent::DrainEnhancedSprint,
											   FieldSkillRules::ENHANCED_SPRINT_DRAIN_INTERVAL, true);
	}
	return EFieldSkillRefusal::None;
}

void UFieldMovementComponent::DrainEnhancedSprint()
{
	UCharacterDataComponent *CharComp = GetCharacterData();
	if (!bEnhancedSprintActive || !CharComp)
	{
		StopEnhancedSprint();
		return;
	}
	if (CharComp->CurrentEP < FieldSkillRules::ENHANCED_SPRINT_EP_PER_SECOND)
	{
		StopEnhancedSprint();
		return;
	}
	CharComp->ServerSpendEnergy(FieldSkillRules::ENHANCED_SPRINT_EP_PER_SECOND);
	if (CharComp->CurrentEP < FieldSkillRules::ENHANCED_SPRINT_EP_PER_SECOND)
	{
		UE_LOG(LogWoR, Log, TEXT("%s %s: Enhanced Sprint out of EP"), FieldMovementLog::TAG, *GetNameSafe(GetOwner()));
		StopEnhancedSprint();
	}
}

void UFieldMovementComponent::StopEnhancedSprint()
{
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DrainTimer);
	}
	if (bEnhancedSprintActive)
	{
		SetEnhancedSprintActive(false);
	}
}

void UFieldMovementComponent::SetEnhancedSprintActive(bool bActive)
{
	bEnhancedSprintActive = bActive;
	UE_LOG(LogWoR, Log, TEXT("%s %s: Enhanced Sprint %s"), FieldMovementLog::TAG, *GetNameSafe(GetOwner()),
		   bActive ? TEXT("on") : TEXT("off"));
	OnRep_EnhancedSprintActive(); // the server applies it as the client's OnRep does
}

void UFieldMovementComponent::OnRep_EnhancedSprintActive()
{
	ReapplySprintSpeed();
	OnEnhancedSprintChanged.Broadcast(bEnhancedSprintActive);
}

float UFieldMovementComponent::GetSprintSpeedMultiplier() const
{
	return bEnhancedSprintActive ? FieldSkillRules::GetEnhancedSprintMultiplier(EnhancedSprintElement) : FieldSkillRules::NO_SPRINT_BOOST;
}

void UFieldMovementComponent::ReapplySprintSpeed() const
{
	ACharacter *Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent *Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (Movement && Movement->MaxWalkSpeed >= UHubMovementLibrary::GetSprintSpeedMin())
	{
		Movement->MaxWalkSpeed = UHubMovementLibrary::ComputeHubSprintSpeed(Character);
	}
}

FString UFieldMovementComponent::GetFieldStateString() const
{
	const FFieldSkillChoice Choice = GetActiveFieldSkill();
	const UCharacterDataComponent *CharComp = GetCharacterData();
	const ESpellElement SourceElement = GetSourceElement(Choice.Source);
	return FString::Printf(TEXT("%s: skill %s %s (%s, pick %s) | sprint %s x%.2f | EP %d/%d | dash %s | record %s"),
						   *GetNameSafe(GetOwner()), *FieldSkillRules::SourceName(Choice.Source),
						   *FieldSkillRules::TypeName(Choice.Type),
						   *StaticEnum<ESpellElement>()->GetNameStringByValue(static_cast<int64>(SourceElement)),
						   *FieldSkillRules::RefusalName(FieldSkillRules::CheckPick(Choice.Type, SourceElement)),
						   bEnhancedSprintActive ? TEXT("ON") : TEXT("off"), GetSprintSpeedMultiplier(),
						   CharComp ? CharComp->CurrentEP : 0, CharComp ? CharComp->MaxEP : 0,
						   IsDashReady() ? TEXT("ready") : TEXT("cooling"), FindRecord() ? TEXT("yes") : TEXT("no"));
}
