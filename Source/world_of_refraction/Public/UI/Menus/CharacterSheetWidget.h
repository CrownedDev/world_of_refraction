// CharacterSheetWidget.h
// B-442 (D-181, UIScreens #5, mockup screen_mockups/menus/character/): the character sheet. A CommonUI screen
// on the root layout's menu layer, opened from the game menu's Character page (Back returns there) or
// wor.UI.Sheet.
//
// The character tabs on top (LT / RT), then three panels: who they are (HP and EP in their bars, Max Actions,
// curses), the pillars with their sub-stats, and resistance by damage type as a bar from the middle (right and
// green: resists; left and rose: weak). Read only. What it shows comes from WoRCharacterSheet
// (CharacterSheetModel.h), which the specs read.
// Not built yet: "where it comes from" on R3 (the stat's layers) and Y to the loadout.

#pragma once

#include "CoreMinimal.h"
#include "UI/Menus/CharacterSheetModel.h"
#include "UI/WoRActivatableWidget.h"
#include "CharacterSheetWidget.generated.h"

class APlayerController;
class UBorder;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class UWidget;

UCLASS()
class WORLD_OF_REFRACTION_API UCharacterSheetWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	UCharacterSheetWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout (over the game menu, when that is what opened it). */
	static UCharacterSheetWidget *Open(APlayerController *PC);

	/** The next / previous character; wraps. */
	void MoveMember(int32 Delta);
	void PressBack() { HandleBackAction(); }

	int32 GetMemberIndex() const { return MemberIndex; }
	const FCharacterSheetPage &GetPage() const { return Page; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;

private:
	/** A pillar's widgets: its level pips and its sub-stat lines, built once and refilled. */
	struct FPillarWidgets
	{
		TWeakObjectPtr<UTextBlock> Name;
		TArray<TWeakObjectPtr<UBorder>> Pips;
		TWeakObjectPtr<UTextBlock> Level;
		TArray<TWeakObjectPtr<UWidget>> StatLines;
		TArray<TWeakObjectPtr<UTextBlock>> StatLabels;
		TArray<TWeakObjectPtr<UTextBlock>> StatValues;
	};

	/** One resistance line: a bar that fills left from the middle (weak) and one that fills right (resists). */
	struct FResistanceWidgets
	{
		TWeakObjectPtr<UWidget> Root;
		TWeakObjectPtr<UTextBlock> Label;
		TWeakObjectPtr<UProgressBar> WeakBar;
		TWeakObjectPtr<UProgressBar> ResistBar;
		TWeakObjectPtr<UTextBlock> Value;
	};

	void BuildTree();
	UWidget *BuildIdentityPanel();
	UWidget *BuildPillarsPanel();
	UWidget *BuildResistancePanel();
	void Refresh();
	int32 FindMemberTabUnder(const FPointerEvent &MouseEvent) const;

	TArray<FPauseMenuMember> Members;
	FCharacterSheetPage Page;
	int32 MemberIndex = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> MemberTabs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> MemberTabLabels;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RefusalText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ClassLineText;
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HPBar;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HPValue;
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> EPBar;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EPValue;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> ActionPips;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MaxActionsValue;
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ActionPointBar;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ActionPointValue;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ActionRegainValue;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> CurseTitles;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> CurseDescriptions;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CurseNote;

	TArray<FPillarWidgets> PillarWidgets;
	TArray<FResistanceWidgets> ResistanceWidgets;
};
