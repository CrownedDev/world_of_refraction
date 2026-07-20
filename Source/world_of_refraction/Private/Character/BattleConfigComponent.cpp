// BattleConfigComponent.cpp

#include "Character/BattleConfigComponent.h"

#include "Party/Party.h"

UBattleConfigComponent::UBattleConfigComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBattleConfigComponent::SetBattleContext(UParty *InOwningParty, int32 InTeamIndex, const FText &InDisplayContext)
{
	OwningParty = InOwningParty;
	TeamIndex = InTeamIndex;
	DisplayContext = InDisplayContext;

	// Keep the grid cell's own TeamIndex aligned even if the cell was assigned
	// first — this component is the authority for which side the pawn is on.
	GridPosition.TeamIndex = InTeamIndex;
}

void UBattleConfigComponent::SetGridPosition(const FCombatGridPosition &InGridPosition)
{
	GridPosition = InGridPosition;
	GridPosition.TeamIndex = TeamIndex;
}
