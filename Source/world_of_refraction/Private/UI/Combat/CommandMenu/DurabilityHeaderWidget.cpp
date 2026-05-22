// DurabilityHeaderWidget.cpp

#include "UI/Combat/CommandMenu/DurabilityHeaderWidget.h"
#include "RingManager.h"
#include "RingData.h"
#include "EvolutionItemData.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "CrystalManager.h"
#include "WeaponManager.h"
#include "WeaponData.h"
#include "LoadoutComponent.h"
#include "FCombatLoadout.h"
#include "FRingLoadoutEntry.h"
#include "FRuntimeAttachedItem.h"

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
	UnbindCrystalManager();
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
	UnbindCrystalManager();

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
				   !Entry.RingEntry.AttachedItem.IsEmpty() &&
				   !Entry.RingEntry.AttachedItem.IsBroken();
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

		// Weapon resource: active weapon with a slotted, non-broken attachment.
		// Resolve per-instance attachment via LoadoutComponent — the asset-side
		// AttachedItem carries no runtime durability, so broken-state must be
		// read from the runtime attachment (writes go to runtime post-Phase-B).
		if (UGameInstance *GI = GetGameInstance())
		{
			if (UWeaponManager *WM = GI->GetSubsystem<UWeaponManager>())
			{
				if (UWeaponData *ActiveWeapon = WM->GetActiveWeapon(InActor))
				{
					const FRuntimeAttachedItem WeaponAttachment = LC->GetCrystalEntryByHolder(ActiveWeapon);
					if (!WeaponAttachment.IsEmpty() && !WeaponAttachment.IsBroken())
					{
						bHasWeaponResource = true;
					}
				}
			}
		}
	}

	// === Bind unified crystal manager if any durability resource exists ===
	if (bHasRingResource || bHasWeaponResource)
	{
		if (UGameInstance *GI = GetGameInstance())
		{
			if (UCrystalManager *CrystalMgr = GI->GetSubsystem<UCrystalManager>())
			{
				BindCrystalManager(CrystalMgr);
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
	URingManager *RingMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<URingManager>() : nullptr;
	if (!Actor || !RingMgr)
	{
		Slot1DurText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	URingData *ActiveRing = RingMgr->GetActiveRing(Actor);
	ULoadoutComponent *LC = Actor->FindComponentByClass<ULoadoutComponent>();
	const FRuntimeAttachedItem Attachment = (ActiveRing && LC) ? LC->GetCrystalEntryByHolder(ActiveRing) : FRuntimeAttachedItem();

	if (Attachment.IsEmpty())
	{
		Slot1DurText->SetText(FText::FromString(FString::Printf(TEXT("%s:0/0"), HeaderLabels::RingDur)));
		Slot1DurText->SetVisibility(ESlateVisibility::Visible);
		return;
	}

	const int32 Current = Attachment.GetCurrentDurability();
	const int32 Max = Attachment.GetMaxDurability();
	Slot1DurText->SetText(FText::FromString(FString::Printf(TEXT("%s:%d/%d"),
		HeaderLabels::RingDur, Current, Max)));
	Slot1DurText->SetVisibility(ESlateVisibility::Visible);
}

void UDurabilityHeaderWidget::UpdateSlot1FromWeapon()
{
	if (!Slot1DurText)
		return;

	AActor *Actor = BoundActor.Get();
	UWeaponManager *WM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UWeaponManager>() : nullptr;
	if (!Actor || !WM)
	{
		Slot1DurText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UWeaponData *ActiveWeapon = WM->GetActiveWeapon(Actor);
	ULoadoutComponent *LC = Actor->FindComponentByClass<ULoadoutComponent>();
	const FRuntimeAttachedItem Attachment = (ActiveWeapon && LC) ? LC->GetCrystalEntryByHolder(ActiveWeapon) : FRuntimeAttachedItem();

	if (Attachment.IsEmpty())
	{
		Slot1DurText->SetText(FText::FromString(FString::Printf(TEXT("%s:0/0"), HeaderLabels::WeaponDur)));
		Slot1DurText->SetVisibility(ESlateVisibility::Visible);
		return;
	}

	const int32 Current = Attachment.GetCurrentDurability();
	const int32 Max = Attachment.GetMaxDurability();
	Slot1DurText->SetText(FText::FromString(FString::Printf(TEXT("%s:%d/%d"),
		HeaderLabels::WeaponDur, Current, Max)));
	Slot1DurText->SetVisibility(ESlateVisibility::Visible);
}

void UDurabilityHeaderWidget::UpdateSlot2FromWeapon()
{
	if (!Slot2DurText)
		return;

	AActor *Actor = BoundActor.Get();
	UWeaponManager *WM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UWeaponManager>() : nullptr;
	if (!Actor || !WM)
	{
		Slot2DurText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UWeaponData *ActiveWeapon = WM->GetActiveWeapon(Actor);
	ULoadoutComponent *LC = Actor->FindComponentByClass<ULoadoutComponent>();
	const FRuntimeAttachedItem Attachment = (ActiveWeapon && LC) ? LC->GetCrystalEntryByHolder(ActiveWeapon) : FRuntimeAttachedItem();

	if (Attachment.IsEmpty())
	{
		Slot2DurText->SetText(FText::FromString(FString::Printf(TEXT("%s:0/0"), HeaderLabels::WeaponDur)));
		Slot2DurText->SetVisibility(ESlateVisibility::Visible);
		return;
	}

	const int32 Current = Attachment.GetCurrentDurability();
	const int32 Max = Attachment.GetMaxDurability();
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

void UDurabilityHeaderWidget::BindCrystalManager(UCrystalManager *CrystalMgr)
{
	if (!CrystalMgr)
		return;

	BoundCrystalManager = CrystalMgr;
	CrystalMgr->OnCrystalDurabilityChanged.AddDynamic(this, &UDurabilityHeaderWidget::HandleCrystalDurabilityChanged);
	CrystalMgr->OnCrystalBroken.AddDynamic(this, &UDurabilityHeaderWidget::HandleCrystalBroken);
}

void UDurabilityHeaderWidget::UnbindCrystalManager()
{
	if (UCrystalManager *CrystalMgr = BoundCrystalManager.Get())
	{
		CrystalMgr->OnCrystalDurabilityChanged.RemoveDynamic(this, &UDurabilityHeaderWidget::HandleCrystalDurabilityChanged);
		CrystalMgr->OnCrystalBroken.RemoveDynamic(this, &UDurabilityHeaderWidget::HandleCrystalBroken);
	}

	BoundCrystalManager.Reset();
	BoundActor.Reset();
}

void UDurabilityHeaderWidget::HandleCrystalDurabilityChanged(AActor *Actor, UObject *Holder, int32 NewDurability, int32 MaxDurability)
{
	if (Actor != BoundActor.Get())
		return;

	// Ring crystal: always updates slot 1 (ring is the primary durability resource).
	if (Cast<URingData>(Holder))
	{
		UpdateSlot1FromRing();
		return;
	}

	// Weapon crystal: slot 2 if the character has a ring resource, slot 1 otherwise.
	if (!Cast<UWeaponData>(Holder))
	{
		return;
	}

	// Re-detect ring presence (loadout may have changed mid-turn — defensive).
	ULoadoutComponent *LC = Actor->FindComponentByClass<ULoadoutComponent>();
	bool bHasRingResource = false;
	if (LC)
	{
		const FCombatLoadout Loadout = LC->GetActiveLoadout();
		auto RingIsUsable = [](const FRingLoadoutEntry &Entry)
		{
			return Entry.IsValid() &&
				   !Entry.RingEntry.AttachedItem.IsEmpty() &&
				   !Entry.RingEntry.AttachedItem.IsBroken();
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

void UDurabilityHeaderWidget::HandleCrystalBroken(AActor *Actor, UObject *Holder, FBrokenCrystalPayload Payload)
{
	if (Actor != BoundActor.Get())
		return;

	// A break may have triggered a ring auto-switch, which can change which
	// slot displays which resource. Cheapest correct response: re-detect
	// resources and re-bind from scratch.
	RefreshForActor(Actor);
}

