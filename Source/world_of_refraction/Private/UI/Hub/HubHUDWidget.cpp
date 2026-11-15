// HubHUDWidget.cpp

#include "UI/Hub/HubHUDWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyWallet.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Field/FieldMovementComponent.h"
#include "Field/FieldSkillTypes.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Player/WoRPlayerState.h"
#include "TimerManager.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRIconTable.h"

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
	/** B-503h: the field attack chip ends the party strip's row, a card like the members' (B-503i). TODO: tune in PIE */
	constexpr float FieldAttackIconSize = 36.0f;
	constexpr float FieldAttackDotSize = 12.0f;
	/** How long the chip's panel stays lit after R / LB cycles it. TODO: tune */
	constexpr float FieldAttackFlashSeconds = 0.35f;
	/** B-503g: the party strip (mockup hub/hud: 48, 640 at 720p, so x1.5 here); bottom left, cards left to right.
	 *  B-503i: a card's width follows its content (WoRHubHUD::PARTY_CARD_MIN_WIDTH to _MAX_WIDTH). */
	constexpr float PartyStripBottom = 54.0f;
	constexpr float PartyCardHeight = 66.0f;
	constexpr float PartyCardPaddingX = 15.0f;
	constexpr float PartyDotSize = 24.0f;
	constexpr float PartyDotGap = 12.0f;
	constexpr float PartyBarHeight = 8.0f;
	/** B-503i: the mockup's card: the top-left corner cut 6 px at 720p, a 1 px outline. */
	constexpr float CardCutSize = 9.0f;
	constexpr float CardOutlineThickness = 1.5f;

	/** A box only as tall as Height; its width follows the slot (a bar that fills its column). */
	inline USizeBox *MakeHeightOnly(UWidgetTree *Tree, float Height, UWidget *Content)
	{
		USizeBox *Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetHeightOverride(Height);
		Box->AddChild(Content);
		return Box;
	}

	/** A white disc of Size; SetColorAndOpacity tints it. */
	inline UImage *MakeDot(UWidgetTree *Tree, float Size)
	{
		UImage *Dot = Tree->ConstructWidget<UImage>(UImage::StaticClass());
		Dot->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, Size * 0.5f, FVector2f(Size, Size)));
		return Dot;
	}
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

	// B-503i: the field attack chip, a card: the icon (or its word), then the name over the element's dot and name.
	UHorizontalBox *FieldAttackRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	FieldAttackIcon = MakeIcon(WidgetTree, FieldAttackIconSize);
	FieldAttackIconText = MakeText(WidgetTree, SmallFontSize, DimColor, true);
	AddToRow(FieldAttackRow, FieldAttackIcon, FMargin(0.0f, 0.0f, PartyDotGap, 0.0f));
	AddToRow(FieldAttackRow, FieldAttackIconText, FMargin(0.0f, 0.0f, PartyDotGap, 0.0f));
	UVerticalBox *FieldAttackColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	FieldAttackName = MakeText(WidgetTree, RowFontSize, DimColor, true);
	FieldAttackName->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	AddToColumn(FieldAttackColumn, FieldAttackName);
	FieldAttackElementRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	FieldAttackDot = MakeDot(WidgetTree, FieldAttackDotSize);
	FieldAttackElement = MakeText(WidgetTree, SmallFontSize, DimColor);
	AddToRow(FieldAttackElementRow, FieldAttackDot, FMargin(0.0f, 0.0f, WordGap, 0.0f));
	AddToRow(FieldAttackElementRow, FieldAttackElement);
	AddToColumn(FieldAttackColumn, FieldAttackElementRow);
	AddToRow(FieldAttackRow, FieldAttackColumn)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UHubHUDCardFrame *ChipFrame = nullptr;
	FieldAttackBox = MakeCard(FieldAttackRow, WoRHubHUD::FIELD_ATTACK_CHIP_MIN_WIDTH, WoRHubHUD::FIELD_ATTACK_CHIP_MAX_WIDTH, ChipFrame);
	FieldAttackFrame = ChipFrame;
	// BuildPartyStrip puts the chip at the end of the strip's row.

	BuildPartyStrip(Canvas);

	SetShown(PromptBox, false);
	SetShown(MoneyBox, false);
	SetShown(QuestBox, false);
	SetShown(FieldAttackBox, false);
}

void UHubHUDWidget::BuildPartyStrip(UCanvasPanel *Canvas)
{
	using namespace HubHUDStyle;

	UHorizontalBox *Strip = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 Index = 0; Index < PartyConstants::MAX_PARTY_MEMBERS; ++Index)
	{
		FPartyCardWidgets &Widgets = PartyCards.AddDefaulted_GetRef();
		UHorizontalBox *Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Widgets.Dot = MakeDot(WidgetTree, PartyDotSize); // the refresh tints it with the member's element
		AddToRow(Row, Widgets.Dot, FMargin(0.0f, 0.0f, PartyDotGap, 0.0f));
		UVerticalBox *Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Widgets.Name = MakeText(WidgetTree, RowFontSize, DimColor, true);
		Widgets.Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis); // a name past the card's max width
		Widgets.HP = MakeBar(WidgetTree, HPColor);
		AddToColumn(Column, Widgets.Name);
		AddToColumn(Column, MakeHeightOnly(WidgetTree, PartyBarHeight, Widgets.HP), FMargin(0.0f, LineGap * 0.5f, 0.0f, 0.0f));
		AddToRow(Row, Column)->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); // the bar spans the card's inner width
		UHubHUDCardFrame *Frame = nullptr;
		Widgets.Card = MakeCard(Row, WoRHubHUD::PARTY_CARD_MIN_WIDTH, WoRHubHUD::PARTY_CARD_MAX_WIDTH, Frame);
		// Each card's gap is on its left, so the first card starts at the strip's edge and hidden cards leave none.
		AddToRow(Strip, Widgets.Card, FMargin(Index > 0 ? WoRHubHUD::PARTY_STRIP_GAP : 0.0f, 0.0f, 0.0f, 0.0f));
		SetShown(Widgets.Card, false);
	}
	AddToRow(Strip, FieldAttackBox, FMargin(WoRHubHUD::PARTY_STRIP_GAP, 0.0f, 0.0f, 0.0f));
	if (UCanvasPanelSlot *StripSlot = Canvas->AddChildToCanvas(Strip))
	{
		StripSlot->SetAnchors(BottomLeft);
		StripSlot->SetAlignment(BottomLeftAlignment);
		StripSlot->SetPosition(FVector2D(ScreenPadding, -PartyStripBottom));
		StripSlot->SetAutoSize(true);
	}
}

UWidget *UHubHUDWidget::MakeCard(UWidget *Content, float MinWidth, float MaxWidth, UHubHUDCardFrame *&OutFrame)
{
	using namespace HubHUDStyle;

	UOverlay *Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	OutFrame = WidgetTree->ConstructWidget<UHubHUDCardFrame>(UHubHUDCardFrame::StaticClass());
	OutFrame->FillColor = PanelColor;
	OutFrame->OutlineColor = OffColor;
	if (UOverlaySlot *FrameSlot = Layers->AddChildToOverlay(OutFrame))
	{
		FrameSlot->SetHorizontalAlignment(HAlign_Fill);
		FrameSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (UOverlaySlot *ContentSlot = Layers->AddChildToOverlay(Content))
	{
		ContentSlot->SetPadding(FMargin(PartyCardPaddingX, 0.0f));
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Center);
	}
	USizeBox *Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetMinDesiredWidth(MinWidth);
	Box->SetMaxDesiredWidth(MaxWidth);
	Box->SetHeightOverride(PartyCardHeight);
	Box->AddChild(Layers);
	return Box;
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

void UHubHUDWidget::RefreshPartyStrip()
{
	const UGameInstance *GameInstance = GetGameInstance();
	const UPartySessionSubsystem *PartySession = GameInstance ? GameInstance->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	const TArray<FHubHUDPartyCard> Cards = WoRHubHUD::BuildPartyStrip(PartySession ? PartySession->GetLocalParty() : nullptr);
	for (int32 Index = 0; Index < PartyCards.Num(); ++Index)
	{
		const FPartyCardWidgets &Widgets = PartyCards[Index];
		const bool bShown = Cards.IsValidIndex(Index);
		WoRMenuStyle::SetShown(Widgets.Card, bShown);
		if (!bShown)
		{
			continue;
		}
		const FHubHUDPartyCard &Card = Cards[Index];
		if (!Card.Name.EqualTo(Widgets.Name->GetText()))
		{
			Widgets.Name->SetText(Card.Name);
		}
		Widgets.Dot->SetColorAndOpacity(Card.DotColor);
		Widgets.HP->SetPercent(Card.HPShare);
	}
}

void UHubHUDWidget::SetFieldAttack(const FFieldAttackEntry &Entry)
{
	const FHubHUDFieldAttackChip Chip = WoRHubHUD::BuildFieldAttackChip(Entry);
	if (!Chip.Name.EqualTo(FieldAttackName->GetText()))
	{
		FieldAttackName->SetText(Chip.Name);
	}
	// The item's own icon, else the icon table's; with neither, the icon in words.
	UTexture2D *Texture = Chip.OwnIcon ? Chip.OwnIcon : WoRIcons::Find(Chip.Icon);
	const bool bIcon = WoRMenuStyle::SetIcon(FieldAttackIcon, Texture, Chip.Icon.Name.IsEmpty() ? Chip.Name : Chip.Icon.Name);
	FieldAttackIconText->SetText(Chip.IconText);
	WoRMenuStyle::SetShown(FieldAttackIconText, !bIcon && !Chip.IconText.IsEmpty());
	FieldAttackElement->SetText(Chip.ElementName);
	FieldAttackDot->SetColorAndOpacity(Chip.ElementColor);
	WoRMenuStyle::SetShown(FieldAttackElementRow, Chip.bElemental);
	WoRMenuStyle::SetShown(FieldAttackBox, true);
}

void UHubHUDWidget::RefreshFieldAttack()
{
	if (!FieldAttackBox)
	{
		return; // headless: no tree
	}
	const APawn *Pawn = GetOwningPlayerPawn();
	const UFieldMovementComponent *FieldMovement = Pawn ? Pawn->FindComponentByClass<UFieldMovementComponent>() : nullptr;
	if (!FieldMovement)
	{
		WoRMenuStyle::SetShown(FieldAttackBox, false);
		return;
	}
	SetFieldAttack(FieldMovement->GetFieldAttackEntry());
}

void UHubHUDWidget::ShowFieldAttack(const FFieldAttackEntry &Entry)
{
	if (!FieldAttackBox)
	{
		return; // headless: no tree
	}
	SetFieldAttack(Entry);
	FieldAttackFrame->SetFillColor(WoRMenuStyle::FocusColor);
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FieldAttackTimer, FTimerDelegate::CreateUObject(this, &UHubHUDWidget::EndFieldAttackFlash),
										  HubHUDStyle::FieldAttackFlashSeconds, /*bLoop=*/false);
	}
}

void UHubHUDWidget::EndFieldAttackFlash()
{
	if (FieldAttackFrame)
	{
		FieldAttackFrame->SetFillColor(WoRMenuStyle::PanelColor);
	}
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
		World->GetTimerManager().ClearTimer(FieldAttackTimer);
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
