// VendorServiceWidget.h
// PLACEHOLDER UI (B-445, D-177, D-193): a vendor's Sell and Dismantle, until the vendor screen (B-456) has them as
// tabs. Plain buttons built in C++, opened with wor.UI.Vendor [Blacksmith|Jeweler|Spiritualist]. It walks the
// vendor's services, the party's members and the member's things one at a time; the action asks first (what it
// gives, and D-016's warning for a worn piece), then a second press does it. All rules are WoRVendorServices'.
//
// Built once in RebuildWidget; Refresh only rewrites the texts, so nothing is recreated inside the widget.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Shop/VendorServices.h"
#include "VendorServiceWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS()
class WORLD_OF_REFRACTION_API UVendorServiceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Add a fresh one to PC's viewport for Vendor (UI-only input, cursor on). Null when PC is null. */
	static UVendorServiceWidget *Open(APlayerController *PC, EMerchantType Vendor);

	/** Re-read the rows and rewrite the texts. */
	void Refresh();

	/** Debug: the lines as shown. */
	FString GetVendorServiceString() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	TArray<EVendorService> GatherServices() const;
	TArray<FName> GatherMembers(TArray<FText> &OutNames) const;

	UFUNCTION()
	void HandleNextServiceClicked();
	UFUNCTION()
	void HandleNextMemberClicked();
	UFUNCTION()
	void HandleNextItemClicked();
	UFUNCTION()
	void HandleActClicked();
	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY()
	TObjectPtr<UTextBlock> MemberText;
	UPROPERTY()
	TObjectPtr<UTextBlock> ItemText;
	UPROPERTY()
	TObjectPtr<UTextBlock> ActLabel;
	UPROPERTY()
	TObjectPtr<UButton> ActButton;
	UPROPERTY()
	TObjectPtr<UTextBlock> StatusLine;

	EMerchantType Vendor = EMerchantType::Blacksmith;
	int32 ServiceIndex = 0;
	int32 MemberIndex = 0;
	int32 ItemIndex = 0;
	/** The action was pressed once and its confirm text is up; the next press runs it. */
	bool bConfirming = false;
	FText Status;
};
