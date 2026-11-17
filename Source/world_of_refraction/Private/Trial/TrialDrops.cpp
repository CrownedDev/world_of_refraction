// TrialDrops.cpp

#include "Trial/TrialDrops.h"

#include "Algo/Find.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Actor.h"
#include "Inventory/InventoryComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/SpellData.h"
#include "Trial/EnemyKitRoll.h"
#include "Trial/PrebuiltFindData.h"
#include "Trial/TrialLoot.h"
#include "Trial/TrialPoolLook.h"
#include "WoRGameNames.h"

namespace
{
	const FTrialDropUse *FindDropUse(const UObject *Item, TConstArrayView<FTrialDropUse> Used)
	{
		return Algo::FindByPredicate(Used, [Item](const FTrialDropUse &Use) { return Use.Item == Item; });
	}

	EItemTier PickDropTier(const FTrialTierRange &Range, FRandomStream &Roll)
	{
		return static_cast<EItemTier>(Roll.RandRange(static_cast<int32>(Range.Min), static_cast<int32>(Range.Max)));
	}

	bool IsDropGearPiece(const UObject *Item) { return Cast<UWeaponData>(Item) || Cast<URingData>(Item); }
}

TArray<FTrialDropUse> TrialDrops::GatherUsed(TConstArrayView<FRolledEnemyKit> Kits)
{
	TArray<FTrialDropUse> Used;
	for (const FRolledEnemyKit &Kit : Kits)
	{
		for (const FRolledKitPiece &Piece : Kit.Pieces)
		{
			Used.Add({Piece.Piece, Piece.Tier});
			if (Piece.Crystal.Type != ECrystalType::None)
			{
				Used.Add({nullptr, Piece.Crystal.Tier});
			}
		}
		for (const UAbilityData *Ability : Kit.Abilities)
		{
			Used.Add({Ability, Ability ? Ability->Tier : EItemTier::F_Tier});
		}
		for (const USpellData *Spell : Kit.Spells)
		{
			Used.Add({Spell, Spell ? Spell->Tier : EItemTier::F_Tier});
		}
	}
	return Used;
}

float TrialDrops::GetWeight(const UObject *Item, TConstArrayView<FTrialDropUse> Used)
{
	return FindDropUse(Item, Used) ? USED_WEIGHT : UNUSED_WEIGHT;
}

FTrialDrop TrialDrops::Pick(TConstArrayView<UObject *> Pool, TConstArrayView<FTrialDropUse> Used, ESpellElement TrialElement,
							const FTrialTierRange &Range, FRandomStream &Roll)
{
	const bool bHasTiers = Range.Min <= Range.Max;
	TArray<UObject *> Candidates;
	for (UObject *Item : Pool)
	{
		if (TrialLoot::CanGrant(Item))
		{
			Candidates.Add(Item);
		}
	}
	if (bHasTiers)
	{
		Candidates.Add(nullptr); // the crystal entry
	}
	float Total = 0.0f;
	for (const UObject *Item : Candidates)
	{
		Total += GetWeight(Item, Used);
	}
	FTrialDrop Drop;
	if (Total <= 0.0f)
	{
		return Drop;
	}
	float Draw = Roll.FRandRange(0.0f, Total);
	UObject *Chosen = Candidates.Last();
	for (UObject *Item : Candidates)
	{
		Draw -= GetWeight(Item, Used);
		if (Draw < 0.0f)
		{
			Chosen = Item;
			break;
		}
	}
	const FTrialDropUse *Use = FindDropUse(Chosen, Used);
	Drop.Item = Chosen;
	Drop.bUsed = Use != nullptr;
	if (!Chosen)
	{
		Drop.Crystal = TrialPool::PickCrystal(TrialElement, Range, Roll);
		Drop.Tier = Drop.Crystal.Tier;
	}
	else if (const USkillDataBase *Skill = Cast<USkillDataBase>(Chosen))
	{
		Drop.Tier = Skill->Tier;
	}
	else
	{
		// The tier an enemy wore it at; anything else from the zone (TrialPoolDesign §6).
		Drop.Tier = Use && IsDropGearPiece(Chosen) ? Use->Tier : bHasTiers ? PickDropTier(Range, Roll) : EItemTier::F_Tier;
	}
	return Drop;
}

bool TrialDrops::Grant(AActor *Owner, const FTrialDrop &Drop)
{
	UInventoryComponent *Inventory = Owner ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Inventory || !Drop.IsSet())
	{
		return false;
	}
	if (!Drop.Item)
	{
		return Inventory->AddCrystal(Drop.Crystal);
	}
	if (IsDropGearPiece(Drop.Item))
	{
		return Inventory->AddDroppedGear(Cast<UEquipmentDataBase>(Drop.Item), Drop.Tier);
	}
	return TrialLoot::GrantTo(Owner, Drop.Item); // a skill, an evolution, a prebuilt find (D-231: as set up)
}

FTrialDropView TrialDrops::MakeView(const FTrialDrop &Drop)
{
	FTrialDropView View;
	View.Tier = Drop.Tier;
	View.ItemName = Drop.Item ? TrialLoot::GetItemName(Drop.Item) : WoRGameNames::GetCrystalTypeName(Drop.Crystal.Type);
	View.bPrebuilt = Cast<UPrebuiltFindData>(Drop.Item) != nullptr;
	View.bIsPiece = IsDropGearPiece(Drop.Item) || View.bPrebuilt;
	return View;
}

FString TrialDrops::GetDropString(const FTrialDrop &Drop)
{
	if (!Drop.IsSet())
	{
		return TEXT("nothing");
	}
	const FString Name = Drop.Item ? Drop.Item->GetName() : WoRGameNames::GetCrystalTypeName(Drop.Crystal.Type).ToString();
	return FString::Printf(TEXT("%s %s%s"), *Name, *WoRGameNames::GetTierLetter(Drop.Tier).ToString(), Drop.bUsed ? TEXT(" (used)") : TEXT(""));
}
