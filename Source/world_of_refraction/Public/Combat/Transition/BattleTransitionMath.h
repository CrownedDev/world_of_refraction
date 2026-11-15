// BattleTransitionMath.h
// B-500 (D-212): the battle transition's plain maths, with no world or renderer, so specs run it headless:
// cutting the screen into shards, where a shard is at a given time, a shard's thick mesh, and the clock
// that carries the effect across the map change.
//
// Spaces: "UV" is the screen, (0,0) top left to (1,1) bottom right. "Wall" is the shard wall's plane in
// screen widths: Y right, Z up, centred on the screen; X points away from the camera.

#pragma once

#include "CoreMinimal.h"

class UBattleTransitionData;

namespace BattleTransition
{
	/** The wall reaches this far past each screen edge (UV), so a tilted edge shard never shows a gap. */
	inline constexpr float OVERSCAN = 0.04f;

	/** A longer frame than this counts as this long: the first frame after a map load can be seconds. */
	inline constexpr float MAX_STEP_SECONDS = 0.05f;

	/** One piece of the broken frame. */
	struct FShard
	{
		FVector2D Corners[3];						  // UV
		FVector2D Centroid = FVector2D::ZeroVector;	  // UV
		FVector2D Outward = FVector2D(1.0, 0.0);	  // wall plane (Y, Z), unit: away from the hit point
		float Delay = 0.0f;							  // 0 at the hit point .. 1 at the farthest shard
		float RandomA = 0.0f;						  // 0..1
		float RandomB = 0.0f;						  // 0..1
		FVector TumbleAxis = FVector::RightVector;	  // unit
	};

	/** Where a shard is, relative to its place in the unbroken wall. */
	struct FShardPose
	{
		FVector2D WallOffset = FVector2D::ZeroVector; // wall plane (Y, Z), screen widths
		float TowardCamera = 0.0f;					  // fraction of the wall's distance: 0 = in the wall, 1 = at the camera
		FQuat Rotation = FQuat::Identity;
	};

	/** A shard's mesh around its centroid, in wall space (screen widths). */
	struct FShardMesh
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FColor> Colors;
	};

	/** UV to the wall plane (Y, Z). Aspect = width / height. */
	WORLD_OF_REFRACTION_API FVector2D UVToWall(const FVector2D &UV, float Aspect);

	/** B-503f: how far in front of the camera the unbroken wall sits, in screen widths, so it fills the view.
	 *  ViewTangents = tan of the view's half angles (horizontal, vertical), read from the real projection, so any
	 *  aspect, FOV or aspect-ratio axis rule is covered; the wall goes as near as the tighter axis needs. */
	WORLD_OF_REFRACTION_API double GetWallX(const FVector2D &ViewTangents, float Aspect);

	/** Cuts the overscanned screen into 2 x columns x rows triangles, crowded toward HitUV. The same seed
	 *  gives the same shards, which is how the battle map rebuilds the ones the trial map showed. */
	WORLD_OF_REFRACTION_API TArray<FShard> BuildShards(int32 Seed, const FVector2D &HitUV, float Aspect,
													   const UBattleTransitionData &Data);

	/** A shard at Elapsed seconds after contact, ReleaseElapsed seconds into the blow-away (0 = still held). */
	WORLD_OF_REFRACTION_API FShardPose EvaluatePose(const FShard &Shard, const UBattleTransitionData &Data,
													float Elapsed, float ReleaseElapsed);

	/** 0..1: how far the push in and the lens look have come on, and gone again with the blow-away. */
	WORLD_OF_REFRACTION_API float GetLookAlpha(const UBattleTransitionData &Data, float Elapsed, float ReleaseElapsed);

	/** B-500b: what the shard material is told each frame (BattleTransitionAsset's scalar parameters). */
	struct FPrismLight
	{
		float Flash = 0.0f;		 // brightness of the white flash at the hit
		float LightReach = 0.0f; // how far the light has run out through the cracks, in FShard::Delay units; runs on past 1
		float EdgeGlow = 0.0f;	 // brightness of the light in the cracks and on the shard edges
	};

	/** The prism light at Elapsed seconds after contact. The light reaches a shard as its crack does. */
	WORLD_OF_REFRACTION_API FPrismLight EvaluatePrismLight(const UBattleTransitionData &Data, float Elapsed);

	/** 0..1 around the colour wheel (0 = red): the shard's direction from the hit point, so the spectrum fans
	 *  out around the hit as on the storyboard, with a little of the shard's own random. */
	WORLD_OF_REFRACTION_API float GetShardHue(const FShard &Shard);

	/** A thick bevelled prism: the face toward the camera carries the frame; bevels and sides are marked in
	 *  vertex colour R. G = the shard's random, B = its distance from the hit, A = its hue. */
	WORLD_OF_REFRACTION_API FShardMesh BuildShardMesh(const FShard &Shard, float Aspect, const UBattleTransitionData &Data);

	enum class EState : uint8
	{
		Idle,
		Playing,   // shards live in the world the fight started in
		Frozen,	   // the map is changing: only the held picture shows, the clock stands still
		Arriving,  // the new map is up under the held picture, the shards are rebuilt behind it
		Releasing  // the shards blow away
	};

	/** The transition's clock. It lives across the map change (the shards don't). */
	struct WORLD_OF_REFRACTION_API FClock
	{
		EState State = EState::Idle;
		float Elapsed = 0.0f;		  // seconds of shard motion since contact
		float ReleaseElapsed = 0.0f;  // seconds into the blow-away
		float SettleRemaining = 0.0f; // Arriving: seconds left under the held picture

		void Start();
		/** Playing -> Frozen. False (and no change) from any other state. */
		bool Freeze();
		/** Frozen -> Arriving. */
		bool Arrive(const UBattleTransitionData &Data);
		/** Playing -> Releasing, in the same world. */
		bool Release();
		void Reset();

		/** One frame. True when the blow-away has just finished (the clock is Idle again). */
		bool Advance(float DeltaSeconds, const UBattleTransitionData &Data);

		/** 1 while the held picture covers the screen, fading to 0 as the blow-away starts. */
		float GetPictureAlpha(const UBattleTransitionData &Data) const;
	};

	WORLD_OF_REFRACTION_API const TCHAR *GetStateName(EState State);
}
