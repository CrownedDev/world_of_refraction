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

void ACombatAIController::OnUnPossess()
{
	Super::OnUnPossess();

	// Fires at exactly the displacement moment: AController::OnPossess unpossesses
	// the incumbent before taking the pawn. Also covers the pawn simply being
	// destroyed — either way this controller is now purposeless.
	Destroy();
}
