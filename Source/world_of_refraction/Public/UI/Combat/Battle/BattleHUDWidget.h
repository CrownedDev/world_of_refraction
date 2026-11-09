// BattleHUDWidget.h
// PLACEHOLDER UI (B-391, VisualGameplayPlan step 1, Q-391): the new battle HUD, plain C++ (no .uasset), built for
// today's systems. One full-screen widget added straight to the viewport by UCombatHUD (wor.UI.NewHUD 1):
// - party cards bottom-left: name, HP bar and "HP x / y", "EP x / y" (hidden for a Resonator with no EP use), status chips;
// - enemy cards top-right: name, HP bar (no numbers, Q-391), status chips;
// - turn order down the left edge: the current actor, then the next turns (bonus turns marked);
// - a Statuses button (top centre) opening one panel with every status in full (FeelGuide 5).
// The command menu is its own widget (UBattleCommandMenuWidget). CommonUI replaces both later (D-024).
//
// Created once per fight: Initialise (binds, fills the state) BEFORE AddToViewport, so RebuildWidget builds one card
// per combatant once. After that only texts change (TransBuffer rule: nothing is recreated inside the widget). Every
// update comes from a delegate (no Tick). The state is plain data, so it runs unbuilt too (spec WoR.UI.BattleHUD).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Combat/Battle/BattleHUDView.h"
#include "BattleHUDWidget.generated.h"

class UBorder;
class UButton;
class UCharacterDataComponent;
class UProgressBar;
class USkillEffectManager;
class UTextBlock;
class UTurnManager;
class UTurnStateComponent;
class UVerticalBox;
class UWoREffectMirrorComponent;
struct FActiveSkillEffect;

/** One combatant's card: what it shows (plain data) and, once built, its text blocks. */
USTRUCT()
struct FBattleHUDCard
{
	GENERATED_BODY()

	TWeakObjectPtr<AActor> Actor;
	TWeakObjectPtr<UCharacterDataComponent> CharData;
	bool bParty = false;
	bool bActing = false;
	bool bAlive = true;
	bool bShowEP = true;
	float HPFraction = 0.0f;
	FText Name;
	FText HPLine;
	FText EPLine;
	FText StatusLine;

	UPROPERTY()
	TObjectPtr<UBorder> Frame;

	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY()
	TObjectPtr<UProgressBar> HPBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> HPText;

	UPROPERTY()
	TObjectPtr<UTextBlock> EPText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;
};

UCLASS()
class WORLD_OF_REFRACTION_API UBattleHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bind to the fight's combatants and fill the state. Once per widget; call before AddToViewport. */
	void Initialise(const TArray<AActor *> &Party, const TArray<AActor *> &Enemies);

	/** Unbind every delegate. Safe to call twice; NativeDestruct calls it too. */
	void Teardown();

	/** Debug (wor.UI.Print): every card's lines, the turn strip and the status panel. */
	FString GetBattleHUDString() const;

	const TArray<FBattleHUDCard> &GetCards() const { return Cards; }
	const TArray<FText> &GetTurnLines() const { return TurnLines; }
	const FText &GetStatusPanelText() const { return StatusPanelText; }
	bool IsStatusPanelOpen() const { return bStatusPanelOpen; }

	/** Open or close the view-all status panel (the Statuses button toggles it). */
	void SetStatusPanelOpen(bool bOpen);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void AddCard(AActor *Actor, bool bParty);
	void BuildCardWidgets(FBattleHUDCard &Card, UVerticalBox *Column);

	void RefreshPools();
	void RefreshStatuses();
	void RefreshTurnStrip();
	void PushToWidgets();

	TArray<FActiveSkillEffect> GetStatusesOf(AActor *Actor) const;

	UFUNCTION()
	void HandlePoolChanged(int32 Current, int32 Max);

	UFUNCTION()
	void HandleLifeChanged(AActor *Actor);

	UFUNCTION()
	void HandleEffectChanged(AActor *Target, const FActiveSkillEffect &Effect);

	UFUNCTION()
	void HandleEffectCountChanged(AActor *Target, const FActiveSkillEffect &Effect, int32 Count);

	UFUNCTION()
	void HandleHeldSnapshotChanged(AActor *Holder);

	UFUNCTION()
	void HandleTurnStarted(AActor *Actor, int32 TurnNumber);

	UFUNCTION()
	void HandleTurnStateChanged();

	UFUNCTION()
	void HandleStatusButtonClicked();

	UPROPERTY(Transient)
	TArray<FBattleHUDCard> Cards;

	TArray<FText> TurnLines;
	TArray<FLinearColor> TurnLineColors;
	FText StatusPanelText;
	bool bStatusPanelOpen = false;
	bool bBound = false;
	int32 CurrentTurnNumber = 1;

	TWeakObjectPtr<USkillEffectManager> BoundEffects;
	TWeakObjectPtr<UTurnStateComponent> BoundTurnState;
	TWeakObjectPtr<UTurnManager> BoundTurnManager;
	TArray<TWeakObjectPtr<UWoREffectMirrorComponent>> BoundMirrors;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> TurnTexts;

	UPROPERTY()
	TObjectPtr<UBorder> StatusPanel;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusPanelBody;
};
