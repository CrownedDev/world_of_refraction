// CombatPresentationComponent.h
// B-231 part 1 (D-129, Design/CoopStateOwnership.md): the action in flight runs on the server only
// (UActionExecutor; a client's picks and defense presses reach it by Server RPC, B-229). What a client sees of it
// comes through this component on every combatant, by multicast:
// - the montages the server plays (action legs, item use, a defense) play on the clients' copies too, so the
//   swing, the guard and the hit land identically on both screens (the clients' notifies drive nothing: only the
//   server's executor listens to them);
// - each hit the server resolves fires OnHitShown on every machine (damage numbers bind it; on the server too;
//   reliable, so no screen misses one).
// Projectiles, spell VFX and telegraphs on clients are B-231b.
// Debug: GetDebugString (the last montage and hit sent or received), wor.Net.PrintPresentation.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatPresentationComponent.generated.h"

class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCombatHitShown, AActor *, Target, int32, Damage, bool, bCritical);

namespace CombatPresentation
{
	/** Whether this machine sends the multicast: the server of a networked game (a standalone game has no one to
	 *  send to, a client never sends). */
	WORLD_OF_REFRACTION_API bool SendsFromHere(ENetMode NetMode);

	/** Whether a received montage plays here: not where it was already played (the server, which played it locally). */
	WORLD_OF_REFRACTION_API bool PlaysReceivedMontage(bool bHasAuthority);
}

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UCombatPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatPresentationComponent();

	/** Server: the owner just played Montage locally; the clients play it too. No-op unless SendsFromHere. */
	void SendMontage(UAnimMontage *Montage, float PlayRate);

	/** Server: a hit on the owner resolved. Fires OnHitShown here and, in a networked game, on every client. */
	void SendHit(int32 Damage, bool bCritical);

	/** Fired on every machine for each hit the server resolves on the owner (damage numbers, hit flashes). */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Presentation")
	FOnCombatHitShown OnHitShown;

	/** The owner's component, or null: a combat character's cached one, else a component search. */
	static UCombatPresentationComponent *Find(const AActor *Actor);

	FString GetDebugString() const;

private:
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayMontage(UAnimMontage *Montage, float PlayRate);

	/** Reliable (review): a dropped hit would leave a damage number missing on one screen; it is low volume. */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ShowHit(int32 Damage, bool bCritical);

	/** Plays Montage on the owner's combat anim instance (else the character), as the executor does. */
	void PlayLocally(UAnimMontage *Montage, float PlayRate);

	FString LastMontage;
	int32 LastHitDamage = 0;
	bool bLastHitCritical = false;
	int32 MontagesSent = 0;
	int32 MontagesReceived = 0;
	int32 HitsShown = 0;
};
