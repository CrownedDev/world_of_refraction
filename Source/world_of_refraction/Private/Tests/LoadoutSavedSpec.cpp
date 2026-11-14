// LoadoutSavedSpec.cpp
// B-440e (D-111): the loadout screen's saved loadouts. The list (active and lore marked, a New line while there is
// room), using one, making a new one, duplicating (a lore one gives a copy of the player's own) and renaming (a lore
// one keeps its name; empty and too-long names refused). The screen's Y list, headless, with no party.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Pool/PoolSubsystem.h"
#include "UI/Menus/LoadoutSavedModel.h"
#include "UI/Menus/LoadoutScreenWidget.h"

namespace
{
	constexpr int32 SAVED_TEST_FIRST = 0;
	constexpr int32 SAVED_TEST_SECOND = 1;
	constexpr int32 SAVED_TEST_LORE = 2;

	FCombatLoadout SavedTestLoadout(const TCHAR *Name, bool bLore)
	{
		FCombatLoadout Loadout;
		Loadout.LoadoutName = Name;
		Loadout.bLore = bLore;
		return Loadout;
	}
}

BEGIN_DEFINE_SPEC(FLoadoutSavedSpec, "WoR.UI.LoadoutSaved", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UInventoryComponent *Inventory = nullptr;
	ULoadoutComponent *Loadout = nullptr;
END_DEFINE_SPEC(FLoadoutSavedSpec)

void FLoadoutSavedSpec::Define()
{
	Describe("the model", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			if (!TestTrue(TEXT("world"), TestWorld->Create()))
			{
				return;
			}
			AActor *Owner = TestWorld->GetWorld()->SpawnActor<AActor>();
			if (!TestNotNull(TEXT("owner"), Owner))
			{
				return;
			}
			Inventory = NewObject<UInventoryComponent>(Owner);
			Owner->AddInstanceComponent(Inventory);
			Inventory->RegisterComponent();
			Loadout = NewObject<ULoadoutComponent>(Owner);
			Owner->AddInstanceComponent(Loadout);
			Loadout->RegisterComponent();
			Inventory->SavedLoadouts = {SavedTestLoadout(TEXT("Alpha"), false), SavedTestLoadout(TEXT("Beta"), false),
										SavedTestLoadout(TEXT("Lore"), true)};
			Inventory->ActiveLoadoutIndex = SAVED_TEST_FIRST;
		});
		AfterEach([this]()
		{
			Inventory = nullptr;
			Loadout = nullptr;
			TestWorld.Reset();
		});

		It("lists each loadout, marks the active and the lore one, then a New line", [this]()
		{
			if (!Inventory)
			{
				return;
			}
			const FLoadoutSavedList List = WoRLoadoutSaved::BuildList(*Inventory);
			if (!TestEqual(TEXT("three and New"), List.Rows.Num(), Inventory->SavedLoadouts.Num() + 1))
			{
				return;
			}
			TestTrue(TEXT("the active one"), List.Rows[SAVED_TEST_FIRST].bActive);
			TestFalse(TEXT("only it"), List.Rows[SAVED_TEST_SECOND].bActive);
			TestTrue(TEXT("the lore one"), List.Rows[SAVED_TEST_LORE].bLore);
			TestTrue(TEXT("New last"), List.Rows.Last().IsNewLine());
			TestEqual(TEXT("two of the player's own"), List.CustomCount, 2);
			TestEqual(TEXT("of five"), List.MaxCustom, PoolConstants::MAX_CUSTOM_LOADOUTS);
		});

		It("uses another loadout, and says so when it is the one in use", [this]()
		{
			if (!Inventory)
			{
				return;
			}
			TestTrue(TEXT("used"), WoRLoadoutSaved::SetActive(*Loadout, *Inventory, SAVED_TEST_SECOND).IsEmpty());
			TestEqual(TEXT("now the second"), Inventory->ActiveLoadoutIndex, SAVED_TEST_SECOND);
			TestFalse(TEXT("again: refused with a reason"), WoRLoadoutSaved::SetActive(*Loadout, *Inventory, SAVED_TEST_SECOND).IsEmpty());
			TestFalse(TEXT("a gone one: refused"), WoRLoadoutSaved::SetActive(*Loadout, *Inventory, Inventory->SavedLoadouts.Num()).IsEmpty());
		});

		It("makes new ones named Loadout N until the player has five of their own", [this]()
		{
			if (!Inventory)
			{
				return;
			}
			int32 NewIndex = INDEX_NONE;
			const FString FirstFreeName = WoRLoadoutSaved::MakeNewName(*Inventory);
			TestTrue(TEXT("made"), WoRLoadoutSaved::CreateNew(*Loadout, *Inventory, NewIndex).IsEmpty());
			if (!TestTrue(TEXT("at the end"), Inventory->SavedLoadouts.IsValidIndex(NewIndex)))
			{
				return;
			}
			TestEqual(TEXT("the first free name"), Inventory->SavedLoadouts[NewIndex].LoadoutName, FirstFreeName);
			TestNotEqual(TEXT("the next name is another"), WoRLoadoutSaved::MakeNewName(*Inventory), Inventory->SavedLoadouts[NewIndex].LoadoutName);
			while (WoRLoadoutSaved::BuildList(*Inventory).CustomCount < PoolConstants::MAX_CUSTOM_LOADOUTS)
			{
				if (!TestTrue(TEXT("room for another"), WoRLoadoutSaved::CreateNew(*Loadout, *Inventory, NewIndex).IsEmpty()))
				{
					return;
				}
			}
			TestFalse(TEXT("full: no New line"), WoRLoadoutSaved::BuildList(*Inventory).Rows.Last().IsNewLine());
			TestFalse(TEXT("full: refused"), WoRLoadoutSaved::CreateNew(*Loadout, *Inventory, NewIndex).IsEmpty());
			TestEqual(TEXT("nothing made"), NewIndex, INDEX_NONE);
		});

		It("duplicates a lore loadout into one of the player's own", [this]()
		{
			if (!Inventory)
			{
				return;
			}
			int32 NewIndex = INDEX_NONE;
			TestTrue(TEXT("copied"), WoRLoadoutSaved::Duplicate(*Loadout, *Inventory, SAVED_TEST_LORE, NewIndex).IsEmpty());
			if (TestTrue(TEXT("a new one"), Inventory->SavedLoadouts.IsValidIndex(NewIndex)))
			{
				TestFalse(TEXT("the player's own"), Inventory->SavedLoadouts[NewIndex].bLore);
				TestTrue(TEXT("named after it"), Inventory->SavedLoadouts[NewIndex].LoadoutName.StartsWith(TEXT("Lore")));
			}
		});

		It("renames the player's own, never a lore one; trims; refuses empty and too long", [this]()
		{
			if (!Inventory)
			{
				return;
			}
			TestTrue(TEXT("renamed"), WoRLoadoutSaved::Rename(*Loadout, *Inventory, SAVED_TEST_FIRST, FText::FromString(TEXT("  Bruiser  "))).IsEmpty());
			TestEqual(TEXT("trimmed"), Inventory->SavedLoadouts[SAVED_TEST_FIRST].LoadoutName, FString(TEXT("Bruiser")));
			TestFalse(TEXT("lore: refused"), WoRLoadoutSaved::Rename(*Loadout, *Inventory, SAVED_TEST_LORE, FText::FromString(TEXT("Mine"))).IsEmpty());
			TestEqual(TEXT("lore keeps its name"), Inventory->SavedLoadouts[SAVED_TEST_LORE].LoadoutName, FString(TEXT("Lore")));
			TestFalse(TEXT("empty: refused"), WoRLoadoutSaved::Rename(*Loadout, *Inventory, SAVED_TEST_SECOND, FText::FromString(TEXT("   "))).IsEmpty());
			const FString TooLong = FString::ChrN(WoRLoadoutSaved::MAX_LOADOUT_NAME_LENGTH + 1, TEXT('a'));
			TestFalse(TEXT("too long: refused"), WoRLoadoutSaved::Rename(*Loadout, *Inventory, SAVED_TEST_SECOND, FText::FromString(TooLong)).IsEmpty());
			TestEqual(TEXT("kept"), Inventory->SavedLoadouts[SAVED_TEST_SECOND].LoadoutName, FString(TEXT("Beta")));
			TestFalse(TEXT("debug string"), WoRLoadoutSaved::GetSavedListString(*Inventory).IsEmpty());
		});
	});

	It("on the screen with no party, Y opens nothing and Back still closes (headless)", [this]()
	{
		FWoRTestWorld World;
		if (!TestTrue(TEXT("world"), World.Create()))
		{
			return;
		}
		ULoadoutScreenWidget *Screen = CreateWidget<ULoadoutScreenWidget>(World.GetWorld(), ULoadoutScreenWidget::StaticClass());
		if (!TestNotNull(TEXT("screen"), Screen))
		{
			return;
		}
		Screen->ActivateWidget();
		Screen->PressSavedLoadouts();
		TestFalse(TEXT("no member: no list"), Screen->IsSavedList());
		Screen->CommitRename(FText::FromString(TEXT("Nothing")));
		TestFalse(TEXT("nothing to rename"), Screen->IsRenaming());
		Screen->PressBack();
		TestFalse(TEXT("Back closes it"), Screen->IsActivated());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
