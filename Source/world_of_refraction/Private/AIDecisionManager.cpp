// AIDecisionManager.cpp

#include "AIDecisionManager.h"
#include "AIDecisionConstants.h"
#include "CombatOrchestrator.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "LoadoutComponent.h"
#include "WeaponData.h"
#include "WeaponManager.h"
#include "WeaponAttackData.h"
#include "SpellData.h"
#include "AbilityData.h"
#include "TimerManager.h"
#include "DefenseSystem.h"
#include "EDefenseType.h"
#include "EDefenseDirection.h"

void UAIDecisionManager::Initialize(FSubsystemCollectionBase &Collection)
{
    Super::Initialize(Collection);

    // Note: DefenseSystem may not be ready yet - will lazy-load when needed
    DefenseSystemRef = GetGameInstance()->GetSubsystem<UDefenseSystem>();

    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] Initialized"));
}

void UAIDecisionManager::Deinitialize()
{
    ClearCombatOrchestrator();
    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] Deinitialized"));
    Super::Deinitialize();
}

// ==================== COMBAT REGISTRATION ====================

void UAIDecisionManager::SetCombatOrchestrator(ACombatOrchestrator *Orchestrator)
{
    CurrentCombat = Orchestrator;
    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] Combat orchestrator set"));
}

void UAIDecisionManager::ClearCombatOrchestrator()
{
    if (UWorld *World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ThinkingTimerHandle);
    }
    CurrentCombat = nullptr;
    PendingActor = nullptr;
    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] Combat orchestrator cleared"));
}

// ==================== DECISION MAKING ====================

void UAIDecisionManager::RequestDecision(AActor *AIActor)
{
    if (!AIActor || !CurrentCombat)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIDecisionManager] RequestDecision failed - null actor or combat"));
        return;
    }

    PendingActor = AIActor;

    // Calculate thinking delay
    EAIDifficulty Difficulty = GetCurrentDifficulty();
    float Delay = CalculateThinkingDelay(Difficulty);

    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s thinking for %.2fs (Difficulty: %d)"),
           *AIActor->GetName(), Delay, static_cast<int32>(Difficulty));

    // Schedule decision after delay
    if (UWorld *World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            ThinkingTimerHandle,
            this,
            &UAIDecisionManager::ExecuteDecision,
            Delay,
            false);
    }
}

void UAIDecisionManager::ExecuteDecision()
{
    if (!PendingActor || !CurrentCombat)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIDecisionManager] ExecuteDecision - no pending actor or combat"));
        return;
    }

    // Capture locally BEFORE SubmitAction (which may trigger next turn synchronously)
    AActor *Actor = PendingActor;
    PendingActor = nullptr; // Clear BEFORE submit

    FAction Action = BuildAction(Actor);

    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s executing action type %d"),
           *Actor->GetName(), static_cast<int32>(Action.ActionType));

    // Submit through orchestrator (may advance turn synchronously)
    CurrentCombat->SubmitAction(Action);
}

// ==================== QUERY ====================

EAIDifficulty UAIDecisionManager::GetCurrentDifficulty() const
{
    if (CurrentCombat)
    {
        return CurrentCombat->GetCombatDifficulty();
    }
    return EAIDifficulty::Medium;
}

// ==================== DECISION LOGIC ====================

FAction UAIDecisionManager::BuildAction(AActor *AIActor)
{
    FAction Action;
    Action.ActionType = EActionType::Defend; // Safe default

    if (!AIActor)
    {
        return Action;
    }

    // Get components
    ULoadoutComponent *Loadout = AIActor->FindComponentByClass<ULoadoutComponent>();
    UCharacterDataComponent *CharComp = AIActor->FindComponentByClass<UCharacterDataComponent>();

    if (!Loadout || !CharComp || !CharComp->CharacterData)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIDecisionManager] %s missing Loadout or CharacterData"),
               *AIActor->GetName());
        return Action;
    }

    // Get first living enemy as target
    TArray<AActor *> Enemies = CurrentCombat->GetLivingEnemies(AIActor);
    if (Enemies.Num() == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] No living enemies - defending"));
        return Action;
    }

    AActor *Target = Enemies[0];

    // Choose action type
    EActionType ChosenType = ChooseActionType(AIActor, Loadout);
    Action.ActionType = ChosenType;
    Action.Targets.Add(Target);

    // Populate action data based on type
    switch (ChosenType)
    {
    case EActionType::Attack:
    {
        UWeaponManager *WeaponManager = GetGameInstance()->GetSubsystem<UWeaponManager>();
        UWeaponAttackData *AttackData = WeaponManager ? WeaponManager->GetActiveAttack(AIActor) : nullptr;
        if (AttackData)
        {
            Action.AttackData = AttackData;
            UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s chose Attack: %s"),
                   *AIActor->GetName(), *AttackData->AttackName);
        }
        else
        {
            Action.ActionType = EActionType::Defend;
        }
    }
    break;

    case EActionType::Spell:
    {
        TArray<USpellData *> Spells = Loadout->GetAvailableSpells();
        if (Spells.Num() > 0)
        {
            int32 Index = FMath::RandRange(0, Spells.Num() - 1);
            Action.SpellData = Spells[Index];
            UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s chose Spell: %s"),
                   *AIActor->GetName(), *Spells[Index]->SpellName);
        }
        else
        {
            Action.ActionType = EActionType::Defend;
        }
    }
    break;

    case EActionType::Ability:
    {
        TArray<UAbilityData *> Abilities = Loadout->GetAvailableAbilities();
        if (Abilities.Num() > 0)
        {
            int32 Index = FMath::RandRange(0, Abilities.Num() - 1);
            Action.AbilityData = Abilities[Index];
            UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s chose Ability: %s"),
                   *AIActor->GetName(), *Abilities[Index]->AbilityName);
        }
        else
        {
            Action.ActionType = EActionType::Defend;
        }
    }
    break;

    default:
        break;
    }

    return Action;
}

EActionType UAIDecisionManager::ChooseActionType(AActor *AIActor, ULoadoutComponent *Loadout)
{
    if (!Loadout)
    {
        return EActionType::Defend;
    }

    // Gather available options
    TArray<EActionType> Options;

    UWeaponManager *WeaponManager = GetGameInstance()->GetSubsystem<UWeaponManager>();
    if (WeaponManager && WeaponManager->GetActiveAttack(AIActor))
    {
        Options.Add(EActionType::Attack);
    }

    if (Loadout->GetAvailableSpells().Num() > 0)
    {
        Options.Add(EActionType::Spell);
    }

    if (Loadout->GetAvailableAbilities().Num() > 0)
    {
        Options.Add(EActionType::Ability);
    }

    // Random selection
    if (Options.Num() > 0)
    {
        int32 Index = FMath::RandRange(0, Options.Num() - 1);
        return Options[Index];
    }

    return EActionType::Defend;
}

void UAIDecisionManager::GetThinkingDelayRange(EAIDifficulty Difficulty, float &OutMin, float &OutMax) const
{
    switch (Difficulty)
    {
    case EAIDifficulty::Easy:
        OutMin = AIConstants::EASY_THINK_MIN;
        OutMax = AIConstants::EASY_THINK_MAX;
        break;
    case EAIDifficulty::Medium:
        OutMin = AIConstants::MEDIUM_THINK_MIN;
        OutMax = AIConstants::MEDIUM_THINK_MAX;
        break;
    case EAIDifficulty::Hard:
        OutMin = AIConstants::HARD_THINK_MIN;
        OutMax = AIConstants::HARD_THINK_MAX;
        break;
    case EAIDifficulty::Expert:
        OutMin = AIConstants::EXPERT_THINK_MIN;
        OutMax = AIConstants::EXPERT_THINK_MAX;
        break;
    default:
        OutMin = AIConstants::MEDIUM_THINK_MIN;
        OutMax = AIConstants::MEDIUM_THINK_MAX;
        break;
    }
}

float UAIDecisionManager::CalculateThinkingDelay(EAIDifficulty Difficulty) const
{
    float Min, Max;
    GetThinkingDelayRange(Difficulty, Min, Max);
    return FMath::FRandRange(Min, Max);
}

// ==================== DEFENSE DECISIONS ====================

void UAIDecisionManager::ScheduleDefenseDecision(AActor *Defender, float AttackSize, int32 BaseDamage, float WindowDuration)
{
    if (!Defender)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIDecisionManager] ScheduleDefenseDecision - null Defender"));
        return;
    }

    // Lazy-load DefenseSystem (may not be ready at Initialize time)
    if (!DefenseSystemRef)
    {
        DefenseSystemRef = GetGameInstance()->GetSubsystem<UDefenseSystem>();
    }

    if (!DefenseSystemRef)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIDecisionManager] ScheduleDefenseDecision - could not get DefenseSystem"));
        return;
    }

    EAIDifficulty Difficulty = GetCurrentDifficulty();

    // Roll for defense attempt
    float AttemptChance = GetDefenseAttemptChance(Difficulty);
    float Roll = FMath::FRand();

    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s defense roll: %.2f vs %.2f (%.0f%% chance)"),
           *Defender->GetName(), Roll, AttemptChance, AttemptChance * 100.0f);

    if (Roll > AttemptChance)
    {
        UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s chose not to defend"),
               *Defender->GetName());
        return;
    }

    // Choose defense type
    EDefenseType Choice = ChooseDefenseType(Defender, AttackSize, Difficulty);

    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s chose defense type: %d"),
           *Defender->GetName(), static_cast<int32>(Choice));

    if (Choice == EDefenseType::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIDecisionManager] %s - ChooseDefenseType returned None"),
               *Defender->GetName());
        return;
    }

    // Calculate reaction delay (must be within window)
    float ReactionDelay = CalculateDefenseReactionDelay(Difficulty, WindowDuration);

    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s will %s in %.2fs"),
           *Defender->GetName(),
           Choice == EDefenseType::Block ? TEXT("Block") : Choice == EDefenseType::Parry ? TEXT("Parry")
                                                                                         : TEXT("Dodge"),
           ReactionDelay);

    // Schedule defense input
    FTimerHandle &TimerHandle = DefenseTimerHandles.FindOrAdd(Defender);

    if (UWorld *World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(TimerHandle);

        FTimerDelegate TimerDelegate;
        TimerDelegate.BindLambda([this, Defender, Choice]()
                                 {
            if (!Defender || !DefenseSystemRef)
            {
                return;
            }

            // Roll for timing accuracy
            EAIDifficulty Diff = GetCurrentDifficulty();
            float Accuracy = GetDefenseAccuracy(Diff);
            bool bGoodTiming = FMath::FRand() < Accuracy;

            if (bGoodTiming)
            {
                EDefenseDirection Direction = EDefenseDirection::None;
                if (Choice == EDefenseType::Dodge)
                {
                    Direction = FMath::RandBool() ? EDefenseDirection::Left : EDefenseDirection::Right;
                }

                DefenseSystemRef->SubmitDefenseInput(Defender, Choice, Direction);
                
                UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s executed %s defense"),
                    *Defender->GetName(),
                    Choice == EDefenseType::Block ? TEXT("Block") : 
                    Choice == EDefenseType::Parry ? TEXT("Parry") : TEXT("Dodge"));
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s mistimed defense (%.0f%% accuracy)"),
                    *Defender->GetName(), Accuracy * 100.0f);
            }

            DefenseTimerHandles.Remove(Defender); });

        World->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, ReactionDelay, false);
    }
}

EDefenseType UAIDecisionManager::ChooseDefenseType(AActor *Defender, float AttackSize, EAIDifficulty Difficulty)
{
    // Check if dodge is viable
    bool bCanDodge = DefenseSystemRef && DefenseSystemRef->CanDodgeAttack(Defender, AttackSize);

    // Easy: Always block (safest)
    if (Difficulty == EAIDifficulty::Easy)
    {
        return EDefenseType::Block;
    }

    // Medium: Block or Dodge (no parry - too risky)
    if (Difficulty == EAIDifficulty::Medium)
    {
        if (bCanDodge && FMath::RandBool())
        {
            return EDefenseType::Dodge;
        }
        return EDefenseType::Block;
    }

    // Hard/Expert: Smart choice
    // Prioritize: Dodge (100% avoid) > Parry (70% + reflect) > Block (50%)
    if (bCanDodge)
    {
        return EDefenseType::Dodge;
    }

    // Parry vs Block: Higher difficulty = more parry attempts
    float ParryChance = (Difficulty == EAIDifficulty::Expert) ? 0.7f : 0.4f;
    if (FMath::FRand() < ParryChance)
    {
        return EDefenseType::Parry;
    }

    return EDefenseType::Block;
}

float UAIDecisionManager::GetDefenseAttemptChance(EAIDifficulty Difficulty) const
{
    switch (Difficulty)
    {
    case EAIDifficulty::Easy:
        return AIConstants::EASY_DEFENSE_ATTEMPT;
    case EAIDifficulty::Medium:
        return AIConstants::MEDIUM_DEFENSE_ATTEMPT;
    case EAIDifficulty::Hard:
        return AIConstants::HARD_DEFENSE_ATTEMPT;
    case EAIDifficulty::Expert:
        return AIConstants::EXPERT_DEFENSE_ATTEMPT;
    default:
        return AIConstants::MEDIUM_DEFENSE_ATTEMPT;
    }
}

float UAIDecisionManager::GetDefenseAccuracy(EAIDifficulty Difficulty) const
{
    switch (Difficulty)
    {
    case EAIDifficulty::Easy:
        return AIConstants::EASY_DEFENSE_ACCURACY;
    case EAIDifficulty::Medium:
        return AIConstants::MEDIUM_DEFENSE_ACCURACY;
    case EAIDifficulty::Hard:
        return AIConstants::HARD_DEFENSE_ACCURACY;
    case EAIDifficulty::Expert:
        return AIConstants::EXPERT_DEFENSE_ACCURACY;
    default:
        return AIConstants::MEDIUM_DEFENSE_ACCURACY;
    }
}

float UAIDecisionManager::CalculateDefenseReactionDelay(EAIDifficulty Difficulty, float WindowDuration) const
{
    // Reaction time as fraction of window
    // Easy: Late in window (70-90%)
    // Expert: Early in window (10-30%)
    float MinFraction, MaxFraction;

    switch (Difficulty)
    {
    case EAIDifficulty::Easy:
        MinFraction = 0.7f;
        MaxFraction = 0.9f;
        break;
    case EAIDifficulty::Medium:
        MinFraction = 0.4f;
        MaxFraction = 0.7f;
        break;
    case EAIDifficulty::Hard:
        MinFraction = 0.2f;
        MaxFraction = 0.5f;
        break;
    case EAIDifficulty::Expert:
        MinFraction = 0.1f;
        MaxFraction = 0.3f;
        break;
    default:
        MinFraction = 0.4f;
        MaxFraction = 0.7f;
    }

    float Fraction = FMath::FRandRange(MinFraction, MaxFraction);
    return WindowDuration * Fraction;
}