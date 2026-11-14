// BattleTransitionShards.cpp

#include "Combat/Transition/BattleTransitionShards.h"

#include "Camera/CameraTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/Transition/BattleTransitionData.h"
#include "Combat/Transition/BattleTransitionSubsystem.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Infusion/ElementColors.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Skills/Definitions/ESpellElement.h"

namespace
{
	/** Above the level's own post-process volumes, so the transition's lens wins while it plays. */
	constexpr float LENS_PRIORITY = 1000.0f;

	/** The camera's field of view is read inside this range (degrees), so the wall's size stays finite. */
	constexpr float MIN_FIELD_OF_VIEW = 5.0f;
	constexpr float MAX_FIELD_OF_VIEW = 170.0f;

	/** Stand-in until the shard material is authored: shows the motion, not the frame. */
	const TCHAR *FALLBACK_MATERIAL_PATH = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	const FName FALLBACK_COLOR_PARAMETER = TEXT("Color");
}

ABattleTransitionShards::ABattleTransitionShards()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork; // after the camera has moved this frame, so the wall never lags it
	PrimaryActorTick.bTickEvenWhenPaused = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Lens = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Lens"));
	Lens->SetupAttachment(RootComponent);
	Lens->bUnbound = true;
	Lens->Priority = LENS_PRIORITY;
	Lens->BlendWeight = 0.0f;
}

bool ABattleTransitionShards::GetLocalCameraView(const UWorld *World, FMinimalViewInfo &OutView)
{
	const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
	const APlayerController *Player = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
	if (!Player || !Player->PlayerCameraManager)
	{
		return false;
	}
	OutView = Player->PlayerCameraManager->GetCameraCacheView();
	return true;
}

void ABattleTransitionShards::CaptureFrame(UTextureRenderTarget2D *Target, const UBattleTransitionData &InData)
{
	FMinimalViewInfo View;
	if (!Target || !GetLocalCameraView(GetWorld(), View))
	{
		return;
	}
	USceneCaptureComponent2D *Capture = NewObject<USceneCaptureComponent2D>(this);
	Capture->bCaptureEveryFrame = false; // one picture, taken below
	Capture->bCaptureOnMovement = false;
	Capture->CaptureSource = InData.FrameCaptureSource;
	Capture->TextureTarget = Target;
	Capture->FOVAngle = View.FOV;
	Capture->SetupAttachment(RootComponent);
	Capture->RegisterComponent();
	Capture->SetWorldLocationAndRotation(View.Location, View.Rotation);
	Capture->CaptureScene();
}

void ABattleTransitionShards::Build(const UBattleTransitionData &InData, int32 Seed, const FVector2D &HitUV, float InAspect, UTexture *Frame)
{
	Data = &InData;
	Aspect = InAspect;
	Shards = BattleTransition::BuildShards(Seed, HitUV, Aspect, InData);

	UMaterialInterface *Authored = InData.ShardMaterial.LoadSynchronous();
	UMaterialInterface *Base = Authored ? Authored : LoadObject<UMaterialInterface>(nullptr, FALLBACK_MATERIAL_PATH);
	UMaterialInstanceDynamic *Material = Base ? UMaterialInstanceDynamic::Create(Base, this) : nullptr;
	if (Material && Authored && Frame)
	{
		Material->SetTextureParameterValue(BattleTransitionAsset::FRAME_PARAMETER, Frame);
	}
	else if (Material && !Authored)
	{
		Material->SetVectorParameterValue(FALLBACK_COLOR_PARAMETER, ElementColors::GetColorForElement(ESpellElement::Void));
	}

	ShardMeshes.Reserve(Shards.Num());
	ShardHomes.Reserve(Shards.Num());
	for (const BattleTransition::FShard &Shard : Shards)
	{
		const BattleTransition::FShardMesh Mesh = BattleTransition::BuildShardMesh(Shard, Aspect, InData);
		// One component per shard: each moves by its own transform, which is what gives it motion blur.
		UProceduralMeshComponent *Component = NewObject<UProceduralMeshComponent>(this);
		Component->SetupAttachment(RootComponent);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		Component->RegisterComponent();
		Component->CreateMeshSection(0, Mesh.Vertices, Mesh.Triangles, Mesh.Normals, Mesh.UVs, Mesh.Colors, TArray<FProcMeshTangent>(), false);
		Component->SetMaterial(0, Material);
		ShardMeshes.Add(Component);
		ShardHomes.Add(BattleTransition::UVToWall(Shard.Centroid, Aspect));
	}

	FPostProcessSettings &Settings = Lens->Settings;
	Settings.bOverride_BloomIntensity = true;
	Settings.BloomIntensity = InData.BloomIntensity;
	Settings.bOverride_MotionBlurAmount = true;
	Settings.MotionBlurAmount = InData.MotionBlurAmount;
	Settings.bOverride_MotionBlurMax = true;
	Settings.MotionBlurMax = InData.MotionBlurMaxPercent;
	Settings.bOverride_DepthOfFieldFocalDistance = true;
	Settings.DepthOfFieldFocalDistance = InData.WallDistance; // sharp shards, soft world behind and soft near shards
	Settings.bOverride_DepthOfFieldFstop = true;
	Settings.DepthOfFieldFstop = InData.FocusFstop;
}

void ABattleTransitionShards::ApplyClock(float Elapsed, float ReleaseElapsed)
{
	FMinimalViewInfo View;
	if (!Data || !GetLocalCameraView(GetWorld(), View))
	{
		return;
	}
	// The actor sits on the camera, one unit = one screen width at the wall. The wall then fills the view
	// at any field of view, and every motion value reads in screen widths.
	// shortcut: reads FOV as horizontal (the engine default); a vertical-FOV view would need the other axis.
	const float HalfFieldOfView = FMath::DegreesToRadians(FMath::Clamp(View.FOV, MIN_FIELD_OF_VIEW, MAX_FIELD_OF_VIEW)) * 0.5f;
	const float WallX = 1.0f / (2.0f * FMath::Tan(HalfFieldOfView));
	SetActorTransform(FTransform(View.Rotation, View.Location, FVector(Data->WallDistance / WallX)));

	const float Look = BattleTransition::GetLookAlpha(*Data, Elapsed, ReleaseElapsed);
	Lens->BlendWeight = Look;
	// shortcut: the push in moves the wall toward the camera (the frame grows); the real camera stays put.
	// Move the camera itself if the world behind should push in too.
	const float PushedWallX = WallX * (1.0f - Data->PushIn * Look);
	for (int32 Index = 0; Index < ShardMeshes.Num(); ++Index)
	{
		const BattleTransition::FShardPose Pose = BattleTransition::EvaluatePose(Shards[Index], *Data, Elapsed, ReleaseElapsed);
		const FVector2D &Home = ShardHomes[Index];
		ShardMeshes[Index]->SetRelativeLocationAndRotation(
			FVector(PushedWallX * (1.0f - Pose.TowardCamera), Home.X + Pose.WallOffset.X, Home.Y + Pose.WallOffset.Y), Pose.Rotation);
	}
}

void ABattleTransitionShards::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UBattleTransitionSubsystem *Transition = UBattleTransitionSubsystem::Get(this);
	if (!Transition || !Transition->TickShards(*this, DeltaSeconds))
	{
		Destroy(); // the transition ended, or these shards are no longer its own
	}
}
