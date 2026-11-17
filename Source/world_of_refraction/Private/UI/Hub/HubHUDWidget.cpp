// HubHUDWidget.cpp

#include "UI/Hub/HubHUDWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyWallet.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Field/FieldMovementComponent.h"
#include "Field/FieldSkillTypes.h"
#include "Infusion/ElementColors.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Player/WoRPlayerState.h"
#include "TimerManager.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/Menus/PauseMenuModel.h"
#include "UI/WoRIconTable.h"
#include "UI/WoRKeyHintText.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRHubHUDWidget"

namespace HubHUDStyle
{
	// This HUD's own layout (screen_mockups/hub/hud, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float RefreshSeconds = 0.1f;
	constexpr float ScreenMargin = 60.0f;
	constexpr float ChipPaddingX = 24.0f;
	constexpr float ChipPaddingY = 12.0f;
	constexpr float KeyPaddingX = 12.0f;
	// The prompt's bottom centre sits on its world spot; the money chip's top-right corner on the screen's.
	const FVector2D PromptAlignment(0.5f, 1.0f);
	const FAnchors TopLeft(0.0f, 0.0f);
	const FAnchors TopRight(1.0f, 0.0f);
	const FVector2D TopRightAlignment(1.0f, 0.0f);
	/** B-463b: the tracked quest's line sits under the money chip's place, top right. TODO: tune with the trial HUD (B-465) */
	constexpr float QuestLineTop = 135.0f;
	constexpr float QuestLineWidth = 540.0f;
	/** A step changes on an event, not every frame: a second is soon enough. */
	constexpr float QuestRefreshSeconds = 1.0f;
	const FAnchors BottomLeft(0.0f, 1.0f);
	const FVector2D BottomLeftAlignment(0.0f, 1.0f);
	/** Between the strip and its card. TODO: tune in PIE */
	constexpr float AttackStripGap = 3.0f;
	/** B-503g: the party strip (mockup hub/hud: 48, 640 at 720p, so x1.5 here); bottom left, cards left to right.
	 *  B-503j: the cards are the battle HUD's (BattleCardStyle). */
	constexpr float PartyStripBottom = 54.0f;
	/** B-503i: the mockup's card: the top-left corner cut 6 px at 720p, a 1 px outline. */
	constexpr float CardCutSize = 9.0f;
	constexpr float CardOutlineThickness = 1.5f;
	/** B-503m5: the old wheel's paint (UHubHUDWheelRing, OBSOLETE since B-514). */
	constexpr int32 WheelCircleSegments = 96;
	constexpr int32 WheelMinSliceSegments = 8;
	constexpr float WheelLineThickness = 1.5f;
	/** B-514 (D-222, mockup hub/hud/v2/3_trial_strip_open, 1280x720, so x1.5 here): the quick strip through the
	 *  screen's middle. The most pieces: two weapons, five adapters, the innate, the evolution.
	 *  TODO: tune in PIE (Crown's look check) */
	constexpr int32 StripTileCount = 9;
	constexpr float StripTileSize = 132.0f;
	constexpr float StripPickedTileSize = 144.0f; // the picked tile grows a little
	constexpr float StripTileGap = 24.0f;
	constexpr float StripIconSize = 96.0f;
	constexpr float StripPairIconSize = 60.0f; // a pair: both icons on one tile
	constexpr float StripFrameThickness = 1.5f;
	constexpr float StripDoubleFrameGap = 3.0f;
	constexpr float StripTileTextInset = 9.0f;
	constexpr float StripTagBottom = 6.0f;
	constexpr float StripTagPaddingX = 9.0f;
	constexpr float StripTextGap = 6.0f; // between the row's down arrows and the name under it
	constexpr float StripTextPaddingX = 72.0f;
	constexpr float StripTextPaddingY = 9.0f;
	const FVector2D StripTopFromCentre(0.0f, -160.0f); // the up arrows' top: the row sits a little above the middle
	const FAnchors ScreenCentre(0.5f, 0.5f);
	const FAnchors FullScreen(0.0f, 0.0f, 1.0f, 1.0f);
	const FVector2D TopCentreAlignment(0.5f, 0.0f);
	const FLinearColor StripDimColor(0.0f, 0.0f, 0.0f, 0.45f);
	const FLinearColor StripTileFillColor = PanelColor.CopyWithNewOpacity(0.92f);
	const FLinearColor StripTextFillColor = PanelColor.CopyWithNewOpacity(0.85f);
	const FLinearColor StripFrameColor = DimColor.CopyWithNewOpacity(0.6f);
	const FAnchors BottomRight(1.0f, 1.0f);
	const FVector2D BottomRightAlignment(1.0f, 1.0f);
}

TSharedRef<SWidget> UHubHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UHubHUDWidget::BuildTree()
{
	using namespace HubHUDStyle;

	UCanvasPanel *Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas"));
	Canvas->SetVisibility(ESlateVisibility::HitTestInvisible); // the HUD never takes a click
	WidgetTree->RootWidget = Canvas;

	PromptBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	PromptBox->SetBrushColor(PanelColor);
	PromptBox->SetPadding(FMargin(ChipPaddingX, ChipPaddingY));
	UHorizontalBox *PromptRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	PromptBox->SetContent(PromptRow);
	UBorder *KeyChip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	KeyChip->SetBrushColor(FocusColor);
	KeyChip->SetPadding(FMargin(KeyPaddingX, 0.0f));
	PromptKey = MakeText(WidgetTree, LineFontSize, TextColor, true);
	KeyChip->SetContent(PromptKey);
	PromptVerb = MakeText(WidgetTree, RowFontSize, TextColor, true);
	PromptWhat = MakeText(WidgetTree, RowFontSize, DimColor);
	AddToRow(PromptRow, KeyChip, FMargin(0.0f, 0.0f, SectionGap, 0.0f));
	AddToRow(PromptRow, PromptVerb, FMargin(0.0f, 0.0f, WordGap, 0.0f));
	AddToRow(PromptRow, PromptWhat);
	if (UCanvasPanelSlot *PromptSlot = Canvas->AddChildToCanvas(PromptBox))
	{
		PromptSlot->SetAnchors(TopLeft);
		PromptSlot->SetAlignment(PromptAlignment);
		PromptSlot->SetAutoSize(true);
	}

	MoneyBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	MoneyBox->SetBrushColor(PanelColor);
	MoneyBox->SetPadding(FMargin(ChipPaddingX, ChipPaddingY));
	UHorizontalBox *MoneyRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	MoneyBox->SetContent(MoneyRow);
	MoneyValue = MakeText(WidgetTree, RowFontSize, TextColor, true);
	MoneyLabel = MakeText(WidgetTree, LineFontSize, DimColor);
	AddToRow(MoneyRow, MoneyValue, FMargin(0.0f, 0.0f, WordGap, 0.0f));
	AddToRow(MoneyRow, MoneyLabel);
	if (UCanvasPanelSlot *MoneySlot = Canvas->AddChildToCanvas(MoneyBox))
	{
		MoneySlot->SetAnchors(TopRight);
		MoneySlot->SetAlignment(TopRightAlignment);
		MoneySlot->SetPosition(FVector2D(-ScreenMargin, ScreenMargin));
		MoneySlot->SetAutoSize(true);
	}

	QuestBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	QuestBox->SetBrushColor(PanelColor);
	QuestBox->SetPadding(FMargin(ChipPaddingX, ChipPaddingY));
	QuestText = MakeWrappedText(WidgetTree, LineFontSize, TextColor);
	QuestBox->SetContent(MakeSized(WidgetTree, QuestLineWidth, 0.0f, QuestText));
	if (UCanvasPanelSlot *QuestSlot = Canvas->AddChildToCanvas(QuestBox))
	{
		QuestSlot->SetAnchors(TopRight);
		QuestSlot->SetAlignment(TopRightAlignment);
		QuestSlot->SetPosition(FVector2D(-ScreenMargin, QuestLineTop));
		QuestSlot->SetAutoSize(true);
	}

	BuildPartyStrip(Canvas);
	BuildFieldAttackWheel(Canvas); // on top: built last. B-528 (D-218): the hub too; only the card strip stays a trial's

	SetShown(PromptBox, false);
	SetShown(MoneyBox, false);
	SetShown(QuestBox, false);
}

void UHubHUDWidget::BuildPartyStrip(UCanvasPanel *Canvas)
{
	using namespace HubHUDStyle;

	// B-503j (D-216): the battle HUD's card, at the map's level (B-503m: hub map or not). Each map makes a new HUD,
	// so the level is read once here.
	const EBattleCardLevel Level = WoRHubHUD::GetCardLevel(GetWorld());
	const bool bAttackStrips = WoRHubHUD::ShowsFieldAttackStrip(Level);
	UHorizontalBox *Strip = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 Index = 0; Index < PartyConstants::MAX_PARTY_MEMBERS; ++Index)
	{
		FBattleHUDCard &Card = PartyCards.AddDefaulted_GetRef();
		Card.bParty = true;
		UVerticalBox *Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (bAttackStrips)
		{
			// B-503k (D-217): the field attack strip, directly above the card, in the card's look.
			UBorder *AttackStrip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			AttackStrip->SetBrushColor(BattleCardStyle::CardColor);
			AttackStrip->SetPadding(FMargin(BattleCardStyle::CardPadding));
			UTextBlock *AttackText = MakeText(WidgetTree, BattleCardStyle::LineFontSize, BattleCardStyle::TextColor, true);
			AttackStrip->SetContent(AttackText);
			AddToColumn(Column, AttackStrip, FMargin(0.0f, 0.0f, 0.0f, AttackStripGap));
			SetShown(AttackStrip, false);
			AttackStrips.Add(AttackStrip);
			AttackStripTexts.Add(AttackText);
		}
		AddToColumn(Column, BattleHUDCard::Build(WidgetTree, Card, Level));
		// Each column's gap is on its left, so the first card starts at the strip's edge and hidden cards leave none.
		// Bottom: the cards sit on one line, each its own height (a hub card fits its name).
		AddToRow(Strip, Column, FMargin(Index > 0 ? WoRHubHUD::PARTY_STRIP_GAP : 0.0f, 0.0f, 0.0f, 0.0f), VAlign_Bottom);
		SetShown(Column, false);
		PartyColumns.Add(Column);
	}
	if (UCanvasPanelSlot *StripSlot = Canvas->AddChildToCanvas(Strip))
	{
		StripSlot->SetAnchors(BottomLeft);
		StripSlot->SetAlignment(BottomLeftAlignment);
		StripSlot->SetPosition(FVector2D(ScreenPadding, -PartyStripBottom));
		StripSlot->SetAutoSize(true);
	}
}

int32 UHubHUDCardFrame::NativePaint(const FPaintArgs &Args, const FGeometry &AllottedGeometry, const FSlateRect &MyCullingRect,
									FSlateWindowElementList &OutDrawElements, int32 LayerId, const FWidgetStyle &InWidgetStyle,
									bool bParentEnabled) const
{
	using namespace HubHUDStyle;

	const FVector2f Size = AllottedGeometry.GetLocalSize();
	const float Cut = FMath::Min3(CardCutSize, Size.X, Size.Y);
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
	const FSlateBrush *White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	// The fill: the body under the cut, then one-unit rows stepping in towards the cut corner.
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
							   AllottedGeometry.ToPaintGeometry(FVector2f(Size.X, Size.Y - Cut), FSlateLayoutTransform(FVector2f(0.0f, Cut))),
							   White, ESlateDrawEffect::None, FillColor * Tint);
	for (int32 Row = 0; Row < FMath::CeilToInt(Cut); ++Row)
	{
		const float Inset = Cut - Row;
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
								   AllottedGeometry.ToPaintGeometry(FVector2f(Size.X - Inset, 1.0f), FSlateLayoutTransform(FVector2f(Inset, static_cast<float>(Row)))),
								   White, ESlateDrawEffect::None, FillColor * Tint);
	}
	// The outline, inset by half its thickness so it stays inside the card.
	const float Half = CardOutlineThickness * 0.5f;
	TArray<FVector2f> Outline = {{Cut, Half}, {Size.X - Half, Half}, {Size.X - Half, Size.Y - Half}, {Half, Size.Y - Half}, {Half, Cut}, {Cut, Half}};
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), MoveTemp(Outline), ESlateDrawEffect::None,
								 OutlineColor * Tint, /*bAntialias=*/true, CardOutlineThickness);
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 1, InWidgetStyle, bParentEnabled);
}

namespace HubHUDWheel
{
	/** A point Radius out from Centre at Turn (from the top, clockwise; the screen's Y is down). */
	FVector2f OnCircle(const FVector2f &Centre, float Radius, float Turn)
	{
		return Centre + FVector2f(FMath::Sin(Turn), -FMath::Cos(Turn)) * Radius;
	}

	/** A piece of ring between two turns, as triangles (InnerRadius 0: a pie piece). */
	void AddRingPiece(TArray<FSlateVertex> &Verts, TArray<SlateIndex> &Indices, const FSlateRenderTransform &Transform, const FVector2f &Centre,
					  float InnerRadius, float OuterRadius, float FromTurn, float ToTurn, int32 Segments, const FColor &Colour)
	{
		const FVector2f WhiteTexel(0.5f, 0.5f);
		for (int32 Step = 0; Step <= Segments; ++Step)
		{
			const float Turn = FMath::Lerp(FromTurn, ToTurn, static_cast<float>(Step) / Segments);
			const SlateIndex Inner = static_cast<SlateIndex>(Verts.Num());
			Verts.Add(FSlateVertex::Make(Transform, OnCircle(Centre, InnerRadius, Turn), WhiteTexel, Colour));
			Verts.Add(FSlateVertex::Make(Transform, OnCircle(Centre, OuterRadius, Turn), WhiteTexel, Colour));
			if (Step > 0)
			{
				Indices.Append({static_cast<SlateIndex>(Inner - 2), static_cast<SlateIndex>(Inner - 1), Inner,
								static_cast<SlateIndex>(Inner - 1), static_cast<SlateIndex>(Inner + 1), Inner});
			}
		}
	}

	TArray<FVector2f> MakeCircle(const FVector2f &Centre, float Radius)
	{
		TArray<FVector2f> Points;
		for (int32 Step = 0; Step <= HubHUDStyle::WheelCircleSegments; ++Step)
		{
			Points.Add(OnCircle(Centre, Radius, UE_TWO_PI * Step / HubHUDStyle::WheelCircleSegments));
		}
		return Points;
	}
}

int32 UHubHUDWheelRing::NativePaint(const FPaintArgs &Args, const FGeometry &AllottedGeometry, const FSlateRect &MyCullingRect,
									FSlateWindowElementList &OutDrawElements, int32 LayerId, const FWidgetStyle &InWidgetStyle,
									bool bParentEnabled) const
{
	using namespace HubHUDStyle;
	using namespace HubHUDWheel;

	const FVector2f Size = AllottedGeometry.GetLocalSize();
	const FVector2f Centre = Size * 0.5f;
	const float Rim = FMath::Min(Size.X, Size.Y) * 0.5f - WheelLineThickness;
	if (Rim <= HubRadius || SliceCount <= 0 || !FSlateApplication::IsInitialized())
	{
		return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
	const FSlateRenderTransform &Transform = AllottedGeometry.GetAccumulatedRenderTransform();
	const float Slice = UE_TWO_PI / SliceCount;
	// The disc, the picked slice over it, then the middle: one draw, later triangles on top.
	TArray<FSlateVertex> Verts;
	TArray<SlateIndex> Indices;
	AddRingPiece(Verts, Indices, Transform, Centre, 0.0f, Rim, 0.0f, UE_TWO_PI, WheelCircleSegments, (FillColor * Tint).ToFColor(true));
	if (PickedSlice >= 0 && PickedSlice < SliceCount)
	{
		const float Turn = Slice * PickedSlice;
		AddRingPiece(Verts, Indices, Transform, Centre, HubRadius, Rim, Turn - Slice * 0.5f, Turn + Slice * 0.5f,
					 FMath::Max(WheelMinSliceSegments, WheelCircleSegments / SliceCount), (LitColor * Tint).ToFColor(true));
	}
	AddRingPiece(Verts, Indices, Transform, Centre, 0.0f, HubRadius, 0.0f, UE_TWO_PI, WheelCircleSegments, (HubColor * Tint).ToFColor(true));
	const FSlateBrush *White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, FSlateApplication::Get().GetRenderer()->GetResourceHandle(*White), Verts, Indices,
									   nullptr, 0, 0);
	// The rim, the middle's edge, and a line between each two slices.
	const FLinearColor Line = LineColor * Tint;
	for (const float Radius : {Rim, HubRadius})
	{
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), MakeCircle(Centre, Radius),
									 ESlateDrawEffect::None, Line, /*bAntialias=*/true, WheelLineThickness);
	}
	for (int32 Index = 0; SliceCount > 1 && Index < SliceCount; ++Index)
	{
		const float Between = Slice * (Index + 0.5f);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(),
									 TArray<FVector2f>({OnCircle(Centre, HubRadius, Between), OnCircle(Centre, Rim, Between)}), ESlateDrawEffect::None, Line,
									 /*bAntialias=*/true, WheelLineThickness);
	}
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 1, InWidgetStyle, bParentEnabled);
}

void UHubHUDWidget::RefreshPartyStrip()
{
	const UGameInstance *GameInstance = GetGameInstance();
	const UPartySessionSubsystem *PartySession = GameInstance ? GameInstance->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	const TArray<FBattleHUDCard> Cards =
		WoRHubHUD::BuildPartyStrip(PartySession ? PartySession->GetLocalParty() : nullptr, WoRHubHUD::FindPlayedMemberSlot(GetOwningPlayerPawn()), GetWorld());
	for (int32 Index = 0; Index < PartyCards.Num(); ++Index)
	{
		FBattleHUDCard &Card = PartyCards[Index];
		const bool bShown = Cards.IsValidIndex(Index);
		WoRMenuStyle::SetShown(PartyColumns[Index], bShown);
		if (!bShown)
		{
			continue;
		}
		// The model's data into the card that holds the widgets (built once).
		const FBattleHUDCard &Data = Cards[Index];
		Card.Name = Data.Name;
		Card.DetailLine = Data.DetailLine; // D-229
		Card.bAlive = Data.bAlive;
		Card.HPFraction = Data.HPFraction;
		Card.HPLine = Data.HPLine;
		Card.EPFraction = Data.EPFraction;
		Card.EPLine = Data.EPLine;
		Card.BuildupFraction = Data.BuildupFraction;
		Card.CracksShown = Data.CracksShown;
		Card.BuildupColour = Data.BuildupColour;
		BattleHUDCard::Push(Card);
	}
}

void UHubHUDWidget::SetFieldAttack(const FFieldAttackEntry &Entry)
{
	for (int32 Index = 0; Index < AttackStrips.Num(); ++Index)
	{
		const bool bShown = Index == AttackStripCard && !Entry.Name.IsEmpty(); // unarmed and plain: nothing to name
		WoRMenuStyle::SetShown(AttackStrips[Index], bShown);
		if (bShown && !Entry.Name.EqualTo(AttackStripTexts[Index]->GetText()))
		{
			AttackStripTexts[Index]->SetText(Entry.Name);
		}
	}
}

void UHubHUDWidget::RefreshFieldAttack()
{
	if (AttackStrips.IsEmpty())
	{
		return; // headless, or the hub: no card strips
	}
	const APawn *Pawn = GetOwningPlayerPawn();
	const UFieldMovementComponent *FieldMovement = Pawn ? Pawn->FindComponentByClass<UFieldMovementComponent>() : nullptr;
	AttackStripCard = FieldMovement ? WoRHubHUD::FindFieldAttackCard(Pawn) : INDEX_NONE;
	SetFieldAttack(FieldMovement ? FieldMovement->GetFieldAttackEntry() : FFieldAttackEntry());
}

void UHubHUDWidget::ShowFieldAttack(const FFieldAttackEntry &Entry)
{
	if (!AttackStrips.IsValidIndex(AttackStripCard))
	{
		return; // headless, the hub (no card strips), or no card for the pawn
	}
	SetFieldAttack(Entry); // B-503m: no flash (Crown: the text change is enough)
}

void UHubHUDWidget::BuildFieldAttackWheel(UCanvasPanel *Canvas)
{
	using namespace HubHUDStyle;

	// B-514 (D-222): the world dims behind the open strip; first in the canvas, so the cards stay bright on top.
	UBorder *Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Dim->SetBrushColor(StripDimColor);
	if (UCanvasPanelSlot *DimSlot = Cast<UCanvasPanelSlot>(Canvas->InsertChildAt(0, Dim)))
	{
		DimSlot->SetAnchors(FullScreen);
		DimSlot->SetOffsets(FMargin(0.0f));
	}
	SetShown(Dim, false);
	StripDim = Dim;

	const auto MakeBorder = [this](UWidget *Content)
	{
		UBorder *Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Border->SetPadding(FMargin(0.0f));
		Border->SetContent(Content);
		return Border;
	};
	const auto MakeArrow = [this](const TCHAR *Arrow)
	{
		UTextBlock *Text = MakeText(WidgetTree, LineFontSize, CyanColor);
		Text->SetText(FText::AsCultureInvariant(Arrow));
		Text->SetJustification(ETextJustify::Center);
		return Text;
	};
	UHorizontalBox *Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 Index = 0; Index < StripTileCount; ++Index)
	{
		// The tile's face: the icon (a pair's two side by side), the name when there is no icon, the IN HAND tag.
		UOverlay *Face = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UHorizontalBox *Icons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UImage *Icon = MakeIcon(WidgetTree, StripIconSize);
		UImage *OffHandIcon = MakeIcon(WidgetTree, StripPairIconSize);
		UImage *IconCentre = nullptr;
		AddToRow(Icons, MakeIconStack(WidgetTree, Icon, StripIconSize, IconCentre)); // B-530: an adapter's crystal
		AddToRow(Icons, OffHandIcon);
		UOverlaySlot *IconsSlot = Face->AddChildToOverlay(Icons);
		IconsSlot->SetHorizontalAlignment(HAlign_Center);
		IconsSlot->SetVerticalAlignment(VAlign_Center);
		UTextBlock *Name = MakeWrappedText(WidgetTree, SmallFontSize, TextColor, true);
		Name->SetJustification(ETextJustify::Center);
		UOverlaySlot *NameSlot = Face->AddChildToOverlay(Name);
		NameSlot->SetHorizontalAlignment(HAlign_Center);
		NameSlot->SetVerticalAlignment(VAlign_Center);
		NameSlot->SetPadding(FMargin(StripTileTextInset));
		UTextBlock *TagText = MakeText(WidgetTree, SmallFontSize, CyanColor, true);
		TagText->SetText(LOCTEXT("StripInHand", "IN HAND"));
		UBorder *Tag = MakeBorder(TagText);
		Tag->SetBrushColor(PanelColor);
		Tag->SetPadding(FMargin(StripTagPaddingX, 0.0f));
		UBorder *TagFrame = MakeBorder(Tag);
		TagFrame->SetBrushColor(FocusColor);
		TagFrame->SetPadding(FMargin(StripFrameThickness));
		UOverlaySlot *TagSlot = Face->AddChildToOverlay(TagFrame);
		TagSlot->SetHorizontalAlignment(HAlign_Center);
		TagSlot->SetVerticalAlignment(VAlign_Bottom);
		TagSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, StripTagBottom));
		// Inside out: the fill, its size, then the picked tile's double frame (ring, gap, outer frame).
		UBorder *Fill = MakeBorder(Face);
		USizeBox *Size = MakeSized(WidgetTree, StripTileSize, StripTileSize, Fill);
		UBorder *Ring = MakeBorder(Size);
		UBorder *Gap = MakeBorder(Ring);
		UBorder *Tile = MakeBorder(Gap);
		UTextBlock *Up = MakeArrow(TEXT("▲"));
		UTextBlock *Down = MakeArrow(TEXT("▼"));
		UVerticalBox *Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		for (UWidget *Part : {static_cast<UWidget *>(Up), static_cast<UWidget *>(Tile), static_cast<UWidget *>(Down)})
		{
			AddToColumn(Column, Part)->SetHorizontalAlignment(HAlign_Center);
		}
		AddToRow(Row, Column, FMargin(Index > 0 ? StripTileGap : 0.0f, 0.0f, 0.0f, 0.0f));
		SetShown(Column, false);
		StripTiles.Add(Tile);
		StripTileGaps.Add(Gap);
		StripTileRings.Add(Ring);
		StripTileSizes.Add(Size);
		StripTileFills.Add(Fill);
		StripIcons.Add(Icon);
		StripIconCentres.Add(IconCentre);
		StripOffHandIcons.Add(OffHandIcon);
		StripTileNames.Add(Name);
		StripInHandTags.Add(TagFrame);
		StripUpArrows.Add(Up);
		StripDownArrows.Add(Down);
	}
	// Under the row: the piece's name; quieter, its type and source. D-222 "The text and the frame": one frame
	// around the icons and the text, a single panel.
	StripName = MakeText(WidgetTree, NameFontSize, TextColor, true);
	StripName->SetJustification(ETextJustify::Center);
	StripReading = MakeText(WidgetTree, LineFontSize, DimColor);
	StripReading->SetJustification(ETextJustify::Center);
	UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddToColumn(Box, Row)->SetHorizontalAlignment(HAlign_Center);
	AddToColumn(Box, StripName, FMargin(0.0f, StripTextGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Center);
	AddToColumn(Box, StripReading)->SetHorizontalAlignment(HAlign_Center);
	UBorder *Panel = MakeBorder(Box);
	Panel->SetBrushColor(StripTextFillColor);
	Panel->SetPadding(FMargin(StripTextPaddingX, StripTextPaddingY));
	UBorder *Frame = MakeBorder(Panel);
	Frame->SetBrushColor(StripFrameColor);
	Frame->SetPadding(FMargin(StripFrameThickness));
	if (UCanvasPanelSlot *FrameSlot = Canvas->AddChildToCanvas(Frame))
	{
		FrameSlot->SetAnchors(ScreenCentre);
		FrameSlot->SetAlignment(TopCentreAlignment);
		FrameSlot->SetPosition(StripTopFromCentre);
		FrameSlot->SetAutoSize(true);
	}
	SetShown(Frame, false);
	StripBox = Frame;

	WheelHints = MakeText(WidgetTree, LineFontSize, DimColor);
	if (UCanvasPanelSlot *HintSlot = Canvas->AddChildToCanvas(WheelHints))
	{
		HintSlot->SetAnchors(BottomRight);
		HintSlot->SetAlignment(BottomRightAlignment);
		HintSlot->SetPosition(FVector2D(-ScreenMargin, -PartyStripBottom));
		HintSlot->SetAutoSize(true);
	}
	SetShown(WheelHints, false);
}

bool UHubHUDWidget::ShowFieldAttackWheel(const TArray<FFieldAttackEntry> &Entries, int32 Picked, int32 InHand)
{
	using namespace HubHUDStyle;

	if (StripTiles.IsEmpty() || Entries.IsEmpty())
	{
		return false; // not built yet, or nothing to pick
	}
	const TArray<int32> Slices = FieldSkillRules::GetFieldSliceStarts(Entries);
	if (Slices.Num() > StripTiles.Num())
	{
		UE_LOG(LogWoR, Warning, TEXT("[HubHUD] The quick strip has %d pieces, room for %d: the rest aren't shown"),
			   Slices.Num(), StripTiles.Num());
	}
	const int32 Kept = FieldSkillRules::ClampFieldAttackIndex(Picked, Entries.Num());
	const int32 PickedSlice = FieldSkillRules::GetFieldSliceOf(Entries, Kept);
	const int32 InHandSlice = Entries.IsValidIndex(InHand) ? FieldSkillRules::GetFieldSliceOf(Entries, InHand) : INDEX_NONE;
	const int32 Shown = FMath::Min(Slices.Num(), StripTiles.Num());
	for (int32 Index = 0; Index < StripTiles.Num(); ++Index)
	{
		const bool bShown = Index < Shown;
		SetShown(StripTiles[Index]->GetParent(), bShown);
		SetShown(StripTiles[Index], bShown);
		if (!bShown)
		{
			continue;
		}
		// The tile: an adapter's socket frame with its crystal inside (B-530, D-229), else the piece's own icon, else (a
		// weapon) its type's, else (the innate, an evolution) its element's badge, else its name.
		const FFieldAttackEntry &Piece = Entries[Slices[Index]];
		const FWoRIcon AdapterIcon = Piece.bSliceAdapter ? WoRIcons::MakeAdapterIcon(Piece.Element, Piece.SliceName) : FWoRIcon();
		UTexture2D *Icon = Piece.bSliceAdapter ? WoRIcons::Find(AdapterIcon) : Piece.SliceIcon.Get();
		if (!Icon && Piece.WeaponId.IsValid())
		{
			Icon = WoRIcons::GetWeaponType(Piece.WeaponType);
		}
		if (!Icon && (Piece.Kind == EFieldAttackKind::Innate || Piece.Kind == EFieldAttackKind::Evolution))
		{
			Icon = WoRIcons::GetElementBadge(Piece.Element); // no asset of its own (the mockup's evolution tile)
		}
		UTexture2D *OffHandIcon = Icon ? Piece.SliceOffHandIcon.Get() : nullptr;
		StripIcons[Index]->SetDesiredSizeOverride(FVector2D(OffHandIcon ? StripPairIconSize : StripIconSize));
		const bool bIcon = SetIcon(StripIcons[Index], Icon, Piece.SliceName);
		SetIcon(StripIconCentres[Index], bIcon ? WoRIcons::FindCentre(AdapterIcon) : nullptr, Piece.SliceName);
		SetIcon(StripOffHandIcons[Index], OffHandIcon, Piece.SliceName);
		StripTileNames[Index]->SetText(Piece.SliceName);
		SetShown(StripTileNames[Index], !bIcon);
		SetShown(StripInHandTags[Index], Index == InHandSlice);
		// The picked tile: framed twice, a little bigger, with up / down arrows when it has more than one source.
		const bool bPicked = Index == PickedSlice;
		const int32 SliceEnd = Slices.IsValidIndex(Index + 1) ? Slices[Index + 1] : Entries.Num();
		const ESlateVisibility Arrows = bPicked && SliceEnd - Slices[Index] > 1 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden;
		StripUpArrows[Index]->SetVisibility(Arrows);
		StripDownArrows[Index]->SetVisibility(Arrows);
		StripTiles[Index]->SetBrushColor(bPicked ? CyanColor : StripFrameColor);
		StripTiles[Index]->SetPadding(FMargin(StripFrameThickness));
		StripTileGaps[Index]->SetBrushColor(bPicked ? PanelColor : FLinearColor::Transparent);
		StripTileGaps[Index]->SetPadding(FMargin(bPicked ? StripDoubleFrameGap : 0.0f));
		StripTileRings[Index]->SetBrushColor(bPicked ? CyanColor : FLinearColor::Transparent);
		StripTileRings[Index]->SetPadding(FMargin(bPicked ? StripFrameThickness : 0.0f));
		const float TileSize = bPicked ? StripPickedTileSize : StripTileSize;
		StripTileSizes[Index]->SetWidthOverride(TileSize);
		StripTileSizes[Index]->SetHeightOverride(TileSize);
		StripTileFills[Index]->SetBrushColor(bPicked ? PlateColor : StripTileFillColor);
	}
	// D-222 "The text and the frame": "Storm Ring", then "Ring · Lightning".
	const FFieldAttackEntry &Pick = Entries[Kept];
	StripName->SetText(Pick.SliceName);
	const FText TypeLine = FieldSkillRules::GetFieldStripTypeLine(Pick);
	StripReading->SetText(TypeLine);
	SetShown(StripReading, !TypeLine.IsEmpty());
	SetShown(StripBox, true);
	SetShown(StripDim, true);
	// The strip's keys for the device in use (B-481). TODO: key caps (WoRMenuStyle::MakeHintBar) with the real HUD.
	WheelHints->SetText(WoRKeyHints::IsGamepad(this)
							? LOCTEXT("StripHintsPad", "◄ ►  Piece        ▲ ▼  Source        A  Select        B / RB  Close")
							: LOCTEXT("StripHintsKeys", "A / D, wheel  Piece        W / S  Source        Enter  Select        Backspace / R  Close"));
	SetShown(WheelHints, true);
	return true;
}

void UHubHUDWidget::HideFieldAttackWheel()
{
	for (UWidget *Part : {StripBox.Get(), StripDim.Get(), static_cast<UWidget *>(WheelHints)})
	{
		WoRMenuStyle::SetShown(Part, false); // null-safe
	}
}

int32 UHubHUDWidget::GetFieldWheelSliceUnderCursor(bool &bOnWheel) const
{
	bOnWheel = false;
	if (!StripBox || !StripBox->IsVisible() || !FSlateApplication::IsInitialized())
	{
		return INDEX_NONE;
	}
	const FVector2D CursorAt = FSlateApplication::Get().GetCursorPos();
	for (int32 Index = 0; Index < StripTiles.Num(); ++Index)
	{
		const UBorder *Tile = StripTiles[Index];
		if (Tile && Tile->IsVisible() && Tile->GetCachedGeometry().IsUnderLocation(CursorAt))
		{
			bOnWheel = true;
			return Index;
		}
	}
	return INDEX_NONE;
}

void UHubHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible); // the HUD never takes a click
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimer, FTimerDelegate::CreateUObject(this, &UHubHUDWidget::Refresh),
										  HubHUDStyle::RefreshSeconds, /*bLoop=*/true);
		World->GetTimerManager().SetTimer(QuestTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			RefreshQuestLine();
			RefreshFieldAttack();
		}),
										  HubHUDStyle::QuestRefreshSeconds, /*bLoop=*/true);
	}
	Refresh();
	RefreshQuestLine();
	RefreshFieldAttack();
}

void UHubHUDWidget::NativeDestruct()
{
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
		World->GetTimerManager().ClearTimer(QuestTimer);
	}
	if (UCurrencyComponent *Wallet = BoundWallet.Get())
	{
		Wallet->OnCurrencyChanged.RemoveDynamic(this, &UHubHUDWidget::HandleCurrencyChanged);
	}
	BoundWallet.Reset();
	Super::NativeDestruct();
}

void UHubHUDWidget::BindWallet()
{
	// As the game menu reads it: the pawn's wallet, else the player state's.
	UCurrencyComponent *Wallet = CurrencyWallet::FindFor(GetOwningPlayerPawn());
	if (!Wallet)
	{
		const AWoRPlayerState *Player = GetOwningPlayerState<AWoRPlayerState>();
		Wallet = Player ? Player->GetWallet() : nullptr;
	}
	if (Wallet == BoundWallet.Get())
	{
		return;
	}
	// The wallet in use changed (the HUD is made before the pawn is possessed): follow it.
	if (UCurrencyComponent *Old = BoundWallet.Get())
	{
		Old->OnCurrencyChanged.RemoveDynamic(this, &UHubHUDWidget::HandleCurrencyChanged);
	}
	if (Wallet)
	{
		Wallet->OnCurrencyChanged.AddUniqueDynamic(this, &UHubHUDWidget::HandleCurrencyChanged);
	}
	BoundWallet = Wallet;
}

void UHubHUDWidget::HandleCurrencyChanged(ECurrencyType Currency, uint8 /*SubKey*/, int32 /*NewBalance*/)
{
	const UWorld *World = GetWorld();
	if (World && Currency == WoRHubHUD::BuildMoney(World).Currency)
	{
		MoneyChangedAtSeconds = World->GetTimeSeconds();
		Refresh();
	}
}

void UHubHUDWidget::Refresh()
{
	using namespace HubHUDStyle;

	const UWorld *World = GetWorld();
	BindWallet();
	Prompt = WoRHubHUD::BuildPrompt(GetOwningPlayerPawn());
	const float SinceChange = World && MoneyChangedAtSeconds >= 0.0f ? World->GetTimeSeconds() - MoneyChangedAtSeconds : -1.0f;
	bMoneyShown = WoRHubHUD::ShouldShowMoney(Prompt.bNearVendor, SinceChange);

	if (!PromptBox || !MoneyBox)
	{
		return; // headless: no tree
	}
	RefreshPartyStrip();
	SetShown(MoneyBox, bMoneyShown);
	if (bMoneyShown)
	{
		const FHubHUDMoney Money = WoRHubHUD::BuildMoney(World);
		MoneyValue->SetText(Money.Value);
		MoneyLabel->SetText(Money.Label);
	}
	if (Prompt.bShown)
	{
		PromptKey->SetText(WoRHubHUD::GetInteractKeyLabel(WoRKeyHints::IsGamepad(this))); // B-481: the shared device check
		PromptVerb->SetText(Prompt.Verb);
		PromptWhat->SetText(Prompt.What);
		SetShown(PromptWhat, !Prompt.What.IsEmpty());
	}
	PlacePrompt();
}

void UHubHUDWidget::RefreshQuestLine()
{
	if (!QuestBox || !QuestText)
	{
		return; // headless: no tree
	}
	const FText Quest = WoRHubHUD::BuildQuestLine(GetWorld());
	WoRMenuStyle::SetShown(QuestBox, !Quest.IsEmpty());
	if (!Quest.EqualTo(QuestText->GetText()))
	{
		QuestText->SetText(Quest);
	}
}

void UHubHUDWidget::PlacePrompt()
{
	if (!PromptBox)
	{
		return;
	}
	FVector2D ScreenPosition;
	// False when the spot is behind the camera: no prompt then.
	const bool bOnScreen = Prompt.bShown && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
												 GetOwningPlayer(), Prompt.WorldLocation, ScreenPosition, /*bPlayerViewportRelative=*/false);
	WoRMenuStyle::SetShown(PromptBox, bOnScreen);
	if (UCanvasPanelSlot *PromptSlot = bOnScreen ? Cast<UCanvasPanelSlot>(PromptBox->Slot) : nullptr)
	{
		PromptSlot->SetPosition(ScreenPosition);
	}
}

void UHubHUDWidget::NativeTick(const FGeometry &MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Only this: keep a shown prompt over its world spot as the camera moves (see the header).
	if (Prompt.bShown)
	{
		PlacePrompt();
	}
}

#undef LOCTEXT_NAMESPACE
