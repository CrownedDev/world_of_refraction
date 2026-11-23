// BattleHUDCard.h
// B-503j (D-216): the party card, one class for the battle HUD and the hub / trial HUD. What it shows depends on
// where it is (EBattleCardLevel): the hub shows who is in the party (the name); a trial adds HP, EP and the status
// build-up bar; a battle shows everything (as the B-391 card). GetParts says which parts a level builds; the specs
// read it (WoR.UI.BattleHUD). The data is plain (it runs unbuilt); Build makes the widgets once into the owner's
// widget tree (TransBuffer rule), Push writes the data into them.

#pragma once

#include "CoreMinimal.h"
#include "BattleHUDCard.generated.h"

class UBorder;
class UCharacterDataComponent;
class ULoadoutComponent;
class UProgressBar;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UWidget;
class UWidgetTree;

/** D-216: where a party card is shown, which sets what it shows. */
enum class EBattleCardLevel : uint8
{
	/** Who is in the party: the name only. */
	Hub,
	/** In a run, between fights: HP, EP and the status build-up bar. */
	Trial,
	/** In a fight: everything. */
	Battle,
};

/** Which parts a card builds; the name always shows. */
struct FBattleCardParts
{
	bool bHPBar = false;
	/** B-503m: the HP number, drawn on the HP bar. */
	bool bHPLine = false;
	/** B-503m: the EP bar with its number on it. */
	bool bEPLine = false;
	bool bBuildupBar = false;
	bool bBuildupLine = false;
	/** The gear tooltip on hover; B-503k: no gear line on the card (GearLine stays as data). */
	bool bGear = false;
	bool bActions = false;
	bool bStatus = false;
	/** B-436c: "Tier C" centred at the bottom of a fight's enemy card (the target marker), no rank. */
	bool bTier = false;
	/** B-503k: the card's fixed width (CardWidth); without it the card fits its name. */
	bool bFixedWidth = false;
};

/** The card's look, shared by every HUD that shows it. */
namespace BattleCardStyle
{
	// TODO: style pass (placeholder until the CommonUI screens, D-024). Slate units, DPI-scaled.
	inline constexpr float CardWidth = 260.0f;
	inline constexpr float CompactCardWidth = 190.0f; // B-436a: a fight's party cards other than the featured one
	inline constexpr float BarHeight = 10.0f;
	inline constexpr float CardPadding = 6.0f;
	inline constexpr float RowPadding = 2.0f;
	inline constexpr int32 NameFontSize = 16;
	inline constexpr int32 LineFontSize = 13;
	inline constexpr float DownedCardOpacity = 0.4f;

	inline const FLinearColor CardColor(0.0f, 0.0f, 0.0f, 0.55f);
	inline const FLinearColor TextColor = FLinearColor::White;
	inline const FLinearColor ActingColor(1.0f, 0.78f, 0.25f); // warm gold: whose turn it is
	inline const FLinearColor PartyHPColor(0.75f, 0.1f, 0.1f);
	inline const FLinearColor EnemyHPColor(0.55f, 0.1f, 0.1f);

	// B-391c: the status buildup bar under the HP bar, and its cracks (Q-111 A).
	inline constexpr float BuildupBarHeight = 6.0f; // OBSOLETE (B-585): as tall as the HP bar now; delete after checkpoint 6
	inline constexpr float CrackWidth = 2.0f;
	inline const FLinearColor CrackColor(0.0f, 0.0f, 0.0f, 0.85f);

	// B-503m: the HP and EP numbers on their bars, the bars' quiet empty part, the build-up bar's label.
	inline constexpr float NumberBarHeight = 18.0f;
	inline constexpr int32 BarFontSize = 11;
	inline const FLinearColor BarTextShadowColor(0.0f, 0.0f, 0.0f, 0.9f);
	inline const FLinearColor EmptyBarColor(1.0f, 1.0f, 1.0f, 0.15f);
	inline const FLinearColor EPBarColor(0.15f, 0.4f, 0.85f);
	// OBSOLETE (B-585, D-257): the build-up bar's label; delete the three after checkpoint 6.
	inline constexpr int32 BuildupLabelFontSize = 10;
	inline constexpr float BuildupLabelGap = 4.0f;
	inline const FLinearColor BuildupLabelColor(1.0f, 1.0f, 1.0f, 0.55f);
}

/** One combatant's card: what it shows (plain data) and, once built, its text blocks. */
USTRUCT()
struct FBattleHUDCard
{
	GENERATED_BODY()

	TWeakObjectPtr<AActor> Actor;
	TWeakObjectPtr<UCharacterDataComponent> CharData;
	bool bParty = false;
	bool bActing = false;
	bool bAlive = true;
	bool bShowEP = true;
	/** B-391b: the EP line is the absorption pool. */
	bool bBrokenDarkness = false;
	FLinearColor EPColour = FLinearColor::White;
	float HPFraction = 0.0f;
	/** B-503m: the EP bar (a Broken Darkness pool past max reads full; its line says the overload). */
	float EPFraction = 0.0f;
	FText Name;
	/** OBSOLETE (B-581, D-254): the line under the name (D-229, D-241). No HUD fills it any more, so it stays empty
	 *  and DetailText collapsed; delete both in the sweep (PIE/Sweep.md 53). */
	FText DetailLine;
	FText HPLine;
	FText EPLine;
	/** B-170e: "Actions ●●○ 2 · Max 3 (+75/100)"; party cards in an action-pips fight only. */
	FText ActionsLine;
	FText StatusLine;
	/** B-436c: "Tier C" (WoRCombatTier::GetCombatTierText); enemy cards only. */
	FText TierLine;

	/** B-391c: the status buildup bar (every card; the line on party cards only, Q-391). */
	float BuildupFraction = 0.0f;
	int32 CracksShown = 0;
	FLinearColor BuildupColour = FLinearColor::White;
	FText BuildupLine;

	/** B-391c: party cards: "weapon · ring" and the hover tooltip with the loadout. */
	TWeakObjectPtr<ULoadoutComponent> Loadout;
	FText GearLine;
	FText GearTooltip;

	UPROPERTY()
	TObjectPtr<UBorder> Frame;

	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> DetailText;

	UPROPERTY()
	TObjectPtr<UProgressBar> HPBar;

	/** B-503m: drawn on HPBar. */
	UPROPERTY()
	TObjectPtr<UTextBlock> HPText;

	UPROPERTY()
	TObjectPtr<UProgressBar> EPBar;

	/** B-503m: drawn on EPBar. */
	UPROPERTY()
	TObjectPtr<UTextBlock> EPText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ActionsText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<UTextBlock> TierText;

	UPROPERTY()
	TObjectPtr<UProgressBar> BuildupBar;

	/** One mark per BattleHUDView::GetCrackPositions, built once; shown as the bar passes each. */
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> CrackMarks;

	UPROPERTY()
	TObjectPtr<UTextBlock> BuildupText;

	/** B-436a: holds the card's width (full or compact). */
	UPROPERTY()
	TObjectPtr<USizeBox> WidthBox;

	/** B-436a: the EP bar's row, collapsed on a compact card. */
	UPROPERTY()
	TObjectPtr<UWidget> EPRow;
};

namespace BattleHUDCard
{
	/** D-216: what a card shows at Level. Enemies show bars, no numbers (Q-391); gear, actions and the build-up line
	 *  are a party card's, in a fight. */
	WORLD_OF_REFRACTION_API FBattleCardParts GetParts(EBattleCardLevel Level, bool bParty);

	/** Makes Card's widgets in Tree for Level, once, and returns its frame for the caller to place. */
	WORLD_OF_REFRACTION_API UBorder *Build(UWidgetTree *Tree, FBattleHUDCard &Card, EBattleCardLevel Level);

	/** Writes Card's data into its widgets; the parts its level didn't build are skipped. */
	WORLD_OF_REFRACTION_API void Push(const FBattleHUDCard &Card);

	/** B-436a: a fight's party row. Compact: the name, the HP and build-up bars with their numbers at CompactCardWidth; full: every
	 *  part the card built, at CardWidth. Shows and hides only; call it after Push. The hub and trial HUDs never call it. */
	WORLD_OF_REFRACTION_API void SetCompact(const FBattleHUDCard &Card, bool bCompact);

	/** A line of text in the card's font and colour, added to Box. */
	WORLD_OF_REFRACTION_API UTextBlock *AddText(UWidgetTree *Tree, UVerticalBox *Box, int32 FontSize);
}
