// BattleHUDWidget.h
// PLACEHOLDER UI (B-391, VisualGameplayPlan step 1, Q-391): the new battle HUD, plain C++ (no .uasset), built for
// today's systems. One full-screen widget added straight to the viewport by UCombatHUD (wor.UI.NewHUD 1):
// - party cards bottom-left: name, HP bar and "HP x / y", "EP x / y" (hidden for a Resonator with no EP use), status chips;
// - enemy cards top-right: name, HP bar (no numbers, Q-391), status chips;
// - turn order down the left edge: the current actor, then the next turns (bonus turns marked);
// - a Statuses button (top centre) opening one panel with every status in full (FeelGuide 5);
// - B-391b: floating damage numbers at each card character's hit point (UCombatPresentationComponent::OnHitDetailShown,
//   so every machine shows the server's numbers), styled by DamageNumberView; a Broken Darkness card's EP line is its
//   absorption pool and overload ("Absorb x / y, overload z%", tinted as the old panel);
// - B-170e: in an action-pips fight (wor.Turns.ActionPips 1), each party card's action pips and Max Actions line
//   ("Actions ●●○ 2 · Max 3 (+75/100)", ActionPipsText), live from OnActionsChanged. At 0 the cards are as before.
// - B-391c: a status buildup bar on every card (UStatusBuildupManager::OnStatusBuildupChanged; on a client the
//   replicated UStatusBarStateComponent), tinted as the old panel, with Q-111 A's cracks past half (a fixed set of
//   marks built once, shown as the bar fills); party cards add "Status x / y", a gear line (weapon · ring) and a hover
//   tooltip with the loadout, from the ULoadoutComponent; a 0-damage hit on Broken Darkness reads "Absorbed"
//   (DamageNumberView::SHOW_ABSORB_CUE, Q-391c). Combatants that join mid-fight get no card or numbers: nothing
//   announces a join today (no summons exist; UISystem.md Known Limitations).
// The command menu is its own widget (UBattleCommandMenuWidget). CommonUI replaces both later (D-024).
//
// Created once per fight: Initialise (binds, fills the state) BEFORE AddToViewport, so RebuildWidget builds one card
// per combatant once. After that only texts change (TransBuffer rule: nothing is recreated inside the widget). Every
// update comes from a delegate (no Tick). The state is plain data, so it runs unbuilt too (spec WoR.UI.BattleHUD).
// Damage numbers use a pool of text blocks made once in RebuildWidget; one looping timer moves and fades the live ones
// and stops when none are left (no Tick). Debug: wor.UI.Print (via UCombatHUD), wor.UI.DamageNumber.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Skills/Effects/ActiveSkillEffect.h" // UFUNCTION handler parameter type
#include "UI/Combat/Battle/BattleHUDView.h"
#include "UI/Combat/Battle/DamageNumberView.h" // FCombatHitShown: UFUNCTION handler parameter type
#include "BattleHUDWidget.generated.h"

class UBorder;
class UCombatPresentationComponent;
class UButton;
class UCanvasPanel;
class UCharacterDataComponent;
class ULoadoutComponent;
class UProgressBar;
class USkillEffectManager;
class UStatusBarStateComponent;
class UStatusBuildupManager;
class UTextBlock;
class UTurnManager;
class UTurnStateComponent;
class UVerticalBox;
class UWeaponData;
class UWoREffectMirrorComponent;

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
	/** B-391b: the EP line is the absorption pool. */
	bool bBrokenDarkness = false;
	FLinearColor EPColour = FLinearColor::White;
	float HPFraction = 0.0f;
	FText Name;
	FText HPLine;
	FText EPLine;
	/** B-170e: "Actions ●●○ 2 · Max 3 (+75/100)"; party cards in an action-pips fight only. */
	FText ActionsLine;
	FText StatusLine;

	/** B-391c: the status buildup bar (every card; the line on party cards only, Q-391). */
	float BuildupFraction = 0.0f;
	int32 CracksShown = 0;
	FLinearColor BuildupColour = FLinearColor::White;
	FText BuildupLine;

	/** B-391c: party cards: "weapon · ring" and the hover tooltip with the loadout. */
	TWeakObjectPtr<ULoadoutComponent> Loadout;
	FText GearLine;
	FText GearTooltip;

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
	TObjectPtr<UTextBlock> ActionsText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<UProgressBar> BuildupBar;

	/** One mark per BattleHUDView::GetCrackPositions, built once; shown as the bar passes each. */
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> CrackMarks;

	UPROPERTY()
	TObjectPtr<UTextBlock> BuildupText;

	UPROPERTY()
	TObjectPtr<UTextBlock> GearText;
};

/** B-391b: one pooled floating damage number. */
USTRUCT()
struct FBattleDamageNumber
{
	GENERATED_BODY()

	TWeakObjectPtr<AActor> Target;
	FCombatHitShown Hit;
	FDamageNumberStyle Style;
	/** Where it was hit (the top of the target), projected each step so it stays put as the camera moves. */
	FVector WorldPoint = FVector::ZeroVector;
	double SpawnTime = 0.0;
	/** Its row: one above the highest of this target's numbers live when it appeared (DamageNumberView::NextStackIndex). */
	int32 StackIndex = 0;
	bool bLive = false;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text;
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
	/** B-170e: true when this fight banks actions (read once in Initialise), so party cards show pips. */
	bool IsShowingActions() const { return bShowActions; }
	bool IsStatusPanelOpen() const { return bStatusPanelOpen; }

	/** Open or close the view-all status panel (the Statuses button toggles it). */
	void SetStatusPanelOpen(bool bOpen);

	/** B-391b: float one number over Target, on this machine only (the hit handler and wor.UI.DamageNumber). */
	void ShowDamageNumber(AActor *Target, const FCombatHitShown &Hit);

	int32 GetLiveDamageNumberCount() const;
	const TArray<FBattleDamageNumber> &GetDamageNumbers() const { return DamageNumbers; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void AddCard(AActor *Actor, bool bParty);
	void BuildCardWidgets(FBattleHUDCard &Card, UVerticalBox *Column);

	void BuildBuildupWidgets(FBattleHUDCard &Card, UVerticalBox *Rows);

	void RefreshPools();
	void RefreshStatuses();
	void RefreshTurnStrip();
	void RefreshGear();
	void PushToWidgets();

	/** B-391c: one card's bar from a buildup report (the manager's, or the replicated bar on a client). */
	static void SetCardBuildup(FBattleHUDCard &Card, float Current, float Max, ESpellElement PendingElement, const AActor *Source);

	TArray<FActiveSkillEffect> GetStatusesOf(AActor *Actor) const;

	void EnsureDamageNumberPool();
	void BuildDamageNumberWidgets(UCanvasPanel *Canvas);
	void StepDamageNumbers();
	void PlaceDamageNumber(FBattleDamageNumber &Number, double Now);
	void HideDamageNumber(FBattleDamageNumber &Number);

	UFUNCTION()
	void HandleHitShown(AActor *Target, const FCombatHitShown &Hit);

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

	UFUNCTION()
	void HandleBuildupChanged(AActor *Target, float Current, float Max, ESpellElement PendingElement, AActor *Source);

	UFUNCTION()
	void HandleLoadoutChanged(int32 NewLoadoutIndex);

	UFUNCTION()
	void HandleActiveWeaponChanged(UWeaponData *NewWeapon);

	UPROPERTY(Transient)
	TArray<FBattleHUDCard> Cards;

	TArray<FText> TurnLines;
	TArray<FLinearColor> TurnLineColors;
	FText StatusPanelText;
	bool bStatusPanelOpen = false;
	bool bBound = false;
	bool bShowActions = false;
	int32 CurrentTurnNumber = 1;

	TWeakObjectPtr<USkillEffectManager> BoundEffects;
	TWeakObjectPtr<UTurnStateComponent> BoundTurnState;
	TWeakObjectPtr<UTurnManager> BoundTurnManager;
	TArray<TWeakObjectPtr<UWoREffectMirrorComponent>> BoundMirrors;
	TArray<TWeakObjectPtr<UCombatPresentationComponent>> BoundPresentations;
	TWeakObjectPtr<UStatusBuildupManager> BoundBuildup;
	TArray<TWeakObjectPtr<UStatusBarStateComponent>> BoundBarStates;

	UPROPERTY(Transient)
	TArray<FBattleDamageNumber> DamageNumbers;

	FTimerHandle DamageNumberTimer;
	int32 DamageNumbersShown = 0;
	FString LastDamageNumber;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> TurnTexts;

	UPROPERTY()
	TObjectPtr<UBorder> StatusPanel;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusPanelBody;
};
