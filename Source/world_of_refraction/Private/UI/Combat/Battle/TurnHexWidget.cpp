// TurnHexWidget.cpp

#include "UI/Combat/Battle/TurnHexWidget.h"

#include "Combat/Camera/CombatCameraDirector.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Actor.h"
#include "Misc/App.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "UI/Combat/Battle/BattleHUDView.h"
#include "WoRLog.h"

namespace TurnHexFaceShot
{
	// TODO: tune in PIE (B-590 QA 3, judged against the mockup I_hex_row_small.png). The UE mannequin's head bone, then VRoid's.
	const FName HeadBones[] = {TEXT("head"), TEXT("J_Bip_C_Head")};
	constexpr float FaceAboveHeadBone = 8.0f;	  // cm: the head bone sits at the skull's base
	constexpr float FaceBelowTop = 0.08f;		  // of the body's height, when there is no head bone
	constexpr float FramedHeightOfBody = 0.32f;	  // the picture's height, of the body's: the head, shoulders and upper chest
	constexpr float MinFramedHeight = 50.0f;	  // cm
	constexpr float AimBelowFace = 0.2f;		  // of the framed height: the face sits in the picture's upper half
	constexpr float FieldOfView = 30.0f;		  // degrees: a portrait lens, little distortion
	/** The capture renders every frame this long, so the scene's lighting and exposure settle (a single frame of a new
	 *  view comes out dark), then stops for good. */
	constexpr float SettleSeconds = 1.0f;
}

UTextureRenderTarget2D *TurnHexFace::Capture(AActor *Fighter, UObject *Outer, const FVector &ViewFrom, const TArray<AActor *> &OthersHidden)
{
	using namespace TurnHexFaceShot;
	UWorld *World = Fighter ? Fighter->GetWorld() : nullptr;
	USkeletalMeshComponent *Mesh = Fighter ? Fighter->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	if (!World || !World->Scene || !Mesh || !Mesh->GetSkeletalMeshAsset() || !FApp::CanEverRender() || !Outer)
	{
		return nullptr;
	}
	FVector Feet;
	float Height = 0.0f;
	UCombatCameraDirector::GetTargetShape(Fighter, Feet, Height); // the body Intel frames
	FVector Face = Feet + FVector::UpVector * Height * (1.0f - FaceBelowTop);
	bool bHeadBone = false;
	for (const FName &Bone : HeadBones)
	{
		if (Mesh->DoesSocketExist(Bone))
		{
			Face = Mesh->GetSocketLocation(Bone) + FVector::UpVector * FaceAboveHeadBone;
			bHeadBone = true;
			break;
		}
	}
	const float Framed = FMath::Max(Height * FramedHeightOfBody, MinFramedHeight);
	const FVector Aim = Face - FVector::UpVector * Framed * AimBelowFace;
	const float Distance = Framed * 0.5f / FMath::Tan(FMath::DegreesToRadians(FieldOfView * 0.5f));
	// Along the line from Intel's camera to the face: the same side and height Intel shows the character from.
	const FVector Fallback = Fighter->GetActorForwardVector().GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FVector Back = (ViewFrom - Aim).GetSafeNormal(UE_SMALL_NUMBER, Fallback);

	UTextureRenderTarget2D *Target = NewObject<UTextureRenderTarget2D>(Outer, MakeUniqueObjectName(Outer, UTextureRenderTarget2D::StaticClass(),
																								   *FString::Printf(TEXT("FaceCapture_%s"), *Fighter->GetName())));
	Target->InitCustomFormat(CAPTURE_SIZE, CAPTURE_SIZE, PF_B8G8R8A8, /*bInForceLinearGamma=*/false);
	Target->UpdateResourceImmediate(true);

	// The whole lit scene, as Intel's view shows it (the arena behind), with only the other fighters left out so none
	// stands in front of the face.
	USceneCaptureComponent2D *Camera = NewObject<USceneCaptureComponent2D>(Fighter);
	Camera->bCaptureEveryFrame = true; // until SettleSeconds
	Camera->bCaptureOnMovement = false;
	Camera->bAlwaysPersistRenderingState = true; // exposure and lighting carry from frame to frame while it settles
	Camera->FOVAngle = FieldOfView;
	Camera->TextureTarget = Target;
	Camera->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	for (AActor *Other : OthersHidden)
	{
		if (Other && Other != Fighter)
		{
			Camera->HiddenActors.Add(Other);
		}
	}
	Camera->ShowFlags.SetMotionBlur(false);
	Camera->SetWorldLocationAndRotation(Aim + Back * Distance, (-Back).Rotation());
	Camera->RegisterComponentWithWorld(World);
	Camera->CaptureScene();
	FTimerHandle Settle;
	World->GetTimerManager().SetTimer(Settle, FTimerDelegate::CreateWeakLambda(Camera, [Camera]()
	{
		Camera->bCaptureEveryFrame = false; // the last settled frame stays in the target
		Camera->DestroyComponent();
	}), SettleSeconds, false);
	UE_LOG(LogWoRUI, Log, TEXT("[TurnHex] Face captured: %s (%s, %s, %.0f cm framed, %.0f cm away)"), *Fighter->GetName(),
		   bHeadBone ? TEXT("head bone") : TEXT("body top"), ViewFrom.IsZero() ? TEXT("facing") : TEXT("from Intel's side"), Framed, Distance);
	return Target;
}

void UTurnHexWidget::SetHex(UTexture *InPortrait, const FLinearColor &InBorder, float InBorderThickness)
{
	if (Portrait != InPortrait)
	{
		Portrait = InPortrait;
		PortraitBrush = FSlateBrush();
		if (Portrait)
		{
			PortraitBrush.SetResourceObject(Portrait);
		}
	}
	Border = InBorder;
	BorderThickness = InBorderThickness;
	InvalidateLayoutAndVolatility();
}

int32 UTurnHexWidget::NativePaint(const FPaintArgs &Args, const FGeometry &AllottedGeometry, const FSlateRect &MyCullingRect,
								  FSlateWindowElementList &OutDrawElements, int32 LayerId, const FWidgetStyle &InWidgetStyle,
								  bool bParentEnabled) const
{
	const FVector2f Box = AllottedGeometry.GetLocalSize();
	const float Size = FMath::Min(Box.X, Box.Y);
	if (Size <= 0.0f || !FSlateApplication::IsInitialized())
	{
		return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
	const TArray<FVector2f> Corners = BattleHUDView::GetTurnHexCorners(Size);

	// The fill: a fan of triangles from the middle, the portrait's texture mapped across the hexagon's box (each corner's
	// UV is its place in the box), or the white brush tinted as the placeholder.
	const bool bPortrait = Portrait != nullptr;
	const FSlateBrush *Fill = bPortrait ? &PortraitBrush : FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	const FColor Colour = ((bPortrait ? FLinearColor::White : BattleHUDView::TURN_HEX_PLACEHOLDER_COLOUR) * Tint).ToFColor(true);
	const FSlateRenderTransform &Transform = AllottedGeometry.GetAccumulatedRenderTransform();
	const FVector2f Middle(Size * 0.5f, Size * 0.5f);
	const auto Uv = [bPortrait, Size](const FVector2f &Point) { return bPortrait ? Point / Size : FVector2f(0.5f, 0.5f); };
	TArray<FSlateVertex> Verts;
	TArray<SlateIndex> Indices;
	Verts.Add(FSlateVertex::Make(Transform, Middle, Uv(Middle), Colour));
	for (const FVector2f &Corner : Corners)
	{
		Verts.Add(FSlateVertex::Make(Transform, Corner, Uv(Corner), Colour));
	}
	for (int32 Index = 0; Index < Corners.Num(); ++Index)
	{
		Indices.Append({0, static_cast<SlateIndex>(Index + 1), static_cast<SlateIndex>((Index + 1) % Corners.Num() + 1)});
	}
	// A scene capture's alpha isn't the picture's opacity (its background can read 0): the face is drawn opaque.
	FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Fill), Verts, Indices,
									   nullptr, 0, 0, bPortrait ? ESlateDrawEffect::IgnoreTextureAlpha : ESlateDrawEffect::None);

	// The edge, inset by half its thickness so it stays inside the box, closed back to the first corner.
	const float Half = BorderThickness * 0.5f;
	TArray<FVector2f> Edge;
	for (const FVector2f &Corner : Corners)
	{
		Edge.Add(Middle + (Corner - Middle) * ((Size * 0.5f - Half) / (Size * 0.5f)));
	}
	const FVector2f First = Edge[0]; // a copy: adding an element of the array being grown asserts
	Edge.Add(First);
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), MoveTemp(Edge), ESlateDrawEffect::None,
								 Border * Tint, /*bAntialias=*/true, BorderThickness);
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 1, InWidgetStyle, bParentEnabled);
}
