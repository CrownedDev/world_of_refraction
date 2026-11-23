// TurnHexWidget.cpp

#include "UI/Combat/Battle/TurnHexWidget.h"

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
#include "UI/Combat/Battle/BattleHUDView.h"
#include "WoRLog.h"

namespace TurnHexFaceShot
{
	// TODO: tune in PIE (B-590 QA). The UE mannequin's head bone, then VRoid's.
	const FName HeadBones[] = {TEXT("head"), TEXT("J_Bip_C_Head")};
	constexpr float FaceAboveHeadBone = 8.0f; // cm: the head bone sits at the skull's base
	constexpr float FaceBelowMeshTop = 0.08f; // of the mesh's height, when there is no head bone
	constexpr float FramedHeightOfBody = 0.17f; // the picture's height, of the mesh's height: the head and a little neck
	constexpr float MinFramedHeight = 28.0f;  // cm
	constexpr float FieldOfView = 30.0f;	  // degrees
}

UTextureRenderTarget2D *TurnHexFace::Capture(AActor *Fighter, UObject *Outer)
{
	using namespace TurnHexFaceShot;
	UWorld *World = Fighter ? Fighter->GetWorld() : nullptr;
	USkeletalMeshComponent *Mesh = Fighter ? Fighter->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	if (!World || !World->Scene || !Mesh || !Mesh->GetSkeletalMeshAsset() || !FApp::CanEverRender() || !Outer)
	{
		return nullptr;
	}
	const FBoxSphereBounds Bounds = Mesh->Bounds;
	const float Height = static_cast<float>(Bounds.BoxExtent.Z) * 2.0f;
	FVector Face = Bounds.Origin + FVector::UpVector * (Bounds.BoxExtent.Z - Height * FaceBelowMeshTop);
	for (const FName &Bone : HeadBones)
	{
		if (Mesh->DoesSocketExist(Bone))
		{
			Face = Mesh->GetSocketLocation(Bone) + FVector::UpVector * FaceAboveHeadBone;
			break;
		}
	}
	const float Framed = FMath::Max(Height * FramedHeightOfBody, MinFramedHeight);
	const float Distance = Framed * 0.5f / FMath::Tan(FMath::DegreesToRadians(FieldOfView * 0.5f));
	const FVector Forward = Fighter->GetActorForwardVector().GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);

	UTextureRenderTarget2D *Target = NewObject<UTextureRenderTarget2D>(Outer, MakeUniqueObjectName(Outer, UTextureRenderTarget2D::StaticClass(),
																								   *FString::Printf(TEXT("FaceCapture_%s"), *Fighter->GetName())));
	Target->InitCustomFormat(CAPTURE_SIZE, CAPTURE_SIZE, PF_B8G8R8A8, /*bInForceLinearGamma=*/false);
	Target->UpdateResourceImmediate(true);

	USceneCaptureComponent2D *Camera = NewObject<USceneCaptureComponent2D>(Fighter);
	Camera->bCaptureEveryFrame = false;
	Camera->bCaptureOnMovement = false;
	Camera->FOVAngle = FieldOfView;
	Camera->TextureTarget = Target;
	Camera->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Camera->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Camera->ShowOnlyActorComponents(Fighter);
	Camera->ShowFlags.SetMotionBlur(false);
	Camera->ShowFlags.SetFog(false);
	Camera->ShowFlags.SetAtmosphere(false);
	Camera->SetWorldLocationAndRotation(Face + Forward * Distance, (-Forward).Rotation());
	Camera->RegisterComponentWithWorld(World);
	Camera->CaptureScene();
	Camera->DestroyComponent();
	UE_LOG(LogWoRUI, Log, TEXT("[TurnHex] Face captured: %s (%s, %.0f cm framed)"), *Fighter->GetName(),
		   Mesh->DoesSocketExist(HeadBones[0]) || Mesh->DoesSocketExist(HeadBones[1]) ? TEXT("head bone") : TEXT("mesh top"), Framed);
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
