// CombatCharacter.h
// C++ base for all combat-capable characters (Cluster T-C1a). Exists to
// native-create UCharacterDataComponent: Blueprint-panel (SCS) components are
// not instantiated until FinishSpawningActor runs the construction script, so
// BattleGameMode::SpawnCombatant's deferred-spawn window (assign CharacterData
// BEFORE FinishSpawning so the component's BeginPlay cascade reads the right
// asset) only works with a native component.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CombatCharacter.generated.h"

class UCharacterDataComponent;

UCLASS()
class WORLD_OF_REFRACTION_API ACombatCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ACombatCharacter();

    /** Native so it exists between SpawnActorDeferred and FinishSpawning. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
    TObjectPtr<UCharacterDataComponent> CharacterDataComponent;
};
