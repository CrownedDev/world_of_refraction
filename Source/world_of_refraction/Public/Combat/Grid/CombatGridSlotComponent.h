// CombatGridSlotComponent.h
// B-230b (Design/CoopStateOwnership.md: the grid slot is actor state, replicated for the HUD): a combatant's row and
// slot as the server's UCombatGridSubsystem holds them, replicated so a client reads rows too. The subsystem stays
// the source of truth (server); it writes here on every change, and its position reads fall back to this component
// for an actor it doesn't hold (a client's grid is empty: the fight runs on the server).
// Debug: GetDebugString, wor.Grid.PrintSlots.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Grid/FCombatGridPosition.h"
#include "CombatGridSlotComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGridSlotChanged, AActor *, Actor);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UCombatGridSlotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatGridSlotComponent();

	/** Server: the owner stands at Position (the grid subsystem's change). */
	void SetSlot(const FCombatGridPosition &Position);

	/** Server: the owner left the grid (the fight ended, it was removed). */
	void ClearSlot();

	/** The replicated slot; false when the owner is off the grid. */
	bool GetSlot(FCombatGridPosition &OutPosition) const;

	/** Fired on every machine when the slot changes (a client: when it arrives). */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Grid")
	FOnGridSlotChanged OnSlotChanged;

	/** The owner's component, or null. */
	static UCombatGridSlotComponent *Find(const AActor *Actor);

	FString GetDebugString() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_Slot();

	UPROPERTY(ReplicatedUsing = OnRep_Slot)
	FCombatGridPosition Slot;

	UPROPERTY(ReplicatedUsing = OnRep_Slot)
	bool bOnGrid = false;
};
