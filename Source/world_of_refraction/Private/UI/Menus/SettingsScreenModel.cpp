// SettingsScreenModel.cpp

#include "UI/Menus/SettingsScreenModel.h"

#include "Combat/Camera/CombatCameraTypes.h"
#include "Combat/CombatOrchestrator.h"
#include "UI/Settings/TextSizeSetting.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Save/SaveSubsystem.h"
#include "UI/Combat/Battle/BattleHUDView.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRSettingsScreen"

namespace SettingsScreenBuild
{
	USaveSubsystem *FindSave(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<USaveSubsystem>() : nullptr;
	}

	FText OnOff(bool bOn)
	{
		return bOn ? LOCTEXT("On", "On") : LOCTEXT("Off", "Off");
	}

	/** A row the code has no setting for yet: listed (the mockup is approved with it), focusable, and it says so. */
	FSettingsRow NotBuilt(ESettingId Id, const FText &Label, const FText &Explanation)
	{
		FSettingsRow Row;
		Row.Id = Id;
		Row.Label = Label;
		Row.Explanation = Explanation;
		Row.Refusal = LOCTEXT("NotBuilt", "This setting isn't built yet."); // TODO: B-453 (and B-436l for the battle controls)
		return Row;
	}

	FSettingsRow BattleControlsRow()
	{
		return NotBuilt(ESettingId::BattleControls, LOCTEXT("BattleControls", "Battle menu controls"),
						LOCTEXT("BattleControlsExplain", "How you move through the skill menu in a fight. Shortcuts is faster; Omni D-pad uses fewer buttons. Both reach every skill."));
	}

	FSettingsChoice Choice(const FText &Name, const FText &What, bool bDefault)
	{
		FSettingsChoice Out;
		Out.Name = Name;
		Out.What = What;
		Out.bDefault = bDefault;
		return Out;
	}

	/** B-631: an On / Off row: its value, its two choices (On first) and the switch look. */
	void SetSwitch(FSettingsRow &Row, bool bOn, bool bDefaultOn, const FText &OnWhat, const FText &OffWhat)
	{
		Row.Value = OnOff(bOn);
		Row.Choices = {Choice(OnOff(true), OnWhat, bDefaultOn), Choice(OnOff(false), OffWhat, !bDefaultOn)};
		Row.Current = bOn ? 0 : 1;
		Row.bSwitch = true;
	}

	/** B-631: a row of named choices (Left / Right step them in this order); Current is the one in use. */
	void SetChoices(FSettingsRow &Row, TArray<FSettingsChoice> &&Choices, int32 Current)
	{
		Row.Choices = MoveTemp(Choices);
		Row.Current = Row.Choices.IsValidIndex(Current) ? Current : INDEX_NONE;
		Row.Value = Row.Choices.IsValidIndex(Current) ? Row.Choices[Current].Name : FText::GetEmpty();
	}

	FSettingsKeyRow Heading(const FText &Section)
	{
		FSettingsKeyRow Row;
		Row.Section = Section;
		return Row;
	}

	FSettingsKeyRow Key(const FText &Action, const FText &Keyboard, const FText &Gamepad)
	{
		FSettingsKeyRow Row;
		Row.Action = Action;
		Row.Keyboard = Keyboard;
		Row.Gamepad = Gamepad;
		return Row;
	}
}

FText WoRSettingsScreen::GetPageName(ESettingsPage Page)
{
	switch (Page)
	{
	case ESettingsPage::Gameplay:
		return LOCTEXT("PageGameplay", "Gameplay");
	case ESettingsPage::Camera:
		return LOCTEXT("PageCamera", "Camera");
	case ESettingsPage::Display:
		return LOCTEXT("PageDisplay", "Display");
	case ESettingsPage::Audio:
		return LOCTEXT("PageAudio", "Audio");
	case ESettingsPage::Controls:
		return LOCTEXT("PageControls", "Controls");
	case ESettingsPage::Accessibility:
		return LOCTEXT("PageAccessibility", "Accessibility");
	default:
		return FText::GetEmpty();
	}
}

TArray<FSettingsRow> WoRSettingsScreen::BuildRows(const UWorld *World, ESettingsPage Page)
{
	using namespace SettingsScreenBuild;

	TArray<FSettingsRow> Rows;
	const USaveSubsystem *Save = FindSave(World);
	if (Page == ESettingsPage::Gameplay)
	{
		Rows.Add(BattleControlsRow());

		FSettingsRow TurnView;
		TurnView.Id = ESettingId::TurnView;
		TurnView.Label = FCombatCameraRules::GetTurnViewSettingName();
		TurnView.Explanation = LOCTEXT("TurnViewExplain", "Where the camera sits on your turn in a fight: behind and above the character, over their shoulder, or from the side.");
		// In ETurnViewStyle's order, so Left / Right step as ChangeSetting does; Over the Shoulder is the default (Q-600g).
		SetChoices(TurnView,
				   {Choice(FCombatCameraRules::GetTurnViewStyleName(ETurnViewStyle::CentredWide), LOCTEXT("TurnViewWide", "Behind and above your side; the whole fight on screen."), false),
					Choice(FCombatCameraRules::GetTurnViewStyleName(ETurnViewStyle::OverTheShoulder), LOCTEXT("TurnViewShoulder", "Close behind the fighter whose turn it is."), true),
					Choice(FCombatCameraRules::GetTurnViewStyleName(ETurnViewStyle::Tactical), LOCTEXT("TurnViewTactical", "From the side: both rows and their places easy to read."), false)},
				   static_cast<int32>(Save ? Save->GetTurnViewStyle() : ETurnViewStyle::OverTheShoulder));
		Rows.Add(TurnView);

		FSettingsRow EndTurn;
		EndTurn.Id = ESettingId::EndTurnButton;
		EndTurn.Label = LOCTEXT("EndTurnButton", "End Turn button");
		EndTurn.Explanation = LOCTEXT("EndTurnExplain", "Shows the End Turn button on the battle menu.");
		SetSwitch(EndTurn, !Save || !Save->IsEndTurnButtonHidden(), true, LOCTEXT("EndTurnOn", "The battle menu shows End Turn."),
				  LOCTEXT("EndTurnOff", "Hidden from the battle menu."));
		Rows.Add(EndTurn);

		Rows.Add(NotBuilt(ESettingId::CornerHints, LOCTEXT("CornerHints", "Button hints"),
						  LOCTEXT("CornerHintsExplain", "Shows which button does what, in the corner of the screen.")));
		Rows.Add(NotBuilt(ESettingId::BossCinematics, LOCTEXT("BossCinematics", "Boss cinematics"),
						  LOCTEXT("BossCinematicsExplain", "Plays the short scene when a great foe appears or changes phase. They can always be skipped.")));
		FSettingsRow Idle; // B-453e (D-190)
		Idle.Id = ESettingId::AIFinishesTurn;
		Idle.Label = LOCTEXT("AIFinishesTurn", "Auto-finish turn");
		Idle.Explanation = FText::Format(LOCTEXT("AIFinishesTurnExplain", "If you don't act for {0} seconds, the AI plays the rest of your turn. Your next turn is yours again."),
										 FText::AsNumber(FMath::RoundToInt32(ACombatOrchestrator::GetIdleTurnSeconds())));
		SetSwitch(Idle, Save && Save->IsAIFinishesIdleTurnOn(), false, LOCTEXT("AIFinishesOn", "When you idle, the AI plays the rest of your turn."),
				  LOCTEXT("AIFinishesOff", "Your turn waits for you."));
		Rows.Add(Idle);
		Rows.Add(NotBuilt(ESettingId::TutorialTips, LOCTEXT("TutorialTips", "Tutorial tips"),
						  LOCTEXT("TutorialTipsExplain", "Short tips the first time something new comes up.")));
		FSettingsRow Intel; // B-587 (D-262)
		Intel.Id = ESettingId::IntelKey;
		Intel.Label = LOCTEXT("IntelKey", "Intel key");
		Intel.Explanation = LOCTEXT("IntelKeyExplain", "V / R3 in a fight. Cycle: the first press shows your party, the second the enemies, the third closes it. Open and close: each press opens or closes it. Holding it always closes.");
		SetChoices(Intel,
				   {Choice(LOCTEXT("IntelKeyCycle", "Cycle"), LOCTEXT("IntelKeyCycleWhat", "Your party, then the enemies, then closed."), true),
					Choice(LOCTEXT("IntelKeyOpenClose", "Open and close"), LOCTEXT("IntelKeyOpenCloseWhat", "Each press opens or closes it."), false)},
				   !Save || Save->IsIntelKeyCycle() ? 0 : 1);
		Rows.Add(Intel);
		FSettingsRow Layout; // B-590 (D-264)
		Layout.Id = ESettingId::TurnOrderLayout;
		Layout.Label = LOCTEXT("TurnOrderLayout", "Turn order layout");
		Layout.Explanation = LOCTEXT("TurnOrderLayoutExplain", "How the fight's turn order sits at the top right: a row across, or a column down the side.");
		SetChoices(Layout,
				   {Choice(LOCTEXT("TurnOrderRow", "Row"), LOCTEXT("TurnOrderRowWhat", "A row across the top right."), true),
					Choice(LOCTEXT("TurnOrderColumn", "Column"), LOCTEXT("TurnOrderColumnWhat", "A column down the right side."), false)},
				   Save && Save->IsTurnOrderColumn() ? 1 : 0);
		Rows.Add(Layout);
		FSettingsRow Turns; // B-590 (D-264)
		Turns.Id = ESettingId::TurnsShown;
		Turns.Label = LOCTEXT("TurnsShown", "Turns shown");
		Turns.Explanation = FText::Format(LOCTEXT("TurnsShownExplain", "How many turns the turn order shows, the current one included: {0} to {1}."),
										  FText::AsNumber(BattleHUDView::MIN_TURNS_SHOWN), FText::AsNumber(BattleHUDView::MAX_TURNS_SHOWN));
		TArray<FSettingsChoice> Counts;
		for (int32 Count = BattleHUDView::MIN_TURNS_SHOWN; Count <= BattleHUDView::MAX_TURNS_SHOWN; ++Count)
		{
			Counts.Add(Choice(FText::AsNumber(Count), FText::GetEmpty(), Count == BattleHUDView::DEFAULT_TURNS_SHOWN));
		}
		SetChoices(Turns, MoveTemp(Counts), (Save ? Save->GetTurnsShown() : BattleHUDView::DEFAULT_TURNS_SHOWN) - BattleHUDView::MIN_TURNS_SHOWN);
		Rows.Add(Turns);
	}
	else if (Page == ESettingsPage::Accessibility)
	{
		Rows.Add(BattleControlsRow()); // repeated here on purpose: people look for it in both places
		FSettingsRow Text; // B-453d (D-190)
		Text.Id = ESettingId::TextSize;
		Text.Label = WoRTextSize::GetSettingName();
		Text.Explanation = LOCTEXT("TextSizeExplain", "Makes the text in menus and in fights larger: the whole screen's text and panels grow together.");
		SetChoices(Text,
				   {Choice(WoRTextSize::GetLevelName(ETextSizeLevel::Normal), LOCTEXT("TextSizeNormal", "The game's own size."), true),
					Choice(WoRTextSize::GetLevelName(ETextSizeLevel::Large), LOCTEXT("TextSizeLarge", "Text and panels one size up."), false),
					Choice(WoRTextSize::GetLevelName(ETextSizeLevel::Larger), LOCTEXT("TextSizeLarger", "Text and panels two sizes up."), false)},
				   static_cast<int32>(Save ? Save->GetTextSizeLevel() : ETextSizeLevel::Normal));
		Rows.Add(Text);
		FSettingsRow Movement; // B-453b (D-190, D-078)
		Movement.Id = ESettingId::FightCameraMovement;
		Movement.Label = LOCTEXT("FightCameraMovement", "Camera motion");
		Movement.Explanation = LOCTEXT("FightCameraMovementExplain", "Off keeps the camera still during attacks, for anyone the movement makes unwell.");
		SetSwitch(Movement, !Save || !Save->IsFightCameraMovementOff(), true, LOCTEXT("FightCameraMovementOn", "The camera moves with attacks."),
				  LOCTEXT("FightCameraMovementOff", "One still view through the fight."));
		Rows.Add(Movement);
		FSettingsRow Shake; // B-453a (D-190)
		Shake.Id = ESettingId::ScreenShake;
		Shake.Label = FCombatCameraRules::GetScreenShakeSettingName();
		Shake.Explanation = LOCTEXT("ScreenShakeExplain", "How much the screen shakes on heavy hits: full, reduced to half, or off.");
		SetChoices(Shake,
				   {Choice(FCombatCameraRules::GetScreenShakeLevelName(EScreenShakeLevel::Full), LOCTEXT("ScreenShakeFull", "Heavy hits shake the screen."), true),
					Choice(FCombatCameraRules::GetScreenShakeLevelName(EScreenShakeLevel::Reduced), LOCTEXT("ScreenShakeReduced", "Half the shake."), false),
					Choice(FCombatCameraRules::GetScreenShakeLevelName(EScreenShakeLevel::Off), LOCTEXT("ScreenShakeOff", "The screen stays still."), false)},
				   static_cast<int32>(Save ? Save->GetScreenShakeLevel() : EScreenShakeLevel::Full));
		Rows.Add(Shake);
		FSettingsRow Hold; // B-453c (D-190)
		Hold.Id = ESettingId::HoldToConfirm;
		Hold.Label = LOCTEXT("HoldToConfirm", "Hold to confirm");
		Hold.Explanation = LOCTEXT("HoldToConfirmExplain", "Asks you to hold the button, not press it, before something that can't be undone. Off: a second press confirms.");
		SetSwitch(Hold, !Save || Save->IsHoldToConfirmOn(), true, LOCTEXT("HoldToConfirmOn", "Hold the button before something that can't be undone."),
				  LOCTEXT("HoldToConfirmOff", "A second press confirms."));
		Rows.Add(Hold);
		FSettingsRow Letters; // B-453f (D-190)
		Letters.Id = ESettingId::ElementLetters;
		Letters.Label = LOCTEXT("ElementLetters", "Element letters");
		Letters.Explanation = LOCTEXT("ElementLettersExplain", "Adds a letter to each element's colour, so elements can be told apart without colour: damage numbers read \"120 Fi\" and the status line names the element.");
		SetSwitch(Letters, Save && Save->IsElementLettersOn(), false, LOCTEXT("ElementLettersOn", "Damage numbers and status lines name the element."),
				  LOCTEXT("ElementLettersOff", "Colour only."));
		Rows.Add(Letters);
		Rows.Add(NotBuilt(ESettingId::Subtitles, LOCTEXT("Subtitles", "Subtitles"), LOCTEXT("SubtitlesExplain", "Shows spoken lines as text.")));
	}
	return Rows;
}

int32 WoRSettingsScreen::GetMostRows(const UWorld *World)
{
	int32 Most = 0;
	for (int32 Index = 0; Index < PAGE_COUNT; ++Index)
	{
		Most = FMath::Max(Most, BuildRows(World, static_cast<ESettingsPage>(Index)).Num());
	}
	return Most;
}

FText WoRSettingsScreen::GetPageNote(ESettingsPage Page)
{
	switch (Page)
	{
	case ESettingsPage::Camera:
	case ESettingsPage::Display:
	case ESettingsPage::Audio:
		return LOCTEXT("NothingYet", "Nothing to set here yet."); // TODO: these pages get their rows with their systems (B-125)
	case ESettingsPage::Controls:
		return LOCTEXT("ControlsNote", "Keys are fixed for now. Rebinding comes later.");
	default:
		return FText::GetEmpty();
	}
}

TArray<FSettingsKeyRow> WoRSettingsScreen::BuildKeyRows()
{
	using namespace SettingsScreenBuild;

	// The built rows of docs/Design/InputMap.md; a key added there gets its row here in the same change.
	// B-631: Battle first, as the settings v2 mockup's kind plate (Battle, Field, Menus).
	TArray<FSettingsKeyRow> Rows;
	Rows.Add(Heading(LOCTEXT("SectionBattle", "Battle")));
	Rows.Add(Key(LOCTEXT("KeyAttack", "Attack"), LOCTEXT("KeyAttackKB", "1"), LOCTEXT("KeyAttackPad", "A")));
	Rows.Add(Key(LOCTEXT("KeySkills", "Skills"), LOCTEXT("KeySkillsKB", "2"), LOCTEXT("KeySkillsPad", "X")));
	Rows.Add(Key(LOCTEXT("KeySpells", "Spells"), LOCTEXT("KeySpellsKB", "3"), LOCTEXT("KeySpellsPad", "B")));
	Rows.Add(Key(LOCTEXT("KeyItems", "Items"), LOCTEXT("KeyItemsKB", "4"), LOCTEXT("KeyItemsPad", "D-pad up")));
	Rows.Add(Key(LOCTEXT("KeyFlee", "Flee"), LOCTEXT("KeyFleeKB", "5"), LOCTEXT("KeyFleePad", "D-pad down")));
	Rows.Add(Key(LOCTEXT("KeySwitchWeapon", "Switch weapon"), LOCTEXT("KeySwitchWeaponKB", "6"), LOCTEXT("KeySwitchWeaponPad", "D-pad left")));
	Rows.Add(Key(LOCTEXT("KeyChangeRing", "Switch adapter"), LOCTEXT("KeyChangeRingKB", "7"), LOCTEXT("KeyChangeRingPad", "D-pad right")));
	Rows.Add(Key(LOCTEXT("KeyDefend", "Defend"), LOCTEXT("KeyDefendKB", "R"), LOCTEXT("KeyDefendPad", "RB"))); // B-627
	Rows.Add(Key(LOCTEXT("KeyEndTurn", "End turn"), LOCTEXT("KeyEndTurnKB", "G"), LOCTEXT("KeyEndTurnPad", "D-pad left")));
	Rows.Add(Key(LOCTEXT("KeyInfusionSource", "Infusion source"), LOCTEXT("KeyInfusionSourceKB", "R"), LOCTEXT("KeyInfusionSourcePad", "LB")));
	Rows.Add(Key(LOCTEXT("KeyInfusionLevel", "Infusion level"), LOCTEXT("KeyInfusionLevelKB", "T"), LOCTEXT("KeyInfusionLevelPad", "RB")));
	Rows.Add(Key(LOCTEXT("KeyIntel", "Intel (tap: party, enemies, close; hold: close)"), LOCTEXT("KeyIntelKB", "V"), LOCTEXT("KeyIntelPad", "R3")));
	Rows.Add(Key(LOCTEXT("KeyIntelStep", "Intel previous / next"), LOCTEXT("KeyIntelStepKB", "Q / E"), LOCTEXT("KeyIntelStepPad", "LB / RB")));
	Rows.Add(Key(LOCTEXT("KeySkipCinematic", "Skip a cinematic"), LOCTEXT("KeySkipCinematicKB", "Tab"), LOCTEXT("KeySkipCinematicPad", "Menu")));

	Rows.Add(Heading(LOCTEXT("SectionField", "Field")));
	Rows.Add(Key(LOCTEXT("KeyMove", "Move"), LOCTEXT("KeyMoveKB", "W A S D"), LOCTEXT("KeyMovePad", "Left stick")));
	Rows.Add(Key(LOCTEXT("KeyCamera", "Camera"), LOCTEXT("KeyCameraKB", "Mouse"), LOCTEXT("KeyCameraPad", "Right stick")));
	Rows.Add(Key(LOCTEXT("KeySprint", "Sprint"), LOCTEXT("KeySprintKB", "Left Shift (hold)"), LOCTEXT("KeySprintPad", "L3")));
	Rows.Add(Key(LOCTEXT("KeyInteract", "Interact"), LOCTEXT("KeyInteractKB", "E"), LOCTEXT("KeyInteractPad", "X")));
	Rows.Add(Key(LOCTEXT("KeyMenu", "Game menu"), LOCTEXT("KeyMenuKB", "Esc"), LOCTEXT("KeyMenuPad", "Menu")));

	Rows.Add(Heading(LOCTEXT("SectionMenus", "Menus")));
	Rows.Add(Key(LOCTEXT("KeyMenuMove", "Move"), LOCTEXT("KeyMenuMoveKB", "Arrows, W / S"), LOCTEXT("KeyMenuMovePad", "D-pad, left stick")));
	Rows.Add(Key(LOCTEXT("KeyMenuSelect", "Select"), LOCTEXT("KeyMenuSelectKB", "Enter, Space"), LOCTEXT("KeyMenuSelectPad", "A")));
	Rows.Add(Key(LOCTEXT("KeyMenuBack", "Back"), LOCTEXT("KeyMenuBackKB", "Backspace, Esc"), LOCTEXT("KeyMenuBackPad", "B, Menu")));
	Rows.Add(Key(LOCTEXT("KeyMenuPage", "Previous / next page"), LOCTEXT("KeyMenuPageKB", "Q / E"), LOCTEXT("KeyMenuPagePad", "LB / RB")));
	Rows.Add(Key(LOCTEXT("KeyMenuCharacter", "Previous / next character"), LOCTEXT("KeyMenuCharacterKB", "Z / C"), LOCTEXT("KeyMenuCharacterPad", "LT / RT")));
	Rows.Add(Key(LOCTEXT("KeyMenuChange", "Change"), LOCTEXT("KeyMenuChangeKB", "Left / Right, A / D"), LOCTEXT("KeyMenuChangePad", "D-pad left / right")));
	return Rows;
}

TArray<FText> WoRSettingsScreen::GetKeySections()
{
	TArray<FText> Sections;
	for (const FSettingsKeyRow &Row : BuildKeyRows())
	{
		if (Row.IsHeading())
		{
			Sections.Add(Row.Section);
		}
	}
	return Sections;
}

TArray<FSettingsRow> WoRSettingsScreen::BuildKeyPage(int32 Section)
{
	using namespace SettingsScreenBuild;

	TArray<FSettingsRow> Rows;
	int32 SectionIndex = INDEX_NONE;
	for (const FSettingsKeyRow &KeyRow : BuildKeyRows())
	{
		if (KeyRow.IsHeading())
		{
			++SectionIndex;
			continue;
		}
		if (SectionIndex != Section)
		{
			continue;
		}
		FSettingsRow Row;
		Row.Id = ESettingId::FixedKey;
		Row.Label = KeyRow.Action;
		Row.Value = FText::Format(LOCTEXT("KeyValue", "{0} / {1}"), KeyRow.Keyboard, KeyRow.Gamepad);
		Row.Refusal = GetPageNote(ESettingsPage::Controls);
		Row.Choices = {Choice(LOCTEXT("DeviceKeyboard", "Keyboard"), KeyRow.Keyboard, false), Choice(LOCTEXT("DeviceController", "Controller"), KeyRow.Gamepad, false)};
		Rows.Add(Row);
	}
	return Rows;
}

bool WoRSettingsScreen::SetSettingChoice(UWorld *World, ESettingId Id, int32 Choice)
{
	// The setting's row, from whichever page lists it (the battle controls row is on two).
	const auto FindRow = [World, Id](FSettingsRow &Out)
	{
		for (const ESettingsPage Page : {ESettingsPage::Gameplay, ESettingsPage::Accessibility})
		{
			for (FSettingsRow &Row : BuildRows(World, Page))
			{
				if (Row.Id == Id)
				{
					Out = MoveTemp(Row);
					return true;
				}
			}
		}
		return false;
	};
	FSettingsRow Row;
	if (!FindRow(Row) || !Row.CanChange() || !Row.Choices.IsValidIndex(Choice) || Row.Current == INDEX_NONE)
	{
		return false;
	}
	// One step at a time, so ChangeSetting stays the one place a setting changes; never more than a lap.
	const int32 Direction = Choice > Row.Current ? 1 : -1;
	bool bChanged = false;
	for (int32 Step = 0; Step < Row.Choices.Num() && Row.Current != Choice; ++Step)
	{
		if (!ChangeSetting(World, Id, Direction) || !FindRow(Row))
		{
			break;
		}
		bChanged = true;
	}
	UE_CLOG(Row.Current != Choice, LogWoRUI, Warning, TEXT("[Settings] %s: choice %d not reached (on %d)"), *Row.Label.ToString(), Choice, Row.Current);
	return bChanged;
}

bool WoRSettingsScreen::ChangeSetting(UWorld *World, ESettingId Id, int32 Direction)
{
	USaveSubsystem *Save = SettingsScreenBuild::FindSave(World);
	if (!Save)
	{
		return false;
	}
	switch (Id)
	{
	case ESettingId::TurnView:
	{
		const int32 StyleCount = static_cast<int32>(StaticEnum<ETurnViewStyle>()->NumEnums()) - 1; // the last is _MAX
		const int32 Current = static_cast<int32>(Save->GetTurnViewStyle());
		const int32 Step = Direction < 0 ? -1 : 1;
		Save->SetTurnViewStyle(static_cast<ETurnViewStyle>(((Current + Step) % StyleCount + StyleCount) % StyleCount));
		return true;
	}
	case ESettingId::EndTurnButton:
		Save->SetEndTurnButtonHidden(!Save->IsEndTurnButtonHidden());
		return true;
	case ESettingId::FightCameraMovement: // B-453b: the next fight reads it (UCombatCameraDirector::BeginCombat)
		Save->SetFightCameraMovementOff(!Save->IsFightCameraMovementOff());
		return true;
	case ESettingId::AIFinishesTurn: // B-453e: the next player turn reads it
		Save->SetAIFinishesIdleTurnOn(!Save->IsAIFinishesIdleTurnOn());
		return true;
	case ESettingId::ElementLetters: // B-453f: the next number or status line reads it
		Save->SetElementLettersOn(!Save->IsElementLettersOn());
		return true;
	case ESettingId::TextSize: // B-453d: Normal, Large, Larger, wrapping either way; applied at once
	{
		const int32 LevelCount = static_cast<int32>(StaticEnum<ETextSizeLevel>()->NumEnums()) - 1; // the last is _MAX
		const int32 Current = static_cast<int32>(Save->GetTextSizeLevel());
		const int32 Step = Direction < 0 ? -1 : 1;
		Save->SetTextSizeLevel(static_cast<ETextSizeLevel>(((Current + Step) % LevelCount + LevelCount) % LevelCount));
		return true;
	}
	case ESettingId::IntelKey: // B-587: the next press reads it (UBattleHUDWidget::PressIntelKey)
		Save->SetIntelKeyCycle(!Save->IsIntelKeyCycle());
		return true;
	case ESettingId::TurnOrderLayout: // B-590: the next turn-order refresh reads it
		Save->SetTurnOrderColumn(!Save->IsTurnOrderColumn());
		return true;
	case ESettingId::TurnsShown: // B-590: 1 to 6, stopping at the ends (a count, not a list to wrap)
	{
		const int32 Current = Save->GetTurnsShown();
		const int32 Next = FMath::Clamp(Current + (Direction < 0 ? -1 : 1), BattleHUDView::MIN_TURNS_SHOWN, BattleHUDView::MAX_TURNS_SHOWN);
		if (Next == Current)
		{
			return false;
		}
		Save->SetTurnsShown(Next);
		return true;
	}
	case ESettingId::HoldToConfirm: // B-453c
		Save->SetHoldToConfirmOn(!Save->IsHoldToConfirmOn());
		return true;
	case ESettingId::ScreenShake: // B-453a: Full, Reduced, Off, wrapping either way
	{
		const int32 LevelCount = static_cast<int32>(StaticEnum<EScreenShakeLevel>()->NumEnums()) - 1; // the last is _MAX
		const int32 Current = static_cast<int32>(Save->GetScreenShakeLevel());
		const int32 Step = Direction < 0 ? -1 : 1;
		Save->SetScreenShakeLevel(static_cast<EScreenShakeLevel>(((Current + Step) % LevelCount + LevelCount) % LevelCount));
		return true;
	}
	default: // not built yet (BuildRows says so)
		return false;
	}
}

void WoRSettingsScreen::SaveChanges(UWorld *World)
{
	// A full save, as a rest spot's: the settings ride in the one save file (FSavedSettings).
	USaveSubsystem *Save = SettingsScreenBuild::FindSave(World);
	if (Save && !Save->SaveNow(World, /*bAsync=*/true))
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[Settings] Not written: no local player owns the save slot yet. The change holds until the game closes."));
	}
}

FString WoRSettingsScreen::GetSettingsString(const UWorld *World)
{
	FString Out = TEXT("[Settings]");
	for (int32 PageIndex = 0; PageIndex < PAGE_COUNT; ++PageIndex)
	{
		const ESettingsPage Page = static_cast<ESettingsPage>(PageIndex);
		Out += FString::Printf(TEXT("\n  %s:"), *GetPageName(Page).ToString());
		for (const FSettingsRow &Row : BuildRows(World, Page))
		{
			Out += FString::Printf(TEXT("\n    %s = %s"), *Row.Label.ToString(), Row.CanChange() ? *Row.Value.ToString() : TEXT("(not built)"));
			if (Row.Choices.Num() > 0)
			{
				Out += FString::Printf(TEXT(" (choice %d of %d, default %d)"), Row.Current + 1, Row.Choices.Num(), Row.GetDefaultChoice() + 1);
			}
		}
		const FText Note = GetPageNote(Page);
		if (!Note.IsEmpty())
		{
			Out += FString::Printf(TEXT("\n    %s"), *Note.ToString());
		}
	}
	Out += FString::Printf(TEXT("\n  %d key rows on Controls"), BuildKeyRows().Num());
	return Out;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GSettingsPrintCommand(
	TEXT("wor.UI.PrintSettings"),
	TEXT("DEBUG (B-452): every settings page's rows and their values."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		UE_LOG(LogWoRUI, Display, TEXT("%s"), *WoRSettingsScreen::GetSettingsString(World));
	}));
#endif

#undef LOCTEXT_NAMESPACE
