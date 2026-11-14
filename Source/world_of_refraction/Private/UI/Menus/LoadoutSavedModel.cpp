// LoadoutSavedModel.cpp

#include "UI/Menus/LoadoutSavedModel.h"

#include "Algo/Count.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Pool/PoolSubsystem.h"
#include "UI/Menus/LoadoutScreenModel.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRLoadoutSaved"

namespace LoadoutSavedBuild
{
	/** "Loadout 1" is the first name tried. */
	constexpr int32 FIRST_NEW_NAME_NUMBER = 1;

	int32 CountCustom(const UInventoryComponent &Inventory)
	{
		return Algo::CountIf(Inventory.SavedLoadouts, [](const FCombatLoadout &Saved) { return !Saved.bLore; });
	}

	bool HasRoom(const UInventoryComponent &Inventory)
	{
		return CountCustom(Inventory) < PoolConstants::MAX_CUSTOM_LOADOUTS;
	}

	FText NoRoom()
	{
		return FText::Format(LOCTEXT("NoRoom", "You keep at most {0} loadouts of your own: rename or change one."),
							 FText::AsNumber(PoolConstants::MAX_CUSTOM_LOADOUTS));
	}

	bool NameInUse(const UInventoryComponent &Inventory, const FString &Name)
	{
		return Inventory.SavedLoadouts.ContainsByPredicate([&Name](const FCombatLoadout &Saved) { return Saved.LoadoutName.Equals(Name, ESearchCase::IgnoreCase); });
	}
}

FLoadoutSavedList WoRLoadoutSaved::BuildList(const UInventoryComponent &Inventory)
{
	using namespace LoadoutSavedBuild;

	FLoadoutSavedList List;
	List.CustomCount = CountCustom(Inventory);
	List.MaxCustom = PoolConstants::MAX_CUSTOM_LOADOUTS;
	for (int32 Index = 0; Index < Inventory.SavedLoadouts.Num(); ++Index)
	{
		const FCombatLoadout &Saved = Inventory.SavedLoadouts[Index];
		FLoadoutSavedRow &Row = List.Rows.AddDefaulted_GetRef();
		Row.LoadoutIndex = Index;
		Row.Name = FText::FromString(Saved.LoadoutName);
		Row.bActive = Index == Inventory.ActiveLoadoutIndex;
		Row.bLore = Saved.bLore;
		Row.Note = Row.bActive ? LOCTEXT("ActiveNote", "Active")
				   : Row.bLore ? LOCTEXT("LoreNote", "The character's own: duplicate it to change it")
							   : FText::GetEmpty();
	}
	if (HasRoom(Inventory))
	{
		FLoadoutSavedRow &New = List.Rows.AddDefaulted_GetRef();
		New.Name = LOCTEXT("NewLine", "New loadout");
		New.Note = FText::Format(LOCTEXT("NewLineNote", "{0} of {1} of your own"), FText::AsNumber(List.CustomCount), FText::AsNumber(List.MaxCustom));
	}
	return List;
}

FText WoRLoadoutSaved::SetActive(ULoadoutComponent &Loadout, UInventoryComponent &Inventory, int32 LoadoutIndex)
{
	if (!Inventory.SavedLoadouts.IsValidIndex(LoadoutIndex))
	{
		return LOCTEXT("NoSuchLoadout", "That loadout is gone.");
	}
	if (LoadoutIndex == Inventory.ActiveLoadoutIndex)
	{
		return LOCTEXT("AlreadyActive", "It's the one in use already.");
	}
	if (!Loadout.SetActiveLoadoutIndex(LoadoutIndex))
	{
		return LOCTEXT("NotSwitched", "It can't be used now.");
	}
	UE_LOG(LogWoRUI, Log, TEXT("[LoadoutSaved] %s now uses '%s' (%d of %d)"), *GetNameSafe(Inventory.GetOwner()),
		   *Inventory.SavedLoadouts[LoadoutIndex].LoadoutName, LoadoutIndex + 1, Inventory.SavedLoadouts.Num());
	return FText::GetEmpty();
}

FText WoRLoadoutSaved::CreateNew(ULoadoutComponent &Loadout, UInventoryComponent &Inventory, int32 &OutIndex)
{
	OutIndex = INDEX_NONE;
	if (!LoadoutSavedBuild::HasRoom(Inventory))
	{
		return LoadoutSavedBuild::NoRoom();
	}
	OutIndex = Loadout.CreateNewLoadout(MakeNewName(Inventory));
	return OutIndex == INDEX_NONE ? LoadoutSavedBuild::NoRoom() : FText::GetEmpty();
}

FText WoRLoadoutSaved::Duplicate(ULoadoutComponent &Loadout, UInventoryComponent &Inventory, int32 SourceIndex, int32 &OutIndex)
{
	OutIndex = INDEX_NONE;
	if (!Inventory.SavedLoadouts.IsValidIndex(SourceIndex))
	{
		return LOCTEXT("NoSuchLoadout", "That loadout is gone.");
	}
	if (!LoadoutSavedBuild::HasRoom(Inventory))
	{
		return LoadoutSavedBuild::NoRoom();
	}
	const FString CopyName = FText::Format(LOCTEXT("CopyName", "{0} copy"), FText::FromString(Inventory.SavedLoadouts[SourceIndex].LoadoutName))
								 .ToString()
								 .Left(MAX_LOADOUT_NAME_LENGTH);
	OutIndex = Loadout.DuplicateLoadout(SourceIndex, CopyName);
	return OutIndex == INDEX_NONE ? LoadoutSavedBuild::NoRoom() : FText::GetEmpty();
}

FText WoRLoadoutSaved::Rename(ULoadoutComponent &Loadout, UInventoryComponent &Inventory, int32 LoadoutIndex, const FText &NewName)
{
	if (!Inventory.SavedLoadouts.IsValidIndex(LoadoutIndex))
	{
		return LOCTEXT("NoSuchLoadout", "That loadout is gone.");
	}
	if (Inventory.SavedLoadouts[LoadoutIndex].bLore)
	{
		return LOCTEXT("LoreName", "The character's own loadout keeps its name: duplicate it to name a copy.");
	}
	const FString Name = NewName.ToString().TrimStartAndEnd();
	if (Name.IsEmpty())
	{
		return LOCTEXT("EmptyName", "A loadout needs a name.");
	}
	if (Name.Len() > MAX_LOADOUT_NAME_LENGTH)
	{
		return FText::Format(LOCTEXT("LongName", "At most {0} letters."), FText::AsNumber(MAX_LOADOUT_NAME_LENGTH));
	}
	if (!Loadout.RenameLoadout(LoadoutIndex, Name))
	{
		return LOCTEXT("NotRenamed", "It can't be renamed now.");
	}
	return FText::GetEmpty();
}

FString WoRLoadoutSaved::MakeNewName(const UInventoryComponent &Inventory)
{
	for (int32 Number = LoadoutSavedBuild::FIRST_NEW_NAME_NUMBER;; ++Number)
	{
		const FString Name = FText::Format(LOCTEXT("NewName", "Loadout {0}"), FText::AsNumber(Number)).ToString();
		if (!LoadoutSavedBuild::NameInUse(Inventory, Name))
		{
			return Name; // ends: there are finitely many saved loadouts
		}
	}
}

bool WoRLoadoutSaved::FindComponents(UWorld *World, const FLoadoutScreenMember &Member, ULoadoutComponent *&OutLoadout, UInventoryComponent *&OutInventory)
{
	OutInventory = nullptr;
	OutLoadout = WoRLoadoutScreen::FindMemberLoadout(World, Member, OutInventory);
	return OutLoadout && OutInventory;
}

FString WoRLoadoutSaved::GetSavedListString(const UInventoryComponent &Inventory)
{
	const FLoadoutSavedList List = BuildList(Inventory);
	FString Out = FString::Printf(TEXT("%s saved loadouts: %d own of %d, %d in all"), *GetNameSafe(Inventory.GetOwner()), List.CustomCount,
								  List.MaxCustom, Inventory.SavedLoadouts.Num());
	for (const FLoadoutSavedRow &Row : List.Rows)
	{
		Out += FString::Printf(TEXT("\n  [%s] %s%s"), Row.IsNewLine() ? TEXT("+") : *FString::FromInt(Row.LoadoutIndex), *Row.Name.ToString(),
							   Row.Note.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Row.Note.ToString()));
	}
	return Out;
}

#undef LOCTEXT_NAMESPACE
