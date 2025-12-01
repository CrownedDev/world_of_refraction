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
    // Don't interrupt attacks
    if (bIsPlayingAttack)
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

void UCombatAnimInstance::PlayAttackMontage(UAnimMontage *AttackMontage)
{
    if (!AttackMontage)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatAnimInstance] PlayAttackMontage - null montage"));
        return;
    }

    // Stop stance montage
    if (CurrentStanceMontage && Montage_IsPlaying(CurrentStanceMontage))
    {
        Montage_Stop(0.2f, CurrentStanceMontage);
    }

    // Mark as attacking
    bIsPlayingAttack = true;
    CurrentAttackMontage = AttackMontage;

    // Play attack
    float Duration = Montage_Play(AttackMontage, 1.0f);

    if (Duration > 0.f)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatAnimInstance] Playing attack montage: %s (%.2fs)"),
               *AttackMontage->GetName(), Duration);

        // Set up end delegate
        FOnMontageEnded EndDelegate;
        EndDelegate.BindUObject(this, &UCombatAnimInstance::OnAttackMontageEnded);
        Montage_SetEndDelegate(EndDelegate, AttackMontage);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatAnimInstance] Failed to play attack montage: %s"),
               *AttackMontage->GetName());
        bIsPlayingAttack = false;
        CurrentAttackMontage = nullptr;
    }
}

void UCombatAnimInstance::OnAttackMontageEnded(UAnimMontage *Montage, bool bInterrupted)
{
    if (Montage == CurrentAttackMontage)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatAnimInstance] Attack montage ended: %s (Interrupted: %s)"),
               *Montage->GetName(), bInterrupted ? TEXT("Yes") : TEXT("No"));

        bIsPlayingAttack = false;
        CurrentAttackMontage = nullptr;

        // Resume stance using existing function
        ResumeStanceMontage();
    }
}

void UCombatAnimInstance::DebugPlayCurrentAttack()
{
    if (!CharacterDataComponent || !CharacterDataComponent->CharacterData)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatAnimInstance] DebugPlayCurrentAttack - No CharacterData"));
        return;
    }

    UCharacterData *CharData = CharacterDataComponent->CharacterData;
    UAnimMontage *AttackMontage = CharData->GetCurrentAttackMontage();

    if (!AttackMontage)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatAnimInstance] DebugPlayCurrentAttack - No attack montage (unarmed or no weapon attack)"));
        return;
    }

    PlayAttackMontage(AttackMontage);
}