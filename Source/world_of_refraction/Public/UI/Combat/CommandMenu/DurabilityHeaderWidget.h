// DurabilityHeaderWidget.h
// Read-only header at the top of the combat command menu.
// Shows the active character's durability resources (ring, weapon).
// Updates per-cast via RingManager / WeaponManager delegates.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DurabilityHeaderWidget.generated.h"

class UTextBlock;
class URingData;
class UEvolutionItemData;
class UCrystalManager;
class UWeaponData;

UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UDurabilityHeaderWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Refresh the header for the given actor. Reads class-specific
	 *  durability state (ring, weapon) and populates the lines.
	 *  Called by CombatCommandMenuWidget on each menu open. */
	UFUNCTION(BlueprintCallable, Category = "Durability Header")
	void RefreshForActor(AActor *InActor);

	/** Hide all lines and reset bound state. */
	UFUNCTION(BlueprintCallable, Category = "Durability Header")
	void Hide();

	/** Show the header (visibility flip — content is set by Refresh). */
	UFUNCTION(BlueprintCallable, Category = "Durability Header")
	void Show();

protected:
	virtual void NativeConstruct() override;
	virtual void BeginDestroy() override;

	/** Slot 1 durability — primary resource. Slot 1 holds the ring durability
	 *  if the character has a ring source; otherwise the weapon durability if
	 *  they have a weapon-with-crystal; otherwise hidden.
	 *  Bound from BP. Required. */
	UPROPERTY(meta = (BindWidget))
	UTextBlock *Slot1DurText = nullptr;

	/** Slot 2 durability — secondary resource. Slot 2 holds the weapon
	 *  durability if the character has both a ring source AND a weapon-with-
	 *  crystal; otherwise hidden.
	 *  Bound from BP. Optional — header is single-line when no secondary
	 *  resource is present. */
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock *Slot2DurText = nullptr;

private:
	/** Currently bound actor. Header refreshes when this character's resources change. */
	TWeakObjectPtr<AActor> BoundActor;

	/** Crystal manager binding for unified per-cast durability and break
	 *  events. Replaces the per-WeaponManager and per-RingManager bindings
	 *  the widget used to hold. */
	TWeakObjectPtr<UCrystalManager> BoundCrystalManager;

	/** Update slot 1 with ring durability. Called when character has a ring source. */
	void UpdateSlot1FromRing();

	/** Update slot 1 with weapon durability. Called when character has a weapon
	 *  source but no ring source. Populates the slot with the active weapon's
	 *  slotted-crystal durability and makes the line visible. */
	void UpdateSlot1FromWeapon();

	/** Update slot 2 with weapon durability. Called when character has both a ring
	 *  AND a weapon source. Populates the slot with the active weapon's
	 *  slotted-crystal durability and makes the line visible. */
	void UpdateSlot2FromWeapon();

	/** Hide slot 1 (used when character has no durability resources). */
	void HideSlot1();

	/** Hide slot 2 (used when character has only one durability resource). */
	void HideSlot2();

	/** Bind/unbind crystal manager delegates safely. */
	void BindCrystalManager(UCrystalManager *CrystalMgr);
	void UnbindCrystalManager();

	/** Delegate handler for UCrystalManager::OnCrystalDurabilityChanged.
	 *  Routes weapon vs ring crystal updates to the right slot by holder type. */
	UFUNCTION()
	void HandleCrystalDurabilityChanged(AActor *Actor, UObject *Holder, int32 NewDurability, int32 MaxDurability);

	/** Delegate handler for UCrystalManager::OnCrystalBroken.
	 *  Re-detects resources after a break — auto-switch may change which
	 *  slot displays what. */
	UFUNCTION()
	void HandleCrystalBroken(AActor *Actor, UObject *Holder, UEvolutionItemData *Crystal);
};
