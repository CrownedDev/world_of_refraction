// SpellProjectileTestActor.cpp
// Test actor implementation for SpellProjectile verification

#include "SpellProjectileTestActor.h"
#include "SpellProjectile.h"
#include "SpellData.h"
#include "Engine/World.h"
#include "TimerManager.h"

ASpellProjectileTestActor::ASpellProjectileTestActor()
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

void ASpellProjectileTestActor::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Log, TEXT("[SpellProjectileTest] Test actor ready. Assign Target and ProjectileClass, then use context menu."));
}

// ==================== TEST FUNCTIONS ====================

void ASpellProjectileTestActor::SpawnTestProjectile()
{
	if (!TargetActor)
	{
		UE_LOG(LogTemp, Error, TEXT("[SpellProjectileTest] No TargetActor assigned!"));
		return;
	}

	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[SpellProjectileTest] No ProjectileClass assigned! Assign BP_Projectile1."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[SpellProjectileTest] Spawning projectile toward %s"), *TargetActor->GetName());

	// Spawn projectile
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASpellProjectile* Projectile = GetWorld()->SpawnActor<ASpellProjectile>(
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
			TestDamage          // Damage
		);

		// Bind to events
		Projectile->OnSpellImpact.AddDynamic(this, &ASpellProjectileTestActor::OnProjectileImpact);
		Projectile->OnSpellDodged.AddDynamic(this, &ASpellProjectileTestActor::OnProjectileDodged);

		UE_LOG(LogTemp, Log, TEXT("[SpellProjectileTest] Projectile spawned! Speed=%.1f, Radius=%.2f, Damage=%d"),
			TestSpeed, TestImpactRadius, TestDamage);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[SpellProjectileTest] Failed to spawn projectile!"));
	}
}

void ASpellProjectileTestActor::SpawnWithSpellData()
{
	if (!TargetActor)
	{
		UE_LOG(LogTemp, Error, TEXT("[SpellProjectileTest] No TargetActor assigned!"));
		return;
	}

	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[SpellProjectileTest] No ProjectileClass assigned!"));
		return;
	}

	if (!TestSpell)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SpellProjectileTest] No TestSpell assigned - using SpawnTestProjectile instead"));
		SpawnTestProjectile();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[SpellProjectileTest] Spawning projectile with spell: %s"), *TestSpell->SpellName);

	// Spawn projectile
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASpellProjectile* Projectile = GetWorld()->SpawnActor<ASpellProjectile>(
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
			TestDamage
		);

		// Assign VFX from spell
		Projectile->SetVFXAssets(
			TestSpell->MuzzleVFX,
			TestSpell->SpellVFX,
			TestSpell->ImpactVFX
		);

		// Bind to events
		Projectile->OnSpellImpact.AddDynamic(this, &ASpellProjectileTestActor::OnProjectileImpact);
		Projectile->OnSpellDodged.AddDynamic(this, &ASpellProjectileTestActor::OnProjectileDodged);

		UE_LOG(LogTemp, Log, TEXT("[SpellProjectileTest] Spell projectile spawned! Type=%d, Speed=%.1f, Radius=%.2f"),
			(int32)TestSpell->DeliveryType, TestSpell->ProjectileSpeed, FinalImpactRadius);
	}
}

void ASpellProjectileTestActor::SpawnBurst()
{
	if (!TargetActor || !ProjectileClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[SpellProjectileTest] Missing TargetActor or ProjectileClass!"));
		return;
	}

	BurstCount = 3;
	UE_LOG(LogTemp, Log, TEXT("[SpellProjectileTest] Starting burst of %d projectiles"), BurstCount);
	
	SpawnNextBurst();
}

void ASpellProjectileTestActor::SpawnNextBurst()
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
			&ASpellProjectileTestActor::SpawnNextBurst,
			0.3f,
			false
		);
	}
}

// ==================== EVENT HANDLERS ====================

void ASpellProjectileTestActor::OnProjectileImpact(AActor* Target, FVector ImpactLocation, float ImpactRadius, int32 Damage)
{
	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT("[SpellProjectileTest] IMPACT!"));
	UE_LOG(LogTemp, Display, TEXT("  Target: %s"), Target ? *Target->GetName() : TEXT("None"));
	UE_LOG(LogTemp, Display, TEXT("  Location: %s"), *ImpactLocation.ToString());
	UE_LOG(LogTemp, Display, TEXT("  Radius: %.2f"), ImpactRadius);
	UE_LOG(LogTemp, Display, TEXT("  Damage: %d"), Damage);
	UE_LOG(LogTemp, Display, TEXT("========================================"));

	// Visual feedback - draw debug sphere at impact
	DrawDebugSphere(GetWorld(), ImpactLocation, ImpactRadius * 100.f, 16, FColor::Red, false, 3.0f);
}

void ASpellProjectileTestActor::OnProjectileDodged(AActor* Target, FVector ImpactLocation)
{
	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT("[SpellProjectileTest] DODGED!"));
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
