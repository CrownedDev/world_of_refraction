// BattleHUDWidget.h
// PLACEHOLDER UI (B-391, VisualGameplayPlan step 1, Q-391): the new battle HUD, plain C++ (no .uasset), built for
// today's systems. One full-screen widget added straight to the viewport by UCombatHUD (wor.UI.NewHUD 1):
// - party cards along the bottom from the left (B-436a, D-254): the featured card (the party member acting, else the
//   last one that acted, else the first) is full: name, HP bar and "HP x / y", "EP x / y" (hidden for a Resonator with
//   no EP use), "Act: 0/2", status chips; the others sit in a row to its right as compact cards (name, HP bar,
//   build-up bar). The command menu stacks above the featured card (GetCommandMenuOffset). D-254 (B-581): no Spirit /
//   Body lines on a card; a click on a party card inspects it in the status panel (InspectCard);
// - turn order top-right: the current actor, then the next turns (bonus turns marked);
// - enemy cards under the turn order: name, HP bar (no numbers, Q-391), status chips;
// - a Statuses button (top centre) opening one panel with every status in full (FeelGuide 5);
// - B-391b: floating damage numbers at each card character's hit point (UCombatPresentationComponent::OnHitDetailShown,
//   so every machine shows the server's numbers), styled by DamageNumberView; a Broken Darkness card's EP line is its
//   absorption pool and overload ("Absorb x / y, overload z%", tinted as the old panel).
// - B-391c: a status buildup bar on every card (UStatusBuildupManager::OnStatusBuildupChanged; on a client the
//   replicated UStatusBarStateComponent), tinted as the old panel, with Q-111 A's cracks past half (a fixed set of
//   marks built once, shown as the bar fills); party cards add "Status x / y", a gear line (weapon · ring) and a hover
//   tooltip with the loadout, from the ULoadoutComponent; a 0-damage hit on Broken Darkness reads "Negated"
//   (DamageNumberView::SHOW_NEGATED_CUE, Q-391c). Combatants that join mid-fight get no card or numbers: nothing
//   announces a join today (no summons exist; UISystem.md Known Limitations).
// - B-170e: in every fight (B-170h2), each party card's action pips and Max Actions line
//   ("Actions ●●○ 2 · Max 3 (+75/100)", ActionPipsText), live from OnActionsChanged. At 0 the cards are as before.
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
#include "UI/Combat/Battle/BattleHUDCard.h"
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
	bool IsStatusPanelOpen() const { return bStatusPanelOpen; }

	/** Open or close the view-all status panel (the Statuses button toggles it). Closing ends an inspect. */
	void SetStatusPanelOpen(bool bOpen);

	/** D-254: inspect a party card (a click on it): the status panel shows that character alone: name, Spirit of ... /
	 *  Body of ..., the full actions line (pips, Max, action-point progress, the bonus warning) and its statuses. The
	 *  same card again closes it; another card switches; the Status button goes back to everyone. Enemy cards: no-op. */
	void InspectCard(int32 CardIndex);

	/** The Status button: the view of everyone. From an inspected character it switches to that view, else it opens
	 *  or closes the panel. */
	void ToggleAllStatuses();

	/** The party card being inspected; INDEX_NONE when the panel is closed or shows everyone. */
	int32 GetInspectedCardIndex() const { return InspectedCard; }

	/** B-391b: float one number over Target, on this machine only (the hit handler and wor.UI.DamageNumber). */
	void ShowDamageNumber(AActor *Target, const FCombatHitShown &Hit);

	int32 GetLiveDamageNumberCount() const;
	const TArray<FBattleDamageNumber> &GetDamageNumbers() const { return DamageNumbers; }

	/** B-436a: the party card shown in full: the party member acting, else the last one that acted, else the first.
	 *  INDEX_NONE with no party card. */
	int32 GetFeaturedCardIndex() const;

	/** B-436a: where UCombatHUD puts the command menu, from the bottom-left corner: just above the featured card. */
	static FVector2D GetCommandMenuOffset();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void AddCard(AActor *Actor, bool bParty);
	/** B-436a: the featured card full at the bottom-left corner, the others compact in a row to its right. Moves canvas
	 *  slots and shows or hides lines only: nothing is re-parented or recreated (TransBuffer rule). */
	void LayoutPartyCards();

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

	/** D-254: a left click on a party card (UBorder::OnMouseButtonDownEvent, one handler for all of them). */
	UFUNCTION()
	FEventReply HandleCardMouseDown(FGeometry MyGeometry, const FPointerEvent &MouseEvent);

	/** What the status panel shows for one inspected card. */
	FText BuildInspectText(const FBattleHUDCard &Card) const;

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
	/** D-254: the party card the status panel shows alone; INDEX_NONE: everyone. */
	int32 InspectedCard = INDEX_NONE;
	bool bBound = false;
	int32 CurrentTurnNumber = 1;
	/** B-436a: the last party card that acted (GetFeaturedCardIndex falls back to the first party card). */
	int32 LastActingPartyCard = INDEX_NONE;

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
