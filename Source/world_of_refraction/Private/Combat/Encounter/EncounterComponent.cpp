// EncounterComponent.cpp

#include "Combat/Encounter/EncounterComponent.h"

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Infusion/ElementColors.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Skills/Definitions/ESpellElement.h"
#include "TimerManager.h"
#include "Trial/TrialRunSubsystem.h"

namespace EncounterConstants
{
	// /Engine/BasicShapes/Sphere is 100 uu in diameter.
	constexpr float ENGINE_SPHERE_MESH_RADIUS = 50.0f;
}

UEncounterComponent::UEncounterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEncounterComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor *Owner = GetOwner();
	if (!Owner || !Owner->GetRootComponent())
	{
		UE_LOG(LogTemp, Warning, TEXT("[EncounterComponent] Owner has no root component - no trigger created"));
		return;
	}

	// Runtime-created trigger; collision profile mirrors ATrialDoor's
	// pawn-only QueryOnly box.
	USphereComponent *Trigger = NewObject<USphereComponent>(Owner, TEXT("EncounterTrigger"));
	Trigger->SetupAttachment(Owner->GetRootComponent());
	Trigger->SetSphereRadius(TriggerRadius);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->RegisterComponent();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &UEncounterComponent::HandleOverlap);
	Sphere = Trigger;
}

void UEncounterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(JoinWindowTimer);
	}
	if (ArenaSphere.IsValid())
	{
		ArenaSphere->Destroy();
	}
	Super::EndPlay(EndPlayReason);
}

bool UEncounterComponent::IsPlayerCombatant(const AActor *Actor)
{
	// Combatant test is the component, never the pawn class. AI-controlled
	// combatants are excluded — an AI pawn wandering in must not open (or join
	// the player side of) combat.
	const UCharacterDataComponent *CharComp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	return CharComp && CharComp->CharacterData && !CharComp->CharacterData->ShouldUseAI();
}

void UEncounterComponent::HandleOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor,
										UPrimitiveComponent *OtherComp, int32 OtherBodyIndex,
										bool bFromSweep, const FHitResult &SweepResult)
{
	if (bTriggered || !OtherActor || OtherActor == GetOwner() || !IsPlayerCombatant(OtherActor))
	{
		return;
	}

	bTriggered = true;
	PendingLocalParty.Reset();
	PendingLocalParty.Add(OtherActor);

	const FVector ArenaCenter = (OtherActor->GetActorLocation() + GetOwner()->GetActorLocation()) * 0.5f;
	SpawnArenaSphere(ArenaCenter);

	GetWorld()->GetTimerManager().SetTimer(JoinWindowTimer, this, &UEncounterComponent::OnJoinWindowExpired,
										   FMath::Max(JoinWindowSeconds, KINDA_SMALL_NUMBER), false);

	UE_LOG(LogTemp, Log, TEXT("[EncounterComponent] %s engaged by %s - join window %.1fs open at %s"),
		   *GetOwner()->GetName(), *OtherActor->GetName(), JoinWindowSeconds, *ArenaCenter.ToCompactString());
}

void UEncounterComponent::SpawnArenaSphere(const FVector &Center)
{
	AStaticMeshActor *Arena = GetWorld()->SpawnActor<AStaticMeshActor>(Center, FRotator::ZeroRotator);
	if (!Arena)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EncounterComponent] ArenaSphere spawn failed - encounter proceeds without arena visual"));
		return;
	}

	UStaticMeshComponent *Mesh = Arena->GetStaticMeshComponent();
	Mesh->SetMobility(EComponentMobility::Movable);
	if (UStaticMesh *SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
	{
		Mesh->SetStaticMesh(SphereMesh);
	}
	Arena->SetActorScale3D(FVector(ArenaSphereMaxRadius / EncounterConstants::ENGINE_SPHERE_MESH_RADIUS));

	// Overlap-only: join detection during the window. The swap destroys it.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Mesh->SetGenerateOverlapEvents(true);
	Mesh->OnComponentBeginOverlap.AddDynamic(this, &UEncounterComponent::HandleJoin);

	// Authored translucent material preferred; fallback = engine material MID
	// tinted Void (opaque - dome only visible from outside, see header).
	if (UMaterialInterface *AuthoredMaterial = ArenaSphereMaterial.LoadSynchronous())
	{
		Mesh->SetMaterial(0, AuthoredMaterial);
	}
	else if (UMaterialInstanceDynamic *ArenaMID = Mesh->CreateDynamicMaterialInstance(0))
	{
		ArenaMID->SetVectorParameterValue(TEXT("Color"), ElementColors::GetColorForElement(ESpellElement::Void));
	}

	ArenaSphere = Arena;
}

void UEncounterComponent::HandleJoin(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor,
									 UPrimitiveComponent *OtherComp, int32 OtherBodyIndex,
									 bool bFromSweep, const FHitResult &SweepResult)
{
	if (!OtherActor || OtherActor == GetOwner() || PendingLocalParty.Contains(OtherActor) || !IsPlayerCombatant(OtherActor))
	{
		return;
	}

	PendingLocalParty.Add(OtherActor);

	if (ArenaSphere.IsValid())
	{
		const float ShrunkRadius = FMath::Max(ArenaSphereMinRadius,
											  ArenaSphereMaxRadius - ArenaSphereShrinkPerCombatant * (PendingLocalParty.Num() - 1));
		ArenaSphere->SetActorScale3D(FVector(ShrunkRadius / EncounterConstants::ENGINE_SPHERE_MESH_RADIUS));
	}

	UE_LOG(LogTemp, Log, TEXT("[EncounterComponent] %s joined the encounter (%d pending)"),
		   *OtherActor->GetName(), PendingLocalParty.Num());
}

void UEncounterComponent::OnJoinWindowExpired()
{
	// Roster extraction: authored CharacterData assets only (see header ⚠️).
	TArray<UCharacterData *> LocalPartyData;
	for (const TWeakObjectPtr<AActor> &Pending : PendingLocalParty)
	{
		const UCharacterDataComponent *CharComp =
			Pending.IsValid() ? Pending->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (CharComp && CharComp->CharacterData)
		{
			LocalPartyData.Add(CharComp->CharacterData);
		}
	}

	const UCharacterDataComponent *OwnerCharComp = GetOwner()->FindComponentByClass<UCharacterDataComponent>();
	UCharacterData *OwnerData = OwnerCharComp ? OwnerCharComp->CharacterData : nullptr;

	UTrialRunSubsystem *TrialRun = GetWorld()->GetGameInstance()->GetSubsystem<UTrialRunSubsystem>();
	UTrialData *ActiveTrial = TrialRun ? TrialRun->GetActiveTrial() : nullptr;

	if (LocalPartyData.Num() == 0 || !OwnerData || !ActiveTrial)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EncounterComponent] %s - encounter aborted (%s) - re-arming"),
			   *GetOwner()->GetName(),
			   !ActiveTrial	   ? TEXT("no active trial on TrialRunSubsystem")
			   : !OwnerData	   ? TEXT("owner has no CharacterData")
							   : TEXT("no pending pawns survived the join window"));
		RearmEncounter();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[EncounterComponent] Join window closed - handing off %d v 1 to battle stage (difficulty %s)"),
		   LocalPartyData.Num(), *UEnum::GetValueAsString(Difficulty));

	// EnterEncounter OpenLevels to the trial's battle stage; this component,
	// the arena, and every trial actor die with the swap. If it rejects
	// (no EncounterLevel authored), re-arm so the trial stays playable.
	TrialRun->EnterEncounter(ActiveTrial, LocalPartyData, {OwnerData}, Difficulty);
	if (!TrialRun->HasPendingEncounter())
	{
		RearmEncounter();
	}
}

void UEncounterComponent::RearmEncounter()
{
	if (ArenaSphere.IsValid())
	{
		ArenaSphere->Destroy();
	}
	PendingLocalParty.Reset();
	bTriggered = false;
}
