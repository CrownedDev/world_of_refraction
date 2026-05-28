// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Combat/Actions/ActionStructs.h"
#include "Combat/Damage/EPhysicalDamageType.h"
#include "Skills/Definitions/ESpellElement.h"
#include "WeaponManager.generated.h"

class UWeaponData;
class UWeaponAttackData;
class ULoadoutComponent;

/**
 * UWeaponManager
 *
 * Thin façade over LoadoutComponent for weapon-side queries, plus the
 * weapon-crystal durability wear path.
 *
 * Usage:
 *   UWeaponManager* WeaponMgr = GetGameInstance()->GetSubsystem<UWeaponManager>();
 *   UWeaponData* Weapon = WeaponMgr->GetActiveWeapon(Actor);
 *   // Attack execution flows through UActionExecutor / ApplyHit, not this subsystem.
 */
UCLASS()
class WORLD_OF_REFRACTION_API UWeaponManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	// ========================================
	// QUERIES
	// ========================================

	/**
	 * Get currently active weapon (or nullptr if unarmed)
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager")
	UWeaponData *GetActiveWeapon(AActor *Actor) const;

	/** Get the active attack for the actor. Thin delegate to
	 *  ULoadoutComponent::GetCurrentAttack — the LoadoutComponent
	 *  is the source of truth for "which weapon is wielded right now."
	 *  Kept on UWeaponManager for caller convenience until UWeaponManager
	 *  is itself absorbed into LoadoutComponent (potential follow-up). */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager|Query")
	UWeaponAttackData *GetActiveAttack(AActor *Actor) const;

private:
	// ========================================
	// HELPERS
	// ========================================

	/** Get LoadoutComponent from actor */
	ULoadoutComponent *GetLoadoutComponent(AActor *Actor) const;
};
