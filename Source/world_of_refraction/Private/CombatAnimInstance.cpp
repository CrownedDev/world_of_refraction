// CombatAnimInstance.cpp

#include "CombatAnimInstance.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "StanceData.h"
#include "WeaponAttackData.h"
#include "LoadoutComponent.h"
#include "GameFramework/Character.h"

UCombatAnimInstance::UCombatAnimInstance()
{
}

void UCombatAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    CacheReferences();

    // Bind to montage notify events
    OnPlayMontageNotifyBegin.AddDynamic(this, &UCombatAnimInstance::HandleMontageNotify);
}

void UCombatAnimInstance::HandleMontageNotify(FName NotifyName, const FBranchingPointNotifyPayload &BranchingPointPayload)
{
    UE_LOG(LogTemp, Log, TEXT("[CombatAnimInstance] Notify: %s"), *NotifyName.ToString());
    OnActionNotify.Broadcast(NotifyName);
}

void UCombatAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!CharacterDataComponent)
    {
        CacheReferences();
    }

    UpdateCombatState();

    // Diagnostic - what's actually playing?
    static int32 LogCounter = 0;
    if (++LogCounter % 60 == 0) // Log every 60 frames (~1s)
    {
        APawn *Owner = TryGetPawnOwner();
        if (Owner)
        {
            FString MontageInfo = TEXT("None");
            UAnimMontage *ActiveMontage = GetCurrentActiveMontage();
            if (ActiveMontage)
            {
                MontageInfo = FString::Printf(TEXT("%s (Pos: %.2f)"),
                                              *ActiveMontage->GetName(),
                                              Montage_GetPosition(ActiveMontage));
            }

            UE_LOG(LogTemp, Display, TEXT("[CombatAnimInstance] %s: Action=%s, Movement=%s, Stance=%s, Active=%s"),
                   *Owner->GetName(),
                   bIsPlayingAction ? TEXT("Y") : TEXT("N"),
                   bIsPlayingMovement ? TEXT("Y") : TEXT("N"),
                   CurrentStanceMontage ? *CurrentStanceMontage->GetName() : TEXT("None"),
                   *MontageInfo);
        }
    }
}
void UCombatAnimInstance::CacheReferences()
{
    APawn *Owner = TryGetPawnOwner();
    if (!Owner)
    {
        return;
    }

    CharacterDataComponent = Owner->FindComponentByClass<UCharacterDataComponent>();
}

UStanceData *UCombatAnimInstance::GetDesiredStance() const
{
    if (!CharacterDataComponent || !CharacterDataComponent->CharacterData)
    {
        return nullptr;
    }

    ULoadoutComponent *LoadoutComp = GetOwningActor() ? GetOwningActor()->FindComponentByClass<ULoadoutComponent>() : nullptr;
    return LoadoutComp ? LoadoutComp->GetCurrentStance() : nullptr;
}

void UCombatAnimInstance::UpdateCombatState()
{
    // Don't interrupt actions
    if (bIsPlayingAction || bIsPlayingMovement)
    {
        return;
    }
    if (!CharacterDataComponent || !CharacterDataComponent->CharacterData)
    {
        bIsArmed = false;
        return;
    }

    UCharacterData *Data = CharacterDataComponent->CharacterData;
    ULoadoutComponent *LoadoutComp = GetOwningActor() ? GetOwningActor()->FindComponentByClass<ULoadoutComponent>() : nullptr;
    bIsArmed = LoadoutComp ? LoadoutComp->IsArmed() : false;

    // Get the stance we should be in
    UStanceData *DesiredStance = GetDesiredStance();

    // Check if stance changed OR if correct montage isn't playing
    bool bStanceChanged = (DesiredStance != LastAppliedStance);
    bool bMontageNotPlaying = (CurrentStanceMontage && !Montage_IsPlaying(CurrentStanceMontage));
    bool bNoMontageButShouldHave = (!CurrentStanceMontage && DesiredStance && DesiredStance->IdleAnimMontage);

    if (bStanceChanged || bMontageNotPlaying || bNoMontageButShouldHave)
    {
        PlayStanceMontage();
        LastAppliedStance = DesiredStance;
    }
}

void UCombatAnimInstance::PlayStanceMontage()
{
    UStanceData *Stance = GetDesiredStance();

    if (!Stance || !Stance->IdleAnimMontage)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatAnimInstance] No stance montage available"));
        StopStanceMontage();
        return;
    }

    // Stop previous stance montage if different
    if (CurrentStanceMontage && CurrentStanceMontage != Stance->IdleAnimMontage)
    {
        Montage_Stop(0.2f, CurrentStanceMontage);
    }

    // Play new stance montage
    CurrentStanceMontage = Stance->IdleAnimMontage;
    Montage_Play(CurrentStanceMontage, 1.0f);

    UE_LOG(LogTemp, Log, TEXT("[CombatAnimInstance] Playing stance montage: %s"),
           *CurrentStanceMontage->GetName());
}

void UCombatAnimInstance::StopStanceMontage()
{
    if (CurrentStanceMontage)
    {
        Montage_Stop(0.2f, CurrentStanceMontage);
        UE_LOG(LogTemp, Log, TEXT("[CombatAnimInstance] Stopped stance montage: %s"),
               *CurrentStanceMontage->GetName());
        CurrentStanceMontage = nullptr;
    }
}

void UCombatAnimInstance::ResumeStanceMontage()
{
    // Called after attack/ability montage finishes
    PlayStanceMontage();
}

void UCombatAnimInstance::PlayActionMontage(UAnimMontage *Montage, float PlayRate)
{
    if (!Montage)
    {
        return;
    }

    // Stop current action
    if (CurrentActionMontage)
    {
        Montage_Stop(0.2f, CurrentActionMontage);
    }

    CurrentActionMontage = Montage;
    bIsPlayingAction = true;

    // Calculate duration accounting for play rate
    float Duration = Montage->GetPlayLength() / FMath::Abs(PlayRate);

    // For reverse playback, start at the end
    float StartPosition = (PlayRate < 0.0f) ? Montage->GetPlayLength() : 0.0f;

    float PlayedDuration = Montage_Play(Montage, PlayRate, EMontagePlayReturnType::MontageLength, StartPosition);

    if (PlayedDuration > 0.f)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatAnimInstance] Playing action montage: %s (%.2fs at %.2fx)"),
               *Montage->GetName(), Duration, PlayRate);

        // CRITICAL: Bind end callback to reset bIsPlayingAction
        FOnMontageEnded EndDelegate;
        EndDelegate.BindUObject(this, &UCombatAnimInstance::OnActionMontageEndedInternal);
        Montage_SetEndDelegate(EndDelegate, Montage);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatAnimInstance] Failed to play action montage: %s"),
               *Montage->GetName());
        bIsPlayingAction = false;
        CurrentActionMontage = nullptr;
    }
}

void UCombatAnimInstance::OnActionMontageEndedInternal(UAnimMontage *Montage, bool bInterrupted)
{
    if (Montage == CurrentActionMontage)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatAnimInstance] Action montage ended: %s (Interrupted: %s)"),
               *Montage->GetName(), bInterrupted ? TEXT("Yes") : TEXT("No"));

        bIsPlayingAction = false;
        CurrentActionMontage = nullptr;

        // Broadcast for external listeners (VFX cleanup, damage finalization, etc.)
        OnActionMontageEnded.Broadcast(Montage, bInterrupted);

        // Resume stance
        ResumeStanceMontage();
    }
}

void UCombatAnimInstance::PlayMovementMontage(UAnimMontage *MovementMontage)
{
    if (!MovementMontage)
    {
        return;
    }

    // Stop stance montage
    if (CurrentStanceMontage && Montage_IsPlaying(CurrentStanceMontage))
    {
        Montage_Stop(0.1f, CurrentStanceMontage);
    }

    bIsPlayingMovement = true;
    CurrentMovementMontage = MovementMontage;

    float Duration = Montage_Play(MovementMontage, 1.0f);
    UE_LOG(LogTemp, Log, TEXT("[CombatAnimInstance] Playing movement montage: %s (%.2fs)"),
           *MovementMontage->GetName(), Duration);
}

void UCombatAnimInstance::StopMovementMontage()
{
    if (bIsPlayingMovement && CurrentMovementMontage)
    {
        Montage_Stop(0.2f, CurrentMovementMontage);
        UE_LOG(LogTemp, Log, TEXT("[CombatAnimInstance] Stopped movement montage: %s"),
               *CurrentMovementMontage->GetName());
    }
    bIsPlayingMovement = false;
    CurrentMovementMontage = nullptr;
}