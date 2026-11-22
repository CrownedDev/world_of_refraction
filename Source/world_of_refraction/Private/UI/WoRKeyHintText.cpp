// WoRKeyHintText.cpp

#include "UI/WoRKeyHintText.h"

#include "Blueprint/UserWidget.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "CommonInputSubsystem.h"
#include "Containers/Ticker.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Styling/StyleDefaults.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRKeyHints"

namespace WoRKeyHintsTable
{
	/** One key name: what it reads on each device, how its gamepad cap is drawn (B-483), and the key a click on it
	 *  sends to the screen (invalid: a click does nothing). TODO: glyphs per pad brand with the CommonUI input data. */
	struct FKeyName
	{
		const TCHAR *Name;
		FText Gamepad;
		FText Keyboard;
		EWoRKeyCapStyle GamepadStyle = EWoRKeyCapStyle::Framed;
		FKey ClickKey;
	};

	TArray<FKeyName> BuildTable()
	{
		// InputMap.md, Menus. The gamepad side uses the Xbox letters the footers always used. A click sends the
		// keyboard key (a click is a mouse input, so the keyboard's keys are the ones shown when it lands).
		return {
			{TEXT("Confirm"), LOCTEXT("ConfirmPad", "A"), LOCTEXT("ConfirmKey", "Enter"), EWoRKeyCapStyle::FaceA, EKeys::Enter},
			{TEXT("Back"), LOCTEXT("BackPad", "B"), LOCTEXT("BackKey", "Backspace"), EWoRKeyCapStyle::FaceB, EKeys::BackSpace},
			{TEXT("Toggle"), LOCTEXT("TogglePad", "X"), LOCTEXT("ToggleKey", "X"), EWoRKeyCapStyle::FaceX, EKeys::X},
			{TEXT("Second"), LOCTEXT("SecondPad", "Y"), LOCTEXT("SecondKey", "R"), EWoRKeyCapStyle::FaceY, EKeys::R},
			{TEXT("Tabs"), LOCTEXT("TabsPad", "LB / RB"), LOCTEXT("TabsKey", "Q / E"), EWoRKeyCapStyle::Framed, EKeys::E},
			{TEXT("TabPrev"), LOCTEXT("TabPrevPad", "LB"), LOCTEXT("TabPrevKey", "Q"), EWoRKeyCapStyle::Framed, EKeys::Q},
			{TEXT("TabNext"), LOCTEXT("TabNextPad", "RB"), LOCTEXT("TabNextKey", "E"), EWoRKeyCapStyle::Framed, EKeys::E},
			{TEXT("SubTabs"), LOCTEXT("SubTabsPad", "LT / RT"), LOCTEXT("SubTabsKey", "Z / C"), EWoRKeyCapStyle::Framed, EKeys::C},
			{TEXT("SubTabPrev"), LOCTEXT("SubTabPrevPad", "LT"), LOCTEXT("SubTabPrevKey", "Z"), EWoRKeyCapStyle::Framed, EKeys::Z},
			{TEXT("SubTabNext"), LOCTEXT("SubTabNextPad", "RT"), LOCTEXT("SubTabNextKey", "C"), EWoRKeyCapStyle::Framed, EKeys::C},
			{TEXT("SetActive"), LOCTEXT("SetActivePad", "L3"), LOCTEXT("SetActiveKey", "6"), EWoRKeyCapStyle::Framed, EKeys::Six},
			{TEXT("Arrows"), LOCTEXT("ArrowsPad", "D-pad"), LOCTEXT("ArrowsKey", "Arrows"), EWoRKeyCapStyle::Framed, EKeys::Invalid},
			{TEXT("Interact"), LOCTEXT("InteractPad", "X"), LOCTEXT("InteractKey", "E"), EWoRKeyCapStyle::FaceX, EKeys::Invalid},
			{TEXT("ViewButton"), LOCTEXT("ViewButtonPad", "View"), LOCTEXT("ViewButtonKey", "Tab"), EWoRKeyCapStyle::Framed, EKeys::Invalid},
			{TEXT("Help"), LOCTEXT("HelpPad", "View"), LOCTEXT("HelpKey", "H"), EWoRKeyCapStyle::Framed, EKeys::H}, // B-543
			{TEXT("Details"), LOCTEXT("DetailsPad", "R3"), LOCTEXT("DetailsKey", "V"), EWoRKeyCapStyle::Framed, EKeys::V}, // B-586: the fight's Intel
		};
	}
}

namespace WoRKeyCaps
{
	// B-483: how a footer's pattern is laid out, and the look of a cap (Slate units at 1080p).
	/** Hints stand this many spaces (or more) apart in a pattern. */
	constexpr int32 HINT_GAP_SPACES = 4;
	/** A hint's keys and its label stand this far apart. */
	const TCHAR *const KEY_LABEL_GAP = TEXT("  ");
	/** Keys of one hint are joined by this ("Up / Down"). */
	const TCHAR *const KEY_JOIN = TEXT(" / ");

	constexpr float HINT_GAP = 36.0f;
	constexpr float CAP_GAP = 6.0f;
	constexpr float CAP_LABEL_GAP = 9.0f;
	constexpr float CAP_PADDING_X = 9.0f;
	constexpr float CAP_PADDING_Y = 2.0f;
	constexpr float CAP_CORNER = 5.0f;
	constexpr float CAP_OUTLINE = 1.0f;
	constexpr float FACE_SIZE = 30.0f;
	constexpr float FACE_OUTLINE = 2.0f;
	/** B-483b: the footer's thin bar around its hints. */
	constexpr float BAR_PADDING_X = 24.0f;
	constexpr float BAR_PADDING_Y = 9.0f;

	// The mockups' face-button colours (screen_mockups/_kit).
	const FLinearColor FaceAColor = FLinearColor::FromSRGBColor(FColor(108, 203, 95));
	const FLinearColor FaceBColor = FLinearColor::FromSRGBColor(FColor(235, 87, 87));
	const FLinearColor FaceXColor = FLinearColor::FromSRGBColor(FColor(91, 141, 239));
	const FLinearColor FaceYColor = FLinearColor::FromSRGBColor(FColor(242, 201, 76));

	bool IsFace(EWoRKeyCapStyle Style)
	{
		return Style != EWoRKeyCapStyle::Framed;
	}

	FLinearColor CapTextColor(EWoRKeyCapStyle Style)
	{
		switch (Style)
		{
		case EWoRKeyCapStyle::FaceA:
			return FaceAColor;
		case EWoRKeyCapStyle::FaceB:
			return FaceBColor;
		case EWoRKeyCapStyle::FaceX:
			return FaceXColor;
		case EWoRKeyCapStyle::FaceY:
			return FaceYColor;
		case EWoRKeyCapStyle::Framed:
		default:
			return WoRMenuStyle::TextColor;
		}
	}

	/** The cap's frame: a rounded plate for a key, a coloured ring for a face button. The brushes live as long as the game. */
	const FSlateBrush *CapBrush(EWoRKeyCapStyle Style)
	{
		static const FSlateRoundedBoxBrush Framed(WoRMenuStyle::PlateColor, CAP_CORNER, WoRMenuStyle::DimColor, CAP_OUTLINE);
		static const FSlateRoundedBoxBrush FaceA(FLinearColor::Transparent, FACE_SIZE * 0.5f, FaceAColor, FACE_OUTLINE);
		static const FSlateRoundedBoxBrush FaceB(FLinearColor::Transparent, FACE_SIZE * 0.5f, FaceBColor, FACE_OUTLINE);
		static const FSlateRoundedBoxBrush FaceX(FLinearColor::Transparent, FACE_SIZE * 0.5f, FaceXColor, FACE_OUTLINE);
		static const FSlateRoundedBoxBrush FaceY(FLinearColor::Transparent, FACE_SIZE * 0.5f, FaceYColor, FACE_OUTLINE);
		switch (Style)
		{
		case EWoRKeyCapStyle::FaceA:
			return &FaceA;
		case EWoRKeyCapStyle::FaceB:
			return &FaceB;
		case EWoRKeyCapStyle::FaceX:
			return &FaceX;
		case EWoRKeyCapStyle::FaceY:
			return &FaceY;
		case EWoRKeyCapStyle::Framed:
		default:
			return &Framed;
		}
	}

	/** A pattern's parts: the text between runs of HINT_GAP_SPACES or more spaces. */
	TArray<FString> SplitParts(const FString &Source)
	{
		TArray<FString> Parts;
		FString Current;
		int32 Spaces = 0;
		const auto EndPart = [&Parts, &Current]()
		{
			Current.TrimStartAndEndInline();
			if (!Current.IsEmpty())
			{
				Parts.Add(Current);
			}
			Current.Reset();
		};
		for (const TCHAR Char : Source)
		{
			if (Char == TEXT(' '))
			{
				++Spaces;
				continue;
			}
			if (Spaces >= HINT_GAP_SPACES)
			{
				EndPart();
			}
			else
			{
				Current.Append(FString::ChrN(Spaces, TEXT(' ')));
			}
			Spaces = 0;
			Current.AppendChar(Char);
		}
		EndPart();
		return Parts;
	}

	/** The key a plain direction word sends when clicked; invalid for any other word. */
	FKey WordKey(const FString &Word)
	{
		static const TMap<FString, FKey> Words = {{TEXT("Up"), EKeys::Up}, {TEXT("Down"), EKeys::Down}, {TEXT("Left"), EKeys::Left}, {TEXT("Right"), EKeys::Right}};
		const FKey *Key = Words.Find(Word);
		return Key ? *Key : EKeys::Invalid;
	}

	/** One key name as a cap; false when the table has no such name. */
	bool AddNamedCap(const FString &Name, bool bGamepad, TArray<FWoRKeyCap> &OutCaps)
	{
		for (const WoRKeyHintsTable::FKeyName &Key : WoRKeyHintsTable::BuildTable())
		{
			if (Name == Key.Name)
			{
				FWoRKeyCap &Cap = OutCaps.AddDefaulted_GetRef();
				Cap.Text = bGamepad ? Key.Gamepad : Key.Keyboard;
				Cap.Style = bGamepad ? Key.GamepadStyle : EWoRKeyCapStyle::Framed;
				Cap.ClickKey = Key.ClickKey;
				return true;
			}
		}
		return false;
	}

	/** Keys ("{Confirm}", "Up / Down", "{Confirm} / Right", "{Tabs}") as caps. False when Keys isn't a list of key names
	 *  (and, where bAllowWords, single words): the part is a sentence and stays text. */
	bool ParseCaps(const FString &Keys, bool bGamepad, bool bAllowWords, TArray<FWoRKeyCap> &OutCaps)
	{
		TArray<FString> Tokens;
		Keys.ParseIntoArray(Tokens, KEY_JOIN);
		if (Tokens.IsEmpty())
		{
			return false;
		}
		for (FString &Token : Tokens)
		{
			Token.TrimStartAndEndInline();
			if (Token.StartsWith(TEXT("{")) && Token.EndsWith(TEXT("}")))
			{
				const FString Name = Token.Mid(1, Token.Len() - 2);
				// A pair draws as its two caps, each clickable on its own.
				const bool bAdded = Name == TEXT("Tabs")	  ? AddNamedCap(TEXT("TabPrev"), bGamepad, OutCaps) && AddNamedCap(TEXT("TabNext"), bGamepad, OutCaps)
									: Name == TEXT("SubTabs") ? AddNamedCap(TEXT("SubTabPrev"), bGamepad, OutCaps) && AddNamedCap(TEXT("SubTabNext"), bGamepad, OutCaps)
															  : AddNamedCap(Name, bGamepad, OutCaps);
				if (!bAdded)
				{
					return false;
				}
			}
			else if (bAllowWords && !Token.IsEmpty() && !Token.Contains(TEXT(" ")) && !Token.Contains(TEXT("{")))
			{
				FWoRKeyCap &Cap = OutCaps.AddDefaulted_GetRef();
				Cap.Text = FText::FromString(Token);
				Cap.ClickKey = WordKey(Token);
			}
			else
			{
				return false;
			}
		}
		return true;
	}
}

FKey FWoRKeyHint::GetClickKey() const
{
	for (int32 Index = Caps.Num() - 1; Index >= 0; --Index)
	{
		if (Caps[Index].ClickKey.IsValid())
		{
			return Caps[Index].ClickKey;
		}
	}
	return EKeys::Invalid;
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

TArray<FWoRKeyHint> WoRKeyHints::ParseHints(const FText &Pattern, bool bGamepad, const FFormatNamedArguments &Extra)
{
	using namespace WoRKeyCaps;
	TArray<FWoRKeyHint> Hints;
	for (const FString &Part : SplitParts(Pattern.ToString()))
	{
		FWoRKeyHint &Hint = Hints.AddDefaulted_GetRef();
		const int32 Gap = Part.Find(KEY_LABEL_GAP);
		const FString Keys = Gap == INDEX_NONE ? Part : Part.Left(Gap);
		// A part with no label ("{SubTabPrev}", a tab row's key label) is caps only when it is all key names.
		if (ParseCaps(Keys, bGamepad, /*bAllowWords=*/Gap != INDEX_NONE, Hint.Caps))
		{
			const FString Label = Gap == INDEX_NONE ? FString() : Part.Mid(Gap).TrimStart();
			Hint.Label = Label.IsEmpty() ? FText::GetEmpty() : Format(FText::FromString(Label), bGamepad, Extra);
			// A hold ("Hold: start over") can't be done by a click, which is one press: its caps click nothing.
			if (Label.StartsWith(LOCTEXT("HoldLabelStart", "Hold").ToString()))
			{
				for (FWoRKeyCap &Cap : Hint.Caps)
				{
					Cap.ClickKey = EKeys::Invalid;
				}
			}
		}
		else
		{
			Hint.Caps.Reset(); // a sentence: plain text
			Hint.Label = Format(FText::FromString(Part), bGamepad, Extra);
		}
	}
	return Hints;
}

FString WoRKeyHints::GetHintsString(const TArray<FWoRKeyHint> &Hints)
{
	FString Out;
	for (const FWoRKeyHint &Hint : Hints)
	{
		Out += Out.IsEmpty() ? TEXT("") : TEXT("\n");
		for (const FWoRKeyCap &Cap : Hint.Caps)
		{
			Out += FString::Printf(TEXT("[%s%s] "), *Cap.Text.ToString(), Cap.ClickKey.IsValid() ? *FString::Printf(TEXT(" -> %s"), *Cap.ClickKey.ToString()) : TEXT(""));
		}
		Out += Hint.Label.ToString();
	}
	return Out;
}

FText WoRKeyHints::Literal(const FText &PlayerText)
{
	// FText::Format's escape character is the backtick: `{ shows a brace instead of opening a name.
	FString Typed = PlayerText.ToString();
	Typed.ReplaceInline(TEXT("`"), TEXT("``"));
	Typed.ReplaceInline(TEXT("{"), TEXT("`{"));
	Typed.ReplaceInline(TEXT("}"), TEXT("`}"));
	return FText::FromString(Typed);
}

FString WoRKeyHints::GetKeyNamesString()
{
	FString Out = TEXT("Key hints (name: gamepad / keyboard, the key a click sends):");
	for (const WoRKeyHintsTable::FKeyName &Key : WoRKeyHintsTable::BuildTable())
	{
		Out += FString::Printf(TEXT("\n  {%s}: %s / %s%s"), Key.Name, *Key.Gamepad.ToString(), *Key.Keyboard.ToString(),
							   Key.ClickKey.IsValid() ? *FString::Printf(TEXT(", click -> %s"), *Key.ClickKey.ToString()) : TEXT(""));
	}
	return Out;
}

void UWoRKeyHintText::SetHint(const FText &InPattern, const FFormatNamedArguments &InExtra)
{
	Pattern = InPattern;
	Extra = InExtra;
	ShowHint();
}

TArray<FWoRKeyHint> UWoRKeyHintText::GetHints() const
{
	return Pattern.IsEmpty() ? TArray<FWoRKeyHint>() : WoRKeyHints::ParseHints(Pattern, WoRKeyHints::IsGamepad(this), Extra);
}

void UWoRKeyHintText::ShowHint()
{
	BindInput(); // the player may not have been known when the text was built
	// The text is kept in caps mode too: it is what GetText and the specs read.
	SetText(Pattern.IsEmpty() ? FText::GetEmpty() : WoRKeyHints::Format(Pattern, WoRKeyHints::IsGamepad(this), Extra));
	if (bKeyCaps && CapsBox.IsValid())
	{
		CapsBox->SetContent(BuildCapsRow());
	}
}

TSharedRef<SWidget> UWoRKeyHintText::BuildCapsRow()
{
	using namespace WoRKeyCaps;
	const FSlateFontInfo LabelFont = GetFont();
	FSlateFontInfo CapFont = LabelFont;
	CapFont.TypefaceFontName = WoRMenuStyle::BoldTypeface;
	const FSlateColor LabelColor = GetColorAndOpacity();
	const TWeakObjectPtr<UWoRKeyHintText> WeakThis(this);

	// Content that sends Key to the screen when clicked; content as it is when the key does nothing.
	const auto MakeClickable = [WeakThis](const TSharedRef<SWidget> &Content, const FKey &Key) -> TSharedRef<SWidget>
	{
		if (!Key.IsValid())
		{
			return Content;
		}
		return SNew(SBorder)
			.BorderImage(FStyleDefaults::GetNoBrush())
			.Padding(FMargin(0.0f))
			.Cursor(EMouseCursor::Hand)
			.OnMouseButtonDown_Lambda([WeakThis, Key](const FGeometry &, const FPointerEvent &MouseEvent)
			{
				if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !WeakThis.IsValid())
				{
					return FReply::Unhandled();
				}
				WeakThis->ClickKey(Key);
				return FReply::Handled();
			})
			[Content];
	};

	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	const TArray<FWoRKeyHint> Hints = GetHints();
	for (int32 HintIndex = 0; HintIndex < Hints.Num(); ++HintIndex)
	{
		const FWoRKeyHint &Hint = Hints[HintIndex];
		TSharedRef<SHorizontalBox> HintBox = SNew(SHorizontalBox);
		for (int32 CapIndex = 0; CapIndex < Hint.Caps.Num(); ++CapIndex)
		{
			const FWoRKeyCap &Cap = Hint.Caps[CapIndex];
			const bool bFace = IsFace(Cap.Style);
			const TSharedRef<SWidget> Frame = SNew(SBorder)
												  .BorderImage(CapBrush(Cap.Style))
												  .Padding(bFace ? FMargin(0.0f) : FMargin(CAP_PADDING_X, CAP_PADDING_Y))
												  .HAlign(HAlign_Center)
												  .VAlign(VAlign_Center)
												  [SNew(STextBlock).Text(Cap.Text).Font(CapFont).ColorAndOpacity(FSlateColor(CapTextColor(Cap.Style)))];
			const TSharedRef<SWidget> Sized = bFace ? StaticCastSharedRef<SWidget>(SNew(SBox).WidthOverride(FACE_SIZE).HeightOverride(FACE_SIZE)[Frame]) : Frame;
			HintBox->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(CapIndex > 0 ? CAP_GAP : 0.0f, 0.0f, 0.0f, 0.0f))[MakeClickable(Sized, Cap.ClickKey)];
		}
		if (!Hint.Label.IsEmpty())
		{
			HintBox->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(Hint.Caps.IsEmpty() ? 0.0f : CAP_LABEL_GAP, 0.0f, 0.0f, 0.0f))
				[SNew(STextBlock).Text(Hint.Label).Font(LabelFont).ColorAndOpacity(LabelColor)];
		}
		// The whole hint is clickable too (its label does what its key does).
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(HintIndex > 0 ? HINT_GAP : 0.0f, 0.0f, 0.0f, 0.0f))[MakeClickable(HintBox, Hint.GetClickKey())];
	}
	if (!bBar || Hints.IsEmpty())
	{
		return Row; // a key label, or nothing to say: no bar
	}
	// B-483b: a footer's hints in a thin bar that hugs them.
	static const FSlateColorBrush BarBrush(WoRMenuStyle::PanelColor);
	return SNew(SBorder).BorderImage(&BarBrush).Padding(FMargin(BAR_PADDING_X, BAR_PADDING_Y))[Row];
}

void UWoRKeyHintText::ClickKey(FKey Key)
{
	if (!Key.IsValid() || !FSlateApplication::IsInitialized())
	{
		return;
	}
	// On the next frame, not inside the click: the key may close or rebuild the screen the click came from.
	const TWeakObjectPtr<UWoRKeyHintText> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, Key](float)
	{
		UUserWidget *Screen = WeakThis.IsValid() ? WeakThis->GetTypedOuter<UUserWidget>() : nullptr;
		if (Screen && FSlateApplication::IsInitialized())
		{
			if (!Screen->HasAnyUserFocus() && !Screen->HasFocusedDescendants())
			{
				Screen->SetKeyboardFocus(); // the key goes to the screen the hint is on; a focused child keeps its focus
			}
			FSlateApplication &Slate = FSlateApplication::Get();
			const FKeyEvent Press(Key, FModifierKeysState(), Slate.GetUserIndexForKeyboard(), /*bIsRepeat=*/false, 0, 0);
			Slate.ProcessKeyDownEvent(Press);
			Slate.ProcessKeyUpEvent(Press);
			UE_LOG(LogWoRUI, Verbose, TEXT("[KeyHints] a click sent %s to %s"), *Key.ToString(), *GetNameSafe(Screen));
		}
		return false; // once
	}));
}

void UWoRKeyHintText::BindInput()
{
	if (BoundInput.IsValid())
	{
		return; // one binding
	}
	if (UCommonInputSubsystem *Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		BoundInput = Input;
		InputChangedHandle = Input->OnInputMethodChangedNative.AddUObject(this, &UWoRKeyHintText::HandleInputMethodChanged);
	}
}

void UWoRKeyHintText::UnbindInput()
{
	if (UCommonInputSubsystem *Input = BoundInput.Get())
	{
		Input->OnInputMethodChangedNative.Remove(InputChangedHandle);
	}
	BoundInput.Reset();
	InputChangedHandle.Reset();
}

TSharedRef<SWidget> UWoRKeyHintText::RebuildWidget()
{
	if (bKeyCaps)
	{
		// B-483: caps and labels in a box of our own, not the text block's one line of text.
		// B-483b: a footer's bar hugs its hints at the right end of whatever width its slot gives it.
		CapsBox = SNew(SBox).HAlign(bBar ? HAlign_Right : HAlign_Fill);
		ShowHint();
		return CapsBox.ToSharedRef();
	}
	TSharedRef<SWidget> Widget = Super::RebuildWidget();
	ShowHint(); // binds, and shows the pattern for the device now that the player is known
	return Widget;
}

void UWoRKeyHintText::ReleaseSlateResources(bool bReleaseChildren)
{
	UnbindInput();
	CapsBox.Reset();
	Super::ReleaseSlateResources(bReleaseChildren);
}

void UWoRKeyHintText::HandleInputMethodChanged(ECommonInputType NewInputType)
{
	ShowHint();
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GKeyHintsPrintCommand(
	TEXT("wor.UI.KeyHints.Print"),
	TEXT("B-481 / B-483: print every menu key name on both devices and the key a click on its cap sends, and the device the first player last used."),
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
