// InviteWidget.cpp

#include "UI/Party/InviteWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Online/WoRSessionSubsystem.h"
#include "UI/Party/JoinSeatWidget.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "Invite"

namespace InviteStyle
{
	// TODO: style pass (placeholder until the CommonUI screens).
	const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.7f);
	constexpr int32 FontSize = 22;
	constexpr float RowPadding = 6.0f;
	constexpr int32 ZOrder = 50;
}

namespace InviteBuild
{
	UTextBlock *AddText(UWidgetTree *Tree, UVerticalBox *Box)
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = InviteStyle::FontSize;
		Text->SetFont(Font);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Text))
		{
			BoxSlot->SetPadding(FMargin(InviteStyle::RowPadding));
		}
		return Text;
	}

	UButton *AddButton(UWidgetTree *Tree, UVerticalBox *Box, const FText &Label, UTextBlock **OutLabel = nullptr)
	{
		UButton *Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = InviteStyle::FontSize;
		Text->SetFont(Font);
		Text->SetText(Label);
		Button->AddChild(Text);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Button))
		{
			BoxSlot->SetPadding(FMargin(InviteStyle::RowPadding));
		}
		if (OutLabel)
		{
			*OutLabel = Text;
		}
		return Button;
	}

	UWoRSessionSubsystem *SessionsOf(const UUserWidget *Widget)
	{
		const UGameInstance *GI = Widget ? Widget->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UWoRSessionSubsystem>() : nullptr;
	}
}

UInviteWidget *UInviteWidget::Open(APlayerController *PC)
{
	if (!PC)
	{
		return nullptr;
	}
	UInviteWidget *Widget = CreateWidget<UInviteWidget>(PC, UInviteWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->AddToViewport(InviteStyle::ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	return Widget;
}

TSharedRef<SWidget> UInviteWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(InviteStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;

		UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Overlay->SetContent(Box);

		SessionText = InviteBuild::AddText(WidgetTree, Box);
		FriendsText = InviteBuild::AddText(WidgetTree, Box);
		InviteBuild::AddButton(WidgetTree, Box, LOCTEXT("Previous", "Previous friend"))->OnClicked.AddDynamic(this, &UInviteWidget::HandlePreviousClicked);
		InviteBuild::AddButton(WidgetTree, Box, LOCTEXT("Next", "Next friend"))->OnClicked.AddDynamic(this, &UInviteWidget::HandleNextClicked);
		UTextBlock *InviteText = nullptr;
		InviteButton = InviteBuild::AddButton(WidgetTree, Box, FText::GetEmpty(), &InviteText);
		InviteLabel = InviteText;
		InviteButton->OnClicked.AddDynamic(this, &UInviteWidget::HandleInviteClicked);
		HostButton = InviteBuild::AddButton(WidgetTree, Box, LOCTEXT("Host", "Host a session (this level reopens for friends)"));
		HostButton->OnClicked.AddDynamic(this, &UInviteWidget::HandleHostClicked);
		InviteBuild::AddButton(WidgetTree, Box, LOCTEXT("Seat", "Seat a friend who joined"))->OnClicked.AddDynamic(this, &UInviteWidget::HandleSeatClicked);
		InviteBuild::AddButton(WidgetTree, Box, LOCTEXT("Close", "Close"))->OnClicked.AddDynamic(this, &UInviteWidget::HandleCloseClicked);
	}
	return Super::RebuildWidget();
}

void UInviteWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UWoRSessionSubsystem *Sessions = InviteBuild::SessionsOf(this))
	{
		FriendsReadHandle = Sessions->OnFriendsRead.AddUObject(this, &UInviteWidget::HandleFriendsRead);
		Sessions->ReadFriends(); // Refresh again when it lands (at once on LAN: no friends list)
	}
	Refresh();
}

void UInviteWidget::NativeDestruct()
{
	if (UWoRSessionSubsystem *Sessions = InviteBuild::SessionsOf(this))
	{
		Sessions->OnFriendsRead.Remove(FriendsReadHandle);
	}
	Super::NativeDestruct();
}

void UInviteWidget::Refresh()
{
	const UWoRSessionSubsystem *Sessions = InviteBuild::SessionsOf(this);
	if (!SessionText || !FriendsText || !InviteLabel)
	{
		return;
	}
	if (!Sessions)
	{
		SessionText->SetText(LOCTEXT("NoSessions", "No session service here"));
		return;
	}
	const bool bHosting = Sessions->IsHosting();
	SessionText->SetText(bHosting ? LOCTEXT("Hosting", "You host a session: friends can join")
								  : LOCTEXT("NotHosting", "No session hosted: host one first"));
	HostButton->SetVisibility(bHosting ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);

	const TArray<FString> Names = Sessions->GetFriendNames();
	PickedFriend = Names.IsEmpty() ? 0 : FMath::Clamp(PickedFriend, 0, Names.Num() - 1);
	if (Names.IsEmpty())
	{
		FriendsText->SetText(LOCTEXT("NoFriends", "No friends list here (LAN): a friend on your network uses wor.Session.Find, then wor.Session.Join <n>"));
		InviteLabel->SetText(LOCTEXT("InviteNone", "Invite (no friend picked)"));
		InviteButton->SetIsEnabled(false);
		return;
	}
	FriendsText->SetText(FText::Format(LOCTEXT("FriendCount", "{0} friend(s); picked {1} of {0}"), Names.Num(), PickedFriend + 1));
	InviteLabel->SetText(FText::Format(LOCTEXT("InviteName", "Invite {0}"), FText::FromString(Names[PickedFriend])));
	InviteButton->SetIsEnabled(bHosting);
}

FString UInviteWidget::GetInviteString() const
{
	FString Out = SessionText ? SessionText->GetText().ToString() : FString();
	Out += TEXT("\n") + (FriendsText ? FriendsText->GetText().ToString() : FString());
	Out += TEXT("\n") + (InviteLabel ? InviteLabel->GetText().ToString() : FString());
	return Out;
}

void UInviteWidget::HandlePreviousClicked()
{
	--PickedFriend;
	Refresh();
}

void UInviteWidget::HandleNextClicked()
{
	++PickedFriend;
	Refresh();
}

void UInviteWidget::HandleInviteClicked()
{
	if (UWoRSessionSubsystem *Sessions = InviteBuild::SessionsOf(this))
	{
		Sessions->InviteFriend(PickedFriend);
	}
}

void UInviteWidget::HandleHostClicked()
{
	UWoRSessionSubsystem *Sessions = InviteBuild::SessionsOf(this);
	Close(); // hosting reopens the level, which takes this screen with it
	if (Sessions)
	{
		Sessions->HostSession();
	}
}

void UInviteWidget::HandleSeatClicked()
{
	APlayerController *PC = GetOwningPlayer();
	Close();
	UJoinSeatWidget::Open(PC);
}

void UInviteWidget::HandleCloseClicked()
{
	Close();
}

void UInviteWidget::HandleFriendsRead()
{
	Refresh();
}

void UInviteWidget::Close()
{
	if (APlayerController *PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

static FAutoConsoleCommandWithWorld GInviteUICommand(
	TEXT("wor.UI.Invite"),
	TEXT("PLACEHOLDER UI (B-233b): invite friends to the co-op session (host one first); also on the rest menu."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld *World)
		{
			if (!UInviteWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
			{
				UE_LOG(LogWoR, Warning, TEXT("[Invite] wor.UI.Invite: no player controller"));
			}
		}));

#undef LOCTEXT_NAMESPACE
