// GiveWidget.h
// OBSOLETE (B-339, D-131): wor.UI.Inventory (UInventoryWidget) does this as one of its verbs; delete after Crown's PIE.
// PLACEHOLDER UI (B-323, D-117): give a weapon, adapter or pouch stack from one of your characters to
// another. Plain buttons built in C++, opened with the console command wor.UI.Give (Backlog rule 4).
// Replaced by the CommonUI inventory screen (D-024). Rules and refusals: WoRGive (Inventory/GearGive.h).
// B-316b: the giver's learned skills are choices too; "Ask for it" has the receiver request the chosen skill.
// B-361b: "Teach it" has the giver's player teach the chosen skill to another player's character for Mentor Tokens.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "GiveWidget.generated.h"

class UButton;
class UTextBlock;
class UPoolSubsystem;

UCLASS()
class WORLD_OF_REFRACTION_API UGiveWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Add a fresh one to PC's viewport (UI-only input, cursor on). Null when PC is null. */
	static UGiveWidget *Open(APlayerController *PC);

	/** Re-read the giver's gear and pouch and rewrite every row. */
	void Refresh();

	/** Debug: one line per row as shown. */
	FString GetGiveString() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	/** A party member, as giver or receiver. */
	struct FMember
	{
		FName Key;
		FText Name;
	};

	/** One thing the giver could give: a piece of gear (GearID), a pouch stack (Item, Count) or a skill (B-316b). */
	struct FGiveChoice
	{
		FGuid GearID;
		FCrystalId Item;
		int32 Count = 0;
		TWeakObjectPtr<const UObject> Skill;
		FText Label;
		bool IsGear() const { return GearID.IsValid(); }
		bool IsSkill() const { return Skill.IsValid(); }
	};

	TArray<FMember> GatherMembers() const;
	TArray<FGiveChoice> GatherChoices() const;
	UPoolSubsystem *GetPool() const;

	FName GiverKey() const;
	FName ReceiverKey() const;

	/** Gives the selected gear, or Count of the selected stack. */
	void GiveSelected(int32 Count);

	UFUNCTION()
	void HandleNextGiverClicked();

	UFUNCTION()
	void HandleNextItemClicked();

	UFUNCTION()
	void HandleNextReceiverClicked();

	UFUNCTION()
	void HandleGiveOneClicked();

	UFUNCTION()
	void HandleGiveStackClicked();

	/** B-316b: the receiver asks the giver for the chosen skill (WoRGive::TryRequestSkill). */
	UFUNCTION()
	void HandleRequestClicked();

	/** B-361b (D-154): the giver's player teaches the chosen skill to the receiver, another player's character, for
	 *  Mentor Tokens (WoRGive::TryTeachSkill). */
	UFUNCTION()
	void HandleTeachClicked();

	UFUNCTION()
	void HandleCloseClicked();

	int32 GiverIndex = 0;
	int32 ReceiverIndex = 1;
	int32 SelectedIndex = 0;

	FText StatusText;

	UPROPERTY()
	TObjectPtr<UTextBlock> GiverText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ItemText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ReceiverText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusLine;

	UPROPERTY()
	TObjectPtr<UButton> GiveOneButton;

	UPROPERTY()
	TObjectPtr<UButton> GiveStackButton;

	UPROPERTY()
	TObjectPtr<UButton> RequestButton;

	UPROPERTY()
	TObjectPtr<UButton> TeachButton;
};
