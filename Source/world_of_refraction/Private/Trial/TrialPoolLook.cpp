// TrialPoolLook.cpp

#include "Trial/TrialPoolLook.h"

#include "Inventory/ItemTier.h"
#include "WoRGameNames.h"

#define LOCTEXT_NAMESPACE "WoRTrialPoolLook"

namespace TrialPoolLookExample
{
	// Stage 1's made-up numbers (TrialPoolDesign §8 example 2). TODO: stage 6 sets the real essence a convert gives.
	constexpr int32 SwordSlots = 3;
	constexpr int32 ConvertEssence = 12;
}

FText WoRTrialPoolLook::GetDroppedLine(const FTrialDropView &Drop)
{
	const FText Tier = WoRGameNames::GetTierLetter(Drop.Tier);
	if (!Drop.bIsPiece)
	{
		return FText::Format(LOCTEXT("DroppedThing", "Dropped: {0} ({1})"), Drop.ItemName, Tier);
	}
	return FText::Format(Drop.bPrebuilt ? LOCTEXT("DroppedBuilt", "Dropped: {0} ({1}), built") : LOCTEXT("DroppedEmpty", "Dropped: {0} ({1}), empty"),
						 Drop.ItemName, Tier);
}

FText WoRTrialPoolLook::GetDuplicateQuestion(const FDuplicateDropView &View)
{
	return FText::Format(LOCTEXT("DuplicateQuestion", "You already have {0} ({1}). This one is {2}."), View.Drop.ItemName,
						 WoRGameNames::GetTierLetter(View.HeldTier), WoRGameNames::GetTierLetter(View.Drop.Tier));
}

FText WoRTrialPoolLook::GetDuplicateChoiceText(EDuplicateChoice Choice, const FDuplicateDropView &View)
{
	switch (Choice)
	{
	case EDuplicateChoice::Replace:
		return LOCTEXT("ChoiceReplace", "Replace yours");
	case EDuplicateChoice::Add:
		return LOCTEXT("ChoiceAdd", "Add it (keep both)");
	case EDuplicateChoice::Convert:
		return FText::Format(LOCTEXT("ChoiceConvert", "Convert to {0} essence"), FText::AsNumber(View.EssenceValue));
	}
	return FText::GetEmpty();
}

FEnemyKitCard WoRTrialPoolLook::MakeKitCardExample()
{
	FEnemyKitCard Card;
	Card.Name = LOCTEXT("ExampleName", "Wandering Duelist");
	Card.ClassLine = LOCTEXT("ExampleClass", "Generic  ·  Mind 1  ·  Body 1  ·  Spirit 1");
	Card.Pieces.Add(FText::Format(LOCTEXT("PieceLine", "{0}  ·  {1}  ·  {2} slots"), LOCTEXT("ArmingSword", "Arming Sword"),
								  WoRGameNames::GetTierLetter(EItemTier::D_Tier), FText::AsNumber(TrialPoolLookExample::SwordSlots)));
	Card.Skills = {LOCTEXT("Lunge", "Lunge"), LOCTEXT("HeavyStrike", "Heavy Strike"), LOCTEXT("Thrust", "Thrust")};
	return Card;
}

FTrialDropView WoRTrialPoolLook::MakeDropExample()
{
	FTrialDropView Drop;
	Drop.ItemName = LOCTEXT("ArmingSword", "Arming Sword");
	Drop.Tier = EItemTier::D_Tier;
	return Drop;
}

FDuplicateDropView WoRTrialPoolLook::MakeDuplicateExample()
{
	FDuplicateDropView View;
	View.Drop = MakeDropExample();
	View.HeldTier = EItemTier::F_Tier;
	View.EssenceValue = TrialPoolLookExample::ConvertEssence;
	return View;
}

FString WoRTrialPoolLook::GetKitCardString(const FEnemyKitCard &Card)
{
	TArray<FString> Pieces;
	for (const FText &Piece : Card.Pieces)
	{
		Pieces.Add(Piece.ToString());
	}
	TArray<FString> Skills;
	for (const FText &Skill : Card.Skills)
	{
		Skills.Add(Skill.ToString());
	}
	return FString::Printf(TEXT("%s (%s) | pieces: %s | skills: %s"), *Card.Name.ToString(), *Card.ClassLine.ToString(),
						   *FString::Join(Pieces, TEXT("; ")), *FString::Join(Skills, TEXT(", ")));
}

#undef LOCTEXT_NAMESPACE
