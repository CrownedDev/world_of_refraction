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
	enum class EKitPieceKind : uint8
	{
		Weapon,
		Adapter,
		Evolution,
		Other, // apparel: its own place, outside the item limits
		Unknown
	};

	EKitPieceKind GetKind(const UObject *Item)
	{
		if (const UPrebuiltFindData *Find = Cast<UPrebuiltFindData>(Item))
		{
			return GetKind(Find->Piece);
		}
		if (Cast<UWeaponData>(Item))
		{
			return EKitPieceKind::Weapon;
		}
		if (Cast<URingData>(Item))
		{
			return EKitPieceKind::Adapter;
		}
		if (Cast<UEvolutionItemData>(Item))
		{
			return EKitPieceKind::Evolution;
		}
		return Cast<UEquipmentDataBase>(Item) ? EKitPieceKind::Other : EKitPieceKind::Unknown;
	}

	int32 CountKind(const FEnemyKit &Kit, EKitPieceKind Kind)
	{
		return Kit.Pieces.FilterByPredicate([Kind](const FEnemyKitPiece &Piece) { return GetKind(Piece.Item) == Kind; }).Num();
	}

	/** A class limit (NO_LIMIT: none) lowered by the kit's own when set. */
	int32 Lower(int32 ClassLimit, bool bKitLimit, int32 KitLimit)
	{
		if (!bKitLimit)
		{
			return ClassLimit;
		}
		return ClassLimit == LoadoutClassTable::NO_LIMIT ? KitLimit : FMath::Min(ClassLimit, KitLimit);
	}

	FString GetLimitString(int32 Limit)
	{
		return Limit == LoadoutClassTable::NO_LIMIT ? FString(TEXT("any")) : FString::FromInt(Limit);
	}
}

int32 EnemyKitRules::GetMostWeapons(const FEnemyKit &Kit, ECharacterClass Class)
{
	return Lower(LoadoutClassTable::Get(Class).MaxWeapons, Kit.bLimitWeapons, Kit.MostWeapons);
}

int32 EnemyKitRules::GetMostAdapters(const FEnemyKit &Kit, ECharacterClass Class)
{
	return Lower(LoadoutClassTable::Get(Class).MaxAdapters, Kit.bLimitAdapters, Kit.MostAdapters);
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
	const int32 Weapons = CountKind(Kit, EKitPieceKind::Weapon);
	const int32 Adapters = CountKind(Kit, EKitPieceKind::Adapter);
	const int32 Items = Weapons + Adapters + CountKind(Kit, EKitPieceKind::Evolution);
	if (Weapons > Limits.MaxWeapons)
	{
		Problems.Add(FString::Printf(TEXT("%d weapons; its class carries %d"), Weapons, Limits.MaxWeapons));
	}
	if (Limits.MaxAdapters != LoadoutClassTable::NO_LIMIT && Adapters > Limits.MaxAdapters)
	{
		Problems.Add(FString::Printf(TEXT("%d adapters; its class carries %d"), Adapters, Limits.MaxAdapters));
	}
	if (Limits.MaxItems != LoadoutClassTable::NO_LIMIT && Items > Limits.MaxItems)
	{
		Problems.Add(FString::Printf(TEXT("%d items; its class carries %d"), Items, Limits.MaxItems));
	}
	for (int32 Index = 0; Index < Kit.Pieces.Num(); ++Index)
	{
		if (GetKind(Kit.Pieces[Index].Item) == EKitPieceKind::Unknown)
		{
			Problems.Add(FString::Printf(TEXT("piece %d is empty or not gear"), Index));
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
		for (const FEnemyKitPiece &Piece : Enemy ? Enemy->EnemyKit.Pieces : TArray<FEnemyKitPiece>())
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
	Output += FString::Printf(TEXT(" | most weapons %s, adapters %s, skills %d (before slots)"), *GetLimitString(GetMostWeapons(Kit, Class)),
							  *GetLimitString(GetMostAdapters(Kit, Class)), Kit.bLimitSkills ? Kit.MostSkills : DEFAULT_MOST_SKILLS);
	Output += FString::Printf(TEXT(" | weapon types %s | elements %s | tiers %s | evolution %s"),
							  Types.IsEmpty() ? TEXT("any") : *FString::Join(Types, TEXT(", ")),
							  Elements.IsEmpty() ? TEXT("any") : *FString::Join(Elements, TEXT(", ")),
							  Kit.bLimitTiers ? *FString::Printf(TEXT("%s to %s"), *ItemIdentity::GetTierLetter(Kit.MinTier), *ItemIdentity::GetTierLetter(Kit.MaxTier))
											  : TEXT("its zone's"),
							  Kit.bAllowEvolution ? TEXT("yes") : TEXT("no"));
	for (const FString &Problem : CheckAlwaysHas(Kit, Class))
	{
		Output += FString::Printf(TEXT(" | PROBLEM: %s"), *Problem);
	}
	return Output;
}
