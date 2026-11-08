// StatusBarStateComponent.h
// B-230 part 1 (Design/CoopStateOwnership.md: buildup is actor state, replicated for the HUD): a combatant's status
// bar as the server's UStatusBuildupManager last reported it, replicated so a client's character panel shows it.
// On the server it follows the manager's OnStatusBuildupChanged for its owner; on a client it broadcasts
// OnBarChanged when the replicated bar arrives. The manager stays the source of truth (server-only).
// Debug: GetDebugString.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "StatusBarStateComponent.generated.h"

/** One status bar, as replicated. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FReplicatedStatusBar
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Status Bar")
	float Current = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Status Bar")
	float Max = 0.0f;

	/** The most recent hit's element (the bar's tint). */
	UPROPERTY(BlueprintReadOnly, Category = "Status Bar")
	ESpellElement PendingElement = ESpellElement::None;

	/** The last attacker (Broken Darkness darkens the tint); may be null. */
	UPROPERTY(BlueprintReadOnly, Category = "Status Bar")
	TObjectPtr<AActor> Source = nullptr;
};

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UStatusBarStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStatusBarStateComponent();

	/** Server: the owner's bar changed (the manager's report); replicates it. Specs call it directly. */
	void SetBar(float Current, float Max, ESpellElement PendingElement, AActor *Source);

	const FReplicatedStatusBar &GetBar() const { return Bar; }

	/** Fired on a client when the replicated bar arrives (same shape as UStatusBuildupManager::OnStatusBuildupChanged,
	 *  Target = the owner). */
	UPROPERTY(BlueprintAssignable, Category = "Status Bar")
	FOnStatusBuildupChanged OnBarChanged;

	FString GetDebugString() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleBuildupChanged(AActor *Target, float Current, float Max, ESpellElement PendingElement, AActor *Source);

	UFUNCTION()
	void OnRep_Bar();

	UPROPERTY(ReplicatedUsing = OnRep_Bar)
	FReplicatedStatusBar Bar;

	TWeakObjectPtr<UStatusBuildupManager> BoundManager;
};
