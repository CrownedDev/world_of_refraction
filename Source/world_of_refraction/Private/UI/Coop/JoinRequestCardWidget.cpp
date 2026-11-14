// JoinRequestCardWidget.cpp

#include "UI/Coop/JoinRequestCardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/InputPriorities.h"
#include "Player/RunStateComponent.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRJoinRequestCardWidget"

namespace JoinRequestCardStyle
{
	// This card's own layout (mockup 2_join_request, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float CardWidth = 710.0f;
	constexpr float CardPadding = 30.0f;
	constexpr float CornerMargin = 90.0f;
	constexpr float SeatHeight = 80.0f;
	constexpr float SeatPadding = 21.0f;
	/** Under the menus and the host-lost card (100): a corner notice. */
	constexpr int32 ZOrder = 40;
	/** One seat row per party slot that can be an AI seat (never the leader's). */
	constexpr int32 MaxSeats = PartyConstants::MAX_PARTY_MEMBERS - 1;
}

namespace JoinRequestKeys
{
	/** Hold this long to answer (a tap of View stays free for the map, D-063 / InputMap). */
	constexpr float HOLD_SECONDS = 0.5f;
	inline const FName ACTION_NAME(TEXT("IA_Coop_AnswerJoinRequest"));
	inline const FKey KEYBOARD = EKeys::Tab;
	inline const FKey GAMEPAD = EKeys::Gamepad_Special_Left;
}

UJoinRequestCardWidget::UJoinRequestCardWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true); // the keys, once held to answer
}

TSharedRef<SWidget> UJoinRequestCardWidget::RebuildWidget()
{
	using namespace JoinRequestCardStyle;
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Corner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Corner"));
		Corner->SetBrushColor(FLinearColor::Transparent);
		Corner->SetPadding(FMargin(CornerMargin));
		Corner->SetHorizontalAlignment(HAlign_Right);
		Corner->SetVerticalAlignment(VAlign_Top);
		WidgetTree->RootWidget = Corner;

		Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrushColor(PanelColor);
		Panel->SetPadding(FMargin(CardPadding));
		Corner->SetContent(MakeSized(WidgetTree, CardWidth, 0.0f, Panel));
		UVerticalBox *Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Column);

		TitleText = MakeText(WidgetTree, NameFontSize, TextColor, true);
		WithText = MakeText(WidgetTree, LineFontSize, DimColor);
		AddToColumn(Column, TitleText);
		AddToColumn(Column, WithText, FMargin(0.0f, LineGap, 0.0f, 0.0f));
		UTextBlock *SeatsLabel = MakeText(WidgetTree, LineFontSize, DimColor, true);
		SeatsLabel->SetText(LOCTEXT("Seats", "Seats"));
		AddToColumn(Column, SeatsLabel, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
		for (int32 Index = 0; Index < MaxSeats; ++Index)
		{
			FSeatWidgets Row;
			UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Frame->SetBrushColor(PlateColor);
			Frame->SetPadding(FMargin(SeatPadding, 0.0f));
			Frame->SetVerticalAlignment(VAlign_Center);
			UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			Frame->SetContent(Line);
			UTextBlock *Name = MakeText(WidgetTree, RowFontSize, TextColor, true);
			UTextBlock *Status = MakeText(WidgetTree, LineFontSize, DimColor, true);
			AddToRow(Line, Name)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			AddToRow(Line, Status);
			AddToColumn(Column, MakeSized(WidgetTree, 0.0f, SeatHeight, Frame), FMargin(0.0f, LineGap, 0.0f, 0.0f));
			Row.Frame = Frame;
			Row.Name = Name;
			Row.Status = Status;
			SeatWidgets.Add(Row);
		}
		ReplaceText = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
		NoteText = MakeWrappedText(WidgetTree, LineFontSize, WarnColor);
		MessageText = MakeWrappedText(WidgetTree, LineFontSize, WarnColor);
		HintText = MakeWrappedText(WidgetTree, LineFontSize, CyanColor);
		AddToColumn(Column, ReplaceText, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
		AddToColumn(Column, NoteText, FMargin(0.0f, LineGap, 0.0f, 0.0f));
		AddToColumn(Column, MessageText, FMargin(0.0f, LineGap, 0.0f, 0.0f));
		AddToColumn(Column, HintText, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	}
	return Super::RebuildWidget();
}

void UJoinRequestCardWidget::Refresh(const FJoinRequestCard &InCard, bool bInFocused)
{
	if (InCard.PlayerId != Card.PlayerId || !bInFocused)
	{
		Message = FText::GetEmpty(); // a new request, or let go: the last answer's message goes
	}
	Card = InCard;
	bFocused = bInFocused;
	SeatRow = FMath::Clamp(SeatRow, 0, FMath::Max(0, Card.Seats.Num() - 1));
	if (!TitleText)
	{
		return; // headless
	}
	TitleText->SetText(Card.Title);
	WithText->SetText(Card.WithLine);
	for (int32 Index = 0; Index < SeatWidgets.Num(); ++Index)
	{
		const bool bSeat = Card.Seats.IsValidIndex(Index);
		if (UBorder *Frame = SeatWidgets[Index].Frame.Get())
		{
			WoRMenuStyle::SetShown(Frame->GetParent() ? Frame->GetParent() : Frame, bSeat);
		}
		if (bSeat)
		{
			SeatWidgets[Index].Name->SetText(Card.Seats[Index].Name);
			SeatWidgets[Index].Status->SetText(Card.Seats[Index].Status);
		}
	}
	ReplaceText->SetText(Card.ReplaceLine);
	NoteText->SetText(Card.Note);
	WoRMenuStyle::SetShown(NoteText, !Card.Note.IsEmpty());
	HintText->SetText(Card.Hints);
	// Not answering: mouse clicks go through to the game.
	SetVisibility(bFocused ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
	RefreshSelection();
	ShowMessage(Message);
}

void UJoinRequestCardWidget::RefreshSelection()
{
	using namespace JoinRequestCardStyle;
	for (int32 Index = 0; Index < SeatWidgets.Num(); ++Index)
	{
		const bool bLit = bFocused && Index == SeatRow;
		if (UBorder *Frame = SeatWidgets[Index].Frame.Get())
		{
			Frame->SetBrushColor(bLit ? FocusColor : PlateColor);
		}
		if (UTextBlock *Status = SeatWidgets[Index].Status.Get())
		{
			Status->SetColorAndOpacity(FSlateColor(bLit ? CyanColor : DimColor));
		}
	}
}

void UJoinRequestCardWidget::ShowMessage(const FText &InMessage)
{
	Message = InMessage;
	if (MessageText)
	{
		MessageText->SetText(Message);
		WoRMenuStyle::SetShown(MessageText, !Message.IsEmpty());
	}
}

void UJoinRequestCardWidget::MoveSelection(int32 Delta)
{
	if (Card.Seats.Num() > 0)
	{
		SeatRow = FMath::Clamp(SeatRow + Delta, 0, Card.Seats.Num() - 1);
		RefreshSelection();
	}
}

void UJoinRequestCardWidget::PressGive()
{
	if (!Card.Seats.IsValidIndex(SeatRow))
	{
		ShowMessage(LOCTEXT("NoSeat", "No AI seat to give."));
		return;
	}
	// On success the request goes, the run broadcasts and the presenter shows the next one (or hides the card).
	ShowMessage(WoRJoinRequestCard::Give(GetWorld(), Card.PlayerId, Card.Seats[SeatRow].Slot));
}

void UJoinRequestCardWidget::PressRefuse()
{
	ShowMessage(WoRJoinRequestCard::Refuse(GetWorld(), Card.PlayerId));
}

FReply UJoinRequestCardWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (WoRMenuStyle::IsUpKey(Key))
	{
		MoveSelection(-1);
	}
	else if (WoRMenuStyle::IsDownKey(Key))
	{
		MoveSelection(1);
	}
	else if (InKeyEvent.IsRepeat())
	{
		// A held A / X / B answers once.
	}
	else if (WoRMenuStyle::IsConfirmKey(Key))
	{
		PressGive();
	}
	else if (WoRMenuStyle::IsToggleKey(Key))
	{
		PressRefuse();
	}
	else if (WoRMenuStyle::IsBackKey(Key))
	{
		if (OnLetGo)
		{
			OnLetGo();
		}
	}
	return FReply::Handled();
}

bool UJoinRequestCardPresenter::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UJoinRequestCardPresenter::OnWorldBeginPlay(UWorld &InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	URunStateComponent *Found = URunStateComponent::Find(&InWorld);
	if (!Found || !Found->GetOwner() || !Found->GetOwner()->HasAuthority())
	{
		return; // a client, or no run here: only the host answers
	}
	Run = Found;
	RequestsChangedHandle = Found->OnJoinRequestsChanged.AddUObject(this, &UJoinRequestCardPresenter::Sync);
	Sync();
}

void UJoinRequestCardPresenter::Deinitialize()
{
	if (URunStateComponent *Bound = Run.Get())
	{
		Bound->OnJoinRequestsChanged.Remove(RequestsChangedHandle);
	}
	Hide();
	Super::Deinitialize();
}

void UJoinRequestCardPresenter::Sync()
{
	UWorld *World = GetWorld();
	const FJoinRequestCard Next = WoRJoinRequestCard::BuildCard(World, bFocused);
	if (!Next.bShown || !World || World->bIsTearingDown)
	{
		Hide();
		return;
	}
	UJoinRequestCardWidget *Shown = Card.Get();
	if (!Shown || !Shown->IsInViewport())
	{
		APlayerController *PC = World->GetFirstPlayerController();
		if (!PC || !PC->IsLocalController())
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[JoinRequestCard] a join request waits but this host has no local player to show it"));
			return;
		}
		Shown = CreateWidget<UJoinRequestCardWidget>(PC, UJoinRequestCardWidget::StaticClass());
		if (!Shown)
		{
			return;
		}
		Shown->OnLetGo = [WeakThis = TWeakObjectPtr<UJoinRequestCardPresenter>(this)]()
		{
			if (UJoinRequestCardPresenter *Presenter = WeakThis.Get())
			{
				Presenter->LetGo();
			}
		};
		Shown->AddToViewport(JoinRequestCardStyle::ZOrder);
		Card = Shown;
		AddHoldKey(PC);
		UE_LOG(LogWoRUI, Log, TEXT("[JoinRequestCard] Shown: %s"), *Next.Title.ToString());
	}
	Shown->Refresh(Next, bFocused);
}

void UJoinRequestCardPresenter::Focus()
{
	UJoinRequestCardWidget *Shown = Card.Get();
	APlayerController *PC = Shown ? Shown->GetOwningPlayer() : nullptr;
	const UGameInstance *GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	if (bFocused || !PC || (PartySession && PartySession->IsFightRunning()))
	{
		return; // in a fight the card's note says to answer after it
	}
	bFocused = true;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Shown->TakeWidget());
	PC->SetInputMode(InputMode);
	UE_LOG(LogWoRUI, Log, TEXT("[JoinRequestCard] Answering"));
	Sync();
}

void UJoinRequestCardPresenter::LetGo()
{
	if (ReleaseKeys())
	{
		Sync();
	}
}

bool UJoinRequestCardPresenter::ReleaseKeys()
{
	if (!bFocused)
	{
		return false;
	}
	bFocused = false;
	// The card is only focused from play (UI-only input blocks the hold key), so play gets the keys back.
	if (APlayerController *PC = Card.IsValid() ? Card->GetOwningPlayer() : nullptr)
	{
		PC->SetInputMode(FInputModeGameOnly());
	}
	return true;
}

void UJoinRequestCardPresenter::Hide()
{
	ReleaseKeys();
	RemoveHoldKey();
	UJoinRequestCardWidget *Shown = Card.Get();
	Card.Reset();
	if (Shown)
	{
		Shown->RemoveFromParent();
		UE_LOG(LogWoRUI, Log, TEXT("[JoinRequestCard] Hidden"));
	}
}

void UJoinRequestCardPresenter::AddHoldKey(APlayerController *PC)
{
	UEnhancedInputComponent *Input = PC ? Cast<UEnhancedInputComponent>(PC->InputComponent) : nullptr;
	UEnhancedInputLocalPlayerSubsystem *Mappings =
		PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (!Input || !Mappings)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[JoinRequestCard] no Enhanced Input on the host's controller: answer with wor.UI.AnswerJoinRequest"));
		return;
	}
	if (!AnswerAction)
	{
		AnswerAction = NewObject<UInputAction>(this, JoinRequestKeys::ACTION_NAME);
		AnswerAction->ValueType = EInputActionValueType::Boolean;
		AnswerAction->bConsumeInput = false; // a tap still reaches whatever else the key does
		UInputTriggerHold *Hold = NewObject<UInputTriggerHold>(AnswerAction);
		Hold->HoldTimeThreshold = JoinRequestKeys::HOLD_SECONDS;
		Hold->bIsOneShot = true;
		AnswerAction->Triggers.Add(Hold);
		AnswerContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Coop_JoinRequest"));
		AnswerContext->MapKey(AnswerAction, JoinRequestKeys::KEYBOARD);
		AnswerContext->MapKey(AnswerAction, JoinRequestKeys::GAMEPAD);
	}
	HoldBindingHandle = Input->BindAction(AnswerAction, ETriggerEvent::Triggered, this, &UJoinRequestCardPresenter::Focus).GetHandle();
	Mappings->AddMappingContext(AnswerContext, WoRInputPriority::JOIN_REQUEST_CARD);
	KeyOwner = PC;
}

void UJoinRequestCardPresenter::RemoveHoldKey()
{
	APlayerController *PC = KeyOwner.Get();
	KeyOwner.Reset();
	if (!PC)
	{
		return;
	}
	if (UEnhancedInputComponent *Input = Cast<UEnhancedInputComponent>(PC->InputComponent))
	{
		Input->RemoveBindingByHandle(HoldBindingHandle);
	}
	if (UEnhancedInputLocalPlayerSubsystem *Mappings = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		Mappings->RemoveMappingContext(AnswerContext);
	}
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GAnswerJoinRequestCommand(
	TEXT("wor.UI.AnswerJoinRequest"),
	TEXT("B-459c, host: the join request card takes the keys, as holding View (Tab) does."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		UJoinRequestCardPresenter *Presenter = World ? World->GetSubsystem<UJoinRequestCardPresenter>() : nullptr;
		if (!Presenter || !Presenter->GetCard())
		{
			UE_LOG(LogWoRUI, Display, TEXT("[JoinRequestCard] no join request waits here"));
			return;
		}
		Presenter->Focus();
	}));
#endif

#undef LOCTEXT_NAMESPACE
