// LoadoutApparelModel.cpp

#include "UI/Menus/LoadoutApparelModel.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/Entries/FApparelEntry.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutComponent.h"
#include "Pool/PoolSubsystem.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRLoadoutApparel"

namespace LoadoutApparelBuild
{
	int32 FindWorn(const FCombatLoadout &Loadout, const FGuid &GearID)
	{
		return GearID.IsValid() ? Loadout.Apparel.IndexOfByPredicate([&GearID](const FApparelEntry &Worn) { return Worn.PersistentID == GearID; })
								: INDEX_NONE;
	}

	UPoolSubsystem *FindPool(UWorld *World)
	{
		UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
	}

	/** The member's loadout component and active loadout; OutRefusal says why not (gone, or a lore loadout). */
	ULoadoutComponent *FindChangeable(UWorld *World, const FLoadoutScreenMember &Member, UInventoryComponent *&OutInventory, FText &OutRefusal)
	{
		ULoadoutComponent *Loadout = WoRLoadoutScreen::FindMemberLoadout(World, Member, OutInventory);
		if (!Loadout || !OutInventory || !OutInventory->SavedLoadouts.IsValidIndex(OutInventory->ActiveLoadoutIndex))
		{
			OutRefusal = FText::Format(LOCTEXT("MemberNotHere", "{0} isn't here, so their apparel can't be changed."), Member.Name);
			return nullptr;
		}
		if (OutInventory->SavedLoadouts[OutInventory->ActiveLoadoutIndex].bLore)
		{
			// TODO: B-380e saves a lore loadout's apparel; until then it would be lost on reload.
			OutRefusal = LOCTEXT("LoreApparel", "The character's own loadout keeps its apparel for now: duplicate it to change it.");
			return nullptr;
		}
		return Loadout;
	}
}

bool WoRLoadoutApparel::IsApparelRow(const FLoadoutScreenRow &Row)
{
	return Row.Kind == ELoadoutScreenRowKind::Apparel || Row.Kind == ELoadoutScreenRowKind::EmptyApparel;
}

TArray<FLoadoutScreenChoice> WoRLoadoutApparel::BuildApparelChoices(const FCombatLoadout &Loadout, const FLoadoutScreenRow &Row,
																	 const TArray<FApparelEntry> &Owned)
{
	TArray<FLoadoutScreenChoice> Choices;
	if (!IsApparelRow(Row))
	{
		return Choices;
	}
	for (const FApparelEntry &Piece : Owned)
	{
		if (!Piece.Apparel || !Piece.PersistentID.IsValid() || Piece.PersistentID == Row.GearID ||
			LoadoutApparelBuild::FindWorn(Loadout, Piece.PersistentID) != INDEX_NONE)
		{
			continue; // the row's own piece, or worn in the other slot
		}
		FLoadoutScreenChoice &Choice = Choices.AddDefaulted_GetRef();
		Choice.Row = WoRLoadoutScreen::BuildApparelRow(Piece);
		if (!Piece.AttachedItem.IsEmpty() && !ApparelRules::CanSocket(Piece.AttachedItem))
		{
			Choice.Refusal = LOCTEXT("RefuseStoneInSocket", "Its socket holds a stone, which apparel can't wear.");
		}
	}
	return Choices;
}

bool WoRLoadoutApparel::PutApparel(FCombatLoadout &Loadout, const FLoadoutScreenRow &Row, const FApparelEntry &Piece)
{
	if (!IsApparelRow(Row) || !Piece.Apparel)
	{
		return false;
	}
	const int32 Worn = LoadoutApparelBuild::FindWorn(Loadout, Row.GearID);
	if (Worn == INDEX_NONE)
	{
		return ApparelRules::AddApparel(Loadout, Piece); // a free slot (MAX_APPAREL_SLOTS, no budget)
	}
	if (LoadoutApparelBuild::FindWorn(Loadout, Piece.PersistentID) != INDEX_NONE ||
		(!Piece.AttachedItem.IsEmpty() && !ApparelRules::CanSocket(Piece.AttachedItem)))
	{
		return false;
	}
	Loadout.Apparel[Worn] = Piece;
	return true;
}

bool WoRLoadoutApparel::TakeOffApparel(FCombatLoadout &Loadout, const FLoadoutScreenRow &Row)
{
	const int32 Worn = Row.Kind == ELoadoutScreenRowKind::Apparel ? LoadoutApparelBuild::FindWorn(Loadout, Row.GearID) : INDEX_NONE;
	if (Worn == INDEX_NONE)
	{
		return false;
	}
	Loadout.Apparel.RemoveAt(Worn);
	return true;
}

TArray<FApparelEntry> WoRLoadoutApparel::GetMemberApparel(UWorld *World, const FLoadoutScreenMember &Member)
{
	TArray<FApparelEntry> Mine;
	if (const UPoolSubsystem *Pool = LoadoutApparelBuild::FindPool(World))
	{
		for (const FApparelEntry &Piece : Pool->GetOwnedApparel())
		{
			if (Pool->GetGearOwner(Piece.PersistentID) == Member.Key)
			{
				Mine.Add(Piece);
			}
		}
	}
	return Mine;
}

TArray<FLoadoutScreenChoice> WoRLoadoutApparel::BuildApparelChoices(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row)
{
	UInventoryComponent *Inventory = nullptr;
	if (!WoRLoadoutScreen::FindMemberLoadout(World, Member, Inventory) || !Inventory || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
	{
		return {};
	}
	return BuildApparelChoices(Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex], Row, GetMemberApparel(World, Member));
}

FText WoRLoadoutApparel::ApplyApparelChoice(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row, const FLoadoutScreenChoice &Choice)
{
	if (!Choice.Refusal.IsEmpty())
	{
		return Choice.Refusal;
	}
	FText Refusal;
	UInventoryComponent *Inventory = nullptr;
	ULoadoutComponent *Loadout = LoadoutApparelBuild::FindChangeable(World, Member, Inventory, Refusal);
	const UPoolSubsystem *Pool = LoadoutApparelBuild::FindPool(World);
	const FApparelEntry *Piece = Pool ? Pool->FindApparelInPool(Choice.Row.GearID) : nullptr;
	if (!Loadout)
	{
		return Refusal;
	}
	if (!Piece)
	{
		return LOCTEXT("PieceGone", "That piece is no longer yours.");
	}
	const FApparelEntry Wearing = *Piece; // a copy: the lambda runs after the store may move
	FCombatLoadout Probe = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	if (!PutApparel(Probe, Row, Wearing))
	{
		return LOCTEXT("RefuseApparelPlace", "It can't be worn here.");
	}
	if (!Loadout->TryChangeGear(Inventory->ActiveLoadoutIndex, [&Row, &Wearing](FCombatLoadout &Changed) { PutApparel(Changed, Row, Wearing); }, Refusal))
	{
		return Refusal;
	}
	Loadout->NotifyApparelChanged(); // B-380c: pools and stat readers re-read it
	UE_LOG(LogWoRUI, Log, TEXT("[LoadoutApparel] %s wears %s"), *Member.Name.ToString(), *GetNameSafe(Wearing.Apparel.Get()));
	return FText::GetEmpty();
}

FText WoRLoadoutApparel::TakeOffRow(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row)
{
	FText Refusal;
	UInventoryComponent *Inventory = nullptr;
	ULoadoutComponent *Loadout = LoadoutApparelBuild::FindChangeable(World, Member, Inventory, Refusal);
	if (!Loadout)
	{
		return Refusal;
	}
	FCombatLoadout Probe = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	if (!TakeOffApparel(Probe, Row))
	{
		return LOCTEXT("NothingWorn", "Nothing is worn here.");
	}
	if (!Loadout->TryChangeGear(Inventory->ActiveLoadoutIndex, [&Row](FCombatLoadout &Changed) { TakeOffApparel(Changed, Row); }, Refusal))
	{
		return Refusal;
	}
	Loadout->NotifyApparelChanged();
	return FText::GetEmpty();
}

FString WoRLoadoutApparel::GetMemberApparelString(UWorld *World, const FLoadoutScreenMember &Member)
{
	FString Out = FString::Printf(TEXT("%s's apparel:"), *Member.Name.ToString());
	UInventoryComponent *Inventory = nullptr;
	WoRLoadoutScreen::FindMemberLoadout(World, Member, Inventory);
	const FCombatLoadout *Active = Inventory && Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex)
									   ? &Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex]
									   : nullptr;
	for (const FApparelEntry &Piece : GetMemberApparel(World, Member))
	{
		const bool bWorn = Active && LoadoutApparelBuild::FindWorn(*Active, Piece.PersistentID) != INDEX_NONE;
		Out += FString::Printf(TEXT("\n  %s %s%s"), *GetNameSafe(Piece.Apparel.Get()), *Piece.PersistentID.ToString(EGuidFormats::Short),
							   bWorn ? TEXT(" (worn)") : TEXT(""));
	}
	if (Active)
	{
		Out += FString::Printf(TEXT("\n  active loadout wears %d of %d%s"), Active->Apparel.Num(), ApparelRules::MAX_APPAREL_SLOTS,
							   Active->bLore ? TEXT(" (lore: not changeable here)") : TEXT(""));
	}
	return Out;
}

#undef LOCTEXT_NAMESPACE
