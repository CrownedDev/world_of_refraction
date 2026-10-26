// PartyDisplayWidget.h
// C++ base for WBP_PartyDisplay (Arc 1.5b Cluster 3): the hub's persistent
// party HUD strip — MAX_PARTY_MEMBERS cards, populated left-to-right from the
// Trial Party, ghosts for vacant slots. Persistent HUD, NOT modal: no pause /
// input / cursor bracket, unlike the shop window.
//
// Cards are built ONCE in NativeConstruct and toggled between populated and
// ghost thereafter — never recreated (TransBuffer crash pattern). Data flow is
// OnPartyChanged → RefreshFromParty, plus one initial refresh at construct:
// required, not optional, because CreateSoloParty's early-return on an existing
// party does not broadcast, so a pure-reactive widget would miss state on hub
// re-entry.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "PartyDisplayWidget.generated.h"

class UCharacterPanelWidget;
class UPanelWidget;
class UParty;
class UPartySessionSubsystem;

UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UPartyDisplayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Re-read the local party and repaint every panel. Members is DENSE — panel
	 *  i is populated when i < GetMemberCount(), vacant otherwise (vacancy is
	 *  derived; UParty has no empty-slot state). Panel 0 gets bIsLeader — slot 0
	 *  is the player per the design lock. Null party → all panels vacant. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	void RefreshFromParty();

	// ==================== DEBUG READ SURFACE ====================

	/** Number of panels actually built (0 until NativeConstruct, or when
	 *  PanelClass / CardContainer are unconfigured). */
	UFUNCTION(BlueprintPure, Category = "Party")
	int32 GetPanelCount() const { return Panels.Num(); }

	/** Panel at Index, or null out of range. */
	UFUNCTION(BlueprintPure, Category = "Party")
	UCharacterPanelWidget *GetPanel(int32 Index) const;

	/** Display name shown on panel Index at the last refresh; empty for
	 *  vacant slots. Cached here — the panel's data accessor is private. */
	UFUNCTION(BlueprintPure, Category = "Party")
	FString GetPanelName(int32 Index) const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** The panel WBP to instantiate (WBP_CharacterPanel or a roster-slimmed
	 *  subclass). Unset = the strip builds no panels and logs once — graceful,
	 *  not fatal. */
	UPROPERTY(EditDefaultsOnly, Category = "Party")
	TSubclassOf<UCharacterPanelWidget> PanelClass;

	// ==================== BINDWIDGET (names must match the WBP tree) ====================

	/** Horizontal/vertical box (or any panel) the panels are added to. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Party|Widgets")
	TObjectPtr<UPanelWidget> CardContainer;

private:
	/** Built once, MAX_PARTY_MEMBERS entries, never recreated (TransBuffer). */
	UPROPERTY()
	TArray<TObjectPtr<UCharacterPanelWidget>> Panels;

	/** Per-panel display name at the last refresh (empty = vacant) — the debug
	 *  read-back, since the panel's GetDisplayData is private. */
	TArray<FString> LastPanelNames;

	/** Build the panel pool if not built yet (no-op after the first call). */
	void EnsurePanels();

	UFUNCTION()
	void HandlePartyChanged(UParty *Party);

	UPartySessionSubsystem *GetPartySession() const;
};
