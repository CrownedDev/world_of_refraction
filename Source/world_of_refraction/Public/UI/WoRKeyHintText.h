// WoRKeyHintText.h
// B-481 (Crown, 2026-11-14): one place that says which key a menu hint names. A footer or prompt is written once as a
// pattern with key names in braces ("{Confirm}  Select        {Back}  Back") and shown for the device in use: gamepad
// buttons after a gamepad key, keyboard keys after a keyboard or mouse input (CommonUI's UCommonInputSubsystem), switching
// live. On keyboard Back reads "Backspace" (Esc stops PIE in the editor; WoRMenuStyle::IsBackKey takes both).
// A screen makes its hint text with WoRMenuStyle::MakeHintText and fills it with WoRKeyHints::SetHint: no per-screen
// device code. A pattern that also carries the caller's own values takes them by NAME (FText::FormatNamed, or Extra):
// a numbered argument ({0}) would fill the first key name's place. Debug: wor.UI.KeyHints.Print.

#pragma once

#include "Components/TextBlock.h"
#include "CoreMinimal.h"
#include "WoRKeyHintText.generated.h"

class UCommonInputSubsystem;
class UWidget;
enum class ECommonInputType : uint8;

namespace WoRKeyHints
{
	/** True when the last input of Context's player came from a gamepad; keyboard and mouse otherwise, and with no
	 *  player (headless). */
	WORLD_OF_REFRACTION_API bool IsGamepad(const UWidget *Context);

	/** The key names a pattern may use, for one device (InputMap.md, Menus): {Confirm} {Back} {Toggle} {Second} {Tabs}
	 *  {TabPrev} {TabNext} {SubTabs} {SubTabPrev} {SubTabNext} {SetActive} {Arrows} {Interact} {ViewButton}. */
	WORLD_OF_REFRACTION_API FFormatNamedArguments GetKeyNames(bool bGamepad);

	/** Pattern with its key names filled for the device; Extra adds the caller's own arguments ("{What}"). */
	WORLD_OF_REFRACTION_API FText Format(const FText &Pattern, bool bGamepad, const FFormatNamedArguments &Extra = FFormatNamedArguments());

	/** Shows Pattern on Text for the device in use. A text made by WoRMenuStyle::MakeHintText keeps the pattern and
	 *  shows it again when the device changes; any other text is filled once. */
	WORLD_OF_REFRACTION_API void SetHint(UTextBlock *Text, const FText &Pattern, const FFormatNamedArguments &Extra = FFormatNamedArguments());

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

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	void HandleInputMethodChanged(ECommonInputType NewInputType);
	void ShowHint();
	/** Listens to the player's input subsystem once it is known (not yet at construction, never headless). */
	void BindInput();
	void UnbindInput();

	FText Pattern;
	FFormatNamedArguments Extra;
	/** The subsystem listened to, kept so the binding comes off even when the player is gone at teardown. */
	TWeakObjectPtr<UCommonInputSubsystem> BoundInput;
	FDelegateHandle InputChangedHandle;
};
