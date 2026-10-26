// WoRActivatableWidget.h
// Shared base for every CommonUI screen in the game (F2b). Exists for exactly
// one reason: to state the non-modal input contract ONCE, so no screen can
// forget it and silently fall back to engine defaults.
//
// This replaces the UMG-era SetInputMode / SetShowMouseCursor / SetGamePaused
// bracket that UMerchantShopSubsystem still uses. CommonUI has NO pause concept
// at all — "no pause" is not a value you set here, it is what you get by not
// pausing anywhere. Nothing derived from this class may call SetInputMode:
// mixing the two input systems on one controller is the illegal combination.
//
// ⚠️ Screens parent to THIS, not to UCommonActivatableWidget directly. A screen
// that parents to the raw CommonUI class inherits the base implementation of
// GetDesiredInputConfig, which returns an EMPTY TOptional meaning "no opinion" —
// the previous config stays in force and the screen may end up unusable.

#pragma once

#include "CommonActivatableWidget.h"
#include "CoreMinimal.h"
#include "WoRActivatableWidget.generated.h"

UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UWoRActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** Non-modal menu config — see the .cpp for the exact constructor arguments
	 *  and why each one is what it is. Returns a POPULATED TOptional; the base
	 *  class's empty return is precisely what this override exists to avoid. */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
};
