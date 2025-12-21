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
#include "StatusEffectManager.h"
#include "DamageCalculator.h"

UStatusEffectManager *UAIDecisionManager::GetStatusEffectManager() const
{
    UGameInstance *GameInstance = GetGameInstance();
    return GameInstance ? GameInstance->GetSubsystem<UStatusEffectManager>() : nullptr;
}

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

    // Get living enemies - needed for all branches
    TArray<AActor *> Enemies = CurrentCombat->GetLivingEnemies(AIActor);
    if (Enemies.Num() == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] No living enemies - defending"));
        return Action;
    }

    // Difficulty determines decision quality
    EAIDifficulty Difficulty = GetCurrentDifficulty();

    if (Difficulty == EAIDifficulty::Easy)
    {
        // Easy: Random action, random target (old behavior)
        AActor *Target = Enemies[FMath::RandRange(0, Enemies.Num() - 1)];
        EActionType ChosenType = ChooseActionType(AIActor, Loadout);

        Action.ActionType = ChosenType;
        Action.Targets.Add(Target);

        // Populate data (simplified)
        switch (ChosenType)
        {
        case EActionType::Spell:
        {
            TArray<USpellData *> Spells = Loadout->GetAvailableSpells();
            if (Spells.Num() > 0)
            {
                Action.SpellData = Spells[FMath::RandRange(0, Spells.Num() - 1)];
            }
            break;
        }
        case EActionType::Ability:
        {
            TArray<UAbilityData *> Abilities = Loadout->GetAvailableAbilities();
            if (Abilities.Num() > 0)
            {
                Action.AbilityData = Abilities[FMath::RandRange(0, Abilities.Num() - 1)];
            }
            break;
        }
        case EActionType::Attack:
        {
            UWeaponManager *WeaponManager = GetGameInstance()->GetSubsystem<UWeaponManager>();
            Action.AttackData = WeaponManager ? WeaponManager->GetActiveAttack(AIActor) : nullptr;
            break;
        }
        default:
            break;
        }

        return Action;
    }

    // Medium/Hard/Expert: Use smart decision branches
    return BuildAction_Smart(AIActor, Loadout, CharComp);
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

// ==================== TARGET SCORING ====================

int32 UAIDecisionManager::ScoreTarget(AActor *Attacker, AActor *Target, int32 EstimatedDamage)
{
    if (!Target)
    {
        return 0;
    }

    int32 Score = 0;
    int32 TargetHP = GetCurrentHP(Target);
    int32 TargetMaxHP = GetMaxHP(Target);

    // Kill potential - highest priority
    if (CanKillTarget(Attacker, Target, EstimatedDamage))
    {
        Score += AIConstants::KILL_POTENTIAL_SCORE;

        UE_LOG(LogTemp, Verbose, TEXT("[AIDecisionManager] %s can kill %s (+%d)"),
               *Attacker->GetName(), *Target->GetName(), AIConstants::KILL_POTENTIAL_SCORE);
    }

    // Low HP bonus - prioritize wounded targets
    float HPPercent = GetHPPercent(Target);
    int32 HPMissingScore = FMath::RoundToInt((1.0f - HPPercent) * AIConstants::HP_MISSING_WEIGHT);
    Score += HPMissingScore;

    // Threat assessment - prioritize dangerous enemies
    int32 ThreatScore = FMath::RoundToInt(CalculateThreatLevel(Target) * AIConstants::THREAT_WEIGHT);
    Score += ThreatScore;

    UE_LOG(LogTemp, Verbose, TEXT("[AIDecisionManager] Score for %s: %d (Kill:%s, HP:%.0f%%, Threat:%d)"),
           *Target->GetName(), Score,
           CanKillTarget(Attacker, Target, EstimatedDamage) ? TEXT("Yes") : TEXT("No"),
           HPPercent * 100.0f, ThreatScore);

    return Score;
}

AActor *UAIDecisionManager::SelectBestTarget(AActor *Attacker, const TArray<AActor *> &Enemies)
{
    if (Enemies.Num() == 0)
    {
        return nullptr;
    }

    // Easy difficulty: random target
    EAIDifficulty Difficulty = GetCurrentDifficulty();
    if (Difficulty == EAIDifficulty::Easy)
    {
        int32 RandomIndex = FMath::RandRange(0, Enemies.Num() - 1);
        UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] Easy difficulty - random target: %s"),
               *Enemies[RandomIndex]->GetName());
        return Enemies[RandomIndex];
    }

    // Medium+ difficulty: score and pick best
    AActor *BestTarget = nullptr;
    int32 BestScore = -1;

    for (AActor *Enemy : Enemies)
    {
        int32 EstimatedDamage = EstimateBestDamage(Attacker, Enemy);
        int32 Score = ScoreTarget(Attacker, Enemy, EstimatedDamage);

        if (Score > BestScore)
        {
            BestScore = Score;
            BestTarget = Enemy;
        }
    }

    if (BestTarget)
    {
        UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] Best target: %s (Score: %d)"),
               *BestTarget->GetName(), BestScore);
    }

    return BestTarget ? BestTarget : Enemies[0];
}

int32 UAIDecisionManager::EstimateBestDamage(AActor *Attacker, AActor *Target)
{
    // Get components
    ULoadoutComponent *Loadout = Attacker->FindComponentByClass<ULoadoutComponent>();
    UCharacterDataComponent *CharComp = Attacker->FindComponentByClass<UCharacterDataComponent>();

    if (!Loadout || !CharComp || !CharComp->CharacterData)
    {
        return 50; // Default estimate
    }

    int32 BestDamage = 0;

    // Check spells
    TArray<USpellData *> Spells = Loadout->GetAvailableSpells();
    for (USpellData *Spell : Spells)
    {
        if (Spell)
        {
            int32 SpellDamage = Spell->CalculateDamage(CharComp->CharacterData);
            BestDamage = FMath::Max(BestDamage, SpellDamage);
        }
    }

    // Check abilities
    TArray<UAbilityData *> Abilities = Loadout->GetAvailableAbilities();
    for (UAbilityData *Ability : Abilities)
    {
        if (Ability)
        {
            int32 AbilityDamage = Ability->CalculateDamage(CharComp->CharacterData, false);
            BestDamage = FMath::Max(BestDamage, AbilityDamage);
        }
    }

    // Check weapon attack
    UWeaponManager *WeaponManager = GetGameInstance()->GetSubsystem<UWeaponManager>();
    if (WeaponManager)
    {
        UWeaponAttackData *Attack = WeaponManager->GetActiveAttack(Attacker);
        if (Attack)
        {
            UDamageCalculator *DamageCalc = GetGameInstance()->GetSubsystem<UDamageCalculator>();
            if (DamageCalc)
            {
                FDamageCalculationResult DamageResult = DamageCalc->CalculateAttackDamage(Attacker, Target, Attack, false);
                int32 AttackDamage = DamageResult.FinalDamage;
                BestDamage = FMath::Max(BestDamage, AttackDamage);
            }
            else
            {
                // Fallback if DamageCalculator unavailable
                BestDamage = FMath::Max(BestDamage, 50);
            }
        }
    }

    return BestDamage > 0 ? BestDamage : 50;
}

int32 UAIDecisionManager::CalculateThreatLevel(AActor *Actor)
{
    UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
    if (!CharComp || !CharComp->CharacterData)
    {
        return 50; // Default threat
    }

    UCharacterData *Data = CharComp->CharacterData;
    int32 Threat = 0;

    // Raw damage stat points
    Threat += FMath::RoundToInt(Data->GetTotalRawDamage() * AIConstants::RAW_DAMAGE_THREAT_MULT);

    // Effect damage stat points
    Threat += FMath::RoundToInt(Data->GetTotalEffectDamage() * AIConstants::EFFECT_DAMAGE_THREAT_MULT);

    // Spell power (same as effect damage)
    Threat += FMath::RoundToInt(Data->GetTotalEffectDamage() * AIConstants::SPELL_POWER_THREAT_MULT);

    return Threat;
}

bool UAIDecisionManager::CanKillTarget(AActor *Attacker, AActor *Target, int32 Damage)
{
    int32 TargetHP = GetCurrentHP(Target);
    return Damage >= TargetHP && TargetHP > 0;
}

float UAIDecisionManager::GetHPPercent(AActor *Actor)
{
    int32 Current = GetCurrentHP(Actor);
    int32 Max = GetMaxHP(Actor);

    if (Max <= 0)
        return 1.0f;
    return FMath::Clamp((float)Current / (float)Max, 0.0f, 1.0f);
}

int32 UAIDecisionManager::GetCurrentHP(AActor *Actor)
{
    UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
    return CharComp ? CharComp->CurrentHP : 0;
}

int32 UAIDecisionManager::GetMaxHP(AActor *Actor)
{
    UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
    if (CharComp && CharComp->CharacterData)
    {
        return CharComp->CharacterData->CalculateMaxHealth();
    }
    return 100;
}

// ==================== DECISION BRANCHES ====================

FAction UAIDecisionManager::BuildAction_Smart(AActor *AIActor, ULoadoutComponent *Loadout, UCharacterDataComponent *CharComp)
{
    FAction Action;
    Action.ActionType = EActionType::Defend;

    // Branch 1: Survival
    if (TrySurvivalBranch(AIActor, Loadout, Action))
    {
        UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s: Survival branch triggered"),
               *AIActor->GetName());
        return Action;
    }

    // Branch 2: Cleanse
    if (TryCleanseBranch(AIActor, Loadout, Action))
    {
        UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s: Cleanse branch triggered"),
               *AIActor->GetName());
        return Action;
    }

    // Branch 3: Offensive (default)
    return BuildOffensiveAction(AIActor, Loadout, CharComp);
}

bool UAIDecisionManager::TrySurvivalBranch(AActor *AIActor, ULoadoutComponent *Loadout, FAction &OutAction)
{
    float HPPercent = GetHPPercent(AIActor);

    if (HPPercent >= AIConstants::SURVIVAL_HP_THRESHOLD)
    {
        return false; // HP is fine
    }

    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s HP at %.0f%% - checking survival options"),
           *AIActor->GetName(), HPPercent * 100.0f);

    // Try to find healing spell
    USpellData *HealSpell = FindHealingSpell(Loadout);
    if (HealSpell)
    {
        OutAction.ActionType = EActionType::Spell;
        OutAction.SpellData = HealSpell;
        OutAction.Targets.Add(AIActor); // Self-target

        UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s will heal with %s"),
               *AIActor->GetName(), *HealSpell->SpellName);
        return true;
    }

    // TODO: Check for healing items when ItemExecutor supports it

    // No healing available - defend to reduce incoming damage
    OutAction.ActionType = EActionType::Defend;
    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s has no healing - defending"),
           *AIActor->GetName());
    return true;
}

bool UAIDecisionManager::TryCleanseBranch(AActor *AIActor, ULoadoutComponent *Loadout, FAction &OutAction)
{
    if (!HasDangerousDebuff(AIActor))
    {
        return false;
    }

    USpellData *CleanseSpell = FindCleanseSpell(Loadout);
    if (CleanseSpell)
    {
        OutAction.ActionType = EActionType::Spell;
        OutAction.SpellData = CleanseSpell;
        OutAction.Targets.Add(AIActor); // Self-target

        UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s will cleanse with %s"),
               *AIActor->GetName(), *CleanseSpell->SpellName);
        return true;
    }

    // No cleanse available - continue to offensive
    return false;
}

FAction UAIDecisionManager::BuildOffensiveAction(AActor *AIActor, ULoadoutComponent *Loadout, UCharacterDataComponent *CharComp)
{
    FAction Action;
    Action.ActionType = EActionType::Defend;

    // Get enemies and select best target
    TArray<AActor *> Enemies = CurrentCombat->GetLivingEnemies(AIActor);
    if (Enemies.Num() == 0)
    {
        return Action;
    }

    AActor *Target = SelectBestTarget(AIActor, Enemies);
    Action.Targets.Add(Target);

    // Choose action type
    EActionType ChosenType = ChooseActionType(AIActor, Loadout);
    Action.ActionType = ChosenType;

    // Populate action data based on type
    switch (ChosenType)
    {
    case EActionType::Attack:
    {
        UWeaponManager *WeaponManager = GetGameInstance()->GetSubsystem<UWeaponManager>();
        Action.AttackData = WeaponManager ? WeaponManager->GetActiveAttack(AIActor) : nullptr;

        UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s chose Attack on %s"),
               *AIActor->GetName(), *Target->GetName());
        break;
    }

    case EActionType::Spell:
    {
        TArray<USpellData *> Spells = Loadout->GetAvailableSpells();
        if (Spells.Num() > 0)
        {
            // Pick highest damage spell for now
            USpellData *BestSpell = nullptr;
            int32 BestDamage = 0;

            for (USpellData *Spell : Spells)
            {
                if (Spell && Spell->School == ESpellSchool::Destruction)
                {
                    int32 Damage = Spell->CalculateDamage(CharComp->CharacterData);
                    if (Damage > BestDamage)
                    {
                        BestDamage = Damage;
                        BestSpell = Spell;
                    }
                }
            }

            // Fallback to any spell if no destruction spells
            if (!BestSpell && Spells.Num() > 0)
            {
                BestSpell = Spells[FMath::RandRange(0, Spells.Num() - 1)];
            }

            Action.SpellData = BestSpell;

            UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s chose Spell: %s on %s"),
                   *AIActor->GetName(),
                   BestSpell ? *BestSpell->SpellName : TEXT("None"),
                   *Target->GetName());
        }
        break;
    }

    case EActionType::Ability:
    {
        TArray<UAbilityData *> Abilities = Loadout->GetAvailableAbilities();
        if (Abilities.Num() > 0)
        {
            // Pick highest damage ability
            UAbilityData *BestAbility = nullptr;
            int32 BestDamage = 0;

            for (UAbilityData *Ability : Abilities)
            {
                if (Ability)
                {
                    int32 Damage = Ability->CalculateDamage(CharComp->CharacterData, false);
                    if (Damage > BestDamage)
                    {
                        BestDamage = Damage;
                        BestAbility = Ability;
                    }
                }
            }

            Action.AbilityData = BestAbility;

            UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s chose Ability: %s on %s"),
                   *AIActor->GetName(),
                   BestAbility ? *BestAbility->AbilityName : TEXT("None"),
                   *Target->GetName());
        }
        break;
    }

    default:
        UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s chose Defend"),
               *AIActor->GetName());
        break;
    }

    return Action;
}

bool UAIDecisionManager::HasDangerousDebuff(AActor *Actor)
{
    UStatusEffectManager *StatusManager = GetGameInstance()->GetSubsystem<UStatusEffectManager>();
    if (!StatusManager)
    {
        return false;
    }

    TArray<FStatusEffect> Effects = StatusManager->GetActiveEffects(Actor);

    for (const FStatusEffect &Effect : Effects)
    {
        // Check for dangerous debuffs (stun, heavy DOT, etc.)
        switch (Effect.EffectType)
        {
        case EStatusType::SkipTurn: // Was Stun
            return true;
        case EStatusType::DOT:
            // DOT is dangerous if it will kill us
            if (Effect.EffectValue * Effect.RemainingTurns >= GetCurrentHP(Actor))
            {
                return true;
            }
            break;
        default:
            break;
        }
    }

    return false;
}

USpellData *UAIDecisionManager::FindHealingSpell(ULoadoutComponent *Loadout)
{
    if (!Loadout)
    {
        return nullptr;
    }

    TArray<USpellData *> Spells = Loadout->GetAvailableSpells();

    for (USpellData *Spell : Spells)
    {
        if (Spell && Spell->School == ESpellSchool::Restoration)
        {
            // Check if it's actually a heal (positive effect on self)
            if (Spell->PrimaryEffect == EStatusType::Heal ||
                Spell->Damage < 0) // Negative damage = healing
            {
                return Spell;
            }
        }
    }

    return nullptr;
}

USpellData *UAIDecisionManager::FindCleanseSpell(ULoadoutComponent *Loadout)
{
    if (!Loadout)
    {
        return nullptr;
    }

    TArray<USpellData *> Spells = Loadout->GetAvailableSpells();

    for (USpellData *Spell : Spells)
    {
        if (Spell && Spell->PrimaryEffect == EStatusType::Cleanse)
        {
            return Spell;
        }
    }

    return nullptr;
}

// ==================== STATUS BAR QUERIES ====================

bool UAIDecisionManager::IsStatusBarNearTrigger(AActor *Target, float Threshold) const
{
    UStatusEffectManager *StatusManager = GetGameInstance()->GetSubsystem<UStatusEffectManager>();
    if (!StatusManager)
    {
        return false;
    }

    float BarPercent = StatusManager->GetStatusBarPercent(Target);
    return BarPercent >= Threshold;
}

bool UAIDecisionManager::WouldTriggerStatusBar(AActor *Attacker, AActor *Target, float BuildupAmount) const
{
    UStatusEffectManager *StatusManager = GetGameInstance()->GetSubsystem<UStatusEffectManager>();
    if (!StatusManager)
    {
        return false;
    }

    float RemainingBuildup = StatusManager->GetBuildupToTrigger(Target);
    return BuildupAmount >= RemainingBuildup;
}

bool UAIDecisionManager::IsValuableStatus(EStatusType StatusType, AActor *Target) const
{
    // Skip turn statuses are always valuable
    if (StatusType == EStatusType::SkipTurn)
    {
        return true;
    }

    UStatusEffectManager *StatusManager = GetGameInstance()->GetSubsystem<UStatusEffectManager>();
    if (!StatusManager)
    {
        return true; // Assume valuable if we can't check
    }

    // DOTs are valuable if target doesn't already have one
    if (StatusType == EStatusType::DOT)
    {
        return !StatusManager->HasActiveDOT(Target);
    }

    // Debuffs are valuable if target doesn't have too many already
    int32 DebuffCount = StatusManager->GetDebuffCount(Target);
    return DebuffCount < 3;
}

// ==================== INFUSION DECISIONS ====================

int32 UAIDecisionManager::DecideSpellInfusionLevel(AActor *Attacker, AActor *Target, USpellData *Spell) const
{
    if (!Attacker || !Target || !Spell)
    {
        return 0;
    }

    UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>();
    UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();
    if (!AttackerComp || !TargetComp || !AttackerComp->CharacterData)
    {
        return 0;
    }

    // Get difficulty
    EAIDifficulty Difficulty = GetCurrentDifficulty();

    // Easy: Never infuse
    if (Difficulty == EAIDifficulty::Easy)
    {
        return 0;
    }

    // Get energy state
    int32 CurrentEnergy = AttackerComp->CurrentEP;
    int32 MaxEnergy = AttackerComp->CharacterData->CalculateMaxEnergy();
    float EnergyPercent = static_cast<float>(CurrentEnergy) / MaxEnergy;

    // Calculate damage at each level
    int32 L0Damage = Spell->CalculateDamage(AttackerComp->CharacterData);
    int32 L2Damage = FMath::RoundToInt(L0Damage * 1.3f); // +30% damage

    int32 TargetHP = TargetComp->CurrentHP;

    // Priority 1: Can we kill with L0? Don't waste energy
    if (L0Damage >= TargetHP)
    {
        return 0;
    }

    // Priority 2: Can we kill with L2? Use it if we have energy
    if (L2Damage >= TargetHP && EnergyPercent > AIConstants::ENERGY_CONSERVATION_THRESHOLD)
    {
        return 2;
    }

    // Priority 3: Status bar considerations (Medium+)
    UStatusEffectManager *StatusManager = GetGameInstance()->GetSubsystem<UStatusEffectManager>();
    if (StatusManager)
    {
        // Calculate L1 buildup
        float BaseBuildup = Spell->StatusBuildup;
        float L1Buildup = BaseBuildup * 1.5f; // L1 boost

        // Would L1 trigger the bar?
        bool bL1WouldTrigger = WouldTriggerStatusBar(Attacker, Target, L1Buildup);
        bool bL0WouldTrigger = WouldTriggerStatusBar(Attacker, Target, BaseBuildup);

        // Get pending status type
        EStatusType PendingStatus = StatusManager->GetPendingStatus(Target);
        if (PendingStatus == EStatusType::None)
        {
            // Determine what status this spell would set
            if (Spell->PrimaryEffect != EStatusType::None)
            {
                PendingStatus = Spell->PrimaryEffect; // Already EStatusType!
            }
            else
            {
                PendingStatus = EStatusType::DOT; // Default for elemental damage
            }
        }

        // Use L1 if it triggers valuable status and L0 wouldn't
        if (bL1WouldTrigger && !bL0WouldTrigger && IsValuableStatus(PendingStatus, Target))
        {
            return 1;
        }

        // Hard+: Use L1 if bar is high (>70%) and we have good energy
        if (Difficulty >= EAIDifficulty::Hard &&
            IsStatusBarNearTrigger(Target, 0.70f) &&
            EnergyPercent > AIConstants::ENERGY_ABUNDANT_THRESHOLD &&
            IsValuableStatus(PendingStatus, Target))
        {
            return 1;
        }
    }

    // Conserve energy if low
    if (EnergyPercent < AIConstants::ENERGY_CONSERVATION_THRESHOLD)
    {
        return 0;
    }

    // Default: No infusion
    return 0;
}

int32 UAIDecisionManager::DecideAbilityInfusionLevel(AActor *Attacker, AActor *Target, UAbilityData *Ability) const
{
    // Abilities use same logic as spells for now
    // (can differentiate later if needed)

    if (!Ability)
    {
        return 0;
    }

    // Treat ability similar to spell for infusion decisions
    // For now, simplified - just check kill potential

    UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>();
    UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();
    if (!AttackerComp || !TargetComp || !AttackerComp->CharacterData)
    {
        return 0;
    }

    EAIDifficulty Difficulty = GetCurrentDifficulty();
    if (Difficulty == EAIDifficulty::Easy)
    {
        return 0;
    }

    int32 CurrentEnergy = AttackerComp->CurrentEP;
    int32 MaxEnergy = AttackerComp->CharacterData->CalculateMaxEnergy();
    float EnergyPercent = static_cast<float>(CurrentEnergy) / MaxEnergy;

    int32 L0Damage = Ability->CalculateDamage(AttackerComp->CharacterData, false);
    int32 L2Damage = FMath::RoundToInt(L0Damage * 1.3f);
    int32 TargetHP = TargetComp->CurrentHP;

    // Kill with L0? Don't waste
    if (L0Damage >= TargetHP)
    {
        return 0;
    }

    // Kill with L2? Use it
    if (L2Damage >= TargetHP && EnergyPercent > AIConstants::ENERGY_CONSERVATION_THRESHOLD)
    {
        return 2;
    }

    // Status bar logic (similar to spell)
    // TODO: Can add ability-specific status logic here

    return 0;
}