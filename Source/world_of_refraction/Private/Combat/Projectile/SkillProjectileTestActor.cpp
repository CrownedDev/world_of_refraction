// SkillProjectileTestActor.cpp
// Test actor implementation for SkillProjectile verification

#include "Combat/Projectile/SkillProjectileTestActor.h"
#include "Combat/Projectile/SkillProjectile.h"
#include "Skills/Definitions/SpellData.h"
#include "Engine/World.h"
#include "TimerManager.h"

ASkillProjectileTestActor::ASkillProjectileTestActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Defaults
	TargetActor = nullptr;
	ProjectileClass = nullptr;
	TestSpell = nullptr;
	TestSpeed = 1500.f;
	TestImpactRadius = 1.0f;
	TestVisualScale = 1.0f;
	TestDamage = 50;
	BurstCount = 0;
}

void ASkillProjectileTestActor::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Log, TEXT("[SkillProjectileTest] Test actor ready. Assign Target and ProjectileClass, then use context menu."));
}

// ==================== TEST FUNCTIONS ====================

void ASkillProjectileTestActor::SpawnTestProjectile()
{
	if (!TargetActor)
	{
		UE_LOG(LogTemp, Error, TEXT("[SkillProjectileTest] No TargetActor assigned!"));
		return;
	}

	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[SkillProjectileTest] No ProjectileClass assigned! Assign BP_Projectile1."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[SkillProjectileTest] Spawning projectile toward %s"), *TargetActor->GetName());

	// Spawn projectile
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASkillProjectile* Projectile = GetWorld()->SpawnActor<ASkillProjectile>(
		ProjectileClass,
		GetActorLocation(),
		FRotator::ZeroRotator,
		SpawnParams
	);

	if (Projectile)
	{
		// Initialize without spell data (uses defaults)
		Projectile->InitializeProjectile(
			nullptr,            // No spell data
			this,               // Caster = this test actor
			TargetActor,        // Target
			TestImpactRadius,   // Impact radius
			TestVisualScale,    // Visual scale
			TestDamage,         // Damage
			ESpellElement::None // No spell -> non-elemental
		);

		// Bind to events
		Projectile->OnSkillImpact.AddDynamic(this, &ASkillProjectileTestActor::OnProjectileImpact);
		Projectile->OnSkillDodged.AddDynamic(this, &ASkillProjectileTestActor::OnProjectileDodged);

		UE_LOG(LogTemp, Log, TEXT("[SkillProjectileTest] Projectile spawned! Speed=%.1f, Radius=%.2f, Damage=%d"),
			TestSpeed, TestImpactRadius, TestDamage);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[SkillProjectileTest] Failed to spawn projectile!"));
	}
}

void ASkillProjectileTestActor::SpawnWithSpellData()
{
	if (!TargetActor)
	{
		UE_LOG(LogTemp, Error, TEXT("[SkillProjectileTest] No TargetActor assigned!"));
		return;
	}

	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[SkillProjectileTest] No ProjectileClass assigned!"));
		return;
	}

	if (!TestSpell)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillProjectileTest] No TestSpell assigned - using SpawnTestProjectile instead"));
		SpawnTestProjectile();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[SkillProjectileTest] Spawning projectile with spell: %s"), *TestSpell->Name);

	// Spawn projectile
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASkillProjectile* Projectile = GetWorld()->SpawnActor<ASkillProjectile>(
		ProjectileClass,
		GetActorLocation(),
		FRotator::ZeroRotator,
		SpawnParams
	);

	if (Projectile)
	{
		// Calculate size from spell data
		float FinalImpactRadius = TestSpell->BaseSize * TestSpell->HitboxRatio;
		float FinalVisualScale = TestSpell->BaseSize;

		// Initialize with spell data
		Projectile->InitializeProjectile(
			TestSpell,
			this,
			TargetActor,
			FinalImpactRadius,
			FinalVisualScale,
			TestDamage,
			TestSpell->Element  // test harness: authored element (no resolver context)
		);

		// Assign VFX from spell
		Projectile->SetVFXAssets(
			TestSpell->MuzzleVFX,
			TestSpell->SpellVFX,
			TestSpell->ImpactVFX
		);

		// Bind to events
		Projectile->OnSkillImpact.AddDynamic(this, &ASkillProjectileTestActor::OnProjectileImpact);
		Projectile->OnSkillDodged.AddDynamic(this, &ASkillProjectileTestActor::OnProjectileDodged);

		UE_LOG(LogTemp, Log, TEXT("[SkillProjectileTest] Spell projectile spawned! Type=%d, Speed=%.1f, Radius=%.2f"),
			(int32)TestSpell->DeliveryType, TestSpell->ProjectileSpeed, FinalImpactRadius);
	}
}

void ASkillProjectileTestActor::SpawnBurst()
{
	if (!TargetActor || !ProjectileClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[SkillProjectileTest] Missing TargetActor or ProjectileClass!"));
		return;
	}

	BurstCount = 3;
	UE_LOG(LogTemp, Log, TEXT("[SkillProjectileTest] Starting burst of %d projectiles"), BurstCount);
	
	SpawnNextBurst();
}

void ASkillProjectileTestActor::SpawnNextBurst()
{
	if (BurstCount <= 0) return;

	SpawnTestProjectile();
	BurstCount--;

	if (BurstCount > 0)
	{
		// Schedule next spawn in 0.3 seconds
		GetWorld()->GetTimerManager().SetTimer(
			BurstTimerHandle,
			this,
			&ASkillProjectileTestActor::SpawnNextBurst,
			0.3f,
			false
		);
	}
}

// ==================== EVENT HANDLERS ====================

void ASkillProjectileTestActor::OnProjectileImpact(AActor* Target, FVector ImpactLocation, float ImpactRadius, int32 Damage, int32 CastEntryIndex)
{
	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT("[SkillProjectileTest] IMPACT!"));
	UE_LOG(LogTemp, Display, TEXT("  Target: %s"), Target ? *Target->GetName() : TEXT("None"));
	UE_LOG(LogTemp, Display, TEXT("  Location: %s"), *ImpactLocation.ToString());
	UE_LOG(LogTemp, Display, TEXT("  Radius: %.2f"), ImpactRadius);
	UE_LOG(LogTemp, Display, TEXT("  Damage: %d"), Damage);
	UE_LOG(LogTemp, Display, TEXT("========================================"));

	// Visual feedback - draw debug sphere at impact
	DrawDebugSphere(GetWorld(), ImpactLocation, ImpactRadius * 100.f, 16, FColor::Red, false, 3.0f);
}

void ASkillProjectileTestActor::OnProjectileDodged(AActor* Target, FVector ImpactLocation)
{
	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT("[SkillProjectileTest] DODGED!"));
	UE_LOG(LogTemp, Display, TEXT("  Target: %s"), Target ? *Target->GetName() : TEXT("None"));
	UE_LOG(LogTemp, Display, TEXT("  Original Location: %s"), *ImpactLocation.ToString());
	if (Target)
	{
		float DistanceMoved = FVector::Dist(Target->GetActorLocation(), ImpactLocation);
		UE_LOG(LogTemp, Display, TEXT("  Distance Moved: %.1f"), DistanceMoved);
	}
	UE_LOG(LogTemp, Display, TEXT("========================================"));

	// Visual feedback - draw debug sphere at original location (green = missed)
	DrawDebugSphere(GetWorld(), ImpactLocation, 50.f, 16, FColor::Green, false, 3.0f);
}
