// CurseClearWidget.h
// PLACEHOLDER UI (B-378b, Q-378 changed by Crown 2026-11-08): the hub's service for clearing a curse you hold, by your
// own choice (UCurseSubsystem::ClearCurse). Plain buttons built in C++, opened with wor.UI.ClearCurse. Crown can move
// the service; the real screen replaces this (D-024). A cleared curse comes back when its giver is met again (Q-378).
//
// Built once in RebuildWidget; Refresh only rewrites the texts, so nothing is recreated inside the widget.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CurseClearWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS()
class WORLD_OF_REFRACTION_API UCurseClearWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Add a fresh one to PC's viewport (UI-only input, cursor on). Null when PC is null. */
	static UCurseClearWidget *Open(APlayerController *PC);

	/** Re-read the held curses and rewrite the texts. */
	void Refresh();

	/** Debug: the selected curse and the status line as shown. */
	FString GetCurseClearString() const;

	/** The hub service only: refused while a trial run is on (Q-378: a hub service). */
	static bool CanClearHere(const UObject *Context);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	/** The held curse ids, sorted, so Next walks them in a fixed order. */
	TArray<FName> GatherHeld() const;

	UFUNCTION()
	void HandleNextClicked();

	UFUNCTION()
	void HandleClearClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY()
	TObjectPtr<UTextBlock> CurseText;

	UPROPERTY()
	TObjectPtr<UButton> ClearButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusLine;

	int32 SelectedIndex = 0;
	FText Status;
};
