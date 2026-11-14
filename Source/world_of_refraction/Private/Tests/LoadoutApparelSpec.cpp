// LoadoutApparelSpec.cpp
// B-440f (D-164, CL-50): the loadout screen's apparel rows. The picker lists the owned pieces the loadout doesn't
// already wear (a stone in the socket refused); a pick replaces the row's piece or fills a free slot (two at most);
// X takes the row's piece off.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Loadout/Entries/FApparelEntry.h"
#include "Loadout/FCombatLoadout.h"
#include "UI/Menus/LoadoutApparelModel.h"
#include "UI/Menus/LoadoutScreenModel.h"

namespace
{
	FApparelEntry ApparelTestPiece()
	{
		FApparelEntry Piece = FApparelEntry::CreateFromApparel(NewObject<UApparelData>(GetTransientPackage()));
		Piece.PersistentID = FGuid::NewGuid();
		return Piece;
	}

	FLoadoutScreenRow ApparelTestEmptyRow()
	{
		FLoadoutScreenRow Row;
		Row.Kind = ELoadoutScreenRowKind::EmptyApparel;
		return Row;
	}
}

BEGIN_DEFINE_SPEC(FLoadoutApparelSpec, "WoR.UI.LoadoutApparel", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	FApparelEntry Worn;
	FApparelEntry Spare;
	FApparelEntry Stoned;
	FCombatLoadout Loadout;
END_DEFINE_SPEC(FLoadoutApparelSpec)

void FLoadoutApparelSpec::Define()
{
	BeforeEach([this]()
	{
		Worn = ApparelTestPiece();
		Spare = ApparelTestPiece();
		Stoned = ApparelTestPiece();
		Stoned.AttachedItem.Kind = EAttachedItemKind::AugmentStone; // apparel's socket takes a crystal or an evolution only
		Loadout = FCombatLoadout();
		Loadout.Apparel = {Worn};
	});

	It("gives a worn piece's row its owned id", [this]()
	{
		const FLoadoutScreenRow Row = WoRLoadoutScreen::BuildApparelRow(Worn);
		TestEqual(TEXT("the piece"), Row.GearID, Worn.PersistentID);
		TestTrue(TEXT("an apparel row"), WoRLoadoutApparel::IsApparelRow(Row));
		TestFalse(TEXT("outside the budget"), Row.bCountsToBudget);
	});

	It("lists the owned pieces not already worn, a stoned one refused", [this]()
	{
		const TArray<FApparelEntry> Owned = {Worn, Spare, Stoned};
		const TArray<FLoadoutScreenChoice> Choices = WoRLoadoutApparel::BuildApparelChoices(Loadout, ApparelTestEmptyRow(), Owned);
		if (!TestEqual(TEXT("the spare and the stoned one"), Choices.Num(), 2))
		{
			return;
		}
		TestEqual(TEXT("the spare first"), Choices[0].Row.GearID, Spare.PersistentID);
		TestTrue(TEXT("allowed"), Choices[0].Refusal.IsEmpty());
		TestFalse(TEXT("the stoned one refused"), Choices[1].Refusal.IsEmpty());
		TestEqual(TEXT("not for a gear row"), WoRLoadoutApparel::BuildApparelChoices(Loadout, FLoadoutScreenRow(), Owned).Num(), 0);
	});

	It("replaces the row's piece, or fills a free slot up to two", [this]()
	{
		const FLoadoutScreenRow WornRow = WoRLoadoutScreen::BuildApparelRow(Worn);
		TestTrue(TEXT("replaced"), WoRLoadoutApparel::PutApparel(Loadout, WornRow, Spare));
		if (TestEqual(TEXT("still one worn"), Loadout.Apparel.Num(), 1))
		{
			TestEqual(TEXT("the spare"), Loadout.Apparel[0].PersistentID, Spare.PersistentID);
		}
		TestTrue(TEXT("a free slot"), WoRLoadoutApparel::PutApparel(Loadout, ApparelTestEmptyRow(), Worn));
		TestEqual(TEXT("two worn"), Loadout.Apparel.Num(), ApparelRules::MAX_APPAREL_SLOTS);
		TestFalse(TEXT("no third"), WoRLoadoutApparel::PutApparel(Loadout, ApparelTestEmptyRow(), ApparelTestPiece()));
		TestFalse(TEXT("not a stoned piece"), WoRLoadoutApparel::PutApparel(Loadout, WoRLoadoutScreen::BuildApparelRow(Worn), Stoned));
	});

	It("takes the row's piece off, and nothing from an empty row", [this]()
	{
		TestFalse(TEXT("empty row: nothing"), WoRLoadoutApparel::TakeOffApparel(Loadout, ApparelTestEmptyRow()));
		TestTrue(TEXT("off"), WoRLoadoutApparel::TakeOffApparel(Loadout, WoRLoadoutScreen::BuildApparelRow(Worn)));
		TestEqual(TEXT("none worn"), Loadout.Apparel.Num(), 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
