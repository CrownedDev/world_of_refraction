# Turn Manager Design Document
**Project:** World of Refraction (Unreal Engine 5.7)  
**Date:** November 25, 2025  
**Status:** Design Complete - Ready for Implementation  
**Branch:** refactor/character-stats-base

---

## Table of Contents
1. [Project Context & Requirements](#project-context--requirements)
2. [Core Architecture Decisions](#core-architecture-decisions)
3. [Turn Order System Design](#turn-order-system-design)
4. [Tie-Breaker System](#tie-breaker-system)
5. [Implementation Specifications](#implementation-specifications)
6. [Next Steps](#next-steps)

---

## Project Context & Requirements

### Game Overview
- **Genre:** Turn-based combat RPG with anime aesthetic
- **Combat Scale:** Variable team sizes (1v1, 2v2, 3v3)
- **Core Philosophy:** Player = Enemy (no distinction in code, only control differs)
- **Multiplayer:** Server-authoritative combat with replication
- **Unique Features:**
  - Character-to-enemy system (players create characters that become AI opponents)
  - PvP "boss battles" (friends' characters as bosses)
  - Speed-based turn order with double-turn mechanics
  - Element-based combat (9 elements + special mechanics)

### Critical Design Constraints
1. **NO "Player" vs "Enemy" classes** - Controller determines AI vs Human
2. **Scalable team sizes** - 1v1 to 3v3+ with zero code changes
3. **Production-grade multiplayer** - Full replication support
4. **Component-based architecture** - Unreal Engine best practices
5. **CharacterData as source of truth** - All formulas in DataAssets

---

## Core Architecture Decisions

### Decision 1: Actor → CharacterData Access Pattern

**CHOSEN: Component Pattern with Replication**

#### Implementation
```cpp
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UCharacterDataComponent : public UActorComponent
{
    GENERATED_BODY()
    
public:
    UCharacterDataComponent();
    
    // Replication setup
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    
    // ==================== CHARACTER TEMPLATE ====================
    
    /** Static character template - defines base stats, abilities, etc. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
    UCharacterData* CharacterData;
    
    // ==================== RUNTIME COMBAT STATE ====================
    
    /** Current hit points (replicates to clients) */
    UPROPERTY(ReplicatedUsing=OnRep_CurrentHP, BlueprintReadOnly, Category = "Combat")
    int32 CurrentHP;
    
    /** Current energy points (replicates to clients) */
    UPROPERTY(ReplicatedUsing=OnRep_CurrentEP, BlueprintReadOnly, Category = "Combat")
    int32 CurrentEP;
    
    /** Active status effects (replicates to clients) */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Combat")
    TArray<FStatusEffect> ActiveStatusEffects;
    
    /** Absorption state for BrokenDarkness (replicates to clients) */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Combat")
    FAbsorptionState AbsorptionState;
    
    // ==================== INITIALIZATION ====================
    
    /** Initialize combat state from CharacterData template */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void InitializeForCombat();
    
    // ==================== SERVER FUNCTIONS ====================
    
    /** Apply damage (server-authoritative) */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerTakeDamage(int32 Damage);
    
    /** Apply healing (server-authoritative) */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerHeal(int32 Amount);
    
    /** Spend energy (server-authoritative) */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerSpendEnergy(int32 Cost);
    
    // ==================== REPLICATION CALLBACKS ====================
    
    UFUNCTION()
    void OnRep_CurrentHP();
    
    UFUNCTION()
    void OnRep_CurrentEP();
    
    // ==================== DELEGATES ====================
    
    UPROPERTY(BlueprintAssignable, Category = "Combat")
    FOnHealthChanged OnHealthChanged;
    
    UPROPERTY(BlueprintAssignable, Category = "Combat")
    FOnEnergyChanged OnEnergyChanged;
};
```

#### Rationale

**Why Component Pattern:**

1. **Multiplayer Replication**
   - Components have built-in replication support
   - Server changes HP → Automatically syncs to all clients
   - No manual networking code needed

2. **Character-to-Enemy System**
   - Trivial implementation: `EnemyActor->AddComponent(PlayerCharacterComponent)`
   - Download friend's CharacterData → Spawn actor → Attach component
   - Perfect for "friend as boss" feature

3. **Controller-Agnostic Design**
   ```cpp
   // Character doesn't care WHO controls it
   void UTurnManager::StartTurn(AActor* Actor)
   {
       AController* Controller = Cast<APawn>(Actor)->GetController();
       
       if (Cast<AAIController>(Controller))
           AISystem->MakeDecision(Actor);
       else if (Cast<APlayerController>(Controller))
           UISystem->ShowActionMenu(Actor);
   }
   ```

4. **3v3 Scaling**
   - Array of actors, not hardcoded "Player" and "Enemy"
   - Scales from 1v1 to 5v5 with zero code changes
   - Variable team sizes supported inherently

5. **Testing & Iteration**
   - Easy to swap CharacterData in editor
   - Can test different builds without code changes
   - Mock components for unit testing

**Alternatives Considered:**

| Pattern | Pros | Cons | Verdict |
|---------|------|------|---------|
| Direct Reference | Simple, fast access | No replication, tight coupling | ❌ Not viable for multiplayer |
| Interface | Decoupled, flexible | Verbose, complex | ❌ Over-engineered for needs |
| Component | Replication, UE-idiomatic | Slight indirection | ✅ **CHOSEN** |

---

## Turn Order System Design

### Decision 2: Turn Structure

**CHOSEN: Turn Debt System**

#### Core Concept

Each character accumulates "turns owed" based on their speed relative to the slowest combatant. The character with the highest net debt (owed - taken) acts next.

#### Mathematical Model

```cpp
// At each turn calculation:
float SlowestSpeed = FindSlowestCombatant();

for each combatant:
    float SpeedRatio = combatant.Speed / SlowestSpeed;
    combatant.TurnsOwed += SpeedRatio;

// Find combatant with highest net debt
AActor* NextActor = FindHighestDebt();
NextActor.TurnsTaken++;
```

#### Example Scenario

**Team Setup:**
```
Team 1:              Team 2:
Character A: 30      Character X: 25
Character B: 20      Character Y: 15  
Character C: 12      Character Z: 10 (slowest - baseline)
```

**Turn Calculation:**
```
Round 1:
A (30): Ratio 30/10 = 3.0 → Owed 3.0 turns
X (25): Ratio 25/10 = 2.5 → Owed 2.5 turns
B (20): Ratio 20/10 = 2.0 → Owed 2.0 turns
Y (15): Ratio 15/10 = 1.5 → Owed 1.5 turns
C (12): Ratio 12/10 = 1.2 → Owed 1.2 turns
Z (10): Ratio 10/10 = 1.0 → Owed 1.0 turns

Turn Sequence (based on net debt):
Turn 1: A acts (owed 3.0, taken 0 → net 3.0) → taken=1
Turn 2: X acts (owed 2.5, taken 0 → net 2.5) → taken=1
Turn 3: A acts (owed 3.0, taken 1 → net 2.0) → taken=2
Turn 4: B acts (owed 2.0, taken 0 → net 2.0) → taken=1
Turn 5: X acts (owed 2.5, taken 1 → net 1.5) → taken=2
Turn 6: Y acts (owed 1.5, taken 0 → net 1.5) → taken=1
Turn 7: C acts (owed 1.2, taken 0 → net 1.2) → taken=1
Turn 8: A acts (owed 3.0, taken 2 → net 1.0) → taken=3
Turn 9: Z acts (owed 1.0, taken 0 → net 1.0) → taken=1
```

**Result:** Character A acts 3 times while Character Z acts once (natural 3:1 ratio)

#### Data Structure

```cpp
USTRUCT()
struct FCombatantTurnDebt
{
    GENERATED_BODY()
    
    UPROPERTY()
    AActor* Actor;
    
    UPROPERTY()
    float Speed; // Cached from CharacterData
    
    UPROPERTY()
    float TurnsOwed; // Accumulates based on speed ratio
    
    UPROPERTY()
    int32 TurnsTaken; // How many actions this combatant has used
    
    UPROPERTY()
    bool bIsAlive; // Skip if dead
};
```

#### Rationale

**Why Turn Debt System:**

1. **Handles 3v3 Elegantly**
   - Speed differences create natural rhythm
   - No hardcoded team sizes
   - All combatants treated equally

2. **Double-Turn Mechanic Works Naturally**
   - 15+ speed difference = ~2:1 ratio automatically
   - No hardcoded thresholds needed
   - Scales smoothly (faster = more turns proportionally)

3. **Production-Ready**
   - Used in Fire Emblem, Final Fantasy Tactics
   - Well-understood by players
   - Battle-tested design pattern

4. **Speed Buffs Impactful**
   - Recalculate debt ratios when speed changes
   - Immediate effect on turn frequency
   - Dynamic and responsive

5. **Strictly Turn-Based**
   - No real-time ticking (easier for multiplayer)
   - Predictable turn order
   - Can calculate next N actors for UI

**Alternatives Considered:**

| Pattern | Pros | Cons | Verdict |
|---------|------|------|---------|
| Simple Queue | Easy to understand, predictable | Double-turns don't fit, static | ❌ Inflexible |
| Action Points (ATB) | Natural double-turns, dynamic | Real-time ticking, complex UI | ❌ Not turn-based |
| Turn Debt | Production-tested, flexible, fair | Complex math | ✅ **CHOSEN** |

---

### Speed Calculation Formulas

#### Turn Order Speed

**Formula:**
```cpp
int32 CalculateTurnOrderSpeed() const
{
    int32 BaseSpeed = WorldBody; // 0-7
    int32 TurnSpeedBonus = GetSubStat(ESubStatType::TurnSpeed); // 0-21 (Mind substat)
    return BaseSpeed + TurnSpeedBonus;
}
```

**Range:** 0-28 total
- WorldBody: 0-7 (physical reflexes)
- TurnSpeed: 0-21 max (if WorldMind 7, get 21 points to distribute)

**Example:**
- WorldBody 5, TurnSpeed 10 → Turn Order Speed = 15
- WorldBody 7, TurnSpeed 21 → Turn Order Speed = 28 (fastest possible)

#### Attack Speed (Execution Speed)

**Formula:**
```cpp
int32 CalculateAttackSpeed() const
{
    int32 BaseSpeed = WorldBody; // 0-7
    int32 AttackSpeedBonus = GetSubStat(ESubStatType::AttackSpeed); // 0-21 (Body substat)
    return BaseSpeed + AttackSpeedBonus;
}
```

**Purpose:** Affects spell/attack/ability execution speed, NOT turn order

**Range:** 0-28 total

**Key Distinction:**
- **TurnSpeed** (Mind substat) = When you act
- **AttackSpeed** (Body substat) = How fast actions execute

---

### Decision 3: Speed Buff Timing

**CHOSEN: Flexible Timing via EEffectActivationTiming**

#### Implementation

```cpp
UENUM(BlueprintType)
enum class EEffectActivationTiming : uint8
{
    Immediate,          // Activates when applied (Haste spell)
    StartOfNextTurn,    // Activates at start of target's next turn
    AfterTurns,         // Activates after X turns pass (delayed buff)
    OnTrigger           // Manual activation (conditional)
};

USTRUCT(BlueprintType)
struct FStatusEffect
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EEffectActivationTiming ActivationTiming;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TurnsUntilActivation = 0; // If AfterTurns
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SpeedModifier = 0;
};
```

#### Usage Examples

**Immediate Activation:**
```cpp
// Spell: Haste
SpellEffect.ActivationTiming = EEffectActivationTiming::Immediate;
SpellEffect.SpeedModifier = +10;
// → Turn order recalculates NOW
```

**Delayed Activation:**
```cpp
// Spell: Delayed Acceleration
SpellEffect.ActivationTiming = EEffectActivationTiming::AfterTurns;
SpellEffect.TurnsUntilActivation = 2;
SpellEffect.SpeedModifier = +15;
// → Turn order recalculates in 2 turns
```

**Conditional Activation:**
```cpp
// Spell: Adrenaline Rush (activates when HP < 30%)
SpellEffect.ActivationTiming = EEffectActivationTiming::OnTrigger;
SpellEffect.SpeedModifier = +20;
// → Turn order recalculates when condition met
```

#### TurnManager Response

```cpp
void UStatusEffectManager::ActivateEffect(FStatusEffect& Effect, AActor* Target)
{
    if (Effect.SpeedModifier != 0)
    {
        // Apply modifier
        UCharacterDataComponent* Comp = Target->FindComponentByClass<UCharacterDataComponent>();
        Comp->RuntimeSpeedModifier += Effect.SpeedModifier;
        
        // Signal turn manager to recalculate
        TurnManager->OnSpeedChanged(Target);
    }
}

void UTurnManager::OnSpeedChanged(AActor* AffectedActor)
{
    // Recalculate turn debts with new speeds
    RecalculateTurnDebts();
    
    UE_LOG(LogCombat, Log, TEXT("Speed changed for %s, turn order recalculated"), 
           *AffectedActor->GetName());
}
```

#### Rationale

- ✅ **Maximum designer flexibility** - Control timing per spell
- ✅ **Supports complex mechanics** - Delayed buffs, conditional triggers
- ✅ **Clear player feedback** - Effect timing matches expectations
- ✅ **Multiplayer-safe** - Server-authoritative activation

---

### Decision 4: Double-Turn Threshold

**CHOSEN: No Hard Threshold - Natural Ratio from Debt System**

#### How It Works

The turn debt system naturally creates double-turn (and triple-turn) scenarios based on speed ratios:

| Speed Difference | Approximate Ratio | Player Experience |
|------------------|-------------------|-------------------|
| 0-5 | ~1:1 | Equal turns |
| 6-10 | ~1.2:1 | Slightly faster |
| 11-15 | ~1.5:1 | Noticeably faster |
| 16-20 | ~2:1 | **Double turn territory** |
| 21-30 | ~3:1 | Dominant speed |
| 31+ | ~4:1+ | Blitz mode |

**Example:**
```
Speed 30 vs Speed 10:
- Ratio = 30/10 = 3.0
- Natural 3:1 ratio (no hardcoded threshold)

Speed 25 vs Speed 10:
- Ratio = 25/10 = 2.5
- Natural 2.5:1 ratio

Speed 30 vs Speed 20:
- Ratio = 30/20 = 1.5
- Natural 1.5:1 ratio
```

#### Optional Speed Ratio Cap

If testing shows speed stacking is too powerful:

```cpp
void UTurnManager::CalculateNextActor()
{
    for (FCombatantTurnDebt& Debt : TurnDebts)
    {
        float SpeedRatio = Debt.Speed / SlowestSpeed;
        
        // Optional: Cap ratio at 3:1 maximum
        SpeedRatio = FMath::Min(SpeedRatio, 3.0f);
        
        Debt.TurnsOwed += SpeedRatio;
    }
}
```

**Recommendation:** No cap initially - let players experiment and discover optimal builds.

#### Rationale

- ✅ **Smooth scaling** - No artificial breakpoints
- ✅ **Intuitive** - Faster = more turns (proportional)
- ✅ **Flexible** - Easy to tune via speed stat ranges
- ✅ **Emergent gameplay** - Players discover speed thresholds naturally

---

## Tie-Breaker System

### Decision 5: Multi-Level Cascade

**CHOSEN: 7-Level Tie-Breaker Cascade**

When two characters have identical turn order speed, resolve ties using this priority order:

#### Level 1: Turn Order Speed (Primary)

```cpp
int32 TurnOrderA = CharDataA->WorldBody + CharDataA->GetSubStat(ESubStatType::TurnSpeed);
int32 TurnOrderB = CharDataB->WorldBody + CharDataB->GetSubStat(ESubStatType::TurnSpeed);

if (TurnOrderA != TurnOrderB)
    return (TurnOrderA > TurnOrderB) ? ActorA : ActorB;
```

**Range:** 0-28

---

#### Level 2: Attack Speed (Quick Reflexes)

```cpp
int32 AttackSpeedA = CharDataA->WorldBody + CharDataA->GetSubStat(ESubStatType::AttackSpeed);
int32 AttackSpeedB = CharDataB->WorldBody + CharDataB->GetSubStat(ESubStatType::AttackSpeed);

if (AttackSpeedA != AttackSpeedB)
{
    UE_LOG(LogCombat, Verbose, TEXT("Tie broken by AttackSpeed: %s (%d) vs %s (%d)"),
           *ActorA->GetName(), AttackSpeedA, *ActorB->GetName(), AttackSpeedB);
    return (AttackSpeedA > AttackSpeedB) ? ActorA : ActorB;
}
```

**Range:** 0-28

**Rationale:** Character with faster execution speed has quicker reflexes

**Example:**
```
Character A: Body 5, TurnSpeed 10, AttackSpeed 5 → Turn Order 15, Attack Speed 10
Character B: Body 5, TurnSpeed 10, AttackSpeed 8 → Turn Order 15, Attack Speed 13

Level 1: Turn Order tied (15 = 15)
Level 2: Attack Speed 10 vs 13 → B WINS
```

---

#### Level 3: Underdog Advantage

```cpp
int32 TotalStatsA = CharDataA->BaseMind + CharDataA->BaseBody + CharDataA->BaseSpirit;
int32 TotalStatsB = CharDataB->BaseMind + CharDataB->BaseBody + CharDataB->BaseSpirit;

if (TotalStatsA != TotalStatsB)
{
    UE_LOG(LogCombat, Verbose, TEXT("Tie broken by Underdog: %s (stats %d) vs %s (stats %d)"),
           *ActorA->GetName(), TotalStatsA, *ActorB->GetName(), TotalStatsB);
    return (TotalStatsA < TotalStatsB) ? ActorA : ActorB; // LOWER wins
}
```

**CRITICAL:** Lower total stats wins (underdog advantage)

**Rationale:**
- ✅ Helps balance asymmetric battles (3 low-level vs 1 high-level)
- ✅ Rewards creative low-stat speed builds
- ✅ Makes PvP more interesting (high-stat doesn't auto-win ties)
- ✅ Thematic: "Scrappy underdog strikes first"

---

#### Level 4: World Body (Physical Reflexes)

```cpp
if (CharDataA->WorldBody != CharDataB->WorldBody)
{
    UE_LOG(LogCombat, Verbose, TEXT("Tie broken by Body: %s (%d) vs %s (%d)"),
           *ActorA->GetName(), CharDataA->WorldBody, *ActorB->GetName(), CharDataB->WorldBody);
    return (CharDataA->WorldBody > CharDataB->WorldBody) ? ActorA : ActorB;
}
```

**Rationale:** Body determines base speed → higher Body = better reflexes

---

#### Level 5: World Mind (Tactical Awareness)

```cpp
if (CharDataA->WorldMind != CharDataB->WorldMind)
{
    UE_LOG(LogCombat, Verbose, TEXT("Tie broken by Mind: %s (%d) vs %s (%d)"),
           *ActorA->GetName(), CharDataA->WorldMind, *ActorB->GetName(), CharDataB->WorldMind);
    return (CharDataA->WorldMind > CharDataB->WorldMind) ? ActorA : ActorB;
}
```

**Rationale:** Mind = quick thinking = react faster in perfect tie

---

#### Level 6: World Spirit (Willpower/Determination)

```cpp
if (CharDataA->WorldSpirit != CharDataB->WorldSpirit)
{
    UE_LOG(LogCombat, Verbose, TEXT("Tie broken by Spirit: %s (%d) vs %s (%d)"),
           *ActorA->GetName(), CharDataA->WorldSpirit, *ActorB->GetName(), CharDataB->WorldSpirit);
    return (CharDataA->WorldSpirit > CharDataB->WorldSpirit) ? ActorA : ActorB;
}
```

**Rationale:** Spirit = force of will breaks the deadlock

---

#### Level 7: Team Position (Deterministic Fallback)

```cpp
// At this point, characters are IDENTICAL in all stats
bool AIsTeam1 = Team1Participants.Contains(ActorA);
bool BIsTeam1 = Team1Participants.Contains(ActorB);

if (AIsTeam1 != BIsTeam1)
{
    UE_LOG(LogCombat, Verbose, TEXT("Tie broken by Team: %s (Team%d) vs %s (Team%d)"),
           *ActorA->GetName(), AIsTeam1 ? 1 : 2, *ActorB->GetName(), BIsTeam1 ? 1 : 2);
    return AIsTeam1 ? ActorA : ActorB; // Team 1 advantage
}

// Both same team - use array position
const TArray<AActor*>& Team = AIsTeam1 ? Team1Participants : Team2Participants;
int32 IndexA = Team.IndexOfByKey(ActorA);
int32 IndexB = Team.IndexOfByKey(ActorB);

UE_LOG(LogCombat, Verbose, TEXT("Tie broken by Position: %s (pos %d) vs %s (pos %d)"),
       *ActorA->GetName(), IndexA, *ActorB->GetName(), IndexB);

return (IndexA < IndexB) ? ActorA : ActorB;
```

**Rationale:** Guaranteed deterministic result for testing

---

### Tie-Breaker Summary

| Level | Check | Winner | Rationale |
|-------|-------|--------|-----------|
| 1 | Turn Order Speed | Higher | Primary - who goes first |
| 2 | Attack Speed | Higher | Quick reflexes |
| 3 | Total Base Stats | **LOWER** | Underdog advantage |
| 4 | World Body | Higher | Physical reflexes |
| 5 | World Mind | Higher | Tactical awareness |
| 6 | World Spirit | Higher | Willpower |
| 7 | Team Position | Team 1 → Position | Deterministic |

---

### Example Scenarios

#### Scenario 1: Attack Speed Breaks Tie
```
Character A: Body 5, TurnSpeed 10, AttackSpeed 2 → Turn Order 15, Attack Speed 7
Character B: Body 4, TurnSpeed 11, AttackSpeed 4 → Turn Order 15, Attack Speed 8

Level 1: Turn Order tied (15 = 15)
Level 2: Attack Speed 7 vs 8 → B WINS
```

#### Scenario 2: Underdog Breaks Tie
```
Character A: Body 4, Mind 2, Spirit 1, TurnSpeed 8, AttackSpeed 8 → Turn Order 12, Total Stats 7
Character B: Body 5, Mind 4, Spirit 4, TurnSpeed 7, AttackSpeed 7 → Turn Order 12, Total Stats 13

Level 1: Turn Order tied (12 = 12)
Level 2: Attack Speed tied (12 = 12)
Level 3: Total Stats 7 vs 13 → A WINS (underdog!)
```

#### Scenario 3: Perfect Match - Goes to Position
```
Character A: Body 5, Mind 3, Spirit 2, TurnSpeed 5, AttackSpeed 5
Character B: Body 5, Mind 3, Spirit 2, TurnSpeed 5, AttackSpeed 5
Both Team 1

Level 1-6: All tied
Level 7: Position 0 vs Position 1 → A WINS
```

---

## Implementation Specifications

### Dead Character Handling

**CHOSEN: Skip Dead, Keep in Queue**

```cpp
void UTurnManager::AdvanceToNextActor()
{
    int32 SafetyCounter = 0;
    const int32 MaxIterations = TurnDebts.Num() * 2; // Prevent infinite loop
    
    do {
        // Find next actor with highest debt
        AActor* NextActor = CalculateNextActorByDebt();
        
        // Check if alive
        if (IsActorAlive(NextActor))
        {
            CurrentActor = NextActor;
            OnTurnChanged.Broadcast(CurrentActor, PreviousActor);
            return;
        }
        
        // Dead - mark turn as "skipped" but keep in debt system
        MarkTurnSkipped(NextActor);
        
        SafetyCounter++;
        
    } while (SafetyCounter < MaxIterations);
    
    // All actors dead - end combat
    EndCombat(nullptr);
}
```

**Rationale:**
- ✅ Resurrection spells simple: `SetActorAlive(true)` → rejoins turn order naturally
- ✅ Death doesn't mess up debt calculations
- ✅ Can show "ghosted" portraits in turn order UI
- ✅ No array manipulation needed

---

### Core Data Structures

#### FCombatantTurnDebt

```cpp
USTRUCT(BlueprintType)
struct FCombatantTurnDebt
{
    GENERATED_BODY()
    
    /** Actor in combat */
    UPROPERTY()
    AActor* Actor;
    
    /** Cached speed from CharacterData */
    UPROPERTY()
    float Speed;
    
    /** Accumulated turn debt based on speed ratio */
    UPROPERTY()
    float TurnsOwed;
    
    /** Number of turns this combatant has taken */
    UPROPERTY()
    int32 TurnsTaken;
    
    /** Whether this combatant is alive */
    UPROPERTY()
    bool bIsAlive;
    
    /** Calculate net debt (owed - taken) */
    float GetNetDebt() const
    {
        return TurnsOwed - static_cast<float>(TurnsTaken);
    }
};
```

---

### Turn Manager Class Structure

```cpp
UCLASS()
class WORLD_OF_REFRACTION_API UTurnManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ==================== COMBAT INITIALIZATION ====================
    
    /** Initialize combat between two teams */
    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    bool InitializeCombat(TArray<AActor*> Team1, TArray<AActor*> Team2);
    
    /** End current combat and reset state */
    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    void EndCombat(AActor* Winner = nullptr);
    
    // ==================== TURN MANAGEMENT ====================
    
    /** Get the actor whose turn it currently is */
    UFUNCTION(BlueprintPure, Category = "Turn Manager")
    AActor* GetCurrentActor() const { return CurrentActor; }
    
    /** Get the opponent of the specified actor */
    UFUNCTION(BlueprintPure, Category = "Turn Manager")
    AActor* GetOpponentOf(AActor* Actor) const;
    
    /** Advance to next actor's turn */
    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    void AdvanceToNextActor();
    
    /** End current actor's turn and advance */
    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    void EndCurrentTurn();
    
    // ==================== TURN ORDER CALCULATION ====================
    
    /** Initialize turn debt system */
    void InitializeTurnDebts();
    
    /** Recalculate turn debts (call after speed changes) */
    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    void RecalculateTurnDebts();
    
    /** Calculate next actor by debt */
    AActor* CalculateNextActorByDebt();
    
    /** Resolve speed tie between two actors */
    AActor* ResolveSpeedTie(AActor* ActorA, AActor* ActorB);
    
    // ==================== DEATH/RESURRECTION ====================
    
    /** Mark actor as dead (skipped in turn order) */
    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    void MarkActorDead(AActor* DeadActor);
    
    /** Mark actor as alive (rejoins turn order) */
    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    void MarkActorAlive(AActor* RevivedActor);
    
    /** Check if actor is alive */
    UFUNCTION(BlueprintPure, Category = "Turn Manager")
    bool IsActorAlive(AActor* Actor) const;
    
    // ==================== SPEED CHANGES ====================
    
    /** Called when an actor's speed changes (buff/debuff) */
    void OnSpeedChanged(AActor* AffectedActor);
    
    // ==================== STATE QUERIES ====================
    
    /** Check if combat is active */
    UFUNCTION(BlueprintPure, Category = "Turn Manager")
    bool IsCombatActive() const { return bCombatActive; }
    
    /** Get all combatants */
    UFUNCTION(BlueprintPure, Category = "Turn Manager")
    TArray<AActor*> GetAllCombatants() const;
    
    /** Get team members */
    UFUNCTION(BlueprintPure, Category = "Turn Manager")
    TArray<AActor*> GetTeamMembers(int32 TeamIndex) const;
    
    // ==================== DELEGATES ====================
    
    /** Broadcast when turn changes */
    UPROPERTY(BlueprintAssignable, Category = "Turn Manager")
    FOnTurnChanged OnTurnChanged;
    
    /** Broadcast when combat starts */
    UPROPERTY(BlueprintAssignable, Category = "Turn Manager")
    FOnCombatStarted OnCombatStarted;
    
    /** Broadcast when combat ends */
    UPROPERTY(BlueprintAssignable, Category = "Turn Manager")
    FOnCombatEnded OnCombatEnded;

private:
    // ==================== INTERNAL STATE ====================
    
    /** Turn debt tracking for all combatants */
    UPROPERTY()
    TArray<FCombatantTurnDebt> TurnDebts;
    
    /** Team 1 participants */
    UPROPERTY()
    TArray<AActor*> Team1Participants;
    
    /** Team 2 participants */
    UPROPERTY()
    TArray<AActor*> Team2Participants;
    
    /** Current actor whose turn it is */
    UPROPERTY()
    AActor* CurrentActor;
    
    /** Previous actor (for transition callbacks) */
    UPROPERTY()
    AActor* PreviousActor;
    
    /** Combat active flag */
    UPROPERTY()
    bool bCombatActive;
    
    // ==================== HELPER FUNCTIONS ====================
    
    /** Get CharacterDataComponent from actor */
    UCharacterDataComponent* GetCharacterDataComponent(AActor* Actor) const;
    
    /** Get turn order speed for actor */
    int32 GetTurnOrderSpeed(AActor* Actor) const;
    
    /** Get attack speed for actor */
    int32 GetAttackSpeed(AActor* Actor) const;
    
    /** Find slowest combatant's speed */
    float FindSlowestSpeed() const;
};
```

---

### Delegate Definitions

```cpp
/**
 * Delegate broadcast when turn changes
 * @param NewActor - Actor whose turn it now is
 * @param PreviousActor - Actor whose turn just ended (can be nullptr on first turn)
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTurnChanged, AActor*, NewActor, AActor*, PreviousActor);

/**
 * Delegate broadcast when combat starts
 * @param Team1 - First team
 * @param Team2 - Second team
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCombatStarted, TArray<AActor*>, Team1, TArray<AActor*>, Team2);

/**
 * Delegate broadcast when combat ends
 * @param Winner - Actor that won (nullptr if draw/cancelled)
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatEnded, AActor*, Winner);
```

---

---

## Status Effect Timing System

### Decision: Hybrid Processing with Separate Enum

**CHOSEN: Option B - Separate EStatusEffectTiming Enum**

#### Rationale

**Keep Existing:** EPassiveTrigger for passive abilities (already working)

**Add New:** EStatusEffectTiming for status effect processing timing

**Can Reference Both:** Status effects can use conditional triggers from EPassiveTrigger

---

### EStatusEffectTiming Enum

```cpp
UENUM(BlueprintType)
enum class EStatusEffectTiming : uint8
{
    Immediate       UMETA(DisplayName = "Immediate (One-Shot)"),
    StartOfOwnTurn  UMETA(DisplayName = "Start of Own Turn"),
    EndOfOwnTurn    UMETA(DisplayName = "End of Own Turn"),
    OnTrigger       UMETA(DisplayName = "Conditional (Uses EPassiveTrigger)"),
    Persistent      UMETA(DisplayName = "Persistent (Always Active)")
};
```

---

### Processing Rules

| Timing | When Processed | Use Cases | Examples |
|--------|----------------|-----------|----------|
| **Immediate** | Once when applied | One-shot effects | Instant damage, cleanse, dispel |
| **StartOfOwnTurn** | Start of affected actor's turn | Buffs, shields, preparations | Haste, Defense Up, Regeneration Shield |
| **EndOfOwnTurn** | End of affected actor's turn | DOTs, lingering damage | Poison, Burn, Bleed |
| **OnTrigger** | When condition met | Conditional effects | Activate when HP < 30% |
| **Persistent** | Always active | Stat modifiers, auras | +10% damage (permanent) |

---

### FStatusEffect Structure

```cpp
USTRUCT(BlueprintType)
struct FStatusEffect
{
    GENERATED_BODY()
    
    // ==================== IDENTITY ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FString EffectName;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    int32 EffectID; // Unique identifier
    
    // ==================== TIMING ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    EStatusEffectTiming ProcessTiming;
    
    /** If ProcessTiming = OnTrigger, specify the condition using EPassiveTrigger */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
              meta = (EditCondition = "ProcessTiming == EStatusEffectTiming::OnTrigger"))
    EPassiveTrigger TriggerCondition;
    
    /** Threshold value for conditional triggers (HP%, Energy%, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
              meta = (ClampMin = "0", ClampMax = "100",
                      EditCondition = "TriggerCondition == EPassiveTrigger::OnHPBelowThreshold || TriggerCondition == EPassiveTrigger::OnHPAboveThreshold || TriggerCondition == EPassiveTrigger::OnEnergyBelowThreshold || TriggerCondition == EPassiveTrigger::OnEnergyAboveThreshold"))
    float TriggerThreshold = 30.0f;
    
    // ==================== DURATION ====================
    
    /** Number of affected actor's turns remaining */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duration")
    int32 RemainingTurns;
    
    /** Effect never expires */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duration")
    bool bPermanent = false;
    
    /** Tick duration on owner's turn (not global turn) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Duration")
    bool bTickOnOwnerTurn = true; // Always true
    
    // ==================== EFFECT DATA ====================
    
    /** Type of effect (uses existing EAbilityEffectType) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    EAbilityEffectType EffectType;
    
    /** Effect magnitude/value */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    float EffectValue;
    
    /** Element associated with effect */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    ERefractionElement Element;
    
    // ==================== STACKING ====================
    
    /** Can multiple instances of this effect stack? */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stacking")
    bool bCanStack = false;
    
    /** Current number of stacks */
    UPROPERTY(BlueprintReadOnly, Category = "Stacking")
    int32 CurrentStacks = 1;
    
    /** Maximum allowed stacks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stacking",
              meta = (EditCondition = "bCanStack", ClampMin = "1", ClampMax = "10"))
    int32 MaxStacks = 3;
    
    // ==================== SOURCE TRACKING ====================
    
    /** Actor who applied this effect */
    UPROPERTY(BlueprintReadOnly, Category = "Source")
    AActor* SourceActor;
    
    /** Spell/Ability that applied this effect (for tracking) */
    UPROPERTY(BlueprintReadOnly, Category = "Source")
    FString SourceAbility;
};
```

---

### Duration Tracking Rules

**Confirmed:** Durations tick on affected actor's turn (not global turn count)

**Example:**
```
Turn 1 (Player): Apply Haste to Player (3 turns)
Turn 2 (Enemy): Player still has Haste (3 turns remaining)
Turn 3 (Player): Haste processes → 2 turns remaining
Turn 4 (Enemy): Player still has Haste (2 turns remaining)
Turn 5 (Player): Haste processes → 1 turn remaining
Turn 6 (Enemy): Player still has Haste (1 turn remaining)
Turn 7 (Player): Haste processes → 0 turns remaining, REMOVED
```

**Rationale:**
- ✅ Intuitive: "3 turn buff" = benefit for 3 of MY turns
- ✅ Fair: All characters get equal duration regardless of speed
- ✅ Predictable: Players understand duration clearly

---

### Integration with Existing Systems

#### TurnManager Events

```cpp
// TurnManager broadcasts these events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTurnStarted, AActor*, Actor, int32, TurnNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTurnEnded, AActor*, Actor, int32, TurnNumber);

// StatusEffectManager subscribes to these
void UTurnManager::OnTurnStarted_Internal(AActor* Actor, int32 TurnNumber)
{
    CurrentActor = Actor;
    GlobalTurnCount++;
    
    // Broadcast to listeners
    OnTurnStarted.Broadcast(Actor, TurnNumber);
    // → StatusEffectManager processes StartOfOwnTurn effects
}

void UTurnManager::OnTurnEnded_Internal(AActor* Actor, int32 TurnNumber)
{
    // Broadcast to listeners
    OnTurnEnded.Broadcast(Actor, TurnNumber);
    // → StatusEffectManager processes EndOfOwnTurn effects
    // → StatusEffectManager ticks durations
    
    PreviousActor = Actor;
    AdvanceToNextActor();
}
```

#### StatusEffectManager Processing

```cpp
void UStatusEffectManager::Initialize(ACombatOrchestrator* Orchestrator)
{
    // Subscribe to TurnManager events
    UTurnManager* TM = GetGameInstance()->GetSubsystem<UTurnManager>();
    TM->OnTurnStarted.AddDynamic(this, &UStatusEffectManager::OnTurnStarted);
    TM->OnTurnEnded.AddDynamic(this, &UStatusEffectManager::OnTurnEnded);
}

void UStatusEffectManager::OnTurnStarted(AActor* Actor, int32 TurnNumber)
{
    // Process StartOfOwnTurn effects
    ProcessEffectsWithTiming(Actor, EStatusEffectTiming::StartOfOwnTurn);
    
    // Check conditional triggers
    CheckTriggerConditions(Actor);
}

void UStatusEffectManager::OnTurnEnded(AActor* Actor, int32 TurnNumber)
{
    // Process EndOfOwnTurn effects
    ProcessEffectsWithTiming(Actor, EStatusEffectTiming::EndOfOwnTurn);
    
    // Tick durations for this actor
    TickDurations(Actor);
}

void UStatusEffectManager::ProcessEffectsWithTiming(AActor* Actor, EStatusEffectTiming Timing)
{
    if (!ActiveEffects.Contains(Actor)) return;
    
    TArray<FStatusEffect>& Effects = ActiveEffects[Actor];
    
    for (FStatusEffect& Effect : Effects)
    {
        if (Effect.ProcessTiming == Timing)
        {
            // Apply effect
            ApplyEffectLogic(Actor, Effect);
            
            // Broadcast event for UI/feedback
            OnEffectTriggered.Broadcast(Actor, Effect);
        }
    }
}

void UStatusEffectManager::TickDurations(AActor* Actor)
{
    if (!ActiveEffects.Contains(Actor)) return;
    
    TArray<FStatusEffect>& Effects = ActiveEffects[Actor];
    
    // Tick down durations (iterate backwards for safe removal)
    for (int32 i = Effects.Num() - 1; i >= 0; --i)
    {
        FStatusEffect& Effect = Effects[i];
        
        if (Effect.bPermanent) continue; // Skip permanent effects
        
        Effect.RemainingTurns--;
        
        if (Effect.RemainingTurns <= 0)
        {
            // Effect expired
            OnEffectRemoved.Broadcast(Actor, Effect);
            Effects.RemoveAt(i);
        }
    }
}
```

---

### Effect Examples

#### Buff (Start of Turn)

```cpp
// Haste Buff
FStatusEffect HasteEffect;
HasteEffect.EffectName = TEXT("Haste");
HasteEffect.ProcessTiming = EStatusEffectTiming::StartOfOwnTurn;
HasteEffect.EffectType = EAbilityEffectType::SpeedBuff;
HasteEffect.EffectValue = 10.0f; // +10 speed
HasteEffect.RemainingTurns = 3;
HasteEffect.Element = ERefractionElement::Wind;

// Applied at start of turn → Speed buff active for action
```

#### DOT (End of Turn)

```cpp
// Poison DOT
FStatusEffect PoisonEffect;
PoisonEffect.EffectName = TEXT("Poison");
PoisonEffect.ProcessTiming = EStatusEffectTiming::EndOfOwnTurn;
PoisonEffect.EffectType = EAbilityEffectType::DamageOverTime;
PoisonEffect.EffectValue = 15.0f; // 15 damage per turn
PoisonEffect.RemainingTurns = 4;
PoisonEffect.Element = ERefractionElement::Earth;

// Applied at end of turn → Damage after acting
```

#### Conditional (Trigger-Based)

```cpp
// Adrenaline Rush (activates when HP < 30%)
FStatusEffect AdrenalineEffect;
AdrenalineEffect.EffectName = TEXT("Adrenaline Rush");
AdrenalineEffect.ProcessTiming = EStatusEffectTiming::OnTrigger;
AdrenalineEffect.TriggerCondition = EPassiveTrigger::OnHPBelowThreshold;
AdrenalineEffect.TriggerThreshold = 30.0f;
AdrenalineEffect.EffectType = EAbilityEffectType::AttackBuff;
AdrenalineEffect.EffectValue = 25.0f; // +25% damage
AdrenalineEffect.bPermanent = true; // Lasts until healed

// Checked every turn, activates when condition met
```

#### Persistent (Stat Modifier)

```cpp
// Permanent Strength Enhancement
FStatusEffect StrengthEffect;
StrengthEffect.EffectName = TEXT("Strength Enhancement");
StrengthEffect.ProcessTiming = EStatusEffectTiming::Persistent;
StrengthEffect.EffectType = EAbilityEffectType::AttackBuff;
StrengthEffect.EffectValue = 15.0f; // +15% damage
StrengthEffect.bPermanent = true;
StrengthEffect.Element = ERefractionElement::Fire;

// Always active, never removed (equipment bonus, etc.)
```

---

### Unification with EPassiveTrigger

**Status effects can reference EPassiveTrigger for conditional activation:**

```cpp
// Effect that activates when landing a critical hit
FStatusEffect CritEffect;
CritEffect.ProcessTiming = EStatusEffectTiming::OnTrigger;
CritEffect.TriggerCondition = EPassiveTrigger::OnCrit; // ← Reuses existing enum!
CritEffect.EffectType = EAbilityEffectType::DamageBuff;
CritEffect.EffectValue = 50.0f;
CritEffect.RemainingTurns = 1; // Buff for 1 turn after crit

// Effect that activates when casting a spell
FStatusEffect SpellEffect;
SpellEffect.ProcessTiming = EStatusEffectTiming::OnTrigger;
SpellEffect.TriggerCondition = EPassiveTrigger::OnSpellCast; // ← Reuses existing!
SpellEffect.EffectType = EAbilityEffectType::CostReduction;
SpellEffect.EffectValue = 20.0f;
SpellEffect.RemainingTurns = 2;
```

**Benefits:**
- ✅ Reuses existing trigger infrastructure
- ✅ Passive abilities and status effects share event system
- ✅ No duplication of trigger logic
- ✅ Designers familiar with one system understand the other

---

## Real-Time Defense System

### Combat Model: Turn-Based Offense + Real-Time Defense

**CRITICAL:** World of Refraction uses a hybrid combat system inspired by Expedition 33:
- **Offensive Phase:** Turn-based action selection (player chooses spell/ability/item)
- **Defensive Phase:** Real-time reaction during opponent's attack animation

This creates skill-based gameplay where turn order determines WHO acts, but real-time reactions determine HOW MUCH damage is dealt.

---

### Defense Mechanics

#### Three Defense Options

**1. Block - Safe Mitigation**
```cpp
// Always reduces damage by 50%, regardless of attack size
float BlockReduction = 0.50f;
FinalDamage = IncomingDamage * (1.0f - BlockReduction);

// Works against any attack (melee, spell, item, AOE)
// Safe option but doesn't negate damage completely
// No additional benefits beyond reduction
```

**Use Case:** Safe, reliable damage reduction for any attack

---

**2. Parry - High Risk/High Reward**
```cpp
// Reduces damage by 70% AND reflects 30% back to attacker
float ParryReduction = 0.70f;
float ParryReflection = 0.30f;

FinalDamage = IncomingDamage * (1.0f - ParryReduction);
ReflectedDamage = IncomingDamage * ParryReflection;

// Tighter timing window than Block
// Best reward but requires precision
```

**Use Case:** Skilled defense with counter-attack potential

---

**3. Dodge - Complete Avoidance (If Possible)**
```cpp
// Complete avoidance IF attack size allows
bool CanDodge = (AttackHitboxSize < DodgeThreshold);

if (CanDodge)
{
    FinalDamage = 0; // 100% avoidance
}
else
{
    // Attack too big, dodge fails completely
    FinalDamage = IncomingDamage; // Full damage
}

// Player can dodge left or right
// Direction may matter in future (directional attacks)
```

**Dodge Threshold Calculation:**
```cpp
float PlayerHitbox = 1.0f; // Character collision size
float DodgeDistance = 1.5f; // Movement range when dodging
float DodgeThreshold = PlayerHitbox + DodgeDistance; // 2.5f total

// Attack smaller than 2.5 → Can dodge
// Attack larger than 2.5 → Cannot dodge (must Block/Parry)
```

**Use Case:** Best defense against small/medium attacks, useless against large/AOE

---

### Defense Window Timing

**Animation-Driven Windows:**

Defense windows are embedded in attack animations and vary per action:

```cpp
USTRUCT(BlueprintType)
struct FDefenseWindow
{
    GENERATED_BODY()
    
    /** When in animation does window open? (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WindowStartTime = 0.5f;
    
    /** How long is window open? (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WindowDuration = 0.3f; // Tight window for high skill requirement
    
    /** Visual/audio cue timing (before window opens) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CueTime = 0.4f; // 0.1s warning before window
};
```

**Design Philosophy: High Risk/High Reward**
- Windows are 0.3-0.5 seconds (tight timing)
- Success = significant benefit (50-100% damage reduction)
- Failure = full damage (no partial credit)
- Creates skill-based defense ceiling

---

**Example Timings:**

```cpp
// Fast Attack (Quick Strike)
Animation Duration: 1.0 second
Visual Cue: 0.3s (flash/sound)
Defense Window: 0.4s - 0.7s (0.3s duration)
→ Quick reaction needed, less warning

// Medium Attack (Fireball Spell)
Animation Duration: 2.0 seconds
Visual Cue: 1.3s (fireball charges in hand)
Defense Window: 1.4s - 1.7s (0.3s duration)
→ More warning time, same tight window

// Slow Attack (Heavy Swing)
Animation Duration: 3.0 seconds
Visual Cue: 2.3s (weapon raised high)
Defense Window: 2.4s - 2.9s (0.5s duration)
→ Long telegraph, slightly more forgiving

// Garnet Crystal Throw (Item)
Animation Duration: 1.5 seconds
Visual Cue: 0.8s (crystal glows in hand)
Defense Window: 0.9s - 1.2s (0.3s duration)
→ Items have defense windows too!
```

---

### Failed Defense Consequences

**Confirmed: Option A (Full Damage)**

```cpp
if (!DefenseSuccessful || DefenseChoice == EDefenseType::None)
{
    FinalDamage = IncomingDamage; // 100% damage, no reduction
}
```

**Rationale:**
- Clear success/fail feedback
- Incentivizes learning attack patterns
- No "partial credit" keeps skill ceiling high
- Failed dodge against large attack = same as not trying

---

### 3v3 Multi-Target Defense

**Confirmed: Sequential Defense Windows (Option B)**

When attacks hit multiple targets, each character gets their own defense opportunity:

```cpp
void UActionExecutor::ExecuteAOEAttack(AActor* Attacker, TArray<AActor*> Targets)
{
    // Play attack animation once
    PlayAnimation(Attacker, AttackAnimation);
    
    // Schedule defense windows for each target (staggered)
    for (int32 i = 0; i < Targets.Num(); i++)
    {
        AActor* Target = Targets[i];
        
        // Stagger hits by 0.5 seconds per target
        float HitTime = BaseHitTime + (i * 0.5f);
        
        // Each target gets individual defense window
        ScheduleDefenseWindow(Target, HitTime);
    }
}
```

**Example Flow:**
```
Enemy casts AOE Fireball at 3 player characters:

Time 0.0s: Enemy begins cast animation
Time 1.5s: Fireball releases from hand
Time 2.0s: Hits Player 1 → [Player 1 Defense Window: 0.3s]
Time 2.5s: Hits Player 2 → [Player 2 Defense Window: 0.3s]
Time 3.0s: Hits Player 3 → [Player 3 Defense Window: 0.3s]

Each player defends independently
Results stored per player
Damage applied individually based on defense success
```

**Rationale:**
- Maintains individual skill expression in team battles
- Creates tension (multiple quick reactions needed)
- Fair (each character gets opportunity to defend)
- Scalable (works for any team size)

---

### Defense State Tracking

```cpp
UENUM(BlueprintType)
enum class EDefenseType : uint8
{
    None    UMETA(DisplayName = "No Defense Attempted"),
    Block   UMETA(DisplayName = "Block"),
    Parry   UMETA(DisplayName = "Parry"),
    Dodge   UMETA(DisplayName = "Dodge")
};

USTRUCT()
struct FDefenseState
{
    GENERATED_BODY()
    
    /** Actor being attacked */
    UPROPERTY()
    AActor* Defender;
    
    /** Size of incoming attack */
    UPROPERTY()
    float AttackSize;
    
    /** Is defense window currently open? */
    UPROPERTY()
    bool bWindowOpen;
    
    /** What defense did player choose? */
    UPROPERTY()
    EDefenseType DefenseChosen;
    
    /** Did player press button within window? */
    UPROPERTY()
    bool bSuccessfulTiming;
};
```

---

## Infusion Systems

### Overview: Three Distinct Infusion Mechanics

World of Refraction features three separate infusion systems that enhance spells and abilities:

1. **Spell Infusion** (Casters Only) - Charge spells to increase size
2. **Ability Power Infusion** (Generic Class Only) - Charge abilities to increase damage
3. **Ability Element Infusion** (Casters Only) - Toggle element on abilities (existing system)

---

### System 1: Spell Infusion (Casters Only)

**Who Can Use:** Spell casters ONLY (Generic class cannot)

**What It Does:** Increases spell size by charging energy into the spell

**Mechanic:** Hold button while selecting spell from radial menu
- Keep holding → Spell charges → Costs more energy → Becomes bigger
- Release at desired level → Spell executes

**Levels:**
```cpp
UENUM(BlueprintType)
enum class ESpellInfusionLevel : uint8
{
    Normal          UMETA(DisplayName = "Normal (No Infusion)"),
    Infused         UMETA(DisplayName = "Infused (+50% Size)"),
    FullyInfused    UMETA(DisplayName = "Fully Infused (+100% Size)")
};
```

---

#### Spell Size Calculation

**Two Components:**

1. **Base Size** - From AbilitySize stat (Spirit substat, 0-21 points possible)
2. **Infusion Multiplier** - Player's charging choice

```cpp
float CalculateFinalSpellSize(UCharacterData* CharData, ESpellInfusionLevel InfusionLevel)
{
    // Base size from character stat
    float BaseSize = CharData->GetSubStat(ESubStatType::AbilitySize);
    
    // Infusion multiplier
    float Multiplier = 1.0f;
    switch (InfusionLevel)
    {
        case ESpellInfusionLevel::Normal:
            Multiplier = 1.0f; // No change
            break;
        case ESpellInfusionLevel::Infused:
            Multiplier = 1.5f; // 50% bigger
            break;
        case ESpellInfusionLevel::FullyInfused:
            Multiplier = 2.0f; // 100% bigger (doubled)
            break;
    }
    
    return BaseSize * Multiplier;
}
```

---

#### Spell Infusion Scaling

```cpp
USTRUCT(BlueprintType)
struct FSpellInfusionData
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EnergyCostMultiplier = 1.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SizeMultiplier = 1.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ChargeTimeRequired = 0.0f; // Seconds to hold button
};

// Scaling values
Normal:         Cost ×1.0,  Size ×1.0,  Charge: 0.0s
Infused:        Cost ×1.3,  Size ×1.5,  Charge: 1.0s
Fully Infused:  Cost ×1.6,  Size ×2.0,  Charge: 2.0s
```

---

#### Spell Infusion Examples

**Low AbilitySize Build (3 points):**
```
Normal Cast:        Size = 3 × 1.0 = 3.0
                    Energy = BaseCost × 1.0
                    
Infused:            Size = 3 × 1.5 = 4.5 (+50%)
                    Energy = BaseCost × 1.3 (+30%)
                    
Fully Infused:      Size = 3 × 2.0 = 6.0 (+100%)
                    Energy = BaseCost × 1.6 (+60%)
```

**High AbilitySize Build (21 points):**
```
Normal Cast:        Size = 21 × 1.0 = 21
                    Energy = BaseCost × 1.0
                    
Infused:            Size = 21 × 1.5 = 31.5 (+50%)
                    Energy = BaseCost × 1.3 (+30%)
                    
Fully Infused:      Size = 21 × 2.0 = 42 (+100%)
                    Energy = BaseCost × 1.6 (+60%)
                    → MASSIVE spell, nearly impossible to dodge!
```

---

#### Spell Size Impact on Defense

**Size affects dodge viability:**

```cpp
float DodgeThreshold = 2.5f; // Player hitbox + dodge distance

bool CanDodge(float SpellSize)
{
    return SpellSize < DodgeThreshold;
}

// Examples:
Fireball (Size 1.5) < 2.5 → ✅ Can dodge
Fireball Infused (Size 2.25) < 2.5 → ✅ Can dodge (barely)
Fireball Fully Infused (Size 3.0) > 2.5 → ❌ Cannot dodge (must Block/Parry)
Massive AOE (Size 42) >> 2.5 → ❌ Completely undodgeable
```

**Block and Parry effectiveness:** UNCHANGED by spell size
- Block always reduces 50%
- Parry always reduces 70% + reflects 30%
- Size only affects whether dodge is possible

---

#### Strategic Implications

**Character Building:**
- High AbilitySize investment → Massive infused spells
- Low AbilitySize investment → Smaller spells even when infused
- Trade-off: AbilitySize vs EffectDamage/Resistance points

**Combat Decisions:**
- Normal cast: Energy efficient, dodgeable
- Infused: Costs more, harder to dodge
- Fully Infused: Very expensive, often undodgeable
- Risk/reward: Spend energy to deny opponent's best defense option

---

### System 2: Ability Power Infusion (Generic Class Only)

**Who Can Use:** Generic class ONLY (physical/melee characters)

**What It Does:** Increases ability damage and status effect strength

**Mechanic:** Hold button while selecting ability from radial menu
- Keep holding → Ability charges → Costs more energy → Becomes stronger
- Release at desired level → Ability executes

**Why Generic Only:**
- Generic class has no innate element
- Cannot use Ability Element Infusion (casters' mechanic)
- Power Infusion is their unique enhancement system
- Represents physical conditioning/intensity

---

#### Ability Power Calculation

**Two Components:**

1. **RawDamage Stat** - Body substat, affects physical damage
2. **EffectDamage Stat** - Spirit substat, affects status buildup

```cpp
float CalculateFinalAbilityDamage(UCharacterData* CharData, 
                                    UAbilityData* Ability,
                                    EAbilityInfusionLevel InfusionLevel)
{
    // Base damage from ability
    float BaseDamage = Ability->CalculateDamage(CharData);
    
    // RawDamage stat bonus
    int32 RawDamageBonus = CharData->GetSubStat(ESubStatType::RawDamage);
    float RawDamageMultiplier = 1.0f + (RawDamageBonus * 0.05f); // +5% per point
    
    // Infusion multiplier
    float InfusionMultiplier = 1.0f;
    switch (InfusionLevel)
    {
        case EAbilityInfusionLevel::Normal:
            InfusionMultiplier = 1.0f;
            break;
        case EAbilityInfusionLevel::Infused:
            InfusionMultiplier = 1.3f; // +30% damage
            break;
        case EAbilityInfusionLevel::FullyInfused:
            InfusionMultiplier = 1.6f; // +60% damage
            break;
    }
    
    return BaseDamage * RawDamageMultiplier * InfusionMultiplier;
}

float CalculateStatusEffectStrength(UCharacterData* CharData,
                                     EAbilityInfusionLevel InfusionLevel)
{
    // EffectDamage stat
    int32 EffectDamageBonus = CharData->GetSubStat(ESubStatType::EffectDamage);
    
    // Infusion multiplier
    float InfusionMultiplier = 1.0f;
    switch (InfusionLevel)
    {
        case EAbilityInfusionLevel::Normal:
            InfusionMultiplier = 1.0f;
            break;
        case EAbilityInfusionLevel::Infused:
            InfusionMultiplier = 1.3f; // +30% status
            break;
        case EAbilityInfusionLevel::FullyInfused:
            InfusionMultiplier = 1.6f; // +60% status
            break;
    }
    
    return EffectDamageBonus * InfusionMultiplier;
}
```

---

#### Ability Power Infusion Scaling

```cpp
UENUM(BlueprintType)
enum class EAbilityInfusionLevel : uint8
{
    Normal          UMETA(DisplayName = "Normal (No Infusion)"),
    Infused         UMETA(DisplayName = "Infused (+30% Power)"),
    FullyInfused    UMETA(DisplayName = "Fully Infused (+60% Power)")
};

USTRUCT(BlueprintType)
struct FAbilityInfusionData
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EnergyCostMultiplier = 1.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DamageMultiplier = 1.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StatusMultiplier = 1.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ChargeTimeRequired = 0.0f;
};

// Scaling values
Normal:         Cost ×1.0,  Damage ×1.0,  Status ×1.0,  Charge: 0.0s
Infused:        Cost ×1.3,  Damage ×1.3,  Status ×1.3,  Charge: 1.0s
Fully Infused:  Cost ×1.6,  Damage ×1.6,  Status ×1.6,  Charge: 2.0s
```

---

#### Ability Power Infusion Examples

**High RawDamage Build (15 points):**
```
Normal Ability:     Damage = Base × (1 + 15×0.05) × 1.0 = Base × 1.75
                    Energy = BaseCost × 1.0
                    
Infused:            Damage = Base × 1.75 × 1.3 = Base × 2.275 (+30%)
                    Energy = BaseCost × 1.3 (+30%)
                    
Fully Infused:      Damage = Base × 1.75 × 1.6 = Base × 2.8 (+60%)
                    Energy = BaseCost × 1.6 (+60%)
```

**High EffectDamage Build (15 points):**
```
Normal Ability:     Status = 15 × 1.0 = 15 buildup
                    Energy = BaseCost × 1.0
                    
Infused:            Status = 15 × 1.3 = 19.5 buildup (+30%)
                    Energy = BaseCost × 1.3 (+30%)
                    
Fully Infused:      Status = 15 × 1.6 = 24 buildup (+60%)
                    Energy = BaseCost × 1.6 (+60%)
```

---

#### Strategic Implications

**Character Building:**
- RawDamage investment → Burst damage potential
- EffectDamage investment → Status application specialist
- Balanced build → Moderate both

**Combat Decisions:**
- Normal ability: Energy efficient
- Infused: Meaningful power spike
- Fully Infused: Devastating but expensive
- Use for finishing blows or when energy abundant

---

### System 3: Ability Element Infusion (Casters Only - Existing)

**Who Can Use:** Spell casters ONLY (characters with innate elements)

**What It Does:** Adds character's innate element to ability (already implemented)

**Mechanic:** Toggle on/off (NO charging, instant decision)
- Toggle ON → Ability gains element, damage penalty, status buildup
- Toggle OFF → Normal ability execution

**From AbilityData.h (Existing System):**
```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite)
bool bSupportsInfusion = false; // Can this ability be element-infused?

// When infused:
// - Damage: -30% penalty
// - Energy Cost: +50%
// - Status Buildup: Applies elemental status effects
// - Element: Uses character's innate element
```

---

#### Element Infusion Execution

```cpp
void UActionExecutor::ExecuteAbility_ElementInfused(AActor* Caster, 
                                                     UAbilityData* Ability,
                                                     TArray<AActor*> Targets,
                                                     bool bInfused)
{
    UCharacterDataComponent* CasterComp = Caster->FindComponentByClass<UCharacterDataComponent>();
    UCharacterData* CasterData = CasterComp->CharacterData;
    
    // Calculate base values
    int32 BaseEnergyCost = Ability->CalculateEnergyCost(CasterData);
    int32 BaseDamage = Ability->CalculateDamage(CasterData);
    
    // Apply infusion modifiers if toggled on
    float EnergyCostMultiplier = 1.0f;
    float DamagePenalty = 1.0f;
    bool bApplyStatusBuildup = false;
    ERefractionElement InfusedElement = ERefractionElement::Generic;
    
    if (bInfused)
    {
        EnergyCostMultiplier = 1.5f; // +50% energy cost
        DamagePenalty = 0.7f; // -30% damage
        bApplyStatusBuildup = true;
        InfusedElement = CasterData->InnateElement; // Character's element
    }
    
    // Calculate final values
    int32 FinalEnergyCost = BaseEnergyCost * EnergyCostMultiplier;
    int32 FinalDamage = BaseDamage * DamagePenalty;
    
    // Deduct energy
    CasterComp->ServerSpendEnergy(FinalEnergyCost);
    
    // Execute ability with element
    PlayAbilityAnimation(Caster, Ability, InfusedElement);
    
    // Apply to targets
    for (AActor* Target : Targets)
    {
        // Apply damage
        StartDefenseSequence(Caster, Target, Ability, FinalDamage);
        
        // Apply elemental status buildup
        if (bApplyStatusBuildup)
        {
            StatusEffectManager->ApplyStatusBuildup(Target, InfusedElement, Ability->StatusBuildupAmount);
        }
    }
}
```

---

#### Element Infusion Trade-offs

```
NOT Infused:
✅ Full damage (100%)
✅ Normal energy cost
❌ No elemental status effects
❌ Generic/neutral damage type

Element Infused:
❌ Reduced damage (70%, -30% penalty)
❌ Higher energy cost (+50%)
✅ Applies elemental status buildup
✅ Elemental damage (Fire/Water/etc.)
✅ Can trigger elemental reactions
```

**Use Cases:**
- Infuse when: Want to apply status effects, trigger reactions, exploit weaknesses
- Don't infuse when: Need maximum damage, low on energy, target resists element

---

### Infusion Systems Summary

| System | Class | Mechanic | Levels | Affects | Trade-off |
|--------|-------|----------|--------|---------|-----------|
| **Spell Infusion** | Casters | Hold to charge | 2 (1.5x, 2.0x size) | AbilitySize stat | Energy cost vs dodge denial |
| **Ability Power** | Generic | Hold to charge | 2 (1.3x, 1.6x power) | RawDamage + EffectDamage | Energy cost vs burst damage |
| **Ability Element** | Casters | Toggle on/off | 1 (binary) | Adds element | Damage penalty vs status/reactions |

**Key Design:**
- Each class has unique enhancement options
- All involve resource management (energy cost)
- All create meaningful combat decisions
- Generic gets raw power, Casters get utility/control

---

## Action Execution Flow

### Execution Pattern: Asynchronous Event-Driven

**Architecture:** Non-blocking, callback-based execution for multiplayer compatibility

---

### Turn Sequence

```cpp
// 1. TURN START
void ACombatOrchestrator::OnTurnStarted(AActor* Actor, int32 TurnNumber)
{
    CurrentActor = Actor;
    
    // Process start-of-turn status effects
    StatusEffectManager->ProcessStartOfTurnEffects(Actor);
    
    // Check if actor died from DOT
    if (IsActorDead(Actor))
    {
        TurnManager->EndCurrentTurn();
        return;
    }
    
    // Request action (async)
    RequestActionFromActor(Actor);
}

// 2. ACTION REQUEST
void ACombatOrchestrator::RequestActionFromActor(AActor* Actor)
{
    AController* Controller = Cast<APawn>(Actor)->GetController();
    
    if (Cast<AAIController>(Controller))
    {
        // AI decision (async)
        AIDecisionManager->MakeDecisionAsync(Actor, 
            FOnDecisionMade::CreateUObject(this, &ACombatOrchestrator::OnActionReceived, Actor));
    }
    else if (Cast<APlayerController>(Controller))
    {
        // Show UI (async, waits for player input)
        BattleUIManager->ShowActionMenu(Actor);
        // UI will call OnActionReceived via delegate when player chooses
    }
}

// 3. ACTION RECEIVED
void ACombatOrchestrator::OnActionReceived(AActor* Actor, FAction Action)
{
    // Validate action
    FActionValidationResult ValidationResult = ActionExecutor->ValidateAction(Actor, Action);
    
    if (!ValidationResult.bIsValid)
    {
        // Show error and retry
        BattleUIManager->ShowErrorMessage(ValidationResult.ErrorMessage);
        RequestActionFromActor(Actor);
        return;
    }
    
    // Execute action (async)
    ActionExecutor->ExecuteActionAsync(Actor, Action,
        FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::OnActionCompleted, Actor));
}

// 4. ACTION COMPLETED
void ACombatOrchestrator::OnActionCompleted(AActor* Actor, const FActionResult& Result)
{
    // Process end-of-turn status effects
    StatusEffectManager->ProcessEndOfTurnEffects(Actor);
    
    // Check win condition
    if (CheckWinCondition())
    {
        EndCombat(GetVictor());
        return;
    }
    
    // Advance turn
    TurnManager->EndCurrentTurn();
    // → Loops back to OnTurnStarted for next actor
}
```

---

### Action Validation

**Centralized in ActionExecutor:**

```cpp
struct FActionValidationResult
{
    bool bIsValid;
    FString ErrorMessage; // Shown to player if invalid
};

FActionValidationResult UActionExecutor::ValidateAction(AActor* Actor, const FAction& Action)
{
    UCharacterDataComponent* Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
    
    // Check energy cost
    int32 EnergyCost = CalculateActionEnergyCost(Actor, Action);
    if (Comp->CurrentEP < EnergyCost)
    {
        return { false, TEXT("Not enough energy") };
    }
    
    // Check cooldowns (ultimates, etc.)
    if (IsOnCooldown(Actor, Action))
    {
        return { false, TEXT("Still on cooldown") };
    }
    
    // Check requirements (world stat requirements)
    if (!MeetsRequirements(Actor, Action))
    {
        return { false, TEXT("Requirements not met") };
    }
    
    // Check blocking status effects (Silenced, Stunned, etc.)
    if (HasBlockingStatus(Actor, Action))
    {
        return { false, TEXT("Cannot act (status effect)") };
    }
    
    // Check target validity (alive, in range, etc.)
    if (!AreTargetsValid(Action.Targets))
    {
        return { false, TEXT("Invalid targets") };
    }
    
    return { true, TEXT("") };
}
```

---

### Spell Execution with Defense Windows

```cpp
void UActionExecutor::ExecuteSpell(AActor* Caster, USpellData* Spell, 
                                    TArray<AActor*> Targets, 
                                    ESpellInfusionLevel InfusionLevel)
{
    UCharacterDataComponent* CasterComp = Caster->FindComponentByClass<UCharacterDataComponent>();
    UCharacterData* CasterData = CasterComp->CharacterData;
    
    // Get infusion data
    FSpellInfusionData InfusionData = GetSpellInfusionData(InfusionLevel);
    
    // Calculate energy cost with infusion
    int32 BaseEnergyCost = Spell->CalculateEnergyCost(CasterData);
    int32 FinalEnergyCost = BaseEnergyCost * InfusionData.EnergyCostMultiplier;
    
    // Deduct energy
    CasterComp->ServerSpendEnergy(FinalEnergyCost);
    
    // Calculate final spell size
    float BaseAbilitySize = CasterData->GetSubStat(ESubStatType::AbilitySize);
    float FinalSpellSize = BaseAbilitySize * InfusionData.SizeMultiplier;
    
    // Play animation with size scaling
    PlaySpellAnimation(Caster, Spell, FinalSpellSize);
    
    // Spawn VFX with correct size
    SpawnSpellVFX(Caster, Spell, FinalSpellSize);
    
    // For each target, schedule defense sequence (async)
    for (int32 i = 0; i < Targets.Num(); i++)
    {
        AActor* Target = Targets[i];
        
        // Stagger hits for multiple targets
        float HitDelay = i * 0.5f;
        
        FTimerHandle HitTimer;
        GetWorld()->GetTimerManager().SetTimer(HitTimer,
            [this, Caster, Target, Spell, FinalSpellSize]()
            {
                StartDefenseSequence(Caster, Target, Spell, FinalSpellSize);
            },
            HitDelay,
            false
        );
    }
}
```

---

### Defense Sequence Implementation

```cpp
void ACombatOrchestrator::StartDefenseSequence(AActor* Attacker, AActor* Defender, 
                                                USpellData* Spell, 
                                                float SpellSize)
{
    // Calculate when defense window opens
    float WindowStart = Spell->DefenseWindow.WindowStartTime;
    float WindowDuration = Spell->DefenseWindow.WindowDuration;
    
    // Show visual cue (warning before window)
    FTimerHandle CueTimer;
    GetWorld()->GetTimerManager().SetTimer(CueTimer,
        [this, Defender]()
        {
            BattleUIManager->ShowDefenseCue(Defender); // "!" icon or visual flash
        },
        Spell->DefenseWindow.CueTime,
        false
    );
    
    // Open defense window
    FTimerHandle WindowTimer;
    GetWorld()->GetTimerManager().SetTimer(WindowTimer,
        [this, Defender, SpellSize]()
        {
            OpenDefenseWindow(Defender, SpellSize);
        },
        WindowStart,
        false
    );
    
    // Close defense window and apply damage
    FTimerHandle CloseTimer;
    GetWorld()->GetTimerManager().SetTimer(CloseTimer,
        [this, Attacker, Defender, Spell, SpellSize]()
        {
            CloseDefenseWindowAndApplyDamage(Attacker, Defender, Spell, SpellSize);
        },
        WindowStart + WindowDuration,
        false
    );
}

void ACombatOrchestrator::OpenDefenseWindow(AActor* Defender, float AttackSize)
{
    // Create defense state
    FDefenseState State;
    State.Defender = Defender;
    State.AttackSize = AttackSize;
    State.bWindowOpen = true;
    State.DefenseChosen = EDefenseType::None;
    State.bSuccessfulTiming = false;
    
    ActiveDefenseStates.Add(Defender, State);
    
    // Show UI prompt
    BattleUIManager->ShowDefensePrompt(Defender, AttackSize);
    
    // Input system will call OnDefenseInput when player presses button
}

void ACombatOrchestrator::OnDefenseInput(AActor* Defender, EDefenseType DefenseType)
{
    if (!ActiveDefenseStates.Contains(Defender)) return;
    
    FDefenseState& State = ActiveDefenseStates[Defender];
    
    if (!State.bWindowOpen)
    {
        // Too late, window already closed
        return;
    }
    
    // Record successful defense choice
    State.DefenseChosen = DefenseType;
    State.bSuccessfulTiming = true;
    
    // Hide UI
    BattleUIManager->HideDefensePrompt(Defender);
    
    // Visual feedback
    BattleUIManager->ShowDefenseSuccess(Defender, DefenseType);
}

void ACombatOrchestrator::CloseDefenseWindowAndApplyDamage(AActor* Attacker, 
                                                             AActor* Defender,
                                                             USpellData* Spell,
                                                             float SpellSize)
{
    // Get defense state
    FDefenseState State = ActiveDefenseStates.FindOrAdd(Defender);
    State.bWindowOpen = false;
    
    // Calculate base damage
    int32 BaseDamage = DamageCalculator->CalculateSpellDamage(Attacker, Spell, Defender);
    int32 FinalDamage = BaseDamage;
    int32 ReflectedDamage = 0;
    
    // Apply defense modifier if successful
    if (State.bSuccessfulTiming)
    {
        switch (State.DefenseChosen)
        {
            case EDefenseType::Block:
                FinalDamage = BaseDamage * 0.50f; // 50% reduction
                break;
                
            case EDefenseType::Parry:
                FinalDamage = BaseDamage * 0.30f; // 70% reduction
                ReflectedDamage = BaseDamage * 0.30f; // 30% reflected
                break;
                
            case EDefenseType::Dodge:
                // Check if dodge is possible based on spell size
                float DodgeThreshold = GetDodgeThreshold(Defender);
                if (SpellSize < DodgeThreshold)
                {
                    FinalDamage = 0; // Complete avoidance
                    BattleUIManager->ShowDodgeSuccess(Defender);
                }
                else
                {
                    // Spell too big, dodge fails
                    FinalDamage = BaseDamage; // Full damage
                    BattleUIManager->ShowDodgeFailed(Defender);
                }
                break;
        }
    }
    else
    {
        // Failed timing or didn't defend
        FinalDamage = BaseDamage; // Full damage
    }
    
    // Apply damage
    Defender->CharacterDataComponent->ServerTakeDamage(FinalDamage);
    
    // Apply reflected damage if parry
    if (ReflectedDamage > 0)
    {
        Attacker->CharacterDataComponent->ServerTakeDamage(ReflectedDamage);
        BattleUIManager->ShowParryReflection(Attacker, ReflectedDamage);
    }
    
    // Cleanup
    ActiveDefenseStates.Remove(Defender);
    
    // Check if this was the last target
    PendingDefenseCount--;
    if (PendingDefenseCount == 0)
    {
        // All defenses resolved, action complete
        OnActionCompleted.Broadcast(Attacker, FActionResult(...));
    }
}
```

---

### Animation Timing: Animation Notifies

**Recommended Approach:** Use UE5's Animation Notify system for precision

```cpp
// In SpellData or AbilityData
UPROPERTY(EditAnywhere, BlueprintReadWrite)
UAnimMontage* CastAnimation;

// Animation has notify events:
// - "DefenseWindowOpen" at 1.0s
// - "ApplyDamage" at 1.5s
// - "DefenseWindowClose" at 1.3s

void UActionExecutor::ExecuteSpell(AActor* Caster, USpellData* Spell, ...)
{
    UAnimInstance* AnimInst = GetAnimInstance(Caster);
    
    // Bind to animation notifies
    AnimInst->OnPlayMontageNotifyBegin.AddDynamic(this, &UActionExecutor::OnAnimNotify);
    
    // Play animation
    AnimInst->Montage_Play(Spell->CastAnimation);
}

void UActionExecutor::OnAnimNotify(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload)
{
    if (NotifyName == "DefenseWindowOpen")
    {
        // Open defense windows for all targets
        for (AActor* Target : CurrentTargets)
        {
            OpenDefenseWindow(Target, CurrentSpellSize);
        }
    }
    else if (NotifyName == "ApplyDamage")
    {
        // Close windows and apply damage
        for (AActor* Target : CurrentTargets)
        {
            CloseDefenseWindowAndApplyDamage(...);
        }
    }
}
```

**Benefits:**
- Designers control exact timing in animation editor
- Synchronized with visual feedback
- Easy to adjust per spell/ability
- Production-standard approach

---

## Next Steps

---

## AI Decision Making System

### Architecture: Behavior Trees + Difficulty Tiers

**Hybrid Approach:**
- **Behavior Trees** provide decision structure (visual, designer-friendly)
- **Difficulty Tiers** modify BT parameters and enable/disable branches
- **Full Player Parity** - AI can use all mechanics players can (infusion, defense, items)
- **Tactical Targeting** - Kill potential + Threat level
- **Scalable Difficulty** - Easy (tutorial) to Expert (competitive challenge)

---

### Difficulty Tier Definitions

```cpp
UENUM(BlueprintType)
enum class EAIDifficulty : uint8
{
    Easy    UMETA(DisplayName = "Easy (Tutorial Friendly)"),
    Medium  UMETA(DisplayName = "Medium (Competent)"),
    Hard    UMETA(DisplayName = "Hard (Optimal Play)"),
    Expert  UMETA(DisplayName = "Expert (Perfect Execution)")
};
```

---

### Master Behavior Tree Structure

```
Root Selector (Runs top to bottom, first valid branch executes)
│
├─ [SURVIVAL BRANCH]
│  ├─ Decorator: Difficulty != Easy (disabled on Easy)
│  ├─ Condition: MyHP < SurvivalThreshold
│  ├─ Condition: HealingItemAvailable
│  └─ Action: UseHealingItem(Self)
│
├─ [CLEANSE BRANCH]
│  ├─ Decorator: Difficulty != Easy (disabled on Easy)
│  ├─ Condition: HasDangerousDebuff (Poison, Burn, etc.)
│  ├─ Condition: CleanseItemAvailable
│  └─ Action: UseCleanse(Self)
│
├─ [ULTIMATE BRANCH]
│  ├─ Decorator: Difficulty != Easy (disabled on Easy)
│  ├─ Condition: UltimateAvailable
│  ├─ Condition: WorthUsingUltimate
│  └─ Task: UseUltimateWithTargeting
│     ├─ SelectTarget: TacticalTargeting
│     └─ Action: CastUltimate(Target)
│
├─ [AGGRESSIVE BRANCH]
│  ├─ Condition: CurrentEnergy > EnergyThreshold
│  └─ Selector: OffensiveActions
│     ├─ Task: EvaluateSpellCast
│     │  ├─ SelectTarget: TacticalTargeting
│     │  ├─ CalculateInfusion: DetermineSpellInfusionLevel
│     │  └─ Action: CastSpell(Target, InfusionLevel)
│     │
│     ├─ Task: EvaluateAbility
│     │  ├─ SelectTarget: TacticalTargeting
│     │  ├─ CalculateInfusion: DetermineAbilityInfusionLevel (Generic)
│     │  ├─ CalculateElement: DetermineElementInfusion (Caster)
│     │  └─ Action: UseAbility(Target, InfusionLevel, ElementInfused)
│     │
│     └─ Fallback: UseBasicAbility
│        ├─ SelectTarget: TacticalTargeting
│        └─ Action: BasicAbility(Target)
│
└─ [CONSERVATIVE BRANCH]
   ├─ Condition: CurrentEnergy <= EnergyThreshold
   └─ Action: EnergyEfficientAction
      ├─ SelectTarget: TacticalTargeting
      └─ Action: BasicAbility(Target, NoInfusion)
```

---

### Difficulty-Specific Parameters

```cpp
struct FAIDifficultySettings
{
    // Behavior Tree toggles
    bool bUseSurvivalBranch;
    bool bUseCleanseBranch;
    bool bUseUltimateBranch;
    
    // Thresholds
    float SurvivalHPThreshold;    // When to use healing items
    float AggressiveEnergyThreshold; // Switch between aggressive/conservative
    
    // Infusion logic
    EInfusionStrategy InfusionStrategy;
    
    // Defense capabilities
    float DefenseAttemptChance;   // % chance to attempt defense
    float DefenseTimingAccuracy;  // % chance to time defense correctly
    
    // Reaction time
    float MinThinkingDelay;
    float MaxThinkingDelay;
};

// Settings per difficulty
TMap<EAIDifficulty, FAIDifficultySettings> DifficultySettings = 
{
    { EAIDifficulty::Easy, {
        false,  // No survival branch
        false,  // No cleanse branch
        false,  // No ultimate branch
        0.0f,   // N/A
        0.70f,  // Aggressive if > 70% energy
        EInfusionStrategy::Never,
        0.40f,  // 40% defense attempt
        0.50f,  // 50% timing accuracy
        2.0f,   // 2-3.5 second thinking
        3.5f
    }},
    
    { EAIDifficulty::Medium, {
        true,   // Survival enabled
        true,   // Cleanse enabled
        true,   // Ultimate enabled
        0.30f,  // Heal at 30% HP
        0.60f,  // Aggressive if > 60% energy
        EInfusionStrategy::Basic,
        0.65f,  // 65% defense attempt
        0.75f,  // 75% timing accuracy
        1.0f,   // 1-2 second thinking
        2.0f
    }},
    
    { EAIDifficulty::Hard, {
        true,   // All branches enabled
        true,
        true,
        0.20f,  // Heal at 20% HP
        0.50f,  // Aggressive if > 50% energy
        EInfusionStrategy::Strategic,
        0.85f,  // 85% defense attempt
        0.90f,  // 90% timing accuracy
        0.5f,   // 0.5-1 second thinking
        1.0f
    }},
    
    { EAIDifficulty::Expert, {
        true,   // All branches enabled
        true,
        true,
        0.15f,  // Heal at 15% HP (accounts for DOTs)
        0.50f,  // Aggressive if > 50% energy
        EInfusionStrategy::Perfect,
        0.95f,  // 95% defense attempt
        0.98f,  // 98% timing accuracy
        0.25f,  // 0.25 second thinking
        0.25f
    }}
};
```

---

### Target Selection: Tactical (Kill Potential + Threat)

```cpp
AActor* UAIDecisionManager::SelectTarget(AActor* AIActor, EAIDifficulty Difficulty)
{
    TArray<AActor*> Enemies = GetAliveEnemies();
    
    AActor* BestTarget = nullptr;
    float HighestScore = -1.0f;
    
    for (AActor* Enemy : Enemies)
    {
        float Score = 0.0f;
        
        // === KILL POTENTIAL (HIGHEST PRIORITY) ===
        int32 EstimatedDamage = EstimateDamageToTarget(AIActor, Enemy);
        int32 TargetHP = GetHP(Enemy);
        
        if (EstimatedDamage >= TargetHP)
        {
            Score += 2000.0f; // Can kill this turn - EXECUTE
        }
        else
        {
            // Close to death = high priority
            float HPPercent = (float)TargetHP / GetMaxHP(Enemy);
            Score += (1.0f - HPPercent) * 500.0f; // 0-500 based on missing HP
        }
        
        // === THREAT LEVEL (HIGH PRIORITY) ===
        int32 ThreatLevel = CalculateThreatLevel(Enemy);
        Score += ThreatLevel * 2.0f; // Threat contributes significantly
        
        if (Score > HighestScore)
        {
            HighestScore = Score;
            BestTarget = Enemy;
        }
    }
    
    return BestTarget ? BestTarget : Enemies[0]; // Fallback to first enemy
}

int32 UAIDecisionManager::CalculateThreatLevel(AActor* Enemy)
{
    UCharacterDataComponent* EnemyComp = Enemy->FindComponentByClass<UCharacterDataComponent>();
    UCharacterData* EnemyData = EnemyComp->CharacterData;
    
    // Offensive capability = Threat
    int32 RawDamage = EnemyData->GetSubStat(ESubStatType::RawDamage);
    int32 EffectDamage = EnemyData->GetSubStat(ESubStatType::EffectDamage);
    int32 SpellPower = EnemyData->GetSubStat(ESubStatType::SpellPower);
    
    // Weight offensive stats
    int32 ThreatLevel = (RawDamage * 2) + (EffectDamage * 1.5f) + (SpellPower * 2);
    
    return ThreatLevel;
}
```

**Example Scenarios:**

```
Scenario 1: Killable Target Exists
Player 1: 15/100 HP, Threat 50
    Score = 2000 (killable) + 425 (85% HP missing) + 100 (threat) = 2525 ★

Player 2: 80/100 HP, Threat 120
    Score = 0 + 100 (20% HP missing) + 240 (threat) = 340

Player 3: 50/100 HP, Threat 80
    Score = 0 + 250 (50% HP missing) + 160 (threat) = 410

→ AI targets Player 1 (finish them off)


Scenario 2: No Killable Targets
Player 1: 60/100 HP, Threat 150
    Score = 0 + 200 (40% HP missing) + 300 (threat) = 500 ★

Player 2: 80/100 HP, Threat 90
    Score = 0 + 100 (20% HP missing) + 180 (threat) = 280

Player 3: 40/100 HP, Threat 70
    Score = 0 + 300 (60% HP missing) + 140 (threat) = 440

→ AI targets Player 1 (highest threat despite more HP)


Scenario 3: Equal Threat
Player 1: 70/100 HP, Threat 100
    Score = 0 + 150 (30% HP missing) + 200 (threat) = 350

Player 2: 30/100 HP, Threat 100
    Score = 0 + 350 (70% HP missing) + 200 (threat) = 550 ★

→ AI targets Player 2 (HP difference breaks tie)
```

---

### Infusion Decision Logic

#### Easy Difficulty: Never Infuse

```cpp
// EASY: No infusion, keep it simple
ESpellInfusionLevel SpellInfusionLevel = ESpellInfusionLevel::Normal;
EAbilityInfusionLevel AbilityInfusionLevel = EAbilityInfusionLevel::Normal;
bool bElementInfusion = false;
```

---

#### Medium Difficulty: Basic Infusion

```cpp
// MEDIUM: Infuse if high energy + can kill
ESpellInfusionLevel DetermineSpellInfusion_Medium(AActor* AIActor, AActor* Target)
{
    int32 CurrentEnergy = GetEnergy(AIActor);
    int32 MaxEnergy = GetMaxEnergy(AIActor);
    
    if (CurrentEnergy > MaxEnergy * 0.70f) // Plenty of energy
    {
        int32 NormalDamage = EstimateDamage(Normal);
        int32 InfusedDamage = EstimateDamage(Infused);
        int32 TargetHP = GetHP(Target);
        
        if (InfusedDamage >= TargetHP && NormalDamage < TargetHP)
        {
            return Infused; // Infusion secures kill
        }
    }
    
    return Normal; // Default: don't infuse
}

EAbilityInfusionLevel DetermineAbilityInfusion_Medium(AActor* AIActor, AActor* Target)
{
    // Same logic as spell infusion for Generic class
    // (only Generic can use ability power infusion)
    return /* same as spell */;
}

bool DetermineElementInfusion_Medium(AActor* AIActor, AActor* Target)
{
    // MEDIUM: Only infuse if elemental advantage
    ERefractionElement MyElement = GetElement(AIActor);
    ERefractionElement TargetElement = GetElement(Target);
    
    return HasElementalAdvantage(MyElement, TargetElement);
}
```

---

#### Hard Difficulty: Strategic Infusion

```cpp
// HARD: Consider energy efficiency + size for dodge denial
ESpellInfusionLevel DetermineSpellInfusion_Hard(AActor* AIActor, AActor* Target, USpellData* Spell)
{
    int32 CurrentEnergy = GetEnergy(AIActor);
    int32 MaxEnergy = GetMaxEnergy(AIActor);
    int32 TargetHP = GetHP(Target);
    
    // Check if we can kill with each level
    int32 NormalDamage = EstimateDamage(Spell, Normal);
    int32 InfusedDamage = EstimateDamage(Spell, Infused);
    int32 FullyInfusedDamage = EstimateDamage(Spell, FullyInfused);
    
    // Priority 1: Kill with minimum energy
    if (NormalDamage >= TargetHP)
        return Normal; // Don't waste energy
    if (InfusedDamage >= TargetHP)
        return Infused; // Use minimum needed
    if (FullyInfusedDamage >= TargetHP)
        return FullyInfused; // Secure kill
    
    // Priority 2: Deny dodge if high energy
    if (CurrentEnergy > MaxEnergy * 0.70f)
    {
        float TargetDodgeThreshold = GetDodgeThreshold(Target);
        float NormalSize = CalculateSpellSize(Spell, Normal);
        float InfusedSize = CalculateSpellSize(Spell, Infused);
        float FullyInfusedSize = CalculateSpellSize(Spell, FullyInfused);
        
        if (NormalSize >= TargetDodgeThreshold)
            return Normal; // Already undodgeable
        if (InfusedSize >= TargetDodgeThreshold)
            return Infused; // Make undodgeable efficiently
        return FullyInfused; // Maximize size
    }
    
    // Priority 3: Conserve energy if low
    if (CurrentEnergy < MaxEnergy * 0.50f)
        return Normal; // Don't risk running out
    
    // Default: Use some infusion if medium energy
    return Infused;
}

EAbilityInfusionLevel DetermineAbilityInfusion_Hard(AActor* AIActor, AActor* Target, UAbilityData* Ability)
{
    // Same kill-securing logic as spells
    // (Generic class only)
    int32 CurrentEnergy = GetEnergy(AIActor);
    int32 MaxEnergy = GetMaxEnergy(AIActor);
    int32 TargetHP = GetHP(Target);
    
    int32 NormalDamage = EstimateDamage(Ability, Normal);
    int32 InfusedDamage = EstimateDamage(Ability, Infused);
    int32 FullyInfusedDamage = EstimateDamage(Ability, FullyInfused);
    
    if (NormalDamage >= TargetHP)
        return Normal;
    if (InfusedDamage >= TargetHP)
        return Infused;
    if (FullyInfusedDamage >= TargetHP && CurrentEnergy > MaxEnergy * 0.60f)
        return FullyInfused;
    
    // Otherwise maximize damage if high energy
    if (CurrentEnergy > MaxEnergy * 0.75f)
        return FullyInfused;
    else if (CurrentEnergy > MaxEnergy * 0.50f)
        return Infused;
    
    return Normal;
}

bool DetermineElementInfusion_Hard(AActor* AIActor, AActor* Target, UAbilityData* Ability)
{
    // HARD: Weigh damage penalty vs status buildup value
    ERefractionElement MyElement = GetElement(AIActor);
    ERefractionElement TargetElement = GetElement(Target);
    
    // Always infuse if elemental advantage
    if (HasElementalAdvantage(MyElement, TargetElement))
        return true;
    
    // Check if target lacks status effects we could apply
    if (!HasStatusEffect(Target, MyElement))
    {
        // Worth building status even with damage penalty
        return true;
    }
    
    return false; // Default: no element infusion
}
```

---

#### Expert Difficulty: Perfect Infusion

```cpp
// EXPERT: Mathematical optimization
ESpellInfusionLevel DetermineSpellInfusion_Expert(AActor* AIActor, AActor* Target, USpellData* Spell)
{
    int32 CurrentEnergy = GetEnergy(AIActor);
    int32 TargetHP = GetHP(Target);
    
    // Calculate damage and cost for each level
    int32 NormalDamage = EstimateDamage(Spell, Normal);
    int32 InfusedDamage = EstimateDamage(Spell, Infused);
    int32 FullyInfusedDamage = EstimateDamage(Spell, FullyInfused);
    
    int32 NormalCost = CalculateEnergyCost(Spell, Normal);
    int32 InfusedCost = CalculateEnergyCost(Spell, Infused);
    int32 FullyInfusedCost = CalculateEnergyCost(Spell, FullyInfused);
    
    // Priority 1: Kill with minimum energy
    if (NormalDamage >= TargetHP)
        return Normal;
    if (InfusedDamage >= TargetHP)
        return Infused;
    if (FullyInfusedDamage >= TargetHP)
        return FullyInfused;
    
    // Priority 2: Calculate efficiency (damage per energy)
    float NormalEfficiency = (float)NormalDamage / NormalCost;
    float InfusedEfficiency = (float)InfusedDamage / InfusedCost;
    float FullyInfusedEfficiency = (float)FullyInfusedDamage / FullyInfusedCost;
    
    // Pick highest efficiency IF we have the energy
    if (FullyInfusedEfficiency >= InfusedEfficiency && 
        FullyInfusedEfficiency >= NormalEfficiency &&
        CurrentEnergy >= FullyInfusedCost)
        return FullyInfused;
        
    if (InfusedEfficiency >= NormalEfficiency && 
        CurrentEnergy >= InfusedCost)
        return Infused;
    
    return Normal;
}

EAbilityInfusionLevel DetermineAbilityInfusion_Expert(AActor* AIActor, AActor* Target, UAbilityData* Ability)
{
    // Same mathematical optimization as spells
    // (Generic class only)
    // [Same efficiency calculation logic]
}

bool DetermineElementInfusion_Expert(AActor* AIActor, AActor* Target, UAbilityData* Ability)
{
    // EXPERT: Calculate if damage penalty is worth status value
    float NormalDamage = EstimateDamage(Ability, false); // No element
    float ElementDamage = EstimateDamage(Ability, true) * 0.7f; // -30% penalty
    
    // Estimate value of status buildup
    float StatusValue = EstimateStatusValue(Target, GetElement(AIActor));
    
    // Infuse if total value exceeds normal damage
    return (ElementDamage + StatusValue) > NormalDamage;
}
```

---

### Defense System: AI Integration

```cpp
void ACombatOrchestrator::OnDefenseWindowOpened_AI(AActor* AIDefender, 
                                                     float AttackSize,
                                                     EAttackType AttackType)
{
    AAIController* AIController = Cast<AAIController>(AIDefender->GetController());
    if (!AIController) return;
    
    EAIDifficulty Difficulty = GetAIDifficulty(AIController);
    FAIDifficultySettings Settings = GetDifficultySettings(Difficulty);
    
    // Step 1: Does AI attempt to defend?
    if (FMath::FRand() > Settings.DefenseAttemptChance)
    {
        // Failed to react - no defense
        return;
    }
    
    // Step 2: Choose defense type
    EDefenseType ChosenDefense = ChooseDefense_AI(AIDefender, AttackSize, Difficulty);
    
    // Step 3: Simulate timing accuracy
    if (FMath::FRand() > Settings.DefenseTimingAccuracy)
    {
        // Attempted defense but mistimed - full damage
        return;
    }
    
    // Step 4: Execute successful defense
    ExecuteDefense(AIDefender, ChosenDefense);
}

EDefenseType UAIDecisionManager::ChooseDefense_AI(AActor* AIDefender, 
                                                    float AttackSize,
                                                    EAIDifficulty Difficulty)
{
    switch (Difficulty)
    {
        case EAIDifficulty::Easy:
            // Random choice (no intelligence)
            return PickRandom({Block, Parry, Dodge});
            
        case EAIDifficulty::Medium:
            // Basic logic: Dodge if possible, else Block
            {
                float DodgeThreshold = GetDodgeThreshold(AIDefender);
                if (AttackSize < DodgeThreshold)
                    return Dodge; // Can dodge
                else
                    return Block; // Can't dodge, play safe
            }
            
        case EAIDifficulty::Hard:
        case EAIDifficulty::Expert:
            // Optimal choice
            {
                float DodgeThreshold = GetDodgeThreshold(AIDefender);
                int32 IncomingDamage = GetEstimatedIncomingDamage(AIDefender);
                int32 CurrentHP = GetHP(AIDefender);
                
                if (AttackSize < DodgeThreshold)
                {
                    return Dodge; // Perfect avoidance
                }
                else
                {
                    // Can't dodge - choose Block vs Parry
                    if (IncomingDamage > CurrentHP * 0.60f)
                    {
                        return Block; // High damage, play safe
                    }
                    else
                    {
                        return Parry; // Lower damage, risk for counter
                    }
                }
            }
    }
    
    return Block; // Fallback
}
```

**Defense Success Rates:**

```
EASY Difficulty:
- 40% attempt to defend
- 50% timing accuracy IF attempted
- Net success: 20% (40% × 50%)
- Random defense choice

MEDIUM Difficulty:
- 65% attempt to defend
- 75% timing accuracy
- Net success: 48.75% (65% × 75%)
- Basic defense choice (dodge if possible, else block)

HARD Difficulty:
- 85% attempt to defend
- 90% timing accuracy
- Net success: 76.5% (85% × 90%)
- Optimal defense choice

EXPERT Difficulty:
- 95% attempt to defend
- 98% timing accuracy
- Net success: 93.1% (95% × 98%)
- Perfect defense choice
```

---

### Reaction Time System

```cpp
void ACombatOrchestrator::OnTurnStarted(AActor* Actor, int32 TurnNumber)
{
    // Process start-of-turn effects
    StatusEffectManager->ProcessStartOfTurnEffects(Actor);
    
    // Determine if AI or player
    AAIController* AIController = Cast<AAIController>(Actor->GetController());
    
    if (AIController)
    {
        // AI turn - add thinking delay
        EAIDifficulty Difficulty = GetAIDifficulty(AIController);
        float ThinkingDelay = CalculateAIThinkingDelay(Difficulty);
        
        FTimerHandle ThinkTimer;
        GetWorld()->GetTimerManager().SetTimer(ThinkTimer,
            [this, Actor]()
            {
                RequestActionFromActor(Actor);
            },
            ThinkingDelay,
            false
        );
    }
    else
    {
        // Player turn - no delay
        RequestActionFromActor(Actor);
    }
}

float UAIDecisionManager::CalculateAIThinkingDelay(EAIDifficulty Difficulty)
{
    FAIDifficultySettings Settings = GetDifficultySettings(Difficulty);
    
    // Random delay within range
    return FMath::FRandRange(Settings.MinThinkingDelay, Settings.MaxThinkingDelay);
}
```

**Thinking Delays:**
```
Easy:   2.0 - 3.5 seconds (gives player time to prepare)
Medium: 1.0 - 2.0 seconds (natural pace)
Hard:   0.5 - 1.0 seconds (quick decisions)
Expert: 0.25 seconds (nearly instant, slight delay for visibility)
```

---

### Implementation Checklist

**Core AI Systems:**
- [ ] Create UAIDecisionManager component
- [ ] Implement Behavior Tree asset
- [ ] Create difficulty settings struct
- [ ] Implement target selection (kill + threat)
- [ ] Implement infusion decision logic (4 difficulty levels)
- [ ] Integrate defense system with AI
- [ ] Add reaction time delays

**Behavior Tree Tasks:**
- [ ] BTTask_SelectTarget (tactical scoring)
- [ ] BTTask_DetermineSpellInfusion
- [ ] BTTask_DetermineAbilityInfusion
- [ ] BTTask_DetermineElementInfusion
- [ ] BTTask_CastSpell
- [ ] BTTask_UseAbility
- [ ] BTTask_UseItem
- [ ] BTTask_UseUltimate

**Behavior Tree Decorators:**
- [ ] BTDecorator_DifficultyCheck (enable/disable branches)
- [ ] BTDecorator_EnergyThreshold
- [ ] BTDecorator_HPThreshold
- [ ] BTDecorator_HasDebuff
- [ ] BTDecorator_ItemAvailable

**Testing:**
- [ ] Easy AI acts predictably (tutorial-friendly)
- [ ] Medium AI uses basic strategy
- [ ] Hard AI plays optimally
- [ ] Expert AI feels challenging but fair
- [ ] Defense success rates match difficulty
- [ ] Infusion decisions appropriate per difficulty
- [ ] Reaction times feel natural

---

### AI Design Philosophy

**Easy (Tutorial Friendly):**
- Random but legal actions
- No complex mechanics (ultimates, infusion)
- Low defense success (20%)
- Slow reaction (2-3.5s)
- **Goal:** Let players learn without punishment

**Medium (Competent Opponent):**
- Basic strategy (kill low HP, block when can't dodge)
- Uses all mechanics at basic level
- Moderate defense (49%)
- Normal reaction (1-2s)
- **Goal:** Engaging challenge for average players

**Hard (Optimal Play):**
- Strategic decisions (energy efficiency, dodge denial)
- Uses mechanics intelligently
- High defense (77%)
- Quick reaction (0.5-1s)
- **Goal:** Challenge for experienced players

**Expert (Perfect Execution):**
- Mathematical optimization
- Perfect mechanical execution (93% defense)
- Near-instant reaction (0.25s)
- **Goal:** Competitive challenge, test player skill ceiling

---

## BrokenDarkness Absorption System

### Overview: Unique Element Mechanic

**BrokenDarkness** is a special variant of the Darkness element with a unique absorption-based playstyle. BD characters can absorb energy from enemy spells through successful defense, unlocking hybrid spells that combine Darkness with other elements.

**Core Fantasy:**
- Absorb enemy magic to fuel your own power
- Master all elements through absorption
- High-risk, high-reward overflow mechanic
- Tactical positioning to maximize/minimize aura impact

**Key Differences from Regular Darkness:**
- BD must build energy before casting base spells
- Can create hybrids with all 8 other elements
- Weaker silence (50% vs 100%)
- Weaker status effects (25% of normal)
- Access to overflow aura mechanic
- Forbidden elements (Light/Void) deal self-damage

---

### Absorption Mechanic

**Energy Absorption from Defense:**

When BD character successfully **Blocks** or **Parries** an enemy spell, they absorb energy based on the spell's cost.

```cpp
float CalculateAbsorption(AActor* BDCharacter, USpellData* EnemySpell)
{
    // Base absorption = percentage of enemy spell's energy cost
    float BaseAbsorptionRate = 0.30f; // 30% of spell cost
    float BaseAbsorption = EnemySpell->EnergyCost * BaseAbsorptionRate;
    
    // CostReduction substat improves efficiency
    UCharacterData* BDData = BDCharacter->CharacterDataComponent->CharacterData;
    int32 CostReduction = BDData->GetSubStat(ESubStatType::CostReduction);
    float EfficiencyBonus = CostReduction * 0.05f; // +5% per point
    
    float FinalAbsorption = BaseAbsorption * (1.0f + EfficiencyBonus);
    
    return FinalAbsorption;
}
```

**Example:**
```
Enemy casts Fireball (50 energy cost)
BD Blocks (takes 50% damage)

Base Absorption: 50 × 0.30 = 15 energy
CostReduction 10: 15 × 1.5 = 22.5 energy absorbed

BD gains 22.5 energy + unlocks Dark Flames hybrid
```

**Key Rules:**
- ✅ **Block** absorbs energy
- ✅ **Parry** absorbs energy
- ❌ **Dodge** does NOT absorb (100% avoidance = no contact)
- ✅ Absorption happens ONLY during defense windows (enemy turns)
- ✅ Can absorb while in overflow (extends duration)

---

### Hybrid Spell System

**Unlocking Hybrids:**

When BD absorbs energy from an elemental spell, they unlock the hybrid variant of that element.

```cpp
void AbsorbElement(AActor* BDCharacter, ERefractionElement Element)
{
    UCharacterDataComponent* BDComp = BDCharacter->FindComponentByClass<UCharacterDataComponent>();
    
    if (Element == ERefractionElement::Darkness)
    {
        // Absorbing Darkness reverts to base (unless x3 stacks)
        if (BDComp->HybridStacks < 3)
        {
            BDComp->CurrentHybridElement = None;
            BDComp->HybridStacks = 0;
        }
        // x3 stacks = "mastered", immune to reversion
    }
    else
    {
        // New element absorbed
        BDComp->CurrentHybridElement = Element;
        BDComp->HybridStacks = 1; // Reset to 1 stack
    }
    
    UpdateAvailableSpells(BDCharacter);
}
```

**Hybrid Elements:**
```
Fire → Dark Flames
Water → Dark Waters
Earth → Dark Earth
Wind → Dark Winds
Light → Dark Light (FORBIDDEN: Self-damage)
Darkness → Revert to Pure Darkness (unless x3 stacks)
Lightning → Dark Lightning
Void → Dark Void (FORBIDDEN: Self-damage)
Reality → (Deferred for future design)
```

---

### Stack System

**Building Mastery:**

Absorbing the same element multiple times builds stacks, increasing status effect power.

```cpp
void IncrementHybridStack(AActor* BDCharacter, ERefractionElement Element)
{
    UCharacterDataComponent* BDComp = BDCharacter->FindComponentByClass<UCharacterDataComponent>();
    
    if (BDComp->CurrentHybridElement == Element && BDComp->HybridStacks < 3)
    {
        BDComp->HybridStacks++;
        
        if (BDComp->HybridStacks == 3)
        {
            // Mastery achieved
            OnHybridMastered.Broadcast(BDCharacter, Element);
        }
    }
}
```

**Stack Progression:**
```
x1 Stack: Base status damage (100%)
x2 Stack: Enhanced status damage (130%)
x3 Stack: Maximum status damage (170%)
```

**Stack Bonus Formula:**
```cpp
float CalculateStackBonus(int32 Stacks)
{
    switch (Stacks)
    {
        case 1: return 1.0f;   // 100%
        case 2: return 1.3f;   // 130%
        case 3: return 1.7f;   // 170%
        default: return 1.0f;
    }
}

// Applied to status buildup:
float StatusBuildup = BaseStatusBuildup * CalculateStackBonus(HybridStacks);
```

**Stack Rules:**
- ✅ **Stacks reset** when switching elements
- ✅ **x3 stacks** unlocks advanced spells (MinStacksRequired = 3)
- ✅ **x3 stacks** immune to Darkness reversion
- ✅ **Stacks refresh** when hitting same element again (don't add to x3)

**Example:**
```
Turn 1: Absorb Fire → Dark Flames x1
Turn 3: Absorb Fire → Dark Flames x2
Turn 5: Absorb Fire → Dark Flames x3 (MASTERED)
Turn 7: Absorb Water → Dark Waters x1 (Fire stacks lost)
Turn 9: Absorb Fire → Dark Flames x1 (start over)
```

---

### Overflow System

**Activation:**

When BD's energy exceeds their base maximum (100), they enter **Overflow** state.

```cpp
UPROPERTY()
int32 MaxEnergy = 100;

UPROPERTY()
int32 OverflowCap = 150; // 50% extra capacity

void CheckOverflowActivation(AActor* BDCharacter)
{
    UCharacterDataComponent* BDComp = BDCharacter->FindComponentByClass<UCharacterDataComponent>();
    
    if (BDComp->CurrentEP > BDComp->MaxEnergy && !BDComp->bInOverflow)
    {
        // Enter overflow
        BDComp->bInOverflow = true;
        ActivateOverflowAura(BDCharacter);
        OnOverflowActivated.Broadcast(BDCharacter);
    }
    else if (BDComp->CurrentEP <= BDComp->MaxEnergy && BDComp->bInOverflow)
    {
        // Exit overflow
        BDComp->bInOverflow = false;
        DeactivateOverflowAura(BDCharacter);
        OnOverflowEnded.Broadcast(BDCharacter);
    }
}
```

**Energy Capacity:**
```
Base Max: 100 energy
Overflow Zone: 100-150 energy (50% extra)
Hard Cap: 150 energy (prevents infinite stacking)
```

---

### Overflow Aura

**When in overflow, BD radiates a damage aura based on their current hybrid element.**

#### Aura Range Calculation

```cpp
float CalculateAuraRange(AActor* BDCharacter)
{
    UCharacterData* BDData = BDCharacter->CharacterDataComponent->CharacterData;
    
    float BaseRange = 3.0f; // meters
    int32 AbilitySize = BDData->GetSubStat(ESubStatType::AbilitySize);
    float RangeBonus = AbilitySize * 0.2f; // +0.2m per point
    
    return BaseRange + RangeBonus;
}

// Example:
// AbilitySize 0: 3.0m range
// AbilitySize 10: 5.0m range (3 + 10×0.2)
// AbilitySize 21: 7.2m range (3 + 21×0.2)
```

---

#### Aura Damage (Per Turn)

**Triggers:** Start of turn AND end of turn (twice per turn)

```cpp
void ApplyOverflowAuraDamage(AActor* BDCharacter)
{
    UCharacterData* BDData = BDCharacter->CharacterDataComponent->CharacterData;
    
    // Base damage
    float BaseDamage = 5.0f;
    
    // EffectDamage increases aura damage
    int32 EffectDamage = BDData->GetSubStat(ESubStatType::EffectDamage);
    float AuraDamage = BaseDamage + (EffectDamage * 0.5f); // +0.5 per point
    
    // Find all actors in aura range
    float AuraRange = CalculateAuraRange(BDCharacter);
    TArray<AActor*> ActorsInRange = GetActorsInAuraRange(BDCharacter, AuraRange);
    
    for (AActor* Target : ActorsInRange)
    {
        if (Target == BDCharacter) continue; // BD immune to own aura damage
        
        // Apply damage to enemies AND allies (friendly fire)
        Target->CharacterDataComponent->ServerTakeDamage(AuraDamage);
        
        // Visual feedback
        SpawnAuraDamageVFX(Target, BDCharacter->CurrentHybridElement);
    }
}
```

**Example:**
```
BD in overflow with Dark Flames active
EffectDamage: 15
Aura Damage: 5 + (15 × 0.5) = 12.5 damage per trigger
Triggers twice per turn = 25 damage per turn total

Ally at Front Right position (4m away)
BD at Front Left with 5m aura range
Ally IN RANGE → Takes 12.5 damage (start of turn)
Ally IN RANGE → Takes 12.5 damage (end of turn)
Total: 25 damage to ally per turn (friendly fire!)
```

---

#### Aura Status Buildup (Per Turn)

```cpp
void ApplyOverflowStatusBuildup(AActor* BDCharacter)
{
    UCharacterDataComponent* BDComp = BDCharacter->FindComponentByClass<UCharacterDataComponent>();
    
    // Only applies if hybrid active
    if (BDComp->CurrentHybridElement == None) return;
    
    UCharacterData* BDData = BDComp->CharacterData;
    int32 EffectDamage = BDData->GetSubStat(ESubStatType::EffectDamage);
    
    // Base buildup
    float BaseBuildup = 10.0f;
    float BuildupAmount = BaseBuildup + (EffectDamage * 1.0f); // +1 per point
    
    // Apply weakened buildup (25% of normal)
    BuildupAmount *= 0.25f;
    
    // Apply to all in range
    TArray<AActor*> ActorsInRange = GetActorsInAuraRange(BDCharacter, 
                                                          BDComp->OverflowAuraRange);
    
    for (AActor* Target : ActorsInRange)
    {
        if (Target == BDCharacter) continue;
        
        // Build hybrid status (e.g., Dark Flames status)
        StatusEffectManager->ApplyHybridStatusBuildup(Target, 
                                                       BDComp->CurrentHybridElement,
                                                       BuildupAmount);
    }
}
```

---

#### Energy Drain (Per Turn)

**Overflow drains energy every turn to balance the power.**

```cpp
void DrainOverflowEnergy(AActor* BDCharacter)
{
    UCharacterData* BDData = BDCharacter->CharacterDataComponent->CharacterData;
    
    // Base drain
    float BaseDrain = 10.0f;
    
    // AbilitySize INCREASES drain (bigger aura = more cost)
    int32 AbilitySize = BDData->GetSubStat(ESubStatType::AbilitySize);
    float SizeIncrease = AbilitySize * 0.5f; // +0.5 energy per point
    
    // CostReduction DECREASES drain (efficiency)
    int32 CostReduction = BDData->GetSubStat(ESubStatType::CostReduction);
    float ReductionDecrease = CostReduction * 0.3f; // -0.3 energy per point
    
    float FinalDrain = BaseDrain + SizeIncrease - ReductionDecrease;
    FinalDrain = FMath::Max(FinalDrain, 5.0f); // Minimum 5 energy drain
    
    // Deduct energy
    BDCharacter->CharacterDataComponent->ServerSpendEnergy(FinalDrain);
}
```

**Example:**
```
Base Drain: 10 energy/turn
AbilitySize 15: +7.5 drain (15 × 0.5)
CostReduction 10: -3 drain (10 × 0.3)
Final Drain: 10 + 7.5 - 3 = 14.5 energy/turn

Turn starts at 130 energy:
- Drain 14.5 → 115.5
Turn ends:
- Drain 14.5 → 101 (still in overflow)
```

---

#### Overflow Duration Strategy

**High AbilitySize:**
- ✅ Massive aura range (threatens entire battlefield)
- ❌ Drains energy FAST (short duration)
- ❌ High self-damage from aura
- **Playstyle:** Aggressive burst, zone control

**High CostReduction:**
- ✅ Slow energy drain (long duration)
- ✅ Efficient absorption
- ❌ Smaller aura range
- **Playstyle:** Sustained pressure, energy management

**Balanced:**
- Moderate range and duration
- **Playstyle:** Flexible, adaptable

---

### Positioning Integration

**5-Position System:**
```
        [Back Left]    [Back Right]
[Front Left]  [Back Middle]  [Front Right]
```

**Position Distances (Example Values):**
```cpp
// Adjacent positions
Front Left ↔ Back Middle: 3m
Front Right ↔ Back Middle: 3m
Back Left ↔ Back Middle: 3m
Back Right ↔ Back Middle: 3m

// Front row separation
Front Left ↔ Front Right: 5m

// Back row separation
Back Left ↔ Back Right: 5m

// Diagonal long distance
Front Left ↔ Back Right: 7m
Front Right ↔ Back Left: 7m
```

**Aura Range Scenarios:**

**Scenario 1: Small Aura (3m base, AbilitySize 0)**
```
BD at Front Left with 3m aura:
✅ Back Middle (3m) - JUST in range
✅ Back Left (3m) - JUST in range
❌ Front Right (5m) - OUT of range
❌ Back Right (7m) - OUT of range

2 positions threatened
```

**Scenario 2: Medium Aura (5m, AbilitySize 10)**
```
BD at Front Left with 5m aura:
✅ Back Middle (3m) - In range
✅ Back Left (3m) - In range
✅ Front Right (5m) - JUST in range
❌ Back Right (7m) - OUT of range

3 positions threatened
```

**Scenario 3: Large Aura (7.2m, AbilitySize 21)**
```
BD at Front Left with 7.2m aura:
✅ Back Middle (3m) - In range
✅ Back Left (3m) - In range
✅ Front Right (5m) - In range
✅ Back Right (7m) - In range

ALL 4 positions threatened!
```

**Strategic Positioning:**
- Spread formation counters large aura (sacrifice synergy for safety)
- Clustered formation enables buffs but vulnerable to aura
- Ranged characters can stay at Back Right (safest from Front Left BD)

---

### Silence System

**Three Types of Silence:**

#### Type 1: Gradual Silence (Hybrid Spell Hits)

Applied each time a hybrid spell hits, accumulates over duration.

```cpp
USTRUCT()
struct FSilenceEffect
{
    GENERATED_BODY()
    
    UPROPERTY()
    float PercentPerTurn = 15.0f; // % energy locked per turn
    
    UPROPERTY()
    int32 RemainingTurns = 3;
    
    UPROPERTY()
    float CurrentSilencedPercent = 0.0f; // Accumulates
};

void ApplyGradualSilence(AActor* Target, USpellData* HybridSpell)
{
    FSilenceEffect NewSilence;
    NewSilence.PercentPerTurn = HybridSpell->SilencePercentPerTurn; // From spell data
    NewSilence.RemainingTurns = HybridSpell->SilenceDuration;
    
    AddStatusEffect(Target, NewSilence);
}

void ProcessSilenceEffect(AActor* Target, FSilenceEffect& Silence)
{
    // Each turn, lock more energy
    Silence.CurrentSilencedPercent += Silence.PercentPerTurn;
    Silence.RemainingTurns--;
    
    // Calculate usable energy
    int32 MaxEnergy = Target->CharacterDataComponent->MaxEP;
    int32 SilencedAmount = MaxEnergy * (Silence.CurrentSilencedPercent / 100.0f);
    int32 UsableEnergy = MaxEnergy - SilencedAmount;
    
    // Clamp current energy to usable amount
    Target->CharacterDataComponent->CurrentEP = FMath::Min(
        Target->CharacterDataComponent->CurrentEP, 
        UsableEnergy
    );
}
```

**Example:**
```
Dark Flames hits enemy
SilencePercentPerTurn: 15%
SilenceDuration: 3 turns

Turn 1: 15% energy locked (85 usable)
Turn 2: 30% energy locked (70 usable)
Turn 3: 45% energy locked (55 usable)
Turn 4: Expires, back to 100% usable
```

---

#### Type 2: Status Break Silence (Full Bar)

When hybrid status bar fills to 100%, applies powerful full silence.

```cpp
void OnStatusBarFilled(AActor* Target, ERefractionElement HybridElement)
{
    // Apply BrokenDarkness silence (50% energy)
    ApplyFullSilence(Target, 0.50f, 1); // 50% for 1 turn
    
    // Apply weakened elemental status
    ApplyWeakenedStatus(Target, HybridElement, 0.25f);
    
    // Reset status bar
    ResetStatusBar(Target, HybridElement);
}

void ApplyFullSilence(AActor* Target, float SilencePercent, int32 Duration)
{
    FSilenceEffect FullSilence;
    FullSilence.bFullSilence = true;
    FullSilence.CurrentSilencedPercent = SilencePercent * 100.0f; // 50%
    FullSilence.RemainingTurns = Duration; // 1 turn
    
    AddStatusEffect(Target, FullSilence);
}
```

**Example:**
```
Dark Flames status bar: 100/100 (FULL)
↓
Apply: 50% energy silenced for 1 turn
Apply: Weakened Burn (25% of normal Fire burn)
↓
Reset: Dark Flames status bar → 0/100
```

**Comparison to Pure Darkness:**
```
Pure Darkness status break: 100% energy silenced
BrokenDarkness status break: 50% energy silenced (WEAKER)
```

---

#### Type 3: Onyx Crystal Silence (Item)

S-Tier Onyx Crystal instantly applies full silence.

```cpp
void UseOnyxCrystal(AActor* BDUser, AActor* Target)
{
    // Apply full silence to target
    ApplyFullSilence(Target, 1.0f, 1); // 100% for 1 turn
    
    // Grant BD user Darkness energy (item tier bonus)
    int32 TierBonus = CalculateItemTierBonus(OnyxCrystal->Tier);
    BDUser->CharacterDataComponent->ServerGainEnergy(TierBonus);
    
    // Revert BD to pure Darkness
    BDUser->CharacterDataComponent->CurrentHybridElement = None;
    BDUser->CharacterDataComponent->HybridStacks = 0;
}
```

**Strategic Use:**
- Silence dangerous enemy for 1 turn (100% lock)
- Gain energy boost
- Reset your hybrid (tactical choice to switch elements)

---

### Forbidden Elements: Light & Void

**Self-Damage Mechanic:**

Absorbing Light or Void elements unlocks powerful hybrids, but casting them deals damage to the BD character.

```cpp
void OnAbsorbForbiddenElement(AActor* BDCharacter, USpellData* AbsorbedSpell)
{
    UCharacterDataComponent* BDComp = BDCharacter->FindComponentByClass<UCharacterDataComponent>();
    
    // Mark as forbidden
    BDComp->bHasForbiddenElement = true;
    
    // Store penalty based on absorbed spell's power
    BDComp->ForbiddenElementPenalty = AbsorbedSpell->CalculateDamage(Caster, Target) * 0.5f;
    
    // Visual warning
    ShowForbiddenElementWarning(BDCharacter);
}

void OnCastForbiddenSpell(AActor* BDCharacter, USpellData* ForbiddenSpell)
{
    UCharacterDataComponent* BDComp = BDCharacter->FindComponentByClass<UCharacterDataComponent>();
    
    if (!BDComp->bHasForbiddenElement) return;
    
    // Apply self-damage EVERY cast
    int32 SelfDamage = BDComp->ForbiddenElementPenalty;
    
    // Resistance stat reduces self-damage?
    int32 Resistance = BDComp->CharacterData->GetSubStat(ESubStatType::Resistance);
    SelfDamage -= (Resistance * 0.5f); // -0.5 HP per point
    SelfDamage = FMath::Max(SelfDamage, 10); // Minimum 10 self-damage
    
    // Apply damage to self
    BDCharacter->CharacterDataComponent->ServerTakeDamage(SelfDamage);
    
    // Visual feedback (dark energy backlash)
    SpawnForbiddenElementVFX(BDCharacter);
}
```

**Example:**
```
Enemy casts Holy Beam (Light, 60 damage)
BD Blocks, absorbs Light energy
ForbiddenElementPenalty = 60 × 0.5 = 30 HP

BD casts Dark Light Fireball:
- Deals damage to enemy
- Takes 30 HP self-damage
- Resistance 10: 30 - 5 = 25 HP self-damage

BD casts Dark Light Fireball again:
- Takes 25 HP self-damage again (EVERY cast)
```

**Strategic Consideration:**
- Powerful hybrids (Light = holy damage, Void = destruction)
- High risk (lose HP every cast)
- Positioning matters (allies can't heal you mid-combat if overflow active)
- Build choice (High HP builds can afford forbidden elements)

---

### Hybrid Spell Data Structure

```cpp
UCLASS(BlueprintType)
class USpellData : public UPrimaryDataAsset
{
    GENERATED_BODY()
    
    // ==================== BASIC INFO ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell")
    FText SpellName;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell")
    ERefractionElement Element;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell")
    int32 EnergyCost = 30;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell")
    int32 BaseDamage = 50;
    
    // ==================== HYBRID SYSTEM ====================
    
    /** Can BrokenDarkness characters create a hybrid variant? */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hybrid System")
    bool bCanBeHybridized = false;
    
    /** Name for hybrid variant (e.g., "Dark Fireball") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hybrid System",
              meta = (EditCondition = "bCanBeHybridized"))
    FName HybridSpellName;
    
    // ==================== BROKEN DARKNESS MECHANICS ====================
    
    /** Duration of gradual silence effect (turns) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broken Darkness",
              meta = (EditCondition = "bCanBeHybridized"))
    int32 SilenceDuration = 3;
    
    /** Percentage of energy locked per turn */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broken Darkness",
              meta = (EditCondition = "bCanBeHybridized"))
    float SilencePercentPerTurn = 15.0f;
    
    /** Minimum stack level to cast this spell (0 = any level) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broken Darkness",
              meta = (EditCondition = "bCanBeHybridized"))
    int32 MinStacksRequired = 0;
    
    /** Maximum stack level to cast this spell (3 = all levels) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broken Darkness",
              meta = (EditCondition = "bCanBeHybridized"))
    int32 MaxStacksRequired = 3;
    
    // ==================== BD-EXCLUSIVE SPELLS ====================
    
    /** Only BrokenDarkness characters can use this spell */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broken Darkness")
    bool bIsBDExclusive = false;
    
    /** Minimum energy required to cast (BD base spells require buildup) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broken Darkness",
              meta = (EditCondition = "bIsBDExclusive"))
    int32 MinEnergyRequired = 0;
    
    /** Can this spell absorb energy as part of its effect? */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broken Darkness")
    bool bAbsorbsEnergy = false;
};
```

**Example Spell Configurations:**

**Universal Fireball:**
```
SpellName: "Fireball"
Element: Fire
EnergyCost: 35
BaseDamage: 50

bCanBeHybridized: true
HybridSpellName: "Dark Fireball"
SilenceDuration: 3
SilencePercentPerTurn: 15%
MinStacksRequired: 0 (available at all stack levels)
MaxStacksRequired: 3
```

**Advanced Fire Spell (Stack-Locked):**
```
SpellName: "Inferno"
Element: Fire
EnergyCost: 60
BaseDamage: 100

bCanBeHybridized: true
HybridSpellName: "Dark Inferno"
SilenceDuration: 4
SilencePercentPerTurn: 20%
MinStacksRequired: 3 (ONLY at x3 stacks)
MaxStacksRequired: 3
```

**BD-Exclusive Spell:**
```
SpellName: "Shadow Strike"
Element: BrokenDarkness
EnergyCost: 25
BaseDamage: 40

bIsBDExclusive: true
MinEnergyRequired: 50 (must have 50+ energy to cast)
bCanBeHybridized: false (no hybrid variant)
```

---

### Status Bar System

**Hybrid Status Bars:**

Instead of two separate bars (Darkness + Element), BD creates ONE hybrid status bar with combined visual.

```cpp
USTRUCT()
struct FHybridStatusBar
{
    GENERATED_BODY()
    
    /** Which hybrid element (e.g., Fire for Dark Flames) */
    UPROPERTY()
    ERefractionElement HybridElement;
    
    /** Buildup progress (0-100) */
    UPROPERTY()
    float Progress = 0.0f;
    
    /** Visual color (e.g., Dark Red for Dark Flames) */
    UPROPERTY()
    FLinearColor BarColor;
};

FLinearColor GetHybridColor(ERefractionElement Element)
{
    switch (Element)
    {
        case Fire: return FLinearColor(0.3f, 0.0f, 0.0f); // Dark Red
        case Water: return FLinearColor(0.0f, 0.0f, 0.3f); // Dark Blue
        case Earth: return FLinearColor(0.2f, 0.15f, 0.0f); // Dark Brown
        case Wind: return FLinearColor(0.1f, 0.15f, 0.15f); // Dark Cyan
        case Light: return FLinearColor(0.2f, 0.15f, 0.2f); // Dark Purple
        case Lightning: return FLinearColor(0.15f, 0.0f, 0.15f); // Dark Violet
        case Void: return FLinearColor(0.05f, 0.0f, 0.1f); // Near Black
        default: return FLinearColor::Black;
    }
}
```

**Status Bar Filling:**
```cpp
void ApplyHybridStatusBuildup(AActor* Target, 
                               ERefractionElement HybridElement, 
                               float BuildupAmount)
{
    FHybridStatusBar& StatusBar = GetOrCreateStatusBar(Target, HybridElement);
    
    StatusBar.Progress += BuildupAmount;
    
    if (StatusBar.Progress >= 100.0f)
    {
        // Status break!
        StatusBar.Progress = 100.0f;
        OnStatusBarFilled(Target, HybridElement);
    }
}

void OnStatusBarFilled(AActor* Target, ERefractionElement HybridElement)
{
    // Apply BrokenDarkness silence (50% for 1 turn)
    ApplyFullSilence(Target, 0.50f, 1);
    
    // Apply weakened elemental status
    float WeakenedMultiplier = 0.25f; // 25% of normal effect
    ApplyWeakenedElementalStatus(Target, HybridElement, WeakenedMultiplier);
    
    // Reset bar
    ResetStatusBar(Target, HybridElement);
}
```

**Weakened Status Calculation:**
```cpp
void ApplyWeakenedElementalStatus(AActor* Target, 
                                   ERefractionElement Element,
                                   float WeakenedMultiplier)
{
    // Calculate normal status effect
    int32 NormalDamage = CalculateElementalStatusDamage(Caster, Element, Target);
    int32 NormalDuration = GetElementalStatusDuration(Element);
    
    // Apply weakened version (25% of normal)
    int32 WeakenedDamage = NormalDamage * WeakenedMultiplier;
    
    // Same duration, reduced damage
    ApplyElementalStatus(Target, Element, WeakenedDamage, NormalDuration);
}
```

**Example:**
```
Pure Fire spell would apply:
Burn: 40 damage over 4 turns (10/turn)

Dark Flames status break applies:
Weakened Burn: 10 damage over 4 turns (2.5/turn)
+ BrokenDarkness Silence: 50% energy locked for 1 turn
```

---

### Character Data Integration

```cpp
// In CharacterData.h
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Element")
bool bIsBrokenDarkness = false;

// In CharacterDataComponent.h (Runtime State)
UCLASS()
class UCharacterDataComponent : public UActorComponent
{
    GENERATED_BODY()
    
    // ==================== BROKEN DARKNESS STATE ====================
    
    /** Current hybrid element (None if base Darkness) */
    UPROPERTY(ReplicatedUsing=OnRep_HybridElement)
    ERefractionElement CurrentHybridElement = None;
    
    /** Hybrid stack level (0-3) */
    UPROPERTY(ReplicatedUsing=OnRep_HybridStacks)
    int32 HybridStacks = 0;
    
    /** Is character in overflow state? */
    UPROPERTY(ReplicatedUsing=OnRep_Overflow)
    bool bInOverflow = false;
    
    /** Current aura range (meters) */
    UPROPERTY(Replicated)
    float OverflowAuraRange = 0.0f;
    
    /** Has forbidden element active (Light/Void)? */
    UPROPERTY(Replicated)
    bool bHasForbiddenElement = false;
    
    /** Self-damage penalty for forbidden element casts */
    UPROPERTY(Replicated)
    int32 ForbiddenElementPenalty = 0;
    
    // ==================== ENERGY SYSTEM ====================
    
    /** Current energy */
    UPROPERTY(ReplicatedUsing=OnRep_CurrentEP)
    int32 CurrentEP;
    
    /** Base maximum energy */
    UPROPERTY()
    int32 MaxEP = 100;
    
    /** Overflow capacity (150 = 50% extra) */
    UPROPERTY()
    int32 OverflowCap = 150;
    
    // ==================== REPLICATION ====================
    
    UFUNCTION()
    void OnRep_HybridElement();
    
    UFUNCTION()
    void OnRep_HybridStacks();
    
    UFUNCTION()
    void OnRep_Overflow();
};
```

---

### Complete Turn Integration

```cpp
void ACombatOrchestrator::OnTurnStarted(AActor* Actor, int32 TurnNumber)
{
    // Process start-of-turn status effects
    StatusEffectManager->ProcessStartOfTurnEffects(Actor);
    
    // Process BD overflow (if applicable)
    if (IsBrokenDarkness(Actor))
    {
        AbsorptionManager->ProcessOverflowTurn(Actor, true); // bTurnStart = true
    }
    
    // Request action
    RequestActionFromActor(Actor);
}

void ACombatOrchestrator::OnTurnEnded(AActor* Actor, int32 TurnNumber)
{
    // Process end-of-turn status effects
    StatusEffectManager->ProcessEndOfTurnEffects(Actor);
    
    // Process BD overflow (if applicable)
    if (IsBrokenDarkness(Actor))
    {
        AbsorptionManager->ProcessOverflowTurn(Actor, false); // bTurnStart = false
    }
    
    // Check win condition
    if (CheckWinCondition())
    {
        EndCombat(GetVictor());
        return;
    }
    
    // Advance turn
    TurnManager->EndCurrentTurn();
}
```

---

### Defense Integration

```cpp
void ACombatOrchestrator::OnDefenseWindowClosed(AActor* Defender, 
                                                  AActor* Attacker,
                                                  USpellData* AttackSpell,
                                                  EDefenseType DefenseChosen,
                                                  bool bSuccessful)
{
    // ... Normal defense processing ...
    
    // BD absorption check
    if (bSuccessful && IsBrokenDarkness(Defender))
    {
        // Only Block and Parry absorb (Dodge doesn't)
        if (DefenseChosen == EDefenseType::Block || DefenseChosen == EDefenseType::Parry)
        {
            AbsorptionManager->OnSuccessfulDefense(Defender, Attacker, AttackSpell, DefenseChosen);
        }
    }
}

void UAbsorptionManager::OnSuccessfulDefense(AActor* BDCharacter, 
                                              AActor* Attacker,
                                              USpellData* AbsorbedSpell,
                                              EDefenseType DefenseUsed)
{
    // Calculate energy absorbed
    int32 EnergyGained = CalculateAbsorption(BDCharacter, AbsorbedSpell);
    
    // Grant energy
    BDCharacter->CharacterDataComponent->ServerGainEnergy(EnergyGained);
    
    // Absorb element
    AbsorbElement(BDCharacter, AbsorbedSpell->Element);
    
    // Check overflow
    CheckOverflowActivation(BDCharacter);
    
    // Visual feedback
    SpawnAbsorptionVFX(BDCharacter, AbsorbedSpell->Element);
}
```

---

### Implementation Checklist

**Core Systems:**
- [ ] CharacterDataComponent BD state replication
- [ ] AbsorptionManager component
- [ ] Hybrid spell creation system
- [ ] Stack tracking and progression
- [ ] Overflow activation/deactivation
- [ ] Aura range calculation
- [ ] Aura damage application (start + end of turn)
- [ ] Aura status buildup
- [ ] Energy drain system

**Spell System:**
- [ ] SpellData hybrid flags
- [ ] Hybrid spell variant generation
- [ ] Stack-gated spell filtering
- [ ] BD-exclusive spell handling
- [ ] Forbidden element tracking

**Silence System:**
- [ ] Gradual silence (per-spell hit)
- [ ] Status break silence (50%)
- [ ] Onyx Crystal silence (100%)
- [ ] Energy bar locking/unlocking
- [ ] Usable energy calculation

**Status Effects:**
- [ ] Hybrid status bar creation
- [ ] Status buildup calculation
- [ ] Weakened status application (25%)
- [ ] Bar filling detection
- [ ] Bar reset on proc

**Positioning:**
- [ ] 5-position system implementation
- [ ] Distance calculation between positions
- [ ] Aura range vs distance checks
- [ ] Position-based targeting

**UI/UX:**
- [ ] Hybrid element indicator
- [ ] Stack counter (x1, x2, x3)
- [ ] Overflow visual feedback (aura VFX)
- [ ] Hybrid status bar colors
- [ ] Energy bar with overflow zone
- [ ] Forbidden element warning
- [ ] Absorption VFX

**Testing:**
- [ ] Absorption energy calculations
- [ ] Hybrid unlock progression
- [ ] Stack system (build, reset, mastery)
- [ ] Overflow aura range accuracy
- [ ] Aura damage to allies (friendly fire)
- [ ] Energy drain vs AbilitySize/CostReduction
- [ ] Forbidden element self-damage
- [ ] Silence percentage calculations
- [ ] Weakened status effectiveness
- [ ] Position-based aura interactions

---

### Balance Considerations

**High-Risk, High-Reward:**
- BD sacrifices pure Darkness power (50% silence vs 100%)
- Must absorb to unlock hybrids (reactive playstyle)
- Overflow drains energy (time-limited power spike)
- Forbidden elements deal self-damage (dangerous power)
- Friendly fire from aura (positioning matters)

**Build Diversity:**
- **High AbilitySize:** Massive aura, fast drain, zone control
- **High CostReduction:** Long overflow, efficient absorption, sustained pressure
- **High EffectDamage:** Strong aura damage/status, offensive overflow
- **High Resistance:** Reduces forbidden element self-damage, tankier
- **Balanced:** Flexible, adaptable to any situation

**Counter play:**
- Spread positioning to avoid aura
- Cleanse items to remove silence
- Don't cast expensive spells vs BD (less absorption)
- Force BD to absorb Darkness (revert hybrid)
- Exploit forbidden element self-damage (bait Light/Void casts)
- Target BD while in overflow (drain energy faster)

---

## Next Steps

### ✅ COMPLETED DESIGN SECTIONS

**1. Core Architecture** (Complete)
- Component pattern with replication
- Turn debt system
- 7-level tie-breaker cascade
- CombatOrchestrator coordination

**2. Status Effect System** (Complete)
- Hybrid timing (buffs at start, DOTs at end)
- EStatusEffectTiming enum
- Duration tracking on owner's turn
- Conditional triggers

**3. Real-Time Defense** (Complete)
- Block/Parry/Dodge mechanics
- Animation-driven windows
- 3v3 sequential defense
- Attack size vs dodge threshold

**4. Infusion Systems** (Complete)
- Spell Infusion (Casters): Size scaling
- Ability Power Infusion (Generic): Damage scaling
- Ability Element Infusion (Casters): Element toggle

**5. Action Execution** (Complete)
- Async event-driven pattern
- Centralized validation
- Animation notify integration
- Defense sequence implementation

**6. AI Decision Making** (Complete ✅ JUST FINISHED)
- Behavior Trees + Difficulty Tiers
- Tactical targeting (kill potential + threat)
- Full infusion support (all difficulties)
- Defense integration (difficulty-scaled success rates)
- Reaction time system

---

### ⏸️ DEFERRED TOPICS

**UI Integration** (Defer until implementation phase)
- Radial menu details
- Turn order preview
- Defense prompt design
- Spell charging UI
- Ability infusion UI

**BrokenDarkness Absorption** (Complex mechanic, future discussion)
- Integration with defense system
- Absorption state tracking
- Multi-target absorption rules
- Expiration mechanics

**Advanced Mechanics** (Post-launch features)
- Combo system
- Elemental reactions
- Equipment bonuses
- Ultimate cooldown system

---

## Implementation Readiness

### Core Systems Ready for Development

**Week 1: Foundation**
```cpp
Priority 1: TurnManager
- Turn debt calculation
- Tie-breaker cascade
- Speed change handling
- Dead character skipping

Priority 2: CharacterDataComponent
- Replication setup
- HP/EP tracking
- Stat access methods
- Server-authoritative changes
```

**Week 2: Combat Flow**
```cpp
Priority 3: CombatOrchestrator
- System coordination
- Turn lifecycle management
- Win condition checking

Priority 4: StatusEffectManager
- Effect timing implementation
- Duration tracking
- Conditional triggers

Priority 5: ActionExecutor
- Action validation
- Spell execution with infusion
- Ability execution with infusion
- Animation notify hooks

Priority 6: DamageCalculator
- Formula implementation from CharacterData
- Element calculations
- Infusion modifiers
```

**Week 3: Real-Time Features**
```cpp
Priority 7: DefenseSystem
- Defense window timing
- Block/Parry/Dodge logic
- Attack size calculations
- Sequential multi-target defense

Priority 8: InfusionSystems
- Spell charging UI (hold button)
- Ability power charging (Generic)
- Element toggle (Casters)
- Energy cost calculations
```

**Week 4: AI & Polish**
```cpp
Priority 9: AIDecisionManager
- Behavior Tree implementation
- Difficulty tier settings
- Target selection logic
- Infusion decision algorithms
- Defense choice logic
- Reaction time delays

Priority 10: Integration & Testing
- End-to-end combat flow
- Multiplayer testing
- Difficulty balancing
- Performance optimization
```

---

### Testing Strategy

**Unit Tests:**
- Turn debt calculations
- Tie-breaker cascade determinism
- Damage formulas
- Infusion multipliers
- Defense damage reduction

**Integration Tests:**
- Full combat flow (turn start → action → defense → turn end)
- Status effect timing
- Multi-target attacks with defense
- AI decision making per difficulty

**Multiplayer Tests:**
- Server authority enforcement
- Replication accuracy
- Defense input validation
- Turn synchronization

**Balance Tests:**
- Speed investment ROI
- Infusion energy efficiency
- Defense success rates per difficulty
- AI competitiveness per tier

---

## Next Steps

**Option A: Begin Implementation**
- Start with TurnManager.h/cpp
- Build CharacterDataComponent
- Create test combat scenario

**Option B: Continue Design**
- Deep dive on BrokenDarkness absorption
- UI/UX detailed specifications
- Advanced combo system design

**Option C: Review & Refine**
- Review complete design document
- Identify any gaps or inconsistencies
- Make final adjustments

**Your choice?**

---

### Implementation Timeline

**Phase 1: Core TurnManager (Week 1)**
- CharacterDataComponent with replication
- TurnManager with turn debt system
- Tie-breaker cascade
- Dead character handling
- Debug tools (context menu, logging)

**Phase 2: Integration (Week 2)**
- CombatOrchestrator design
- StatusEffectManager integration
- Speed buff/debuff system
- Turn transition hooks

**Phase 3: Combat Systems (Week 3)**
- ActionExecutor
- DamageCalculator
- AbsorptionManager
- EnemyAI

**Phase 4: UI & Polish (Week 4)**
- BattleUIManager
- RadialMenuWidget
- Turn order display
- VFX integration

---

## Design Benefits Summary

### Architectural Wins

1. **Single Responsibility**
   - TurnManager: Only tracks whose turn it is
   - No damage calculation, no action execution
   - Clean separation of concerns

2. **Scalability**
   - 1v1 to 5v5+ with zero code changes
   - Variable team sizes supported
   - No hardcoded player/enemy distinctions

3. **Multiplayer-Ready**
   - Server-authoritative turn management
   - Full replication support
   - Network-efficient (minimal state sync)

4. **Production-Grade**
   - Battle-tested turn debt algorithm
   - Comprehensive tie-breaker system
   - Deterministic for testing

### Gameplay Wins

1. **Speed Investment Value**
   - Clear ROI on speed stats
   - Natural double/triple turn mechanics
   - Smooth scaling (no breakpoints)

2. **Build Diversity**
   - Speed builds viable
   - Underdog mechanics reward creativity
   - Every stat matters for ties

3. **Player Understanding**
   - Predictable turn order
   - Clear speed thresholds (2:1, 3:1, etc.)
   - Visual feedback opportunities

4. **Flexibility**
   - Speed buffs impactful
   - Resurrection simple
   - Dynamic turn order

---

---

## Production Readiness Summary

### Complete Design Coverage

**✅ LOCKED DECISIONS:**

1. **Architecture** - Component-based with replication for multiplayer
2. **Turn Order** - Turn debt system with speed-based ratios
3. **Combat State** - CombatOrchestrator coordinates all subsystems
4. **Status Effects** - Hybrid timing (buffs at start, DOTs at end) with separate enum
5. **Action Execution** - Async event-driven for network compatibility
6. **Defense System** - Real-time reaction windows during animations
7. **Infusion Mechanics** - Three distinct systems for different classes

---

### System Completeness

| System | Status | Implementation Priority |
|--------|--------|------------------------|
| **TurnManager** | ✅ Design Complete | Week 1 |
| **CharacterDataComponent** | ✅ Design Complete | Week 1 |
| **CombatOrchestrator** | ✅ Design Complete | Week 1-2 |
| **StatusEffectManager** | ✅ Design Complete | Week 2 |
| **ActionExecutor** | ✅ Design Complete | Week 2 |
| **DamageCalculator** | ✅ Design Complete | Week 2 |
| **DefenseSystem** | ✅ Design Complete | Week 3 |
| **InfusionSystems** | ✅ Design Complete | Week 3 |
| **BattleUIManager** | ⏸️ Deferred | Week 4 |
| **AIDecisionManager** | ⏸️ Deferred | Week 4 |
| **AbsorptionManager** | ⏸️ Deferred | Later discussion |

---

### Key Design Principles Maintained

**✅ Single Responsibility**
- Each system has one job and does it well
- TurnManager only tracks turns
- ActionExecutor only executes actions
- DamageCalculator only calculates numbers

**✅ Production-Grade Multiplayer**
- Server-authoritative combat
- Full replication support
- Async execution for network latency
- Component-based for easy replication

**✅ No Lazy Work**
- Clean architecture, no shortcuts
- Proper separation of concerns
- Scalable from 1v1 to 5v5
- Testable systems

**✅ Player = Enemy**
- No hardcoded player/enemy distinctions
- Controller determines AI vs Human
- Characters defined by data assets only
- Perfect for "friend as boss" feature

---

### Technical Highlights

**Speed System:**
```
Turn Order = WorldBody + TurnSpeed (Mind substat)
Range: 0-28 possible
Debt system creates natural double/triple turns
No hard thresholds, smooth scaling
```

**Tie-Breaker Cascade:**
```
1. Turn Order Speed (primary)
2. Attack Speed (Body substat)
3. Underdog Advantage (lower total stats wins)
4. World Body → Mind → Spirit
5. Team position (deterministic)
```

**Defense System:**
```
Block: 50% reduction (always works)
Parry: 70% reduction + 30% reflect (tight window)
Dodge: 100% avoidance (if spell small enough)
Window: 0.3-0.5s (animation-driven)
Failure: Full damage (high risk/reward)
```

**Infusion Systems:**
```
Spell (Casters):    Size ×1.5/2.0, Cost ×1.3/1.6
Ability (Generic):  Damage ×1.3/1.6, Cost ×1.3/1.6
Element (Casters):  Add element, -30% damage, +50% cost
```

---

### Implementation Strategy

**Phase 1: Core Systems (Week 1)**
```cpp
// Create foundational classes
- CharacterDataComponent with replication
- TurnManager with turn debt system
- Tie-breaker cascade
- Dead character handling
- Debug tools (context menus, logging)
```

**Phase 2: Combat Flow (Week 2)**
```cpp
// Build combat orchestration
- CombatOrchestrator actor
- StatusEffectManager with timing enums
- ActionExecutor with validation
- DamageCalculator with CharacterData formulas
- Speed buff integration
```

**Phase 3: Real-Time Features (Week 3)**
```cpp
// Add player-interactive systems
- Defense window system
- Block/Parry/Dodge mechanics
- Spell infusion charging
- Ability power infusion
- Element infusion integration
- Animation notify hooks
```

**Phase 4: Polish & AI (Week 4)**
```cpp
// Complete the experience
- BattleUIManager (HUD, radial menu, prompts)
- AIDecisionManager (decision-making)
- VFX integration (size scaling, elements)
- Audio system (combat sounds, cues)
```

---

### Testing Checklist

**Core Combat:**
- [ ] Turn order calculates correctly for 2-6 combatants
- [ ] Speed differences create proper ratios (2:1, 3:1, etc.)
- [ ] Tie-breaker cascade resolves all conflicts deterministically
- [ ] Dead characters skip turns but can be resurrected
- [ ] Speed buffs trigger turn order recalculation

**Status Effects:**
- [ ] Buffs process at start of own turn
- [ ] DOTs process at end of own turn
- [ ] Durations tick on owner's turn only
- [ ] Conditional triggers (OnTrigger) activate correctly
- [ ] Persistent effects apply continuously

**Defense System:**
- [ ] Defense windows open/close at correct animation times
- [ ] Block reduces damage by 50%
- [ ] Parry reduces 70% and reflects 30%
- [ ] Dodge works for small spells, fails for large
- [ ] Failed defense = full damage
- [ ] 3v3 gives each character individual window

**Infusion Systems:**
- [ ] Spell infusion (casters only) increases size correctly
- [ ] Ability power infusion (generic only) increases damage
- [ ] Element infusion (casters only) adds element with penalty
- [ ] Energy costs scale properly
- [ ] Charge time thresholds work (1s, 2s)
- [ ] Size affects dodge viability

**Multiplayer:**
- [ ] Server authoritative damage/healing
- [ ] HP/EP replicate to all clients
- [ ] Turn changes broadcast correctly
- [ ] Defense inputs from clients validated on server
- [ ] Action validation prevents cheating

---

### Known Design Gaps (Future Discussion)

**BrokenDarkness Absorption:**
- Integration with defense system
- When absorption available vs standard defense
- Absorption state tracking
- Multi-target absorption rules

**AI Decision Making:**
- Simple heuristic vs complex evaluation
- Difficulty scaling
- Charge/infusion decision-making
- Defense timing for AI-controlled characters

**Radial Menu Details:**
- Three-ring navigation structure
- Spell infusion UI (hold-to-charge visual)
- Ability power infusion UI
- Element toggle interface
- Animation/VFX during selection

**Advanced Mechanics:**
- Combo system (future)
- Elemental reactions (future)
- Equipment bonuses (future)
- Ultimate cooldown UI (future)

---

### Success Metrics

**Architecture Quality:**
- ✅ Single responsibility per system
- ✅ No god objects
- ✅ Clear separation of concerns
- ✅ Testable in isolation
- ✅ Scalable team sizes

**Gameplay Quality:**
- ✅ Turn order predictable and fair
- ✅ Speed investment meaningful
- ✅ Defense creates skill expression
- ✅ Infusion adds strategic depth
- ✅ Status effects feel impactful

**Multiplayer Quality:**
- ✅ Server-authoritative combat
- ✅ Full replication support
- ✅ Network-efficient design
- ✅ Anti-cheat validation

**Production Quality:**
- ✅ No lazy shortcuts
- ✅ Clean, maintainable code
- ✅ Designer-friendly data assets
- ✅ Performance-conscious
- ✅ Debug tools included

---

## Conclusion

**Status:** ✅ Design Phase Complete

**Ready for Implementation:** YES

All core combat systems have been designed with production-grade architecture:
- Clean separation of concerns
- Multiplayer-ready from day one
- No hardcoded player/enemy distinctions
- Scalable from 1v1 to 5v5+
- High skill ceiling with defense mechanics
- Strategic depth via infusion systems
- Status effects with proper timing
- **BrokenDarkness absorption system** (unique element mechanic)

The system supports your vision of:
- Expedition 33-style real-time defense
- Turn-based strategic action selection
- Character-to-enemy system (player = enemy)
- Friend as boss feature
- 3v3 team battles with 5-position system
- Anime aesthetic with elemental combat
- **BrokenDarkness** as a high-risk, high-reward element with absorption, hybrids, and overflow

**Next Steps:**
1. Review this document for any corrections
2. Begin implementation with TurnManager.h/cpp
3. Build CharacterDataComponent with replication
4. Create CombatOrchestrator actor
5. Implement BrokenDarkness AbsorptionManager
6. Iterate based on playtesting

---

**Document Version:** 4.0  
**Last Updated:** November 25, 2025  
**Status:** ✅ **COMPLETE** - Ready for Implementation  
**Total Pages:** ~160 pages of comprehensive design documentation

**Design Coverage:**
- ✅ Core Architecture (Component pattern, multiplayer replication)
- ✅ Turn Management (Debt system, tie-breakers, speed mechanics)
- ✅ Status Effects (Hybrid timing, conditional triggers)
- ✅ Real-Time Defense (Block/Parry/Dodge with animation windows)
- ✅ Infusion Systems (3 types: spell size, ability power, element)
- ✅ Action Execution (Async event-driven, validation, animation)
- ✅ AI Decision Making (Behavior Trees, 4 difficulty tiers, full parity)
- ✅ **BrokenDarkness Absorption** (Hybrid spells, overflow aura, 3 silence types, positioning)

**Deferred Topics:**
- ⏸️ UI/UX Details (radial menu, prompts, preview)
- ⏸️ Reality Element Design (abstract element mechanics)
- ⏸️ Advanced Mechanics (combos, reactions, equipment)

---

**End of Document**
