// TrialNPCWidget.h
// PLACEHOLDER UI (B-449, Backlog rule 4): a trial NPC's talk panel. Its name, then what it offers as plain buttons
// built in C++ (like URestSpotWidget). V: Extract, which asks first (the rest spot's own confirm text and "what you
// earned" line), then ends the run at the half share. Opened by ATrialNPC::Interact. Replaced by the CommonUI screen
// (D-024).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TrialNPCWidget.generated.h"

class APlayerController;
class ATrialNPC;
class UButton;
class UTextBlock;

UCLASS()
class WORLD_OF_REFRACTION_API UTrialNPCWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Adds one to PC's viewport for NPC (UI-only input, cursor on). Null when either is null. */
	static UTrialNPCWidget *Open(APlayerController *PC, ATrialNPC *NPC);

	/** Debug: what the panel shows. */
	FString GetTrialNPCWidgetString() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void Refresh();
	void CloseMenu();

	UFUNCTION()
	void HandleExtractClicked();
	UFUNCTION()
	void HandleConfirmExtractClicked();
	UFUNCTION()
	void HandleCloseClicked();

	TWeakObjectPtr<ATrialNPC> NPC;

	/** Extract asks first; the second press (Leave) extracts. */
	bool bConfirmingExtract = false;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NoteText;
	UPROPERTY(Transient)
	TObjectPtr<UButton> ExtractButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConfirmText;
	UPROPERTY(Transient)
	TObjectPtr<UButton> ConfirmButton;
};
