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
	constexpr int32 LATEST = 3; // 2: B-208's run and carried state; 3: B-124's gear detail (older load with defaults)
	/** Saves below this are wiped (D-060). Upgrade steps in HandlePostLoad after the first public build. */
	constexpr int32 MIN_SUPPORTED = 1;
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
