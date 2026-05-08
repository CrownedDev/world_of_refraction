// DurabilityHeaderWidget.cpp

#include "UI/Combat/CommandMenu/DurabilityHeaderWidget.h"
#include "RingManager.h"
#include "RingData.h"
#include "ItemData.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "WeaponManager.h"
#include "WeaponData.h"
#include "LoadoutComponent.h"
#include "FCombatLoadout.h"
#include "FRingLoadoutEntry.h"

namespace HeaderLabels
{
	constexpr const TCHAR* RingDur   = TEXT("RD");
	constexpr const TCHAR* WeaponDur = TEXT("WD");  // Reserved for Item 32
}

void UDurabilityHeaderWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ClearFlags(RF_Transactional);

	// Default state — both lines hidden until Refresh is called
	HideSlot1();
	HideSlot2();
}

void UDurabilityHeaderWidget::BeginDestroy()
{
	UnbindRingManager();
	UnbindWeaponManager();
	Super::BeginDestroy();
}

void UDurabilityHeaderWidget::Show()
{
	SetVisibility(ESlateVisibility::Visible);
}

void UDurabilityHeaderWidget::Hide()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UDurabilityHeaderWidget::RefreshForActor(AActor *InActor)
{
	UnbindRingManager();
	UnbindWeaponManager();

	if (!InActor)
	{
		Hide();
		return;
	}

	BoundActor = InActor;

	UCharacterDataComponent *CDC = InActor->FindComponentByClass<UCharacterDataComponent>();
	if (!CDC || !CDC->CharacterData)
	{
		Hide();
		return;
	}

	// === Resource detection (class-agnostic) ===
	// Read the active loadout to determine what durability resources this
	// character actually has, regardless of class.

	bool bHasRingResource = false;
	bool bHasWeaponResource = false;

	ULoadoutComponent *LC = InActor->FindComponentByClass<ULoadoutComponent>();
	if (LC)
	{
		const FCombatLoadout Loadout = LC->GetActiveLoadout();

		// Ring resource: any usable ring across the loadout. Resonators
		// hold rings in RingLoadout; Generic/Caster hold a single ring
		// in PrimaryRing when PrimarySlotType == Ring.
		auto RingIsUsable = [](const FRingLoadoutEntry &Entry)
		{
			return Entry.IsValid() &&
				   Entry.RingEntry.Ring &&
				   Entry.RingEntry.Ring->SlottedCrystal &&
				   !Entry.RingEntry.Ring->SlottedCrystal->IsBroken();
		};

		for (const FRingLoadoutEntry &Entry : Loadout.RingLoadout)
		{
			if (RingIsUsable(Entry))
			{
				bHasRingResource = true;
				break;
			}
		}

		if (!bHasRingResource && Loadout.HasPrimaryRing() && RingIsUsable(Loadout.PrimaryRing))
		{
			bHasRingResource = true;
		}

		// Weapon resource: active weapon with a slotted, non-broken crystal.
		// Read from WeaponManager which knows which weapon is currently active.
		if (UGameInstance *GI = GetGameInstance())
		{
			if (UWeaponManager *WM = GI->GetSubsystem<UWeaponManager>())
			{
				if (UWeaponData *ActiveWeapon = WM->GetActiveWeapon(InActor))
				{
					if (ActiveWeapon->SlottedCrystal &&
						!ActiveWeapon->SlottedCrystal->IsBroken())
					{
						bHasWeaponResource = true;
					}
				}
			}
		}
	}

	// === Bind manager delegates for resources we have ===
	if (bHasRingResource)
	{
		if (UGameInstance *GI = GetGameInstance())
		{
			if (URingManager *RingMgr = GI->GetSubsystem<URingManager>())
			{
				BindRingManager(RingMgr);
			}
		}
	}

	if (bHasWeaponResource)
	{
		if (UGameInstance *GI = GetGameInstance())
		{
			if (UWeaponManager *WM = GI->GetSubsystem<UWeaponManager>())
			{
				BindWeaponManager(WM);
			}
		}
	}

	// === Slot assignment per locked rule ===
	// Slot 1: ring if present, else weapon if present, else hidden
	// Slot 2: weapon if BOTH ring AND weapon are present, else hidden
	if (bHasRingResource)
	{
		UpdateSlot1FromRing();
		if (bHasWeaponResource)
		{
			UpdateSlot2FromWeapon();
		}
		else
		{
			HideSlot2();
		}
	}
	else if (bHasWeaponResource)
	{
		UpdateSlot1FromWeapon();
		HideSlot2();
	}
	else
	{
		HideSlot1();
		HideSlot2();
	}

	// === Header visibility ===
	const bool bAnyLineVisible = bHasRingResource || bHasWeaponResource;
	if (bAnyLineVisible)
	{
		Show();
	}
	else
	{
		Hide();
	}
}

void UDurabilityHeaderWidget::UpdateSlot1FromRing()
{
	if (!Slot1DurText)
		return;

	AActor *Actor = BoundActor.Get();
	URingManager *RingMgr = BoundRingManager.Get();
	if (!Actor || !RingMgr)
	{
		Slot1DurText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	URingData *ActiveRing = RingMgr->GetActiveRing(Actor);
	if (!ActiveRing || !ActiveRing->SlottedCrystal)
	{
		Slot1DurText->SetText(FText::FromString(FString::Printf(TEXT("%s:0/0"), HeaderLabels::RingDur)));
		Slot1DurText->SetVisibility(ESlateVisibility::Visible);
		return;
	}

	const int32 Current = ActiveRing->SlottedCrystal->CurrentDurability;
	const int32 Max = ActiveRing->SlottedCrystal->MaxDurability;
	Slot1DurText->SetText(FText::FromString(FString::Printf(TEXT("%s:%d/%d"),
		HeaderLabels::RingDur, Current, Max)));
	Slot1DurText->SetVisibility(ESlateVisibility::Visible);
}

void UDurabilityHeaderWidget::UpdateSlot1FromWeapon()
{
	if (!Slot1DurText)
		return;

	AActor *Actor = BoundActor.Get();
	UWeaponManager *WM = BoundWeaponManager.Get();
	if (!Actor || !WM)
	{
		Slot1DurText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UWeaponData *ActiveWeapon = WM->GetActiveWeapon(Actor);
	if (!ActiveWeapon || !ActiveWeapon->SlottedCrystal)
	{
		Slot1DurText->SetText(FText::FromString(FString::Printf(TEXT("%s:0/0"), HeaderLabels::WeaponDur)));
		Slot1DurText->SetVisibility(ESlateVisibility::Visible);
		return;
	}

	const int32 Current = ActiveWeapon->SlottedCrystal->CurrentDurability;
	const int32 Max = ActiveWeapon->SlottedCrystal->MaxDurability;
	Slot1DurText->SetText(FText::FromString(FString::Printf(TEXT("%s:%d/%d"),
		HeaderLabels::WeaponDur, Current, Max)));
	Slot1DurText->SetVisibility(ESlateVisibility::Visible);
}

void UDurabilityHeaderWidget::UpdateSlot2FromWeapon()
{
	if (!Slot2DurText)
		return;

	AActor *Actor = BoundActor.Get();
	UWeaponManager *WM = BoundWeaponManager.Get();
	if (!Actor || !WM)
	{
		Slot2DurText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UWeaponData *ActiveWeapon = WM->GetActiveWeapon(Actor);
	if (!ActiveWeapon || !ActiveWeapon->SlottedCrystal)
	{
		Slot2DurText->SetText(FText::FromString(FString::Printf(TEXT("%s:0/0"), HeaderLabels::WeaponDur)));
		Slot2DurText->SetVisibility(ESlateVisibility::Visible);
		return;
	}

	const int32 Current = ActiveWeapon->SlottedCrystal->CurrentDurability;
	const int32 Max = ActiveWeapon->SlottedCrystal->MaxDurability;
	Slot2DurText->SetText(FText::FromString(FString::Printf(TEXT("%s:%d/%d"),
		HeaderLabels::WeaponDur, Current, Max)));
	Slot2DurText->SetVisibility(ESlateVisibility::Visible);
}

void UDurabilityHeaderWidget::HideSlot1()
{
	if (Slot1DurText)
	{
		Slot1DurText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UDurabilityHeaderWidget::HideSlot2()
{
	if (Slot2DurText)
	{
		Slot2DurText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UDurabilityHeaderWidget::BindRingManager(URingManager *RingMgr)
{
	if (!RingMgr || bIsBound)
		return;

	BoundRingManager = RingMgr;
	RingMgr->OnRingDurabilityChanged.AddDynamic(this, &UDurabilityHeaderWidget::HandleRingDurabilityChanged);
	RingMgr->OnRingCrystalBroken.AddDynamic(this, &UDurabilityHeaderWidget::HandleRingCrystalBroken);
	bIsBound = true;
}

void UDurabilityHeaderWidget::UnbindRingManager()
{
	if (!bIsBound)
		return;

	if (URingManager *RingMgr = BoundRingManager.Get())
	{
		RingMgr->OnRingDurabilityChanged.RemoveDynamic(this, &UDurabilityHeaderWidget::HandleRingDurabilityChanged);
		RingMgr->OnRingCrystalBroken.RemoveDynamic(this, &UDurabilityHeaderWidget::HandleRingCrystalBroken);
	}

	BoundRingManager.Reset();
	BoundActor.Reset();
	bIsBound = false;
}

void UDurabilityHeaderWidget::BindWeaponManager(UWeaponManager *WeaponMgr)
{
	if (!WeaponMgr)
		return;

	BoundWeaponManager = WeaponMgr;
	WeaponMgr->OnWeaponDurabilityChanged.AddDynamic(this, &UDurabilityHeaderWidget::HandleWeaponDurabilityChanged);
	WeaponMgr->OnWeaponCrystalBroken.AddDynamic(this, &UDurabilityHeaderWidget::HandleWeaponCrystalBroken);
}

void UDurabilityHeaderWidget::UnbindWeaponManager()
{
	if (UWeaponManager *WeaponMgr = BoundWeaponManager.Get())
	{
		WeaponMgr->OnWeaponDurabilityChanged.RemoveDynamic(this, &UDurabilityHeaderWidget::HandleWeaponDurabilityChanged);
		WeaponMgr->OnWeaponCrystalBroken.RemoveDynamic(this, &UDurabilityHeaderWidget::HandleWeaponCrystalBroken);
	}

	BoundWeaponManager.Reset();
}

void UDurabilityHeaderWidget::HandleRingDurabilityChanged(AActor *Actor, URingData *Ring, int32 NewDurability, int32 MaxDurability)
{
	if (Actor != BoundActor.Get())
		return;
	UpdateSlot1FromRing();
}

void UDurabilityHeaderWidget::HandleRingCrystalBroken(AActor *Actor, URingData *Ring, UItemData *Crystal)
{
	if (Actor != BoundActor.Get())
		return;
	// Active ring may have auto-switched — re-read.
	UpdateSlot1FromRing();
}

void UDurabilityHeaderWidget::HandleWeaponDurabilityChanged(AActor *Actor, UWeaponData *Weapon, int32 NewDurability, int32 MaxDurability)
{
	if (Actor != BoundActor.Get())
		return;

	// Determine which slot the weapon line is in. If the character has a ring
	// too, weapon is in slot 2. Otherwise slot 1.
	// Re-detect to be safe (loadout may have changed mid-turn — defensive).
	ULoadoutComponent *LC = Actor->FindComponentByClass<ULoadoutComponent>();
	bool bHasRingResource = false;
	if (LC)
	{
		const FCombatLoadout Loadout = LC->GetActiveLoadout();
		auto RingIsUsable = [](const FRingLoadoutEntry &Entry)
		{
			return Entry.IsValid() &&
				   Entry.RingEntry.Ring &&
				   Entry.RingEntry.Ring->SlottedCrystal &&
				   !Entry.RingEntry.Ring->SlottedCrystal->IsBroken();
		};

		for (const FRingLoadoutEntry &Entry : Loadout.RingLoadout)
		{
			if (RingIsUsable(Entry))
			{
				bHasRingResource = true;
				break;
			}
		}

		if (!bHasRingResource && Loadout.HasPrimaryRing() && RingIsUsable(Loadout.PrimaryRing))
		{
			bHasRingResource = true;
		}
	}

	if (bHasRingResource)
	{
		UpdateSlot2FromWeapon();
	}
	else
	{
		UpdateSlot1FromWeapon();
	}
}

void UDurabilityHeaderWidget::HandleWeaponCrystalBroken(AActor *Actor, UWeaponData *Weapon, UItemData *Crystal)
{
	if (Actor != BoundActor.Get())
		return;
	// Crystal broke — weapon is no longer a resource. Re-detect everything.
	RefreshForActor(Actor);
}
