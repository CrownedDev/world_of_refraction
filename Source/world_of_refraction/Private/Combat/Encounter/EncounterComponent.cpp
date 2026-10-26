// EncounterComponent.cpp

#include "Combat/Encounter/EncounterComponent.h"

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Encounter/EncounterData.h"
#include "Character/CombatCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"
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

	// B-184: a team beaten earlier in this run stays beaten when the trial level reloads.
	const UGameInstance *GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UTrialRunSubsystem *TrialRun = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	if (Owner && TrialRun && TrialRun->IsEncounterBeaten(GetEncounterId()))
	{
		UE_LOG(LogTemp, Log, TEXT("[EncounterComponent] %s was beaten earlier this run - removed"), *Owner->GetName());
		Owner->Destroy();
		return;
	}

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

FName UEncounterComponent::GetEncounterId() const
{
	return GetOwner() ? FName(*UWorld::RemovePIEPrefix(GetOwner()->GetPathName())) : NAME_None;
}

TArray<UCharacterData *> UEncounterComponent::GetOwnTeam() const
{
	const UCharacterDataComponent *OwnerCharComp = GetOwner() ? GetOwner()->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	return UEncounterData::BuildOpposingRoster(EncounterData, OwnerCharComp ? OwnerCharComp->CharacterData : nullptr);
}

TArray<UEncounterComponent *> UEncounterComponent::GatherCaughtEncounters() const
{
	TArray<UEncounterComponent *> Caught = {const_cast<UEncounterComponent *>(this)};
	const float CaughtRadiusSquared = FMath::Square(ArenaSphereMaxRadius);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		UEncounterComponent *Other = It->FindComponentByClass<UEncounterComponent>();
		if (Other && Other != this && !Other->bTriggered && IsValid(*It) &&
			FVector::DistSquared(It->GetActorLocation(), ArenaCenter) <= CaughtRadiusSquared)
		{
			Caught.Add(Other);
		}
	}
	return Caught;
}

bool UEncounterComponent::IsAnotherWindowOpen() const
{
	for (TObjectIterator<UEncounterComponent> It; It; ++It)
	{
		if (*It != this && It->bTriggered && It->GetWorld() == GetWorld())
		{
			return true;
		}
	}
	return false;
}

UPartySessionSubsystem *UEncounterComponent::GetPartySession() const
{
	const UGameInstance *GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
}

bool UEncounterComponent::IsPlayerCombatant(const AActor *Actor)
{
	// Combatant test is the component, never the pawn class. Enemy-origin
	// combatants are excluded — an enemy wandering into the sphere must not open
	// (or join the player side of) combat.
	//
	// This is an ORIGIN question, not a control one: an AI companion is a
	// Player-origin character under a bot controller, and it must still be able to
	// join its own party's encounter. A control check here would reject it.
	const UCharacterDataComponent *CharComp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	return CharComp && CharComp->CharacterData && CharComp->CharacterData->IsPlayerOrigin();
}

void UEncounterComponent::HandleOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor,
										UPrimitiveComponent *OtherComp, int32 OtherBodyIndex,
										bool bFromSweep, const FHitResult &SweepResult)
{
	if (bTriggered || !OtherActor || OtherActor == GetOwner() || !IsPlayerCombatant(OtherActor) || IsAnotherWindowOpen())
	{
		return;
	}

	// Trial Party (session-scoped) must exist before we can ask who belongs to it.
	// Created HERE rather than at battle bootstrap: this is where the player is
	// possessing their real character. Leader is PC0 as a session property — NOT
	// the triggering pawn's controller, which is an ACombatAIController when an AI
	// companion trips the sphere.
	UPartySessionSubsystem *PartySession = GetPartySession();
	if (PartySession)
	{
		PartySession->EnsureLocalParty(UGameplayStatics::GetPlayerController(this, 0),
									   TSoftClassPtr<ACombatCharacter>(OtherActor->GetClass()->GetPathName()));
	}

	// Battle Party = the subset of the Trial Party present when the window closes.
	// Anyone else in the sphere is a spectator.
	//
	// ⚠️ POC: the Trial Party was just created FROM this pawn's class, so this test
	// cannot fail yet. It is real code exercising an always-passing path until Arc 2
	// ships hub party assembly and parties can contain classes that did not trigger
	// the encounter.
	if (PartySession && !PartySession->IsTrialPartyMember(OtherActor))
	{
		UE_LOG(LogTemp, Log, TEXT("[EncounterComponent] %s is not in the Trial Party - spectating, not engaging"),
			   *OtherActor->GetName());
		return;
	}

	bTriggered = true;
	PendingBattleParty.Reset();
	PendingBattleParty.Add(OtherActor);

	ArenaCenter = (OtherActor->GetActorLocation() + GetOwner()->GetActorLocation()) * 0.5f;
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
	if (!OtherActor || OtherActor == GetOwner() || PendingBattleParty.Contains(OtherActor) || !IsPlayerCombatant(OtherActor))
	{
		return;
	}

	// Same Trial-Party gate as the trigger: only members join the Battle Party.
	// A Player-origin character who is not on the roster is a spectator.
	const UPartySessionSubsystem *PartySession = GetPartySession();
	if (PartySession && !PartySession->IsTrialPartyMember(OtherActor))
	{
		UE_LOG(LogTemp, Log, TEXT("[EncounterComponent] %s entered the arena but is not in the Trial Party - spectating"),
			   *OtherActor->GetName());
		return;
	}

	PendingBattleParty.Add(OtherActor);

	if (ArenaSphere.IsValid())
	{
		const float ShrunkRadius = FMath::Max(ArenaSphereMinRadius,
											  ArenaSphereMaxRadius - ArenaSphereShrinkPerCombatant * (PendingBattleParty.Num() - 1));
		ArenaSphere->SetActorScale3D(FVector(ShrunkRadius / EncounterConstants::ENGINE_SPHERE_MESH_RADIUS));
	}

	UE_LOG(LogTemp, Log, TEXT("[EncounterComponent] %s joined the encounter (%d pending)"),
		   *OtherActor->GetName(), PendingBattleParty.Num());
}

void UEncounterComponent::OnJoinWindowExpired()
{
	// Roster extraction: the LOCAL side comes from the party, not from who was
	// standing in the arena. Presence was only ever the player themselves — the
	// party is the authored roster and is what the battle must spawn.
	//
	// TODO(multiplayer): remote players still join via the arena
	// overlap window. The LOCAL party is roster-sourced and spawns
	// with the player. Presence-authoritative solo (companion
	// follow-AI) is the deferred alternative -- see banked note.
	//
	// Nothing above this line changed: the trigger, the join window, the arena
	// sphere and PendingBattleParty all still run. They are the remote-join path
	// and must stay warm; this function simply no longer reads them for the local
	// roster. Dead members are filtered — a corpse does not re-enter combat.
	UPartySessionSubsystem *PartySession = GetPartySession();
	const UParty *LocalParty = PartySession ? PartySession->GetLocalParty() : nullptr;
	const TArray<UCharacterData *> BattlePartyData =
		LocalParty ? LocalParty->GetLivingMemberCharacterData() : TArray<UCharacterData *>();

	// B-184: every team inside the arena joins: one fights whole, several send their strongest
	// each. The hardest rank and AI level among them rule the fight; a win beats them all.
	const TArray<UEncounterComponent *> Caught = GatherCaughtEncounters();
	TArray<TArray<UCharacterData *>> CaughtTeams;
	TArray<FName> CaughtIds;
	EAIDifficulty FightDifficulty = Difficulty;
	EEncounterRank FightRank = Rank;
	for (const UEncounterComponent *Encounter : Caught)
	{
		CaughtTeams.Add(Encounter->GetOwnTeam());
		CaughtIds.Add(Encounter->GetEncounterId());
		FightDifficulty = FMath::Max(FightDifficulty, Encounter->Difficulty);
		FightRank = FMath::Max(FightRank, Encounter->Rank);
	}
	const TArray<UCharacterData *> OpposingPartyData = UEncounterData::BuildCaughtRoster(CaughtTeams);

	UTrialRunSubsystem *TrialRun = GetWorld()->GetGameInstance()->GetSubsystem<UTrialRunSubsystem>();
	UTrialData *ActiveTrial = TrialRun ? TrialRun->GetActiveTrial() : nullptr;

	// Party wipe is called out as its own cause: it is a legitimate run state, not a
	// wiring fault, and it must NOT reach EnterEncounter — ABattleGameMode hard-aborts
	// on an empty Battle Party, which would bounce through a level load to get back
	// here. Re-arming leaves the trial playable.
	//
	// TODO: stopgap. Real defeat handling (run ends, return to hub) is banked.
	if (!ActiveTrial || OpposingPartyData.Num() == 0 || !LocalParty || BattlePartyData.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EncounterComponent] %s - encounter aborted (%s) - re-arming"),
			   *GetOwner()->GetName(),
			   !ActiveTrial	   ? TEXT("no active trial on TrialRunSubsystem")
			   : OpposingPartyData.Num() == 0 ? TEXT("no enemy team and the owner has no CharacterData")
			   : !LocalParty   ? TEXT("no local party - the encounter path did not create one")
							   : TEXT("party wiped - no living members"));
		RearmEncounter();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[EncounterComponent] Join window closed - handing off %d v %d to battle stage (difficulty %s, %d team(s) caught)"),
		   BattlePartyData.Num(), OpposingPartyData.Num(), *UEnum::GetValueAsString(FightDifficulty), Caught.Num());

	// EnterEncounter OpenLevels to the trial's battle stage; this component,
	// the arena, and every trial actor die with the swap. If it rejects
	// (no EncounterLevel authored), re-arm so the trial stays playable.
	TrialRun->SetPendingEncounterSources(CaughtIds);
	TrialRun->EnterEncounter(ActiveTrial, BattlePartyData, OpposingPartyData, FightDifficulty, FightRank);
	if (!TrialRun->HasPendingEncounter())
	{
		TrialRun->SetPendingEncounterSources({});
		RearmEncounter();
	}
}

void UEncounterComponent::RearmEncounter()
{
	if (ArenaSphere.IsValid())
	{
		ArenaSphere->Destroy();
	}
	PendingBattleParty.Reset();
	bTriggered = false;
}
