// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ActionStructs.h"
#include "EPhysicalDamageType.h"
#include "ESpellElement.h"
#include "ItemTier.h"
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

	// ========================================
	// DURABILITY (Phase 4d)
	// ========================================

	/** Apply wear to the slotted crystal of the active weapon after an action.
	 *  Returns the wear amount applied.
	 *
	 *  Generic signature taking EItemTier directly so it works for spells,
	 *  abilities, and attacks (vs RingManager which is spell-only and reads
	 *  USpellData::Tier directly). Caller extracts ActionTier from the weapon
	 *  itself (Phase 4d Path A: action tier inherits from weapon tier).
	 *
	 *  TODO: Reads Weapon->SlottedCrystal (asset-side), inheriting the same
	 *  architectural shortcut as URingManager. Should migrate both to runtime
	 *  inventory entries when inventory persistence is wired. */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager")
	int32 ProcessPostCastWear(AActor *Actor, EItemTier ActionTier, int32 InfusionLevel, bool bIsSpell);

	// ==================== DELEGATES (durability) ====================

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnWeaponDurabilityChanged, AActor *, Actor, UWeaponData *, Weapon, int32, NewDurability, int32, MaxDurability);

	/** Fires every time a weapon crystal's durability changes (per-cast wear).
	 *  Use this for real-time UI updates of the weapon durability bar. */
	UPROPERTY(BlueprintAssignable, Category = "Weapon Manager|Events")
	FOnWeaponDurabilityChanged OnWeaponDurabilityChanged;

private:
	// ========================================
	// HELPERS
	// ========================================

	/** Get LoadoutComponent from actor */
	ULoadoutComponent *GetLoadoutComponent(AActor *Actor) const;
};
