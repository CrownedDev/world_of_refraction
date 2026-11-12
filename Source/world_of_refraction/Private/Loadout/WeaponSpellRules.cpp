// WeaponSpellRules.cpp

#include "Loadout/WeaponSpellRules.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Loadout/Entries/FWeaponInventoryEntry.h"
#include "Pool/PoolSubsystem.h"
#include "Equipment/Weapons/WeaponData.h"
#include "WoRLog.h"

namespace
{
	TAutoConsoleVariable<int32> CVarLoadoutWeaponSpells(
		TEXT("wor.Loadout.WeaponSpells"),
		0,
		TEXT("B-098 (D-027): 0 = weapons give no spells (the rule); 1 = a weapon crystal offers its spells again (the old ")
		TEXT("Resonate Weapon list), for a PIE comparison only. Read when a menu opens or spells are listed."),
		ECVF_Default);

	int32 CountSet(const TArray<USpellData *> &Spells)
	{
		int32 Count = 0;
		for (const USpellData *Spell : Spells)
		{
			Count += Spell ? 1 : 0;
		}
		return Count;
	}
}

bool WeaponSpellRules::WeaponsGiveSpells()
{
	return CVarLoadoutWeaponSpells.GetValueOnGameThread() != 0;
}

static FAutoConsoleCommandWithWorld GLoadoutListWeaponSpellsCommand(
	TEXT("wor.Loadout.ListWeaponSpells"),
	TEXT("B-098 survey: every owned weapon that still holds spells (assigned or picked). Since D-027 they are not offered."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld *World)
		{
			const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
			const UPoolSubsystem *Pool = GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
			if (!Pool)
			{
				UE_LOG(LogWoR, Display, TEXT("[WeaponSpells] no pool"));
				return;
			}
			int32 Holding = 0;
			for (const FWeaponInventoryEntry &Entry : Pool->GetOwnedWeapons())
			{
				const int32 Assigned = CountSet(Entry.AssignedSpells);
				const int32 Picked = CountSet(Entry.SpellPicks);
				if (Assigned + Picked > 0)
				{
					++Holding;
					UE_LOG(LogWoR, Display, TEXT("[WeaponSpells] %s: %d assigned, %d picked (not offered: D-027)"),
						   Entry.Weapon ? *Entry.Weapon->Name : TEXT("<no weapon>"), Assigned, Picked);
				}
			}
			UE_LOG(LogWoR, Display, TEXT("[WeaponSpells] %d of %d owned weapons hold spells; weapons give spells: %s"), Holding,
				   Pool->GetOwnedWeapons().Num(), WeaponSpellRules::WeaponsGiveSpells() ? TEXT("YES (wor.Loadout.WeaponSpells 1)") : TEXT("no"));
		}));
