// EnemyKit.cpp

#include "Trial/EnemyKit.h"

#include "Character/CharacterData.h"
#include "Combat/Encounter/EnemyZone.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/LoadoutRules.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Trial/PrebuiltFindData.h"
#include "Trial/TrialLoot.h"
#include "Trial/TrialPool.h"

namespace
{
	int32 CountKind(const FEnemyKit &Kit, EEnemyKitKind Kind)
	{
		return Kit.Pieces.FilterByPredicate([Kind](const FEnemyKitPiece &Piece)
		{
			EEnemyKitKind PieceKind;
			return EnemyKitRules::GetKind(Piece.Item, PieceKind) && PieceKind == Kind;
		}).Num();
	}

	bool IsOver(int32 Count, int32 Limit) { return Limit != LoadoutClassTable::NO_LIMIT && Count > Limit; }

	FString GetLimitString(int32 Limit)
	{
		return Limit == LoadoutClassTable::NO_LIMIT ? FString(TEXT("any")) : FString::FromInt(Limit);
	}
}

bool EnemyKitRules::GetKind(const UObject *Item, EEnemyKitKind &OutKind)
{
	if (const UPrebuiltFindData *Find = Cast<UPrebuiltFindData>(Item))
	{
		return GetKind(Find->Piece, OutKind);
	}
	if (Cast<UWeaponData>(Item))
	{
		OutKind = EEnemyKitKind::Weapon;
	}
	else if (Cast<URingData>(Item))
	{
		OutKind = EEnemyKitKind::Adapter;
	}
	else if (Cast<UEvolutionItemData>(Item))
	{
		OutKind = EEnemyKitKind::Evolution;
	}
	else if (Cast<UEquipmentDataBase>(Item))
	{
		OutKind = EEnemyKitKind::Apparel;
	}
	else
	{
		return false;
	}
	return true;
}

int32 EnemyKitRules::GetMostGear(const FEnemyKit &Kit, ECharacterClass Class)
{
	const FLoadoutClassLimits Limits = LoadoutClassTable::Get(Class);
	const int32 ClassItems = Limits.MaxItems == LoadoutClassTable::NO_LIMIT ? Limits.MaxWeapons + Limits.MaxAdapters : Limits.MaxItems;
	return Kit.bLimitGear ? FMath::Min(ClassItems, Kit.MostGear) : ClassItems;
}

int32 EnemyKitRules::GetMostOfKind(const FEnemyKit &Kit, ECharacterClass Class, EEnemyKitKind Kind)
{
	if (!AllowsKind(Kit, Kind))
	{
		return 0;
	}
	const FLoadoutClassLimits Limits = LoadoutClassTable::Get(Class);
	switch (Kind)
	{
	case EEnemyKitKind::Weapon:
		return Limits.MaxWeapons;
	case EEnemyKitKind::Adapter:
		return Limits.MaxAdapters;
	default:
		return LoadoutClassTable::NO_LIMIT;
	}
}

bool EnemyKitRules::AllowsKind(const FEnemyKit &Kit, EEnemyKitKind Kind)
{
	return Kit.Kinds.IsEmpty() || Kit.Kinds.Contains(Kind);
}

int32 EnemyKitRules::GetMostSkills(const FEnemyKit &Kit, int32 Slots)
{
	return FMath::Clamp(Kit.bLimitSkills ? Kit.MostSkills : DEFAULT_MOST_SKILLS, 0, FMath::Max(0, Slots));
}

bool EnemyKitRules::AllowsWeaponType(const FEnemyKit &Kit, EWeaponType Type)
{
	return Kit.WeaponTypes.IsEmpty() || Kit.WeaponTypes.Contains(Type);
}

bool EnemyKitRules::AllowsElement(const FEnemyKit &Kit, ESpellElement Element)
{
	return Kit.Elements.IsEmpty() || Kit.Elements.Contains(Element);
}

FTrialTierRange EnemyKitRules::GetTierRange(const FEnemyKit &Kit, const FTrialTierRange &ZoneRange)
{
	if (!Kit.bLimitTiers)
	{
		return ZoneRange;
	}
	return {FMath::Max(ZoneRange.Min, Kit.MinTier), FMath::Min(ZoneRange.Max, Kit.MaxTier)};
}

TArray<FString> EnemyKitRules::CheckAlwaysHas(const FEnemyKit &Kit, ECharacterClass Class)
{
	TArray<FString> Problems;
	const FLoadoutClassLimits Limits = LoadoutClassTable::Get(Class);
	const int32 Weapons = CountKind(Kit, EEnemyKitKind::Weapon);
	const int32 Adapters = CountKind(Kit, EEnemyKitKind::Adapter);
	const int32 Items = Weapons + Adapters + CountKind(Kit, EEnemyKitKind::Evolution);
	if (IsOver(Weapons, Limits.MaxWeapons))
	{
		Problems.Add(FString::Printf(TEXT("%d weapons; its class carries %d"), Weapons, Limits.MaxWeapons));
	}
	if (IsOver(Adapters, Limits.MaxAdapters))
	{
		Problems.Add(FString::Printf(TEXT("%d adapters; its class carries %d"), Adapters, Limits.MaxAdapters));
	}
	if (IsOver(Items, Limits.MaxItems))
	{
		Problems.Add(FString::Printf(TEXT("%d items; its class carries %d"), Items, Limits.MaxItems));
	}
	// The kit's own limits, which the always-has list counts toward (Q-567).
	if (Kit.bLimitGear && Items > Kit.MostGear)
	{
		Problems.Add(FString::Printf(TEXT("%d pieces always; its most gear is %d"), Items, Kit.MostGear));
	}
	const int32 MostSkills = Kit.bLimitSkills ? Kit.MostSkills : DEFAULT_MOST_SKILLS;
	if (Kit.Skills.Num() > MostSkills)
	{
		Problems.Add(FString::Printf(TEXT("%d skills always; most skills is %d"), Kit.Skills.Num(), MostSkills));
	}
	if (Kit.bLimitTiers && Kit.MinTier > Kit.MaxTier)
	{
		Problems.Add(TEXT("its tier range is upside down (min above max)"));
	}
	for (int32 Index = 0; Index < Kit.Pieces.Num(); ++Index)
	{
		const UObject *Item = Kit.Pieces[Index].Item;
		EEnemyKitKind Kind;
		if (!GetKind(Item, Kind))
		{
			Problems.Add(FString::Printf(TEXT("piece %d is empty or not gear"), Index));
		}
		else if (!AllowsKind(Kit, Kind))
		{
			Problems.Add(FString::Printf(TEXT("piece %d is a kind its own limit leaves out"), Index));
		}
		const UPrebuiltFindData *Find = Cast<UPrebuiltFindData>(Item);
		const UWeaponData *Weapon = Cast<UWeaponData>(Find ? Find->Piece.Get() : Item);
		if (Weapon && !AllowsWeaponType(Kit, Weapon->WeaponType))
		{
			Problems.Add(FString::Printf(TEXT("piece %d is a weapon type its own limit leaves out"), Index));
		}
	}
	return Problems;
}

FString EnemyKitRules::DescribeOutsideZoneRange(const FString &Label, const FEncounterZoneInfo &Zone, TConstArrayView<UCharacterData *> Roster)
{
	const FTrialTierRange Range = TrialPool::GetZoneTierRange(Zone.Tier);
	TArray<FString> Outside;
	for (const UCharacterData *Enemy : Roster)
	{
		if (!Enemy)
		{
			continue;
		}
		for (const FEnemyKitPiece &Piece : Enemy->EnemyKit.Pieces)
		{
			if (Piece.Item && !Range.Contains(Piece.Tier))
			{
				Outside.Add(FString::Printf(TEXT("%s's %s (%s)"), *Enemy->GetName(), *TrialLoot::GetItemName(Piece.Item).ToString(),
											*ItemIdentity::GetTierLetter(Piece.Tier)));
			}
		}
	}
	if (Outside.IsEmpty())
	{
		return FString();
	}
	return FString::Printf(TEXT("%s: zone tier %d holds %s to %s; always has %s (the given tier wins)"), *Label, Zone.Tier,
						   *ItemIdentity::GetTierLetter(Range.Min), *ItemIdentity::GetTierLetter(Range.Max), *FString::Join(Outside, TEXT(", ")));
}

FString EnemyKitRules::GetEnemyKitString(const FEnemyKit &Kit, ECharacterClass Class)
{
	TArray<FString> Pieces;
	for (const FEnemyKitPiece &Piece : Kit.Pieces)
	{
		Pieces.Add(FString::Printf(TEXT("%s (%s)"), Piece.Item ? *TrialLoot::GetItemName(Piece.Item).ToString() : TEXT("<empty>"),
								   *ItemIdentity::GetTierLetter(Piece.Tier)));
	}
	TArray<FString> Skills;
	for (const USkillDataBase *Skill : Kit.Skills)
	{
		Skills.Add(Skill ? Skill->Name : FString(TEXT("<empty>")));
	}
	TArray<FString> Kinds;
	for (const EEnemyKitKind Kind : Kit.Kinds)
	{
		Kinds.Add(UEnum::GetDisplayValueAsText(Kind).ToString());
	}
	TArray<FString> Types;
	for (const EWeaponType Type : Kit.WeaponTypes)
	{
		Types.Add(UEnum::GetDisplayValueAsText(Type).ToString());
	}
	TArray<FString> Elements;
	for (const ESpellElement Element : Kit.Elements)
	{
		Elements.Add(UEnum::GetDisplayValueAsText(Element).ToString());
	}
	FString Output = FString::Printf(TEXT("Always has: %s | skills: %s"), Pieces.IsEmpty() ? TEXT("nothing") : *FString::Join(Pieces, TEXT(", ")),
									 Skills.IsEmpty() ? TEXT("none") : *FString::Join(Skills, TEXT(", ")));
	Output += FString::Printf(TEXT(" | most gear %d (weapons %s, adapters %s), skills %d (before slots)"), GetMostGear(Kit, Class),
							  *GetLimitString(GetMostOfKind(Kit, Class, EEnemyKitKind::Weapon)),
							  *GetLimitString(GetMostOfKind(Kit, Class, EEnemyKitKind::Adapter)), Kit.bLimitSkills ? Kit.MostSkills : DEFAULT_MOST_SKILLS);
	Output += FString::Printf(TEXT(" | kinds %s | weapon types %s | elements %s | tiers %s"), Kinds.IsEmpty() ? TEXT("any") : *FString::Join(Kinds, TEXT(", ")),
							  Types.IsEmpty() ? TEXT("any") : *FString::Join(Types, TEXT(", ")),
							  Elements.IsEmpty() ? TEXT("any") : *FString::Join(Elements, TEXT(", ")),
							  Kit.bLimitTiers ? *FString::Printf(TEXT("%s to %s"), *ItemIdentity::GetTierLetter(Kit.MinTier), *ItemIdentity::GetTierLetter(Kit.MaxTier))
											  : TEXT("its zone's"));
	for (const FString &Problem : CheckAlwaysHas(Kit, Class))
	{
		Output += FString::Printf(TEXT(" | PROBLEM: %s"), *Problem);
	}
	return Output;
}
