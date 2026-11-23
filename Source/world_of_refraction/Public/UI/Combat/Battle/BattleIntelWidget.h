// BattleIntelWidget.h
// PLACEHOLDER UI (B-586, D-261; refine_final/13_intel.png): the fight's Intel panel, plain C++ (no .uasset). B-587
// (Crown, D-262): one title line, "◀ Q  name  E ▶" under its side ("You", "Party", "Enemy"), in place of B-586's tab row;
// then the open page: subline, bars, and its sections
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

DECLARE_DELEGATE_OneParam(FOnBattleIntelStepClicked, int32 /*Delta: -1 previous, 1 next*/);

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
	// if a fight outgrows them (extra effects fold into "+N more").
	static constexpr int32 MAX_EFFECT_ROWS = 8;
	static constexpr int32 MAX_ELEMENT_ROWS = 10;
	static constexpr int32 STAT_ROWS_PER_COLUMN = 6;
	static constexpr int32 PILLAR_COUNT = 3;

	/** Show Pages[Selected]; the arrows only when there is another page. bCycleKey: the V / R3 footer for the Cycle
	 *  setting (party, then enemies, then close). Texts only; safe before the widget is built (no-op). */
	void Push(const TArray<FBattleIntelPage> &Pages, int32 Selected, bool bCycleKey);

	/** A click on an arrow: -1 or 1. The HUD binds it. */
	FOnBattleIntelStepClicked OnStepClicked;

	/** B-587: the title line's side label, name and footer as shown (specs, wor.UI.Print). */
	FText GetSideText() const;
	FText GetTitleText() const;
	bool AreArrowsShown() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UFUNCTION()
	FEventReply HandlePrevMouseDown(FGeometry MyGeometry, const FPointerEvent &MouseEvent);
	UFUNCTION()
	FEventReply HandleNextMouseDown(FGeometry MyGeometry, const FPointerEvent &MouseEvent);

	UVerticalBox *AddSection(UVerticalBox *Parent, const FText &Title, TObjectPtr<UWidget> &OutSection);
	void AddRows(UPanelWidget *Parent, int32 Count, bool bTurnsColumn, FBattleIntelRowPool &OutPool);
	static void PushRows(const FBattleIntelRowPool &Pool, const TArray<FBattleIntelRow> &Rows);

	UPROPERTY()
	TObjectPtr<UTextBlock> SideLabel;
	UPROPERTY()
	TObjectPtr<UBorder> PrevArrow;
	UPROPERTY()
	TObjectPtr<UBorder> NextArrow;
	UPROPERTY()
	TObjectPtr<UTextBlock> Footer;

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
