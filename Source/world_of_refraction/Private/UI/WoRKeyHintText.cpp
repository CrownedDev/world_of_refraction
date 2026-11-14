// WoRKeyHintText.cpp

#include "UI/WoRKeyHintText.h"

#include "CommonInputSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRKeyHints"

namespace WoRKeyHintsTable
{
	/** One key name and what it reads on each device. TODO: glyphs per pad brand with the CommonUI input data. */
	struct FKeyName
	{
		const TCHAR *Name;
		FText Gamepad;
		FText Keyboard;
	};

	TArray<FKeyName> BuildTable()
	{
		// InputMap.md, Menus. The gamepad side uses the Xbox letters the footers always used.
		return {
			{TEXT("Confirm"), LOCTEXT("ConfirmPad", "A"), LOCTEXT("ConfirmKey", "Enter")},
			{TEXT("Back"), LOCTEXT("BackPad", "B"), LOCTEXT("BackKey", "Backspace")},
			{TEXT("Toggle"), LOCTEXT("TogglePad", "X"), LOCTEXT("ToggleKey", "X")},
			{TEXT("Second"), LOCTEXT("SecondPad", "Y"), LOCTEXT("SecondKey", "R")},
			{TEXT("Tabs"), LOCTEXT("TabsPad", "LB / RB"), LOCTEXT("TabsKey", "Q / E")},
			{TEXT("TabPrev"), LOCTEXT("TabPrevPad", "LB"), LOCTEXT("TabPrevKey", "Q")},
			{TEXT("TabNext"), LOCTEXT("TabNextPad", "RB"), LOCTEXT("TabNextKey", "E")},
			{TEXT("SubTabs"), LOCTEXT("SubTabsPad", "LT / RT"), LOCTEXT("SubTabsKey", "Z / C")},
			{TEXT("SubTabPrev"), LOCTEXT("SubTabPrevPad", "LT"), LOCTEXT("SubTabPrevKey", "Z")},
			{TEXT("SubTabNext"), LOCTEXT("SubTabNextPad", "RT"), LOCTEXT("SubTabNextKey", "C")},
			{TEXT("SetActive"), LOCTEXT("SetActivePad", "L3"), LOCTEXT("SetActiveKey", "6")},
			{TEXT("Arrows"), LOCTEXT("ArrowsPad", "D-pad"), LOCTEXT("ArrowsKey", "Arrows")},
			{TEXT("Interact"), LOCTEXT("InteractPad", "X"), LOCTEXT("InteractKey", "E")},
			{TEXT("ViewButton"), LOCTEXT("ViewButtonPad", "View"), LOCTEXT("ViewButtonKey", "Tab")},
		};
	}
}

bool WoRKeyHints::IsGamepad(const UWidget *Context)
{
	const UCommonInputSubsystem *Input = Context ? UCommonInputSubsystem::Get(Context->GetOwningLocalPlayer()) : nullptr;
	return Input && Input->GetCurrentInputType() == ECommonInputType::Gamepad;
}

FFormatNamedArguments WoRKeyHints::GetKeyNames(bool bGamepad)
{
	FFormatNamedArguments Names;
	for (const WoRKeyHintsTable::FKeyName &Key : WoRKeyHintsTable::BuildTable())
	{
		Names.Add(Key.Name, bGamepad ? Key.Gamepad : Key.Keyboard);
	}
	return Names;
}

FText WoRKeyHints::Format(const FText &Pattern, bool bGamepad, const FFormatNamedArguments &Extra)
{
	FFormatNamedArguments Arguments = GetKeyNames(bGamepad);
	Arguments.Append(Extra);
	return FText::Format(Pattern, Arguments);
}

void WoRKeyHints::SetHint(UTextBlock *Text, const FText &Pattern, const FFormatNamedArguments &Extra)
{
	if (UWoRKeyHintText *Hint = Cast<UWoRKeyHintText>(Text))
	{
		Hint->SetHint(Pattern, Extra);
	}
	else if (Text)
	{
		Text->SetText(Format(Pattern, IsGamepad(Text), Extra));
	}
}

FString WoRKeyHints::GetKeyNamesString()
{
	FString Out = TEXT("Key hints (name: gamepad / keyboard):");
	for (const WoRKeyHintsTable::FKeyName &Key : WoRKeyHintsTable::BuildTable())
	{
		Out += FString::Printf(TEXT("\n  {%s}: %s / %s"), Key.Name, *Key.Gamepad.ToString(), *Key.Keyboard.ToString());
	}
	return Out;
}

void UWoRKeyHintText::SetHint(const FText &InPattern, const FFormatNamedArguments &InExtra)
{
	Pattern = InPattern;
	Extra = InExtra;
	ShowHint();
}

void UWoRKeyHintText::ShowHint()
{
	SetText(Pattern.IsEmpty() ? FText::GetEmpty() : WoRKeyHints::Format(Pattern, WoRKeyHints::IsGamepad(this), Extra));
}

TSharedRef<SWidget> UWoRKeyHintText::RebuildWidget()
{
	TSharedRef<SWidget> Widget = Super::RebuildWidget();
	if (UCommonInputSubsystem *Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		Input->OnInputMethodChangedNative.Remove(InputChangedHandle); // rebuilt: one binding, not two
		InputChangedHandle = Input->OnInputMethodChangedNative.AddUObject(this, &UWoRKeyHintText::HandleInputMethodChanged);
	}
	ShowHint(); // the pattern may have been set before the player was known
	return Widget;
}

void UWoRKeyHintText::ReleaseSlateResources(bool bReleaseChildren)
{
	if (UCommonInputSubsystem *Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		Input->OnInputMethodChangedNative.Remove(InputChangedHandle);
	}
	InputChangedHandle.Reset();
	Super::ReleaseSlateResources(bReleaseChildren);
}

void UWoRKeyHintText::HandleInputMethodChanged(ECommonInputType NewInputType)
{
	ShowHint();
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GKeyHintsPrintCommand(
	TEXT("wor.UI.KeyHints.Print"),
	TEXT("B-481: print every menu key name on both devices, and the device the first player last used."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const ULocalPlayer *Player = World ? World->GetFirstLocalPlayerFromController() : nullptr;
		const UCommonInputSubsystem *Input = UCommonInputSubsystem::Get(Player);
		UE_LOG(LogWoRUI, Display, TEXT("[KeyHints] device in use: %s\n%s"),
			   Input && Input->GetCurrentInputType() == ECommonInputType::Gamepad ? TEXT("gamepad") : TEXT("keyboard / mouse"),
			   *WoRKeyHints::GetKeyNamesString());
	}));
#endif

#undef LOCTEXT_NAMESPACE
