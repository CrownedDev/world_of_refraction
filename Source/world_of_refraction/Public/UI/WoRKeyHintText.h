// WoRKeyHintText.h
// B-481 (Crown, 2026-11-14): one place that says which key a menu hint names. A footer or prompt is written once as a
// pattern with key names in braces ("{Confirm}  Select        {Back}  Back") and shown for the device in use: gamepad
// buttons after a gamepad key, keyboard keys after a keyboard or mouse input (CommonUI's UCommonInputSubsystem), switching
// live. On keyboard Back reads "Backspace" (Esc stops PIE in the editor; WoRMenuStyle::IsBackKey takes both).
// A screen makes its hint text with WoRMenuStyle::MakeHintText and fills it with WoRKeyHints::SetHint: no per-screen
// device code. A pattern that also carries the caller's own values takes them by NAME (FText::FormatNamed, or Extra):
// a numbered argument ({0}) would fill the first key name's place. Debug: wor.UI.KeyHints.Print.
//
// B-483 (Crown, 2026-11-14): key caps. A footer (WoRMenuStyle::MakeHintBar) and a tab row's key label (MakeKeyLabel)
// draw each key as a small framed cap with its label beside it: "[Enter] Edit   [X] Simple   [Backspace] Back" on the
// keyboard, the coloured A / B / X / Y circles on a gamepad. Each hint is clickable: a click sends the hint's key to
// the screen, so it does what the key does (no per-screen code). The pattern's layout is what a footer always was:
// hints apart by four or more spaces, each "KEYS  Label" (two spaces), KEYS one or more of "{Name}" or a word (Up,
// Left) joined by " / "; {Tabs} and {SubTabs} draw as their two caps. A part that isn't laid out so stays plain text.

#pragma once

#include "Components/TextBlock.h"
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "WoRKeyHintText.generated.h"

class SBox;
class UCommonInputSubsystem;
class UWidget;
enum class ECommonInputType : uint8;

/** B-483: how a key cap is drawn. */
enum class EWoRKeyCapStyle : uint8
{
	/** A small framed cap: a keyboard key, a shoulder button, a stick. */
	Framed,
	/** The gamepad's face buttons: a circle in the button's colour (the mockups' A green, B red, X blue, Y yellow). */
	FaceA,
	FaceB,
	FaceX,
	FaceY
};

/** B-483: one key of a hint. */
struct FWoRKeyCap
{
	FText Text;
	EWoRKeyCapStyle Style = EWoRKeyCapStyle::Framed;
	/** The key a click on the cap sends to the screen; invalid for a cap that does nothing when clicked. */
	FKey ClickKey;
};

/** B-483: one hint of a footer: its keys and the word beside them. No caps: plain text (the label alone). */
struct FWoRKeyHint
{
	TArray<FWoRKeyCap> Caps;
	FText Label;
	/** What a click on the label sends: the last cap's key that does something (a pair's "next"). */
	FKey GetClickKey() const;
};

namespace WoRKeyHints
{
	/** True when the last input of Context's player came from a gamepad; keyboard and mouse otherwise, and with no
	 *  player (headless). */
	WORLD_OF_REFRACTION_API bool IsGamepad(const UWidget *Context);

	/** The key names a pattern may use, for one device (InputMap.md, Menus): {Confirm} {Back} {Toggle} {Second} {Tabs}
	 *  {TabPrev} {TabNext} {SubTabs} {SubTabPrev} {SubTabNext} {SetActive} {Skills} {Arrows} {Interact} {ViewButton}. */
	WORLD_OF_REFRACTION_API FFormatNamedArguments GetKeyNames(bool bGamepad);

	/** Pattern with its key names filled for the device; Extra adds the caller's own arguments ("{What}"). */
	WORLD_OF_REFRACTION_API FText Format(const FText &Pattern, bool bGamepad, const FFormatNamedArguments &Extra = FFormatNamedArguments());

	/** Shows Pattern on Text for the device in use. A text made by WoRMenuStyle::MakeHintText keeps the pattern and
	 *  shows it again when the device changes; any other text is filled once. */
	WORLD_OF_REFRACTION_API void SetHint(UTextBlock *Text, const FText &Pattern, const FFormatNamedArguments &Extra = FFormatNamedArguments());

	/** B-483: Pattern as key caps and labels for the device (the layout is in this file's header). */
	WORLD_OF_REFRACTION_API TArray<FWoRKeyHint> ParseHints(const FText &Pattern, bool bGamepad, const FFormatNamedArguments &Extra = FFormatNamedArguments());

	/** Debug: the hints one per line, "[Enter] Edit", a clickable cap marked with its key. */
	WORLD_OF_REFRACTION_API FString GetHintsString(const TArray<FWoRKeyHint> &Hints);

	/** Text the player typed (a loadout's name), safe to put in a hint: its braces and backticks show as typed instead
	 *  of being read as key names. */
	WORLD_OF_REFRACTION_API FText Literal(const FText &PlayerText);

	/** Debug: every key name on both devices. */
	WORLD_OF_REFRACTION_API FString GetKeyNamesString();
}

/** A text block that holds a key-hint pattern and follows the device in use. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRKeyHintText : public UTextBlock
{
	GENERATED_BODY()

public:
	void SetHint(const FText &InPattern, const FFormatNamedArguments &InExtra);

	/** B-483: draw the pattern as key caps with their labels, each clickable (a footer, a key label), not as a line of
	 *  text. Set once, before the widget is drawn (WoRMenuStyle::MakeHintBar / MakeKeyLabel).
	 *  B-483b: bInBar puts the caps in a thin panel bar that hugs them, at the right end of the room it is given (a
	 *  footer); none for a key label. */
	void SetKeyCaps(bool bInKeyCaps, bool bInBar = false)
	{
		bKeyCaps = bInKeyCaps;
		bBar = bInBar;
	}
	bool HasKeyCaps() const { return bKeyCaps; }
	bool HasBar() const { return bBar; }
	/** The hints as drawn now (the specs read them). */
	TArray<FWoRKeyHint> GetHints() const;
	/** What a click on a hint does: Key goes to the owning screen as a key press on the next frame. */
	void ClickKey(FKey Key);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	void HandleInputMethodChanged(ECommonInputType NewInputType);
	void ShowHint();
	/** Listens to the player's input subsystem once it is known (not yet at construction, never headless). */
	void BindInput();
	void UnbindInput();
	/** B-483: the caps and labels as Slate, built again when the pattern or the device changes (no UObject is made). */
	TSharedRef<SWidget> BuildCapsRow();

	bool bKeyCaps = false;
	bool bBar = false;
	TSharedPtr<SBox> CapsBox;

	FText Pattern;
	FFormatNamedArguments Extra;
	/** The subsystem listened to, kept so the binding comes off even when the player is gone at teardown. */
	TWeakObjectPtr<UCommonInputSubsystem> BoundInput;
	FDelegateHandle InputChangedHandle;
};
