// TrialPoolLook.h
// B-565 (D-225, TrialPoolDesign §9 stage 1, the look): what the three new trial-pool screens say, with no widget in
// them. Stage 1 shows them with made-up contents (the Make*Example functions, from the design's §8 examples); later
// stages fill them from the real roll (stage 4), the drop (stage 5) and the duplicate (stage 6).
//   - The kit card (UEnemyKitCardWidget): an enemy's name, class, the pieces it carries and its skills. No rank word:
//     "Elite", "Mini-boss" and "Boss" are never shown (D-187).
//   - The dropped line on the fight's results (UCombatResultsWidget): "Dropped: Arming Sword (D), empty".
//   - The duplicate prompt (UDuplicateDropWidget): Replace, Add, or Convert to essence.
// Debug: wor.UI.PreviewKitCard, wor.UI.PreviewFightReward, wor.UI.PreviewDuplicate.

#pragma once

#include "CoreMinimal.h"

enum class EItemTier : uint8;

/** What the kit card shows. */
struct FEnemyKitCard
{
	FText Name;
	/** "Generic  ·  Mind 1  ·  Body 1  ·  Spirit 1". */
	FText ClassLine;
	/** One line per piece: "Arming Sword  ·  D  ·  3 slots". */
	TArray<FText> Pieces;
	TArray<FText> Skills;
};

/** One thing dropped at a fight's end. */
struct FTrialDropView
{
	FText ItemName;
	EItemTier Tier;
	/** D-231: a piece set up prebuilt arrives built (its crystal in slot 1); everything else arrives empty. */
	bool bPrebuilt = false;
	/** A skill or a crystal has no "empty". */
	bool bIsPiece = true;
};

/** TrialPoolDesign §6: a drop the player already has: one of three. */
enum class EDuplicateChoice : uint8
{
	Replace,
	Add,
	Convert
};

/** What the duplicate prompt shows. */
struct FDuplicateDropView
{
	FTrialDropView Drop;
	/** The tier of the one the player has. */
	EItemTier HeldTier;
	/** Essence the convert gives. */
	int32 EssenceValue = 0;
};

namespace WoRTrialPoolLook
{
	/** The three choices, in order. */
	inline constexpr EDuplicateChoice DUPLICATE_CHOICES[] = {EDuplicateChoice::Replace, EDuplicateChoice::Add, EDuplicateChoice::Convert};
	/** Q-565: Back on the duplicate prompt keeps both (Add): nothing is lost by closing it. */
	inline constexpr EDuplicateChoice DUPLICATE_BACK_CHOICE = EDuplicateChoice::Add;

	/** "Dropped: Arming Sword (D), empty" / ", built" / no word for a skill or a crystal. */
	WORLD_OF_REFRACTION_API FText GetDroppedLine(const FTrialDropView &Drop);
	/** "You already have Arming Sword (F). This one is D." */
	WORLD_OF_REFRACTION_API FText GetDuplicateQuestion(const FDuplicateDropView &View);
	/** "Replace yours", "Add it (keep both)", "Convert to 12 essence". */
	WORLD_OF_REFRACTION_API FText GetDuplicateChoiceText(EDuplicateChoice Choice, const FDuplicateDropView &View);

	/** Stage 1's made-up contents: the design's §8 example 2 (a low enemy with a high sword) and its drop. */
	WORLD_OF_REFRACTION_API FEnemyKitCard MakeKitCardExample();
	WORLD_OF_REFRACTION_API FTrialDropView MakeDropExample();
	WORLD_OF_REFRACTION_API FDuplicateDropView MakeDuplicateExample();

	WORLD_OF_REFRACTION_API FString GetKitCardString(const FEnemyKitCard &Card);
}
