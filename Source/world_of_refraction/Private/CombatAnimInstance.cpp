// CombatAnimInstance.cpp

#include "CombatAnimInstance.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "StanceData.h"
#include "GameFramework/Character.h"

UCombatAnimInstance::UCombatAnimInstance()
{
}

void UCombatAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    CacheReferences();
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