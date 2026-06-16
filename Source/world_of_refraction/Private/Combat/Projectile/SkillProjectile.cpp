// SkillProjectile.cpp
// Unified spell delivery actor implementation

#include "Combat/Projectile/SkillProjectile.h"
#include "Skills/Definitions/SpellData.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Infusion/HybridSpellColors.h"
#include "DrawDebugHelpers.h"

// Note: SpellData properties used:
// - SpellVFX (main projectile effect)
// - ImpactVFX (hit explosion)
// - MuzzleVFX (cast flash at caster)
// - DeliveryType, ProjectileSpeed, HomingStrength
// - BaseSize, HitboxRatio, Element

// ==================== CONSTANTS ====================

namespace SkillProjectileConstants
{
    /** Default projectile speed (units/second) */
    constexpr float DEFAULT_SPEED = 1500.f;

    /** Default homing strength */
    constexpr float DEFAULT_HOMING_STRENGTH = 0.5f;

    /** Default beam duration (seconds) */
    constexpr float DEFAULT_BEAM_DURATION = 1.0f;

    /** Minimum distance to consider "reached destination" */
    constexpr float DESTINATION_THRESHOLD = 50.f;

    /** Time to wait after impact before destroying actor */
    constexpr float POST_IMPACT_LIFETIME = 2.0f;

    /** Default collision radius (before scaling) */
    constexpr float DEFAULT_COLLISION_RADIUS = 50.f;

    /** Scale multiplier from game units to UE units */
    constexpr float UNITS_SCALE = 100.f;
}

// ==================== CONSTRUCTOR ====================

ASkillProjectile::ASkillProjectile()
{
    PrimaryActorTick.bCanEverTick = true;

    // Root
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    // Collision
    HitBox = CreateDefaultSubobject<USphereComponent>(TEXT("HitBox"));
    HitBox->SetupAttachment(Root);
    HitBox->SetSphereRadius(SkillProjectileConstants::DEFAULT_COLLISION_RADIUS);
    HitBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    HitBox->SetGenerateOverlapEvents(true);

    // VFX Components (Niagara systems assigned at runtime)
    MuzzleFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("MuzzleFX"));
    MuzzleFX->SetupAttachment(Root);
    MuzzleFX->bAutoActivate = false;

    ProjectileFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ProjectileFX"));
    ProjectileFX->SetupAttachment(Root);
    ProjectileFX->bAutoActivate = false;

    HitFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("HitFX"));
    HitFX->SetupAttachment(Root);
    HitFX->bAutoActivate = false;

    // Default values
    DeliveryType = ESpellDeliveryType::Projectile;
    Element = ESpellElement::Fire;
    SourceSpell = nullptr;
    Caster = nullptr;
    Target = nullptr;
    TargetLocation = FVector::ZeroVector;
    Speed = SkillProjectileConstants::DEFAULT_SPEED;
    HomingStrength = SkillProjectileConstants::DEFAULT_HOMING_STRENGTH;
    ImpactRadius = 1.0f;
    Damage = 0;
    VisualScale = 1.0f;
    bHasImpacted = false;
}

// ==================== LIFECYCLE ====================

void ASkillProjectile::BeginPlay()
{
    Super::BeginPlay();

    // Bind collision event
    HitBox->OnComponentBeginOverlap.AddDynamic(this, &ASkillProjectile::OnHitBoxOverlap);
}

void ASkillProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bIsLaunched || bHasImpacted)
        return;

    if (bHasImpacted)
        return;

    switch (DeliveryType)
    {
    case ESpellDeliveryType::Projectile:
        TickProjectile(DeltaTime);
        break;

    case ESpellDeliveryType::Homing:
        TickHoming(DeltaTime);
        break;

    default:
        // AOE and Instant don't use this actor
        UE_LOG(LogTemp, Warning, TEXT("[SkillProjectile] Invalid DeliveryType for projectile actor"));
        DestroyProjectile();
        break;
    }
}

void ASkillProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Cleanup any bound delegates
    HitBox->OnComponentBeginOverlap.RemoveAll(this);

    Super::EndPlay(EndPlayReason);
}

// ==================== INITIALIZATION ====================

void ASkillProjectile::InitializeProjectile(
    USpellData *Spell,
    AActor *InCaster,
    AActor *InTarget,
    float FinalImpactRadius,
    float FinalVisualScale,
    int32 FinalDamage)
{
    SourceSpell = Spell;
    Caster = InCaster;
    Target = InTarget;
    ImpactRadius = FinalImpactRadius;
    VisualScale = FinalVisualScale;
    Damage = FinalDamage;

    if (Spell)
    {
        DeliveryType = Spell->DeliveryType;
        Element = Spell->Element;
        Speed = Spell->ProjectileSpeed;
        HomingStrength = Spell->HomingStrength;
    }
    else
    {
        // Fallback defaults
        DeliveryType = ESpellDeliveryType::Projectile;
        Element = ESpellElement::Generic;
        Speed = SkillProjectileConstants::DEFAULT_SPEED;
        HomingStrength = SkillProjectileConstants::DEFAULT_HOMING_STRENGTH;
    }

    InitializeCommon();
}

void ASkillProjectile::InitializeProjectile(
    const FSkillCastEntry &Entry,
    USpellData *Spell,
    AActor *InCaster,
    AActor *InTarget,
    float FinalImpactRadius,
    float FinalVisualScale,
    int32 FinalDamage,
    int32 InCastEntryIndex)
{
    SourceSpell = Spell;
    Caster = InCaster;
    Target = InTarget;
    ImpactRadius = FinalImpactRadius;
    VisualScale = FinalVisualScale;
    Damage = FinalDamage;

    // Cast-entry index (Stage 6 cluster 3) — read at arrival for per-entry defense difficulty
    // (cluster 4). Only the entry path stamps it; the legacy overload leaves it at INDEX_NONE.
    CastEntryIndex = InCastEntryIndex;

    // Delivery values come from the Cast ENTRY (D6) — Spell supplies element/
    // color context only.
    DeliveryType = Entry.DeliveryType;
    Element = Spell ? Spell->Element : ESpellElement::Generic;
    Speed = Entry.ProjectileSpeed;
    HomingStrength = Entry.HomingStrength;

    InitializeCommon();
}

void ASkillProjectile::InitializeCommon()
{
    // Capture target location at cast time (for Projectile type)
    if (Target)
    {
        TargetLocation = Target->GetActorLocation();
    }

    // Size collision to impact radius (convert to UE units)
    float CollisionRadius = ImpactRadius * SkillProjectileConstants::UNITS_SCALE;
    HitBox->SetSphereRadius(CollisionRadius);

    // Position at caster
    if (Caster)
    {
        SetActorLocation(Caster->GetActorLocation());

        FVector Direction = (TargetLocation - GetActorLocation()).GetSafeNormal();
        if (!Direction.IsNearlyZero())
        {
            SetActorRotation(Direction.Rotation());
        }
    }

    // Apply VFX settings
    ApplyElementColors();
    ApplyVisualScale();

    UE_LOG(LogTemp, Log, TEXT("[SkillProjectile] Initialized: Type=%d, Target=%s, Radius=%.2f, Speed=%.1f"),
           (int32)DeliveryType,
           Target ? *Target->GetName() : TEXT("None"),
           ImpactRadius,
           Speed);
}

void ASkillProjectile::SetVFXAssets(
    UNiagaraSystem *InMuzzleFX,
    UNiagaraSystem *InProjectileFX,
    UNiagaraSystem *InHitFX)
{
    if (InMuzzleFX && MuzzleFX)
    {
        MuzzleFX->SetAsset(InMuzzleFX);
    }

    if (InProjectileFX && ProjectileFX)
    {
        ProjectileFX->SetAsset(InProjectileFX);
    }

    if (InHitFX && HitFX)
    {
        HitFX->SetAsset(InHitFX);
    }

    // Reapply visual settings after asset change
    ApplyVisualScale();
    ApplyElementColors();
}

void ASkillProjectile::Launch()
{
    // Validate setup
    if (!Caster || !Target)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SkillProjectile] Launch called without valid Caster/Target"));
        return;
    }

    if (!ProjectileFX || !ProjectileFX->GetAsset())
    {
        UE_LOG(LogTemp, Warning, TEXT("[SkillProjectile] Launch called without ProjectileFX asset"));
    }

    // Start muzzle effect
    if (MuzzleFX && MuzzleFX->GetAsset())
    {
        MuzzleFX->Activate(true);
    }

    // Start projectile effect (for Projectile/Homing)
    if (DeliveryType == ESpellDeliveryType::Projectile ||
        DeliveryType == ESpellDeliveryType::Homing)
    {
        if (ProjectileFX && ProjectileFX->GetAsset())
        {
            ProjectileFX->Activate(true);
        }
    }

    bIsLaunched = true;

    UE_LOG(LogTemp, Log, TEXT("[SkillProjectile] Launched toward %s (VFX: %s)"),
           *Target->GetName(),
           (ProjectileFX && ProjectileFX->GetAsset()) ? TEXT("Active") : TEXT("None"));
}

// ==================== MOVEMENT ====================

void ASkillProjectile::TickProjectile(float DeltaTime)
{
    // Move toward FIXED target location (captured at cast time)
    FVector CurrentLocation = GetActorLocation();
    FVector Direction = (TargetLocation - CurrentLocation).GetSafeNormal();

    if (Direction.IsNearlyZero())
    {
        // Already at destination
        ResolveImpact();
        return;
    }

    FVector NewLocation = CurrentLocation + Direction * Speed * DeltaTime;
    SetActorLocation(NewLocation);
    SetActorRotation(Direction.Rotation());

    // Check if reached destination
    float DistanceRemaining = FVector::Dist(NewLocation, TargetLocation);
    if (DistanceRemaining < SkillProjectileConstants::DESTINATION_THRESHOLD)
    {
        ResolveImpact();
    }
}

void ASkillProjectile::TickHoming(float DeltaTime)
{
    if (!Target || !IsValid(Target))
    {
        // Target lost, continue to last known location
        TickProjectile(DeltaTime);
        return;
    }

    // Update target location with homing strength interpolation
    FVector CurrentTargetLocation = Target->GetActorLocation();
    TargetLocation = FMath::Lerp(TargetLocation, CurrentTargetLocation, HomingStrength);

    // Move toward interpolated location
    FVector CurrentLocation = GetActorLocation();
    FVector Direction = (TargetLocation - CurrentLocation).GetSafeNormal();

    if (Direction.IsNearlyZero())
    {
        ResolveImpact();
        return;
    }

    FVector NewLocation = CurrentLocation + Direction * Speed * DeltaTime;
    SetActorLocation(NewLocation);
    SetActorRotation(Direction.Rotation());

    // Homing uses overlap detection, not distance check
}

// ==================== COLLISION ====================

void ASkillProjectile::OnHitBoxOverlap(
    UPrimitiveComponent *OverlappedComponent,
    AActor *OtherActor,
    UPrimitiveComponent *OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult &SweepResult)
{
    // Ignore self and caster
    if (OtherActor == this || OtherActor == Caster)
        return;

    // Only trigger on target
    if (OtherActor != Target)
        return;
    if (bHasImpacted)
        return;

    UE_LOG(LogTemp, Log, TEXT("[SkillProjectile] Overlap with target: %s"), *OtherActor->GetName());

    // For Homing, impact on overlap (can't dodge by moving)
    if (DeliveryType == ESpellDeliveryType::Homing)
    {
        ResolveImpact();
    }
    // Projectile uses destination check in Tick, not overlap
    // This allows the move-dodge mechanic to work
}

// ==================== RESOLUTION ====================

void ASkillProjectile::ResolveImpact()
{
    if (bHasImpacted)
        return;
    bHasImpacted = true;

    UE_LOG(LogTemp, Log, TEXT("[SkillProjectile] Resolving impact at %s (CastEntryIndex=%d)"),
           *GetActorLocation().ToString(), CastEntryIndex);

    // Deactivate projectile VFX
    if (ProjectileFX)
    {
        ProjectileFX->Deactivate();
    }

    // Check for move dodge (Projectile type only)
    if (DeliveryType == ESpellDeliveryType::Projectile && CheckMoveDodge())
    {
        // Target moved out of impact zone - DODGED
        UE_LOG(LogTemp, Log, TEXT("[SkillProjectile] Target dodged by moving!"));
        OnSkillDodged.Broadcast(Target, TargetLocation);
        PlayHitEffect(TargetLocation); // VFX at original location
    }
    else
    {
        // HIT - broadcast for defense window
        FVector ImpactLocation = GetActorLocation();
        UE_LOG(LogTemp, Log, TEXT("[SkillProjectile] Impact! Damage=%d, Radius=%.2f"), Damage, ImpactRadius);
        OnSkillImpact.Broadcast(Target, ImpactLocation, ImpactRadius, Damage, CastEntryIndex);
        PlayHitEffect(ImpactLocation);
    }

    // Cleanup after VFX plays
    FTimerHandle TimerHandle;
    GetWorld()->GetTimerManager().SetTimer(
        TimerHandle,
        this,
        &ASkillProjectile::DestroyProjectile,
        SkillProjectileConstants::POST_IMPACT_LIFETIME,
        false);
}

bool ASkillProjectile::CheckMoveDodge() const
{
    if (!Target || !IsValid(Target))
        return false;

    // How far did target move from original position?
    FVector CurrentTargetLocation = Target->GetActorLocation();
    float DistanceMoved = FVector::Dist(CurrentTargetLocation, TargetLocation);

    // Convert ImpactRadius to UE units for comparison
    float ImpactRadiusUnits = ImpactRadius * SkillProjectileConstants::UNITS_SCALE;

    // Dodged if moved beyond impact radius
    bool bDodged = DistanceMoved > ImpactRadiusUnits;

    UE_LOG(LogTemp, Verbose, TEXT("[SkillProjectile] Dodge check: Moved=%.1f, Radius=%.1f, Dodged=%d"),
           DistanceMoved, ImpactRadiusUnits, bDodged);

    return bDodged;
}

void ASkillProjectile::PlayHitEffect(FVector Location)
{
    if (HitFX && HitFX->GetAsset())
    {
        HitFX->SetWorldLocation(Location);
        HitFX->Activate(true);
    }
}

void ASkillProjectile::DestroyProjectile()
{
    UE_LOG(LogTemp, Verbose, TEXT("[SkillProjectile] Destroying projectile"));
    Destroy();
}

// ==================== VFX ====================

void ASkillProjectile::ApplyElementColors()
{
    // Get element colors (supports BD hybrid colors)
    FHybridSpellColorData Colors = UHybridSpellColors::GetInfusionColors(Element, false);

    // Apply to each Niagara component
    TArray<UNiagaraComponent *> VFXComponents = {MuzzleFX, ProjectileFX, HitFX};

    for (UNiagaraComponent *Comp : VFXComponents)
    {
        if (Comp && Comp->GetAsset())
        {
            Comp->SetColorParameter(FName("CoreColor"), Colors.PrimaryColor);
            Comp->SetColorParameter(FName("EdgeColor"), Colors.BlendedColor);
            Comp->SetColorParameter(FName("TrailColor"), Colors.SecondaryColor);
        }
    }
}

void ASkillProjectile::ApplyVisualScale()
{
    // Scale VFX components
    FVector Scale = FVector(VisualScale);

    if (ProjectileFX)
    {
        ProjectileFX->SetWorldScale3D(Scale);
    }

    if (HitFX)
    {
        HitFX->SetWorldScale3D(Scale);
    }

    // Muzzle typically stays at caster scale
}

// ==================== DEBUG ====================

#if WITH_EDITOR
void ASkillProjectile::Debug_DrawCollision()
{
    if (HitBox)
    {
        float Radius = HitBox->GetScaledSphereRadius();
        DrawDebugSphere(GetWorld(), GetActorLocation(), Radius, 16, FColor::Green, false, 5.0f);

        UE_LOG(LogTemp, Display, TEXT("[SkillProjectile] Collision radius: %.1f"), Radius);
    }
}

void ASkillProjectile::Debug_PrintState()
{
    UE_LOG(LogTemp, Display, TEXT("=========================================="));
    UE_LOG(LogTemp, Display, TEXT("SPELL PROJECTILE STATE"));
    UE_LOG(LogTemp, Display, TEXT("=========================================="));
    UE_LOG(LogTemp, Display, TEXT("DeliveryType: %d"), (int32)DeliveryType);
    UE_LOG(LogTemp, Display, TEXT("Element: %d"), (int32)Element);
    UE_LOG(LogTemp, Display, TEXT("Caster: %s"), Caster ? *Caster->GetName() : TEXT("None"));
    UE_LOG(LogTemp, Display, TEXT("Target: %s"), Target ? *Target->GetName() : TEXT("None"));
    UE_LOG(LogTemp, Display, TEXT("TargetLocation: %s"), *TargetLocation.ToString());
    UE_LOG(LogTemp, Display, TEXT("Speed: %.1f"), Speed);
    UE_LOG(LogTemp, Display, TEXT("ImpactRadius: %.2f"), ImpactRadius);
    UE_LOG(LogTemp, Display, TEXT("Damage: %d"), Damage);
    UE_LOG(LogTemp, Display, TEXT("VisualScale: %.2f"), VisualScale);
    UE_LOG(LogTemp, Display, TEXT("bHasImpacted: %d"), bHasImpacted);
    UE_LOG(LogTemp, Display, TEXT("=========================================="));
}
#endif
