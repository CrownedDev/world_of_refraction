// WoRSaveTypes.h
// B-090: the explicit save structs (D-059: copied whole, no SaveGame tags). Asset references are
// saved as an FPrimaryAssetId plus the asset's path, never a pointer: the id survives an asset
// move, the path resolves a type the Asset Manager doesn't scan.

#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/SoftObjectPath.h"
#include "Combat/Grid/ECombatRow.h"
#include "Inventory/ItemTier.h"
#include "Inventory/ItemQuality.h"
#include "WoRSaveTypes.generated.h"

/** A data asset by id and path. Resolve loads it (synchronously) or returns null. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedAssetRef
{
	GENERATED_BODY()

	UPROPERTY()
	FPrimaryAssetId Id;

	UPROPERTY()
	FSoftObjectPath Path;

	static FSavedAssetRef Make(const UObject *Asset);
	bool IsSet() const { return Id.IsValid() || Path.IsValid(); }
	UObject *Resolve() const;

	template <typename T>
	T *ResolveAs() const { return Cast<T>(Resolve()); }
};

USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedPartyMember
{
	GENERATED_BODY()

	UPROPERTY()
	FSavedAssetRef CharacterData;

	UPROPERTY()
	FSoftClassPath PawnClass;

	UPROPERTY()
	ECombatRow PreferredRow = ECombatRow::Middle;

	UPROPERTY()
	int32 PreferredColumn = 0;

	UPROPERTY()
	bool bPlayerControlled = true;

	UPROPERTY()
	int32 CurrentHP = 0;

	UPROPERTY()
	int32 CurrentEP = 0;
};

USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedParty
{
	GENERATED_BODY()

	UPROPERTY()
	bool bHasParty = false;

	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	bool bLetAIFinish = false;

	UPROPERTY()
	TArray<FSavedPartyMember> Members;
};

/** One banked balance: an ECurrencyType, its essence sub-key, the amount. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedBalance
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 Currency = 0;

	UPROPERTY()
	uint8 SubKey = 0;

	UPROPERTY()
	int32 Amount = 0;
};

/** The player's banked currencies. Gold is never banked (CurrencySystem.md), so it is not here. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedWallet
{
	GENERATED_BODY()

	UPROPERTY()
	bool bHasWallet = false;

	UPROPERTY()
	TArray<FSavedBalance> Balances;
};

/** An owned gear or skill instance. InstanceID is the weapon/ring PersistentID or the
 *  spell/ability/evolution InstanceID. TODO(B-124): the rest of the entry (crystal, spells, rolls). */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedOwnedItem
{
	GENERATED_BODY()

	UPROPERTY()
	FSavedAssetRef Def;

	UPROPERTY()
	FGuid InstanceID;

	UPROPERTY()
	EItemTier Tier = EItemTier::F_Tier;

	UPROPERTY()
	EItemQuality Quality = EItemQuality::F_Quality;

	UPROPERTY()
	int32 Durability = 0;
};

/** A counted crystal stack. Gem or stone follows from the type (UPoolSubsystem::AddCrystalToPool). */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedCrystalStack
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 Type = 0;

	UPROPERTY()
	EItemTier Tier = EItemTier::F_Tier;

	UPROPERTY()
	int32 Count = 0;
};

/** UPoolSubsystem's owned items. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedPool
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FSavedOwnedItem> Weapons;

	UPROPERTY()
	TArray<FSavedOwnedItem> Rings;

	UPROPERTY()
	TArray<FSavedOwnedItem> Evolutions;

	UPROPERTY()
	TArray<FSavedOwnedItem> Spells;

	UPROPERTY()
	TArray<FSavedOwnedItem> Abilities;

	UPROPERTY()
	TArray<FSavedCrystalStack> Crystals;

	int32 Num() const
	{
		return Weapons.Num() + Rings.Num() + Evolutions.Num() + Spells.Num() + Abilities.Num() + Crystals.Num();
	}
};

/** Everything one save slot holds. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FWoRSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	FSavedParty Party;

	UPROPERTY()
	FSavedWallet Wallet;

	UPROPERTY()
	FSavedPool Pool;
};
