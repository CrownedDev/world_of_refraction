// BattleTransitionShards.cpp

#include "Combat/Transition/BattleTransitionShards.h"

#include "Camera/CameraTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/Transition/BattleTransitionData.h"
#include "Combat/Transition/BattleTransitionSubsystem.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Infusion/ElementColors.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "SceneView.h"
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

	/** B-503f: tan of the view's half angles (horizontal, vertical), from the local player's real projection, so the
	 *  aspect-ratio axis rule (a camera may keep its vertical FOV) and the viewport's shape are both counted. With no
	 *  projection (a headless world), the FOV is read as horizontal. */
	FVector2D GetViewTangents(const UWorld *World, const FMinimalViewInfo &View, float Aspect)
	{
		const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		const APlayerController *Player = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
		const ULocalPlayer *Local = Player ? Player->GetLocalPlayer() : nullptr;
		FSceneViewProjectionData Projection;
		if (Local && Local->ViewportClient && Local->ViewportClient->Viewport &&
			Local->GetProjectionData(Local->ViewportClient->Viewport, Projection) && Projection.IsPerspectiveProjection() &&
			Projection.ProjectionMatrix.M[0][0] > 0.0 && Projection.ProjectionMatrix.M[1][1] > 0.0)
		{
			return FVector2D(1.0 / Projection.ProjectionMatrix.M[0][0], 1.0 / Projection.ProjectionMatrix.M[1][1]);
		}
		const double Horizontal = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(View.FOV, MIN_FIELD_OF_VIEW, MAX_FIELD_OF_VIEW)) * 0.5);
		return FVector2D(Horizontal, Horizontal / FMath::Max(Aspect, UE_KINDA_SMALL_NUMBER));
	}
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
	// The capture's FOV is always horizontal: take the screen's real one, so the picture lines up with the world.
	const float TargetAspect = static_cast<float>(Target->SizeX) / static_cast<float>(FMath::Max(Target->SizeY, 1));
	Capture->FOVAngle = FMath::RadiansToDegrees(2.0 * FMath::Atan(GetViewTangents(GetWorld(), View, TargetAspect).X));
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
	ShardMaterial = Material;
	if (Material && Authored)
	{
		if (Frame)
		{
			Material->SetTextureParameterValue(BattleTransitionAsset::FRAME_PARAMETER, Frame);
		}
		// A material without the prism parameters ignores them and shows the plain frame.
		Material->SetScalarParameterValue(BattleTransitionAsset::SHARD_COLOR_PARAMETER, InData.ShardColorStrength);
		Material->SetScalarParameterValue(BattleTransitionAsset::WHITE_REACH_PARAMETER, InData.WhiteReach);
	}
	else if (Material)
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
	// The actor sits on the camera, one unit = one screen width at the wall. The wall then fills the view on both
	// axes (B-503f: from the real projection), and every motion value reads in screen widths.
	const float WallX = static_cast<float>(BattleTransition::GetWallX(GetViewTangents(GetWorld(), View, Aspect), Aspect));
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

	if (ShardMaterial)
	{
		const BattleTransition::FPrismLight Light = BattleTransition::EvaluatePrismLight(*Data, Elapsed);
		ShardMaterial->SetScalarParameterValue(BattleTransitionAsset::FLASH_PARAMETER, Light.Flash);
		ShardMaterial->SetScalarParameterValue(BattleTransitionAsset::LIGHT_REACH_PARAMETER, Light.LightReach);
		ShardMaterial->SetScalarParameterValue(BattleTransitionAsset::EDGE_GLOW_PARAMETER, Light.EdgeGlow);
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
