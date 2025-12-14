// CombatAnimInstance.cpp

#include "CombatAnimInstance.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "StanceData.h"
#include "WeaponAttackData.h"
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

    return CharacterDataComponent->CharacterData->GetCurrentStance();
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
    bIsArmed = Data->IsArmed();

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

void UCombatAnimInstance::PlayActionMontage(UAnimMontage *ActionMontage, float PlayRate)
{
    if (!ActionMontage)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatAnimInstance] PlayActionMontage - null montage"));
        return;
    }

    // Stop stance montage
    if (CurrentStanceMontage && Montage_IsPlaying(CurrentStanceMontage))
    {
        Montage_Stop(0.2f, CurrentStanceMontage);
    }

    // Stop any current action
    if (bIsPlayingAction && CurrentActionMontage)
    {
        Montage_Stop(0.1f, CurrentActionMontage);
    }

    bIsPlayingAction = true;
    CurrentActionMontage = ActionMontage;

    float Duration = Montage_Play(ActionMontage, PlayRate);

    if (Duration > 0.f)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatAnimInstance] Playing action montage: %s (%.2fs at %.2fx)"),
               *ActionMontage->GetName(), Duration, PlayRate);

        FOnMontageEnded EndDelegate;
        EndDelegate.BindUObject(this, &UCombatAnimInstance::OnActionMontageEndedInternal);
        Montage_SetEndDelegate(EndDelegate, ActionMontage);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatAnimInstance] Failed to play action montage: %s"),
               *ActionMontage->GetName());
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