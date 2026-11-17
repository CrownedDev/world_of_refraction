// EnemyKitRoll.cpp

#include "Trial/EnemyKitRoll.h"

#include "Character/CharacterData.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/GearSockets.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/LoadoutRules.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/ElementHelpers.h"
#include "Skills/Definitions/SpellData.h"
#include "Combat/Encounter/EncounterComponent.h"
#include "Combat/Encounter/EncounterData.h"
#include "Combat/Encounter/EnemyZone.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Trial/PrebuiltFindData.h"
#include "Trial/TrialData.h"
#include "Trial/TrialLoot.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRLog.h"

namespace
{
	/** A pool or always-has item as a wearable piece: a weapon or an adapter, a prebuilt find's piece with its crystal. */
	bool AsPiece(UObject *Item, FRolledKitPiece &Out)
	{
		if (const UPrebuiltFindData *Find = Cast<UPrebuiltFindData>(Item))
		{
			if (!AsPiece(Find->Piece, Out))
			{
				return false;
			}
			Out.Crystal = Find->Crystal; // arrives built (D-231)
			return true;
		}
		if (Cast<UWeaponData>(Item) || Cast<URingData>(Item))
		{
			Out.Piece = Cast<UEquipmentDataBase>(Item);
			return true;
		}
		return false;
	}

	bool IsWeapon(const FRolledKitPiece &Piece) { return Cast<UWeaponData>(Piece.Piece) != nullptr; }

	int32 CountWeapons(const FRolledEnemyKit &Kit)
	{
		return Kit.Pieces.FilterByPredicate([](const FRolledKitPiece &Piece) { return IsWeapon(Piece); }).Num();
	}

	bool HasRoom(int32 Count, int32 Limit) { return Limit == LoadoutClassTable::NO_LIMIT || Count < Limit; }

	EItemTier RollTier(const FTrialTierRange &Range, FRandomStream &Roll)
	{
		return static_cast<EItemTier>(Roll.RandRange(static_cast<int32>(Range.Min), static_cast<int32>(Range.Max)));
	}

	/** Step 3: a crystal for an adapter whose socket is empty, inside the element limit and the socket's accepted type. */
	void RollCrystal(FRolledKitPiece &Piece, const FEnemyKit &Limits, const FEnemyKitRollInput &Input, const FTrialTierRange &Range, FRandomStream &Roll)
	{
		if (IsWeapon(Piece) || Piece.Crystal.Type != ECrystalType::None)
		{
			return;
		}
		const ECrystalType Accepted = Piece.Piece->AcceptedCrystalType;
		Piece.Crystal = TrialPool::PickCrystal(Input.TrialElement, Range, Roll, [&Limits, Accepted](ECrystalType Type)
		{
			return GearSockets::AcceptsCrystal(Accepted, Type) && EnemyKitRules::AllowsElement(Limits, CrystalTypeHelpers::GetElement(Type));
		});
	}

	bool CanUse(const FRolledEnemyKit &Kit, const UAbilityData &Ability)
	{
		// The same test FCombatLoadout::FillEmptyPicksFromKnown fills with, so a chosen skill finds its slot.
		return !Ability.IsAttack() && Kit.Pieces.ContainsByPredicate([&Ability](const FRolledKitPiece &Piece)
		{
			const UWeaponData *Weapon = Cast<UWeaponData>(Piece.Piece);
			return Weapon && Weapon->WeaponType == Ability.RequiredWeaponType;
		});
	}

	bool CanUse(const FRolledEnemyKit &Kit, const USpellData &Spell)
	{
		return Kit.Pieces.ContainsByPredicate([&Spell](const FRolledKitPiece &Piece)
		{
			return !IsWeapon(Piece) && Piece.Crystal.Type != ECrystalType::None &&
				   ElementHelpers::SpellElementMatchesHost(Spell.Element, CrystalTypeHelpers::GetElement(Piece.Crystal.Type));
		});
	}

	FString GetPieceString(const FRolledKitPiece &Piece)
	{
		FString Line = FString::Printf(TEXT("%s %s"), *GetNameSafe(Piece.Piece), *ItemIdentity::GetTierLetter(Piece.Tier));
		if (Piece.Crystal.Type != ECrystalType::None)
		{
			Line += FString::Printf(TEXT(" [%s]"), *ItemIdentity::GetDisplayName(Piece.Crystal));
		}
		return Piece.bAlwaysHas ? Line + TEXT(" (always)") : Line;
	}
}

FString EnemyKitRoll::MakeEncounterKey(TArray<FName> EncounterIds)
{
	EncounterIds.Sort(FNameLexicalLess());
	TArray<FString> Names;
	for (const FName &Id : EncounterIds)
	{
		Names.Add(Id.ToString());
	}
	return FString::Join(Names, TEXT("|"));
}

int32 EnemyKitRoll::MakeSeed(int32 RunSeed, const FString &EncounterKey, const UCharacterData *Template, int32 Occurrence)
{
	// Text hashes: an FName's or a pointer's hash changes between launches.
	uint32 Hash = HashCombine(GetTypeHash(RunSeed), FCrc::StrCrc32(*EncounterKey));
	Hash = HashCombine(Hash, FCrc::StrCrc32(Template ? *Template->GetPathName() : TEXT("")));
	return static_cast<int32>(HashCombine(Hash, GetTypeHash(Occurrence)));
}

FRolledEnemyKit EnemyKitRoll::Roll(const UCharacterData &Template, const FEnemyKitRollInput &Input)
{
	const FEnemyKit &Limits = Template.EnemyKit;
	const ECharacterClass Class = Template.CharacterClass;
	const FTrialTierRange Range = EnemyKitRules::GetTierRange(Limits, Input.ZoneRange);
	FRandomStream Roll(Input.Seed);
	FRolledEnemyKit Kit;

	// 1. The always-has pieces, at their given tiers.
	for (const FEnemyKitPiece &Given : Limits.Pieces)
	{
		FRolledKitPiece Piece;
		if (!AsPiece(Given.Item, Piece))
		{
			UE_LOG(LogWoRLoadout, Log, TEXT("[EnemyKitRoll] %s: %s is not worn yet (an evolution or apparel; B-568 shortcut)"), *Template.GetName(),
				   *GetNameSafe(Given.Item));
			continue;
		}
		Piece.Tier = Given.Tier;
		Piece.bAlwaysHas = true;
		Kit.Pieces.Add(Piece);
	}

	// 2. More from the pool, inside the limits and the class cap (unset: everything the class can wear).
	const int32 MostWeapons = EnemyKitRules::GetMostOfKind(Limits, Class, EEnemyKitKind::Weapon);
	const int32 MostAdapters = EnemyKitRules::GetMostOfKind(Limits, Class, EEnemyKitKind::Adapter);
	const int32 MostGear = EnemyKitRules::GetMostGear(Limits, Class);
	TArray<FRolledKitPiece> Candidates;
	for (UObject *Item : Input.Pool)
	{
		FRolledKitPiece Piece;
		if (!AsPiece(Item, Piece))
		{
			continue;
		}
		const UWeaponData *Weapon = Cast<UWeaponData>(Piece.Piece);
		if (!Weapon || EnemyKitRules::AllowsWeaponType(Limits, Weapon->WeaponType))
		{
			Candidates.Add(Piece);
		}
	}
	const bool bRangeMeets = Range.Min <= Range.Max; // else nothing from the pool (Q-567)
	while (bRangeMeets)
	{
		const int32 Weapons = CountWeapons(Kit);
		const int32 Adapters = Kit.Pieces.Num() - Weapons;
		if (!HasRoom(Kit.Pieces.Num(), MostGear))
		{
			break;
		}
		const TArray<FRolledKitPiece> Fits = Candidates.FilterByPredicate([&](const FRolledKitPiece &Piece)
		{
			return IsWeapon(Piece) ? HasRoom(Weapons, MostWeapons) : HasRoom(Adapters, MostAdapters);
		});
		if (Fits.IsEmpty())
		{
			break;
		}
		FRolledKitPiece Piece = Fits[Roll.RandRange(0, Fits.Num() - 1)]; // the same piece may come twice (two Adapter Rings)
		Piece.Tier = RollTier(Range, Roll);
		Kit.Pieces.Add(Piece);
	}

	// 3. A crystal in each empty adapter.
	for (FRolledKitPiece &Piece : Kit.Pieces)
	{
		RollCrystal(Piece, Limits, Input, bRangeMeets ? Range : Input.ZoneRange, Roll); // an always-has adapter still gets one
	}

	// 4. Skills: the always-has ones, then ones the gear can use, up to the skill limit and the slots.
	const int32 MostSkills = EnemyKitRules::GetMostSkills(Limits, GetSlotCount(Kit));
	int32 Chosen = 0;
	for (USkillDataBase *Skill : Limits.Skills)
	{
		if (Chosen >= MostSkills)
		{
			break;
		}
		if (UAbilityData *Ability = Cast<UAbilityData>(Skill))
		{
			Kit.Abilities.AddUnique(Ability);
			++Chosen;
		}
		else if (USpellData *Spell = Cast<USpellData>(Skill))
		{
			Kit.Spells.AddUnique(Spell);
			++Chosen;
		}
	}
	TArray<USkillDataBase *> Usable;
	for (UObject *Item : Input.Pool)
	{
		const UAbilityData *Ability = Cast<UAbilityData>(Item);
		const USpellData *Spell = Cast<USpellData>(Item);
		const bool bUsable = (Ability && CanUse(Kit, *Ability)) || (Spell && EnemyKitRules::AllowsElement(Limits, Spell->Element) && CanUse(Kit, *Spell));
		if (bUsable && !Limits.Skills.Contains(Cast<USkillDataBase>(Item)))
		{
			Usable.Add(Cast<USkillDataBase>(Item));
		}
	}
	while (Chosen < MostSkills && !Usable.IsEmpty())
	{
		USkillDataBase *Skill = Usable[Roll.RandRange(0, Usable.Num() - 1)];
		Usable.RemoveSingle(Skill);
		if (UAbilityData *Ability = Cast<UAbilityData>(Skill))
		{
			Kit.Abilities.Add(Ability);
		}
		else
		{
			Kit.Spells.Add(Cast<USpellData>(Skill));
		}
		++Chosen;
	}
	return Kit;
}

FEnemyKitRollInput EnemyKitRoll::MakeInput(const UTrialData *Trial, const FEncounterZoneInfo &Zone)
{
	FEnemyKitRollInput Input;
	const FGameplayTag Pool = TrialPool::GetPoolTag(Trial, Zone);
	if (Pool.IsValid())
	{
		Input.Pool = TrialLoot::GatherPool(Pool);
	}
	Input.TrialElement = Trial ? Trial->Element : ESpellElement::Generic;
	Input.ZoneRange = TrialPool::GetZoneTierRange(Zone.Tier);
	return Input;
}

TArray<FRolledEnemyKit> EnemyKitRoll::RollTeam(TConstArrayView<UCharacterData *> Team, FEnemyKitRollInput Input, int32 RunSeed, const FString &EncounterKey)
{
	TArray<FRolledEnemyKit> Kits;
	Kits.SetNum(Team.Num());
	if (Input.Pool.IsEmpty())
	{
		return Kits;
	}
	TMap<const UCharacterData *, int32> Occurrences;
	for (int32 i = 0; i < Team.Num(); ++i)
	{
		if (Team[i])
		{
			Input.Seed = MakeSeed(RunSeed, EncounterKey, Team[i], Occurrences.FindOrAdd(Team[i])++);
			Kits[i] = Roll(*Team[i], Input);
		}
	}
	return Kits;
}

int32 EnemyKitRoll::GetSlotCount(const FRolledEnemyKit &Kit)
{
	int32 Slots = 0;
	for (const FRolledKitPiece &Piece : Kit.Pieces)
	{
		const bool bCrystalTier = !IsWeapon(Piece) && Piece.Crystal.Type != ECrystalType::None;
		Slots += CrystalEffectTable::SlotsForContainerTier(bCrystalTier ? Piece.Crystal.Tier : Piece.Tier);
	}
	return Slots;
}

FString EnemyKitRoll::GetRolledKitString(const FRolledEnemyKit &Kit)
{
	TArray<FString> Pieces;
	for (const FRolledKitPiece &Piece : Kit.Pieces)
	{
		Pieces.Add(GetPieceString(Piece));
	}
	TArray<FString> Skills;
	for (const UAbilityData *Ability : Kit.Abilities)
	{
		Skills.Add(GetNameSafe(Ability));
	}
	for (const USpellData *Spell : Kit.Spells)
	{
		Skills.Add(GetNameSafe(Spell));
	}
	return FString::Printf(TEXT("%s | %d slots | %s"), Pieces.IsEmpty() ? TEXT("no gear") : *FString::Join(Pieces, TEXT("; ")), GetSlotCount(Kit),
						   Skills.IsEmpty() ? TEXT("no skills") : *FString::Join(Skills, TEXT(", ")));
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GPrintEnemyKitsCommand(
	TEXT("wor.Trial.PrintEnemyKits"),
	TEXT("Every encounter on this map: each enemy's kit for this run, rolled as if that encounter were fought on its own (B-568)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const UGameInstance *Game = World ? World->GetGameInstance() : nullptr;
		const UTrialRunSubsystem *Run = Game ? Game->GetSubsystem<UTrialRunSubsystem>() : nullptr;
		if (!Run || !Run->GetActiveTrial())
		{
			UE_LOG(LogWoRLoadout, Log, TEXT("[EnemyKitRoll] no active trial: enemies wear their authored inventories"));
			return;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const UEncounterComponent *Encounter = It->FindComponentByClass<UEncounterComponent>();
			const TArray<UCharacterData *> Team = Encounter && Encounter->EncounterData ? Encounter->EncounterData->GetEnemyRoster() : TArray<UCharacterData *>();
			if (Team.IsEmpty())
			{
				continue;
			}
			const FEncounterZoneInfo Zone = AEnemyZone::FindZoneAt(World, It->GetActorLocation());
			const TArray<FRolledEnemyKit> Kits = EnemyKitRoll::RollTeam(Team, EnemyKitRoll::MakeInput(Run->GetActiveTrial(), Zone), Run->GetRunSeed(),
																		EnemyKitRoll::MakeEncounterKey({Encounter->GetEncounterId()}));
			for (int32 i = 0; i < Team.Num(); ++i)
			{
				UE_LOG(LogWoRLoadout, Log, TEXT("[EnemyKitRoll] %s (zone %d) %s: %s"), *It->GetName(), Zone.Tier, *GetNameSafe(Team[i]),
					   Kits[i].HasGear() ? *EnemyKitRoll::GetRolledKitString(Kits[i]) : TEXT("authored inventory (no pool)"));
			}
		}
	}));
#endif
