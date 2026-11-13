// FieldMovementComponent.cpp

#include "Field/FieldMovementComponent.h"

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/ECharacterClass.h"
#include "Combat/WoRFightSubsystem.h"
#include "Engine/World.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Field/FieldSkillUnlockSubsystem.h"
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

namespace
{
	/** B-214b: has the account unlocked Type for Element? bScanStore first records the store's spells (a pick reads
	 *  ranks reached since the last scan). No unlock subsystem: only what needs no unlock. */
	bool IsFieldTypeUnlocked(const UObject *Context, EFieldMovementType Type, ESpellElement Element, bool bScanStore)
	{
		UFieldSkillUnlockSubsystem *Unlocks = WoRFight::Get<UFieldSkillUnlockSubsystem>(Context);
		if (!Unlocks)
		{
			return !FieldSkillRules::NeedsUnlock(Type);
		}
		if (bScanStore)
		{
			Unlocks->RecordFromPool();
		}
		return Unlocks->IsUnlocked(Type, Element);
	}
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
	DOREPLIFETIME(UFieldMovementComponent, bGliding);
	DOREPLIFETIME(UFieldMovementComponent, bWallRunning);
}

void UFieldMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	if (ACharacter *Character = Cast<ACharacter>(GetOwner()))
	{
		if (const UCharacterMovementComponent *Movement = Character->GetCharacterMovement())
		{
			BaseGravityScale = Movement->GravityScale;
			BaseAirControl = Movement->AirControl;
			bHasBaseMovement = true;
		}
		Character->LandedDelegate.AddUniqueDynamic(this, &UFieldMovementComponent::HandleLanded); // B-214c
	}
}

void UFieldMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		// A pawn that outlives this component gets its gravity and air control back.
		SetGliding(false);
		SetWallRunning(false);
	}
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DrainTimer);
		World->GetTimerManager().ClearTimer(MoveStepTimer);
	}
	if (ACharacter *Character = Cast<ACharacter>(GetOwner()))
	{
		Character->LandedDelegate.RemoveDynamic(this, &UFieldMovementComponent::HandleLanded);
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
	const ESpellElement SourceElement = GetSourceElement(Source);
	const EFieldSkillRefusal Refusal = FieldSkillRules::CheckPick(Type, SourceElement,
																  IsFieldTypeUnlocked(this, Type, SourceElement, true));
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
	if (Choice != GetActiveFieldSkill())
	{
		if (bEnhancedSprintActive)
		{
			StopEnhancedSprint();
		}
		SetGliding(false); // B-214c
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
	// The client stamps its own cooldown (no RPC spam) and launches too, so its movement doesn't snap back while the
	// server's launch of the same pawn arrives; the server's launch is the real one.
	LastDashTime = GetWorld()->GetTimeSeconds();
	FVector Direction = Character->GetVelocity().GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = Character->GetActorForwardVector().GetSafeNormal2D();
	}
	Character->LaunchCharacter(Direction * FieldSkillRules::DASH_LAUNCH_SPEED, true, false);
	if (!HasAuthority())
	{
		ServerDash();
		return EFieldSkillRefusal::None;
	}
	UE_LOG(LogWoR, Verbose, TEXT("%s %s: dash"), FieldMovementLog::TAG, *GetNameSafe(Character));
	return EFieldSkillRefusal::None;
}

void UFieldMovementComponent::ServerDash_Implementation()
{
	Dash();
}

EFieldSkillRefusal UFieldMovementComponent::ActivateFieldSkill()
{
	const FFieldSkillChoice Choice = GetActiveFieldSkill();
	if (!HasAuthority())
	{
		if (Choice.Type == EFieldMovementType::Dash)
		{
			return Dash(); // launches here too, then asks the server
		}
		ServerActivateFieldSkill();
		return EFieldSkillRefusal::None;
	}
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
	case EFieldMovementType::Glide: // B-214c
		return ToggleGlide();
	case EFieldMovementType::HighJump:
		return HighJump();
	case EFieldMovementType::AirDash:
		return AirDash();
	default:
		UE_LOG(LogWoR, Log, TEXT("%s %s: %s isn't built yet (B-214d..e)"), FieldMovementLog::TAG, *GetNameSafe(GetOwner()),
			   *FieldSkillRules::TypeName(Choice.Type));
		return EFieldSkillRefusal::NotBuiltYet;
	}
}

void UFieldMovementComponent::ServerActivateFieldSkill_Implementation()
{
	ActivateFieldSkill();
}

EFieldSkillRefusal UFieldMovementComponent::CheckActiveChoiceStillPickable(EFieldMovementType Type) const
{
	const FFieldSkillChoice Choice = GetActiveFieldSkill();
	if (Choice.Type != Type)
	{
		return EFieldSkillRefusal::NotActiveSkill;
	}
	const ESpellElement Element = GetSourceElement(Choice.Source);
	return FieldSkillRules::CheckPick(Choice.Type, Element, IsFieldTypeUnlocked(this, Choice.Type, Element, false));
}

EFieldSkillRefusal UFieldMovementComponent::StartEnhancedSprint()
{
	UCharacterDataComponent *CharComp = GetCharacterData();
	const ESpellElement Element = GetSourceElement(GetActiveFieldSkill().Source);
	const EFieldSkillRefusal Refusal = FieldSkillRules::CheckEnhancedSprintStart(
		Element, CharComp ? CharComp->CurrentEP : 0, IsFieldTypeUnlocked(this, EFieldMovementType::EnhancedSprint, Element, false));
	if (Refusal != EFieldSkillRefusal::None)
	{
		UE_LOG(LogWoR, Log, TEXT("%s %s: Enhanced Sprint refused (%s, %s, EP %d)"), FieldMovementLog::TAG,
			   *GetNameSafe(GetOwner()), *FieldSkillRules::RefusalName(Refusal),
			   *StaticEnum<ESpellElement>()->GetNameStringByValue(static_cast<int64>(Element)), CharComp ? CharComp->CurrentEP : 0);
		return Refusal;
	}
	EnhancedSprintElement = Element;
	SetEnhancedSprintActive(true);
	DrainEnhancedSprint(); // started mid-sprint, the first second is paid up front, so tapping it on and off is never free
	if (bEnhancedSprintActive)
	{
		GetWorld()->GetTimerManager().SetTimer(DrainTimer, this, &UFieldMovementComponent::DrainEnhancedSprint,
											   FieldSkillRules::ENHANCED_SPRINT_DRAIN_INTERVAL, true);
		UpdateMoveStepTimer(); // B-214c: the wall-run check
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
	if (GetSourceElement(GetActiveFieldSkill().Source) != EnhancedSprintElement)
	{
		// A gear change or a broken crystal took the element it started with.
		UE_LOG(LogWoR, Log, TEXT("%s %s: Enhanced Sprint lost its source element"), FieldMovementLog::TAG, *GetNameSafe(GetOwner()));
		StopEnhancedSprint();
		return;
	}
	if (!IsMovingAtSprintSpeed())
	{
		return; // Q-214a (Crown): standing, walking or stopping to talk costs nothing; it stays on
	}
	CharComp->ServerSpendEnergy(FieldSkillRules::ENHANCED_SPRINT_EP_PER_SECOND);
	if (CharComp->CurrentEP < FieldSkillRules::ENHANCED_SPRINT_EP_PER_SECOND)
	{
		UE_LOG(LogWoR, Log, TEXT("%s %s: Enhanced Sprint out of EP"), FieldMovementLog::TAG, *GetNameSafe(GetOwner()));
		StopEnhancedSprint();
	}
}

bool UFieldMovementComponent::IsMovingAtSprintSpeed() const
{
	// The real ground speed, not MaxWalkSpeed: on the server a co-op guest's pawn moves at its true speed even if the
	// pawn Blueprint's sprint press only changed the guest's own MaxWalkSpeed.
	const ACharacter *Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent *Movement = Character ? Character->GetCharacterMovement() : nullptr;
	return Movement && FieldSkillRules::IsMovingAtSprintSpeed(Movement->Velocity.Size2D());
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
	SetWallRunning(false); // B-214c: a wall-run is part of Enhanced Sprint
	UpdateMoveStepTimer();
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
	return FString::Printf(TEXT("%s: skill %s %s (%s, pick %s) | sprint %s x%.2f, %s | EP %d/%d | dash %s | record %s"),
						   *GetNameSafe(GetOwner()), *FieldSkillRules::SourceName(Choice.Source),
						   *FieldSkillRules::TypeName(Choice.Type),
						   *StaticEnum<ESpellElement>()->GetNameStringByValue(static_cast<int64>(SourceElement)),
						   *FieldSkillRules::RefusalName(FieldSkillRules::CheckPick(
							   Choice.Type, SourceElement, IsFieldTypeUnlocked(this, Choice.Type, SourceElement, false))),
						   bEnhancedSprintActive ? TEXT("ON") : TEXT("off"), GetSprintSpeedMultiplier(),
						   IsMovingAtSprintSpeed() ? TEXT("moving at sprint speed (draining)") : TEXT("not sprinting (no drain)"),
						   CharComp ? CharComp->CurrentEP : 0, CharComp ? CharComp->MaxEP : 0,
						   IsDashReady() ? TEXT("ready") : TEXT("cooling"), FindRecord() ? TEXT("yes") : TEXT("no"))
		+ FString::Printf(TEXT(" | glide %s | wall-run %s | air jump %s, air dash %s"), // B-214c
						  bGliding ? TEXT("ON") : TEXT("off"), bWallRunning ? TEXT("ON") : TEXT("off"),
						  bAirJumpUsed ? TEXT("used") : TEXT("ready"), bAirDashUsed ? TEXT("used") : TEXT("ready"));
}
