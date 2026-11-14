// HubHUDWidget.h
// B-443 (D-181, UIScreens #14, mockup screen_mockups/hub/hud/1_hub_hud_prompt.png): the hub and trial-map HUD's
// two new pieces. The party strip stays the existing UPartyDisplayWidget.
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

UCLASS()
class WORLD_OF_REFRACTION_API UHubHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Reads what is in reach and the money, and updates both pieces. The timer calls it; so can a spec. */
	void Refresh();

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

	TWeakObjectPtr<UCurrencyComponent> BoundWallet;
};
