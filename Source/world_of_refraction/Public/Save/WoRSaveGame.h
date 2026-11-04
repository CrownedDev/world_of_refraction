// WoRSaveGame.h
// B-090: the one save slot per local player. Version-stamped through ULocalPlayerSaveGame;
// until the first public build a save below MIN_SUPPORTED_VERSION is wiped with a warning (D-060).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Save/WoRSaveTypes.h"
#include "WoRSaveGame.generated.h"

namespace WoRSaveVersion
{
	/** Bump when a save struct changes shape. */
	// 12: B-351's two sockets per weapon and adapter (D-141) and one skill-slot list (D-145); older saves wiped (D-138).
	// 11 is taken by B-348 (one save version) and B-333 (seven balances), both unmerged: merge B-348, then B-333 (it
	// moves to 12 on merge), then this branch moves to the next free number with MIN_SUPPORTED = LATEST (D-138).
	constexpr int32 LATEST = 12; // 2: B-208's run and carried state; 3: B-124's gear detail; 4: B-279's form meters (older load with defaults); 5: B-308's one gear list (v3 / v4 lists still read); 6: B-311's custom loadouts; 7: B-321's gear owners; 8: B-279b's worn skin; 9: B-322's pouch per character (the party pouch of older saves still read); 10: B-279c's current outfit (older load with none); 12: B-351's two sockets
	/** Saves below this are wiped with a warning (D-060). D-138: equal to LATEST while in dev. */
	constexpr int32 MIN_SUPPORTED = LATEST;
}

UCLASS()
class WORLD_OF_REFRACTION_API UWoRSaveGame : public ULocalPlayerSaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FWoRSaveData Data;

	virtual int32 GetLatestDataVersion() const override { return WoRSaveVersion::LATEST; }
	virtual void HandlePostLoad() override;
	virtual void ResetToDefault() override;

	/** True when the last load threw an out-of-date save away. */
	bool WasWiped() const { return bWiped; }

private:
	UPROPERTY(Transient)
	bool bWiped = false;
};
