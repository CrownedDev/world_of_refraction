// TrialDoor.cpp

#include "Trial/TrialDoor.h"
#include "Player/WoRInteract.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Infusion/ElementColors.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Trial/GateScreenWidget.h"
#include "UI/Trial/TrialGateWidget.h"
#include "UObject/ConstructorHelpers.h"

namespace TrialDoorConstants
{
    // Door-shaped reach: wide/deep enough to catch an approach from the front,
    // half the merchant sphere's height (a door is walked INTO, not around).
    const FVector DEFAULT_INTERACTION_EXTENT(200.0f, 200.0f, 100.0f);
}

ATrialDoor::ATrialDoor()
{
    PrimaryActorTick.bCanEverTick = false;

    DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
    RootComponent = DoorMesh;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeFinder.Succeeded())
    {
        DoorMesh->SetStaticMesh(CubeFinder.Object);
    }

    InteractionTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionTrigger"));
    InteractionTrigger->SetupAttachment(RootComponent);
    InteractionTrigger->SetBoxExtent(TrialDoorConstants::DEFAULT_INTERACTION_EXTENT);
    InteractionTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionTrigger->SetCollisionObjectType(ECC_WorldDynamic);
    InteractionTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void ATrialDoor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    WoRInteract::HideHint(*this);
    PawnInRange.Reset();
    Super::EndPlay(EndPlayReason);
}

void ATrialDoor::BeginPlay()
{
    Super::BeginPlay();

    InteractionTrigger->OnComponentBeginOverlap.AddDynamic(this, &ATrialDoor::OnTriggerBeginOverlap);
    InteractionTrigger->OnComponentEndOverlap.AddDynamic(this, &ATrialDoor::OnTriggerEndOverlap);

    // Element tint (T-A2): per-component MID, so the merchant cubes sharing the
    // engine material are unaffected. Parameter is "Color" — BasicShapeMaterial's
    // actual Vector parameter (it has no "BaseColor"). Null Trial = return door,
    // tints Generic.
    const ESpellElement TintElement = Trial ? Trial->Element : ESpellElement::Generic;
    // Capture the slot-0 material BEFORE creating the MID — afterwards GetMaterial(0)
    // returns the MID itself and hides the base material's name in the log.
    UMaterialInterface *BaseMat = DoorMesh->GetMaterial(0);
    UMaterialInstanceDynamic *DoorMID = DoorMesh->CreateDynamicMaterialInstance(0);
    UE_LOG(LogTemp, Warning, TEXT("[TrialDoor] Tint: Element=%s Colour=%s Mat=%s MID=%s"),
           *UEnum::GetValueAsString(TintElement),
           *ElementColors::GetColorForElement(TintElement).ToString(),
           BaseMat ? *BaseMat->GetName() : TEXT("NULL"),
           DoorMID ? TEXT("VALID") : TEXT("NULL"));
    if (DoorMID)
    {
        DoorMID->SetVectorParameterValue(TEXT("Color"), ElementColors::GetColorForElement(TintElement));
    }

    // No null-Trial warning here — null is the designed "Return to Hub" door
    // (see the file header). A hub-side ENTRY door authored without a trial
    // surfaces through its hint text in PIE instead.
}

void ATrialDoor::Interact()
{
    APawn *Pawn = PawnInRange.Get();
    if (!Pawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("TrialDoor '%s': Interact with no pawn in range — ignored."), *GetName());
        return;
    }

    UGameInstance *GI = GetGameInstance();
    UTrialRunSubsystem *TrialRun = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
    if (!TrialRun)
    {
        UE_LOG(LogTemp, Warning, TEXT("TrialDoor '%s': TrialRunSubsystem unavailable — ignored."), *GetName());
        return;
    }

    if (Trial)
    {
        // B-466 (D-192): the Gate screen picks the gate, the trial and the difficulty, and enters. Without a root
        // layout the B-183 placeholder asks instead; no local player (a bot at the door) enters with the defaults.
        APlayerController *PC = Cast<APlayerController>(Pawn->GetController());
        if (!UGateScreenWidget::Open(PC, Trial) && !UTrialGateWidget::Open(PC, Trial))
        {
            TrialRun->SetGateChoice(Trial->Variations.IsEmpty() ? INDEX_NONE : 0, TrialGate::DEFAULT_DIFFICULTY);
            TrialRun->EnterTrial(Trial);
        }
    }
    else
    {
        // Designed null branch: this is the "Return to Hub" door.
        TrialRun->ExitTrial();
    }
}

ATrialDoor *ATrialDoor::FindNearestInRange(APawn *Pawn)
{
    return WoRInteract::FindNearestOverlapping<ATrialDoor>(Pawn); // B-256
}

void ATrialDoor::OnTriggerBeginOverlap(UPrimitiveComponent * /*OverlappedComponent*/, AActor *OtherActor,
                                       UPrimitiveComponent * /*OtherComp*/, int32 /*OtherBodyIndex*/,
                                       bool /*bFromSweep*/, const FHitResult & /*SweepResult*/)
{
    APawn *Pawn = Cast<APawn>(OtherActor);
    if (!Pawn || !Pawn->IsPlayerControlled())
    {
        return;
    }

    PawnInRange = Pawn;
    WoRInteract::ShowHint(*this, FColor::Cyan, GetHintText());
}

void ATrialDoor::OnTriggerEndOverlap(UPrimitiveComponent * /*OverlappedComponent*/, AActor *OtherActor,
                                     UPrimitiveComponent * /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
    if (OtherActor != PawnInRange.Get())
    {
        return;
    }

    PawnInRange = nullptr;
    WoRInteract::HideHint(*this);
}

FString ATrialDoor::GetHintText() const
{
    return Trial
               ? FString::Printf(TEXT("Press E — Enter %s"), *Trial->Name.ToString())
               : FString(TEXT("Press E — Return to Hub"));
}
