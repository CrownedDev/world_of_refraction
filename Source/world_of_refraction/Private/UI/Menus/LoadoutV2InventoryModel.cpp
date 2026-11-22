// LoadoutV2InventoryModel.cpp

#include "UI/Menus/LoadoutV2InventoryModel.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Crystals/StatStone.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/FCombatLoadout.h"
#include "UI/Menus/LoadoutV2Model.h"
#include "UI/WoRIconTable.h"
#include "WoRGameNames.h"

#define LOCTEXT_NAMESPACE "WoRLoadoutV2Inventory"

namespace LoadoutV2Inventory
{
	bool IsWorn(const FCombatLoadout &Worn, const FGuid &ID)
	{
		FCombatLoadout Probe = Worn;
		return UInventoryComponent::StripFromLoadout(Probe, ID);
	}

	/** What a piece is apart from its asset: tier, quality and its rolled numbers (a roll makes it its own). */
	template <typename TEntry>
	FString PieceKey(const UObject *Asset, const TEntry &Entry)
	{
		FString Rolls;
		FEquipmentStatBonus::StaticStruct()->ExportText(Rolls, &Entry.StatBonus, nullptr, nullptr, PPF_None, nullptr);
		FResistanceBonus::StaticStruct()->ExportText(Rolls, &Entry.ResistanceBonus, nullptr, nullptr, PPF_None, nullptr);
		return FString::Printf(TEXT("%s|%d|%d|%d/%d|%d/%d|%s"), *GetPathNameSafe(Asset), static_cast<int32>(Entry.Tier), static_cast<int32>(Entry.Quality),
							   Entry.StatPool, Entry.StatMaxPool, Entry.ResistancePool, Entry.ResistanceMaxPool, *Rolls);
	}

	template <typename TEntry>
	bool IsBuilt(const TEntry &Entry)
	{
		return !Entry.AttachedItem.IsEmpty() || !Entry.StoneSocket.IsEmpty() || Entry.AssignedSpells.Num() > 0 || Entry.SpellPicks.Num() > 0;
	}

	void AddHeader(FLoadoutV2InventoryPage &Page, const FText &Name)
	{
		FLoadoutV2InventoryRow &Header = Page.Rows.AddDefaulted_GetRef();
		Header.Kind = ELoadoutV2InventoryRowKind::Header;
		Header.Name = Name;
	}

	/** Each stack of Rows (one line per piece) as one line, its first piece's, under Header; nothing when empty. */
	void AddGroup(FLoadoutV2InventoryPage &Page, const FText &Header, const TArray<FLoadoutV2InventoryRow> &Rows, const TArray<FString> &Keys)
	{
		if (Rows.IsEmpty())
		{
			return;
		}
		AddHeader(Page, Header);
		for (const FLoadoutV2Stack &Stack : WoRLoadoutV2::GroupStacks(Keys))
		{
			FLoadoutV2InventoryRow &Row = Page.Rows.Add_GetRef(Rows[Stack.First]);
			Row.Count = Stack.Count;
			Row.Name = WoRLoadoutV2::GetStackedName(Row.Name, Stack.Count);
		}
	}
}

FString WoRLoadoutV2::GetStackKey(const FWeaponInventoryEntry &Weapon)
{
	using namespace LoadoutV2Inventory;
	return !Weapon.Weapon || IsBuilt(Weapon) || Weapon.SkillPicks.Num() > 0 ? FString() : FString(TEXT("W|")) + PieceKey(Weapon.Weapon, Weapon);
}

FString WoRLoadoutV2::GetStackKey(const FRingInventoryEntry &Adapter)
{
	using namespace LoadoutV2Inventory;
	return !Adapter.Ring || IsBuilt(Adapter) ? FString() : FString(TEXT("A|")) + PieceKey(Adapter.Ring, Adapter);
}

TArray<FLoadoutV2Stack> WoRLoadoutV2::GroupStacks(const TArray<FString> &Keys)
{
	TArray<FLoadoutV2Stack> Stacks;
	TMap<FString, int32> StackOfKey;
	for (int32 Index = 0; Index < Keys.Num(); ++Index)
	{
		if (const int32 *Found = Keys[Index].IsEmpty() ? nullptr : StackOfKey.Find(Keys[Index]))
		{
			++Stacks[*Found].Count;
			continue;
		}
		if (!Keys[Index].IsEmpty())
		{
			StackOfKey.Add(Keys[Index], Stacks.Num());
		}
		Stacks.Add({Index, 1});
	}
	return Stacks;
}

FText WoRLoadoutV2::GetStackedName(const FText &Name, int32 Count)
{
	return Count > 1 ? FText::Format(LOCTEXT("StackedName", "{0} x{1}"), Name, FText::AsNumber(Count)) : Name;
}

FLoadoutV2InventoryPage WoRLoadoutV2::BuildInventoryPage(const FCombatLoadout &Worn, const TArray<FWeaponInventoryEntry> &Weapons,
														 const TArray<FRingInventoryEntry> &Adapters, const TArray<FEvolutionInventoryEntry> &Evolutions,
														 const TSet<FGuid> &EvolutionsInUse, const TMap<FCrystalId, int32> &Pouch)
{
	using namespace LoadoutV2Inventory;
	FLoadoutV2InventoryPage Page;

	TArray<FLoadoutV2InventoryRow> Rows;
	TArray<FString> Keys;
	for (const FWeaponInventoryEntry &Owned : Weapons)
	{
		if (!Owned.Weapon || IsWorn(Worn, Owned.PersistentID))
		{
			continue;
		}
		FLoadoutV2InventoryRow &Row = Rows.AddDefaulted_GetRef();
		Row.Kind = ELoadoutV2InventoryRowKind::Weapon;
		Row.Name = FText::FromString(Owned.Weapon->GetDisplayName());
		Row.Icon.Kind = EWoRIconKind::WeaponType;
		Row.Icon.WeaponType = Owned.Weapon->WeaponType;
		Row.Icon.Name = WoRGameNames::GetWeaponTypeName(Owned.Weapon->WeaponType);
		Row.bHasTier = true;
		Row.Tier = Owned.Tier;
		Row.Weapon = Owned;
		Keys.Add(GetStackKey(Owned));
	}
	AddGroup(Page, LOCTEXT("GroupWeapons", "Weapons"), Rows, Keys);

	Rows.Reset();
	Keys.Reset();
	for (const FRingInventoryEntry &Owned : Adapters)
	{
		if (!Owned.Ring || IsWorn(Worn, Owned.PersistentID))
		{
			continue;
		}
		FLoadoutV2InventoryRow &Row = Rows.AddDefaulted_GetRef();
		Row.Kind = ELoadoutV2InventoryRowKind::Adapter;
		Row.Name = WoRGameNames::GetAdapterName(Owned.Ring, Owned.AttachedItem);
		Row.Icon = MakeAdapterIcon(Owned);
		Row.bHasTier = true;
		Row.Tier = Owned.Tier;
		Row.Adapter = Owned;
		Keys.Add(GetStackKey(Owned));
	}
	AddGroup(Page, WoRGameNames::GetAdapterWordPlural(), Rows, Keys);

	Rows.Reset();
	Keys.Reset();
	for (const FEvolutionInventoryEntry &Owned : Evolutions)
	{
		if (!Owned.Item || !Owned.InstanceID.IsValid() || EvolutionsInUse.Contains(Owned.InstanceID))
		{
			continue;
		}
		FLoadoutV2InventoryRow &Row = Rows.AddDefaulted_GetRef();
		Row.Kind = ELoadoutV2InventoryRowKind::Evolution;
		Row.Name = FText::FromString(Owned.Item->ItemName);
		Row.Icon = WoRIcons::AsLoose(MakeSocketIcon(MakeEvolutionAttachment(Owned))); // B-584: none listed here is slotted
		Row.bHasTier = true;
		Row.Tier = Owned.Tier;
		Row.Evolution = Owned;
		Keys.AddDefaulted(); // each evolution is its own (its roll and wear go with it)
	}
	AddGroup(Page, LOCTEXT("GroupEvolutions", "Evolutions"), Rows, Keys);

	// The pouch is stacks already: one line each, highest tier first, as the socket list sorts them.
	TArray<TPair<FCrystalId, int32>> Stacks;
	for (const TPair<FCrystalId, int32> &Stack : Pouch)
	{
		if (Stack.Value > 0 && Stack.Key.Type != ECrystalType::None && !CrystalTypeHelpers::IsPotionType(Stack.Key.Type)) // a potion fits no socket
		{
			Stacks.Add(Stack);
		}
	}
	Stacks.Sort([](const TPair<FCrystalId, int32> &A, const TPair<FCrystalId, int32> &B)
				{ return A.Key.Tier != B.Key.Tier ? A.Key.Tier > B.Key.Tier : A.Key.Type < B.Key.Type; });
	if (!Stacks.IsEmpty())
	{
		AddHeader(Page, LOCTEXT("GroupCrystals", "Crystals"));
	}
	for (const TPair<FCrystalId, int32> &Stack : Stacks)
	{
		const bool bStone = !CrystalTypeHelpers::IsGemType(Stack.Key.Type);
		FLoadoutV2InventoryRow &Row = Page.Rows.AddDefaulted_GetRef();
		Row.Kind = ELoadoutV2InventoryRowKind::Crystal;
		Row.Crystal.Kind = bStone ? EAttachedItemKind::AugmentStone : EAttachedItemKind::Crystal;
		Row.Crystal.Crystal.Id = StatStone::Normalize(Stack.Key); // as a socket holds it
		Row.Crystal.Crystal.CurrentDurability = bStone ? 0 : ItemIdentity::GetMaxDurability(Stack.Key);
		Row.Count = Stack.Value;
		Row.Name = GetStackedName(WoRGameNames::GetCrystalTypeName(StatStone::AsRowId(Row.Crystal.Crystal.Id).Type), Stack.Value);
		Row.Icon = WoRIcons::AsLoose(MakeSocketIcon(Row.Crystal)); // B-584: the pouch is loose
		Row.bHasTier = GetAttachmentTier(Row.Crystal, Row.Tier);
	}
	return Page;
}

FString WoRLoadoutV2::GetInventoryPageString(const FLoadoutV2InventoryPage &Page)
{
	TArray<FString> Lines;
	for (const FLoadoutV2InventoryRow &Row : Page.Rows)
	{
		Lines.Add(Row.IsPiece() ? FString::Printf(TEXT("  %s%s"), *Row.Name.ToString(), Row.bHasTier ? *FString::Printf(TEXT(" (%s)"), *WoRGameNames::GetTierLetter(Row.Tier).ToString()) : TEXT(""))
								: Row.Name.ToString());
	}
	return Lines.IsEmpty() ? FString(TEXT("(nothing owned that isn't worn)")) : FString::Join(Lines, TEXT("\n"));
}

#undef LOCTEXT_NAMESPACE
