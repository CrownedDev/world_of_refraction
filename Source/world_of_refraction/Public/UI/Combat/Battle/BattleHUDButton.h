#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "BattleHUDButton.generated.h"

/**
 * A battle HUD button that never takes keyboard focus: a focused button would take A / Enter / Space as its own
 * click, so the menu keys would stop reaching Enhanced Input after one mouse click (B-391 review).
 * UButton::InitIsFocusable is protected, so it is set here, before the Slate button is built.
 */
UCLASS()
class WORLD_OF_REFRACTION_API UBattleHUDButton : public UButton
{
	GENERATED_BODY()

public:
	UBattleHUDButton(const FObjectInitializer &ObjectInitializer)
		: Super(ObjectInitializer)
	{
		InitIsFocusable(false);
	}
};
