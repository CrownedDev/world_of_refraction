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
	/** B-503m4 (D-217, mockup hub/hud/v2/3_trial_wheel_open: centre 180, 442 at 720p, so x1.5 here): the RB wheel
	 *  above the strip, bottom left. The most slices: two weapons, five adapters, the innate, the evolution. The
	 *  circle grows past a few slices so the names keep their spacing. B-503m5: one round wheel (the mockup's rim
	 *  125 and middle 54 at 720p). TODO: tune in PIE (Crown's look check) */
	constexpr int32 WheelSlotCount = 9;
	constexpr float WheelMinRadius = 140.0f; // the circle the names sit on
	constexpr float WheelSlotSpacing = 110.0f; // along the circle, between neighbouring names
	constexpr float WheelRimPastNames = 55.0f;
	constexpr float WheelHubRadius = 85.0f;
	const FVector2D WheelCentreFromBottomLeft(270.0f, -420.0f);
	constexpr int32 WheelCentreFontSize = 16;
	constexpr float WheelCentreTextInset = 12.0f; // the middle's text wraps inside its circle
	constexpr int32 WheelCircleSegments = 96;
	constexpr int32 WheelMinSliceSegments = 8;
	constexpr float WheelLineThickness = 1.5f;
	const FLinearColor WheelFillColor = PanelColor.CopyWithNewOpacity(0.92f);
	const FLinearColor WheelLineColor = DimColor.CopyWithNewOpacity(0.6f);
	const FVector2D CentreAlignment(0.5f, 0.5f);
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
	if (!AttackStrips.IsEmpty())
	{
		BuildFieldAttackWheel(Canvas); // on top: built last
	}

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
		WoRHubHUD::BuildPartyStrip(PartySession ? PartySession->GetLocalParty() : nullptr, WoRHubHUD::FindPlayedMemberSlot(GetOwningPlayerPawn()));
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
		return; // headless, or the hub: no strips
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
		return; // headless, the hub, or no card for the pawn
	}
	SetFieldAttack(Entry); // B-503m: no flash (Crown: the text change is enough)
}

void UHubHUDWidget::BuildFieldAttackWheel(UCanvasPanel *Canvas)
{
	using namespace HubHUDStyle;

	const auto AddOnWheel = [Canvas](UWidget *Widget)
	{
		if (UCanvasPanelSlot *CanvasSlot = Canvas->AddChildToCanvas(Widget))
		{
			CanvasSlot->SetAnchors(BottomLeft);
			CanvasSlot->SetAlignment(CentreAlignment);
			CanvasSlot->SetPosition(WheelCentreFromBottomLeft);
			CanvasSlot->SetAutoSize(true);
		}
		SetShown(Widget, false);
	};
	// B-503m5: the round body first, so the names and the middle's text draw on it.
	WheelRing = WidgetTree->ConstructWidget<UHubHUDWheelRing>(UHubHUDWheelRing::StaticClass());
	WheelRing->FillColor = WheelFillColor;
	WheelRing->HubColor = PanelColor;
	WheelRing->LitColor = FocusColor;
	WheelRing->LineColor = WheelLineColor;
	AddOnWheel(WheelRing);
	if (UCanvasPanelSlot *RingSlot = Cast<UCanvasPanelSlot>(WheelRing->Slot))
	{
		RingSlot->SetAutoSize(false); // sized to the slice count when shown
	}
	WheelCentre = MakeText(WidgetTree, WheelCentreFontSize, CyanColor, true);
	WheelCentre->SetJustification(ETextJustify::Center);
	WheelCentre->SetAutoWrapText(true);
	WheelCentre->SetWrapTextAt(2.0f * (WheelHubRadius - WheelCentreTextInset));
	AddOnWheel(WheelCentre);
	for (int32 Index = 0; Index < WheelSlotCount; ++Index)
	{
		UBorder *WheelSlot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		WheelSlot->SetBrushColor(FLinearColor::Transparent); // B-503m5: the names sit on the wheel, not in boxes
		WheelSlot->SetPadding(FMargin(BattleCardStyle::CardPadding));
		UVerticalBox *Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		WheelSlot->SetContent(Column);
		UTextBlock *Diamond = MakeText(WidgetTree, BattleCardStyle::LineFontSize, TextColor);
		Diamond->SetText(FText::AsCultureInvariant(TEXT("◆")));
		Diamond->SetJustification(ETextJustify::Center);
		UTextBlock *Text = MakeText(WidgetTree, BattleCardStyle::LineFontSize, BattleCardStyle::TextColor, true);
		Text->SetJustification(ETextJustify::Center);
		AddToColumn(Column, Diamond);
		AddToColumn(Column, Text);
		AddOnWheel(WheelSlot);
		WheelSlots.Add(WheelSlot);
		WheelTexts.Add(Text);
		WheelDiamonds.Add(Diamond);
	}
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

bool UHubHUDWidget::ShowFieldAttackWheel(const TArray<FFieldAttackEntry> &Entries, int32 Picked)
{
	using namespace HubHUDStyle;

	if (WheelSlots.IsEmpty() || Entries.IsEmpty())
	{
		return false; // the hub, headless, or nothing to pick
	}
	const TArray<int32> Slices = FieldSkillRules::GetFieldSliceStarts(Entries);
	if (Slices.Num() > WheelSlots.Num())
	{
		UE_LOG(LogWoR, Warning, TEXT("[HubHUD] The field attack wheel has %d slices, room for %d: the rest aren't shown"),
			   Slices.Num(), WheelSlots.Num());
	}
	const int32 Kept = FieldSkillRules::ClampFieldAttackIndex(Picked, Entries.Num());
	const int32 PickedSlice = FieldSkillRules::GetFieldSliceOf(Entries, Kept);
	const int32 Shown = FMath::Min(Slices.Num(), WheelSlots.Num());
	const float Radius = FMath::Max(WheelMinRadius, Shown * WheelSlotSpacing / UE_TWO_PI);
	const float Rim = Radius + WheelRimPastNames;
	if (UCanvasPanelSlot *RingSlot = Cast<UCanvasPanelSlot>(WheelRing->Slot))
	{
		RingSlot->SetSize(FVector2D(2.0f * Rim));
	}
	WheelRing->SetWheel(Shown, PickedSlice, WheelHubRadius);
	SetShown(WheelRing, true);
	for (int32 Index = 0; Index < WheelSlots.Num(); ++Index)
	{
		const bool bShown = Index < Shown;
		SetShown(WheelSlots[Index], bShown);
		if (!bShown)
		{
			continue;
		}
		const float Turn = UE_TWO_PI * Index / Shown; // from the top, clockwise; the screen's Y is down
		if (UCanvasPanelSlot *CanvasSlot = Cast<UCanvasPanelSlot>(WheelSlots[Index]->Slot))
		{
			CanvasSlot->SetPosition(WheelCentreFromBottomLeft + FVector2D(Radius * FMath::Sin(Turn), -Radius * FMath::Cos(Turn)));
		}
		// The picked slice shows the picked option's element; the others their first option's (D-217 mockup).
		const FFieldAttackEntry &Option = Entries[Index == PickedSlice ? Kept : Slices[Index]];
		WheelTexts[Index]->SetText(Option.SliceName);
		const FLinearColor Element = Option.bIce ? ElementColors::Ice : ElementColors::GetColorForElement(Option.Element);
		WheelDiamonds[Index]->SetColorAndOpacity(FSlateColor(Element));
		WheelTexts[Index]->SetColorAndOpacity(FSlateColor(Index == PickedSlice ? TextColor : DimColor));
	}
	const FFieldAttackEntry &Pick = Entries[Kept];
	const int32 SliceEnd = Slices.IsValidIndex(PickedSlice + 1) ? Slices[PickedSlice + 1] : Entries.Num();
	const bool bHasOptions = SliceEnd - Slices[PickedSlice] > 1;
	// B-503m5 (Crown): only what the strip will read ("Sword", "Fire Sword"); an unarmed plain swing reads as its option.
	const FText Result = Pick.Name.IsEmpty() ? Pick.OptionName : Pick.Name;
	WheelCentre->SetText(bHasOptions ? FText::Format(LOCTEXT("WheelResultArrows", "◄  {0}  ►"), Result) : Result);
	SetShown(WheelCentre, true);
	// The wheel's keys for the device in use (B-481). TODO: key caps (WoRMenuStyle::MakeHintBar) with the real HUD.
	WheelHints->SetText(WoRKeyHints::IsGamepad(this)
							? LOCTEXT("WheelHintsToggle", "L-stick  Pick        D-pad ◄ ►  Element        A  Confirm        B / RB  Close")
							: LOCTEXT("WheelHintsToggleKeys", "Mouse  Pick        A / D, wheel  Element        Click  Confirm        Backspace / R  Close"));
	SetShown(WheelHints, true);
	return true;
}

void UHubHUDWidget::HideFieldAttackWheel()
{
	for (UBorder *WheelSlot : WheelSlots)
	{
		WoRMenuStyle::SetShown(WheelSlot, false);
	}
	for (UWidget *Part : {static_cast<UWidget *>(WheelRing), static_cast<UWidget *>(WheelCentre), static_cast<UWidget *>(WheelHints)})
	{
		if (Part)
		{
			WoRMenuStyle::SetShown(Part, false);
		}
	}
}

int32 UHubHUDWidget::GetFieldWheelSliceUnderCursor(bool &bOnWheel) const
{
	bOnWheel = false;
	if (!WheelRing || !WheelRing->IsVisible() || !FSlateApplication::IsInitialized())
	{
		return INDEX_NONE;
	}
	const FGeometry &Geometry = WheelRing->GetCachedGeometry();
	const FVector2D Size(Geometry.GetLocalSize());
	const FVector2D FromCentre = FVector2D(Geometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos())) - Size * 0.5;
	const float Rim = FMath::Min(Size.X, Size.Y) * 0.5f;
	bOnWheel = Rim > 0.0f && FromCentre.Size() <= Rim;
	return FieldSkillRules::GetFieldWheelSliceAt(FromCentre, HubHUDStyle::WheelHubRadius, Rim, WheelRing->GetSliceCount());
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
