// CombatCharacter.cpp

#include "Character/CombatCharacter.h"

#include "Character/BattleConfigComponent.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/AI/CombatAIController.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Weapons/WeaponMeshComponent.h"
#include "Infusion/InfusionVFXComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "AbilitySystemComponent.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "GAS/WoRTurnEffectTracker.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "GAS/WoRAttributeSets.h"

ACombatCharacter::ACombatCharacter()
{
    // CharacterDataComponent stays first: native components run BeginPlay in
    // constructor-declaration order, and its cascade seeds what the rest read.
    CharacterDataComponent = CreateDefaultSubobject<UCharacterDataComponent>(TEXT("CharacterDataComponent"));
    WeaponMeshComponent = CreateDefaultSubobject<UWeaponMeshComponent>(TEXT("WeaponMeshComponent"));
    CurrencyComponent = CreateDefaultSubobject<UCurrencyComponent>(TEXT("CurrencyComponent"));
    InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
    CrystalInventoryComponent = CreateDefaultSubobject<UCrystalInventoryComponent>(TEXT("CrystalInventoryComponent"));
    EvolutionInventoryComponent = CreateDefaultSubobject<UEvolutionInventoryComponent>(TEXT("EvolutionInventoryComponent"));
    InfusionVFXComponent = CreateDefaultSubobject<UInfusionVFXComponent>(TEXT("InfusionVFXComponent"));
    LoadoutComponent = CreateDefaultSubobject<ULoadoutComponent>(TEXT("LoadoutComponent"));
    BrokenDarknessComponent = CreateDefaultSubobject<UBrokenDarknessManager>(TEXT("BrokenDarknessComponent"));
    BattleConfigComponent = CreateDefaultSubobject<UBattleConfigComponent>(TEXT("BattleConfigComponent"));

    // GAS stage 2 (B-041). Mixed replication suits player characters (GASMigration.md §1); the
    // per-AI Minimal mode waits until co-op, when replication is actually exercised.
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
    VitalsAttributes = CreateDefaultSubobject<UWoRAttributeSet_Vitals>(TEXT("VitalsAttributes"));
    StatsAttributes = CreateDefaultSubobject<UWoRAttributeSet_Stats>(TEXT("StatsAttributes"));
    ResistanceAttributes = CreateDefaultSubobject<UWoRAttributeSet_Resistance>(TEXT("ResistanceAttributes"));
    AttributeSyncComponent = CreateDefaultSubobject<UWoRAttributeSyncComponent>(TEXT("AttributeSyncComponent"));
    TurnEffectTracker = CreateDefaultSubobject<UWoRTurnEffectTracker>(TEXT("TurnEffectTracker"));
    EffectMirrorComponent = CreateDefaultSubobject<UWoREffectMirrorComponent>(TEXT("EffectMirrorComponent"));

    // Every combat character gets an AI controller by default, so engine
    // possession can answer "who is driving this?" truthfully. PC0 takes over the
    // player's own pawn in ABattleGameMode, unpossessing the auto-spawned one.
    // The controller auto-spawns in PostInitializeComponents — i.e. inside
    // FinishSpawningActor, which the deferred-spawn path already calls.
    AIControllerClass = ACombatAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}
