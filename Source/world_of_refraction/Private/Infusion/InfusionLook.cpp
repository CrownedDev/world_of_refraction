// InfusionLook.cpp

#include "Infusion/InfusionLook.h"
#include "Loadout/FLoadoutSlot.h"
#include "NiagaraSystem.h"

TSoftObjectPtr<UNiagaraSystem> FInfusionLookSet::GetVFX(ESpellElement Element) const
{
	const TSoftObjectPtr<UNiagaraSystem> *Row = ElementVFX.Find(Element);
	return Row && !Row->IsNull() ? *Row : DefaultVFX;
}

TArray<FName> InfusionLook::GetDefaultRingFingerSockets()
{
	return {FName(TEXT("index_01_r")), FName(TEXT("middle_01_r")), FName(TEXT("ring_01_r")), FName(TEXT("pinky_01_r"))};
}

EInfusionLookKind InfusionLook::KindForSource(EInfusionSourceOption Source)
{
	switch (Source)
	{
	case EInfusionSourceOption::WeaponCrystal:
		return EInfusionLookKind::Weapon;
	case EInfusionSourceOption::ActiveRing:
	case EInfusionSourceOption::PrimaryRing:
		return EInfusionLookKind::Ring;
	case EInfusionSourceOption::Innate:
	case EInfusionSourceOption::Evolution:
	case EInfusionSourceOption::Raw:
		return EInfusionLookKind::Aura;
	default:
		return EInfusionLookKind::None;
	}
}

int32 InfusionLook::RingFingerIndex(const TArray<FLoadoutSlot> &Slots, const FLoadoutSlot *InUse)
{
	int32 Finger = 0;
	for (const FLoadoutSlot &Slot : Slots)
	{
		if (Slot.IsEmpty() || Slot.Kind() != EGearKind::Adapter)
		{
			continue;
		}
		if (&Slot == InUse)
		{
			return Finger;
		}
		++Finger;
	}
	return INDEX_NONE;
}

FName InfusionLook::FingerSocket(const TArray<FName> &Sockets, int32 FingerIndex)
{
	if (Sockets.IsEmpty() || FingerIndex < 0)
	{
		return NAME_None;
	}
	return Sockets[FMath::Min(FingerIndex, Sockets.Num() - 1)];
}

TArray<EInfusionLookKind> InfusionLook::KindsForStack(const TArray<EInfusionSourceOption> &Stack)
{
	TArray<EInfusionLookKind> Kinds;
	for (const EInfusionSourceOption Source : Stack)
	{
		const EInfusionLookKind Kind = KindForSource(Source);
		if (Kind != EInfusionLookKind::None)
		{
			Kinds.AddUnique(Kind);
		}
	}
	return Kinds;
}
