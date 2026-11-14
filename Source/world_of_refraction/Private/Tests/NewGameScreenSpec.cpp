// NewGameScreenSpec.cpp
// B-469: the new game screen's words and keys, the pick's start character, and the fresh game in place
// (USaveSubsystem::StartFreshGame). Headless: the screen without its tree; the save in a test world with no local
// player, so the real slot is never written.

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Character/Forms/CharacterFormSubsystem.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Loadout/LoadoutRules.h"
#include "Memory/HiddenCounterSubsystem.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "UI/Menus/NewGameScreenModel.h"
#include "UI/Menus/NewGameScreenWidget.h"

namespace
{
	const FName NEW_GAME_TEST_COUNTER(TEXT("NewGameSpecCounter"));
	constexpr int32 NEW_GAME_TEST_GARNETS = 4;
	/** Darkness stands in the grid's top row, last column (Fire, Water, Earth / Wind, Light, Darkness / ...). */
	constexpr int32 DARKNESS_GRID_INDEX = 5;

	UCharacterData *MakeStart(ECharacterClass Class, ESpellElement Element, bool bBornBrokenDarkness = false)
	{
		UCharacterData *Data = NewObject<UCharacterData>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UCharacterData::StaticClass()));
		Data->CharacterClass = Class;
		Data->InnateElement = Element;
		Data->bBrokenDarknessInnate = bBornBrokenDarkness;
		return Data;
	}

	FPartyMember MakeMember(UCharacterData *Data)
	{
		FPartyMember Member;
		Member.PawnClass = TSoftClassPtr<ACombatCharacter>(ACombatCharacter::StaticClass());
		Member.CharacterData = Data;
		Member.MemberId = FGuid::NewGuid();
		return Member;
	}

	FNewGamePick MakePick(ECharacterClass Class, ESpellElement Element = ESpellElement::Darkness)
	{
		FNewGamePick Pick;
		Pick.Class = Class;
		Pick.Element = Element;
		return Pick;
	}
}

BEGIN_DEFINE_SPEC(FNewGameScreenSpec, "WoR.UI.NewGame", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FNewGameScreenSpec)

void FNewGameScreenSpec::Define()
{
	It("names the pick on the panel and the button, and each class's budget from the class table", [this]()
	{
		TestEqual(TEXT("three classes"), WoRNewGame::GetClasses().Num(), 3);
		TestEqual(TEXT("nine elements"), WoRNewGame::GetElements().Num(), 9);
		TestEqual(TEXT("Refractor title"), WoRNewGame::GetPanelTitle(MakePick(ECharacterClass::Caster)).ToString(), FString(TEXT("Darkness Refractor")));
		TestEqual(TEXT("Refractor Begin"), WoRNewGame::GetBeginLabel(MakePick(ECharacterClass::Caster, ESpellElement::Fire)).ToString(),
				  FString(TEXT("Begin as Fire Refractor")));
		TestEqual(TEXT("Generic Begin"), WoRNewGame::GetBeginLabel(MakePick(ECharacterClass::Generic)).ToString(), FString(TEXT("Begin as a Generic")));
		TestEqual(TEXT("Generic title: no element"), WoRNewGame::GetPanelTitle(MakePick(ECharacterClass::Generic, ESpellElement::Fire)).ToString(),
				  FString(TEXT("Generic")));
		for (const ECharacterClass Class : WoRNewGame::GetClasses())
		{
			const FString Budget = FString::Printf(TEXT("Budget %d."), LoadoutClassTable::Get(Class).Budget);
			TestTrue(*FString::Printf(TEXT("%s's line ends with its budget"), *WoRNewGame::GetClassKind(Class).ToString()),
					 WoRNewGame::GetClassLine(Class).ToString().EndsWith(Budget));
		}
		for (const ESpellElement Element : WoRNewGame::GetElements())
		{
			TestFalse(TEXT("every element has its full-bar line"), WoRNewGame::GetElementLine(Element).IsEmpty());
		}
	});

	It("starts as the character whose class, and a Refractor's element, match; never a born Broken Darkness", [this]()
	{
		UCharacterData *Generic = MakeStart(ECharacterClass::Generic, ESpellElement::Generic);
		UCharacterData *BornBD = MakeStart(ECharacterClass::Caster, ESpellElement::Darkness, true);
		UCharacterData *Darkness = MakeStart(ECharacterClass::Caster, ESpellElement::Darkness);
		UCharacterData *Fire = MakeStart(ECharacterClass::Caster, ESpellElement::Fire);
		const TArray<TSoftObjectPtr<UCharacterData>> Choices = {Generic, BornBD, Darkness, Fire};
		TestTrue(TEXT("Generic"), WoRNewGame::FindStartCharacter(Choices, MakePick(ECharacterClass::Generic)) == Generic);
		TestTrue(TEXT("Darkness Refractor, not the born BD"), WoRNewGame::FindStartCharacter(Choices, MakePick(ECharacterClass::Caster)) == Darkness);
		TestTrue(TEXT("Fire Refractor"), WoRNewGame::FindStartCharacter(Choices, MakePick(ECharacterClass::Caster, ESpellElement::Fire)) == Fire);
		TestNull(TEXT("no Void Refractor set up"), WoRNewGame::FindStartCharacter(Choices, MakePick(ECharacterClass::Caster, ESpellElement::Void)));
		TestNull(TEXT("no Resonator set up"), WoRNewGame::FindStartCharacter(Choices, MakePick(ECharacterClass::Resonator)));
	});

	It("moves between body, class, a Refractor's elements and Begin", [this]()
	{
		UNewGameScreenWidget *Screen = NewObject<UNewGameScreenWidget>();
		TestTrue(TEXT("opens on the class, as a Darkness Refractor"),
				 Screen->GetFocus() == ENewGameFocus::Class && Screen->GetPick().Class == ECharacterClass::Caster &&
					 Screen->GetPick().Element == ESpellElement::Darkness);
		Screen->MoveSide(1);
		TestTrue(TEXT("Right: the element grid"), Screen->GetFocus() == ENewGameFocus::Element);
		Screen->MoveSide(-1);
		TestTrue(TEXT("Left in the grid picks the element beside it"), Screen->GetPick().Element == WoRNewGame::GetElements()[DARKNESS_GRID_INDEX - 1]);
		Screen->MoveDown();
		TestTrue(TEXT("Down picks the element under it"), Screen->GetPick().Element == WoRNewGame::GetElements()[DARKNESS_GRID_INDEX + 2]);
		Screen->MoveDown();
		Screen->MoveDown();
		TestTrue(TEXT("Down from the last row: Begin"), Screen->GetFocus() == ENewGameFocus::Begin);
		Screen->MoveUp();
		Screen->MoveUp();
		TestTrue(TEXT("Up from Begin: the class, Up again: the class above"),
				 Screen->GetFocus() == ENewGameFocus::Class && Screen->GetPick().Class == ECharacterClass::Generic);
		Screen->MoveSide(1);
		TestTrue(TEXT("a Generic has no element grid"), Screen->GetFocus() == ENewGameFocus::Class);
		Screen->MoveUp();
		Screen->MoveSide(1);
		TestTrue(TEXT("Up from the top class: the body; Right picks Quinn"),
				 Screen->GetFocus() == ENewGameFocus::Body && Screen->GetPick().Body == EPlayerBaseForm::Quinn);
		Screen->Confirm();
		Screen->Confirm();
		TestTrue(TEXT("A walks on: body, class, then Begin (a Generic)"), Screen->GetFocus() == ENewGameFocus::Begin);
		Screen->Confirm();
		TestFalse(TEXT("Begin with no player refuses in words"), Screen->GetStatus().IsEmpty());
	});

	Describe("the fresh game in place", [this]()
	{
		It("leaves only the picked character in the party and empties what the old game held", [this]()
		{
			FWoRTestWorld TestWorld;
			if (!TestTrue(TEXT("test world"), TestWorld.Create()))
			{
				return;
			}
			USaveSubsystem *Save = TestWorld.GetSubsystem<USaveSubsystem>();
			UPartySessionSubsystem *PartySession = TestWorld.GetSubsystem<UPartySessionSubsystem>();
			UPoolSubsystem *Pool = TestWorld.GetSubsystem<UPoolSubsystem>();
			UHiddenCounterSubsystem *Counters = TestWorld.GetSubsystem<UHiddenCounterSubsystem>();
			UCharacterFormSubsystem *Forms = TestWorld.GetSubsystem<UCharacterFormSubsystem>();
			if (!TestNotNull(TEXT("subsystems"), Save) || !PartySession || !Pool || !Counters || !Forms)
			{
				return;
			}
			UCharacterData *Old = MakeStart(ECharacterClass::Generic, ESpellElement::Generic);
			UCharacterData *Picked = MakeStart(ECharacterClass::Caster, ESpellElement::Fire);
			PartySession->RestoreLocalParty(FText::GetEmpty(), false, {MakeMember(Old), MakeMember(Old)});
			const FCrystalId Garnet(ECrystalType::Garnet, EItemTier::F_Tier);
			Pool->AddCrystalToPool(Garnet, NEW_GAME_TEST_GARNETS);
			Counters->Add(NEW_GAME_TEST_COUNTER, Old);

			TestFalse(TEXT("no character: refused"), Save->StartFreshGame(FPartyMember(), EPlayerBaseForm::Quinn));
			TestEqual(TEXT("refused: the old party stays"), PartySession->GetLocalParty()->Members.Num(), 2);

			FPartyMember Leader = MakeMember(Picked);
			Leader.bPlayerControlled = false;
			if (!TestTrue(TEXT("started"), Save->StartFreshGame(Leader, EPlayerBaseForm::Quinn)))
			{
				return;
			}
			const UParty *Party = PartySession->GetLocalParty();
			if (!TestNotNull(TEXT("a party"), Party) || !TestEqual(TEXT("one member"), Party->Members.Num(), 1))
			{
				return;
			}
			TestTrue(TEXT("the picked character"), Party->Members[0].CharacterData == Picked);
			TestTrue(TEXT("the leader is the player's"), Party->Members[0].bPlayerControlled);
			TestEqual(TEXT("the pool is empty"), Pool->GetCrystals().FindRef(Garnet), 0);
			TestEqual(TEXT("the counters start again"), Counters->Get(NEW_GAME_TEST_COUNTER, Old), 0);
			TestTrue(TEXT("the picked body"), Forms->GetBaseForm() == EPlayerBaseForm::Quinn);
			TestFalse(TEXT("no run waits"), Save->GetData().Run.bHasRun);
		});
	});
}
