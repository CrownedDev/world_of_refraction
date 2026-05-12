// Copyright Epic Games, Inc. All Rights Reserved.

#include "WeaponManager.h"
#include "WeaponData.h"
#include "LoadoutComponent.h"

void UWeaponManager::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[WeaponManager] Initialized"));
}

void UWeaponManager::Deinitialize()
{
	Super::Deinitialize();
}

// ========================================
// QUERIES
// ========================================

UWeaponData *UWeaponManager::GetActiveWeapon(AActor *Actor) const
{
	if (ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor))
	{
		return LoadoutComp->GetActiveWeapon();
	}
	return nullptr;
}

UWeaponAttackData *UWeaponManager::GetActiveAttack(AActor *Actor) const
{
	if (ULoadoutComponent *Loadout = GetLoadoutComponent(Actor))
	{
		return Loadout->GetCurrentAttack();
	}
	return nullptr;
}

// ========================================
// HELPERS
// ========================================

ULoadoutComponent *UWeaponManager::GetLoadoutComponent(AActor *Actor) const
{
	if (!Actor)
		return nullptr;
	return Actor->FindComponentByClass<ULoadoutComponent>();
}
