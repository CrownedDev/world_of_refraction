// PlayerBuildSpec.cpp
// B-554 (D-228, D-229, one-player stage 3): the player built from the picks (WoRPlayerBuild), the picks saved and the
// player built again on load (USaveSubsystem::CaptureParty / RestoreParty), one save file per character (the slot
// names) and the title's slot rows. Headless; no real save file is written.

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Character/PlayerBuild.h"
#include "Loadout/ClassDefaultLoadoutData.h"
#include "Misc/AutomationTest.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveTypes.h"
#include "UI/Menus/TitleScreenModel.h"
#include "WoRTestWorld.h"

namespace
{
	constexpr int32 PLAYER_BUILD_TEST_LEVEL = 3;
	constexpr int32 PLAYER_BUILD_TEST_TEMPLATE_LEVEL = 5;
	constexpr int32 PLAYER_BUILD_TEST_SPEED = 40;
	constexpr int32 PLAYER_BUILD_TEST_SECOND_SLOT = 2;
	constexpr int32 PLAYER_BUILD_TEST_MAX_SLOTS = 3;
	const TCHAR *PLAYER_BUILD_TEST_NAME = TEXT("Ash");
	const FName PLAYER_BUILD_TEST_OBJECT(TEXT("WoRPlayerSpec"));

	/** The shared player asset's stand-in: the Generic copy's name and class, which the build must not keep. */
	UCharacterData *MakeTemplate()
	{
		UCharacterData *Template = FWoRTestWorld::NewCharacterData();
		Template->Name = TEXT("Generic");
		Template->CharacterClass = ECharacterClass::Generic;
		Template->InnateElement = ESpellElement::Generic;
		Template->WorldMindLevel = PLAYER_BUILD_TEST_TEMPLATE_LEVEL;
		return Template;
	}

	FPlayerPicks MakePicks(ECharacterClass Class, ESpellElement Element, bool bBrokenDarkness = false)
	{
		FPlayerPicks Picks;
		Picks.Name = PLAYER_BUILD_TEST_NAME;
		Picks.Body = EPlayerBaseForm::Quinn;
		Picks.Class = Class;
		Picks.Element = Element;
		Picks.bBrokenDarkness = bBrokenDarkness;
		return Picks;
	}

	FTitleSaveSummary MakeSlot(int32 Slot, const TCHAR *Name)
	{
		FTitleSaveSummary Summary;
		Summary.bHasSave = true;
		Summary.LeaderName = FText::FromString(Name);
		Summary.Slot = Slot;
		return Summary;
	}
}

BEGIN_DEFINE_SPEC(FPlayerBuildSpec, "WoR.Player.Build", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FPlayerBuildSpec)

void FPlayerBuildSpec::Define()
{
	It("builds the player from the picks and the class numbers, not the shared asset's name and class", [this]()
	{
		const UCharacterData *Template = MakeTemplate();
		FClassElementNumbers Numbers;
		Numbers.Element = ESpellElement::Fire;
		Numbers.WorldMindLevel = PLAYER_BUILD_TEST_LEVEL;
		Numbers.WorldBodyLevel = PLAYER_BUILD_TEST_LEVEL;
		Numbers.WorldSpiritLevel = PLAYER_BUILD_TEST_LEVEL;
		const UCharacterData *Built = WoRPlayerBuild::BuildFrom(MakePicks(ECharacterClass::Caster, ESpellElement::Fire), Template, &Numbers,
																/*ActionSpeed=*/0, nullptr, PLAYER_BUILD_TEST_OBJECT);
		if (!TestNotNull(TEXT("built"), Built))
		{
			return;
		}
		TestEqual(TEXT("the typed name"), Built->Name, FString(PLAYER_BUILD_TEST_NAME));
		TestTrue(TEXT("the picked class and element"), Built->CharacterClass == ECharacterClass::Caster && Built->InnateElement == ESpellElement::Fire);
		TestEqual(TEXT("the class table's levels"), Built->WorldMindLevel, PLAYER_BUILD_TEST_LEVEL);
		TestEqual(TEXT("action speed 0: the normal speed"), Built->ActionSpeed, 0);
		TestTrue(TEXT("its fixed name, so the pool's key holds across loads"), Built->GetFName() == PLAYER_BUILD_TEST_OBJECT);
		TestEqual(TEXT("the shared asset is untouched"), Template->Name, FString(TEXT("Generic")));

		const UCharacterData *Broken = WoRPlayerBuild::BuildFrom(MakePicks(ECharacterClass::Caster, ESpellElement::Darkness, true), Template, nullptr,
																 PLAYER_BUILD_TEST_SPEED, nullptr, PLAYER_BUILD_TEST_OBJECT);
		TestTrue(TEXT("Broken Darkness is born Broken Darkness"), Broken && Broken->bBrokenDarknessInnate && Broken->InnateElement == ESpellElement::Darkness);
		TestTrue(TEXT("no class numbers: the asset's levels stay"), Broken && Broken->WorldMindLevel == PLAYER_BUILD_TEST_TEMPLATE_LEVEL);
		TestTrue(TEXT("a second build takes the fixed name; the first is renamed away"),
				 Broken && Broken->GetFName() == PLAYER_BUILD_TEST_OBJECT && Built->GetFName() != PLAYER_BUILD_TEST_OBJECT);

		const UCharacterData *Generic = WoRPlayerBuild::BuildFrom(MakePicks(ECharacterClass::Generic, ESpellElement::Fire), Template, nullptr,
																  PLAYER_BUILD_TEST_SPEED, nullptr, PLAYER_BUILD_TEST_OBJECT);
		TestTrue(TEXT("a Generic ignores the element pick and keeps its action speed"),
				 Generic && Generic->InnateElement == ESpellElement::Generic && !Generic->bBrokenDarknessInnate && Generic->ActionSpeed == PLAYER_BUILD_TEST_SPEED);
		TestNull(TEXT("no name: no picks, nothing built"), WoRPlayerBuild::BuildFrom(FPlayerPicks(), Template, nullptr, 0, nullptr, PLAYER_BUILD_TEST_OBJECT));
	});

	It("saves the picks and builds the player again from them on load", [this]()
	{
		FWoRTestWorld TestWorld;
		if (!TestTrue(TEXT("test world"), TestWorld.Create()))
		{
			return;
		}
		UPartySessionSubsystem *PartySession = TestWorld.GetSubsystem<UPartySessionSubsystem>();
		if (!TestNotNull(TEXT("party session"), PartySession))
		{
			return;
		}
		PartySession->PlayerCharacter = MakeTemplate();
		PartySession->NewGameCharacters.Reset(); // the built player keeps the shared asset's inventory

		FPartyMember Member;
		Member.PawnClass = TSoftClassPtr<ACombatCharacter>(ACombatCharacter::StaticClass());
		Member.Picks = MakePicks(ECharacterClass::Resonator, ESpellElement::Fire);
		Member.CharacterData = WoRPlayerBuild::Build(Member.Picks, TestWorld.GetWorld()->GetGameInstance());
		Member.MemberId = FGuid::NewGuid();
		if (!TestTrue(TEXT("built for the party"), WoRPlayerBuild::IsBuilt(Member.CharacterData)))
		{
			return;
		}
		UParty *Party = PartySession->RestoreLocalParty(FText::GetEmpty(), false, {Member});
		const FSavedParty Saved = USaveSubsystem::CaptureParty(Party, /*bKeepVitals=*/false);
		TestTrue(TEXT("the picks are saved"), Saved.Members.Num() == 1 && Saved.Members[0].Picks.Name == PLAYER_BUILD_TEST_NAME);

		PartySession->ForgetLocalParty();
		TestTrue(TEXT("restored"), USaveSubsystem::RestoreParty(Saved, *PartySession));
		const UParty *Loaded = PartySession->GetLocalParty();
		const UCharacterData *Rebuilt = Loaded && Loaded->Members.Num() == 1 ? Loaded->Members[0].CharacterData.Get() : nullptr;
		TestTrue(TEXT("built again from the picks"), WoRPlayerBuild::IsBuilt(Rebuilt) && Rebuilt->CharacterClass == ECharacterClass::Resonator);
		TestTrue(TEXT("with the same name, so the same pool key"), Rebuilt && Rebuilt->GetPathName() == Saved.Members[0].CharacterData.Path.ToString());
		TestTrue(TEXT("the member keeps its picks"), Loaded && Loaded->Members[0].Picks.Name == PLAYER_BUILD_TEST_NAME);

		FWoRSaveData Data;
		Data.Party = Saved;
		TestEqual(TEXT("the title names the slot by the picks"), WoRTitleScreen::Summarize(Data, true, 0.0).LeaderName.ToString(), FString(PLAYER_BUILD_TEST_NAME));
	});

	It("gives each character slot its own file and lists the filled slots, then the +", [this]()
	{
		TestEqual(TEXT("the first slot keeps the old file name"), USaveSubsystem::GetSlotName(0), FString(USaveSubsystem::SLOT_NAME));
		TestEqual(TEXT("the others are numbered"), USaveSubsystem::GetSlotName(PLAYER_BUILD_TEST_SECOND_SLOT),
				  FString::Printf(TEXT("%s_%d"), USaveSubsystem::SLOT_NAME, PLAYER_BUILD_TEST_SECOND_SLOT));

		const TArray<FTitleSaveSummary> Slots = {MakeSlot(0, TEXT("Ash")), MakeSlot(PLAYER_BUILD_TEST_SECOND_SLOT, TEXT("Bea"))};
		const TArray<FTitleScreenRow> Rows = WoRTitleScreen::BuildSlotRows(Slots, PLAYER_BUILD_TEST_MAX_SLOTS);
		if (!TestTrue(TEXT("two slots, the +, then the three buttons"), Rows.Num() == 6))
		{
			return;
		}
		TestTrue(TEXT("each filled slot continues its own file"),
				 Rows[0].Id == ETitleScreenRow::Continue && Rows[0].Slot == 0 && Rows[1].Slot == PLAYER_BUILD_TEST_SECOND_SLOT);
		TestTrue(TEXT("the + after them, with no slot of its own"), Rows[2].Id == ETitleScreenRow::NewGame && Rows[2].Slot == INDEX_NONE);
		TestFalse(TEXT("full: no +"), WoRTitleScreen::BuildSlotRows(Slots, Slots.Num()).ContainsByPredicate([](const FTitleScreenRow &Row)
		{
			return Row.Id == ETitleScreenRow::NewGame;
		}));
	});
}

#endif
