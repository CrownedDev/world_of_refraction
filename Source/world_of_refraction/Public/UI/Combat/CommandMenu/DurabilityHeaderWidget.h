// DurabilityHeaderWidget.h
// Read-only header at the top of the combat command menu.
// Shows the active character's durability resources (ring, weapon).
// Updates per-cast via RingManager / WeaponManager delegates.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DurabilityHeaderWidget.generated.h"

class UTextBlock;
class URingManager;
class URingData;
class UItemData;
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

	/** Ring manager binding for per-cast durability updates */
	TWeakObjectPtr<URingManager> BoundRingManager;

	/** Crystal manager binding for unified per-cast durability updates.
	 *  Replaces the per-WeaponManager binding (Phase A commit 3). Ring
	 *  durability still flows via URingManager until commit 4. */
	TWeakObjectPtr<UCrystalManager> BoundCrystalManager;

	/** Update slot 1 with ring durability. Called when character has a ring source. */
	void UpdateSlot1FromRing();

	/** Update slot 1 with weapon durability. Called when character has a weapon
	 *  source but no ring source. Reserved for Item 32 — currently always hides. */
	void UpdateSlot1FromWeapon();

	/** Update slot 2 with weapon durability. Called when character has both a ring
	 *  AND a weapon source. Reserved for Item 32 — currently always hides. */
	void UpdateSlot2FromWeapon();

	/** Hide slot 1 (used when character has no durability resources). */
	void HideSlot1();

	/** Hide slot 2 (used when character has only one durability resource). */
	void HideSlot2();

	/** Bind/unbind ring manager delegates safely. */
	void BindRingManager(URingManager *RingMgr);
	void UnbindRingManager();

	/** Bind/unbind crystal manager delegates safely. */
	void BindCrystalManager(UCrystalManager *CrystalMgr);
	void UnbindCrystalManager();

	/** Delegate handler for OnRingDurabilityChanged */
	UFUNCTION()
	void HandleRingDurabilityChanged(AActor *Actor, URingData *Ring, int32 NewDurability, int32 MaxDurability);

	/** Delegate handler for OnRingCrystalBroken (re-read active ring after auto-switch) */
	UFUNCTION()
	void HandleRingCrystalBroken(AActor *Actor, URingData *Ring, UItemData *Crystal);

	/** Delegate handler for UCrystalManager::OnCrystalDurabilityChanged.
	 *  Filters Holder by Cast<UWeaponData> — ring crystals still flow via
	 *  OnRingDurabilityChanged (URingManager) until commit 4. */
	UFUNCTION()
	void HandleCrystalDurabilityChanged(AActor *Actor, UObject *Holder, int32 NewDurability, int32 MaxDurability);

	bool bIsBound = false;
};
