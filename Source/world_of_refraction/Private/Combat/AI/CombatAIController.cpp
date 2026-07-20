// CombatAIController.cpp

#include "Combat/AI/CombatAIController.h"

ACombatAIController::ACombatAIController()
{
	// Explicit: this controller owns no brain. AAIController defaults this true,
	// which calls RunBehaviorTree on possess — a no-op with no BT assigned, but
	// stating it here records the intent and keeps a future BT from starting by
	// accident. Combat decisions belong to UAIDecisionManager.
	bStartAILogicOnPossess = false;

	PrimaryActorTick.bCanEverTick = false;
}
