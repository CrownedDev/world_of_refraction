// JoinMenuSpec.cpp
// B-562 (D-229, stage 5): a co-op guest joins from the game menu with one of its own characters (the title's slot list,
// read from its own saves) or a new one. Unproven here: the screens over a real second client and the host's answer
// (two-player PIE; the PIE sheet B-562).

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/PlayerPicks.h"
#include "Misc/AutomationTest.h"
#include "Save/WoRSaveTypes.h"
#include "UI/Menus/PauseMenuModel.h"
#include "UI/Menus/TitleScreenModel.h"

namespace JoinMenuSpecValues
{
	constexpr int32 MAX_SLOTS = 3;
}

BEGIN_DEFINE_SPEC(FJoinMenuSpec, "WoR.Coop.JoinMenu", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	static FTitleSaveSummary MakeSlot(int32 Slot)
	{
		FTitleSaveSummary Summary;
		Summary.bHasSave = true;
		Summary.Slot = Slot;
		Summary.LeaderName = FText::FromString(TEXT("Ash"));
		return Summary;
	}
	static int32 IndexOf(const TArray<FPauseMenuPage> &Pages, EPauseMenuPage Page)
	{
		return Pages.IndexOfByPredicate([Page](const FPauseMenuPage &Entry) { return Entry.Page == Page; });
	}
END_DEFINE_SPEC(FJoinMenuSpec)

void FJoinMenuSpec::Define()
{
	It("lists the guest's own slots and the +, without the title's buttons", [this]()
	{
		const TArray<FTitleScreenRow> Rows = WoRTitleScreen::BuildJoinRows({MakeSlot(0)}, JoinMenuSpecValues::MAX_SLOTS);
		if (!TestEqual(TEXT("one slot and the +"), Rows.Num(), 2))
		{
			return;
		}
		TestTrue(TEXT("the slot first, with its save"), Rows[0].Id == ETitleScreenRow::Continue && Rows[0].Slot == 0);
		TestTrue(TEXT("then the +"), Rows[1].Id == ETitleScreenRow::NewGame);
	});

	It("finds the character a save holds by its new-game picks", [this]()
	{
		FWoRSaveData Data;
		Data.Party.Members.AddDefaulted(); // an old member with no picks
		FSavedPartyMember &Leader = Data.Party.Members.AddDefaulted_GetRef();
		Leader.Picks.Name = TEXT("Ash");
		Leader.Picks.Class = ECharacterClass::Caster;
		FPlayerPicks Found;
		TestTrue(TEXT("found"), WoRTitleScreen::FindSavedPicks(Data, Found));
		TestEqual(TEXT("the member with picks"), Found.Name, FString(TEXT("Ash")));
		TestFalse(TEXT("a save from before picks can't join"), WoRTitleScreen::FindSavedPicks(FWoRSaveData(), Found));
	});

	It("sends nothing without a player or picks", [this]()
	{
		FPlayerPicks Picks;
		Picks.Name = TEXT("Ash");
		TestFalse(TEXT("no player"), WoRTitleScreen::SendJoin(nullptr, Picks, 0));
	});

	It("shows the game menu's Join row to a guest only, before Quit, refused once seated", [this]()
	{
		TestEqual(TEXT("not for the host"), IndexOf(WoRPauseMenu::BuildPages(false, true, FText::GetEmpty(), false, false, false), EPauseMenuPage::Join), INDEX_NONE);
		const TArray<FPauseMenuPage> Guest = WoRPauseMenu::BuildPages(false, false, FText::GetEmpty(), false, true, false);
		TestEqual(TEXT("just before Quit"), IndexOf(Guest, EPauseMenuPage::Join), IndexOf(Guest, EPauseMenuPage::Quit) - 1);
		TestTrue(TEXT("a guest with no seat can join"), Guest[IndexOf(Guest, EPauseMenuPage::Join)].CanOpen());
		const TArray<FPauseMenuPage> Seated = WoRPauseMenu::BuildPages(false, false, FText::GetEmpty(), false, true, true);
		TestFalse(TEXT("a seated guest can't join again"), Seated[IndexOf(Seated, EPauseMenuPage::Join)].CanOpen());
	});
}

#endif
