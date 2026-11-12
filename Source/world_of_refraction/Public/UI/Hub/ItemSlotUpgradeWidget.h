// ItemSlotUpgradeWidget.h
// PLACEHOLDER UI (B-239, D-097, Q-239): the hub vendor's item slot upgrades. Pick a party member, see its pouch's
// slots and the next price, buy one slot (ItemSlotUpgrade::TryBuy). Plain buttons built in C++, opened with
// wor.UI.ItemSlots; the real CommonUI vendor screen (the Spiritualist's V service, Q-239) replaces this (D-024).
//
// Built once in RebuildWidget; Refresh only rewrites the texts, so nothing is recreated inside the widget.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemSlotUpgradeWidget.generated.h"

class APlayerController;
class UButton;
class UCurrencyComponent;
class UPoolSubsystem;
class UTextBlock;

UCLASS()
class WORLD_OF_REFRACTION_API UItemSlotUpgradeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Add a fresh one to PC's viewport (UI-only input, cursor on). Null when PC is null. */
	static UItemSlotUpgradeWidget *Open(APlayerController *PC);

	/** Re-read the party, the pouch and the wallet and rewrite the texts. */
	void Refresh();

	/** Debug: the member line and the status line as shown. */
	FString GetItemSlotUpgradeString() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	struct FMember
	{
		FName Key;
		FText Name;
	};

	/** The local party's members, in party order. */
	TArray<FMember> GatherMembers() const;
	UPoolSubsystem *GetPool() const;

	/** The owning player's wallet: through the pawn (CurrencyWallet), else the player state's own (no pawn yet). */
	UCurrencyComponent *GetWallet() const;

	UFUNCTION()
	void HandleNextClicked();

	UFUNCTION()
	void HandleBuyClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY()
	TObjectPtr<UTextBlock> MemberText;

	UPROPERTY()
	TObjectPtr<UButton> BuyButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusLine;

	int32 MemberIndex = 0;
	FText Status;
};
