// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Combat/CharacterPanelWidget.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "StatusEffectManager.h"
#include "StatusEffect.h"
#include "BrokenDarknessManager.h"
#include "WeaponManager.h"
#include "ElementColors.h"
#include "HybridSpellColors.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"

namespace PanelLabels
{
	// Bar prefix labels — change these in one place to update the whole panel
	constexpr const TCHAR *HP = TEXT("HP");
	constexpr const TCHAR *EP = TEXT("EP");
	constexpr const TCHAR *RingDur = TEXT("RD");   // Ring Durability (Resonator)
	constexpr const TCHAR *WeaponDur = TEXT("WD"); // Weapon Durability
	constexpr const TCHAR *Absorb = TEXT("Abs");   // BD Absorption
	constexpr const TCHAR *Status = TEXT("SB");	   // Status Buildup
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

	UStatusEffectManager *StatusMgr = nullptr;
	if (UGameInstance *GI = GetGameInstance())
	{
		StatusMgr = GI->GetSubsystem<UStatusEffectManager>();
	}

	BoundActor = InActor;
	BoundCharData = CharComp;
	BoundStatusManager = StatusMgr;

	// Bind component delegates
	CharComp->OnHPChanged.AddDynamic(this, &UCharacterPanelWidget::HandleHPChanged);
	CharComp->OnEPChanged.AddDynamic(this, &UCharacterPanelWidget::HandleEPChanged);
	CharComp->OnDied.AddDynamic(this, &UCharacterPanelWidget::HandleDied);

	// Bind global subsystem delegates (filtered per-actor inside the handlers)
	if (StatusMgr)
	{
		StatusMgr->OnStatusBuildupChanged.AddDynamic(this, &UCharacterPanelWidget::HandleStatusBuildupChanged);
		StatusMgr->OnEffectApplied.AddDynamic(this, &UCharacterPanelWidget::HandleEffectApplied);
		StatusMgr->OnEffectRemoved.AddDynamic(this, &UCharacterPanelWidget::HandleEffectRemoved);
		StatusMgr->OnEffectDurationChanged.AddDynamic(this, &UCharacterPanelWidget::HandleEffectDurationChanged);
	}

	// BD manager binding (for character-created or runtime-transformed BDs)
	if (UBrokenDarknessManager *BDManager = InActor->FindComponentByClass<UBrokenDarknessManager>())
	{
		BoundBDManager = BDManager;
		BDManager->OnEnergyAbsorbed.AddDynamic(this, &UCharacterPanelWidget::HandleBDEnergyAbsorbed);
		BDManager->OnOverloadStateChanged.AddDynamic(this, &UCharacterPanelWidget::HandleBDOverloadStateChanged);
	}

	// Weapon manager — used to refresh EP-bar visibility on weapon swap.
	// Bound for all characters; the handler is a no-op for non-Resonators
	// since their EP-bar visibility doesn't depend on weapon state.
	if (UGameInstance *GI = GetGameInstance())
	{
		if (UWeaponManager *WeaponMgr = GI->GetSubsystem<UWeaponManager>())
		{
			BoundWeaponManager = WeaponMgr;
			WeaponMgr->OnWeaponSwitched.AddDynamic(this, &UCharacterPanelWidget::HandleWeaponSwitched);
		}
	}

	// Resonator EP-bar visibility: hidden when unarmed (pool is dormant),
	// visible when armed. Re-evaluated on every weapon switch via
	// HandleWeaponSwitched. Centralised in RefreshEPBarVisibility.
	RefreshEPBarVisibility();

	bBound = true;

	// Static text
	ApplyStaticText();

	// Initial snapshot
	HandleHPChanged(CharComp->CurrentHP, CharComp->MaxHP);
	RefreshEnergyBar(); // dispatches to BD / default EP based on character state
	ApplyEnergyBarTint();

	// Status starts at 0 — broadcast doesn't fire until first hit, so seed it
	SetBarSafe(StatusBar, 0.0f);
	SetTextSafe(StatusText, FString::Printf(TEXT("%s:0/100"), PanelLabels::Status));

	// Initial buff/debuff list (usually empty at combat start)
	RefreshBuffDebuffList();

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

	if (UStatusEffectManager *StatusMgr = BoundStatusManager.Get())
	{
		StatusMgr->OnStatusBuildupChanged.RemoveDynamic(this, &UCharacterPanelWidget::HandleStatusBuildupChanged);
		StatusMgr->OnEffectApplied.RemoveDynamic(this, &UCharacterPanelWidget::HandleEffectApplied);
		StatusMgr->OnEffectRemoved.RemoveDynamic(this, &UCharacterPanelWidget::HandleEffectRemoved);
		StatusMgr->OnEffectDurationChanged.RemoveDynamic(this, &UCharacterPanelWidget::HandleEffectDurationChanged);
	}

	if (UBrokenDarknessManager *BDManager = BoundBDManager.Get())
	{
		BDManager->OnEnergyAbsorbed.RemoveDynamic(this, &UCharacterPanelWidget::HandleBDEnergyAbsorbed);
		BDManager->OnOverloadStateChanged.RemoveDynamic(this, &UCharacterPanelWidget::HandleBDOverloadStateChanged);
	}

	if (UWeaponManager *WeaponMgr = BoundWeaponManager.Get())
	{
		WeaponMgr->OnWeaponSwitched.RemoveDynamic(this, &UCharacterPanelWidget::HandleWeaponSwitched);
	}

	BoundActor.Reset();
	BoundCharData.Reset();
	BoundStatusManager.Reset();
	BoundBDManager.Reset();
	BoundWeaponManager.Reset();
	bBound = false;
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
	UCharacterDataComponent *CharComp = BoundCharData.Get();
	if (!CharComp || !CharComp->CharacterData)
	{
		return;
	}

	UCharacterData *Data = CharComp->CharacterData;
	SetTextSafe(NameText, Data->CharacterName);
	// ClassElementText, WorldStatsText left to BP override (enum formatting)
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

void UCharacterPanelWidget::HandleWeaponSwitched(AActor *Actor, EWeaponSlot OldSlot, EWeaponSlot NewSlot)
{
	if (Actor != BoundActor.Get())
		return;

	// Weapon state changed for this character — Resonator EP-bar visibility
	// depends on whether they're armed. Re-evaluate. Also refresh the bar
	// value/tint in case the bar just became visible or changed source.
	RefreshEPBarVisibility();
	RefreshEnergyBar();
	ApplyEnergyBarTint();
}

void UCharacterPanelWidget::HandleStatusBuildupChanged(AActor *Target, float Current, float Max)
{
	if (Target != BoundActor.Get())
	{
		return;
	}
	const float Percent = (Max > 0.0f) ? (Current / Max) : 0.0f;
	SetBarSafe(StatusBar, Percent);
	SetTextSafe(StatusText, FString::Printf(TEXT("%s:%d/%d"), PanelLabels::Status, FMath::TruncToInt(Current), FMath::TruncToInt(Max)));
}

void UCharacterPanelWidget::HandleEffectApplied(AActor *Target, const FStatusEffect &Effect)
{
	if (Target != BoundActor.Get())
	{
		return;
	}
	RefreshBuffDebuffList();
}

void UCharacterPanelWidget::HandleEffectRemoved(AActor *Target, const FStatusEffect &Effect)
{
	if (Target != BoundActor.Get())
	{
		return;
	}
	RefreshBuffDebuffList();
}

void UCharacterPanelWidget::HandleEffectDurationChanged(AActor *Target, const FStatusEffect &Effect, int32 RemainingTurns)
{
	if (Target != BoundActor.Get())
	{
		return;
	}
	RefreshBuffDebuffList();
}

void UCharacterPanelWidget::HandleDied(AActor *DeadActor)
{
	if (DeadActor != BoundActor.Get())
	{
		return;
	}
	// Visual response is BP's responsibility; we just teardown bindings.
	TeardownPanel();
}

// ========================================
// Internal helpers
// ========================================

void UCharacterPanelWidget::RefreshBuffDebuffList()
{
	UStatusEffectManager *StatusMgr = BoundStatusManager.Get();
	AActor *Actor = BoundActor.Get();

	if (!StatusMgr || !Actor)
	{
		RebuildBuffDebuffList(TArray<FStatusEffect>());
		return;
	}

	RebuildBuffDebuffList(StatusMgr->GetActiveEffects(Actor));
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

	// --- BD absorption energy path ---
	if (CharComp->IsBrokenDarkness())
	{
		UBrokenDarknessManager *BDManager = BoundBDManager.Get();
		if (BDManager)
		{
			const float Current = BDManager->GetAbsorptionEnergy();
			const float Max = BDManager->GetMaxAbsorptionEnergy();
			const float Percent = (Max > 0.0f) ? FMath::Min(Current / Max, 1.0f) : 0.0f;

			SetBarSafe(EPBar, Percent);
			SetTextSafe(EPText, FString::Printf(TEXT("%s:%d/%d"),
												PanelLabels::Absorb,
												FMath::TruncToInt(Current),
												FMath::TruncToInt(Max)));
			return;
		}
		// BD without manager — empty
		SetBarSafe(EPBar, 0.0f);
		SetTextSafe(EPText, FString::Printf(TEXT("%s:--/--"), PanelLabels::Absorb));
		return;
	}

	// --- Default: regular EP ---
	{
		const int32 Current = CharComp->CurrentEP;
		const int32 Max = CharComp->MaxEP;
		const float Percent = (Max > 0) ? (static_cast<float>(Current) / static_cast<float>(Max)) : 0.0f;

		SetBarSafe(EPBar, Percent);
		SetTextSafe(EPText, FString::Printf(TEXT("%s:%d/%d"), PanelLabels::EP, Current, Max));
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

	// --- BD: darkened absorbed-element colour (or pure BD black if no absorption) ---
	if (CharComp->IsBrokenDarkness())
	{
		UBrokenDarknessManager *BDManager = BoundBDManager.Get();
		if (BDManager)
		{
			const ESpellElement AbsorbedElement = BDManager->GetHybridElement();
			if (AbsorbedElement == ESpellElement::Generic ||
				AbsorbedElement == ESpellElement::BrokenDarkness)
			{
				BarColour = ElementColors::BrokenDarkness;
			}
			else
			{
				FHybridSpellColorData ColourData = UHybridSpellColors::GetHybridSpellColors(AbsorbedElement);
				BarColour = ColourData.BlendedColor;
			}
		}
		else
		{
			BarColour = ElementColors::BrokenDarkness;
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

	// Rule: Resonator without an active weapon → hide EP bar+text.
	// Their pool exists but is dormant (no usable spend target).
	// All other states (any non-Resonator class, OR Resonator-with-weapon)
	// → show. BD characters are handled by their own absorption-energy
	// path inside RefreshEnergyBar; visibility stays on for them.
	const bool bIsResonatorUnarmed =
		CharComp->CharacterData->CharacterClass == ECharacterClass::Resonator &&
		CharComp->GetActiveWeapon() == nullptr;

	const ESlateVisibility V = bIsResonatorUnarmed
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