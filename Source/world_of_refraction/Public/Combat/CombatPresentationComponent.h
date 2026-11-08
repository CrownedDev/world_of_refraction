// CombatPresentationComponent.h
// B-231 part 1 (D-129, Design/CoopStateOwnership.md): the action in flight runs on the server only
// (UActionExecutor; a client's picks and defense presses reach it by Server RPC, B-229). What a client sees of it
// comes through this component on every combatant, by multicast:
// - the montages the server plays (action legs, item use, a defense) play on the clients' copies too, so the
//   swing, the guard and the hit land identically on both screens (the clients' notifies drive nothing: only the
//   server's executor listens to them);
// - each hit the server resolves fires OnHitShown on every machine (damage numbers bind it; on the server too;
//   reliable, so no screen misses one).
// - B-231b: each projectile the server launches from the owner flies as a cosmetic copy on the clients
//   (ASkillProjectile::InitializeCosmetic: same path and VFX, no gameplay).
// - B-231c: each skill VFX the server's runner spawns (cast / ritual effects, telegraph tells) spawns on the clients
//   too (FCosmeticVFX: attached to the same socket of the owner, or at the same spot; the same element tint).
// Debug: GetDebugString (the last montage and hit sent or received), wor.Net.PrintPresentation.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Projectile/SkillProjectile.h"
#include "CombatPresentationComponent.generated.h"

class UAnimMontage;
class UNiagaraSystem;

/** B-231c: one skill VFX the server spawned, for the clients to spawn the same. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FCosmeticVFX
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> System = nullptr;

	/** Attached to the owner's mesh at Socket; else at Location. */
	UPROPERTY()
	bool bAttached = false;

	UPROPERTY()
	FName Socket;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	float Scale = 1.0f;

	UPROPERTY()
	bool bTinted = false;

	UPROPERTY()
	FLinearColor CoreColor = FLinearColor::White;

	UPROPERTY()
	FLinearColor EdgeColor = FLinearColor::White;
};

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

	/** B-231b (server): the owner just launched Projectile; the clients fly a cosmetic copy. No-op unless
	 *  SendsFromHere. */
	void SendProjectile(const ASkillProjectile &Projectile);

	/** B-231c (server): the runner spawned a skill VFX for the owner; the clients spawn the same. No-op unless
	 *  SendsFromHere. */
	void SendVFX(const FCosmeticVFX &VFX);

	/** B-231c: spawns VFX on this machine (attached to the owner, or at its spot); null when it can't. */
	class UNiagaraComponent *SpawnLocally(const FCosmeticVFX &VFX) const;

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

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_LaunchProjectile(const FCosmeticProjectileLaunch &Launch);

	/** Unreliable: a missed sparkle costs nothing (VFX are many per action). */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SpawnVFX(const FCosmeticVFX &VFX);

	/** Plays Montage on the owner's combat anim instance (else the character), as the executor does. */
	void PlayLocally(UAnimMontage *Montage, float PlayRate);

	FString LastMontage;
	int32 LastHitDamage = 0;
	bool bLastHitCritical = false;
	int32 MontagesSent = 0;
	int32 MontagesReceived = 0;
	int32 HitsShown = 0;
	int32 ProjectilesSent = 0;
	int32 ProjectilesFlown = 0;
	int32 VFXSent = 0;
	int32 VFXSpawned = 0;
};
