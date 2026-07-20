// CombatAIController.h
// Possession host for non-player combatants (Encounter Composition, Arc 1).
//
// Deliberately behaviourless. Combat decisions stay in UAIDecisionManager, which
// drives pawns directly as a GameInstance subsystem and never consults a
// controller. This class exists so that engine control queries tell the truth:
//
//   Pawn->IsPlayerControlled()  -> a human is driving
//   Pawn->IsBotControlled()     -> AI is driving
//
// Before this, non-player combatants had NO controller at all, so both queries
// returned false and "is this AI?" had to be answered from a data-asset flag
// (UCharacterData::bIsAIControlled) that conflated authored identity with runtime
// control. That flag is removed in the commit that wires AutoPossessAI.
//
// No BehaviorTree, no Blackboard, no perception — adding any would put a second
// decision-maker beside UAIDecisionManager.

#pragma once

#include "AIController.h"
#include "CoreMinimal.h"
#include "CombatAIController.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API ACombatAIController : public AAIController
{
	GENERATED_BODY()

public:
	ACombatAIController();
};
