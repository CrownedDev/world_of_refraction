// CharacterSheetModel.h
// B-442 (D-181, UIScreens #5, mockup screen_mockups/menus/character/): what the character sheet shows, with no
// widget in it, so the specs read it headless. UCharacterSheetWidget draws it.
//
// Three columns: who they are (HP, EP, Max Actions, curses), the pillars with their sub-stats grouped under them
// (Rules CL-39), and resistance by damage type (D-168: All, Physical, Spell, then each element).
// Every number is the one the fight uses, read from the spawned member (Q-442).
// Debug: wor.UI.PrintSheet logs GetCharacterSheetString (every party member's sheet).

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/ESpellElement.h"
#include "UI/Menus/PauseMenuModel.h"

class AActor;
class UCharacterDataComponent;
class UWorld;

/** One sub-stat line: "Spell Damage" / "x1.48". */
struct FCharacterSheetStat
{
	FText Label;
	FText Value;
};

struct FCharacterSheetPillar
{
	FText Name;
	int32 Level = 0;
	int32 MaxLevel = 0;
	TArray<FCharacterSheetStat> Stats;
};

/** One resistance line: a damage type and how much less (or, below zero, more) of it the character takes. */
struct FCharacterSheetResistance
{
	FText Label;
	/** +0.40 = takes 40% less; -0.25 = takes 25% more. */
	float Fraction = 0.0f;
	/** The element's line; None for All, Physical and Spell. */
	ESpellElement Element = ESpellElement::None;
	/** The share at which this line can't go further (its bar is full there). */
	float Cap = 1.0f;
};

struct FCharacterSheetCurse
{
	FText Title;
	FText Description;
};

struct FCharacterSheetPage
{
	/** Name, class line, tier, HP and EP: the same read as the game menu's party card. */
	FPauseMenuMember Who;
	int32 MaxActions = 0;
	/** Action points toward the next Max Action, of PointsPerAction. */
	int32 ActionPointProgress = 0;
	int32 PointsPerAction = 0;
	int32 ActionRegain = 0;
	TArray<FCharacterSheetPillar> Pillars;
	TArray<FCharacterSheetResistance> Resistances;
	/** How many of Resistances are the scope lines (All, Physical, Spell) before the elements. */
	int32 ScopeLineCount = 0;
	TArray<FCharacterSheetCurse> Curses;
	/** Why the stats are missing (the member isn't in this level); empty otherwise. */
	FText Refusal;
};

namespace WoRCharacterSheet
{
	/** The resistance lines: All, Physical, Spell, then one per element a hit can carry. */
	inline constexpr int32 SCOPE_LINE_COUNT = 3;
	inline constexpr int32 ELEMENT_LINE_COUNT = 9;
	inline constexpr int32 RESISTANCE_LINE_COUNT = SCOPE_LINE_COUNT + ELEMENT_LINE_COUNT;

	/** A multiplier the fight uses: "x1.48". */
	WORLD_OF_REFRACTION_API FText FormatMultiplier(float Multiplier);

	/** A fraction as a percent: "9%"; signed: "+40%", "-25%", "0%". */
	WORLD_OF_REFRACTION_API FText FormatPercent(float Fraction, bool bSigned);

	/** Mind, Body, Spirit with their sub-stats under them (CL-39), each at its composed value. */
	WORLD_OF_REFRACTION_API TArray<FCharacterSheetPillar> BuildPillars(const UCharacterDataComponent &Character);

	/** All (the Defence line, D-168), Physical, Spell, then each element in the game's order. */
	WORLD_OF_REFRACTION_API TArray<FCharacterSheetResistance> BuildResistances(AActor *Character, int32 &OutScopeLineCount);

	/** The sheet of the local party's member at MemberIndex. */
	WORLD_OF_REFRACTION_API FCharacterSheetPage BuildPage(UWorld *World, int32 MemberIndex);

	WORLD_OF_REFRACTION_API FString GetCharacterSheetString(UWorld *World);
}
