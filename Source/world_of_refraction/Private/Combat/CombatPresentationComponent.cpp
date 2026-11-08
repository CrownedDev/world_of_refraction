// CombatPresentationComponent.cpp

#include "Combat/CombatPresentationComponent.h"
#include "Animation/AnimMontage.h"
#include "Combat/CombatAnimInstance.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Character/CombatCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

bool CombatPresentation::SendsFromHere(ENetMode NetMode)
{
	return NetMode == NM_ListenServer || NetMode == NM_DedicatedServer;
}

bool CombatPresentation::PlaysReceivedMontage(bool bHasAuthority)
{
	return !bHasAuthority;
}

UCombatPresentationComponent::UCombatPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true); // a component's multicast needs the component replicated
}

UCombatPresentationComponent *UCombatPresentationComponent::Find(const AActor *Actor)
{
	// Review fix: every hit and montage asks, so a combat character answers from its cached default subobject.
	if (const ACombatCharacter *Character = Cast<ACombatCharacter>(Actor))
	{
		return Character->CombatPresentation;
	}
	return Actor ? Actor->FindComponentByClass<UCombatPresentationComponent>() : nullptr;
}

void UCombatPresentationComponent::SendMontage(UAnimMontage *Montage, float PlayRate)
{
	const UWorld *World = GetWorld();
	if (!Montage || !World || !GetOwner() || !GetOwner()->HasAuthority() || !CombatPresentation::SendsFromHere(World->GetNetMode()))
	{
		return;
	}
	LastMontage = Montage->GetName();
	++MontagesSent;
	Multicast_PlayMontage(Montage, PlayRate);
}

void UCombatPresentationComponent::SendHit(int32 Damage, bool bCritical)
{
	const UWorld *World = GetWorld();
	if (!World || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	if (CombatPresentation::SendsFromHere(World->GetNetMode()))
	{
		Multicast_ShowHit(Damage, bCritical); // runs here too (a multicast runs on the server and every client)
		return;
	}
	Multicast_ShowHit_Implementation(Damage, bCritical); // standalone: just this machine
}

void UCombatPresentationComponent::SendProjectile(const ASkillProjectile &Projectile)
{
	const UWorld *World = GetWorld();
	if (!World || !GetOwner() || !GetOwner()->HasAuthority() || !CombatPresentation::SendsFromHere(World->GetNetMode()))
	{
		return;
	}
	++ProjectilesSent;
	Multicast_LaunchProjectile(Projectile.MakeCosmeticLaunch());
}

void UCombatPresentationComponent::Multicast_LaunchProjectile_Implementation(const FCosmeticProjectileLaunch &Launch)
{
	UWorld *World = GetWorld();
	if (!World || !GetOwner() || GetOwner()->HasAuthority() || !Launch.ProjectileClass)
	{
		return; // the server flies the real one
	}
	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASkillProjectile *Copy = World->SpawnActor<ASkillProjectile>(Launch.ProjectileClass, GetOwner()->GetActorLocation(), FRotator::ZeroRotator, Params);
	if (!Copy)
	{
		return;
	}
	Copy->InitializeCosmetic(GetOwner(), Launch);
	Copy->Launch();
	++ProjectilesFlown;
}

void UCombatPresentationComponent::SendVFX(const FCosmeticVFX &VFX)
{
	const UWorld *World = GetWorld();
	if (!VFX.System || !World || !GetOwner() || !GetOwner()->HasAuthority() || !CombatPresentation::SendsFromHere(World->GetNetMode()))
	{
		return;
	}
	++VFXSent;
	Multicast_SpawnVFX(VFX);
}

void UCombatPresentationComponent::Multicast_SpawnVFX_Implementation(const FCosmeticVFX &VFX)
{
	if (!GetOwner() || GetOwner()->HasAuthority())
	{
		return; // the server's runner already spawned it
	}
	if (SpawnLocally(VFX))
	{
		++VFXSpawned;
	}
}

UNiagaraComponent *UCombatPresentationComponent::SpawnLocally(const FCosmeticVFX &VFX) const
{
	UWorld *World = GetWorld();
	if (!VFX.System || !World)
	{
		return nullptr;
	}
	UNiagaraComponent *Spawned = nullptr;
	const ACharacter *Character = Cast<ACharacter>(GetOwner());
	if (VFX.bAttached && Character && Character->GetMesh() && Character->GetMesh()->DoesSocketExist(VFX.Socket))
	{
		Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(VFX.System, Character->GetMesh(), VFX.Socket, FVector::ZeroVector,
															   FRotator::ZeroRotator, FVector(VFX.Scale), EAttachLocation::SnapToTarget, true,
															   ENCPoolMethod::None);
	}
	if (!Spawned)
	{
		Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, VFX.System, VFX.Location, FRotator::ZeroRotator, FVector(VFX.Scale), true, true);
	}
	if (Spawned && VFX.bTinted)
	{
		Spawned->SetColorParameter(FName("CoreColor"), VFX.CoreColor);
		Spawned->SetColorParameter(FName("EdgeColor"), VFX.EdgeColor);
	}
	return Spawned;
}

void UCombatPresentationComponent::Multicast_PlayMontage_Implementation(UAnimMontage *Montage, float PlayRate)
{
	if (!CombatPresentation::PlaysReceivedMontage(GetOwner() && GetOwner()->HasAuthority()))
	{
		return; // the server already played it
	}
	LastMontage = GetNameSafe(Montage);
	++MontagesReceived;
	PlayLocally(Montage, PlayRate);
}

void UCombatPresentationComponent::Multicast_ShowHit_Implementation(int32 Damage, bool bCritical)
{
	LastHitDamage = Damage;
	bLastHitCritical = bCritical;
	++HitsShown;
	OnHitShown.Broadcast(GetOwner(), Damage, bCritical);
}

void UCombatPresentationComponent::PlayLocally(UAnimMontage *Montage, float PlayRate)
{
	ACharacter *Character = Cast<ACharacter>(GetOwner());
	if (!Montage || !Character || !Character->GetMesh())
	{
		return;
	}
	if (UCombatAnimInstance *CombatAnim = Cast<UCombatAnimInstance>(Character->GetMesh()->GetAnimInstance()))
	{
		CombatAnim->PlayActionMontage(Montage, PlayRate);
		return;
	}
	Character->PlayAnimMontage(Montage, PlayRate);
}

FString UCombatPresentationComponent::GetDebugString() const
{
	return FString::Printf(TEXT("%s: last montage %s (%d sent, %d received), %d hit(s) shown, last %d%s; projectiles %d sent, %d flown; VFX %d sent, %d spawned"),
						   *GetNameSafe(GetOwner()), LastMontage.IsEmpty() ? TEXT("none") : *LastMontage, MontagesSent, MontagesReceived,
						   HitsShown, LastHitDamage, bLastHitCritical ? TEXT(" (crit)") : TEXT(""), ProjectilesSent, ProjectilesFlown,
						   VFXSent, VFXSpawned);
}

static FAutoConsoleCommandWithWorld GNetPrintPresentationCommand(
	TEXT("wor.Net.PrintPresentation"),
	TEXT("B-231: every combatant's presentation traffic (montages sent by the server / received by this client, hits shown)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!World)
		{
			return;
		}
		int32 Count = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (const UCombatPresentationComponent *Presentation = UCombatPresentationComponent::Find(*It))
			{
				UE_LOG(LogWoR, Display, TEXT("[Presentation] %s"), *Presentation->GetDebugString());
				++Count;
			}
		}
		UE_LOG(LogWoR, Display, TEXT("[Presentation] %d combatant(s), net mode %d"), Count, static_cast<int32>(World->GetNetMode()));
	}));
