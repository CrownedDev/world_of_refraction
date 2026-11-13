// PartyInteractionComponent.cpp

#include "Party/PartyInteractionComponent.h"

#include "GameFramework/PlayerController.h"
#include "Player/WoRPlayerState.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/WoRInteract.h"
#include "Trial/TrialRunSubsystem.h"

namespace PartyInteractionConstants
{
	constexpr float DEFAULT_INTERACTION_RADIUS = 200.0f;
	constexpr float DEFAULT_VIEW_CONE_THRESHOLD = 0.0f; // "anywhere in front"
}

UPartyInteractionComponent::UPartyInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	InteractionRadius = PartyInteractionConstants::DEFAULT_INTERACTION_RADIUS;
	ViewConeThreshold = PartyInteractionConstants::DEFAULT_VIEW_CONE_THRESHOLD;
}

void UPartyInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	USceneComponent *AttachRoot = GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
	if (!AttachRoot)
	{
		UE_LOG(LogTemp, Warning, TEXT("PartyInteractionComponent on '%s': owner has no root scene component — no trigger created."),
			   GetOwner() ? *GetOwner()->GetName() : TEXT("<null>"));
		return;
	}

	// Runtime-created rather than CreateDefaultSubobject: this component may be
	// added to an already-constructed actor (BP panel or AddComponent node), where
	// no constructor-time attach point exists. Collision profile mirrors the
	// merchant/door triggers; explicit SetGenerateOverlapEvents follows the
	// EncounterComponent precedent for NewObject-created shapes.
	TriggerSphere = NewObject<USphereComponent>(GetOwner(), TEXT("PartyInteractionTrigger"));
	TriggerSphere->SetSphereRadius(InteractionRadius);
	TriggerSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerSphere->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerSphere->SetGenerateOverlapEvents(true);
	TriggerSphere->SetupAttachment(AttachRoot);
	TriggerSphere->RegisterComponent();

	TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &UPartyInteractionComponent::OnTriggerBeginOverlap);
	TriggerSphere->OnComponentEndOverlap.AddDynamic(this, &UPartyInteractionComponent::OnTriggerEndOverlap);
}

void UPartyInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	WoRInteract::HideHint(*this);

	if (TriggerSphere)
	{
		TriggerSphere->OnComponentBeginOverlap.RemoveDynamic(this, &UPartyInteractionComponent::OnTriggerBeginOverlap);
		TriggerSphere->OnComponentEndOverlap.RemoveDynamic(this, &UPartyInteractionComponent::OnTriggerEndOverlap);
		TriggerSphere->DestroyComponent();
		TriggerSphere = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UPartyInteractionComponent::Interact()
{
	// B-232e: a client's party is the host's copy; the host runs the invite.
	const UWorld *World = GetWorld();
	if (World && World->GetNetMode() == NM_Client)
	{
		const APlayerController *PC = World->GetFirstPlayerController();
		if (AWoRPlayerState *Player = PC ? PC->GetPlayerState<AWoRPlayerState>() : nullptr)
		{
			Player->ServerRecruit(GetOwner());
			UE_LOG(LogTemp, Log, TEXT("[PartyInteraction] %s: recruit request sent to the host"), *GetNameSafe(GetOwner()));
		}
		return;
	}
	UCharacterData *OwnerData = GetOwnerCharacterData();
	UPartySessionSubsystem *PartySession = GetPartySession();
	if (!OwnerData || !PartySession)
	{
		UE_LOG(LogTemp, Warning, TEXT("PartyInteractionComponent on '%s': Interact with %s — ignored."),
			   GetOwner() ? *GetOwner()->GetName() : TEXT("<null>"),
			   !OwnerData ? TEXT("no owner CharacterData") : TEXT("no PartySessionSubsystem"));
		return;
	}

	const int32 SlotIndex = PartySession->GetMemberSlotByData(OwnerData);
	if (SlotIndex == INDEX_NONE)
	{
		// Same TSoftClassPtr-from-path construction the encounter trigger uses. B-075: the member is
		// this pawn's own CharacterData, not its class default, so the dismiss lookup above finds it.
		// B-076 (D-026): a character found during a trial joins for that run only.
		const UTrialRunSubsystem *TrialRun = GetWorld()->GetGameInstance()->GetSubsystem<UTrialRunSubsystem>();
		const TSoftClassPtr<ACombatCharacter> PawnClass(GetOwner()->GetClass()->GetPathName());
		const bool bInvited = TrialRun && TrialRun->IsRunActive() ? PartySession->InviteRunMember(PawnClass, OwnerData)
																  : PartySession->InviteMemberWithData(PawnClass, OwnerData);
		UE_LOG(LogTemp, Log, TEXT("[PartyInteraction] %s: invite %s"),
			   *OwnerData->Name, bInvited ? TEXT("accepted") : TEXT("rejected"));
	}
	else
	{
		// Slot-0 leader protection is the subsystem's — a refusal logs there.
		const bool bDismissed = PartySession->DismissMember(SlotIndex);
		UE_LOG(LogTemp, Log, TEXT("[PartyInteraction] %s: dismiss from slot %d %s"),
			   *OwnerData->Name, SlotIndex, bDismissed ? TEXT("done") : TEXT("refused"));
	}

	// Refresh the hint in place — the membership verb just flipped.
	if (PawnInRange.IsValid())
	{
		WoRInteract::ShowHint(*this, FColor::Green, GetHintText());
	}
}

UPartyInteractionComponent *UPartyInteractionComponent::FindNearestInRange(APawn *Pawn)
{
	if (!Pawn)
	{
		return nullptr;
	}

	// Overlap-query, mirroring the merchant/door statics: range IS the trigger
	// sphere, so no distance parameter. No class filter on the query — the
	// component, not the owner class, is the marker.
	TArray<AActor *> Overlapping;
	Pawn->GetOverlappingActors(Overlapping);

	UPartyInteractionComponent *Nearest = nullptr;
	float NearestDistSquared = TNumericLimits<float>::Max();
	const FVector PawnLocation = Pawn->GetActorLocation();
	for (AActor *Actor : Overlapping)
	{
		UPartyInteractionComponent *Component = Actor->FindComponentByClass<UPartyInteractionComponent>();
		if (!Component || !IsWithinViewCone(Pawn, Actor, Component->ViewConeThreshold))
		{
			continue;
		}

		const float DistSquared = FVector::DistSquared(PawnLocation, Actor->GetActorLocation());
		if (DistSquared < NearestDistSquared)
		{
			NearestDistSquared = DistSquared;
			Nearest = Component;
		}
	}
	return Nearest;
}

bool UPartyInteractionComponent::IsWithinViewCone(const APawn *QueryingPawn, const AActor *Target, float MinDotProduct)
{
	if (!QueryingPawn || !Target)
	{
		return false;
	}

	// Camera view where a controller provides one; pawn transform otherwise.
	FVector ViewLocation = QueryingPawn->GetActorLocation();
	FRotator ViewRotation = QueryingPawn->GetActorRotation();
	if (const AController *Controller = QueryingPawn->GetController())
	{
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}

	const FVector ToTarget = (Target->GetActorLocation() - ViewLocation).GetSafeNormal();
	return FVector::DotProduct(ViewRotation.Vector(), ToTarget) >= MinDotProduct;
}

void UPartyInteractionComponent::OnTriggerBeginOverlap(UPrimitiveComponent * /*OverlappedComponent*/, AActor *OtherActor,
													   UPrimitiveComponent * /*OtherComp*/, int32 /*OtherBodyIndex*/,
													   bool /*bFromSweep*/, const FHitResult & /*SweepResult*/)
{
	APawn *Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	PawnInRange = Pawn;
	OnAvailabilityChanged.Broadcast(true);
	WoRInteract::ShowHint(*this, FColor::Green, GetHintText());
}

void UPartyInteractionComponent::OnTriggerEndOverlap(UPrimitiveComponent * /*OverlappedComponent*/, AActor *OtherActor,
													 UPrimitiveComponent * /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	if (OtherActor != PawnInRange.Get())
	{
		return;
	}

	PawnInRange = nullptr;
	OnAvailabilityChanged.Broadcast(false);
	WoRInteract::HideHint(*this);
}

UCharacterData *UPartyInteractionComponent::GetOwnerCharacterData() const
{
	const UCharacterDataComponent *DataComponent =
		GetOwner() ? GetOwner()->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	return DataComponent ? DataComponent->CharacterData : nullptr;
}

UPartySessionSubsystem *UPartyInteractionComponent::GetPartySession() const
{
	const UWorld *World = GetWorld();
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
}

FInteractPrompt UPartyInteractionComponent::GetPrompt() const
{
	UCharacterData *OwnerData = GetOwnerCharacterData();
	if (!OwnerData)
	{
		return FInteractPrompt();
	}
	const UPartySessionSubsystem *PartySession = GetPartySession();
	const bool bIsMember = PartySession && PartySession->GetMemberSlotByData(OwnerData) != INDEX_NONE;
	return FInteractPrompt(bIsMember ? EInteractVerb::Dismiss : EInteractVerb::Invite, FText::FromString(OwnerData->Name));
}

FString UPartyInteractionComponent::GetHintText() const
{
	UCharacterData *OwnerData = GetOwnerCharacterData();
	if (!OwnerData)
	{
		return FString::Printf(TEXT("%s (no CharacterData — cannot join a party)"),
							   GetOwner() ? *GetOwner()->GetName() : TEXT("<null>"));
	}

	const UPartySessionSubsystem *PartySession = GetPartySession();
	const bool bIsMember = PartySession && PartySession->GetMemberSlotByData(OwnerData) != INDEX_NONE;
	return FString::Printf(TEXT("Press E to %s %s"),
						   bIsMember ? TEXT("dismiss") : TEXT("invite"),
						   *OwnerData->Name);
}

bool UPartyInteractionComponent::IsWithinReach(const APawn *Pawn) const
{
	return Pawn && TriggerSphere && TriggerSphere->IsOverlappingActor(Pawn);
}

bool UPartyInteractionComponent::InviteForClient()
{
	UCharacterData *OwnerData = GetOwnerCharacterData();
	UPartySessionSubsystem *PartySession = GetPartySession();
	const UWorld *World = GetWorld();
	const UTrialRunSubsystem *TrialRun = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	if (!OwnerData || !PartySession || PartySession->GetMemberSlotByData(OwnerData) != INDEX_NONE)
	{
		UE_LOG(LogTemp, Log, TEXT("[PartyInteraction] A client's recruit of %s refused (already in the party, or no party)"), *GetNameSafe(GetOwner()));
		return false;
	}
	// A friend recruits only in a run (a found character, D-026); the hub's party is the host's to build.
	if (!TrialRun || !TrialRun->IsRunActive())
	{
		UE_LOG(LogTemp, Log, TEXT("[PartyInteraction] A client's recruit of %s refused: not in a run"), *OwnerData->Name);
		return false;
	}
	const TSoftClassPtr<ACombatCharacter> PawnClass(GetOwner()->GetClass()->GetPathName());
	const bool bInvited = PartySession->InviteRunMember(PawnClass, OwnerData);
	UE_LOG(LogTemp, Log, TEXT("[PartyInteraction] %s: a client's recruit %s"), *OwnerData->Name, bInvited ? TEXT("accepted") : TEXT("rejected"));
	return bInvited;
}
