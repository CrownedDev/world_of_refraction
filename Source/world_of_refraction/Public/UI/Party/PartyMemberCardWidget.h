// PartyMemberCardWidget.h
// C++ base for WBP_PartyMemberCard (Arc 1.5b Cluster 2): one card per party
// slot — name, class, element tint, max HP/EP. ALL logic lives here; the WBP
// supplies layout only, wiring by BindWidgetOptional name (the shop-widget
// convention).
//
// The card is PASSIVE: no subsystem reads, no delegate binding — the Cluster 3
// display widget owns the party data flow and drives its three cards through
// SetMemberData / SetEmpty. Cards are built once and toggled between populated
// and ghost states, never recreated (TransBuffer mitigation), so SetEmpty must
// fully reset every field SetMemberData touches.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Styling/SlateBrush.h" // FSlateBrush value member (placeholder cache)
#include "PartyMemberCardWidget.generated.h"

class UCharacterData;
class UImage;
class UTextBlock;

UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UPartyMemberCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPartyMemberCardWidget(const FObjectInitializer &ObjectInitializer);

	/** Populate the card from a member's CharacterData. bIsLeader suffixes the
	 *  name with " (You)" — slot 0 is the player per the design lock. Null Data
	 *  routes to SetEmpty rather than asserting. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	void SetMemberData(UCharacterData *Data, bool bIsLeader);

	/** Ghost state for a vacant slot: "Empty" name, blank class/HP/EP, dimmed
	 *  element bar. Full reset of every populated field. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	void SetEmpty();

	/** True while showing the ghost state (SetEmpty, or never populated). */
	UFUNCTION(BlueprintPure, Category = "Party")
	bool IsEmpty() const { return bIsEmpty; }

protected:
	/** ElementBar opacity in the ghost state (populated cards are opaque).
	 *  WBP-tunable; default from PartyCardConstants. */
	UPROPERTY(EditDefaultsOnly, Category = "Party", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float GhostAlpha;

	// ==================== BINDWIDGET (names must match the WBP tree) ====================

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Party|Widgets")
	TObjectPtr<UImage> PortraitImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Party|Widgets")
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Party|Widgets")
	TObjectPtr<UTextBlock> ClassText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Party|Widgets")
	TObjectPtr<UImage> ElementBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Party|Widgets")
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Party|Widgets")
	TObjectPtr<UTextBlock> EnergyText;

private:
	bool bIsEmpty = true;

	/** The WBP-authored placeholder brush, captured before the first C++ write so
	 *  a null Portrait (today's normal case) and the ghost state can restore the
	 *  designer's placeholder instead of leaving a stale member portrait. */
	FSlateBrush DefaultPortraitBrush;
	bool bPortraitBrushCached = false;

	void CachePortraitBrushOnce();
};
