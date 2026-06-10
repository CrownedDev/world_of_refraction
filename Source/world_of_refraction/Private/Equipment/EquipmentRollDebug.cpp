// EquipmentRollDebug.cpp

#include "Equipment/EquipmentRollDebug.h"
#include "Loadout/LoadoutComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"

namespace
{
	/** Append "Name +X" tokens for every NON-ZERO field; "(none)" when all zero. */
	FString FormatStatBonus(const FEquipmentStatBonus &B)
	{
		TArray<FString> T;
		auto AddInt = [&T](const TCHAR *Name, int32 V) { if (V != 0) { T.Add(FString::Printf(TEXT("%s %+d"), Name, V)); } };
		auto AddFlt = [&T](const TCHAR *Name, float V) { if (V != 0.0f) { T.Add(FString::Printf(TEXT("%s %+.1f"), Name, V)); } };
		AddInt(TEXT("RawDmg"), B.BonusRawDamage);
		AddInt(TEXT("SpellDmg"), B.BonusSpellDamage);
		AddInt(TEXT("Eff"), B.BonusEfficiency);
		AddInt(TEXT("StatusMult"), B.BonusStatusMultiplier);
		AddFlt(TEXT("Crit"), B.BonusCritChance);
		AddInt(TEXT("SpellSpd"), B.BonusSpellSpeed);
		AddInt(TEXT("Def"), B.BonusDefense);
		AddInt(TEXT("ActSpd"), B.BonusActionSpeed);
		AddInt(TEXT("MaxHP"), B.BonusMaxHP);
		AddInt(TEXT("MaxEP"), B.BonusMaxEnergy);
		AddInt(TEXT("Resist"), B.BonusResistance);
		AddInt(TEXT("TurnSpd"), B.BonusTurnSpeed);
		AddInt(TEXT("Luck"), B.BonusLuck);
		AddFlt(TEXT("Mind%"), B.BonusMindModifierPercent);
		AddFlt(TEXT("Body%"), B.BonusBodyModifierPercent);
		AddFlt(TEXT("Spirit%"), B.BonusSpiritModifierPercent);
		return T.Num() ? FString::Join(T, TEXT(", ")) : TEXT("(none)");
	}

	FString FormatResistanceBonus(const FResistanceBonus &R)
	{
		TArray<FString> T;
		auto Add = [&T](const TCHAR *Name, float V) { if (V != 0.0f) { T.Add(FString::Printf(TEXT("%s %+.0f"), Name, V)); } };
		Add(TEXT("Fire"), R.Fire);       Add(TEXT("Water"), R.Water);   Add(TEXT("Earth"), R.Earth);
		Add(TEXT("Wind"), R.Wind);       Add(TEXT("Light"), R.Light);   Add(TEXT("Dark"), R.Darkness);
		Add(TEXT("Ltng"), R.Lightning);  Add(TEXT("Void"), R.Void);     Add(TEXT("Reality"), R.Reality);
		Add(TEXT("Slash"), R.Slash);     Add(TEXT("Pierce"), R.Pierce); Add(TEXT("Impact"), R.Impact);
		return T.Num() ? FString::Join(T, TEXT(", ")) : TEXT("(none)");
	}

	/** One weapon/ring slot block. The path marker keys on PersistentID:
	 *  CreateFrom* deliberately leaves it INVALID, and only the U1c wholesale
	 *  copy of an owned entry carries a valid guid — so valid == the bridge
	 *  resolved an owned instance. */
	void AppendGearSlot(FString &Out, const TCHAR *SlotName, const FString &AssetName,
						const FGuid &PersistentID,
						const FEquipmentStatBonus &Stats, const FResistanceBonus &Resist,
						int32 StatPool, int32 StatMaxPool, int32 ResistPool, int32 ResistMaxPool)
	{
		Out += FString::Printf(TEXT("%s: %s\n"), SlotName, *AssetName);
		Out += PersistentID.IsValid()
				   ? FString::Printf(TEXT("  path: INSTANCE ROLL (ref resolved)  id=%s\n"), *PersistentID.ToString())
				   : TEXT("  path: asset Base (fallback - no instance ref)\n");
		Out += FString::Printf(TEXT("  stats: %s\n"), *FormatStatBonus(Stats));
		Out += FString::Printf(TEXT("  resist: %s\n"), *FormatResistanceBonus(Resist));
		Out += FString::Printf(TEXT("  pools: Stat %d/%d   Resist %d/%d\n"),
							   StatPool, StatMaxPool, ResistPool, ResistMaxPool);
	}
}

FString UEquipmentRollDebug::GetEquipmentRollString(AActor *Actor)
{
	if (!Actor)
	{
		return TEXT("ERROR: Actor is NULL");
	}
	ULoadoutComponent *LC = Actor->FindComponentByClass<ULoadoutComponent>();
	if (!LC)
	{
		return FString::Printf(TEXT("ERROR: %s has no LoadoutComponent"), *Actor->GetName());
	}

	const FCombatLoadout Loadout = LC->GetActiveLoadout();

	FString Out;
	Out += TEXT("========= EQUIPMENT ROLL STATE =========\n");
	Out += FString::Printf(TEXT("Actor: %s   Loadout: %s\n"), *Actor->GetName(), *Loadout.LoadoutName);

	// Per-slot detail — read straight off the active loadout's entries, the same
	// data GetActiveStatBonus/GetActiveResistanceBonus consume.
	if (Loadout.PrimaryWeapon.IsValid())
	{
		const FWeaponInventoryEntry &E = Loadout.PrimaryWeapon.WeaponEntry;
		AppendGearSlot(Out, TEXT("PrimaryWeapon"), E.Weapon ? E.Weapon->Name : TEXT("null"),
					   E.PersistentID, E.StatBonus, E.ResistanceBonus,
					   E.StatPool, E.StatMaxPool, E.ResistancePool, E.ResistanceMaxPool);
	}
	if (Loadout.SecondaryWeapon.IsValid())
	{
		const FWeaponInventoryEntry &E = Loadout.SecondaryWeapon.WeaponEntry;
		AppendGearSlot(Out, TEXT("SecondaryWeapon"), E.Weapon ? E.Weapon->Name : TEXT("null"),
					   E.PersistentID, E.StatBonus, E.ResistanceBonus,
					   E.StatPool, E.StatMaxPool, E.ResistancePool, E.ResistanceMaxPool);
	}
	if (Loadout.PrimaryRing.IsValid())
	{
		const FRingInventoryEntry &E = Loadout.PrimaryRing.RingEntry;
		AppendGearSlot(Out, TEXT("PrimaryRing"), E.Ring ? E.Ring->Name : TEXT("null"),
					   E.PersistentID, E.StatBonus, E.ResistanceBonus,
					   E.StatPool, E.StatMaxPool, E.ResistancePool, E.ResistanceMaxPool);
	}
	for (int32 i = 0; i < Loadout.RingLoadout.Num(); ++i)
	{
		const FRingInventoryEntry &E = Loadout.RingLoadout[i].RingEntry;
		AppendGearSlot(Out, *FString::Printf(TEXT("RingSlot[%d]"), i), E.Ring ? E.Ring->Name : TEXT("null"),
					   E.PersistentID, E.StatBonus, E.ResistanceBonus,
					   E.StatPool, E.StatMaxPool, E.ResistancePool, E.ResistanceMaxPool);
	}

	// Evolution — the SAME accessor the pillar/resistance reads use (case A:
	// weapon-attached), then the standalone slot (case B). FEvolutionAttachment
	// carries no guid (destroyed-on-removal design), so the path marker is a
	// HEURISTIC: any instance state (MaxPool or Generated non-zero) means the
	// bridge carried a roll; all-zero means asset-Base only.
	const FEvolutionAttachment WeaponEvo = LC->GetActivePrimaryEvolutionAttachment(Actor);
	const FEvolutionAttachment *Evo = nullptr;
	const TCHAR *EvoSlotName = nullptr;
	if (WeaponEvo.Item)
	{
		Evo = &WeaponEvo;
		EvoSlotName = TEXT("Evolution (weapon-attached)");
	}
	else if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution.Item)
	{
		Evo = &Loadout.PrimaryEvolution;
		EvoSlotName = TEXT("Evolution (standalone slot)");
	}
	if (Evo)
	{
		const bool bHasInstanceState =
			Evo->StatMaxPool > 0 || Evo->ResistanceMaxPool > 0 ||
			FormatStatBonus(Evo->GeneratedStatBonus) != TEXT("(none)") ||
			FormatResistanceBonus(Evo->GeneratedResistance) != TEXT("(none)");
		Out += FString::Printf(TEXT("%s: %s\n"), EvoSlotName, *Evo->Item->ItemName);
		Out += bHasInstanceState
				   ? TEXT("  path: INSTANCE ROLL (instance state present - heuristic, no guid on attachments)\n")
				   : TEXT("  path: asset Base (no instance state)\n");
		Out += FString::Printf(TEXT("  generated stats: %s\n"), *FormatStatBonus(Evo->GeneratedStatBonus));
		Out += FString::Printf(TEXT("  generated resist: %s\n"), *FormatResistanceBonus(Evo->GeneratedResistance));
		Out += FString::Printf(TEXT("  pools: Stat %d/%d   Resist %d/%d\n"),
							   Evo->StatPool, Evo->StatMaxPool, Evo->ResistancePool, Evo->ResistanceMaxPool);
	}

	// The aggregates combat actually consumes — via the real gameplay accessors.
	Out += TEXT("--- Combat aggregates (gameplay accessors) ---\n");
	Out += FString::Printf(TEXT("  GetActiveStatBonus:        %s\n"), *FormatStatBonus(LC->GetActiveStatBonus(Actor)));
	Out += FString::Printf(TEXT("  GetActiveResistanceBonus:  %s\n"), *FormatResistanceBonus(LC->GetActiveResistanceBonus(Actor)));
	Out += TEXT("========================================\n");
	return Out;
}

void UEquipmentRollDebug::PrintEquipmentRoll(AActor *Actor, float Duration, FLinearColor TextColor)
{
	const FString RollString = GetEquipmentRollString(Actor);
	if (GEngine)
	{
		// Line-split + reverse so on-screen messages read top-to-bottom
		// (mirrors UCharacterDataDebug::PrintCharacterStats).
		TArray<FString> Lines;
		RollString.ParseIntoArray(Lines, TEXT("\n"));
		for (int32 i = Lines.Num() - 1; i >= 0; --i)
		{
			GEngine->AddOnScreenDebugMessage(-1, Duration, TextColor.ToFColor(true), Lines[i]);
		}
	}
}

void UEquipmentRollDebug::LogEquipmentRoll(AActor *Actor)
{
	UE_LOG(LogTemp, Display, TEXT("\n%s"), *GetEquipmentRollString(Actor));
}
