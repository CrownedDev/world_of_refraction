// CombatCharacter.h
// C++ base for all combat-capable characters (Cluster T-C1a). Exists to
// native-create UCharacterDataComponent: Blueprint-panel (SCS) components are
// not instantiated until FinishSpawningActor runs the construction script, so
// BattleGameMode::SpawnCombatant's deferred-spawn window (assign CharacterData
// BEFORE FinishSpawning so the component's BeginPlay cascade reads the right
// asset) only works with a native component.
//
// Component-promotion cluster: the combat component stack is being migrated off
// the BP SCS panel onto this class one component per commit. ORDER MATTERS —
// native components register (and so run BeginPlay) before SCS ones, and among
// natives in constructor-declaration order. CharacterDataComponent MUST stay
// first: its BeginPlay cascade seeds the state the others read.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "CombatCharacter.generated.h"

class UCharacterDataComponent;
class UWeaponMeshComponent;
class UCurrencyComponent;
class UInventoryComponent;
class UCrystalInventoryComponent;
class UEvolutionInventoryComponent;
class UInfusionVFXComponent;
class ULoadoutComponent;
class UBrokenDarknessManager;
class UBattleConfigComponent;
class UAbilitySystemComponent;
class UWoRAttributeSyncComponent;
class UWoRTurnEffectTracker;
class UWoREffectMirrorComponent;
class UWoRAttributeSet_Vitals;
class UWoRAttributeSet_Stats;
class UWoRAttributeSet_Resistance;

UCLASS()
class WORLD_OF_REFRACTION_API ACombatCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ACombatCharacter();

    virtual UAbilitySystemComponent *GetAbilitySystemComponent() const override { return AbilitySystemComponent; }

    /** Native so it exists between SpawnActorDeferred and FinishSpawning.
     *  Declared FIRST — its BeginPlay cascade seeds state the others read. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
    TObjectPtr<UCharacterDataComponent> CharacterDataComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
    TObjectPtr<UWeaponMeshComponent> WeaponMeshComponent;

    /** OBSOLETE for the player's party (D-046, B-151): they spend from the player's wallet on
     *  AWoRPlayerState (CurrencyWallet::FindFor). Still the wallet of a character with no player
     *  wallet behind it (an enemy, a Blueprint game mode without AWoRPlayerState). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
    TObjectPtr<UCurrencyComponent> CurrencyComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
    TObjectPtr<UInventoryComponent> InventoryComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
    TObjectPtr<UCrystalInventoryComponent> CrystalInventoryComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
    TObjectPtr<UEvolutionInventoryComponent> EvolutionInventoryComponent;

    /** Declared last: its BeginPlay caches CharacterData / Loadout / WeaponMesh
     *  pointers, so it reads cleanest once those exist. (Lookup itself is
     *  order-safe — every component is registered before any BeginPlay runs.) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
    TObjectPtr<UInfusionVFXComponent> InfusionVFXComponent;

    /** The loadout's class is the asset's: GetCharacterClass() reads
     *  CharacterData->CharacterClass (Generic without CharacterData). The
     *  component carries no class field since B-153. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
    TObjectPtr<ULoadoutComponent> LoadoutComponent;

    /** Present on every combat character, inert unless the owner is BD: all
     *  behaviour gates on bIsFlipped, seeded from CharacterData->bBrokenDarknessInnate
     *  via the cascade's InitializeBornBrokenDarkness call. Uniform contract beats
     *  a conditionally-present component. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
    TObjectPtr<UBrokenDarknessManager> BrokenDarknessComponent;

    /** Per-encounter placement/ownership, set by ABattleGameMode at spawn. Reads
     *  nothing from its siblings during init, so it is order-free and appends
     *  last — unlike the components above, it has no cascade dependency. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
    TObjectPtr<UBattleConfigComponent> BattleConfigComponent;

    /** GAS (B-041, Design/GASMigration.md §1): the pawn owns its ASC, as battle pawns are spawned per fight.
     *  Holds the attribute sets; nothing gameplay-facing reads it yet (Q-041). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    /** Mirrors the composed stats into the ASC's attributes. Declared after every component it reads,
     *  so its BeginPlay runs once their cascade has seeded them. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<UWoRAttributeSyncComponent> AttributeSyncComponent;

    /** GAS stage 3 (B-042): counts this pawn's turns for its turn-counted GameplayEffects. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<UWoRTurnEffectTracker> TurnEffectTracker;

    /** GAS stage 3 (B-042c): mirrors this pawn's stored effects onto its ASC for `wor.Effects.UseGAS`. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<UWoREffectMirrorComponent> EffectMirrorComponent;

    /** B-230: the status bar, replicated for a client's HUD (the buildup manager is server-only). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<class UStatusBarStateComponent> StatusBarState;

    /** B-231: the server's montages and hits, multicast to the clients (the executor runs on the server only). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<class UCombatPresentationComponent> CombatPresentation;

    /** B-230b: the grid row and slot, replicated for a client's readers (the grid subsystem is server-side). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<class UCombatGridSlotComponent> GridSlot;

private:
    /** Default subobjects, so the ASC finds them at InitializeComponent on every machine (the
     *  replicated sets, never a client-made copy). Read through the ASC, not these pointers. */
    UPROPERTY()
    TObjectPtr<UWoRAttributeSet_Vitals> VitalsAttributes;

    UPROPERTY()
    TObjectPtr<UWoRAttributeSet_Stats> StatsAttributes;

    UPROPERTY()
    TObjectPtr<UWoRAttributeSet_Resistance> ResistanceAttributes;
};
