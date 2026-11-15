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

UCLASS()
class WORLD_OF_REFRACTION_API UHubHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Reads what is in reach and the money, and updates both pieces. The timer calls it; so can a spec. */
	void Refresh();

	/** B-503k (D-217): the infusion R / LB just stepped to: shows it on the strip and flashes it. The strip is up in a
	 *  run while the pawn has a field attack with a name. */
	void ShowFieldAttack(const struct FFieldAttackEntry &Entry);

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
	FTimerHandle FieldAttackTimer;
	void SetFieldAttack(const struct FFieldAttackEntry &Entry);
	/** Reads the pawn's current stop (on the slow timer: the stops read the loadout and the sources). */
	void RefreshFieldAttack();
	void EndFieldAttackFlash();

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
