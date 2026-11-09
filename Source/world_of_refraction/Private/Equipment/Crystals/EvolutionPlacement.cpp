// EvolutionPlacement.cpp

#include "Equipment/Crystals/EvolutionPlacement.h"
#include "Equipment/FRuntimeAttachedItem.h"

bool WoREvolutionPlacement::GivesPassives(EEvolutionPlace Place, int32 ChargeLevel)
{
	return Place == EEvolutionPlace::Character || ChargeLevel >= GEAR_PASSIVES_CHARGE_LEVEL;
}

bool WoREvolutionPlacement::GivesStats(EEvolutionPlace Place)
{
	return Place == EEvolutionPlace::Character;
}

bool WoREvolutionPlacement::AppendFrom(const FRuntimeAttachedItem &Item, EEvolutionPlace Place, TArray<FAttachedEvolution> &Out)
{
	if (!Item.IsEvolution() || !Item.Evolution.Item)
	{
		return false;
	}
	FAttachedEvolution &Added = Out.AddDefaulted_GetRef();
	Added.Attachment = Item.Evolution;
	Added.Place = Place;
	return true;
}

bool WoREvolutionPlacement::AppendCharacterSlot(const FEvolutionAttachment &Attachment, TArray<FAttachedEvolution> &Out)
{
	if (!Attachment.Item)
	{
		return false;
	}
	FAttachedEvolution &Added = Out.AddDefaulted_GetRef();
	Added.Attachment = Attachment;
	Added.Place = EEvolutionPlace::Character;
	return true;
}

TArray<FAttachedEvolution> WoREvolutionPlacement::FilterPassives(const TArray<FAttachedEvolution> &Attached, int32 ChargeLevel)
{
	return Attached.FilterByPredicate([ChargeLevel](const FAttachedEvolution &Evolution) { return GivesPassives(Evolution.Place, ChargeLevel); });
}

bool WoREvolutionPlacement::PowersAction(EEvolutionPlace Place, EActionType ActionType, EInfusionSourceOption Source)
{
	switch (Place)
	{
	case EEvolutionPlace::Weapon:
		return ActionType == EActionType::Ability || Source == EInfusionSourceOption::WeaponCrystal;
	case EEvolutionPlace::Adapter:
		return Source == EInfusionSourceOption::ActiveRing || Source == EInfusionSourceOption::PrimaryRing;
	case EEvolutionPlace::Apparel:
		return true;
	case EEvolutionPlace::Character:
	default:
		return false;
	}
}

TArray<FAttachedEvolution> WoREvolutionPlacement::FilterForAction(const TArray<FAttachedEvolution> &Attached, EActionType ActionType,
																  EInfusionSourceOption Source, int32 ChargeLevel)
{
	if (ChargeLevel < GEAR_PASSIVES_CHARGE_LEVEL)
	{
		return {};
	}
	return Attached.FilterByPredicate([ActionType, Source](const FAttachedEvolution &Evolution)
	{
		return Evolution.Attachment.Item && !Evolution.Attachment.IsBroken() && PowersAction(Evolution.Place, ActionType, Source);
	});
}

const TCHAR *WoREvolutionPlacement::GetPlaceName(EEvolutionPlace Place)
{
	switch (Place)
	{
	case EEvolutionPlace::Weapon:  return TEXT("weapon");
	case EEvolutionPlace::Adapter: return TEXT("adapter");
	case EEvolutionPlace::Apparel: return TEXT("apparel");
	case EEvolutionPlace::Character:
	default:                       return TEXT("character");
	}
}
