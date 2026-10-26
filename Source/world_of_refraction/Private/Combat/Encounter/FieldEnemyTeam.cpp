// FieldEnemyTeam.cpp

#include "Combat/Encounter/FieldEnemyTeam.h"

#include "Combat/Encounter/EncounterComponent.h"
#include "Combat/Encounter/EncounterData.h"
#include "Components/InterpToMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "Trial/TrialRunSubsystem.h"
#include "UObject/ConstructorHelpers.h"

AFieldEnemyTeam::AFieldEnemyTeam()
{
	PrimaryActorTick.bCanEverTick = false;

	TeamMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TeamMesh"));
	RootComponent = TeamMesh;
	TeamMesh->SetMobility(EComponentMobility::Movable);
	// The encounter's trigger sphere does the touching; the placeholder body blocks nothing.
	TeamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderFinder.Succeeded())
	{
		TeamMesh->SetStaticMesh(CylinderFinder.Object);
	}

	Encounter = CreateDefaultSubobject<UEncounterComponent>(TEXT("Encounter"));

	Patrol = CreateDefaultSubobject<UInterpToMovementComponent>(TEXT("Patrol"));
	Patrol->BehaviourType = EInterpToBehaviourType::PingPong;
	Patrol->bAutoActivate = false;
}

void AFieldEnemyTeam::BeginPlay()
{
	Super::BeginPlay();
	// A beaten team is destroyed by its encounter component during BeginPlay.
	if (IsValid(this) && !IsActorBeingDestroyed())
	{
		StartPatrol();
	}
}

void AFieldEnemyTeam::StartPatrol()
{
	if (PatrolPoints.IsEmpty())
	{
		return;
	}
	float PathLength = 0.0f;
	FVector Previous = FVector::ZeroVector;
	Patrol->ResetControlPoints();
	Patrol->AddControlPointPosition(FVector::ZeroVector, true);
	for (const FVector &Point : PatrolPoints)
	{
		Patrol->AddControlPointPosition(Point, true);
		PathLength += FVector::Dist(Previous, Point);
		Previous = Point;
	}
	Patrol->Duration = FMath::Max(PathLength / PatrolSpeed, KINDA_SMALL_NUMBER);
	Patrol->FinaliseControlPoints();
	Patrol->Activate(true);
}

FString AFieldEnemyTeam::GetFieldEnemyTeamString() const
{
	const UGameInstance *GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UTrialRunSubsystem *TrialRun = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	const bool bBeaten = TrialRun && Encounter && TrialRun->IsEncounterBeaten(Encounter->GetEncounterId());
	const int32 TeamSize = Encounter ? Encounter->GetOwnTeam().Num() : 0;
	return FString::Printf(TEXT("[FieldEnemyTeam] %s: %d enemies (%s), %s%s"), *GetName(), TeamSize,
						   Encounter && Encounter->EncounterData ? *Encounter->EncounterData->GetName() : TEXT("no team"),
						   PatrolPoints.IsEmpty() ? TEXT("idle")
												  : *FString::Printf(TEXT("patrols %d point(s)"), PatrolPoints.Num()),
						   bBeaten ? TEXT(", BEATEN") : TEXT(""));
}

#if !UE_BUILD_SHIPPING
namespace
{
	FAutoConsoleCommandWithWorld GPrintFieldEnemyTeamsCmd(
		TEXT("wor.Trial.PrintTeams"),
		TEXT("Print every enemy team on this map: its team, patrol and whether it is beaten this run (B-184)."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				for (TActorIterator<AFieldEnemyTeam> It(World); It; ++It)
				{
					UE_LOG(LogTemp, Display, TEXT("%s"), *It->GetFieldEnemyTeamString());
				}
			}));
}
#endif
