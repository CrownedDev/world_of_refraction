// BattleTransitionSubsystem.cpp

#include "Combat/Transition/BattleTransitionSubsystem.h"

#include "Camera/CameraTypes.h"
#include "Combat/Transition/BattleTransitionData.h"
#include "Combat/Transition/BattleTransitionShards.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Slate/DeferredCleanupSlateBrush.h"
#include "UnrealClient.h"
#include "Widgets/Images/SImage.h"
#include "WoRLog.h"

namespace
{
	/** Above every HUD and menu widget on the viewport. */
	constexpr int32 PICTURE_Z_ORDER = 10000;

	/** The crack starts at least this far in from a screen edge (UV), so there are shards on every side of it. */
	constexpr double HIT_EDGE_MARGIN = 0.15;

	/** A seamless travel that hasn't reached its map this long after it started is taken as lost. */
	constexpr double MAX_FROZEN_SECONDS = 120.0;
}

UBattleTransitionSubsystem *UBattleTransitionSubsystem::Get(const UObject *WorldContext)
{
	const UWorld *World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UBattleTransitionSubsystem>() : nullptr;
}

void UBattleTransitionSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	// OpenLevel announces itself with PreLoadMap, seamless ServerTravel with OnSeamlessTravelStart; both end
	// with PostLoadMapWithWorld on the map the players land in.
	FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &UBattleTransitionSubsystem::HandlePreLoadMap);
	FWorldDelegates::OnSeamlessTravelStart.AddUObject(this, &UBattleTransitionSubsystem::HandleSeamlessTravelStart);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UBattleTransitionSubsystem::HandlePostLoadMap);
	if (GEngine)
	{
		GEngine->OnTravelFailure().AddUObject(this, &UBattleTransitionSubsystem::HandleTravelFailure);
	}
}

void UBattleTransitionSubsystem::Deinitialize()
{
	Cancel();
	FCoreUObjectDelegates::PreLoadMapWithContext.RemoveAll(this);
	FWorldDelegates::OnSeamlessTravelStart.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	if (GEngine)
	{
		GEngine->OnTravelFailure().RemoveAll(this);
	}
	Super::Deinitialize();
}

void UBattleTransitionSubsystem::Play(const FVector &HitWorldLocation)
{
	FVector2D ScreenUV(0.5, 0.5);
	const APlayerController *Player = GetGameInstance()->GetFirstLocalPlayerController(GetGameInstance()->GetWorld());
	int32 SizeX = 0;
	int32 SizeY = 0;
	FVector2D ScreenLocation;
	if (Player)
	{
		Player->GetViewportSize(SizeX, SizeY);
		if (SizeX > 0 && SizeY > 0 && Player->ProjectWorldLocationToScreen(HitWorldLocation, ScreenLocation))
		{
			ScreenUV = ScreenLocation / FVector2D(SizeX, SizeY);
		}
	}
	PlayAtScreen(ScreenUV);
}

void UBattleTransitionSubsystem::PlayAtScreen(const FVector2D &InHitUV)
{
	UWorld *World = GetGameInstance()->GetWorld();
	const UGameViewportClient *ViewportClient = GetGameInstance()->GetGameViewportClient();
	const FIntPoint Size = ViewportClient && ViewportClient->Viewport ? ViewportClient->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
	FMinimalViewInfo View;
	if (!World || Size.X <= 0 || Size.Y <= 0 || !ABattleTransitionShards::GetLocalCameraView(World, View))
	{
		UE_LOG(LogWoR, Verbose, TEXT("[BattleTransition] No local view here - not playing"));
		return;
	}
	if (Clock.State != BattleTransition::EState::Idle)
	{
		if (Shards.IsValid() || Clock.State == BattleTransition::EState::Frozen)
		{
			UE_LOG(LogWoR, Log, TEXT("[BattleTransition] Already %s - not starting another"), BattleTransition::GetStateName(Clock.State));
			return;
		}
		Cancel(); // its shards were lost with their world: start clean
	}

	Data = &UBattleTransitionData::Get();
	Seed = FMath::Rand(); // look only; fight randomness is BattleRandom's
	HitUV = InHitUV.ClampAxes(HIT_EDGE_MARGIN, 1.0 - HIT_EDGE_MARGIN);
	Aspect = static_cast<float>(Size.X) / static_cast<float>(Size.Y);

	Frame = NewObject<UTextureRenderTarget2D>(this);
	Frame->RenderTargetFormat = RTF_RGBA16f; // scene colour is brighter than 1
	Frame->ClearColor = FLinearColor::Black;
	Frame->AddressX = TA_Clamp; // the wall reaches past the screen edge
	Frame->AddressY = TA_Clamp;
	Frame->InitAutoFormat(Size.X, Size.Y);
	Frame->UpdateResourceImmediate(true);

	Clock.Start();
	if (!SpawnShards(*World, true))
	{
		UE_LOG(LogWoR, Warning, TEXT("[BattleTransition] Couldn't spawn the shards in %s - not playing"), *World->GetName());
		Cancel();
		return;
	}
	UE_LOG(LogWoR, Log, TEXT("[BattleTransition] Started: %d shards from (%.2f, %.2f), frame %dx%d, %s"), GetShardCount(), HitUV.X,
		   HitUV.Y, Size.X, Size.Y, Data->HasAnyFlags(RF_ClassDefaultObject) ? TEXT("class defaults (no asset)") : *Data->GetName());
}

void UBattleTransitionSubsystem::Release()
{
	if (Clock.Release())
	{
		UE_LOG(LogWoR, Log, TEXT("[BattleTransition] Released in place after %.2fs"), Clock.Elapsed);
	}
}

void UBattleTransitionSubsystem::Cancel()
{
	if (ABattleTransitionShards *Actor = Shards.Get())
	{
		Actor->Destroy();
	}
	EndRun();
}

void UBattleTransitionSubsystem::EndRun()
{
	Shards = nullptr;
	RemovePicture();
	Frame = nullptr;
	Clock.Reset();
	FTSTicker::RemoveTicker(WatchHandle);
	WatchHandle.Reset();
}

bool UBattleTransitionSubsystem::WatchTravel(float /*DeltaSeconds*/)
{
	switch (Clock.State)
	{
	case BattleTransition::EState::Frozen:
		if (FPlatformTime::Seconds() - FrozenAtSeconds < MAX_FROZEN_SECONDS)
		{
			return true;
		}
		UE_LOG(LogWoR, Warning, TEXT("[BattleTransition] No map arrived %.0fs after the map change - dropped"), MAX_FROZEN_SECONDS);
		break;
	case BattleTransition::EState::Arriving:
	case BattleTransition::EState::Releasing:
		if (Shards.IsValid())
		{
			return true;
		}
		UE_LOG(LogWoR, Warning, TEXT("[BattleTransition] The rebuilt shards were lost - dropped"));
		break;
	default:
		return false; // not in a map change: nothing to watch
	}
	Cancel();
	return false;
}

bool UBattleTransitionSubsystem::TickShards(ABattleTransitionShards &Caller, float DeltaSeconds)
{
	if (Shards.Get() != &Caller || !Data)
	{
		return false;
	}
	if (Clock.Advance(DeltaSeconds, *Data))
	{
		UE_LOG(LogWoR, Log, TEXT("[BattleTransition] Finished"));
		EndRun();
		return false;
	}
	Caller.ApplyClock(Clock.Elapsed, Clock.ReleaseElapsed);
	return true;
}

int32 UBattleTransitionSubsystem::GetShardCount() const
{
	return Shards.IsValid() ? Shards->GetShardCount() : 0;
}

void UBattleTransitionSubsystem::HandlePreLoadMap(const FWorldContext &WorldContext, const FString & /*MapName*/)
{
	if (WorldContext.OwningGameInstance == GetGameInstance())
	{
		FreezeForTravel();
	}
}

void UBattleTransitionSubsystem::HandleSeamlessTravelStart(UWorld *World, const FString & /*MapName*/)
{
	if (World && World->GetGameInstance() == GetGameInstance())
	{
		FreezeForTravel();
	}
}

void UBattleTransitionSubsystem::FreezeForTravel()
{
	if (Clock.State == BattleTransition::EState::Idle || Clock.State == BattleTransition::EState::Frozen)
	{
		return;
	}
	if (!Clock.Freeze())
	{
		Cancel(); // the map changed during a blow-away: nothing left worth carrying over
		return;
	}
	FrozenAtSeconds = FPlatformTime::Seconds();
	WatchHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UBattleTransitionSubsystem::WatchTravel));
	ShowPicture();
	if (ABattleTransitionShards *Actor = Shards.Get())
	{
		Actor->Destroy();
	}
	Shards = nullptr;
	UE_LOG(LogWoR, Log, TEXT("[BattleTransition] Map change at %.2fs - holding the picture (%s)"), Clock.Elapsed,
		   HasPicture() ? TEXT("shown") : TEXT("none: no viewport picture"));
}

void UBattleTransitionSubsystem::HandlePostLoadMap(UWorld *World)
{
	if (Clock.State != BattleTransition::EState::Frozen)
	{
		return;
	}
	if (!World)
	{
		Cancel(); // the load failed
		return;
	}
	if (World->GetGameInstance() != GetGameInstance())
	{
		return; // another game instance's map (a second PIE window)
	}
	if (!Clock.Arrive(*Data) || !SpawnShards(*World, false))
	{
		UE_LOG(LogWoR, Warning, TEXT("[BattleTransition] Couldn't rebuild the shards in %s - dropped"), *World->GetName());
		Cancel();
		return;
	}
	UE_LOG(LogWoR, Log, TEXT("[BattleTransition] Arrived in %s - %d shards rebuilt behind the picture"), *World->GetName(), GetShardCount());
}

void UBattleTransitionSubsystem::HandleTravelFailure(UWorld *World, ETravelFailure::Type /*FailureType*/, const FString &Reason)
{
	if (World && World->GetGameInstance() != GetGameInstance())
	{
		return; // another game instance's travel (a second PIE window)
	}
	if (Clock.State != BattleTransition::EState::Idle)
	{
		UE_LOG(LogWoR, Warning, TEXT("[BattleTransition] Travel failed (%s) - dropped"), *Reason);
		Cancel();
	}
}

bool UBattleTransitionSubsystem::SpawnShards(UWorld &World, bool bCaptureFrame)
{
	FActorSpawnParameters Parameters;
	Parameters.ObjectFlags |= RF_Transient;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABattleTransitionShards *Actor = World.SpawnActor<ABattleTransitionShards>(FVector::ZeroVector, FRotator::ZeroRotator, Parameters);
	if (!Actor)
	{
		return false;
	}
	if (bCaptureFrame)
	{
		Actor->CaptureFrame(Frame, *Data); // before the shards exist, so the frame doesn't show them
	}
	Actor->Build(*Data, Seed, HitUV, Aspect, Frame);
	Actor->ApplyClock(Clock.Elapsed, Clock.ReleaseElapsed);
	Shards = Actor;
	return true;
}

void UBattleTransitionSubsystem::ShowPicture()
{
	UGameViewportClient *ViewportClient = GetGameInstance()->GetGameViewportClient();
	FViewport *Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
	TArray<FColor> Pixels;
	if (!Viewport || !GetViewportScreenShot(Viewport, Pixels))
	{
		UE_LOG(LogWoR, Warning, TEXT("[BattleTransition] Couldn't read the viewport - no held picture over the map change"));
		return;
	}
	// The read gives the viewport's render target, which is not always the viewport's own size.
	const FIntPoint Size = Viewport->GetRenderTargetTextureSizeXY();
	if (Pixels.Num() == 0 || Pixels.Num() != Size.X * Size.Y)
	{
		UE_LOG(LogWoR, Warning, TEXT("[BattleTransition] Viewport read %d pixels for %dx%d - no held picture over the map change"),
			   Pixels.Num(), Size.X, Size.Y);
		return;
	}
	for (FColor &Pixel : Pixels)
	{
		Pixel.A = 255; // the viewport's alpha is not coverage
	}
	UTexture2D *Picture = UTexture2D::CreateTransient(
		Size.X, Size.Y, PF_B8G8R8A8, NAME_None,
		TConstArrayView64<uint8>(reinterpret_cast<const uint8 *>(Pixels.GetData()), Pixels.Num() * sizeof(FColor)));
	if (!Picture)
	{
		return;
	}
	// The brush keeps the texture alive and is owned by the widget, so neither can outlive the other.
	const TSharedRef<FDeferredCleanupSlateBrush> Brush = FDeferredCleanupSlateBrush::CreateBrush(Picture);
	const TWeakObjectPtr<const UBattleTransitionSubsystem> WeakThis(this);
	PictureWidget = SNew(SImage)
						.Image_Lambda([Brush]() { return Brush->GetSlateBrush(); })
						.Visibility(EVisibility::HitTestInvisible)
						.ColorAndOpacity(TAttribute<FSlateColor>::CreateLambda([WeakThis]()
						{
							return FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, WeakThis.IsValid() ? WeakThis->GetPictureAlpha() : 0.0f));
						}));
	ViewportClient->AddViewportWidgetContent(PictureWidget.ToSharedRef(), PICTURE_Z_ORDER);
}

void UBattleTransitionSubsystem::RemovePicture()
{
	if (PictureWidget.IsValid())
	{
		if (UGameViewportClient *ViewportClient = GetGameInstance()->GetGameViewportClient())
		{
			ViewportClient->RemoveViewportWidgetContent(PictureWidget.ToSharedRef());
		}
		PictureWidget.Reset();
	}
}

float UBattleTransitionSubsystem::GetPictureAlpha() const
{
	return Data ? Clock.GetPictureAlpha(*Data) : 0.0f;
}
