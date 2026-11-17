// EnemyKitCardWidget.h
// B-565 (D-225, TrialPoolDesign stage 1): the kit card. What an enemy carries: its name, class, each piece with its tier
// and slots, and its skills. No rank word (D-187). B closes. A CommonUI screen on the root layout's menu layer; stage 1
// opens it with made-up contents (wor.UI.PreviewKitCard), stage 8 from a real enemy.

#pragma once

#include "CoreMinimal.h"
#include "Trial/TrialPoolLook.h"
#include "UI/WoRActivatableWidget.h"
#include "EnemyKitCardWidget.generated.h"

class APlayerController;
class UTextBlock;

UCLASS()
class WORLD_OF_REFRACTION_API UEnemyKitCardWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	UEnemyKitCardWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the card onto PC's root layout; null without one. */
	static UEnemyKitCardWidget *Open(APlayerController *PC, const FEnemyKitCard &Card);

	void SetCard(const FEnemyKitCard &InCard);
	const FEnemyKitCard &GetCard() const { return Card; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;

private:
	void Refresh();

	FEnemyKitCard Card;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ClassText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PiecesText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SkillsText;
};
