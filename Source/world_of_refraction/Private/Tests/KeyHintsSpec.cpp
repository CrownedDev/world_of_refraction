// KeyHintsSpec.cpp
// B-481: menu key hints. A pattern reads gamepad buttons or keyboard keys; Back is Backspace on the keyboard and
// Backspace is a Back key; an unknown name is left as written; a caller's own argument is filled; with no player
// (headless) a hint text shows the keyboard keys.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRKeyHintText.h"
#include "UI/WoRTestScreen.h"
#include "WoRTestWorld.h"

BEGIN_DEFINE_SPEC(FKeyHintsSpec, "WoR.UI.KeyHints", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FKeyHintsSpec)

void FKeyHintsSpec::Define()
{
	It("fills a pattern with the gamepad's buttons or the keyboard's keys", [this]()
	{
		const FText Pattern = FText::FromString(TEXT("{Confirm}  Select        {Second}  Options        {Tabs}  Tab        {Back}  Back"));
		TestEqual(TEXT("gamepad"), WoRKeyHints::Format(Pattern, true).ToString(), FString(TEXT("A  Select        Y  Options        LB / RB  Tab        B  Back")));
		TestEqual(TEXT("keyboard"), WoRKeyHints::Format(Pattern, false).ToString(),
				  FString(TEXT("Enter  Select        R  Options        Q / E  Tab        Backspace  Back")));
	});

	It("names every key differently on the two devices, except the shared X", [this]()
	{
		const FFormatNamedArguments Pad = WoRKeyHints::GetKeyNames(true);
		const FFormatNamedArguments Keys = WoRKeyHints::GetKeyNames(false);
		TestEqual(TEXT("the same names on both"), Pad.Num(), Keys.Num());
		for (const TPair<FString, FFormatArgumentValue> &Name : Pad)
		{
			const FText Pattern = FText::FromString(FString::Printf(TEXT("{%s}"), *Name.Key));
			const FString OnPad = WoRKeyHints::Format(Pattern, true).ToString();
			const FString OnKeys = WoRKeyHints::Format(Pattern, false).ToString();
			TestFalse(*FString::Printf(TEXT("%s has a gamepad name"), *Name.Key), OnPad.IsEmpty() || OnPad.Contains(TEXT("{")));
			TestFalse(*FString::Printf(TEXT("%s has a keyboard name"), *Name.Key), OnKeys.IsEmpty() || OnKeys.Contains(TEXT("{")));
			if (Name.Key != TEXT("Toggle")) // X on both (WoRMenuStyle::IsToggleKey)
			{
				TestNotEqual(*FString::Printf(TEXT("%s differs"), *Name.Key), OnPad, OnKeys);
			}
		}
	});

	It("keeps Backspace and Esc as Back keys", [this]()
	{
		TestTrue(TEXT("Backspace"), WoRMenuStyle::IsBackKey(EKeys::BackSpace));
		TestTrue(TEXT("Esc still works"), WoRMenuStyle::IsBackKey(EKeys::Escape));
		TestTrue(TEXT("gamepad B"), WoRMenuStyle::IsBackKey(EKeys::Gamepad_FaceButton_Right));
	});

	It("fills the caller's own argument and leaves an unknown name as written", [this]()
	{
		FFormatNamedArguments Extra;
		Extra.Add(TEXT("ViewName"), FText::FromString(TEXT("Simple")));
		TestEqual(TEXT("own argument"), WoRKeyHints::Format(FText::FromString(TEXT("{Toggle}  {ViewName}")), true, Extra).ToString(), FString(TEXT("X  Simple")));
		TestEqual(TEXT("unknown name"), WoRKeyHints::Format(FText::FromString(TEXT("{Nope}  Thing")), true).ToString(), FString(TEXT("{Nope}  Thing")));
		// A text filled first with the caller's NAMED argument keeps its key names for the hint (the screens rely on it).
		// A numbered argument ({0}) would not: it fills the first name's place, so mixed patterns use FormatNamed.
		const FText Once = FText::FormatNamed(FText::FromString(TEXT("{Confirm} again: {What} comes off")), TEXT("What"), FText::FromString(TEXT("Garnet")));
		TestEqual(TEXT("two passes"), WoRKeyHints::Format(Once, false).ToString(), FString(TEXT("Enter again: Garnet comes off")));
		// A name the player typed with braces shows as typed, not as a key.
		const FText Named = FText::Format(FText::FromString(TEXT("{0} is now in use.")), WoRKeyHints::Literal(FText::FromString(TEXT("My {Back} `set`"))));
		TestEqual(TEXT("a typed name keeps its braces"), WoRKeyHints::Format(Named, false).ToString(), FString(TEXT("My {Back} `set` is now in use.")));
	});

	It("shows the keyboard's keys on a hint text with no player (headless), and fills a plain text once", [this]()
	{
		FWoRTestWorld World;
		if (!TestTrue(TEXT("world"), World.Create()))
		{
			return;
		}
		UWoRTestScreen *Screen = CreateWidget<UWoRTestScreen>(World.GetWorld(), UWoRTestScreen::StaticClass());
		if (!TestNotNull(TEXT("a screen to own the texts"), Screen) || !TestNotNull(TEXT("its tree"), Screen->WidgetTree.Get()))
		{
			return;
		}
		const FText Pattern = FText::FromString(TEXT("{Back}  Back"));
		UTextBlock *Hint = WoRMenuStyle::MakeHintText(Screen->WidgetTree, WoRMenuStyle::LineFontSize, WoRMenuStyle::DimColor);
		WoRKeyHints::SetHint(Hint, Pattern);
		TestEqual(TEXT("a hint text"), Hint->GetText().ToString(), FString(TEXT("Backspace  Back")));
		UTextBlock *Plain = WoRMenuStyle::MakeText(Screen->WidgetTree, WoRMenuStyle::LineFontSize, WoRMenuStyle::DimColor);
		WoRKeyHints::SetHint(Plain, Pattern);
		TestEqual(TEXT("a plain text"), Plain->GetText().ToString(), FString(TEXT("Backspace  Back")));
		UTextBlock *Label = WoRMenuStyle::MakeKeyLabel(Screen->WidgetTree, FText::FromString(TEXT("{SubTabPrev}")));
		TestEqual(TEXT("a key label"), Label->GetText().ToString(), FString(TEXT("Z")));
		WoRKeyHints::SetHint(nullptr, Pattern); // no text: nothing happens
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
