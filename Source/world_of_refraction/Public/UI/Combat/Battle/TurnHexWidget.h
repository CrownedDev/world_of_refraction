// TurnHexWidget.h
// B-590 (D-264, mockups I_hex_row_small / H_hex_column_small): one hexagon of the fight's turn order. It only paints:
// the character's portrait cut to a flat-topped hexagon (BattleHUDView::GetTurnHexCorners), or a plain dark hexagon
// when it has no portrait, and the coloured edge (violet for the current turn, cyan for the party, rose for enemies).
// Its size comes from the USizeBox around it; UBattleHUDWidget builds a fixed pool once and only sets data.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "TurnHexWidget.generated.h"

class UTexture2D;

UCLASS()
class WORLD_OF_REFRACTION_API UTurnHexWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** What to paint; a null Portrait paints the placeholder. Repaints. */
	void SetHex(UTexture2D *InPortrait, const FLinearColor &InBorder, float InBorderThickness);

	UTexture2D *GetPortrait() const { return Portrait; }
	const FLinearColor &GetBorder() const { return Border; }

protected:
	virtual int32 NativePaint(const FPaintArgs &Args, const FGeometry &AllottedGeometry, const FSlateRect &MyCullingRect,
							  FSlateWindowElementList &OutDrawElements, int32 LayerId, const FWidgetStyle &InWidgetStyle,
							  bool bParentEnabled) const override;

private:
	/** Held so the texture stays loaded while it is drawn. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Portrait;

	/** The portrait as a brush: the paint draws the hexagon's triangles with its texture. */
	FSlateBrush PortraitBrush;
	FLinearColor Border = FLinearColor::White;
	float BorderThickness = 1.0f;
};
