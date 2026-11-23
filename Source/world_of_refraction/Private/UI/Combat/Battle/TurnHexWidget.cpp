// TurnHexWidget.cpp

#include "UI/Combat/Battle/TurnHexWidget.h"

#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "UI/Combat/Battle/BattleHUDView.h"

void UTurnHexWidget::SetHex(UTexture2D *InPortrait, const FLinearColor &InBorder, float InBorderThickness)
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
	FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Fill), Verts, Indices,
									   nullptr, 0, 0);

	// The edge, inset by half its thickness so it stays inside the box, closed back to the first corner.
	const float Half = BorderThickness * 0.5f;
	TArray<FVector2f> Edge;
	for (const FVector2f &Corner : Corners)
	{
		Edge.Add(Middle + (Corner - Middle) * ((Size * 0.5f - Half) / (Size * 0.5f)));
	}
	Edge.Add(Edge[0]);
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), MoveTemp(Edge), ESlateDrawEffect::None,
								 Border * Tint, /*bAntialias=*/true, BorderThickness);
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 1, InWidgetStyle, bParentEnabled);
}
