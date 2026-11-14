// BattleTransitionSubsystem.h
// B-500 (D-212): the battle transition, rough version. No loading screen into a fight: at contact the frame
// is captured and broken into 3D shards in front of the camera (ABattleTransitionShards), which drift while
// the join window runs and the battle map loads, then blow away onto the battle.
//
// Why a game-instance subsystem: the effect starts in the trial world and ends in the battle world, so its
// clock, seed and captured frame must outlive both (it holds no fight state). The shards themselves die
// with the trial world. To cover the map change, the last frame is kept as a picture drawn over the
// viewport (above the level, so OpenLevel and seamless ServerTravel can't remove it); on the battle map the
// same shards are rebuilt from the seed behind the picture, the picture fades, and they blow away.
//
// Started by UEncounterComponent at contact (the player's hit, or an enemy's hit or engage). Not yet: the
// prism light (B-500b), sound (B-500c), first strike and ambush looks (B-500d).
// Debug: wor.Transition.Play, wor.Transition.State (BattleTransitionDebug.h).

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Styling/SlateBrush.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Combat/Transition/BattleTransitionMath.h"
#include "BattleTransitionSubsystem.generated.h"

class ABattleTransitionShards;
class SWidget;
class UBattleTransitionData;
class UTexture2D;
class UTextureRenderTarget2D;
struct FWorldContext;

UCLASS()
class WORLD_OF_REFRACTION_API UBattleTransitionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Null-safe lookup from any object with a world. */
	static UBattleTransitionSubsystem *Get(const UObject *WorldContext);

	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	/** Starts the transition, cracking from where HitWorldLocation shows on screen. Does nothing while one
	 *  is running, or on a machine with no local view (a dedicated server, a headless test). */
	void Play(const FVector &HitWorldLocation);

	/** As Play, from a point on the screen (0,0 top left .. 1,1 bottom right). */
	void PlayAtScreen(const FVector2D &InHitUV);

	/** The fight didn't start (or this is a preview): the shards blow away in the world they are in. */
	void Release();

	/** Drops everything at once. */
	void Cancel();

	/** Called by the shards each frame. False when Caller should destroy itself. */
	bool TickShards(ABattleTransitionShards &Caller, float DeltaSeconds);

	const BattleTransition::FClock &GetClock() const { return Clock; }
	int32 GetShardCount() const;
	bool HasPicture() const { return PictureWidget.IsValid(); }
	bool HasFrame() const { return Frame != nullptr; }

private:
	void HandlePreLoadMap(const FWorldContext &WorldContext, const FString &MapName);
	void HandleSeamlessTravelStart(UWorld *World, const FString &MapName);
	void HandlePostLoadMap(UWorld *World);
	void HandleTravelFailure(UWorld *World, ETravelFailure::Type FailureType, const FString &Reason);

	/** The map is about to change: keep the last frame as a picture over the viewport and stop the clock. */
	void FreezeForTravel();

	bool SpawnShards(UWorld &World, bool bCaptureFrame);
	void ShowPicture();
	void RemovePicture();
	float GetPictureAlpha() const;

	UPROPERTY()
	TObjectPtr<const UBattleTransitionData> Data;

	/** The frame at contact, shown on the shards. */
	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> Frame;

	/** The frame just before the map change, drawn over the viewport. */
	UPROPERTY()
	TObjectPtr<UTexture2D> Picture;

	TWeakObjectPtr<ABattleTransitionShards> Shards;
	TSharedPtr<SWidget> PictureWidget;
	FSlateBrush PictureBrush;

	BattleTransition::FClock Clock;
	int32 Seed = 0;
	FVector2D HitUV = FVector2D(0.5, 0.5);
	float Aspect = 1.0f;
};
