// BattleTransitionShards.h
// B-500 (D-212): the broken frame as real meshes in the world. One thick procedural mesh per shard, held in
// front of the local player's camera and moved every frame by the transition's clock
// (UBattleTransitionSubsystem), with the lens look (bloom, motion blur, depth of field) on an unbound
// post-process component. The actor dies with its world; the subsystem builds a new one from the same seed
// on the battle map. It ticks (after the camera has moved) because it is a per-frame visual.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/Transition/BattleTransitionMath.h"
#include "BattleTransitionShards.generated.h"

class UBattleTransitionData;
class UPostProcessComponent;
class UProceduralMeshComponent;
class UTexture;
class UTextureRenderTarget2D;
struct FMinimalViewInfo;

UCLASS(NotPlaceable, Transient)
class WORLD_OF_REFRACTION_API ABattleTransitionShards : public AActor
{
	GENERATED_BODY()

public:
	ABattleTransitionShards();

	/** The local player's camera this frame. False without a local player or camera (a server, a test). */
	static bool GetLocalCameraView(const UWorld *World, FMinimalViewInfo &OutView);

	/** Draws the world from the local camera into Target, as the frame the shards will carry. */
	void CaptureFrame(UTextureRenderTarget2D *Target, const UBattleTransitionData &InData);

	/** Cuts the wall and builds every shard. Frame may be null (the shards then show no picture). */
	void Build(const UBattleTransitionData &InData, int32 Seed, const FVector2D &HitUV, float InAspect, UTexture *Frame);

	/** Puts the wall in front of the camera and every shard where the clock says. */
	void ApplyClock(float Elapsed, float ReleaseElapsed);

	int32 GetShardCount() const { return Shards.Num(); }

	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TObjectPtr<UPostProcessComponent> Lens;

	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> ShardMeshes;

	UPROPERTY()
	TObjectPtr<const UBattleTransitionData> Data;

	TArray<BattleTransition::FShard> Shards;

	/** Each shard's centroid on the wall (Y, Z), in screen widths. */
	TArray<FVector2D> ShardHomes;

	float Aspect = 1.0f;
};
