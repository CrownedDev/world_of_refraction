// HubHUDWidget.h
// B-443 (D-181, UIScreens #14, mockup screen_mockups/hub/hud/1_hub_hud_prompt.png): the hub and trial-map HUD's
// pieces, and (B-503g) the party strip, bottom left: B-503j (D-216) the battle HUD's party card per member, at the
// Hub level (names, the card fitted to them) or, in a run, the Trial level (HP, EP, the build-up bar). B-503k (D-217):
// in a run, the field attack strip ("Fire Sword") above the played member's card. The old UPartyDisplayWidget (the debug cards) is hidden unless wor.UI.DebugPartyBox shows it.
//   - The interact prompt: key + verb + what, floating above the thing the player faces.
//   - The money chip, top right: only near a vendor, or for a few seconds after the balance changed.
// A plain viewport widget (not a CommonUI screen: it takes no input), created by AWoRHubController for the local
// player and hidden by the root layout while a menu screen is up. What it shows comes from WoRHubHUD
// (HubHUDModel.h), which the specs read.
//
// What is in reach is read on a short timer (there is no "came into reach" event that covers every interactable).
// NativeTick only keeps the prompt over its world spot as the camera moves: the field is real time, and a
// world-anchored label has to follow the view each frame.
// OBSOLETE once this passes PIE: the old on-screen debug hints ("Press E to ...", WoRInteract::ShowHint).

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Currency/CurrencyTypes.h"
#include "UI/Hub/HubHUDModel.h"
#include "HubHUDWidget.generated.h"

class UBorder;
class UCurrencyComponent;
class UImage;
class USizeBox;
class UTextBlock;

/** B-503i (mockup hub/hud): a party strip card's frame: a dark fill and a thin light outline, the top-left corner cut.
 *  It only paints: put it behind the card's content in an overlay.
 *  OBSOLETE (B-503j): unused since the strip took the battle card's look (D-216); delete after B-503j's PIE pass. */
UCLASS()
class WORLD_OF_REFRACTION_API UHubHUDCardFrame : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Sets the fill and repaints (the field attack chip flashes with it). */
	void SetFillColor(const FLinearColor &Color)
	{
		FillColor = Color;
		InvalidateLayoutAndVolatility();
	}

	FLinearColor FillColor = FLinearColor::Black;
	FLinearColor OutlineColor = FLinearColor::White;

protected:
	virtual int32 NativePaint(const FPaintArgs &Args, const FGeometry &AllottedGeometry, const FSlateRect &MyCullingRect,
							  FSlateWindowElementList &OutDrawElements, int32 LayerId, const FWidgetStyle &InWidgetStyle,
							  bool bParentEnabled) const override;
};

/** B-503m5 (D-218, mockup hub/hud/v2/3_trial_wheel_open): the field wheel's round body: a dark disc, the picked slice
 *  lit, the lines between slices and the middle circle. It only paints: the names and the middle's text sit on top.
 *  OBSOLETE (B-514, D-222): the quick strip replaced the wheel and nothing builds this; delete after B-514's PIE pass. */
UCLASS()
class WORLD_OF_REFRACTION_API UHubHUDWheelRing : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Slice 0 at the top, clockwise; PickedSlice INDEX_NONE lights none. The disc fills the widget. */
	void SetWheel(int32 InSliceCount, int32 InPickedSlice, float InHubRadius)
	{
		SliceCount = InSliceCount;
		PickedSlice = InPickedSlice;
		HubRadius = InHubRadius;
		InvalidateLayoutAndVolatility();
	}
	int32 GetSliceCount() const { return SliceCount; }

	FLinearColor FillColor = FLinearColor::Black;
	FLinearColor HubColor = FLinearColor::Black;
	FLinearColor LitColor = FLinearColor::White;
	FLinearColor LineColor = FLinearColor::White;

protected:
	virtual int32 NativePaint(const FPaintArgs &Args, const FGeometry &AllottedGeometry, const FSlateRect &MyCullingRect,
							  FSlateWindowElementList &OutDrawElements, int32 LayerId, const FWidgetStyle &InWidgetStyle,
							  bool bParentEnabled) const override;

private:
	int32 SliceCount = 0;
	int32 PickedSlice = INDEX_NONE;
	float HubRadius = 0.0f;
};

UCLASS()
class WORLD_OF_REFRACTION_API UHubHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Reads what is in reach and the money, and updates both pieces. The timer calls it; so can a spec. */
	void Refresh();

	/** B-503k (D-217): the field attack just picked on the wheel: shows it on the strip (B-503m: no flash). The strip is
	 *  up in a trial map while the pawn has a field attack with a name. */
	void ShowFieldAttack(const struct FFieldAttackEntry &Entry);

	/** B-514 (D-222, mockup hub/hud/v2/3_trial_strip_open; was the B-503m4 wheel): the quick strip, a row of icon tiles
	 *  through the screen's middle, one per slice (piece of gear), the world dimmed behind. The picked slice's tile is
	 *  framed, with up / down arrows when it has more than one source; the InHand entry's tile is tagged IN HAND. Under
	 *  the row: the piece and its source ("Excalibur (Fire)") and, quieter, what the strip will read ("Fire Sword").
	 *  Key hints bottom right. Trial only, as the strip; false when there is nothing to show (the hub, headless, no
	 *  entries). Call again to move the pick. */
	bool ShowFieldAttackWheel(const TArray<struct FFieldAttackEntry> &Entries, int32 Picked, int32 InHand = INDEX_NONE);
	void HideFieldAttackWheel();
	/** How many pieces the strip can show (0: no strip here). */
	int32 GetFieldWheelCapacity() const { return StripTiles.Num(); }
	/** B-514: the slice whose tile is under the mouse cursor; INDEX_NONE off the tiles (bOnWheel false) or before the
	 *  strip's first paint. */
	int32 GetFieldWheelSliceUnderCursor(bool &bOnWheel) const;

	const FHubHUDPrompt &GetPrompt() const { return Prompt; }
	bool IsMoneyShown() const { return bMoneyShown; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry &MyGeometry, float InDeltaTime) override;

private:
	void BuildTree();
	void BindWallet();
	void PlacePrompt();

	UFUNCTION()
	void HandleCurrencyChanged(ECurrencyType Currency, uint8 SubKey, int32 NewBalance);

	FHubHUDPrompt Prompt;
	bool bMoneyShown = false;
	/** World seconds when the place's money last changed; negative: not yet. */
	float MoneyChangedAtSeconds = -1.0f;
	FTimerHandle RefreshTimer;
	/** B-463b: the tracked quest's line, read less often than the prompt (it builds the quest list). */
	FTimerHandle QuestTimer;
	void RefreshQuestLine();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PromptBox;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PromptKey;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PromptVerb;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PromptWhat;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> MoneyBox;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MoneyValue;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MoneyLabel;
	/** B-463b: the tracked quest's step, in a run. */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> QuestBox;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> QuestText;
	/** B-503k (D-217): the field attack strip above each card, built at the Trial level only (none in the hub). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> AttackStrips;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> AttackStripTexts;
	/** The card whose strip shows (WoRHubHUD::FindFieldAttackCard); INDEX_NONE: none. */
	int32 AttackStripCard = INDEX_NONE;
	void SetFieldAttack(const struct FFieldAttackEntry &Entry);
	/** Reads the pawn's current stop (on the slow timer: the stops read the loadout and the sources). */
	void RefreshFieldAttack();

	/** B-514 (D-222): the quick strip, built once with the strips (Trial level), shown while it is open. Per tile: its
	 *  outer frame (the hit-test), the inner ring and gap of the picked tile's double frame, the size box (the picked
	 *  tile grows), the fill, the piece's icon and a pair's second icon, the name when there is no icon, the IN HAND
	 *  tag, and the arrows above and below. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> StripTiles;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> StripTileGaps;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> StripTileRings;
	UPROPERTY(Transient)
	TArray<TObjectPtr<USizeBox>> StripTileSizes;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> StripTileFills;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> StripIcons;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> StripOffHandIcons;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> StripTileNames;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> StripInHandTags;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> StripUpArrows;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> StripDownArrows;
	/** The whole strip: the row and the text under it. */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> StripBox;
	/** The world dimmed behind the open strip (under the cards). */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> StripDim;
	/** Under the row: the piece and its source; under it, quieter, what the strip will read. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StripName;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StripReading;
	/** The strip's keys, bottom right, while it is open. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WheelHints;
	void BuildFieldAttackWheel(class UCanvasPanel *Canvas);

	/** B-503g: the party strip's cards, built once (PartyConstants::MAX_PARTY_MEMBERS) at the map's level (B-503j),
	 *  filled on Refresh. */
	UPROPERTY(Transient)
	TArray<FBattleHUDCard> PartyCards;
	/** B-503k: each card's column (its strip over it), shown and hidden whole so a hidden card leaves no gap. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> PartyColumns;
	void BuildPartyStrip(class UCanvasPanel *Canvas);
	void RefreshPartyStrip();

	TWeakObjectPtr<UCurrencyComponent> BoundWallet;
};
