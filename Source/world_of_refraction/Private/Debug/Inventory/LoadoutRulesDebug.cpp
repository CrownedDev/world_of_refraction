// LoadoutRulesDebug.cpp

#include "Debug/Inventory/LoadoutRulesDebug.h"
#include "Debug/Inventory/InventoryDebug.h"
#include "Loadout/LoadoutComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "UObject/UObjectIterator.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/Entries/FApparelEntry.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Pool/PoolSubsystem.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/FCrystalAttachment.h"
#include "Equipment/Durability/DurabilityConstants.h"
#include "Engine/GameInstance.h"
#include "Character/CharacterDataComponent.h"
#include "UObject/UObjectIterator.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float LOADOUT_RULES_SCREEN_SECONDS = 10.0f;

	FAutoConsoleCommandWithWorld GCompareLoadoutSlotsCmd(
		TEXT("wor.Loadout.CompareSlots"),
		TEXT("DEBUG: per character, the unified slot mirror (B-302, D-110) against the old active weapon / adapter getters."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				ULoadoutRulesDebug::CompareLoadoutSlots(World);
			}));

	// B-172: what wor.Loadout.CompareRules still gave the PIE sheets once the legacy validators went: the live findings.
	FAutoConsoleCommandWithWorld GPrintLoadoutFindingsCmd(
		TEXT("wor.Loadout.PrintFindings"),
		TEXT("DEBUG: per character, what the loadout rules (FLoadoutRules) report on its active loadout, or 'none'."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				ULoadoutRulesDebug::CompareLoadoutRules(World);
			}));

	// B-312 (Q-312): each member's gear copies against its view of the pool store; silence means the views are ready.
	FAutoConsoleCommandWithWorld GCompareStoreCmd(
		TEXT("wor.Inventory.CompareStore"),
		TEXT("DEBUG: per character, its weapon / adapter / evolution copies against its view of the pool store (B-312)."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				int32 Members = 0;
				int32 Differing = 0;
				for (TActorIterator<AActor> It(World); It; ++It)
				{
					const UInventoryComponent *Inventory = It->FindComponentByClass<UInventoryComponent>();
					if (!Inventory || !Inventory->DrawsSharedGear())
					{
						continue;
					}
					++Members;
					const FString Differences = Inventory->CompareWithStore();
					if (!Differences.IsEmpty())
					{
						++Differing;
						UE_LOG(LogTemp, Warning, TEXT("[CompareStore] %s:\n%s"), *GetNameSafe(*It), *Differences);
					}
				}
				UE_LOG(LogTemp, Log, TEXT("[CompareStore] %d member(s) drawing from the store, %d differ"), Members, Differing);
			}));

	FAutoConsoleCommandWithWorldAndArgs GTryAbilityPickCmd(
		TEXT("wor.Loadout.TryAbility"),
		TEXT("DEBUG: wor.Loadout.TryAbility <ability name> - try a learned ability as a pick on each character's active loadout (B-159)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				if (!World || Args.Num() == 0)
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[LoadoutRules] usage: wor.Loadout.TryAbility <ability name>"));
					return;
				}
				const FString AbilityName = FString::Join(Args, TEXT(" "));
				for (TActorIterator<AActor> It(World); It; ++It)
				{
					if (ULoadoutComponent *Loadout = It->FindComponentByClass<ULoadoutComponent>())
					{
						const FString Line = ULoadoutRulesDebug::TryAbilityPick(Loadout, AbilityName);
						UE_LOG(LogWoRLoadout, Display, TEXT("[LoadoutRules] %s"), *Line);
						if (GEngine)
						{
							GEngine->AddOnScreenDebugMessage(INDEX_NONE, LOADOUT_RULES_SCREEN_SECONDS, FColor::Cyan, Line);
						}
					}
				}
			}));

	// ==================== B-351 (D-145): the one skill-slot list, from the console until the gear screen has it ====================

	/** The first player's inventory (its pawn's, else the first actor's with one). */
	UInventoryComponent *PlayerInventory(UWorld *World)
	{
		if (const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr)
		{
			if (UInventoryComponent *Inventory = PC->GetPawn() ? PC->GetPawn()->FindComponentByClass<UInventoryComponent>() : nullptr)
			{
				return Inventory;
			}
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (UInventoryComponent *Inventory = It->FindComponentByClass<UInventoryComponent>())
			{
				return Inventory;
			}
		}
		return nullptr;
	}

	/** The active loadout's primary weapon entry, or null (with a log line) when there is none. */
	FWeaponLoadoutEntry *ActivePrimaryWeapon(UInventoryComponent *Inventory, const TCHAR *Command)
	{
		if (!Inventory || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
		{
			UE_LOG(LogWoRLoadout, Display, TEXT("[%s] no inventory with an active loadout"), Command);
			return nullptr;
		}
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		if (!Active.PrimaryWeapon.WeaponEntry.Weapon)
		{
			UE_LOG(LogWoRLoadout, Display, TEXT("[%s] the active loadout has no primary weapon"), Command);
			return nullptr;
		}
		return &Active.PrimaryWeapon;
	}

	/** A learned ability, or one of the weapon's built-ins, by its Name (case-insensitive). */
	UAbilityData *FindAbilityByName(const UInventoryComponent &Inventory, const FWeaponLoadoutEntry &Weapon, const FString &Name)
	{
		for (const FAbilityInstance &Instance : Inventory.Abilities.LearnedAbilities)
		{
			if (Instance.Ability && Instance.Ability->Name.Equals(Name, ESearchCase::IgnoreCase))
			{
				return Instance.Ability;
			}
		}
		if (Weapon.WeaponEntry.Weapon)
		{
			for (UAbilityData *Ability : Weapon.WeaponEntry.Weapon->PresetAbilities)
			{
				if (Ability && Ability->Name.Equals(Name, ESearchCase::IgnoreCase))
				{
					return Ability;
				}
			}
		}
		return nullptr;
	}

	void ShowSkillSlots(const FWeaponLoadoutEntry &Weapon)
	{
		const FString Line = Weapon.GetSkillSlotString();
		UE_LOG(LogWoRLoadout, Display, TEXT("[SkillSlots] %s"), *Line);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, LOADOUT_RULES_SCREEN_SECONDS, FColor::Cyan, Line);
		}
	}

	/** B-380: one combatant's loadout in a line: weapons, adapters, apparel (outside the budget) and gear HP / Defence. */
	FString GetLoadoutPrintString(const ULoadoutComponent &Loadout)
	{
		const FCombatLoadout &Active = Loadout.GetActiveLoadout();
		TArray<FString> Adapters;
		if (Active.PrimaryRing.RingEntry.Ring)
		{
			Adapters.Add(Active.PrimaryRing.RingEntry.Ring->GetName());
		}
		for (const FRingLoadoutEntry &Ring : Active.RingLoadout)
		{
			Adapters.Add(GetNameSafe(Ring.RingEntry.Ring));
		}
		const FEquipmentStatBonus Gear = Loadout.GetActiveStatBonus(Loadout.GetOwner());
		return FString::Printf(TEXT("%s: weapons %s / %s; adapters [%s]; %s; gear HP %+d Def %+d"), *GetNameSafe(Loadout.GetOwner()),
							   *GetNameSafe(Active.PrimaryWeapon.WeaponEntry.Weapon), *GetNameSafe(Active.SecondaryWeapon.WeaponEntry.Weapon),
							   *FString::Join(Adapters, TEXT(", ")), *ApparelRules::GetApparelString(Active), Gear.BonusMaxHP, Gear.BonusDefense);
	}

	// Merge B-351c + B-380: wor.Loadout.Print is B-351c's (the player's loadout); this one prints every combatant.
	FAutoConsoleCommandWithWorld GPrintAllLoadoutsCmd(
		TEXT("wor.Loadout.PrintAll"),
		TEXT("DEBUG: wor.Loadout.PrintAll - every combatant's active loadout: weapons, adapters, apparel and gear HP / Defence (B-380)."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				int32 Printed = 0;
				for (TObjectIterator<ULoadoutComponent> It; It; ++It)
				{
					if (!It->IsTemplate() && It->GetWorld() == World)
					{
						UE_LOG(LogWoRLoadout, Display, TEXT("[Loadout] %s"), *GetLoadoutPrintString(**It));
						++Printed;
					}
				}
				UE_LOG(LogWoRLoadout, Display, TEXT("[Loadout] %d loadout(s)"), Printed);
			}));

	FAutoConsoleCommandWithWorldAndArgs GWearApparelCmd(
		TEXT("wor.Loadout.WearApparel"),
		TEXT("DEBUG: wor.Loadout.WearApparel <ApparelData asset path> - the player wears that apparel piece in its active loadout (outside the budget, up to 2; B-380 placeholder until the gear screen)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				UInventoryComponent *Inventory = PlayerInventory(World);
				UApparelData *Piece = Args.Num() > 0 ? LoadObject<UApparelData>(nullptr, *Args[0]) : nullptr;
				if (!Inventory || !Piece || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[Loadout] usage: wor.Loadout.WearApparel <ApparelData asset path> (in a level with the player)"));
					return;
				}
				FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
				if (Active.bLore)
				{
					// Review: a lore loadout is not saved (custom loadouts are), so apparel worn there would be lost on reload.
					UE_LOG(LogWoRLoadout, Display, TEXT("[Loadout] the active loadout is a lore loadout (not saved): switch to a custom loadout first (B-380d saves lore apparel)"));
					return;
				}
				FApparelEntry Entry = FApparelEntry::CreateFromApparel(Piece);
				// B-380b: owned first (the pool mints its id), so it is saved with the store.
				if (UPoolSubsystem *Pool = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UPoolSubsystem>() : nullptr)
				{
					if (Active.Apparel.Num() < ApparelRules::MAX_APPAREL_SLOTS)
					{
						// B-380d review: the wearer owns the piece (the evolution owner check reads it).
						const UCharacterDataComponent *Wearer = Inventory->GetOwner()->FindComponentByClass<UCharacterDataComponent>();
						Entry.PersistentID = Pool->AddApparelToPool(Entry, UPoolSubsystem::GetCharacterKey(Wearer));
					}
				}
				if (!ApparelRules::AddApparel(Active, Entry))
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[Loadout] no free apparel slot (%d)"), ApparelRules::MAX_APPAREL_SLOTS);
					return;
				}
				if (ULoadoutComponent *Loadout = Inventory->GetOwner()->FindComponentByClass<ULoadoutComponent>())
				{
					Loadout->NotifyApparelChanged(); // B-380c: pools and stat readers re-read it
					UE_LOG(LogWoRLoadout, Display, TEXT("[Loadout] %s"), *GetLoadoutPrintString(*Loadout));
				}
			}));

	FAutoConsoleCommandWithWorldAndArgs GSocketApparelCmd(
		TEXT("wor.Loadout.SocketApparel"),
		TEXT("DEBUG: wor.Loadout.SocketApparel <slot 0|1> <ECrystalType name, e.g. Garnet> - set an F-tier refined crystal in the player's worn apparel piece (through the store; B-380c placeholder until the gear screen)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				UInventoryComponent *Inventory = PlayerInventory(World);
				UPoolSubsystem *Pool = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UPoolSubsystem>() : nullptr;
				const UEnum *Types = StaticEnum<ECrystalType>();
				const int64 Type = Types && Args.Num() > 1 ? Types->GetValueByNameString(Args[1]) : INDEX_NONE;
				const int32 Slot = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : INDEX_NONE;
				const FCombatLoadout *Active = Inventory && Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex)
												   ? &Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex]
												   : nullptr;
				if (!Pool || !Active || !Active->Apparel.IsValidIndex(Slot) || Type == INDEX_NONE)
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[Loadout] usage: wor.Loadout.SocketApparel <worn apparel slot> <ECrystalType name>"));
					return;
				}
				FRuntimeAttachedItem Gem;
				Gem.Kind = EAttachedItemKind::Crystal;
				Gem.Crystal = FCrystalAttachment(FCrystalId(static_cast<ECrystalType>(Type), EItemTier::F_Tier), DurabilityConstants::MAX_DURABILITY_F_TIER);
				if (!Pool->AttachToApparel(Active->Apparel[Slot].PersistentID, Gem))
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[Loadout] that piece is not owned (wear it with wor.Loadout.WearApparel first)"));
					return;
				}
				if (const ULoadoutComponent *Loadout = Inventory->GetOwner()->FindComponentByClass<ULoadoutComponent>())
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[Loadout] %s"), *GetLoadoutPrintString(*Loadout));
				}
			}));

	FAutoConsoleCommandWithWorldAndArgs GPrintSkillSlotsCmd(
		TEXT("wor.Loadout.PrintSkillSlots"),
		TEXT("DEBUG: wor.Loadout.PrintSkillSlots - the player's primary weapon's one skill-slot list (B-351, D-145)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &, UWorld *World)
			{
				if (const FWeaponLoadoutEntry *Weapon = ActivePrimaryWeapon(PlayerInventory(World), TEXT("PrintSkillSlots")))
				{
					ShowSkillSlots(*Weapon);
				}
			}));

	FAutoConsoleCommandWithWorldAndArgs GPrintLoadoutCmd(
		TEXT("wor.Loadout.Print"),
		TEXT("DEBUG: wor.Loadout.Print - the player's active loadout: gear, both sockets, the one skill-slot list, spells, items (B-351c)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &, UWorld *World)
			{
				// Review fix: the same character the *Skill commands edit (the pawn, else the first with an inventory).
				const UInventoryComponent *Inventory = PlayerInventory(World);
				ULoadoutComponent *Loadout = Inventory && Inventory->GetOwner() ? Inventory->GetOwner()->FindComponentByClass<ULoadoutComponent>() : nullptr;
				if (!Loadout)
				{
					UE_LOG(LogWoR, Warning, TEXT("[LoadoutDebug] Print: no character with a loadout"));
					return;
				}
				UInventoryDebug::LogActiveLoadout(Loadout);
			}));

	FAutoConsoleCommandWithWorldAndArgs GReplaceSkillCmd(
		TEXT("wor.Loadout.ReplaceSkill"),
		TEXT("DEBUG: wor.Loadout.ReplaceSkill <slot> <ability name> - put a learned ability (or one of the weapon's built-ins) in that slot of the player's primary weapon; the list goes live (B-351, D-145)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				if (Args.Num() < 2 || !Args[0].IsNumeric())
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[ReplaceSkill] usage: wor.Loadout.ReplaceSkill <slot> <ability name>"));
					return;
				}
				UInventoryComponent *Inventory = PlayerInventory(World);
				FWeaponLoadoutEntry *Weapon = ActivePrimaryWeapon(Inventory, TEXT("ReplaceSkill"));
				if (!Weapon)
				{
					return;
				}
				const int32 Slot = FCString::Atoi(*Args[0]);
				TArray<FString> NameParts(Args);
				NameParts.RemoveAt(0);
				const FString Name = FString::Join(NameParts, TEXT(" "));
				UAbilityData *Ability = FindAbilityByName(*Inventory, *Weapon, Name);
				if (!Ability)
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[ReplaceSkill] no learned or built-in ability named '%s'"), *Name);
					return;
				}
				if (!Weapon->ReplaceSkill(Slot, Ability))
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[ReplaceSkill] slot %d is not one of this weapon's %d"), Slot, Weapon->GetSkillSlotCount());
					return;
				}
				Inventory->WriteLoadoutToOwned(Inventory->ActiveLoadoutIndex);
				ShowSkillSlots(*Weapon);
			}));

	FAutoConsoleCommandWithWorldAndArgs GRestoreSkillCmd(
		TEXT("wor.Loadout.RestoreSkill"),
		TEXT("DEBUG: wor.Loadout.RestoreSkill <slot> - bring back what that slot of the player's primary weapon held before the pick (B-351, D-145)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				if (Args.Num() < 1 || !Args[0].IsNumeric())
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[RestoreSkill] usage: wor.Loadout.RestoreSkill <slot>"));
					return;
				}
				UInventoryComponent *Inventory = PlayerInventory(World);
				FWeaponLoadoutEntry *Weapon = ActivePrimaryWeapon(Inventory, TEXT("RestoreSkill"));
				if (!Weapon)
				{
					return;
				}
				const int32 Slot = FCString::Atoi(*Args[0]);
				if (!Weapon->RestoreSkill(Slot))
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[RestoreSkill] slot %d holds no pick"), Slot);
					return;
				}
				Inventory->WriteLoadoutToOwned(Inventory->ActiveLoadoutIndex);
				ShowSkillSlots(*Weapon);
			}));

	// ==================== B-351d (D-145): the primary weapon crystal's one spell list ====================

	void ShowSpellSlots(const FWeaponLoadoutEntry &Weapon)
	{
		const FString Line = Weapon.GetSpellSlotString();
		UE_LOG(LogWoRLoadout, Display, TEXT("[SpellSlots] %s"), *Line);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, LOADOUT_RULES_SCREEN_SECONDS, FColor::Cyan, Line);
		}
	}

	/** A loaded spell asset by its display name or asset name. TODO: the gear screen offers the learned spells only. */
	USpellData *FindSpellByName(const FString &Name)
	{
		for (TObjectIterator<USpellData> It; It; ++It)
		{
			if (!It->IsTemplate() && (It->Name == Name || It->GetName() == Name))
			{
				return *It;
			}
		}
		return nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs GPrintSpellSlotsCmd(
		TEXT("wor.Loadout.PrintSpellSlots"),
		TEXT("DEBUG: wor.Loadout.PrintSpellSlots - the player's primary weapon crystal's one spell list (B-351d, D-145)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &, UWorld *World)
			{
				if (const FWeaponLoadoutEntry *Weapon = ActivePrimaryWeapon(PlayerInventory(World), TEXT("PrintSpellSlots")))
				{
					ShowSpellSlots(*Weapon);
				}
			}));

	FAutoConsoleCommandWithWorldAndArgs GReplaceSpellCmd(
		TEXT("wor.Loadout.ReplaceSpell"),
		TEXT("DEBUG: wor.Loadout.ReplaceSpell <slot> <spell name> - put a spell in that slot of the player's primary weapon crystal; the list goes live (B-351d, D-145)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				if (Args.Num() < 2 || !Args[0].IsNumeric())
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[ReplaceSpell] usage: wor.Loadout.ReplaceSpell <slot> <spell name>"));
					return;
				}
				UInventoryComponent *Inventory = PlayerInventory(World);
				FWeaponLoadoutEntry *Weapon = ActivePrimaryWeapon(Inventory, TEXT("ReplaceSpell"));
				if (!Weapon)
				{
					return;
				}
				const int32 Slot = FCString::Atoi(*Args[0]);
				TArray<FString> NameParts(Args);
				NameParts.RemoveAt(0);
				const FString Name = FString::Join(NameParts, TEXT(" "));
				USpellData *Spell = FindSpellByName(Name);
				if (!Spell)
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[ReplaceSpell] no loaded spell named '%s'"), *Name);
					return;
				}
				if (!Weapon->ReplaceSpell(Slot, Spell))
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[ReplaceSpell] slot %d is not one of this crystal's %d (or it gives no spells)"), Slot, Weapon->GetSpellSlotCap());
					return;
				}
				Inventory->WriteLoadoutToOwned(Inventory->ActiveLoadoutIndex);
				ShowSpellSlots(*Weapon);
			}));

	FAutoConsoleCommandWithWorldAndArgs GRestoreSpellCmd(
		TEXT("wor.Loadout.RestoreSpell"),
		TEXT("DEBUG: wor.Loadout.RestoreSpell <slot> - bring back what that slot of the player's primary weapon crystal held before the pick (B-351d, D-145)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				if (Args.Num() < 1 || !Args[0].IsNumeric())
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[RestoreSpell] usage: wor.Loadout.RestoreSpell <slot>"));
					return;
				}
				UInventoryComponent *Inventory = PlayerInventory(World);
				FWeaponLoadoutEntry *Weapon = ActivePrimaryWeapon(Inventory, TEXT("RestoreSpell"));
				if (!Weapon)
				{
					return;
				}
				const int32 Slot = FCString::Atoi(*Args[0]);
				if (!Weapon->RestoreSpell(Slot))
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[RestoreSpell] slot %d holds no pick"), Slot);
					return;
				}
				Inventory->WriteLoadoutToOwned(Inventory->ActiveLoadoutIndex);
				ShowSpellSlots(*Weapon);
			}));
}

FString ULoadoutRulesDebug::GetCompareSlotsString(const ULoadoutComponent *Loadout)
{
	const AActor *Owner = Loadout ? Loadout->GetOwner() : nullptr;
	UInventoryComponent *Inventory = Owner ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Inventory || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
	{
		return FString::Printf(TEXT("%s: no active loadout"), *GetNameSafe(Owner));
	}
	const FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	const UWeaponData *OldWeapon = Loadout->GetActiveWeapon();
	const URingData *OldAdapter = Loadout->GetCharacterClass() == ECharacterClass::Resonator ? Loadout->GetActiveRing()
																							 : Loadout->GetPrimaryRing();
	const UWeaponData *NewWeapon = Active.Slots.IsValidIndex(Active.ActiveWeaponSlot) ? Active.Slots[Active.ActiveWeaponSlot].Gear.GetWeapon() : nullptr;
	const URingData *NewAdapter = Active.Slots.IsValidIndex(Active.ActiveAdapterSlot) ? Active.Slots[Active.ActiveAdapterSlot].Gear.GetAdapter() : nullptr;
	const bool bMatch = OldWeapon == NewWeapon && OldAdapter == NewAdapter;
	return FString::Printf(TEXT("%s slots %d: weapon %s = %s, adapter %s = %s | %s"), *GetNameSafe(Owner), Active.Slots.Num(),
						   *GetNameSafe(OldWeapon), *GetNameSafe(NewWeapon), *GetNameSafe(OldAdapter), *GetNameSafe(NewAdapter),
						   bMatch ? TEXT("match") : TEXT("DIFF"));
}

void ULoadoutRulesDebug::CompareLoadoutRules(const UObject *WorldContextObject)
{
	UWorld *World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const ULoadoutComponent *Loadout = It->FindComponentByClass<ULoadoutComponent>();
		UInventoryComponent *Inventory = It->FindComponentByClass<UInventoryComponent>();
		if (!Loadout || !Inventory)
		{
			continue;
		}
		const TArray<FString> Findings = Loadout->GetValidationErrors(Inventory->ActiveLoadoutIndex, Inventory);
		const FString Line = FString::Printf(TEXT("%s loadout %d: %d finding(s): %s"), *GetNameSafe(*It), Inventory->ActiveLoadoutIndex,
											 Findings.Num(), Findings.Num() > 0 ? *FString::Join(Findings, TEXT("; ")) : TEXT("none"));
		UE_LOG(LogWoRLoadout, Display, TEXT("[LoadoutRules] %s"), *Line);
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, LOADOUT_RULES_SCREEN_SECONDS, FColor::Cyan, Line);
	}
}

void ULoadoutRulesDebug::CompareLoadoutSlots(const UObject *WorldContextObject)
{
	UWorld *World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (const ULoadoutComponent *Loadout = It->FindComponentByClass<ULoadoutComponent>())
		{
			const FString Line = GetCompareSlotsString(Loadout);
			UE_LOG(LogWoRLoadout, Display, TEXT("[LoadoutSlots] %s"), *Line);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(INDEX_NONE, LOADOUT_RULES_SCREEN_SECONDS, FColor::Cyan, Line);
			}
		}
	}
}

FString ULoadoutRulesDebug::TryAbilityPick(ULoadoutComponent *Loadout, const FString &AbilityName)
{
	AActor *Owner = Loadout ? Loadout->GetOwner() : nullptr;
	UInventoryComponent *Inventory = Owner ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Inventory)
	{
		return FString::Printf(TEXT("%s: no inventory"), *GetNameSafe(Owner));
	}
	const FAbilityInstance *Learned = Inventory->Abilities.LearnedAbilities.FindByPredicate(
		[&AbilityName](const FAbilityInstance &Instance) { return Instance.Ability && Instance.Ability->Name == AbilityName; });
	if (!Learned)
	{
		return FString::Printf(TEXT("%s: hasn't learned '%s'"), *GetNameSafe(Owner), *AbilityName);
	}
	FText Refusal;
	const bool bAssigned = Loadout->TryAssignWeaponAbility(Inventory->ActiveLoadoutIndex, Learned->Ability, Refusal);
	return bAssigned
		? FString::Printf(TEXT("%s: '%s' assigned"), *GetNameSafe(Owner), *AbilityName)
		: FString::Printf(TEXT("%s: '%s' refused - %s"), *GetNameSafe(Owner), *AbilityName, *Refusal.ToString());
}
