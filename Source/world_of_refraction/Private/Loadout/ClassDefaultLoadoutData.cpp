// ClassDefaultLoadoutData.cpp

#include "Loadout/ClassDefaultLoadoutData.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Equipment/EquipmentDataBase.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Rings/RingData.h"
#include "Character/CharacterData.h"
#include "Loadout/WornGear.h"
#include "UI/Menus/NewGameScreenModel.h"
#include "Engine/Texture2D.h"
#include "Inventory/InventoryData.h"
#include "Loadout/FSavedLoadout.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "WoRGameNames.h"
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

#define LOCTEXT_NAMESPACE "ClassDefaultLoadout"

const FPlayerPicks *UClassDefaultLoadoutData::FindRecruit(ECharacterClass Class, ESpellElement Element, bool bBrokenDarkness) const
{
	return Recruits.FindByPredicate([Class, Element, bBrokenDarkness](const FPlayerPicks &Row)
	{
		return Row.Class == Class && (Class != ECharacterClass::Caster || (Row.Element == Element && Row.bBrokenDarkness == bBrokenDarkness));
	});
}

TArray<FPlayerPicks> UClassDefaultLoadoutData::MakeDefaultRecruits()
{
	const auto Make = [](ECharacterClass Class, const FText &Name)
	{
		FPlayerPicks Row;
		Row.Name = Name.ToString();
		Row.Class = Class;
		Row.bRecruit = true;
		return Row;
	};
	TArray<FPlayerPicks> Rows;
	FPlayerPicks &Generic = Rows.Add_GetRef(Make(ECharacterClass::Generic, LOCTEXT("GenericSpirit", "Generic Spirit")));
	Generic.Weapons = {EWeaponType::Sword, EWeaponType::Shield};
	FPlayerPicks &Resonator = Rows.Add_GetRef(Make(ECharacterClass::Resonator, LOCTEXT("ResonatorSpirit", "Resonator Spirit")));
	Resonator.Crystals = {ECrystalType::Garnet, ECrystalType::Sapphire};
	const FText SpiritFormat = LOCTEXT("ElementSpirit", "{0} Spirit");
	for (uint8 Value = static_cast<uint8>(ESpellElement::Fire); Value < static_cast<uint8>(ESpellElement::None); ++Value)
	{
		const ESpellElement Element = static_cast<ESpellElement>(Value);
		FPlayerPicks &Refractor = Rows.Add_GetRef(Make(ECharacterClass::Caster, FText::Format(SpiritFormat, WoRGameNames::GetElementName(Element))));
		Refractor.Element = Element;
	}
	FPlayerPicks &Broken = Rows.Add_GetRef(Make(ECharacterClass::Caster, LOCTEXT("BrokenDarknessSpirit", "Broken Darkness Spirit")));
	Broken.Element = ESpellElement::Darkness;
	Broken.bBrokenDarkness = true;
	return Rows;
}

#undef LOCTEXT_NAMESPACE

void UClassDefaultLoadoutData::FillRecruits()
{
	Recruits = MakeDefaultRecruits();
	MarkPackageDirty();
	UE_LOG(LogWoRLoadout, Display, TEXT("[ClassTable] %d recruit row(s) filled; set each Body (Starting spirit), then save"), Recruits.Num());
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
	// B-556: the pick tables.
	Out += FString::Printf(TEXT("\n  Picks: adapter %s; %d weapon type(s)"), *GetNameSafe(PickAdapter.Get()), PickWeapons.Num());
	for (const TPair<EWeaponType, TObjectPtr<UWeaponData>> &Pair : PickWeapons)
	{
		Out += FString::Printf(TEXT("\n    %s: %s (tier %s)"), *UEnum::GetDisplayValueAsText(Pair.Key).ToString(), *GetNameSafe(Pair.Value.Get()),
							   Pair.Value ? *UEnum::GetDisplayValueAsText(Pair.Value->Tier).ToString() : TEXT("-"));
	}
	// B-557: the recruit rows.
	Out += FString::Printf(TEXT("\n  Recruits: %d"), Recruits.Num());
	for (const FPlayerPicks &Recruit : Recruits)
	{
		Out += FString::Printf(TEXT("\n    %s: %s %s%s, body %s"), *Recruit.Name, *UEnum::GetDisplayValueAsText(Recruit.Class).ToString(),
							   *UEnum::GetDisplayValueAsText(Recruit.Element).ToString(), Recruit.bBrokenDarkness ? TEXT(" (BD)") : TEXT(""),
							   *UEnum::GetDisplayValueAsText(Recruit.Body).ToString());
	}
	return Out;
}

FClassDefaultLoadout UClassDefaultLoadoutData::MakePickedGear(const FPlayerPicks &Picks, const FClassDefaultLoadout *Base) const
{
	FClassDefaultLoadout Gear = Base ? *Base : FClassDefaultLoadout();
	const auto Place = [&Gear](int32 Index) -> FClassDefaultPiece &
	{
		if (Gear.Pieces.Num() <= Index)
		{
			Gear.Pieces.SetNum(Index + 1);
		}
		return Gear.Pieces[Index];
	};
	if (Picks.Class == ECharacterClass::Generic)
	{
		TArray<UWeaponData *> Weapons;
		for (const EWeaponType Type : Picks.Weapons)
		{
			UWeaponData *Weapon = PickWeapons.FindRef(Type);
			UE_CLOG(!Weapon, LogWoRLoadout, Warning, TEXT("[ClassDefaults] %s has no pick weapon for %s (Picks | Pick Weapons): none given"),
					*GetName(), *UEnum::GetDisplayValueAsText(Type).ToString());
			if (Weapon)
			{
				Weapons.Add(Weapon);
			}
		}
		// A shield is held in the off hand only (B-515): it goes after the weapon it pairs with.
		Weapons.StableSort([](const UWeaponData &A, const UWeaponData &B) { return A.WeaponType != EWeaponType::Shield && B.WeaponType == EWeaponType::Shield; });
		UE_CLOG(Weapons.Num() > WornGearLayout::SECONDARY_INDEX + 1, LogWoRLoadout, Warning,
				TEXT("[ClassDefaults] %d weapons picked; only the first two are given"), Weapons.Num());
		// Over a kit only the pieces change: the place's skill picks and crystal stay the kit's.
		// The second place first: growing the list for it moves the first, so a reference taken before it would write
		// the picks into freed memory (the Generic trial-entry crash, over a kit of one piece).
		static_assert(WornGearLayout::SECONDARY_INDEX > WornGearLayout::PRIMARY_INDEX, "Place the higher index first");
		FClassDefaultPiece &Second = Place(WornGearLayout::SECONDARY_INDEX);
		FClassDefaultPiece &Main = Place(WornGearLayout::PRIMARY_INDEX);
		Second = FClassDefaultPiece(); // filled again below when a second weapon takes it
		Main.OffHand = nullptr;
		Main.Piece = Weapons.IsValidIndex(0) ? Weapons[0] : nullptr;
		if (Weapons.IsValidIndex(1))
		{
			const UWeaponData *First = Weapons[0];
			const UWeaponData *Other = Weapons[1];
			const bool bShieldPair = Other->WeaponType == EWeaponType::Shield && First->WeaponType != EWeaponType::Shield;
			const bool bDualPair = Other->WeaponType == First->WeaponType && First->WeaponType != EWeaponType::Shield &&
								   First->Handling != EWeaponHandling::SingleOnly &&
								   Other->Handling != EWeaponHandling::SingleOnly;
			if (bShieldPair || bDualPair)
			{
				Main.OffHand = Weapons[1];
			}
			else
			{
				Second = Base && Base->Pieces.IsValidIndex(WornGearLayout::SECONDARY_INDEX) ? Base->Pieces[WornGearLayout::SECONDARY_INDEX] : FClassDefaultPiece();
				Second.OffHand = nullptr;
				Second.Piece = Weapons[1];
			}
		}
		Gear.ActiveIndex = WornGearLayout::PRIMARY_INDEX;
	}
	else if (Picks.Class == ECharacterClass::Resonator)
	{
		UE_CLOG(!PickAdapter, LogWoRLoadout, Warning, TEXT("[ClassDefaults] %s has no pick adapter (Picks | Pick Adapter): none given"), *GetName());
		for (int32 Count = 0; Count < ClassPickRules::PICK_ADAPTER_COUNT; ++Count)
		{
			FClassDefaultPiece &Adapter = Place(WornGearLayout::FIRST_ADAPTER_INDEX + Count);
			Adapter.Piece = PickAdapter.Get(); // empty unless the kit socketed one: the player sockets the picked crystals
		}
		if (!Base)
		{
			Gear.ActiveIndex = WornGearLayout::FIRST_ADAPTER_INDEX;
		}
	}
	return Gear;
}

TMap<FCrystalId, int32> UClassDefaultLoadoutData::GetPickedCrystals(const FPlayerPicks &Picks)
{
	TMap<FCrystalId, int32> Crystals;
	if (Picks.Class == ECharacterClass::Resonator)
	{
		for (const ECrystalType Type : Picks.Crystals)
		{
			Crystals.FindOrAdd(FCrystalId(Type, EItemTier::F_Tier)) += WoRNewGame::STARTING_CRYSTALS_PER_TYPE;
		}
	}
	return Crystals;
}

void UClassDefaultLoadoutData::PrintDefaults() const
{
	UE_LOG(LogWoRLoadout, Display, TEXT("%s"), *GetDefaultsString());
}
