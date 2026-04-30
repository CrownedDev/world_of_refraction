// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DefensePromptWidget.generated.h"

class AActor;
class UDefenseSystem;
struct FDefenseResult;

/**
 * UDefensePromptWidget
 *
 * Native C++ base for WBP_DefensePrompt. Shows defense window UI when player
 * needs to block/parry/dodge.
 *
 * Default state: hidden. Visible only during defense windows where the defender
 * is the local player's controlled actor (filter applied in HandleDefenseWindowOpened).
 *
 * Lifecycle: Owned by UCombatHUDRoot. Bound at combat start, unbound at combat end.
 */
UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UDefensePromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bind to UDefenseSystem delegates. */
	UFUNCTION(BlueprintCallable, Category = "Defense Prompt")
	void InitialiseForCombat();

	/** Unbind delegates. */
	UFUNCTION(BlueprintCallable, Category = "Defense Prompt")
	void TeardownPrompt();

protected:
	virtual void NativeDestruct() override;
	virtual void BeginDestroy() override;

	UFUNCTION() void HandleDefenseWindowOpened(AActor* Defender, float AttackSize, float WindowDuration);
	UFUNCTION() void HandleDefenseWindowClosed(AActor* Defender, const FDefenseResult& Result);

	/** BP fills in countdown bar + button prompts on show. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Defense Prompt")
	void ShowPrompt(AActor* Defender, float WindowDuration, float AttackSize);

	UFUNCTION(BlueprintImplementableEvent, Category = "Defense Prompt")
	void HidePrompt(const FDefenseResult& Result);

private:
	UPROPERTY()
	TWeakObjectPtr<UDefenseSystem> CachedDefenseSystem;

	bool bBound = false;
};
