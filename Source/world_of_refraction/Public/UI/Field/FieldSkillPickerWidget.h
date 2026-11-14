// FieldSkillPickerWidget.h
// PLACEHOLDER UI (B-214b, FieldAbilities "Picking it"): the player's character picks its active field skill: a source
// (Innate / Weapon / Ring, with the element it gives now), then a movement type unlocked for that element
// (UFieldSkillUnlockSubsystem). Plain buttons built in C++, opened from the party screen's "Field skill" button or
// wor.UI.FieldSkill. Replaced by the CommonUI loadout screen (D-024).
// Built once in RebuildWidget; Refresh only rewrites the texts.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Field/FieldSkillTypes.h"
#include "FieldSkillPickerWidget.generated.h"

class UButton;
class UTextBlock;
class UFieldMovementComponent;

UCLASS()
class WORLD_OF_REFRACTION_API UFieldSkillPickerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Add a fresh one to PC's viewport (UI-only input, cursor on). Null when PC is null. */
	static UFieldSkillPickerWidget *Open(APlayerController *PC);

	/** Re-read the pawn's choice and the unlocks and rewrite every text. */
	void Refresh();

	/** Debug: the texts as shown, one line. */
	FString GetPickerString() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	/** Next source (Innate -> Weapon -> Ring); the type list follows its element. Picks nothing yet. */
	UFUNCTION()
	void HandleSourceClicked();

	/** Picks the next movement type unlocked for the shown source's element (wrapping). */
	UFUNCTION()
	void HandleTypeClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFieldMovementComponent *GetFieldMovement() const;

	/** The built types the shown source's element may use now (the dash first). */
	TArray<EFieldMovementType> GetPickableTypes() const;

	/** The source shown (starts at the current choice's). */
	EFieldSkillSource ShownSource = EFieldSkillSource::Innate;

	/** The last pick's answer, shown under the buttons. */
	EFieldSkillRefusal LastRefusal = EFieldSkillRefusal::None;

	UPROPERTY()
	TObjectPtr<UTextBlock> SourceText;

	UPROPERTY()
	TObjectPtr<UTextBlock> TypeText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;
};
