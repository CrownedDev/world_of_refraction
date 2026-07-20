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

	/** Self-destruct once displaced. A possession host with no pawn has no reason
	 *  to exist, and nothing else holds a reference to reap it.
	 *
	 *  This lives here rather than in the GameModes because the leak has two
	 *  sources and only one is reachable from a GameMode: AGameModeBase spawns
	 *  PC0 a pawn from DefaultPawnClass, that pawn auto-possesses an AI controller
	 *  in PostInitializeComponents (Controller is still null at that point), and
	 *  PC0 then displaces it — leaving a controller referenced by nothing before
	 *  any GameMode hook could see it. Every GameMode using an ACombatCharacter as
	 *  DefaultPawnClass leaks one per level load; making the controller reap
	 *  itself covers all of them, including future ones, with no per-site work.
	 *
	 *  ⚠️ Forecloses one thing: an AI controller can never outlive its pawn to
	 *  re-possess a respawn. Not a current use case — UAIDecisionManager drives
	 *  pawns directly and never holds a controller — but a respawn feature would
	 *  need to revisit this. */
	virtual void OnUnPossess() override;
};
