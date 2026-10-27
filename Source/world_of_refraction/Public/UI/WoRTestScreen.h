// WoRTestScreen.h
// PLACEHOLDER (B-103): an empty CommonUI screen that proves the root layout's stacks
// and Back handling. Built in C++ (one line of text); Back (Esc / gamepad B) closes it.
// Opened with the console command wor.UI.TestScreen [Game|Menu|Modal] in the hub.

#pragma once

#include "CoreMinimal.h"
#include "UI/WoRActivatableWidget.h"
#include "WoRTestScreen.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UWoRTestScreen : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	UWoRTestScreen(const FObjectInitializer &ObjectInitializer);

	/** Debug / tests: do what pressing Back does. */
	void PressBack() { HandleBackAction(); }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
};
