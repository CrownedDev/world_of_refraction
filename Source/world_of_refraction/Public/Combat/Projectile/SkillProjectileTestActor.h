// SkillProjectileTestActor.h
// Test actor for SkillProjectile spawning and behavior verification

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkillProjectileTestActor.generated.h"

class ASkillProjectile;
class USpellData;

/**
 * Test actor for verifying SkillProjectile functionality
 * Place in level, assign Target, use context menu to spawn projectiles
 */
UCLASS()
class WORLD_OF_REFRACTION_API ASkillProjectileTestActor : public AActor
{
	GENERATED_BODY()

public:
	ASkillProjectileTestActor();

	// ==================== CONFIGURATION ====================

	/** Target actor to fire at (assign in editor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Config")
	AActor* TargetActor;

	/** Projectile class to spawn (assign BP_Projectile1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Config")
	TSubclassOf<ASkillProjectile> ProjectileClass;

	/** Optional spell data for full test */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Config")
	USpellData* TestSpell;

	/** Projectile speed override */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Config")
	float TestSpeed = 1500.f;

	/** Impact radius for dodge testing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Config")
	float TestImpactRadius = 1.0f;

	/** Visual scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Config")
	float TestVisualScale = 1.0f;

	/** Test damage value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Config")
	int32 TestDamage = 50;

	// ==================== TEST FUNCTIONS ====================

	/** Spawn a projectile toward target */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Tests")
	void SpawnTestProjectile();

	/** Spawn projectile with spell data */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Tests")
	void SpawnWithSpellData();

	/** Spawn 3 projectiles in sequence */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Tests")
	void SpawnBurst();

protected:
	virtual void BeginPlay() override;

private:
	/** Event handlers */
	UFUNCTION()
	void OnProjectileImpact(AActor* Target, FVector ImpactLocation, float ImpactRadius, int32 Damage);

	UFUNCTION()
	void OnProjectileDodged(AActor* Target, FVector ImpactLocation);

	/** Spawn count for burst */
	int32 BurstCount;
	FTimerHandle BurstTimerHandle;
	void SpawnNextBurst();
};
