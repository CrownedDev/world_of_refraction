// ProjectileEventRecorder.h
// B-350 (D-140): test-only listener for ASkillProjectile's OnSkillImpact / OnSkillDodged. A dynamic delegate needs a
// UFUNCTION target, so SkillProjectileSpec binds this and reads back what it saw (replaces the manual
// SkillProjectileTestActor's log-only handlers).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Combat/Projectile/SkillProjectile.h"
#include "ProjectileEventRecorder.generated.h"

UCLASS(Transient)
class UProjectileEventRecorder : public UObject
{
	GENERATED_BODY()

public:
	int32 Impacts = 0;
	int32 Dodges = 0;
	int32 LastDamage = 0;
	AActor *LastTarget = nullptr;

	void Listen(ASkillProjectile *Projectile)
	{
		Projectile->OnSkillImpact.AddDynamic(this, &UProjectileEventRecorder::RecordImpact);
		Projectile->OnSkillDodged.AddDynamic(this, &UProjectileEventRecorder::RecordDodge);
	}

	UFUNCTION()
	void RecordImpact(AActor *Target, FVector ImpactLocation, float ImpactRadius, int32 Damage, int32 CastEntryIndex)
	{
		++Impacts;
		LastDamage = Damage;
		LastTarget = Target;
	}

	UFUNCTION()
	void RecordDodge(AActor *Target, FVector ImpactLocation)
	{
		++Dodges;
		LastTarget = Target;
	}
};
