// CharacterSheetWidget.h
// B-442 (D-181, UIScreens #5): the character sheet. A CommonUI screen on the root layout's menu layer, opened from
// the game menu's Character page or wor.UI.Sheet (Back returns to what opened it).
// B-635 (mockup menus/profile/v1, Q-635): restyled as the Profile, in the three columns of the loadout screen (v5), and
// opened from the Party screen (A on a seat). The member tabs on top (LT / RT). Left: who they are (HP, EP, Max
// Actions) and the groups Mind, Body, Spirit, Resistances, Curses. Middle: the lit group's entries, each with a bar.
// Right: Details, what the lit entry does and where its value comes from, with the total.
// Up / Down move in the lit column; A goes from a group to its entries, B back to the groups, then out; Y opens the
// loadout on this member. Read only. What it shows comes from WoRCharacterSheet (CharacterSheetModel.h).

#pragma once

#include "CoreMinimal.h"
#include "UI/Menus/CharacterSheetModel.h"
#include "UI/WoRActivatableWidget.h"
#include "CharacterSheetWidget.generated.h"

class APlayerController;
class UBorder;
class UImage;
class UProgressBar;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UWidget;

UCLASS()
class WORLD_OF_REFRACTION_API UCharacterSheetWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	UCharacterSheetWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout (over what opened it), on the sheet's member InMemberIndex. */
	static UCharacterSheetWidget *Open(APlayerController *PC, int32 InMemberIndex = 0);

	// What the keys and the mouse do (the specs call these too).
	/** The next / previous character; wraps. */
	void MoveMember(int32 Delta);
	/** Up / Down in the lit column (the groups, or the lit group's entries); wraps. */
	void MoveInColumn(int32 Delta);
	void SelectGroup(int32 Group);
	void SelectEntry(int32 Entry);
	/** A: from a group to its entries. */
	void PressSelect();
	/** B: from the entries back to the groups, else out of the screen. */
	void PressBack();
	/** Y: the loadout screen on this member, over this one. */
	void PressOpenLoadout();

	int32 GetMemberIndex() const { return MemberIndex; }
	const FCharacterSheetPage &GetPage() const { return Page; }
	const TArray<FCharacterSheetGroup> &GetGroups() const { return Groups; }
	int32 GetGroupIndex() const { return GroupIndex; }
	int32 GetEntryIndex() const { return EntryIndex; }
	bool IsOnEntries() const { return bOnEntries; }
	/** The entry Details shows; null when the lit group has none. */
	const FCharacterSheetStat *GetLitEntry() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	/** B-553 (D-233): the tab or plate under a moving mouse lights (hover colour); the click still picks it. */
	virtual FReply NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent &InMouseEvent) override;

private:
	/** A group's row on the left. */
	struct FGroupWidgets
	{
		TWeakObjectPtr<UBorder> Frame;
		TWeakObjectPtr<UTextBlock> Name;
		TWeakObjectPtr<UTextBlock> Blurb;
		TWeakObjectPtr<UTextBlock> Count;
	};

	/** An entry's plate in the middle: its name and value, then its bar or (a curse) its text. */
	struct FEntryWidgets
	{
		TWeakObjectPtr<UBorder> Frame;
		TWeakObjectPtr<UTextBlock> Label;
		TWeakObjectPtr<UTextBlock> Value;
		TWeakObjectPtr<UProgressBar> Bar;
		TWeakObjectPtr<UTextBlock> Text;
	};

	/** One "Where it comes from" line. */
	struct FSourceWidgets
	{
		TWeakObjectPtr<UWidget> Row;
		TWeakObjectPtr<UTextBlock> Label;
		TWeakObjectPtr<UTextBlock> Value;
	};

	/** What the mouse is over. */
	enum class EHoverKind : uint8
	{
		None,
		Tab,
		Group,
		Entry
	};

	void BuildTree();
	UWidget *BuildLeft();
	UWidget *BuildMiddle();
	UWidget *BuildRight();
	/** The page and its groups, read fresh from the model. */
	void Refresh();
	/** The lit group, its entries and Details (no model read). */
	void RefreshSelection();
	void RefreshDetails();
	void PaintMemberTabs();
	void SetHover(EHoverKind Kind, int32 Index);
	int32 FindUnder(const FPointerEvent &MouseEvent, EHoverKind Kind) const;

	TArray<FPauseMenuMember> Members;
	FCharacterSheetPage Page;
	TArray<FCharacterSheetGroup> Groups;
	int32 MemberIndex = 0;
	int32 GroupIndex = 0;
	int32 EntryIndex = 0;
	bool bOnEntries = false;
	EHoverKind HoverKind = EHoverKind::None;
	int32 HoverIndex = INDEX_NONE;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> MemberTabs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> MemberTabLabels;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RefusalText;
	UPROPERTY(Transient)
	TObjectPtr<UImage> ElementBadge;
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
	TObjectPtr<UTextBlock> ActionsValue;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GroupTitle;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GroupCount;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GroupSubtitle;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NoEntriesText;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> EntryScroll;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailName;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailDescription;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> SourcesBlock;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TotalValue;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintsText;

	TArray<FGroupWidgets> GroupWidgets;
	TArray<FEntryWidgets> EntryWidgets;
	TArray<FSourceWidgets> SourceWidgets;
};
