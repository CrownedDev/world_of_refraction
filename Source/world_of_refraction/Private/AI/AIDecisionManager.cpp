// AIDecisionManager.cpp

#include "AI/AIDecisionManager.h"
#include "AI/AIDecisionConstants.h"
#include "Combat/CombatOrchestrator.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CharacterData.h"
#include "Loadout/LoadoutComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Weapons/WeaponAttackData.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/AbilityData.h"
#include "TimerManager.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Defense/EDefenseDirection.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Infusion/InfusionConstants.h"
#include "Combat/CombatConstants.h"
#include "Loadout/Entries/FItemLoadoutSlot.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Equipment/Crystals/ItemIdentity.h"

USkillEffectManager *
UAIDecisionManager::GetSkillEffectManager() const
{
    UGameInstance *GameInstance = GetGameInstance();
    return GameInstance ? GameInstance->GetSubsystem<USkillEffectManager>() : nullptr;
}

UActionExecutor *
UAIDecisionManager::GetActionExecutor() const
{
    UGameInstance *GameInstance = GetGameInstance();
    return GameInstance ? GameInstance->GetSubsystem<UActionExecutor>() : nullptr;
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

    // Capture locally BEFORE any submit (which may trigger next turn synchronously)
    AActor *Actor = PendingActor;
    PendingActor = nullptr; // Clear BEFORE submit

    // Validate turn ownership - turn may have advanced while we were thinking
    AActor *CurrentTurnActor = CurrentCombat->GetCurrentActor();
    if (Actor != CurrentTurnActor)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[AIDecisionManager] %s decision dropped - turn moved to %s while thinking"),
               *Actor->GetName(),
               CurrentTurnActor ? *CurrentTurnActor->GetName() : TEXT("none"));
        return;
    }

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
            Action.AttackData = Loadout->GetCurrentAttack();
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

    if (Loadout->GetAllWeaponAttacks().Num() > 0)
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
    EDefenseType Choice = ChooseDefenseType(Defender, AttackSize, BaseDamage, Difficulty);

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

EDefenseType UAIDecisionManager::ChooseDefenseType(AActor *Defender, float AttackSize, int32 BaseDamage, EAIDifficulty Difficulty) const
{
    // Check if dodge is viable
    bool bCanDodge = DefenseSystemRef && DefenseSystemRef->CanDodgeAttack(Defender, AttackSize);

    // Lethality check — a hit that would kill always warrants a dodge attempt
    // (100% avoid), regardless of difficulty. Gated on bCanDodge: an undodgeable
    // lethal hit falls through to the Block/Parry logic below.
    if (Defender && bCanDodge)
    {
        if (UCharacterDataComponent *DefComp = Defender->FindComponentByClass<UCharacterDataComponent>())
        {
            if (BaseDamage >= DefComp->CurrentHP)
            {
                return EDefenseType::Dodge;
            }
        }
    }

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
    float ParryChance = (Difficulty == EAIDifficulty::Expert) ? AIConstants::EXPERT_PARRY_CHANCE : AIConstants::HARD_PARRY_CHANCE;
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
        MinFraction = AIConstants::EASY_REACTION_FRACTION_MIN;
        MaxFraction = AIConstants::EASY_REACTION_FRACTION_MAX;
        break;
    case EAIDifficulty::Medium:
        MinFraction = AIConstants::MEDIUM_REACTION_FRACTION_MIN;
        MaxFraction = AIConstants::MEDIUM_REACTION_FRACTION_MAX;
        break;
    case EAIDifficulty::Hard:
        MinFraction = AIConstants::HARD_REACTION_FRACTION_MIN;
        MaxFraction = AIConstants::HARD_REACTION_FRACTION_MAX;
        break;
    case EAIDifficulty::Expert:
        MinFraction = AIConstants::EXPERT_REACTION_FRACTION_MIN;
        MaxFraction = AIConstants::EXPERT_REACTION_FRACTION_MAX;
        break;
    default:
        MinFraction = AIConstants::MEDIUM_REACTION_FRACTION_MIN;
        MaxFraction = AIConstants::MEDIUM_REACTION_FRACTION_MAX;
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
            int32 SpellDamage = EstimateSpellDamage(Attacker, Target, Spell);
            BestDamage = FMath::Max(BestDamage, SpellDamage);
        }
    }

    // Check abilities
    TArray<UAbilityData *> Abilities = Loadout->GetAvailableAbilities();
    for (UAbilityData *Ability : Abilities)
    {
        if (Ability)
        {
            int32 AbilityDamage = EstimateAbilityDamage(Attacker, Target, Ability);
            BestDamage = FMath::Max(BestDamage, AbilityDamage);
        }
    }

    // Check weapon attack
    UWeaponAttackData *Attack = Loadout->GetCurrentAttack();
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

    return BestDamage > 0 ? BestDamage : 50;
}

int32 UAIDecisionManager::EstimateSpellDamage(AActor *Attacker, AActor *Target, USpellData *Spell, int32 InfusionLevel) const
{
    if (!Attacker || !Target || !Spell)
    {
        return 0;
    }

    UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>();
    if (!AttackerComp || !AttackerComp->CharacterData)
    {
        return 0;
    }

    UDamageCalculator *DamageCalc = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDamageCalculator>() : nullptr;
    UActionExecutor *ActionExec = GetActionExecutor();

    // Fall back to the raw asset value if the subsystems are unavailable.
    if (!DamageCalc || !ActionExec)
    {
        return Spell->CalculateDamage(AttackerComp->CharacterData);
    }

    // Build a complete action so ComputeActionStatModifiers can walk the
    // Reality/Evolution sources for this specific spell + infusion level.
    // Source is resolved off the attacker's active loadout so the modifier walk
    // sees the real provenance (ring/weapon/evolution casts compute differently
    // from innate). Falls back to Innate if no loadout is reachable.
    ULoadoutComponent *AttackerLoadout = Attacker->FindComponentByClass<ULoadoutComponent>();
    FAction Action;
    Action.ActionType = EActionType::Spell;
    Action.SpellData = Spell;
    Action.SpellSource = AttackerLoadout ? AttackerLoadout->ResolveSpellSource(Spell) : ESpellSource::Innate;
    Action.SpellInfusionLevel = InfusionLevel;
    Action.Targets.Add(Target);

    const FActionStatModifiers ActionMods = ActionExec->ComputeActionStatModifiers(Action, Attacker);

    // BaseDamage is the attacker-side base only; DamageCalculator applies the
    // SpellDamage stat, ActionMods and defender defense once, downstream.
    FDamageCalculationInput Input;
    Input.BaseDamage = Spell->CalculateDamage(AttackerComp->CharacterData);
    Input.ActionType = EActionType::Spell;
    Input.Element = Spell->Element;
    Input.ActionMods = ActionMods;
    Input.bCanCrit = false;

    const FDamageCalculationResult Result = DamageCalc->CalculateDamage(Attacker, Target, Input);

    // CalculateDamage ran with bCanCrit=false — fold expected crit value back in
    // so the estimate matches average execution damage.
    const float CritChance = DamageCalc->GetCriticalChance(Attacker);
    float Estimate = Result.FinalDamage * (1.0f + CritChance * (DamageConstants::CRIT_MULTIPLIER - 1.0f));

    // L2 charge infusion applies a damage multiplier (L0/L1 carry none).
    if (InfusionLevel == 2)
    {
        Estimate *= InfusionConstants::CHARGE_L2_DAMAGE_MULT;
    }

    return FMath::RoundToInt(Estimate);
}

int32 UAIDecisionManager::EstimateAbilityDamage(AActor *Attacker, AActor *Target, UAbilityData *Ability, int32 InfusionLevel) const
{
    if (!Attacker || !Target || !Ability)
    {
        return 0;
    }

    UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>();
    if (!AttackerComp || !AttackerComp->CharacterData)
    {
        return 0;
    }

    UDamageCalculator *DamageCalc = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDamageCalculator>() : nullptr;
    UActionExecutor *ActionExec = GetActionExecutor();

    // Fall back to the raw asset value if the subsystems are unavailable.
    if (!DamageCalc || !ActionExec)
    {
        return Ability->CalculateDamage(AttackerComp->CharacterData, false);
    }

    FAction Action;
    Action.ActionType = EActionType::Ability;
    Action.AbilityData = Ability;
    Action.AbilityInfusionLevel = InfusionLevel;
    Action.Targets.Add(Target);

    const FActionStatModifiers ActionMods = ActionExec->ComputeActionStatModifiers(Action, Attacker);

    // BaseDamage is the attacker-side base only; DamageCalculator applies the
    // RawDamage stat, ActionMods and defender defense once, downstream.
    FDamageCalculationInput Input;
    Input.BaseDamage = Ability->CalculateDamage(AttackerComp->CharacterData, false);
    Input.ActionType = EActionType::Ability;
    Input.Element = ESpellElement::Generic;
    Input.ActionMods = ActionMods;
    Input.bCanCrit = false;

    const FDamageCalculationResult Result = DamageCalc->CalculateDamage(Attacker, Target, Input);

    // CalculateDamage ran with bCanCrit=false — fold expected crit value back in
    // so the estimate matches average execution damage.
    const float CritChance = DamageCalc->GetCriticalChance(Attacker);
    float Estimate = Result.FinalDamage * (1.0f + CritChance * (DamageConstants::CRIT_MULTIPLIER - 1.0f));

    // L2 charge infusion applies a damage multiplier (L0/L1 carry none).
    if (InfusionLevel == 2)
    {
        Estimate *= InfusionConstants::CHARGE_L2_DAMAGE_MULT;
    }

    return FMath::RoundToInt(Estimate);
}

float UAIDecisionManager::EstimateStatusScore(AActor *Attacker, AActor *Target, USpellData *Spell)
{
    if (!Attacker || !Target || !Spell)
    {
        return 0.0f;
    }

    UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>();
    if (!AttackerComp || !AttackerComp->CharacterData)
    {
        return 0.0f;
    }

    const int32 Buildup = Spell->CalculateStatusBuildup(AttackerComp->CharacterData);
    if (Buildup <= 0)
    {
        return 0.0f;
    }

    // Target already carrying a dangerous status — extra buildup is low value.
    if (HasDangerousDebuff(Target))
    {
        return AIConstants::STATUS_SCORE_REDUNDANT;
    }

    // This hit would tip the bar, or the bar is already close — high value.
    if (WouldTriggerStatusBar(Attacker, Target, static_cast<float>(Buildup)) ||
        IsStatusBarNearTrigger(Target))
    {
        return AIConstants::STATUS_SCORE_TRIGGER;
    }

    // Otherwise a modest value for contributing buildup.
    return AIConstants::STATUS_SCORE_CONTRIBUTE;
}

float UAIDecisionManager::EstimateStatusScore(AActor *Attacker, AActor *Target, UAbilityData *Ability)
{
    if (!Attacker || !Target || !Ability)
    {
        return 0.0f;
    }

    UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>();
    if (!AttackerComp || !AttackerComp->CharacterData)
    {
        return 0.0f;
    }

    const int32 Buildup = Ability->CalculateStatusBuildup(AttackerComp->CharacterData);
    if (Buildup <= 0)
    {
        return 0.0f;
    }

    if (HasDangerousDebuff(Target))
    {
        return AIConstants::STATUS_SCORE_REDUNDANT;
    }

    if (WouldTriggerStatusBar(Attacker, Target, static_cast<float>(Buildup)) ||
        IsStatusBarNearTrigger(Target))
    {
        return AIConstants::STATUS_SCORE_TRIGGER;
    }

    return AIConstants::STATUS_SCORE_CONTRIBUTE;
}

bool UAIDecisionManager::CanAffordSpell(AActor *Actor, USpellData *Spell, int32 InfusionLevel) const
{
    if (!Actor || !Spell)
    {
        return false;
    }

    UActionExecutor *ActionExec = GetActionExecutor();
    if (!ActionExec)
    {
        return true; // Cannot compute cost — don't block the AI.
    }

    // Source is resolved off the actor's loadout — CalculateActionEnergyCost
    // returns 0 for ring/weapon crystal sources and varies for BD evolution
    // with the Innate-conversion carve-out, so a wrong default ESpellSource
    // would mis-gate affordability.
    ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>();
    FAction Probe;
    Probe.ActionType = EActionType::Spell;
    Probe.SpellData = Spell;
    Probe.SpellSource = Loadout ? Loadout->ResolveSpellSource(Spell) : ESpellSource::Innate;
    Probe.SpellInfusionLevel = InfusionLevel;

    const int32 Cost = ActionExec->CalculateActionEnergyCost(Actor, Probe);
    return GetCurrentEP(Actor) >= Cost;
}

bool UAIDecisionManager::CanAffordAbility(AActor *Actor, UAbilityData *Ability, int32 InfusionLevel) const
{
    if (!Actor || !Ability)
    {
        return false;
    }

    UActionExecutor *ActionExec = GetActionExecutor();
    if (!ActionExec)
    {
        return true; // Cannot compute cost — don't block the AI.
    }

    FAction Probe;
    Probe.ActionType = EActionType::Ability;
    Probe.AbilityData = Ability;
    Probe.AbilityInfusionLevel = InfusionLevel;

    const int32 Cost = ActionExec->CalculateActionEnergyCost(Actor, Probe);
    return GetCurrentEP(Actor) >= Cost;
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

    // Raw damage — FULL composed RawDamage (innate + equipment + stone + transient), capped
    // [0,2], so a geared/buffed combatant self-scores its physical threat accurately. Consistent
    // with the SpellDamage threat read below. AI heuristic, non-critical.
    Threat += FMath::RoundToInt(CharComp->GetEffectiveRawDamage() * AIConstants::RAW_DAMAGE_THREAT_MULT);

    // StatusMultiplier stat points (Spirit-side — status buildup strength).
    Threat += FMath::RoundToInt(Data->GetTotalStatusMultiplier() * AIConstants::STATUS_MULTIPLIER_THREAT_MULT);

    // Spell power — FULL composed SpellDamage (innate + equipment + stone + transient), so a
    // spell-geared/buffed combatant self-scores its threat more accurately. AI heuristic,
    // non-critical. (The RawDamage read above stays pillar — no composed RawDamage getter.)
    Threat += FMath::RoundToInt(CharComp->GetEffectiveSpellDamage() * AIConstants::SPELL_POWER_THREAT_MULT);

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
    if (CharComp)
    {
        return CharComp->MaxHP;
    }
    return 100;
}

// ==================== SMART DECISION BUILDING ====================

FAction UAIDecisionManager::BuildAction_Smart(AActor *AIActor, ULoadoutComponent *Loadout, UCharacterDataComponent *CharComp)
{
    FAction Action;

    // Branch 1: Survival - heal or defend if low HP
    if (TrySurvivalBranch(AIActor, Loadout, Action))
    {
        UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s chose survival action"), *AIActor->GetName());
        return Action;
    }

    // Branch 2: Cleanse - remove dangerous debuffs
    if (TryCleanseBranch(AIActor, Loadout, Action))
    {
        UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s chose cleanse action"), *AIActor->GetName());
        return Action;
    }

    // Branch 3: Offensive - attack best target
    Action = BuildOffensiveAction(AIActor, Loadout, CharComp);

    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s chose offensive action"), *AIActor->GetName());
    return Action;
}

// ==================== SURVIVAL BRANCH ====================

bool UAIDecisionManager::TrySurvivalBranch(AActor *AIActor, ULoadoutComponent *Loadout, FAction &OutAction)
{
    if (!AIActor || !Loadout)
    {
        return false;
    }

    UCharacterDataComponent *CharComp = AIActor->FindComponentByClass<UCharacterDataComponent>();
    if (!CharComp || !CharComp->CharacterData)
    {
        return false;
    }

    float HPPercent = static_cast<float>(CharComp->CurrentHP) / CharComp->MaxHP;
    int32 CurrentEnergy = GetCurrentEP(AIActor); // CurrentEP — unified BD/non-BD pool
    int32 MaxEnergy = CharComp->MaxEP;
    float EnergyPercent = static_cast<float>(CurrentEnergy) / MaxEnergy;

    // HP threshold based on difficulty
    EAIDifficulty Difficulty = GetCurrentDifficulty();
    float HealThreshold = (Difficulty == EAIDifficulty::Hard || Difficulty == EAIDifficulty::Expert) ? AIConstants::HARDEXPERT_HEAL_HP_THRESHOLD : AIConstants::EASYMED_HEAL_HP_THRESHOLD;

    // Priority 1: Low HP - need healing
    if (HPPercent <= HealThreshold)
    {
        // Try healing spell first (if we have energy)
        USpellData *HealSpell = FindHealingSpell(Loadout);
        if (HealSpell)
        {
            // Energy cost via ActionExecutor so efficiency + infusion multipliers apply.
            // Source resolved once so the probe and the OutAction agree.
            const ESpellSource HealSource = Loadout->ResolveSpellSource(HealSpell);
            FAction HealProbe;
            HealProbe.ActionType = EActionType::Spell;
            HealProbe.SpellData = HealSpell;
            HealProbe.SpellSource = HealSource;
            HealProbe.Targets.Add(AIActor);

            UActionExecutor *ActionExec = GetActionExecutor();
            const int32 HealCost = ActionExec
                                       ? ActionExec->CalculateActionEnergyCost(AIActor, HealProbe)
                                       : HealSpell->CalculateEnergyCost(CharComp->CharacterData);

            if (CurrentEnergy >= HealCost)
            {
                OutAction.ActionType = EActionType::Spell;
                OutAction.SpellData = HealSpell;
                OutAction.Targets.Add(AIActor); // Self-target
                OutAction.SpellSource = HealSource;
                UE_LOG(LogTemp, Log, TEXT("[AI Survival] Using healing spell"));
                return true;
            }
        }

        // Fallback: Use healing item (Sapphire)
        bool bFoundHeal = false;
        const FCrystalId HealId = FindHealingItem(Loadout, bFoundHeal);
        if (bFoundHeal)
        {
            OutAction.ActionType = EActionType::Item;
            OutAction.ItemData = HealId;
            OutAction.Targets.Add(AIActor); // Self-target
            UE_LOG(LogTemp, Log, TEXT("[AI Survival] Using healing item (Sapphire)"));
            return true;
        }

        // Last resort: Defend
        OutAction.ActionType = EActionType::Defend;
        UE_LOG(LogTemp, Log, TEXT("[AI Survival] No healing available - defending"));
        return true;
    }

    // Priority 2: Low energy - need energy restoration
    if (EnergyPercent < AIConstants::ENERGY_CONSERVATION_THRESHOLD) // 30%
    {
        bool bFoundEnergy = false;
        const FCrystalId EnergyId = FindEnergyItem(Loadout, bFoundEnergy);
        if (bFoundEnergy)
        {
            OutAction.ActionType = EActionType::Item;
            OutAction.ItemData = EnergyId;
            OutAction.Targets.Add(AIActor); // Self-target
            UE_LOG(LogTemp, Log, TEXT("[AI Survival] Using energy item (Citrine)"));
            return true;
        }
    }

    // Not in danger
    return false;
}

// ==================== CLEANSE BRANCH ====================

bool UAIDecisionManager::TryCleanseBranch(AActor *AIActor, ULoadoutComponent *Loadout, FAction &OutAction)
{
    if (!AIActor || !Loadout)
    {
        return false;
    }

    // Check for dangerous debuffs
    if (!HasDangerousDebuff(AIActor))
    {
        return false;
    }

    UCharacterDataComponent *CharComp = AIActor->FindComponentByClass<UCharacterDataComponent>();
    if (!CharComp)
    {
        return false;
    }

    int32 CurrentEnergy = GetCurrentEP(AIActor); // CurrentEP — unified BD/non-BD pool

    // Try cleanse spell first (if we have energy)
    USpellData *CleanseSpell = FindCleanseSpell(Loadout);
    if (CleanseSpell)
    {
        // Energy cost via ActionExecutor so efficiency + infusion multipliers apply.
        // Source resolved once so probe and OutAction agree.
        const ESpellSource CleanseSource = Loadout->ResolveSpellSource(CleanseSpell);
        FAction CleanseProbe;
        CleanseProbe.ActionType = EActionType::Spell;
        CleanseProbe.SpellData = CleanseSpell;
        CleanseProbe.SpellSource = CleanseSource;
        CleanseProbe.Targets.Add(AIActor);

        UActionExecutor *ActionExec = GetActionExecutor();
        const int32 Cost = ActionExec
                               ? ActionExec->CalculateActionEnergyCost(AIActor, CleanseProbe)
                               : CleanseSpell->CalculateEnergyCost(CharComp->CharacterData);
        if (CurrentEnergy >= Cost)
        {
            OutAction.ActionType = EActionType::Spell;
            OutAction.SpellData = CleanseSpell;
            OutAction.Targets.Add(AIActor); // Self-target
            OutAction.SpellSource = CleanseSource;
            UE_LOG(LogTemp, Log, TEXT("[AI Cleanse] Using cleanse spell"));
            return true;
        }
    }

    // Fallback: Use cleanse item (Iolite)
    bool bFoundCleanse = false;
    const FCrystalId CleanseId = FindCleanseItem(Loadout, bFoundCleanse);
    if (bFoundCleanse)
    {
        OutAction.ActionType = EActionType::Item;
        OutAction.ItemData = CleanseId;
        OutAction.Targets.Add(AIActor); // Self-target
        UE_LOG(LogTemp, Log, TEXT("[AI Cleanse] Using cleanse item (Iolite)"));
        return true;
    }

    // Can't cleanse - continue to offensive
    return false;
}

// ==================== ITEM DETECTION ====================

FCrystalId UAIDecisionManager::FindHealingItem(ULoadoutComponent *Loadout, bool &bOutFound)
{
    bOutFound = false;
    if (!Loadout)
    {
        return FCrystalId{};
    }

    for (const FItemLoadoutSlot &Slot : Loadout->GetUsableItems())
    {
        if (Slot.IsEmpty())
        {
            continue;
        }
        if (ItemIdentity::GetItemEffectType(Slot.CrystalId) == EItemEffectType::Healing)
        {
            bOutFound = true;
            return Slot.CrystalId;
        }
    }

    return FCrystalId{};
}

FCrystalId UAIDecisionManager::FindCleanseItem(ULoadoutComponent *Loadout, bool &bOutFound)
{
    bOutFound = false;
    if (!Loadout)
    {
        return FCrystalId{};
    }

    // Cleanse-only (Iolite). NOTE: deliberately does NOT match StatusClear
    // (Quartz) — Quartz reduces the status *buildup bar*, it does not remove an
    // already-applied debuff, so it can't satisfy TryCleanseBranch's intent.
    for (const FItemLoadoutSlot &Slot : Loadout->GetUsableItems())
    {
        if (Slot.IsEmpty())
        {
            continue;
        }
        if (ItemIdentity::GetItemEffectType(Slot.CrystalId) == EItemEffectType::Cleanse)
        {
            bOutFound = true;
            return Slot.CrystalId;
        }
    }

    return FCrystalId{};
}

FCrystalId UAIDecisionManager::FindEnergyItem(ULoadoutComponent *Loadout, bool &bOutFound)
{
    bOutFound = false;
    if (!Loadout)
    {
        return FCrystalId{};
    }

    for (const FItemLoadoutSlot &Slot : Loadout->GetUsableItems())
    {
        if (Slot.IsEmpty())
        {
            continue;
        }
        if (ItemIdentity::GetItemEffectType(Slot.CrystalId) == EItemEffectType::EnergyRestore)
        {
            bOutFound = true;
            return Slot.CrystalId;
        }
    }

    return FCrystalId{};
}

// ==================== OFFENSIVE ACTION ====================

FAction UAIDecisionManager::BuildOffensiveAction(AActor *AIActor, ULoadoutComponent *Loadout, UCharacterDataComponent *CharComp)
{
    FAction Action;

    if (!CurrentCombat)
    {
        Action.ActionType = EActionType::Defend;
        return Action;
    }

    // Get all enemies
    TArray<AActor *> Enemies = CurrentCombat->GetLivingEnemies(AIActor);
    if (Enemies.Num() == 0)
    {
        Action.ActionType = EActionType::Defend;
        return Action;
    }

    // Select best target
    AActor *BestTarget = SelectBestTarget(AIActor, Enemies);
    if (!BestTarget)
    {
        Action.ActionType = EActionType::Defend;
        return Action;
    }

    Action.Targets.Add(BestTarget);

    // Evaluate options and pick best
    TArray<EActionType> AvailableActions;
    TMap<EActionType, int32> ActionScores;

    // Gather available actions
    TArray<UWeaponAttackData *> AllAttacks = Loadout->GetAllWeaponAttacks();
    if (AllAttacks.Num() > 0)
    {
        AvailableActions.Add(EActionType::Attack);
    }

    if (Loadout->GetAvailableSpells().Num() > 0)
    {
        AvailableActions.Add(EActionType::Spell);
    }

    if (Loadout->GetAvailableAbilities().Num() > 0)
    {
        AvailableActions.Add(EActionType::Ability);
    }

    if (AvailableActions.Num() == 0)
    {
        Action.ActionType = EActionType::Defend;
        return Action;
    }

    // Score each action type
    for (EActionType ActionType : AvailableActions)
    {
        int32 Score = 0;

        switch (ActionType)
        {
        case EActionType::Attack:
        {
            // Score best attack from all weapons
            UDamageCalculator *DamageCalc = GetGameInstance()->GetSubsystem<UDamageCalculator>();
            if (DamageCalc)
            {
                for (UWeaponAttackData *Attack : AllAttacks)
                {
                    if (Attack)
                    {
                        FDamageCalculationResult Result = DamageCalc->CalculateAttackDamage(AIActor, BestTarget, Attack, false);
                        Score = FMath::Max(Score, Result.FinalDamage);
                    }
                }
            }
            break;
        }
        case EActionType::Spell:
        {
            // Best combined damage + status score among affordable spells.
            TArray<USpellData *> Spells = Loadout->GetAvailableSpells();
            float BestSpellScore = 0.0f;
            for (USpellData *Spell : Spells)
            {
                if (!Spell || Spell->School != ESpellSchool::Destruction)
                {
                    continue;
                }
                if (!CanAffordSpell(AIActor, Spell))
                {
                    continue; // Skip spells the AI cannot pay for.
                }
                const float SpellScore = EstimateSpellDamage(AIActor, BestTarget, Spell) +
                                         EstimateStatusScore(AIActor, BestTarget, Spell) * AIConstants::STATUS_SCORE_WEIGHT;
                BestSpellScore = FMath::Max(BestSpellScore, SpellScore);
            }
            Score = FMath::RoundToInt(BestSpellScore);
            break;
        }
        case EActionType::Ability:
        {
            // Best combined damage + status score among affordable abilities.
            TArray<UAbilityData *> Abilities = Loadout->GetAvailableAbilities();
            float BestAbilityScore = 0.0f;
            for (UAbilityData *Ability : Abilities)
            {
                if (!Ability)
                {
                    continue;
                }
                if (!CanAffordAbility(AIActor, Ability))
                {
                    continue; // Skip abilities the AI cannot pay for.
                }
                const float AbilityScore = EstimateAbilityDamage(AIActor, BestTarget, Ability) +
                                           EstimateStatusScore(AIActor, BestTarget, Ability) * AIConstants::STATUS_SCORE_WEIGHT;
                BestAbilityScore = FMath::Max(BestAbilityScore, AbilityScore);
            }
            Score = FMath::RoundToInt(BestAbilityScore);
            break;
        }
        default:
            break;
        }

        ActionScores.Add(ActionType, Score);
    }

    // Pick action with highest score
    EActionType BestActionType = EActionType::Defend;
    int32 BestScore = -1;

    for (const auto &Pair : ActionScores)
    {
        if (Pair.Value > BestScore)
        {
            BestScore = Pair.Value;
            BestActionType = Pair.Key;
        }
    }

    // Build final action
    Action.ActionType = BestActionType;

    switch (BestActionType)
    {
    case EActionType::Attack:
    {
        // Pick best attack from all weapons
        UWeaponAttackData *BestAttack = nullptr;
        int32 BestAttackDamage = 0;
        UDamageCalculator *DamageCalc = GetGameInstance()->GetSubsystem<UDamageCalculator>();

        for (UWeaponAttackData *Attack : Loadout->GetAllWeaponAttacks())
        {
            if (Attack && DamageCalc)
            {
                FDamageCalculationResult Result = DamageCalc->CalculateAttackDamage(AIActor, Action.Targets[0], Attack, false);
                if (Result.FinalDamage > BestAttackDamage)
                {
                    BestAttackDamage = Result.FinalDamage;
                    BestAttack = Attack;
                }
            }
            else if (Attack && !BestAttack)
            {
                BestAttack = Attack; // Fallback if no DamageCalc
            }
        }

        Action.AttackData = BestAttack;
        break;
    }
    case EActionType::Spell:
    {
        // Pick the affordable spell with the best combined damage + status score.
        TArray<USpellData *> Spells = Loadout->GetAvailableSpells();
        USpellData *BestSpell = nullptr;
        float BestSpellScore = -1.0f;

        for (USpellData *Spell : Spells)
        {
            if (!Spell || Spell->School != ESpellSchool::Destruction)
            {
                continue;
            }
            if (!CanAffordSpell(AIActor, Spell))
            {
                continue;
            }
            const float SpellScore = EstimateSpellDamage(AIActor, BestTarget, Spell) +
                                     EstimateStatusScore(AIActor, BestTarget, Spell) * AIConstants::STATUS_SCORE_WEIGHT;
            if (SpellScore > BestSpellScore)
            {
                BestSpellScore = SpellScore;
                BestSpell = Spell;
            }
        }

        // No affordable spell — fall through to Defend.
        if (!BestSpell)
        {
            Action.ActionType = EActionType::Defend;
            break;
        }

        Action.SpellData = BestSpell;
        Action.SpellSource = Loadout->ResolveSpellSource(BestSpell);

        // Decide infusion, then drop to L0 if the infused cost is unaffordable.
        int32 SpellInfusion = DecideSpellInfusionLevel(AIActor, Action.Targets[0], BestSpell);
        if (SpellInfusion > 0 && !CanAffordSpell(AIActor, BestSpell, SpellInfusion))
        {
            SpellInfusion = 0;
        }
        Action.SpellInfusionLevel = SpellInfusion;
        break;
    }
    case EActionType::Ability:
    {
        // Pick the affordable ability with the best combined damage + status score.
        TArray<UAbilityData *> Abilities = Loadout->GetAvailableAbilities();
        UAbilityData *BestAbility = nullptr;
        float BestAbilityScore = -1.0f;

        for (UAbilityData *Ability : Abilities)
        {
            if (!Ability)
            {
                continue;
            }
            if (!CanAffordAbility(AIActor, Ability))
            {
                continue;
            }
            const float AbilityScore = EstimateAbilityDamage(AIActor, BestTarget, Ability) +
                                       EstimateStatusScore(AIActor, BestTarget, Ability) * AIConstants::STATUS_SCORE_WEIGHT;
            if (AbilityScore > BestAbilityScore)
            {
                BestAbilityScore = AbilityScore;
                BestAbility = Ability;
            }
        }

        // No affordable ability — fall through to Defend.
        if (!BestAbility)
        {
            Action.ActionType = EActionType::Defend;
            break;
        }

        Action.AbilityData = BestAbility;

        // Decide infusion, then drop to L0 if the infused cost is unaffordable.
        int32 AbilityInfusion = DecideAbilityInfusionLevel(AIActor, Action.Targets[0], BestAbility);
        if (AbilityInfusion > 0 && !CanAffordAbility(AIActor, BestAbility, AbilityInfusion))
        {
            AbilityInfusion = 0;
        }
        Action.AbilityInfusionLevel = AbilityInfusion;
        break;
    }
    default:
        Action.ActionType = EActionType::Defend;
        break;
    }

    return Action;
}

// ==================== HELPER FUNCTIONS ====================

bool UAIDecisionManager::HasDangerousDebuff(AActor *Actor)
{
    if (!Actor)
    {
        return false;
    }

    USkillEffectManager *StatusManager = GetSkillEffectManager();
    if (!StatusManager)
    {
        return false;
    }

    TArray<FActiveSkillEffect> Effects = StatusManager->GetActiveEffects(Actor);
    for (const FActiveSkillEffect &Effect : Effects)
    {
        switch (Effect.EffectType)
        {
        case ESkillEffectType::SkipTurn: // Stun
            return true;
        case ESkillEffectType::DOT:
            // Check if DOT will kill us
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
        if (!Spell || Spell->School != ESpellSchool::Restoration)
        {
            continue;
        }

        for (const FSkillEffect &Effect : Spell->Effects)
        {
            if (Effect.EffectType == ESkillEffectType::Heal ||
                Effect.EffectType == ESkillEffectType::HealthRestore)
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
        if (!Spell)
        {
            continue;
        }

        for (const FSkillEffect &Effect : Spell->Effects)
        {
            if (Effect.EffectType == ESkillEffectType::Cleanse)
            {
                return Spell;
            }
        }
    }

    return nullptr;
}

// ==================== STATUS BAR QUERIES ====================

bool UAIDecisionManager::IsStatusBarNearTrigger(AActor *Target, float Threshold) const
{
    if (!Target)
    {
        return false;
    }

    UStatusBuildupManager *BuildupManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStatusBuildupManager>() : nullptr;
    if (!BuildupManager)
    {
        return false;
    }

    // Get current bar percentage (0.0 - 1.0)
    float BarPercent = BuildupManager->GetStatusBarPercent(Target);

    // Check if bar is near trigger threshold
    return BarPercent >= Threshold; // Default threshold is 0.70 (70%)
}

bool UAIDecisionManager::WouldTriggerStatusBar(AActor *Attacker, AActor *Target, float BuildupAmount) const
{
    if (!Attacker || !Target)
    {
        return false;
    }

    UStatusBuildupManager *BuildupManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStatusBuildupManager>() : nullptr;
    if (!BuildupManager)
    {
        return false;
    }

    // Get remaining buildup needed to trigger
    float RemainingBuildup = BuildupManager->GetBuildupToTrigger(Target);

    // TODO (vulnerability awareness): BuildupAmount here is the RAW attacker-side
    // buildup — the target's resistance shave is NOT applied. Since resistance can now
    // go negative (amplification) or positive (reduction), this scorer won't perceive
    // a vulnerability-debuffed target taking up to 2x buildup, nor a resistant target
    // taking less. To exploit/avoid it, project BuildupAmount through the target's
    // resistance before comparing (mirror StatusBuildupManager's resistance aggregate).
    // Check if this hit would trigger the bar
    return BuildupAmount >= RemainingBuildup;
}

bool UAIDecisionManager::IsValuableStatus(ESkillEffectType StatusType, AActor *Target) const
{
    // Skip turn statuses are always valuable
    if (StatusType == ESkillEffectType::SkipTurn)
    {
        return true;
    }

    USkillEffectManager *StatusManager = GetGameInstance()->GetSubsystem<USkillEffectManager>();
    if (!StatusManager)
    {
        return true; // Assume valuable if we can't check
    }

    // DOTs are valuable if target doesn't already have one
    if (StatusType == ESkillEffectType::DOT)
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
    int32 CurrentEnergy = GetCurrentEP(Attacker);
    int32 MaxEnergy = GetMaxEP(Attacker);
    float EnergyPercent = static_cast<float>(CurrentEnergy) / MaxEnergy;

    // Calculate damage at each level
    int32 L0Damage = EstimateSpellDamage(Attacker, Target, Spell);
    int32 L2Damage = FMath::RoundToInt(L0Damage * InfusionConstants::CHARGE_L2_DAMAGE_MULT); // +30% damage

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
    UStatusBuildupManager *BuildupManager = GetGameInstance()->GetSubsystem<UStatusBuildupManager>();
    if (BuildupManager)
    {
        // Calculate L1 buildup
        float BaseBuildup = Spell->StatusBuildup;
        float L1Buildup = BaseBuildup * CombatConstants::SPELL_L1_BUILDUP_MULT; // L1 boost

        // Would L1 trigger the bar?
        bool bL1WouldTrigger = WouldTriggerStatusBar(Attacker, Target, L1Buildup);
        bool bL0WouldTrigger = WouldTriggerStatusBar(Attacker, Target, BaseBuildup);

        // Get pending status type (abilities apply physical status, not specific types)
        ESkillEffectType PendingTrigger = BuildupManager->GetPendingTrigger(Target);
        if (PendingTrigger == ESkillEffectType::None)
        {
            // Default to DOT for abilities (they apply status via infusion)
            PendingTrigger = ESkillEffectType::DOT;
        }

        // Use L1 if it triggers valuable status and L0 wouldn't
        if (bL1WouldTrigger && !bL0WouldTrigger && IsValuableStatus(PendingTrigger, Target))
        {
            return 1;
        }

        // Hard+: Use L1 if bar is high (>70%) and we have good energy
        if (Difficulty >= EAIDifficulty::Hard &&
            IsStatusBarNearTrigger(Target) &&
            EnergyPercent > AIConstants::ENERGY_ABUNDANT_THRESHOLD &&
            IsValuableStatus(PendingTrigger, Target))
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
    if (!Attacker || !Target || !Ability)
    {
        return 0;
    }

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

    int32 CurrentEnergy = GetCurrentEP(Attacker);
    int32 MaxEnergy = GetMaxEP(Attacker);
    float EnergyPercent = static_cast<float>(CurrentEnergy) / MaxEnergy;

    // Calculate damage at each level
    int32 L0Damage = EstimateAbilityDamage(Attacker, Target, Ability);
    int32 L2Damage = FMath::RoundToInt(L0Damage * InfusionConstants::CHARGE_L2_DAMAGE_MULT); // L2: +30% damage
    int32 TargetHP = TargetComp->CurrentHP;

    // Priority 1: Can we kill with L0? Don't waste
    if (L0Damage >= TargetHP)
    {
        return 0;
    }

    // Priority 2: Can we kill with L2? Use it
    if (L2Damage >= TargetHP && EnergyPercent > AIConstants::ENERGY_CONSERVATION_THRESHOLD)
    {
        return 2;
    }

    // Priority 3: Status bar considerations (Medium+)
    UStatusBuildupManager *BuildupManager = GetGameInstance()->GetSubsystem<UStatusBuildupManager>();
    if (BuildupManager && Difficulty >= EAIDifficulty::Medium)
    {
        // Calculate L1 status buildup
        int32 BaseBuildup = Ability->CalculateStatusBuildup(AttackerComp->CharacterData);
        // TODO: shares spell L1 buildup rule by analogy; give ability its own
        // constant if ability buildup ever diverges from spells.
        float L1Buildup = BaseBuildup * CombatConstants::SPELL_L1_BUILDUP_MULT; // L1: +50% status buildup

        // Would L1 trigger the bar?
        bool bL1WouldTrigger = WouldTriggerStatusBar(Attacker, Target, L1Buildup);
        bool bL0WouldTrigger = WouldTriggerStatusBar(Attacker, Target, BaseBuildup);

        // Get pending status type
        ESkillEffectType PendingTrigger = BuildupManager->GetPendingTrigger(Target);
        if (PendingTrigger == ESkillEffectType::None)
        {
            PendingTrigger = ESkillEffectType::DOT;
        }

        // Use L1 if it triggers valuable status and L0 wouldn't
        if (bL1WouldTrigger && !bL0WouldTrigger && IsValuableStatus(PendingTrigger, Target))
        {
            return 1;
        }

        // Hard+: Use L1 if bar is high (>70%) and we have good energy
        if (Difficulty >= EAIDifficulty::Hard &&
            IsStatusBarNearTrigger(Target) &&
            EnergyPercent > AIConstants::ENERGY_ABUNDANT_THRESHOLD &&
            IsValuableStatus(PendingTrigger, Target))
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

// ==================== HELPER FUNCTIONS ====================

int32 UAIDecisionManager::GetCurrentEP(AActor *Actor) const
{
    if (!Actor)
    {
        return 0;
    }

    UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
    if (!CharComp)
    {
        return 0;
    }

    // CurrentEP is the unified spend pool — Broken Darkness (absorption) and
    // non-BD (regenerating EP) characters alike.
    return CharComp->CurrentEP;
}

int32 UAIDecisionManager::GetMaxEP(AActor *Actor) const
{
    if (!Actor)
    {
        return 1; // Avoid division by zero
    }

    UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
    if (CharComp)
    {
        return CharComp->MaxEP;
    }

    return 1; // Avoid division by zero
}

UCharacterData *UAIDecisionManager::GetCharacterData(AActor *Actor) const
{
    if (!Actor)
    {
        return nullptr;
    }

    UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
    if (CharComp)
    {
        return CharComp->CharacterData;
    }

    return nullptr;
}