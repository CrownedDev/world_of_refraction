// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Combat/CharacterPanelWidget.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CharacterData.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Infusion/ClassColors.h"
#include "Infusion/ElementColors.h"
#include "Infusion/HybridSpellColors.h"
#include "Combat/CombatConstants.h"
#include "Party/Party.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "WoRGameNames.h"

namespace PanelLabels
{
	// Bar prefix labels — change these in one place to update the whole panel
	constexpr const TCHAR *HP = TEXT("HP");
	constexpr const TCHAR *EP = TEXT("EP");
	constexpr const TCHAR *RingDur = TEXT("AD");   // Adapter durability (Resonator, D-107)
	constexpr const TCHAR *WeaponDur = TEXT("WD"); // Weapon Durability
	constexpr const TCHAR *Absorb = TEXT("Abs");   // BD Absorption
	constexpr const TCHAR *Status = TEXT("SB");	   // Status Buildup
}

namespace PanelStyle
{
	// TODO: style pass — placeholder palette for the native effect rows.
	const FLinearColor BuffColor(0.35f, 0.9f, 0.45f);
	const FLinearColor DebuffColor(0.95f, 0.35f, 0.3f);
	const FLinearColor NeutralColor(0.85f, 0.85f, 0.85f);
	constexpr float DeadPanelOpacity = 0.4f;
	constexpr float FullPanelOpacity = 1.0f;
}

namespace PanelGhostConstants
{
	/** Vacant-state portrait opacity — ported from PartyCardConstants
	 *  (PartyMemberCardWidget, deleted in the unification). */
	constexpr float DEFAULT_GHOST_ALPHA = 0.25f;
}

UCharacterPanelWidget::UCharacterPanelWidget(const FObjectInitializer &ObjectInitializer)
	: Super(ObjectInitializer)
{
	GhostAlpha = PanelGhostConstants::DEFAULT_GHOST_ALPHA;
}

void UCharacterPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
}

void UCharacterPanelWidget::InitialiseForActor(AActor *InActor)
{
	if (bBound)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterPanel] InitialiseForActor called twice; ignoring"));
		return;
	}

	if (!InActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterPanel] InitialiseForActor called with null actor"));
		return;
	}

	ClearFlags(RF_Transactional);

	UCharacterDataComponent *CharComp = InActor->FindComponentByClass<UCharacterDataComponent>();
	if (!CharComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterPanel] %s has no CharacterDataComponent"),
			   *InActor->GetName());
		return;
	}

	USkillEffectManager *StatusMgr = nullptr;
	UStatusBuildupManager *BuildupMgr = nullptr;
	if (UGameInstance *GI = GetGameInstance())
	{
		StatusMgr = GI->GetSubsystem<USkillEffectManager>();
		BuildupMgr = GI->GetSubsystem<UStatusBuildupManager>();
	}

	BoundActor = InActor;
	BoundCharData = CharComp;
	BoundStatusManager = StatusMgr;
	BoundBuildupManager = BuildupMgr;

	// Bind component delegates
	CharComp->OnHPChanged.AddDynamic(this, &UCharacterPanelWidget::HandleHPChanged);
	CharComp->OnEPChanged.AddDynamic(this, &UCharacterPanelWidget::HandleEPChanged);
	CharComp->OnDied.AddDynamic(this, &UCharacterPanelWidget::HandleDied);

	// Bind global subsystem delegates (filtered per-actor inside the handlers)
	if (StatusMgr)
	{
		StatusMgr->OnEffectApplied.AddDynamic(this, &UCharacterPanelWidget::HandleEffectApplied);
		StatusMgr->OnEffectRemoved.AddDynamic(this, &UCharacterPanelWidget::HandleEffectRemoved);
		StatusMgr->OnEffectDurationChanged.AddDynamic(this, &UCharacterPanelWidget::HandleEffectDurationChanged);
	}
	if (BuildupMgr)
	{
		BuildupMgr->OnStatusBuildupChanged.AddDynamic(this, &UCharacterPanelWidget::HandleStatusBuildupChanged);
	}

	// BD manager binding (for character-created or runtime-transformed BDs)
	if (UBrokenDarknessManager *BDManager = InActor->FindComponentByClass<UBrokenDarknessManager>())
	{
		BoundBDManager = BDManager;
		BDManager->OnEnergyAbsorbed.AddDynamic(this, &UCharacterPanelWidget::HandleBDEnergyAbsorbed);
		BDManager->OnOverloadStateChanged.AddDynamic(this, &UCharacterPanelWidget::HandleBDOverloadStateChanged);
		BDManager->OnStacksChanged.AddDynamic(this, &UCharacterPanelWidget::HandleBDStacksChanged);
		BDManager->OnAlignmentChanged.AddDynamic(this, &UCharacterPanelWidget::HandleBDAlignmentChanged);
		BDManager->OnTransformed.AddDynamic(this, &UCharacterPanelWidget::HandleBDTransformed);
	}

	// Mode transition: a panel previously in Roster/Vacant (which collapse the
	// runtime rows) is now live — restore before the resonator rule re-hides EP.
	DisplayMode = EPanelDisplayMode::Live;
	RosterData = nullptr;
	SetRenderOpacity(PanelStyle::FullPanelOpacity); // undo a previous occupant's death dim
	bRosterLeader = false;
	SetRuntimeRowsVisible(true);

	// Resonator EP-bar visibility: hidden when unarmed (pool is dormant),
	// visible when armed. Initial evaluation only — runtime weapon-swap
	// refresh is not currently wired.
	RefreshEPBarVisibility();

	bBound = true;

	// Static text
	ApplyStaticText();

	// Portrait for the Live path too — GetDisplayData() resolves to the bound
	// component's CharacterData here. Unset portraits (all enemy DAs) show the
	// WBP placeholder, which is what an unset portrait looks like.
	UpdatePortrait(GetDisplayData());

	// Initial snapshot
	HandleHPChanged(CharComp->CurrentHP, CharComp->MaxHP);
	RefreshEnergyBar(); // dispatches to BD / default EP based on character state
	ApplyEnergyBarTint();

	// Status starts at 0 — broadcast doesn't fire until first hit, so seed it
	SetBarSafe(StatusBar, 0.0f);
	SetTextSafe(StatusText, FString::Printf(TEXT("%s:0/100"), PanelLabels::Status));

	// Native rows exist only when no BP graph owns the list.
	if (!GetClass()->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(UCharacterPanelWidget, RebuildEffectsList)))
	{
		SpawnEffectRowPool();
	}

	// Initial effects list (usually empty at combat start). Includes the
	// synthetic BD-stacks row when applicable — no separate call needed.
	RefreshEffectsList();

	UE_LOG(LogTemp, Log, TEXT("[CharacterPanel] Initialised for %s"), *InActor->GetName());
}

void UCharacterPanelWidget::TeardownPanel()
{
	if (!bBound)
	{
		return;
	}

	// Unbind component delegates — Get() pattern for safety
	if (UCharacterDataComponent *CharComp = BoundCharData.Get())
	{
		CharComp->OnHPChanged.RemoveDynamic(this, &UCharacterPanelWidget::HandleHPChanged);
		CharComp->OnEPChanged.RemoveDynamic(this, &UCharacterPanelWidget::HandleEPChanged);
		CharComp->OnDied.RemoveDynamic(this, &UCharacterPanelWidget::HandleDied);
	}

	if (USkillEffectManager *StatusMgr = BoundStatusManager.Get())
	{
		StatusMgr->OnEffectApplied.RemoveDynamic(this, &UCharacterPanelWidget::HandleEffectApplied);
		StatusMgr->OnEffectRemoved.RemoveDynamic(this, &UCharacterPanelWidget::HandleEffectRemoved);
		StatusMgr->OnEffectDurationChanged.RemoveDynamic(this, &UCharacterPanelWidget::HandleEffectDurationChanged);
	}
	if (UStatusBuildupManager *BuildupMgr = BoundBuildupManager.Get())
	{
		BuildupMgr->OnStatusBuildupChanged.RemoveDynamic(this, &UCharacterPanelWidget::HandleStatusBuildupChanged);
	}

	if (UBrokenDarknessManager *BDManager = BoundBDManager.Get())
	{
		BDManager->OnEnergyAbsorbed.RemoveDynamic(this, &UCharacterPanelWidget::HandleBDEnergyAbsorbed);
		BDManager->OnOverloadStateChanged.RemoveDynamic(this, &UCharacterPanelWidget::HandleBDOverloadStateChanged);
		BDManager->OnStacksChanged.RemoveDynamic(this, &UCharacterPanelWidget::HandleBDStacksChanged);
		BDManager->OnAlignmentChanged.RemoveDynamic(this, &UCharacterPanelWidget::HandleBDAlignmentChanged);
		BDManager->OnTransformed.RemoveDynamic(this, &UCharacterPanelWidget::HandleBDTransformed);
	}

	BoundActor.Reset();
	BoundCharData.Reset();
	BoundStatusManager.Reset();
	BoundBDManager.Reset();
	bBound = false;
}

// ========================================
// Roster / Vacant modes (Arc 1.5b unification)
// ========================================

void UCharacterPanelWidget::InitialiseForData(UCharacterData *Data, bool bIsLeader, int32 InCurrentHP, int32 InCurrentEP)
{
	// TransBuffer — the InitialiseForActor precedent: keep the editor's undo
	// buffer from holding this widget across OpenLevel.
	ClearFlags(RF_Transactional);

	if (!Data)
	{
		SetVacant();
		return;
	}

	DisplayMode = EPanelDisplayMode::Roster;
	RosterData = Data;
	SetRenderOpacity(PanelStyle::FullPanelOpacity);
	bRosterLeader = bIsLeader;

	// Static template half only: no components, no subsystems, no delegate
	// binds — bBound stays false. Deliberately NOT RefreshEPBarVisibility:
	// it reads HasUsableEPTarget(), live loadout state with no asset answer.
	ApplyStaticText();
	UpdatePortrait(Data);

	// Hub vs trial, discriminated by the carry sentinel: an uncarried slot is a hub
	// lobby entry and shows identity only (name + class + portrait); a carried slot
	// is mid-trial and shows runtime state. Same test as ApplyRosterVitals' bHasCarry
	// — HP is the carry signal for BOTH pools, so one test covers them.
	const bool bHasCarry = (InCurrentHP != PartyConstants::NO_CARRIED_VITALS);

	// Status stays collapsed in BOTH cases, unlike the vitals rows: buildup is
	// battle-only and is cleared between battles, so a trial roster has nothing to
	// show either. The effects page stays unreachable (mouse handlers gate on Live).
	SetVitalsRowsVisible(bHasCarry);
	SetStatusRowsVisible(false);

	// World stats track the vitals, not status — its own setter now, so the two
	// no longer have to share a visibility lifetime.
	SetWorldStatsRowVisible(bHasCarry);

	// Called unconditionally even when the rows are collapsed: it leaves the bars
	// consistent with the slot, so a later carry→display flip has no stale frame.
	ApplyRosterVitals(Data, InCurrentHP, InCurrentEP);
}

void UCharacterPanelWidget::ApplyRosterVitals(UCharacterData *Data, int32 InCurrentHP, int32 InCurrentEP)
{
	if (!Data)
	{
		return;
	}

	const int32 MaxHP = Data->CalculateMaxHealth();
	const int32 MaxEP = Data->CalculateMaxEnergy();

	// HP is the carry signal for BOTH pools — Cluster 1 writes them together, so
	// a valid HP implies a valid EP and one test covers both.
	const bool bHasCarry = (InCurrentHP != PartyConstants::NO_CARRIED_VITALS);

	// Clamp: a carry banked before an equipment or level change can exceed a max
	// that has since moved. Mirrors the same guard in UCharacterDataComponent.
	const int32 ShowHP = bHasCarry ? FMath::Clamp(InCurrentHP, 0, MaxHP) : MaxHP;

	// Uncarried BD reads 0, not MaxEP: a born-BD has no regular EP pool, and 0 is
	// exactly what SetAllMembersFull will seed on trial entry. Showing MaxEP here
	// would make the hub panel disagree with the first battle.
	const int32 UncarriedEP = Data->bBrokenDarknessInnate ? 0 : MaxEP;
	const int32 ShowEP = bHasCarry ? FMath::Clamp(InCurrentEP, 0, MaxEP) : UncarriedEP;

	// Same formatter the Live path uses, so the two modes read identically.
	HandleHPChanged(ShowHP, MaxHP);

	// EP is written directly rather than via RefreshEnergyBar: that reads live
	// loadout / BD-manager state off a bound component, which Roster has none of.
	// The asset-side bBrokenDarknessInnate flag picks the label instead.
	const float EPPercent = (MaxEP > 0) ? (static_cast<float>(ShowEP) / static_cast<float>(MaxEP)) : 0.0f;
	SetBarSafe(EPBar, EPPercent);
	SetTextSafe(EPText, FString::Printf(TEXT("%s:%d/%d"),
										Data->bBrokenDarknessInnate ? PanelLabels::Absorb : PanelLabels::EP,
										ShowEP, MaxEP));
}

void UCharacterPanelWidget::SetVacant()
{
	// TransBuffer — see InitialiseForData; either entry point may be hit first.
	ClearFlags(RF_Transactional);

	DisplayMode = EPanelDisplayMode::Vacant;
	RosterData = nullptr;
	SetRenderOpacity(PanelStyle::FullPanelOpacity);
	bRosterLeader = false;

	SetTextSafe(NameText, TEXT("Empty"));
	SetTextSafe(ClassElementText, FString());
	SetTextSafe(WorldStatsText, FString());
	SetRuntimeRowsVisible(false);

	if (PortraitImage)
	{
		CachePortraitBrushOnce();
		// Placeholder brush back, dimmed — a vacant slot shows no character.
		PortraitImage->SetBrush(DefaultPortraitBrush);
		PortraitImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, GhostAlpha));
	}
}

UCharacterData *UCharacterPanelWidget::GetDisplayData() const
{
	switch (DisplayMode)
	{
	case EPanelDisplayMode::Live:
	{
		const UCharacterDataComponent *CharComp = BoundCharData.Get();
		return CharComp ? CharComp->CharacterData : nullptr;
	}
	case EPanelDisplayMode::Roster:
		return RosterData;
	default:
		return nullptr;
	}
}

void UCharacterPanelWidget::SetRuntimeRowsVisible(bool bVisible)
{
	// All three groups together — the Live and Vacant callers want all-or-nothing.
	// Roster drives them separately.
	SetVitalsRowsVisible(bVisible);
	SetStatusRowsVisible(bVisible);
	SetWorldStatsRowVisible(bVisible);
}

void UCharacterPanelWidget::SetVitalsRowsVisible(bool bVisible)
{
	const ESlateVisibility V = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (HPBar)      { HPBar->SetVisibility(V); }
	if (HPText)     { HPText->SetVisibility(V); }
	if (EPBar)      { EPBar->SetVisibility(V); }
	if (EPText)     { EPText->SetVisibility(V); }
}

void UCharacterPanelWidget::SetStatusRowsVisible(bool bVisible)
{
	const ESlateVisibility V = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (StatusBar)  { StatusBar->SetVisibility(V); }
	if (StatusText) { StatusText->SetVisibility(V); }
}

void UCharacterPanelWidget::SetWorldStatsRowVisible(bool bVisible)
{
	// Own setter, not bundled with status: world stats track the VITALS lifetime
	// (shown in-trial and in Live, hidden in a hub lobby), whereas status buildup
	// is battle-only and stays collapsed outside Live.
	const ESlateVisibility V = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (WorldStatsText) { WorldStatsText->SetVisibility(V); }
}

void UCharacterPanelWidget::CachePortraitBrushOnce()
{
	if (!bPortraitBrushCached && PortraitImage)
	{
		DefaultPortraitBrush = PortraitImage->GetBrush();
		bPortraitBrushCached = true;
	}
}

void UCharacterPanelWidget::UpdatePortrait(UCharacterData *Data)
{
	if (!PortraitImage)
	{
		return;
	}
	CachePortraitBrushOnce();
	if (Data && Data->Portrait)
	{
		PortraitImage->SetBrushFromTexture(Data->Portrait);
	}
	else
	{
		// Null Portrait is the NORMAL case today (placeholder gems aside) — the
		// WBP's own placeholder holds the layout space, no warning.
		PortraitImage->SetBrush(DefaultPortraitBrush);
	}
	PortraitImage->SetColorAndOpacity(FLinearColor::White);
}

void UCharacterPanelWidget::NativeDestruct()
{
	if (bBound)
	{
		TeardownPanel();
	}
	Super::NativeDestruct();
}

void UCharacterPanelWidget::BeginDestroy()
{
	if (bBound)
	{
		TeardownPanel();
	}
	Super::BeginDestroy();
}

// ========================================
// Static text — BP can override
// ========================================

void UCharacterPanelWidget::ApplyStaticText_Implementation()
{
	// Mode-agnostic data source: Live reads the bound component's asset, Roster
	// reads RosterData. Formatting matches the (to-be-deleted) WBP override
	// exactly — "Class | Element" and "M:x B:y S:z" via enum display names.
	UCharacterData *Data = GetDisplayData();
	if (!Data)
	{
		return;
	}

	const FString DisplayName = bRosterLeader
									? FString::Printf(TEXT("%s (You)"), *Data->Name)
									: Data->Name;
	SetTextSafe(NameText, DisplayName);

	// Element token only for Casters — Generic/Resonator carry
	// InnateElement=Generic, so "Generic | Generic" is noise. BD is a Caster
	// (Darkness + bBrokenDarknessInnate) and keeps its element token; BD-specific
	// display is banked with the wider BD arc.
	const FString ClassLabel = WoRGameNames::GetCharacterClassName(Data->CharacterClass).ToString();
	SetTextSafe(ClassElementText,
				Data->CharacterClass == ECharacterClass::Caster
					? FString::Printf(TEXT("%s | %s"), *ClassLabel,
									  *WoRGameNames::GetElementName(Data->InnateElement).ToString())
					: ClassLabel);
	if (ClassElementText)
	{
		// Class-identity tint: Casters via their innate element, Generic brown,
		// Resonator silver. Runs for Roster and Live alike (both route here);
		// Vacant leaves the text blank, no tint needed.
		ClassElementText->SetColorAndOpacity(ClassColors::GetColorForCharacter(Data));
	}

	// Live reads the component's run values (base + head start); Roster has only the asset.
	const UCharacterDataComponent *LiveComp = DisplayMode == EPanelDisplayMode::Live ? BoundCharData.Get() : nullptr;
	SetTextSafe(WorldStatsText,
				LiveComp
					? FString::Printf(TEXT("M:%d B:%d S:%d"),
									  LiveComp->GetWorldMind(), LiveComp->GetWorldBody(), LiveComp->GetWorldSpirit())
					: FString::Printf(TEXT("M:%d B:%d S:%d"),
									  Data->WorldMindLevel, Data->WorldBodyLevel, Data->WorldSpiritLevel));
}

void UCharacterPanelWidget::SpawnEffectRowPool()
{
	if (!EffectsList || !WidgetTree || EffectRowPool.Num() > 0)
	{
		return;
	}

	// AddChild Modify()s the container — strip it too, not just the rows (TransBuffer crash, CLAUDE.md).
	EffectsList->ClearFlags(RF_Transactional);

	EffectRowPool.Reserve(MaxEffectRows);
	for (int32 i = 0; i < MaxEffectRows; ++i)
	{
		UTextBlock *Row = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		if (!Row)
		{
			continue;
		}
		Row->ClearFlags(RF_Transactional);
		Row->SetVisibility(ESlateVisibility::Collapsed);
		EffectsList->AddChild(Row);
		EffectRowPool.Add(Row);
	}
}

void UCharacterPanelWidget::RebuildEffectsList_Implementation(const TArray<FActiveSkillEffect> &ActiveEffects)
{
	const int32 RowCount = EffectRowPool.Num();
	const bool bOverflow = ActiveEffects.Num() > RowCount;
	const int32 DetailedRows = bOverflow ? RowCount - 1 : ActiveEffects.Num();

	for (int32 i = 0; i < RowCount; ++i)
	{
		UTextBlock *Row = EffectRowPool[i];
		if (!Row)
		{
			continue;
		}

		if (i < DetailedRows)
		{
			const FActiveSkillEffect &Effect = ActiveEffects[i];
			const FString Stacks = Effect.GetStackString();
			Row->SetText(FText::FromString(FString::Printf(TEXT("%s%s%s (%s)"),
														   *Effect.EffectName,
														   Stacks.IsEmpty() ? TEXT("") : TEXT(" "),
														   *Stacks,
														   *Effect.GetDurationString())));
			const FLinearColor Tint = Effect.IsBuff()	  ? PanelStyle::BuffColor
									  : Effect.IsDebuff() ? PanelStyle::DebuffColor
														  : PanelStyle::NeutralColor;
			Row->SetColorAndOpacity(FSlateColor(Tint));
			Row->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else if (bOverflow && i == RowCount - 1)
		{
			Row->SetText(FText::FromString(FString::Printf(TEXT("+%d more"), ActiveEffects.Num() - DetailedRows)));
			Row->SetColorAndOpacity(FSlateColor(PanelStyle::NeutralColor));
			Row->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Row->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

// ========================================
// Delegate handlers
// ========================================

void UCharacterPanelWidget::HandleHPChanged(int32 CurrentHP, int32 MaxHP)
{
	const float Percent = (MaxHP > 0) ? (static_cast<float>(CurrentHP) / static_cast<float>(MaxHP)) : 0.0f;
	SetBarSafe(HPBar, Percent);
	SetTextSafe(HPText, FString::Printf(TEXT("%s:%d/%d"), PanelLabels::HP, CurrentHP, MaxHP));
}

void UCharacterPanelWidget::HandleEPChanged(int32 CurrentEP, int32 MaxEP)
{
	// EP changed — but the bar might be displaying BD absorption or ring
	// durability instead. Defer to RefreshEnergyBar to pick the right source.
	RefreshEnergyBar();
}

void UCharacterPanelWidget::HandleBDEnergyAbsorbed(AActor *Actor, float AmountAbsorbed, ESpellElement AbsorbedElement)
{
	if (Actor != BoundActor.Get())
		return;
	RefreshEnergyBar();
	ApplyEnergyBarTint(); // absorbed element may have changed
}

void UCharacterPanelWidget::HandleBDOverloadStateChanged(AActor *Actor, bool bIsOverloaded)
{
	if (Actor != BoundActor.Get())
		return;
	RefreshEnergyBar();
}

void UCharacterPanelWidget::HandleBDStacksChanged(AActor *Actor, ESpellElement Element, int32 NewStackCount)
{
	if (Actor != BoundActor.Get())
		return;
	RefreshEffectsList();
}

void UCharacterPanelWidget::HandleBDAlignmentChanged(AActor *Actor, ESpellElement OldElement, ESpellElement NewElement)
{
	if (Actor != BoundActor.Get())
		return;
	// The EP/Absorb bar colour is a function of the active pool (alignment), so re-tint
	// here when the pool rotates — the bar must not depend on OnEnergyAbsorbed co-firing
	// to re-tint (today it always does, but a non-absorption rotation must still update).
	ApplyEnergyBarTint();
	RefreshEffectsList();
}

void UCharacterPanelWidget::HandleBDTransformed(AActor *Actor)
{
	if (Actor != BoundActor.Get())
		return;

	// Refresh every BD-affected display so the panel reflects the new state
	// the moment a Darkness character transforms — energy bar tint + label
	// swap to BD's absorption pool; effects list rebuilt so any synthetic
	// stack row appears/clears in sync with the new BD state.
	RefreshEnergyBar();
	ApplyEnergyBarTint();
	RefreshEffectsList();
}

void UCharacterPanelWidget::HandleStatusBuildupChanged(AActor *Target, float Current, float Max, ESpellElement PendingElement, AActor *Source)
{
	if (Target != BoundActor.Get())
	{
		return;
	}
	const float Percent = (Max > 0.0f) ? (Current / Max) : 0.0f;
	SetBarSafe(StatusBar, Percent);
	SetTextSafe(StatusText, FString::Printf(TEXT("%s:%d/%d"), PanelLabels::Status, FMath::TruncToInt(Current), FMath::TruncToInt(Max)));
	// Retint per most-recent-hit element — pending element can shift when a
	// different element lands on the same target.
	ApplyStatusBarTint(PendingElement, Source);
}

void UCharacterPanelWidget::ApplyStatusBarTint(ESpellElement PendingElement, AActor *Source)
{
	if (!StatusBar)
		return;

	// None / Generic = physical-only damage (Slash/Pierce/Impact) — no element to
	// surface, and nothing for a BD attacker to darken, so always the neutral
	// (brown) Generic colour, matching every other surface. Early-return so physical
	// hits skip the BD-darkening branch below.
	if (PendingElement == ESpellElement::None || PendingElement == ESpellElement::Generic)
	{
		StatusBar->SetFillColorAndOpacity(ElementColors::GetColorForElement(PendingElement));
		return;
	}

	// BD attacker — darkened element colour.
	if (Source)
	{
		UBrokenDarknessManager *BDManager = Source->FindComponentByClass<UBrokenDarknessManager>();
		UCharacterDataComponent *SourceChar = Source->FindComponentByClass<UCharacterDataComponent>();
		if (BDManager && BDManager->IsTransformed() && SourceChar && SourceChar->IsBrokenDarkness())
		{
			StatusBar->SetFillColorAndOpacity(
				UHybridSpellColors::GetHybridSpellColors(PendingElement).BlendedColor);
			return;
		}
	}

	// Non-BD attacker (or null Source) — raw element colour. (BD-source tinting handled above
	// via IsBrokenDarkness(); PendingElement is always a real spell element here.)
	StatusBar->SetFillColorAndOpacity(ElementColors::GetColorForElement(PendingElement));
}

void UCharacterPanelWidget::HandleEffectApplied(AActor *Target, const FActiveSkillEffect &Effect)
{
	if (Target != BoundActor.Get())
	{
		return;
	}
	RefreshEffectsList();
}

void UCharacterPanelWidget::HandleEffectRemoved(AActor *Target, const FActiveSkillEffect &Effect)
{
	if (Target != BoundActor.Get())
	{
		return;
	}
	RefreshEffectsList();
}

void UCharacterPanelWidget::HandleEffectDurationChanged(AActor *Target, const FActiveSkillEffect &Effect, int32 RemainingTurns)
{
	if (Target != BoundActor.Get())
	{
		return;
	}
	RefreshEffectsList();
}

void UCharacterPanelWidget::HandleDied(AActor *DeadActor)
{
	if (DeadActor != BoundActor.Get())
	{
		return;
	}
	SetRenderOpacity(PanelStyle::DeadPanelOpacity);
	TeardownPanel();
}

// ========================================
// Internal helpers
// ========================================

void UCharacterPanelWidget::RefreshEffectsList()
{
	USkillEffectManager *StatusMgr = BoundStatusManager.Get();
	AActor *Actor = BoundActor.Get();

	if (!StatusMgr || !Actor)
	{
		RebuildEffectsList(TArray<FActiveSkillEffect>());
		return;
	}

	TArray<FActiveSkillEffect> Effects = StatusMgr->GetActiveEffects(Actor);

	// BD absorption stacks function as a status-multiplier buff on
	// matching-element spells (UBrokenDarknessManager::GetElementStackStatusMultiplier,
	// wired into UStatusBuildupManager::AddStatusBuildup as step 5c).
	// Surface them via the same pipeline as real skill effects: append a
	// synthetic StatusMultiplierBuff entry so the existing BP row widget +
	// SkillEffectBlueprintLibrary helpers render it (IsBuff → buff tint,
	// GetStackString → "xN"). Auto-clears: when stacks drop to 0 or the
	// character is no longer BD, the next refresh just doesn't append it.
	UCharacterDataComponent *CharComp = BoundCharData.Get();
	UBrokenDarknessManager *BDManager = BoundBDManager.Get();
	if (CharComp && CharComp->IsBrokenDarkness() && BDManager && BDManager->IsTransformed())
	{
		const int32 StackCount = BDManager->GetCurrentStackCount();
		if (StackCount > 0)
		{
			const ESpellElement Element = BDManager->GetCurrentAlignment();
			const FString ElementName = WoRGameNames::GetElementName(Element).ToString();

			FActiveSkillEffect StackEntry;
			StackEntry.EffectName = ElementName;
			StackEntry.EffectType = ESkillEffectType::StatusMultiplierBuff;
			StackEntry.Element = Element;
			StackEntry.bCanStack = true;
			StackEntry.CurrentStacks = StackCount;
			StackEntry.MaxStacks = BDManager->GetMaxStacks();
			StackEntry.bPermanent = true;
			Effects.Add(StackEntry);
		}
	}

	RebuildEffectsList(Effects);
}

void UCharacterPanelWidget::SetBarSafe(UProgressBar *Bar, float Percent)
{
	if (Bar)
	{
		Bar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
	}
}

void UCharacterPanelWidget::SetTextSafe(UTextBlock *Text, const FString &Value)
{
	if (Text)
	{
		Text->SetText(FText::FromString(Value));
	}
}

// ========================================
// Energy bar dispatcher + tint
// ========================================

void UCharacterPanelWidget::RefreshEnergyBar()
{
	UCharacterDataComponent *CharComp = BoundCharData.Get();
	if (!CharComp)
	{
		SetBarSafe(EPBar, 0.0f);
		SetTextSafe(EPText, FString::Printf(TEXT("%s:--/--"), PanelLabels::EP));
		return;
	}

	// --- BD absorption energy path: CurrentEP is the BD spend pool, labelled
	//     "Absorb" to distinguish it from regular regenerating EP. The bar may
	//     read over 100% briefly during overload (CurrentEP > MaxEP) — clamped. ---
	if (CharComp->IsBrokenDarkness())
	{
		const int32 Current = CharComp->CurrentEP;
		const int32 Max = CharComp->MaxEP;
		const float Percent = (Max > 0)
								  ? FMath::Min(static_cast<float>(Current) / static_cast<float>(Max), 1.0f)
								  : 0.0f;

		SetBarSafe(EPBar, Percent);
		SetTextSafe(EPText, FString::Printf(TEXT("%s:%d/%d"), PanelLabels::Absorb, Current, Max));

		// Overload text tint — escalates as CurrentEP / MaxEP rises past 1.0.
		// Bar percent is clamped at 1.0 above, so the only visual signal that
		// energy has exceeded the cap is this text color. Thresholds in
		// CombatConstants. Reset to white when not overloaded.
		if (EPText && Max > 0 && Current > Max)
		{
			const float OverPct = static_cast<float>(Current) / static_cast<float>(Max);
			FLinearColor TextColor = FLinearColor::White;
			if (OverPct > CombatConstants::OVERLOAD_RED_THRESHOLD)
			{
				TextColor = FLinearColor::Red;
			}
			else if (OverPct > CombatConstants::OVERLOAD_ORANGE_THRESHOLD)
			{
				TextColor = FLinearColor(1.0f, 0.5f, 0.0f); // orange
			}
			else if (OverPct > CombatConstants::OVERLOAD_YELLOW_THRESHOLD)
			{
				TextColor = FLinearColor::Yellow;
			}
			EPText->SetColorAndOpacity(FSlateColor(TextColor));
		}
		else if (EPText)
		{
			EPText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}
		return;
	}

	// --- Default: regular EP ---
	{
		const int32 Current = CharComp->CurrentEP;
		const int32 Max = CharComp->MaxEP;
		const float Percent = (Max > 0) ? (static_cast<float>(Current) / static_cast<float>(Max)) : 0.0f;

		SetBarSafe(EPBar, Percent);
		SetTextSafe(EPText, FString::Printf(TEXT("%s:%d/%d"), PanelLabels::EP, Current, Max));

		// Non-BD: ensure text stays default-white. Cheap idempotent reset so a
		// runtime transform from non-BD → BD that later returns to default
		// (impossible today but defensive) doesn't leave a stuck colour.
		if (EPText)
		{
			EPText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}
	}
}

void UCharacterPanelWidget::ApplyEnergyBarTint()
{
	if (!EPBar)
		return;

	UCharacterDataComponent *CharComp = BoundCharData.Get();
	if (!CharComp || !CharComp->CharacterData)
	{
		EPBar->SetFillColorAndOpacity(FLinearColor::White);
		return;
	}

	FLinearColor BarColour = FLinearColor::White;

	// --- BD: the EP/Absorb bar tints to the hybrid (darkness-blended) colour of the
	// ACTIVE pool — Dark Fire (darkened red) on Fire, Dark Wind on Wind, etc. The base
	// Darkness pool routes through the SAME path to pure-BD near-black
	// (GetHybridSpellColors(Darkness) -> GetPureBrokenDarknessColors), so no special-case
	// is needed. This mirrors ApplyStatusBarTint so the EP bar and status bar agree on
	// the hybrid colour for a given active element. ---
	if (CharComp->IsBrokenDarkness())
	{
		UBrokenDarknessManager *BDManager = BoundBDManager.Get();
		if (BDManager)
		{
			BarColour = UHybridSpellColors::GetHybridSpellColors(BDManager->GetActivePool()).BlendedColor;
		}
		else
		{
			BarColour = ElementColors::BrokenDarkness; // null-guard: fall back to BD black
		}
		EPBar->SetFillColorAndOpacity(BarColour);
		return;
	}

	// --- Default: character's innate element ---
	const ESpellElement Element = CharComp->CharacterData->InnateElement;
	BarColour = ElementColors::GetColorForElement(Element);
	EPBar->SetFillColorAndOpacity(BarColour);
}

void UCharacterPanelWidget::RefreshEPBarVisibility()
{
	UCharacterDataComponent *CharComp = BoundCharData.Get();
	if (!CharComp || !CharComp->CharacterData)
		return;

	// Rule: Resonator without a usable EP-spend target → hide EP bar+text.
	// Their pool exists but is dormant. Usable target = active weapon
	// (weapon attacks cost EP) OR Evolution primary (Evolution spells cost
	// EP per locked design). Other classes always show. BD characters are
	// handled by their own absorption-energy path inside RefreshEnergyBar.
	const bool bIsResonatorDormant =
		CharComp->CharacterData->CharacterClass == ECharacterClass::Resonator &&
		!CharComp->HasUsableEPTarget();

	const ESlateVisibility V = bIsResonatorDormant
								   ? ESlateVisibility::Collapsed
								   : ESlateVisibility::Visible;

	if (EPBar)
	{
		EPBar->SetVisibility(V);
	}
	if (EPText)
	{
		EPText->SetVisibility(V);
	}
}

void UCharacterPanelWidget::NativeOnMouseEnter(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	// Live-only: a Roster/Vacant panel would page the WBP switcher to an empty
	// effects list on hover. Gating here keeps the BP graph untouched.
	// Roster panels have no effects to page to, so hover/click is inert
	// here. TODO: repurpose page 1 in Roster mode for a loadout + full-stat
	// preview -- both are UCharacterData reads, no runtime state needed.
	// Requires a third WidgetSwitcher page in WBP_CharacterPanel.
	if (DisplayMode != EPanelDisplayMode::Live)
	{
		return;
	}
	OnPanelHovered.Broadcast(true);
}

void UCharacterPanelWidget::NativeOnMouseLeave(const FPointerEvent &InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	if (DisplayMode != EPanelDisplayMode::Live)
	{
		return;
	}
	OnPanelHovered.Broadcast(false);
}

FReply UCharacterPanelWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	// Live-only: a Roster/Vacant panel would pin the empty effects page.
	// Unhandled lets a future roster-mode parent take the click instead.
	if (DisplayMode != EPanelDisplayMode::Live)
	{
		return FReply::Unhandled();
	}
	OnPanelClicked.Broadcast();
	return FReply::Handled();
}