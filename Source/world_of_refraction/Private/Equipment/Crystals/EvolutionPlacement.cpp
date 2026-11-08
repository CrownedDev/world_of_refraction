// EvolutionPlacement.cpp

#include "Equipment/Crystals/EvolutionPlacement.h"
#include "Equipment/FRuntimeAttachedItem.h"

bool WoREvolutionPlacement::GivesPassives(EEvolutionPlace Place, int32 ChargeLevel)
{
	return Place == EEvolutionPlace::Character || ChargeLevel >= GEAR_PASSIVES_CHARGE_LEVEL;
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

const TCHAR *WoREvolutionPlacement::GetPlaceName(EEvolutionPlace Place)
{
	switch (Place)
	{
	case EEvolutionPlace::Weapon:  return TEXT("weapon");
	case EEvolutionPlace::Adapter: return TEXT("adapter");
	case EEvolutionPlace::Character:
	default:                       return TEXT("character");
	}
}
