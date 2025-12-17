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

void UAIDecisionManager::Initialize(FSubsystemCollectionBase &Collection)
{
    Super::Initialize(Collection);
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

    FAction Action = BuildAction(PendingActor);

    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s executing action type %d"),
           *PendingActor->GetName(), static_cast<int32>(Action.ActionType));

    // Submit through orchestrator
    CurrentCombat->SubmitAction(Action);

    PendingActor = nullptr;
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