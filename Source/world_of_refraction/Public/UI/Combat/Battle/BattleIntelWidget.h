// BattleIntelWidget.h
// PLACEHOLDER UI (B-586, D-261; refine_final/13_intel.png): the fight's Intel panel, plain C++ (no .uasset). One row of
// tabs (party first, then enemies; yours reads "You"), then the selected page: heading, subline, bars, and its sections
// (party: Pillars, Stats, Effects; enemy: Weak to, Resists, Effects). Built once in RebuildWidget with fixed row pools;
// Push only changes texts and visibility (TransBuffer rule). The rules are in BattleIntelView; UBattleHUDWidget owns the
// open / tab state and pushes. CommonUI replaces it later (D-024).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Combat/Battle/BattleIntelView.h"
#include "BattleIntelWidget.generated.h"

class UBorder;
class UHorizontalBox;
class UPanelWidget;
class UProgressBar;
class UTextBlock;
class UVerticalBox;

DECLARE_DELEGATE_OneParam(FOnBattleIntelTabClicked, int32 /*Tab*/);

/** One pooled line: label, the value before the fight's effects, an arrow, the value now, an effect's turns left. */
USTRUCT()
struct FBattleIntelRowWidgets
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UWidget> Root;
	UPROPERTY()
	TObjectPtr<UTextBlock> Label;
	UPROPERTY()
	TObjectPtr<UTextBlock> Before;
	UPROPERTY()
	TObjectPtr<UTextBlock> Arrow;
	UPROPERTY()
	TObjectPtr<UTextBlock> Now;
	UPROPERTY()
	TObjectPtr<UTextBlock> Turns;
};

/** One section's pooled lines. */
USTRUCT()
struct FBattleIntelRowPool
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FBattleIntelRowWidgets> Rows;
};

UCLASS()
class WORLD_OF_REFRACTION_API UBattleIntelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// shortcut: fixed pools sized for today's fights (a party of four, a handful of enemies), upgrade to a scroll box
	// if a fight outgrows them (extra tabs are reachable by Q / E only; extra effects fold into "+N more").
	static constexpr int32 MAX_TABS = 10;
	static constexpr int32 MAX_EFFECT_ROWS = 8;
	static constexpr int32 MAX_ELEMENT_ROWS = 10;
	static constexpr int32 STAT_ROWS_PER_COLUMN = 6;
	static constexpr int32 PILLAR_COUNT = 3;

	/** Show Pages[Selected] and the tab row. Texts only; safe before the widget is built (no-op). */
	void Push(const TArray<FBattleIntelPage> &Pages, int32 Selected);

	/** A click on a tab: its index. The HUD binds it. */
	FOnBattleIntelTabClicked OnTabClicked;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UFUNCTION()
	FEventReply HandleTabMouseDown(FGeometry MyGeometry, const FPointerEvent &MouseEvent);

	UVerticalBox *AddSection(UVerticalBox *Parent, const FText &Title, TObjectPtr<UWidget> &OutSection);
	void AddRows(UPanelWidget *Parent, int32 Count, bool bTurnsColumn, FBattleIntelRowPool &OutPool);
	static void PushRows(const FBattleIntelRowPool &Pool, const TArray<FBattleIntelRow> &Rows);

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> Tabs;
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> TabLabels;

	UPROPERTY()
	TObjectPtr<UTextBlock> Heading;
	UPROPERTY()
	TObjectPtr<UTextBlock> Subline;
	UPROPERTY()
	TObjectPtr<UTextBlock> ActionsLine;
	UPROPERTY()
	TObjectPtr<UProgressBar> HPBar;
	UPROPERTY()
	TObjectPtr<UTextBlock> HPValue;
	UPROPERTY()
	TObjectPtr<UWidget> EPRow;
	UPROPERTY()
	TObjectPtr<UProgressBar> EPBar;
	UPROPERTY()
	TObjectPtr<UTextBlock> EPValue;
	UPROPERTY()
	TObjectPtr<UProgressBar> BuildupBar;
	UPROPERTY()
	TObjectPtr<UTextBlock> BuildupValue;

	UPROPERTY()
	TObjectPtr<UWidget> PillarsSection;
	UPROPERTY()
	TObjectPtr<UWidget> StatsSection;
	UPROPERTY()
	TObjectPtr<UWidget> WeakSection;
	UPROPERTY()
	TObjectPtr<UWidget> ResistSection;
	UPROPERTY()
	TObjectPtr<UWidget> EffectsSection;

	UPROPERTY()
	FBattleIntelRowPool PillarRows;
	UPROPERTY()
	FBattleIntelRowPool StatRowsLeft;
	UPROPERTY()
	FBattleIntelRowPool StatRowsRight;
	UPROPERTY()
	FBattleIntelRowPool WeakRows;
	UPROPERTY()
	FBattleIntelRowPool ResistRows;
	UPROPERTY()
	FBattleIntelRowPool EffectRows;
};
