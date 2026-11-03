// CharacterFormWidget.h
// PLACEHOLDER UI (B-279b): the skins screen (Design/CharacterForms.md §1). Steps through every
// character form with its meter; an unlocked one shows its full loadout (the reveal) and can be
// worn. Manny / Quinn pick the base body. Plain buttons built in C++, opened by `wor.UI.Forms`.
// Replaced by the CommonUI screen (D-024).
//
// Built once in RebuildWidget; the page logic only writes Title / Body, so it runs unbuilt too.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Character/Forms/CharacterFormData.h"
#include "CharacterFormWidget.generated.h"

class UCharacterFormSubsystem;
class UTextBlock;

UCLASS()
class WORLD_OF_REFRACTION_API UCharacterFormWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Add one to PC's viewport (UI-only input, cursor on); the open one when PC already has it open. */
	static UCharacterFormWidget *Open(APlayerController *PC);

	/** Select the next known form and show it. False when no form is known. */
	bool SelectNextForm();

	/** Wear the selected form (null selection: take the skin off). The refusal shows on the page. */
	EFormSwitchRefusal WearSelected();
	EFormSwitchRefusal TakeOffSkin();
	bool PickBaseForm(EPlayerBaseForm Base);

	/** Redraw the selected form's page. */
	void ShowPage();

	UCharacterFormData *GetSelectedForm() const { return Selected; }
	const FText &GetTitle() const { return Title; }
	const FText &GetBody() const { return Body; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleNextClicked();

	UFUNCTION()
	void HandleWearClicked();

	UFUNCTION()
	void HandleTakeOffClicked();

	UFUNCTION()
	void HandleMannyClicked();

	UFUNCTION()
	void HandleQuinnClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UCharacterFormSubsystem *GetForms() const;
	void ShowStatus(const FText &InStatus);

	UPROPERTY()
	TObjectPtr<UCharacterFormData> Selected;

	FText Title;
	FText Body;
	FText Status;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> BodyText;
};
