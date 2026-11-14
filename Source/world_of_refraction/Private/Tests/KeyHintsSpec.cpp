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

	It("reads a footer as key caps with their labels, a pair as two caps, a sentence as text (B-483)", [this]()
	{
		const FText Pattern = FText::FromString(TEXT("{Confirm}  Edit        {Tabs}  Tab        Up / Down  Choose        {Confirm} / Right  Add        {Back}  Back"));
		const TArray<FWoRKeyHint> Keys = WoRKeyHints::ParseHints(Pattern, false);
		if (!TestEqual(TEXT("five hints"), Keys.Num(), 5))
		{
			return;
		}
		TestEqual(TEXT("a cap and its label"), WoRKeyHints::GetHintsString({Keys[0]}), FString(TEXT("[Enter -> Enter] Edit")));
		TestTrue(TEXT("Tabs is its two caps"), Keys[1].Caps.Num() == 2 && Keys[1].Caps[0].Text.ToString() == TEXT("Q") && Keys[1].Caps[1].Text.ToString() == TEXT("E"));
		TestTrue(TEXT("each of the pair clicks its own key"), Keys[1].Caps[0].ClickKey == EKeys::Q && Keys[1].Caps[1].ClickKey == EKeys::E);
		TestTrue(TEXT("the pair's label clicks next"), Keys[1].GetClickKey() == EKeys::E);
		TestTrue(TEXT("words are caps too, and Up / Down click the arrows"), Keys[2].Caps.Num() == 2 && Keys[2].Caps[0].ClickKey == EKeys::Up && Keys[2].Caps[1].ClickKey == EKeys::Down);
		TestTrue(TEXT("a key name beside a word"), Keys[3].Caps.Num() == 2 && Keys[3].Caps[0].ClickKey == EKeys::Enter && Keys[3].Caps[1].ClickKey == EKeys::Right);
		TestTrue(TEXT("Back is Backspace, and clicks it"), Keys[4].Caps[0].Text.ToString() == TEXT("Backspace") && Keys[4].GetClickKey() == EKeys::BackSpace);
		TestTrue(TEXT("keyboard caps are framed"), Keys[0].Caps[0].Style == EWoRKeyCapStyle::Framed);

		const TArray<FWoRKeyHint> Pad = WoRKeyHints::ParseHints(Pattern, true);
		TestTrue(TEXT("on a gamepad A is the green circle"), Pad[0].Caps[0].Text.ToString() == TEXT("A") && Pad[0].Caps[0].Style == EWoRKeyCapStyle::FaceA);
		TestTrue(TEXT("B the red one"), Pad[4].Caps[0].Style == EWoRKeyCapStyle::FaceB);
		TestTrue(TEXT("the shoulders are framed caps"), Pad[1].Caps[0].Text.ToString() == TEXT("LB") && Pad[1].Caps[0].Style == EWoRKeyCapStyle::Framed);

		// A tab row's key label: one cap, no label. A sentence, or a lone word, stays text.
		const TArray<FWoRKeyHint> Label = WoRKeyHints::ParseHints(FText::FromString(TEXT("{SubTabPrev}")), false);
		TestTrue(TEXT("a key label is one cap"), Label.Num() == 1 && Label[0].Caps.Num() == 1 && Label[0].Label.IsEmpty() && Label[0].Caps[0].ClickKey == EKeys::Z);
		const TArray<FWoRKeyHint> Sentence = WoRKeyHints::ParseHints(FText::FromString(TEXT("Hold {ViewButton} to answer  ·  then  {Confirm}  Give this seat")), false);
		TestTrue(TEXT("a sentence has no caps"), Sentence.Num() == 1 && Sentence[0].Caps.IsEmpty() && Sentence[0].Label.ToString().StartsWith(TEXT("Hold Tab to answer")));
		const TArray<FWoRKeyHint> Word = WoRKeyHints::ParseHints(FText::FromString(TEXT("Cancel")), false);
		TestTrue(TEXT("a lone word is text"), Word.Num() == 1 && Word[0].Caps.IsEmpty() && Word[0].Label.ToString() == TEXT("Cancel"));

		// The caller's own value in a label is filled; a typed key (the rename box) is a cap that clicks nothing.
		FFormatNamedArguments Extra;
		Extra.Add(TEXT("ViewName"), FText::FromString(TEXT("Simple")));
		const TArray<FWoRKeyHint> View = WoRKeyHints::ParseHints(FText::FromString(TEXT("{Toggle}  {ViewName}        Esc  Cancel")), false, Extra);
		TestTrue(TEXT("the label's own value"), View.Num() == 2 && View[0].Label.ToString() == TEXT("Simple"));
		TestTrue(TEXT("a typed key is a cap that clicks nothing"), View[1].Caps.Num() == 1 && !View[1].GetClickKey().IsValid());
	});

	It("draws a footer made by MakeHintBar as caps, and a key label too; a hint text stays text (B-483)", [this]()
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
		UWoRKeyHintText *Bar = Cast<UWoRKeyHintText>(WoRMenuStyle::MakeHintBar(Screen->WidgetTree));
		if (!TestNotNull(TEXT("the bar"), Bar))
		{
			return;
		}
		WoRKeyHints::SetHint(Bar, FText::FromString(TEXT("{Confirm}  Select        {Back}  Back")));
		TestTrue(TEXT("a footer has caps"), Bar->HasKeyCaps());
		TestEqual(TEXT("its hints"), WoRKeyHints::GetHintsString(Bar->GetHints()), FString(TEXT("[Enter -> Enter] Select\n[Backspace -> BackSpace] Back")));
		TestEqual(TEXT("its text is still readable"), Bar->GetText().ToString(), FString(TEXT("Enter  Select        Backspace  Back")));
		const UWoRKeyHintText *KeyLabel = Cast<UWoRKeyHintText>(WoRMenuStyle::MakeKeyLabel(Screen->WidgetTree, FText::FromString(TEXT("{TabNext}"))));
		TestTrue(TEXT("a key label has a cap"), KeyLabel && KeyLabel->HasKeyCaps() && KeyLabel->GetHints().Num() == 1);
		const UWoRKeyHintText *Text = Cast<UWoRKeyHintText>(WoRMenuStyle::MakeHintText(Screen->WidgetTree, WoRMenuStyle::LineFontSize, WoRMenuStyle::DimColor));
		TestTrue(TEXT("a hint text has none"), Text && !Text->HasKeyCaps());
		Bar->ClickKey(EKeys::Invalid); // no key: nothing is sent
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
