// SettingsScreenModel.cpp

#include "UI/Menus/SettingsScreenModel.h"

#include "Combat/Camera/CombatCameraTypes.h"
#include "Combat/CombatOrchestrator.h"
#include "UI/Settings/TextSizeSetting.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Save/SaveSubsystem.h"
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
		TurnView.Value = FCombatCameraRules::GetTurnViewStyleName(Save ? Save->GetTurnViewStyle() : ETurnViewStyle::CentredWide);
		TurnView.Explanation = LOCTEXT("TurnViewExplain", "Where the camera sits on your turn in a fight: behind and above the character, over their shoulder, or from the side.");
		Rows.Add(TurnView);

		FSettingsRow EndTurn;
		EndTurn.Id = ESettingId::EndTurnButton;
		EndTurn.Label = LOCTEXT("EndTurnButton", "End Turn button");
		EndTurn.Value = OnOff(!Save || !Save->IsEndTurnButtonHidden());
		EndTurn.Explanation = LOCTEXT("EndTurnExplain", "Shows the End Turn button on the battle menu. Off hides it.");
		Rows.Add(EndTurn);

		Rows.Add(NotBuilt(ESettingId::CornerHints, LOCTEXT("CornerHints", "Button hints in the corner"),
						  LOCTEXT("CornerHintsExplain", "Shows which button does what, in the corner of the screen.")));
		Rows.Add(NotBuilt(ESettingId::BossCinematics, LOCTEXT("BossCinematics", "Boss cinematics"),
						  LOCTEXT("BossCinematicsExplain", "Plays the short scene when a great foe appears or changes phase. They can always be skipped.")));
		FSettingsRow Idle; // B-453e (D-190)
		Idle.Id = ESettingId::AIFinishesTurn;
		Idle.Label = LOCTEXT("AIFinishesTurn", "AI finishes my turn when I idle");
		Idle.Value = OnOff(Save && Save->IsAIFinishesIdleTurnOn());
		Idle.Explanation = FText::Format(LOCTEXT("AIFinishesTurnExplain", "If you don't act for {0} seconds, the AI plays the rest of your turn. Your next turn is yours again."),
										 FText::AsNumber(FMath::RoundToInt32(ACombatOrchestrator::GetIdleTurnSeconds())));
		Rows.Add(Idle);
		Rows.Add(NotBuilt(ESettingId::TutorialTips, LOCTEXT("TutorialTips", "Tutorial tips"),
						  LOCTEXT("TutorialTipsExplain", "Short tips the first time something new comes up.")));
	}
	else if (Page == ESettingsPage::Accessibility)
	{
		Rows.Add(BattleControlsRow()); // repeated here on purpose: people look for it in both places
		FSettingsRow Text; // B-453d (D-190)
		Text.Id = ESettingId::TextSize;
		Text.Label = WoRTextSize::GetSettingName();
		Text.Value = WoRTextSize::GetLevelName(Save ? Save->GetTextSizeLevel() : ETextSizeLevel::Normal);
		Text.Explanation = LOCTEXT("TextSizeExplain", "Makes the text in menus and in fights larger: the whole screen's text and panels grow together.");
		Rows.Add(Text);
		FSettingsRow Movement; // B-453b (D-190, D-078)
		Movement.Id = ESettingId::FightCameraMovement;
		Movement.Label = LOCTEXT("FightCameraMovement", "Camera movement in fights");
		Movement.Value = OnOff(!Save || !Save->IsFightCameraMovementOff());
		Movement.Explanation = LOCTEXT("FightCameraMovementExplain", "Off keeps the camera still during attacks, for anyone the movement makes unwell.");
		Rows.Add(Movement);
		FSettingsRow Shake; // B-453a (D-190)
		Shake.Id = ESettingId::ScreenShake;
		Shake.Label = FCombatCameraRules::GetScreenShakeSettingName();
		Shake.Value = FCombatCameraRules::GetScreenShakeLevelName(Save ? Save->GetScreenShakeLevel() : EScreenShakeLevel::Full);
		Shake.Explanation = LOCTEXT("ScreenShakeExplain", "How much the screen shakes on heavy hits: full, reduced to half, or off.");
		Rows.Add(Shake);
		FSettingsRow Hold; // B-453c (D-190)
		Hold.Id = ESettingId::HoldToConfirm;
		Hold.Label = LOCTEXT("HoldToConfirm", "Hold to confirm dangerous actions");
		Hold.Value = OnOff(!Save || Save->IsHoldToConfirmOn());
		Hold.Explanation = LOCTEXT("HoldToConfirmExplain", "Asks you to hold the button, not press it, before something that can't be undone. Off: a second press confirms.");
		Rows.Add(Hold);
		FSettingsRow Letters; // B-453f (D-190)
		Letters.Id = ESettingId::ElementLetters;
		Letters.Label = LOCTEXT("ElementLetters", "Element letters on colours");
		Letters.Value = OnOff(Save && Save->IsElementLettersOn());
		Letters.Explanation = LOCTEXT("ElementLettersExplain", "Adds a letter to each element's colour, so elements can be told apart without colour: damage numbers read \"120 Fi\" and the status line names the element.");
		Rows.Add(Letters);
		Rows.Add(NotBuilt(ESettingId::Subtitles, LOCTEXT("Subtitles", "Subtitles"), LOCTEXT("SubtitlesExplain", "Shows spoken lines as text.")));
	}
	return Rows;
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
		return LOCTEXT("ControlsNote", "Keys are fixed for now. Rebinding comes later; every action already has its own name so it can.");
	default:
		return FText::GetEmpty();
	}
}

TArray<FSettingsKeyRow> WoRSettingsScreen::BuildKeyRows()
{
	using namespace SettingsScreenBuild;

	// The built rows of docs/Design/InputMap.md; a key added there gets its row here in the same change.
	TArray<FSettingsKeyRow> Rows;
	Rows.Add(Heading(LOCTEXT("SectionField", "Field")));
	Rows.Add(Key(LOCTEXT("KeyMove", "Move"), LOCTEXT("KeyMoveKB", "W A S D"), LOCTEXT("KeyMovePad", "Left stick")));
	Rows.Add(Key(LOCTEXT("KeyCamera", "Camera"), LOCTEXT("KeyCameraKB", "Mouse"), LOCTEXT("KeyCameraPad", "Right stick")));
	Rows.Add(Key(LOCTEXT("KeySprint", "Sprint"), LOCTEXT("KeySprintKB", "Left Shift (hold)"), LOCTEXT("KeySprintPad", "L3")));
	Rows.Add(Key(LOCTEXT("KeyInteract", "Interact"), LOCTEXT("KeyInteractKB", "E"), LOCTEXT("KeyInteractPad", "X")));
	Rows.Add(Key(LOCTEXT("KeyMenu", "Game menu"), LOCTEXT("KeyMenuKB", "Esc"), LOCTEXT("KeyMenuPad", "Menu")));

	Rows.Add(Heading(LOCTEXT("SectionBattle", "Battle")));
	Rows.Add(Key(LOCTEXT("KeyAttack", "Attack"), LOCTEXT("KeyAttackKB", "1"), LOCTEXT("KeyAttackPad", "A")));
	Rows.Add(Key(LOCTEXT("KeySkills", "Skills"), LOCTEXT("KeySkillsKB", "2"), LOCTEXT("KeySkillsPad", "X")));
	Rows.Add(Key(LOCTEXT("KeySpells", "Spells"), LOCTEXT("KeySpellsKB", "3"), LOCTEXT("KeySpellsPad", "B")));
	Rows.Add(Key(LOCTEXT("KeyItems", "Items"), LOCTEXT("KeyItemsKB", "4"), LOCTEXT("KeyItemsPad", "D-pad up")));
	Rows.Add(Key(LOCTEXT("KeyFlee", "Flee"), LOCTEXT("KeyFleeKB", "5"), LOCTEXT("KeyFleePad", "D-pad down")));
	Rows.Add(Key(LOCTEXT("KeySwitchWeapon", "Switch weapon"), LOCTEXT("KeySwitchWeaponKB", "6"), LOCTEXT("KeySwitchWeaponPad", "D-pad left")));
	Rows.Add(Key(LOCTEXT("KeyChangeRing", "Change adapter"), LOCTEXT("KeyChangeRingKB", "7"), LOCTEXT("KeyChangeRingPad", "D-pad right")));
	Rows.Add(Key(LOCTEXT("KeyDefend", "Defend"), LOCTEXT("KeyDefendKB", "G"), LOCTEXT("KeyDefendPad", "Y")));
	Rows.Add(Key(LOCTEXT("KeyInfusionSource", "Infusion source"), LOCTEXT("KeyInfusionSourceKB", "R"), LOCTEXT("KeyInfusionSourcePad", "LB")));
	Rows.Add(Key(LOCTEXT("KeyInfusionLevel", "Infusion level"), LOCTEXT("KeyInfusionLevelKB", "T"), LOCTEXT("KeyInfusionLevelPad", "RB")));
	Rows.Add(Key(LOCTEXT("KeySkipCinematic", "Skip a cinematic"), LOCTEXT("KeySkipCinematicKB", "Tab"), LOCTEXT("KeySkipCinematicPad", "Menu")));

	Rows.Add(Heading(LOCTEXT("SectionMenus", "Menus")));
	Rows.Add(Key(LOCTEXT("KeyMenuMove", "Move"), LOCTEXT("KeyMenuMoveKB", "Arrows, W / S"), LOCTEXT("KeyMenuMovePad", "D-pad, left stick")));
	Rows.Add(Key(LOCTEXT("KeyMenuSelect", "Select"), LOCTEXT("KeyMenuSelectKB", "Enter, Space"), LOCTEXT("KeyMenuSelectPad", "A")));
	Rows.Add(Key(LOCTEXT("KeyMenuBack", "Back"), LOCTEXT("KeyMenuBackKB", "Backspace, Esc"), LOCTEXT("KeyMenuBackPad", "B, Menu")));
	Rows.Add(Key(LOCTEXT("KeyMenuPage", "Previous / next page"), LOCTEXT("KeyMenuPageKB", "Q / E"), LOCTEXT("KeyMenuPagePad", "LB / RB")));
	Rows.Add(Key(LOCTEXT("KeyMenuCharacter", "Previous / next character"), LOCTEXT("KeyMenuCharacterKB", "Z / C"), LOCTEXT("KeyMenuCharacterPad", "LT / RT")));
	Rows.Add(Key(LOCTEXT("KeyMenuChange", "Change a setting"), LOCTEXT("KeyMenuChangeKB", "Left / Right, A / D"), LOCTEXT("KeyMenuChangePad", "D-pad left / right")));
	return Rows;
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
