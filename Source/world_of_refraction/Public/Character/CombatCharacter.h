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

UCLASS()
class WORLD_OF_REFRACTION_API ACombatCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ACombatCharacter();

    /** Native so it exists between SpawnActorDeferred and FinishSpawning.
     *  Declared FIRST — its BeginPlay cascade seeds state the others read. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
    TObjectPtr<UCharacterDataComponent> CharacterDataComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
    TObjectPtr<UWeaponMeshComponent> WeaponMeshComponent;

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

    /** CharacterClass carries no authored default here: the cascade overwrites it
     *  from the asset (LoadoutComponent.cpp:1900, CharacterClass =
     *  CharacterData->CharacterClass) before EnsureDefaultLoadout reads it. The
     *  old SCS value was a fallback for a null-CharacterData character only. */
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
};
