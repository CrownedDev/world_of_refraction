// ClassDefaultLoadoutData.cpp

#include "Loadout/ClassDefaultLoadoutData.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Equipment/EquipmentDataBase.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Character/CharacterData.h"
#include "Engine/Texture2D.h"
#include "Inventory/InventoryData.h"
#include "Loadout/FSavedLoadout.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "WoRLog.h"

bool UClassDefaultLoadoutData::FindNumbers(ECharacterClass Class, ESpellElement Element, bool bBrokenDarkness,
										   FClassElementNumbers &OutNumbers, int32 &OutActionSpeed) const
{
	const FClassNumbers *Row = Classes.Find(Class);
	if (!Row)
	{
		return false;
	}
	OutActionSpeed = Row->ActionSpeed;
	if (Class != ECharacterClass::Caster)
	{
		OutNumbers = Row->Base;
		return true;
	}
	const bool bWantBrokenDarkness = bBrokenDarkness && Element == ESpellElement::Darkness;
	const FClassElementNumbers *Choice = Row->Elements.FindByPredicate([Element, bWantBrokenDarkness](const FClassElementNumbers &Entry)
	{
		return Entry.Element == Element && Entry.bBrokenDarkness == bWantBrokenDarkness;
	});
	if (!Choice)
	{
		return false;
	}
	OutNumbers = *Choice;
	return true;
}

int32 UClassDefaultLoadoutData::FillFrom(const TArray<UCharacterData *> &Sources)
{
	int32 Written = 0;
	for (const UCharacterData *Source : Sources)
	{
		if (!Source)
		{
			continue;
		}
		FClassElementNumbers Numbers;
		Numbers.Element = Source->InnateElement;
		Numbers.bBrokenDarkness = Source->bBrokenDarknessInnate;
		Numbers.WorldMindLevel = Source->WorldMindLevel;
		Numbers.WorldBodyLevel = Source->WorldBodyLevel;
		Numbers.WorldSpiritLevel = Source->WorldSpiritLevel;
		Numbers.Portrait = Source->Portrait;
		const UInventoryData *Inventory = Source->Inventory;
		const bool bCaster = Source->CharacterClass == ECharacterClass::Caster;
		if (bCaster && Inventory && !Inventory->SavedLoadouts.IsEmpty())
		{
			const FSavedLoadout &Loadout = Inventory->SavedLoadouts[FMath::Clamp(Inventory->DefaultActiveLoadoutIndex, 0, Inventory->SavedLoadouts.Num() - 1)];
			for (USpellData *Spell : FSpellRef::ExtractSpells(Loadout.InnateSpells))
			{
				Numbers.InnateSpells.Add(Spell);
			}
			for (const FSavedBDElementSpellPool &Pool : Loadout.BDSpellPools)
			{
				Numbers.BDSpellPools.Add(Pool.ToRuntimePool());
			}
		}
		FClassNumbers &Row = Classes.FindOrAdd(Source->CharacterClass);
		Row.ActionSpeed = Source->ActionSpeed;
		if (bCaster)
		{
			Row.Elements.RemoveAll([&Numbers](const FClassElementNumbers &Entry)
			{
				return Entry.Element == Numbers.Element && Entry.bBrokenDarkness == Numbers.bBrokenDarkness;
			});
			Row.Elements.Add(Numbers);
		}
		else
		{
			Row.Base = Numbers;
		}
		++Written;
		UE_LOG(LogWoRLoadout, Log, TEXT("[ClassTable] %s filled from %s"), *UEnum::GetDisplayValueAsText(Source->CharacterClass).ToString(), *Source->GetName());
	}
	return Written;
}

void UClassDefaultLoadoutData::FillFromSources()
{
	TArray<UCharacterData *> Sources;
	for (UCharacterData *Source : FillSources)
	{
		Sources.Add(Source);
	}
	const int32 Written = FillFrom(Sources);
	if (Written > 0)
	{
		MarkPackageDirty();
	}
	UE_LOG(LogWoRLoadout, Display, TEXT("[ClassTable] %d row(s) filled from %d source(s); save the asset"), Written, FillSources.Num());
	PrintClassTable();
}

FString UClassDefaultLoadoutData::GetClassTableString() const
{
	FString Out = FString::Printf(TEXT("[ClassTable] %s: %d class row(s)"), *GetName(), Classes.Num());
	auto Line = [](const FClassElementNumbers &Numbers)
	{
		return FString::Printf(TEXT("M/B/S %d/%d/%d, portrait %s, %d innate spell(s), %d BD pool(s)"), Numbers.WorldMindLevel,
							   Numbers.WorldBodyLevel, Numbers.WorldSpiritLevel, *Numbers.Portrait.GetAssetName(),
							   Numbers.InnateSpells.Num(), Numbers.BDSpellPools.Num());
	};
	for (const TPair<ECharacterClass, FClassNumbers> &Pair : Classes)
	{
		Out += LINE_TERMINATOR;
		Out += FString::Printf(TEXT("  %s: action speed %d"), *UEnum::GetDisplayValueAsText(Pair.Key).ToString(), Pair.Value.ActionSpeed);
		if (Pair.Key != ECharacterClass::Caster)
		{
			Out += TEXT(", ") + Line(Pair.Value.Base);
		}
		for (const FClassElementNumbers &Choice : Pair.Value.Elements)
		{
			Out += LINE_TERMINATOR;
			Out += FString::Printf(TEXT("    %s%s: %s"), *UEnum::GetDisplayValueAsText(Choice.Element).ToString(),
								   Choice.bBrokenDarkness ? TEXT(" (Broken Darkness)") : TEXT(""), *Line(Choice));
		}
	}
	return Out;
}

void UClassDefaultLoadoutData::PrintClassTable() const
{
	UE_LOG(LogWoRLoadout, Display, TEXT("%s"), *GetClassTableString());
}

FString UClassDefaultLoadoutData::GetDefaultsString() const
{
	FString Out = FString::Printf(TEXT("[ClassDefaults] %s: %d class kit(s)"), *GetName(), Defaults.Num());
	for (const TPair<ECharacterClass, FClassDefaultLoadout> &Pair : Defaults)
	{
		const FClassDefaultLoadout &Kit = Pair.Value;
		Out += FString::Printf(TEXT("\n  %s: %d place(s), ACTIVE %d, %d apparel"), *UEnum::GetDisplayValueAsText(Pair.Key).ToString(),
							   Kit.Pieces.Num(), Kit.ActiveIndex, Kit.Apparel.Num());
		for (int32 Index = 0; Index < Kit.Pieces.Num(); ++Index)
		{
			const FClassDefaultPiece &Piece = Kit.Pieces[Index];
			TArray<FString> Skills;
			for (const UAbilityData *Skill : Piece.SkillPicks)
			{
				Skills.Add(GetNameSafe(Skill));
			}
			Out += FString::Printf(TEXT("\n    [%d] %s (tier %s)%s%s"), Index, Piece.Piece ? *Piece.Piece->GetName() : TEXT("<empty>"),
								   Piece.Piece ? *UEnum::GetDisplayValueAsText(Piece.Piece->Tier).ToString() : TEXT("-"),
								   Piece.OffHand ? *FString::Printf(TEXT(" + off hand %s"), *Piece.OffHand->GetName()) : TEXT(""),
								   Skills.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(", skills %s"), *FString::Join(Skills, TEXT(", "))));
		}
		for (const UApparelData *Apparel : Kit.Apparel)
		{
			Out += FString::Printf(TEXT("\n    apparel %s"), *GetNameSafe(Apparel));
		}
		if (!Kit.Pieces.IsValidIndex(Kit.ActiveIndex) || !Kit.Pieces[Kit.ActiveIndex].Piece)
		{
			Out += TEXT("\n    WARNING: the ACTIVE place holds no piece");
		}
	}
	return Out;
}

void UClassDefaultLoadoutData::PrintDefaults() const
{
	UE_LOG(LogWoRLoadout, Display, TEXT("%s"), *GetDefaultsString());
}
