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
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/ClassDefaultLoadoutData.h"
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
	/** Darkness stands in the grid's second row, last column (Fire, Water, Earth / Wind, Light, Darkness / Broken
	 *  Darkness, ...). */
	constexpr int32 DARKNESS_GRID_INDEX = 5;
	/** B-546: Generic and Resonator, and the Refractor once per element (9) and for Broken Darkness. */
	constexpr int32 NEW_GAME_TEST_CLASS_CHOICES = 12;

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

	It("B-546: lists Broken Darkness and Reality for a Refractor, every weapon for a Generic, the nine crystals for a Resonator", [this]()
	{
		const TArray<FNewGameRefractorChoice> &Choices = WoRNewGame::GetRefractorChoices();
		TestEqual(TEXT("nine elements and Broken Darkness"), Choices.Num(), 10);
		TestTrue(TEXT("Broken Darkness after Darkness"), Choices.IsValidIndex(DARKNESS_GRID_INDEX + 1) && Choices[DARKNESS_GRID_INDEX + 1].bBrokenDarkness);
		TestTrue(TEXT("Reality last"), Choices.Last().Element == ESpellElement::Reality);
		FNewGamePick BrokenDarkness = MakePick(ECharacterClass::Caster);
		BrokenDarkness.bBrokenDarkness = true;
		TestEqual(TEXT("its title"), WoRNewGame::GetPanelTitle(BrokenDarkness).ToString(), FString(TEXT("Broken Darkness Refractor")));
		TestTrue(TEXT("a sword and a shield are two entries"), WoRNewGame::GetWeaponChoices().Contains(EWeaponType::Sword) && WoRNewGame::GetWeaponChoices().Contains(EWeaponType::Shield));
		TestEqual(TEXT("nine crystal types"), WoRNewGame::GetCrystalChoices().Num(), 9);
		TestEqual(TEXT("the class choices (the default most slots)"), WoRNewGame::GetClassChoiceCount(), NEW_GAME_TEST_CLASS_CHOICES);
	});

	It("B-559: offers only the weapon types the class table gives a weapon for", [this]()
	{
		UClassDefaultLoadoutData *Table = NewObject<UClassDefaultLoadoutData>(GetTransientPackage());
		TestEqual(TEXT("no pick weapons yet: every type"), WoRNewGame::GetWeaponChoicesFor(Table).Num(), static_cast<int32>(EWeaponType::Shield) + 1);
		Table->PickWeapons.Add(EWeaponType::Axe, nullptr); // a row with no weapon gives nothing, so it is not offered
		TestEqual(TEXT("only empty rows: still every type"), WoRNewGame::GetWeaponChoicesFor(Table).Num(), static_cast<int32>(EWeaponType::Shield) + 1);
		Table->PickWeapons.Add(EWeaponType::Shield, NewObject<UWeaponData>(GetTransientPackage()));
		Table->PickWeapons.Add(EWeaponType::Sword, NewObject<UWeaponData>(GetTransientPackage()));
		const TArray<EWeaponType> Choices = WoRNewGame::GetWeaponChoicesFor(Table);
		TestTrue(TEXT("the sword, then the shield, in type order"), Choices.Num() == 2 && Choices[0] == EWeaponType::Sword && Choices[1] == EWeaponType::Shield);
	});

	It("B-546: two picks for a Generic or a Resonator, the oldest giving way; Begin wants a name and both picks", [this]()
	{
		FNewGamePick Pick = MakePick(ECharacterClass::Generic);
		WoRNewGame::TogglePick(Pick, 0);
		WoRNewGame::TogglePick(Pick, 1);
		WoRNewGame::TogglePick(Pick, 2);
		TestTrue(TEXT("the third drops the first"), Pick.Weapons.Num() == 2 && !WoRNewGame::IsPicked(Pick, 0) && WoRNewGame::IsPicked(Pick, 2));
		WoRNewGame::TogglePick(Pick, 2);
		TestFalse(TEXT("a second press drops it"), WoRNewGame::IsPicked(Pick, 2));
		TestFalse(TEXT("one weapon: refused"), WoRNewGame::GetBeginRefusal(Pick).IsEmpty());
		WoRNewGame::TogglePick(Pick, 0);
		TestTrue(TEXT("two weapons and no name (D-228): Begin may run"), WoRNewGame::GetBeginRefusal(Pick).IsEmpty());
		FNewGamePick Resonator = MakePick(ECharacterClass::Resonator);
		WoRNewGame::TogglePick(Resonator, 0);
		WoRNewGame::TogglePick(Resonator, 6);
		TestTrue(TEXT("two crystal types, 3 of each in the line"),
				 WoRNewGame::GetBeginRefusal(Resonator).IsEmpty() && WoRNewGame::GetPicksLine(Resonator).ToString().Contains(FString::FromInt(WoRNewGame::STARTING_CRYSTALS_PER_TYPE)));
		FNewGamePick Refractor = MakePick(ECharacterClass::Caster);
		TestTrue(TEXT("a Refractor's one pick is always made"), WoRNewGame::GetBeginRefusal(Refractor).IsEmpty());
	});

	It("names a character by what it is, its class and element, never a typed name (D-228)", [this]()
	{
		FNewGamePick Pick = MakePick(ECharacterClass::Caster);
		Pick.Name = TEXT("Ash"); // an older save's typed name
		Pick.Element = ESpellElement::Fire;
		TestEqual(TEXT("Fire Refractor"), WoRNewGame::GetCharacterName(Pick).ToString(), FString(TEXT("Fire Refractor")));
		Pick.Element = ESpellElement::Reality;
		TestEqual(TEXT("Reality reads as Unknown"), WoRNewGame::GetCharacterName(Pick).ToString(), FString(TEXT("Unknown Refractor")));
		Pick.Element = ESpellElement::Darkness;
		Pick.bBrokenDarkness = true;
		TestEqual(TEXT("Broken Darkness Refractor"), WoRNewGame::GetCharacterName(Pick).ToString(), FString(TEXT("Broken Darkness Refractor")));
		TestEqual(TEXT("Generic"), WoRNewGame::GetCharacterName(MakePick(ECharacterClass::Generic)).ToString(), FString(TEXT("Generic")));
		TestEqual(TEXT("Resonator"), WoRNewGame::GetCharacterName(MakePick(ECharacterClass::Resonator)).ToString(), FString(TEXT("Resonator")));
		FPlayerPicks Recruit = MakePick(ECharacterClass::Caster);
		Recruit.Name = TEXT("Fire Spirit");
		Recruit.bRecruit = true;
		TestEqual(TEXT("a recruit keeps its row's name"), WoRNewGame::GetCharacterName(Recruit).ToString(), FString(TEXT("Fire Spirit")));
	});

	It("starts as the character whose class, and a Refractor's element and Broken Darkness, match", [this]()
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
		FNewGamePick BrokenDarkness = MakePick(ECharacterClass::Caster);
		BrokenDarkness.bBrokenDarkness = true;
		TestTrue(TEXT("B-546: Broken Darkness starts as the born one"), WoRNewGame::FindStartCharacter(Choices, BrokenDarkness) == BornBD);
	});

	It("moves between the body, class, the class's picks and Begin", [this]()
	{
		UNewGameScreenWidget *Screen = NewObject<UNewGameScreenWidget>();
		TestTrue(TEXT("opens on the class, as a Darkness Refractor"),
				 Screen->GetFocus() == ENewGameFocus::Class && Screen->GetPick().Class == ECharacterClass::Caster &&
					 Screen->GetPick().Element == ESpellElement::Darkness);
		Screen->MoveSide(1);
		TestTrue(TEXT("Right: the element grid"), Screen->GetFocus() == ENewGameFocus::Element);
		Screen->MoveSide(-1);
		TestTrue(TEXT("Left in the grid picks the element beside it"), Screen->GetPick().Element == WoRNewGame::GetRefractorChoices()[DARKNESS_GRID_INDEX - 1].Element);
		Screen->MoveDown();
		TestTrue(TEXT("Down picks the element under it"), Screen->GetPick().Element == WoRNewGame::GetRefractorChoices()[DARKNESS_GRID_INDEX + 2].Element);
		Screen->MoveDown();
		Screen->MoveDown();
		TestTrue(TEXT("Down from the last row: Begin"), Screen->GetFocus() == ENewGameFocus::Begin);
		Screen->MoveUp();
		Screen->MoveUp();
		TestTrue(TEXT("Up from Begin: the class, Up again: the class above"),
				 Screen->GetFocus() == ENewGameFocus::Class && Screen->GetPick().Class == ECharacterClass::Generic);
		Screen->MoveSide(1);
		TestTrue(TEXT("B-546: a Generic's weapon grid, from its first entry"), Screen->GetFocus() == ENewGameFocus::Element && Screen->GetPickCursor() == 0);
		Screen->MoveSide(-1);
		Screen->MoveSide(-1);
		TestTrue(TEXT("Left from the class: the body cards, on its left (B-539)"), Screen->GetFocus() == ENewGameFocus::Body);
		Screen->MoveUp();
		TestTrue(TEXT("D-228: Up from the body stays there (no name box)"), Screen->GetFocus() == ENewGameFocus::Body);
		Screen->MoveDown();
		Screen->MoveUp();
		Screen->MoveSide(1);
		TestTrue(TEXT("Up from the top class: the body; Right picks Quinn"),
				 Screen->GetFocus() == ENewGameFocus::Body && Screen->GetPick().Body == EPlayerBaseForm::Quinn);
		Screen->Confirm();
		Screen->Confirm();
		Screen->Confirm();
		TestTrue(TEXT("A walks on: body, class, the grid; A there picks the lit weapon (a Generic)"),
				 Screen->GetFocus() == ENewGameFocus::Element && Screen->GetPick().Weapons.Num() == 1);
		for (int32 Step = 0; Step < WoRNewGame::GetWeaponChoices().Num(); ++Step)
		{
			Screen->MoveDown();
		}
		TestTrue(TEXT("Down from the grid's last row: Begin"), Screen->GetFocus() == ENewGameFocus::Begin);
		Screen->Confirm();
		TestFalse(TEXT("Begin with no name refuses in words"), Screen->GetStatus().IsEmpty());
	});

	It("D-233: a click picks a body, a class or an element and takes the focus; a click on Begin is A on Begin", [this]()
	{
		UNewGameScreenWidget *Screen = NewObject<UNewGameScreenWidget>();
		Screen->ClickPart(ENewGameFocus::Element, 0);
		TestTrue(TEXT("an element chip (a Refractor)"),
				 Screen->GetFocus() == ENewGameFocus::Element && Screen->GetPick().Element == WoRNewGame::GetElements()[0]);
		Screen->ClickPart(ENewGameFocus::Body, static_cast<int32>(EPlayerBaseForm::Quinn));
		TestTrue(TEXT("a body card"), Screen->GetFocus() == ENewGameFocus::Body && Screen->GetPick().Body == EPlayerBaseForm::Quinn);
		Screen->ClickPart(ENewGameFocus::Class, 0);
		TestTrue(TEXT("a class card"), Screen->GetFocus() == ENewGameFocus::Class && Screen->GetPick().Class == WoRNewGame::GetClasses()[0]);
		const FNewGamePick Before = Screen->GetPick();
		Screen->ClickPart(ENewGameFocus::Class, WoRNewGame::GetClasses().Num());
		Screen->ClickPart(ENewGameFocus::Body, INDEX_NONE);
		TestTrue(TEXT("a click off the cards changes nothing"),
				 Screen->GetPick().Class == Before.Class && Screen->GetPick().Body == Before.Body && Screen->GetFocus() == ENewGameFocus::Class);
		if (Before.Class == ECharacterClass::Generic)
		{
			Screen->ClickPart(ENewGameFocus::Element, 0);
			TestTrue(TEXT("B-546: a click on a Generic's grid picks that weapon"),
					 Screen->GetFocus() == ENewGameFocus::Element && WoRNewGame::IsPicked(Screen->GetPick(), 0));
			Screen->ClickPart(ENewGameFocus::Element, WoRNewGame::GetWeaponChoices().Num());
			TestEqual(TEXT("a click past the list changes nothing"), Screen->GetPick().Weapons.Num(), 1);
		}
		Screen->ClickPart(ENewGameFocus::Begin, 0);
		TestTrue(TEXT("Begin: the focus, and A's refusal with no player"),
				 Screen->GetFocus() == ENewGameFocus::Begin && !Screen->GetStatus().IsEmpty());
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
