// BattleTransitionData.h
// B-500 (D-212): the battle transition's timings and look, changed without a rebuild. One data asset at a
// fixed path (BattleTransitionAsset::PATH); with none authored the class defaults play, with a plain tinted
// shard material. Lengths are in screen widths and depths in fractions of the wall's distance from the
// camera, so the motion looks the same at any resolution and field of view.
// Debug: the Details-panel button; wor.Transition.Play, wor.Transition.State (BattleTransitionDebug.h).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "BattleTransitionData.generated.h"

class UMaterialInterface;

namespace BattleTransitionAsset
{
	/** The asset the game reads (ContentNaming: DA_). Cooked by the asset manager's BattleTransitionData rule
	 *  (DefaultGame.ini, AlwaysCook). TODO: author it in the editor (docs/PIE/B-500.md, editor steps). */
	inline const TCHAR *PATH = TEXT("/Game/Data/Combat/DA_BattleTransition_Default.DA_BattleTransition_Default");

	/** The shard material's texture parameter that receives the captured frame. */
	inline const FName FRAME_PARAMETER = TEXT("Frame");

	/** B-500b: the shard material's scalar parameters for the prism light. The first three change every frame
	 *  (BattleTransition::EvaluatePrismLight); the last two are set once from the data asset. */
	inline const FName FLASH_PARAMETER = TEXT("Flash");
	inline const FName LIGHT_REACH_PARAMETER = TEXT("LightReach");
	inline const FName EDGE_GLOW_PARAMETER = TEXT("EdgeGlow");
	inline const FName SHARD_COLOR_PARAMETER = TEXT("ShardColor");
	inline const FName WHITE_REACH_PARAMETER = TEXT("WhiteReach");
}

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UBattleTransitionData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The authored asset, else the class defaults. Looked up on every call, so a new asset needs no restart. */
	static const UBattleTransitionData &Get();

	// ---- Timings (seconds). B-500a (Crown's PIE): it opens slowly and finishes fast: the cracks and the break take
	// their time inside the join window, the shards hold for a beat, then the blow-away accelerates hard. ----

	/** Contact to the last crack: the cracks spread from the hit point over this long. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = 0.01))
	float CrackSeconds = 0.5f;

	/** The frame breaks apart over this long, then the shards hold and drift. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = 0.01))
	float BreakSeconds = 0.7f;

	/** How slowly the break starts: 1 = even speed; higher = a slower start and end with a faster middle, so the
	 *  shards ease out of the wall and settle into the hold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = 1.0, ClampMax = 6.0))
	float BreakEase = 2.0f;

	/** If no map change has come this long after contact, the fight didn't start: the shards blow away
	 *  where they are, so they can never stay on screen. Keep it above every encounter's join window. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = 1.0))
	float MaxHoldSeconds = 8.0f;

	/** After the battle map has loaded, the held picture stays this long before the shards blow away. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = 0.0))
	float ArrivalSettleSeconds = 0.25f;

	/** The shards blow past the camera over this long. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = 0.01))
	float BlowAwaySeconds = 0.4f;

	/** How hard the blow-away accelerates: 1 = even speed; 3 = a short slow beat (the first third barely moves),
	 *  then a fast finish. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = 1.0, ClampMax = 6.0))
	float BlowAwayEase = 3.0f;

	/** The held picture fades off the live shards over this long as the blow-away starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = 0.01))
	float PictureFadeSeconds = 0.1f;

	// ---- Shards ----

	/** Columns x rows x 2 shards, each its own mesh moved every frame: the caps keep it to 320. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shards", meta = (ClampMin = 2, ClampMax = 16))
	int32 ShardColumns = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shards", meta = (ClampMin = 2, ClampMax = 10))
	int32 ShardRows = 7;

	/** How far the cut points crowd toward the hit point (0 = even shards, 1 = all at the hit). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shards", meta = (ClampMin = 0.0, ClampMax = 0.8))
	float HitPull = 0.4f;

	/** Shard thickness, in screen widths. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shards", meta = (ClampMin = 0.001))
	float ShardThickness = 0.012f;

	/** Width of the bevel around a shard's face, in screen widths. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shards", meta = (ClampMin = 0.0))
	float ShardBevel = 0.004f;

	/** Lit shard material; its "Frame" texture parameter gets the captured frame, read by UV0 (the screen).
	 *  Vertex colour: R = 1 on bevels and sides, G = a random per shard, B = distance from the hit,
	 *  A = the shard's hue (B-500b). The prism light's scalar parameters: BattleTransitionAsset.
	 *  Unset = the engine's basic material tinted Void, which shows no frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shards")
	TSoftObjectPtr<UMaterialInterface> ShardMaterial;

	/** What the frame capture stores. Scene colour (before post-processing) is exposed again on the shards. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shards")
	TEnumAsByte<ESceneCaptureSource> FrameCaptureSource = SCS_SceneColorHDR;

	// ---- Prism light (B-500b): a white flash at the hit, light running out through the cracks (white near the
	// hit, the spectrum further out), each shard catching a colour. The shard material draws it. ----

	/** The white flash at the hit fades over this long from contact. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prism", meta = (ClampMin = 0.01))
	float FlashSeconds = 0.25f;

	/** The flash's brightness at contact (above 1 blooms; 0 = no flash). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prism", meta = (ClampMin = 0.0))
	float FlashStrength = 6.0f;

	/** The flash covers, and the light stays white, this far from the hit (0 = at the hit, 1 = the farthest
	 *  shard); further out the light splits into the spectrum. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prism", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float WhiteReach = 0.25f;

	/** Brightness of the light in the cracks and on the shard edges as it arrives (above 1 blooms; 0 = none). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prism", meta = (ClampMin = 0.0))
	float EdgeGlowStrength = 4.0f;

	/** The share of the edge light that stays on through the hold; it settles to this during the break. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prism", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float EdgeGlowHoldShare = 0.35f;

	/** How strongly a shard's face takes its colour once the light has reached it (0 = the plain frame,
	 *  1 = fully tinted). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prism", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float ShardColorStrength = 0.45f;

	// ---- Motion ----

	/** A cracked shard tilts up to this many degrees, which is what shows the crack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion", meta = (ClampMin = 0.0))
	float CrackTiltDegrees = 5.0f;

	/** The break throws a shard up to this far out from the hit point, in screen widths. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion", meta = (ClampMin = 0.0))
	float BreakSpread = 0.06f;

	/** The break throws a shard up to this far toward the camera (fraction of the wall's distance). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion", meta = (ClampMin = 0.0, ClampMax = 0.4))
	float BreakTowardCamera = 0.15f;

	/** The break turns a shard up to this many degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion", meta = (ClampMin = 0.0))
	float BreakTumbleDegrees = 35.0f;

	/** During the hold a shard drifts at most this much further toward the camera, however long the hold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion", meta = (ClampMin = 0.0, ClampMax = 0.4))
	float DriftTowardCamera = 0.3f;

	/** The drift covers about two thirds of its distance in this many seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion", meta = (ClampMin = 0.1))
	float DriftSeconds = 2.5f;

	/** How fast a shard keeps turning during the hold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion", meta = (ClampMin = 0.0))
	float DriftTumbleDegreesPerSecond = 20.0f;

	/** The blow-away carries every shard at least this far toward the camera (1 = to the camera, more = past it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion", meta = (ClampMin = 1.0))
	float BlowAwayTowardCamera = 1.6f;

	/** The blow-away throws a shard up to this far outward, in screen widths. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion", meta = (ClampMin = 0.0))
	float BlowAwaySpread = 0.5f;

	// ---- Camera and lens ----

	/** The shard wall stands this far in front of the camera (cm). The lens focuses here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = 30.0))
	float WallDistance = 80.0f;

	/** The small push in during the crack and break: the frame grows by about this fraction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = 0.0, ClampMax = 0.3))
	float PushIn = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = 0.0))
	float BloomIntensity = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float MotionBlurAmount = 1.0f;

	/** Longest blur streak, in percent of the screen width. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = 0.0, ClampMax = 100.0))
	float MotionBlurMaxPercent = 15.0f;

	/** Depth of field: a lower number blurs the world behind and the nearest shards more. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = 1.0, ClampMax = 32.0))
	float FocusFstop = 1.2f;

#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category = "Debug")
	void LogValues() const;
#endif
};
