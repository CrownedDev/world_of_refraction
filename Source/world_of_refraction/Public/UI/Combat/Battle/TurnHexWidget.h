// TurnHexWidget.h
// B-590 (D-264, mockups I_hex_row_small / H_hex_column_small): one hexagon of the fight's turn order. It only paints:
// the character's face cut to a flat-topped hexagon (BattleHUDView::GetTurnHexCorners), or a plain dark hexagon
// when it has none, and the coloured edge (violet for the current turn, cyan for the party, rose for enemies).
// Its size comes from the USizeBox around it; UBattleHUDWidget builds a fixed pool once and only sets data.
// The face is TurnHexFace::Capture's one-shot picture of the fighter's 3D model (Q-590, Crown 2026-11-23).

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "TurnHexWidget.generated.h"

class AActor;
class UTexture;
class UTextureRenderTarget2D;

namespace TurnHexFace
{
	/** The capture's square size, pixels: twice the current hexagon at 1080p, so it stays sharp. */
	constexpr int32 CAPTURE_SIZE = 168;

	/** One picture of Fighter's head, shoulders and upper chest, looking from ViewFrom's side (where Intel's camera
	 *  stands for it), in the lit scene as Intel shows it, with OthersHidden left out, into a new render target owned
	 *  by Outer. It renders for a moment so the light settles, then stops. Null when it can't be taken: no world, no
	 *  skeletal mesh, or no renderer (headless specs). Call once per fighter per fight. */
	WORLD_OF_REFRACTION_API UTextureRenderTarget2D *Capture(AActor *Fighter, UObject *Outer, const FVector &ViewFrom,
															const TArray<AActor *> &OthersHidden);
}

UCLASS()
class WORLD_OF_REFRACTION_API UTurnHexWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** What to paint; a null Portrait paints the placeholder. Repaints. */
	void SetHex(UTexture *InPortrait, const FLinearColor &InBorder, float InBorderThickness);

	UTexture *GetPortrait() const { return Portrait; }
	const FLinearColor &GetBorder() const { return Border; }

protected:
	virtual int32 NativePaint(const FPaintArgs &Args, const FGeometry &AllottedGeometry, const FSlateRect &MyCullingRect,
							  FSlateWindowElementList &OutDrawElements, int32 LayerId, const FWidgetStyle &InWidgetStyle,
							  bool bParentEnabled) const override;

private:
	/** Held so the texture stays loaded while it is drawn. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture> Portrait;

	/** The portrait as a brush: the paint draws the hexagon's triangles with its texture. */
	FSlateBrush PortraitBrush;
	FLinearColor Border = FLinearColor::White;
	float BorderThickness = 1.0f;
};
