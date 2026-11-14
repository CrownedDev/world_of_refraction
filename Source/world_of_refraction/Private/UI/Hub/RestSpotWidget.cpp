// RestSpotWidget.cpp

#include "UI/Hub/RestSpotWidget.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Player/WoRGameState.h"

#include "Player/WoRPlayerState.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Trial/RunRewards.h"
#include "Trial/TrialRestSpot.h"
#include "Trial/TrialRunSubsystem.h"
#include "Online/HostSuccession.h"
#include "UI/Coop/HandOverConfirmWidget.h"
#include "UI/Coop/InviteScreenWidget.h"
#include "UI/Hub/InventoryWidget.h"
#include "UI/Hub/LevelDropWidget.h"
#include "UI/Party/InviteWidget.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "RestSpotWidget"

namespace RestSpotStyle
{
	// TODO: style pass (placeholder until the CommonUI screens).
	const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.7f);
	constexpr int32 FontSize = 22;
	constexpr float RowPadding = 6.0f;
	constexpr int32 ZOrder = 50;

	UTextBlock *AddText(UWidgetTree *Tree, UVerticalBox *Box)
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetAutoWrapText(true);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Text))
		{
			BoxSlot->SetPadding(FMargin(RowPadding));
		}
		return Text;
	}

	UButton *AddButton(UWidgetTree *Tree, UVerticalBox *Box, const FText &Label)
	{
		UButton *Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetText(Label);
		Button->AddChild(Text);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Button))
		{
			BoxSlot->SetPadding(FMargin(RowPadding));
		}
		return Button;
	}
}

URestSpotWidget *URestSpotWidget::Open(APlayerController *PC)
{
	if (!PC)
	{
		return nullptr;
	}
	URestSpotWidget *Widget = CreateWidget<URestSpotWidget>(PC, URestSpotWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->AddToViewport(RestSpotStyle::ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	return Widget;
}

TSharedRef<SWidget> URestSpotWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(RestSpotStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;

		UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Overlay->SetContent(Box);

		RestSpotStyle::AddText(WidgetTree, Box)->SetText(LOCTEXT("Rested", "Rest spot: the party is restored."));
		PayoutText = RestSpotStyle::AddText(WidgetTree, Box);
		UButton *LevelUpButton = RestSpotStyle::AddButton(WidgetTree, Box, LOCTEXT("LevelUp", "Level up"));
		LevelUpButton->OnClicked.AddDynamic(this, &URestSpotWidget::HandleLevelUpClicked); // B-363
		// Merge B-363 + B-187e: a client's level drop reads its own (empty) run, so it would only say "No run".
		// TODO: a server RPC so a client places its own pillar levels at the host's rest spot.
		const bool bMenuOnClient = GetWorld() && GetWorld()->GetNetMode() == NM_Client;
		LevelUpButton->SetVisibility(bMenuOnClient ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		RestSpotStyle::AddButton(WidgetTree, Box, LOCTEXT("Inventory", "Inventory"))
			->OnClicked.AddDynamic(this, &URestSpotWidget::HandleInventoryClicked);
		ReviveButton = RestSpotStyle::AddButton(WidgetTree, Box, LOCTEXT("Revive", "Revive the mini-boss"));
		ReviveButton->OnClicked.AddDynamic(this, &URestSpotWidget::HandleReviveClicked);
		ReviveLabel = Cast<UTextBlock>(ReviveButton->GetChildAt(0));
		VoteText = RestSpotStyle::AddText(WidgetTree, Box);
		VoteYesButton = RestSpotStyle::AddButton(WidgetTree, Box, LOCTEXT("VoteYes", "Vote yes: revive the mini-boss"));
		VoteYesButton->OnClicked.AddDynamic(this, &URestSpotWidget::HandleVoteYesClicked);
		VoteNoButton = RestSpotStyle::AddButton(WidgetTree, Box, LOCTEXT("VoteNo", "Vote no"));
		VoteNoButton->OnClicked.AddDynamic(this, &URestSpotWidget::HandleVoteNoClicked);
		HandOverButton = RestSpotStyle::AddButton(WidgetTree, Box, LOCTEXT("HandOver", "Hand over host (the next player hosts; you rejoin)"));
		HandOverButton->OnClicked.AddDynamic(this, &URestSpotWidget::HandleHandOverClicked);
		InviteButton = RestSpotStyle::AddButton(WidgetTree, Box, LOCTEXT("Invite", "Invite and seat friends"));
		InviteButton->OnClicked.AddDynamic(this, &URestSpotWidget::HandleInviteClicked);
		LeaveButton = RestSpotStyle::AddButton(WidgetTree, Box, LOCTEXT("Leave", "Leave the trial"));
		LeaveButton->OnClicked.AddDynamic(this, &URestSpotWidget::HandleLeaveClicked);
		ConfirmText = RestSpotStyle::AddText(WidgetTree, Box);
		ConfirmButton = RestSpotStyle::AddButton(WidgetTree, Box, LOCTEXT("ConfirmLeave", "Confirm: leave"));
		ConfirmButton->OnClicked.AddDynamic(this, &URestSpotWidget::HandleConfirmLeaveClicked);
		RestSpotStyle::AddButton(WidgetTree, Box, LOCTEXT("Close", "Close"))->OnClicked.AddDynamic(this, &URestSpotWidget::HandleCloseClicked);
	}
	return Super::RebuildWidget();
}

void URestSpotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (AWoRGameState *GameState = GetWorld() ? GetWorld()->GetGameState<AWoRGameState>() : nullptr)
	{
		GameState->OnReviveVoteChanged.AddUniqueDynamic(this, &URestSpotWidget::HandleReviveVoteChanged);
	}
	Refresh();
}

void URestSpotWidget::NativeDestruct()
{
	if (AWoRGameState *GameState = GetWorld() ? GetWorld()->GetGameState<AWoRGameState>() : nullptr)
	{
		GameState->OnReviveVoteChanged.RemoveDynamic(this, &URestSpotWidget::HandleReviveVoteChanged);
	}
	Super::NativeDestruct();
}

void URestSpotWidget::HandleReviveVoteChanged()
{
	Refresh();
}

void URestSpotWidget::HandleVoteYesClicked()
{
	URestSpotWidget::CastReviveVoteFor(GetOwningPlayer(), true);
}

void URestSpotWidget::HandleVoteNoClicked()
{
	URestSpotWidget::CastReviveVoteFor(GetOwningPlayer(), false);
}

void URestSpotWidget::Refresh()
{
	const UGameInstance *GI = GetGameInstance();
	const UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	// B-187d / B-187e: on a client the run lives on the host: no earnings here; Leave leaves the session (Q-187e).
	const bool bOnClient = GetWorld() && GetWorld()->GetNetMode() == NM_Client;
	if (UTextBlock *LeaveLabel = LeaveButton ? Cast<UTextBlock>(LeaveButton->GetChildAt(0)) : nullptr)
	{
		LeaveLabel->SetText(bOnClient ? LOCTEXT("LeaveSession", "Leave the session") : LOCTEXT("Leave", "Leave the trial"));
	}
	if (PayoutText)
	{
		PayoutText->SetVisibility(bOnClient ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		const FRunPayout Earned = Run ? Run->GetRunEndPayout() : FRunPayout();
		PayoutText->SetText(FText::Format(LOCTEXT("Earned", "Earned so far: {0} Gold, {1} Gear Essence, {2} Skill Essence (leaving now pays {3}%)."),
										  FText::AsNumber(Earned.HubGold), FText::AsNumber(Earned.GearEssence), FText::AsNumber(Earned.SkillEssence),
										  FText::AsNumber(FMath::RoundToInt(RunRewards::LEAVE_EARLY_SHARE * 100.0f))));
	}
	if (ConfirmText)
	{
		const FText Confirm = bOnClient ? LOCTEXT("LeaveSessionConfirm", "Leave your friends' run? You go back to your hub; this run's rewards stay with them.")
										: WoRRestSpot::GetLeaveConfirmText();
		ConfirmText->SetText(bConfirmingLeave ? Confirm : FText::GetEmpty());
	}
	if (ConfirmButton)
	{
		ConfirmButton->SetVisibility(bConfirmingLeave ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (HandOverButton)
	{
		// B-234c: only a host with another player can hand over.
		const UWoRHostHandOverSubsystem *HandOver = GI ? GI->GetSubsystem<UWoRHostHandOverSubsystem>() : nullptr;
		const UWorld *World = GetWorld();
		const bool bCanHandOver = HandOver && World && WoRHostSuccession::CanHandOverFrom(World->GetNetMode(), HandOver->GetKnownOrder().Num());
		HandOverButton->SetVisibility(bCanHandOver ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (InviteButton)
	{
		// B-233b: the host (or a solo player, who can host from the invite screen) invites; a client can't.
		const UWorld *World = GetWorld();
		InviteButton->SetVisibility(World && World->GetNetMode() != NM_Client ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	// B-187b: only at a mini-boss's rest spot that can still be revived.
	// B-187e: a client shows what the host said (SetHostRevive).
	const bool bCanRevive = bOnClient ? bHostCanRevive : WoRRestSpot::CanReviveHere(GetWorld());
	// B-187e (Q-187e changed): while a vote runs, Revive gives way to the vote's line and this player's answers.
	const AWoRGameState *GameState = GetWorld() ? GetWorld()->GetGameState<AWoRGameState>() : nullptr;
	const FReviveVote *Vote = GameState ? &GameState->GetReviveVote() : nullptr;
	const APlayerState *Me = GetOwningPlayer() ? GetOwningPlayer()->PlayerState.Get() : nullptr;
	const bool bVoteOpen = Vote && Vote->bOpen;
	const bool bMyTurn = bVoteOpen && Me && ReviveVote::IsWaitingFor(*Vote, Me->GetPlayerId());
	if (ReviveButton)
	{
		ReviveButton->SetVisibility(bCanRevive && !bVoteOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (VoteText)
	{
		VoteText->SetVisibility(bVoteOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bVoteOpen)
		{
			VoteText->SetText(FText::Format(LOCTEXT("VoteLine", "Revive vote: {0} of {1} said yes (everyone outside a fight must agree)"),
											FText::AsNumber(Vote->YesIds.Num()), FText::AsNumber(Vote->VoterIds.Num())));
		}
	}
	for (UButton *Answer : {VoteYesButton.Get(), VoteNoButton.Get()})
	{
		if (Answer)
		{
			Answer->SetVisibility(bMyTurn ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
	const int32 RevivalNumber = bOnClient ? HostRevivalNumber : WoRRestSpot::GetNextRevivalNumber(GetWorld());
	if (ReviveLabel && bCanRevive)
	{
		ReviveLabel->SetText(FText::Format(LOCTEXT("ReviveCount", "Revive the mini-boss: one tier stronger, better drops ({0} of {1})"),
										   FText::AsNumber(RevivalNumber), FText::AsNumber(MiniBossRevival::MAX_REVIVALS)));
	}
}

void URestSpotWidget::SetHostRevive(bool bCanRevive, int32 RevivalNumber)
{
	bHostCanRevive = bCanRevive;
	HostRevivalNumber = RevivalNumber;
	Refresh();
}

void URestSpotWidget::HandleReviveClicked()
{
	// B-187e (Q-187e changed by Crown, 2026-11-08): Revive starts a vote of every player outside battle (solo: it
	// revives at once). A client asks the host (ServerReviveMiniBoss checks reach and fights); the host starts it here.
	UWorld *World = GetWorld();
	APlayerController *PC = GetOwningPlayer();
	AWoRPlayerState *Player = PC ? PC->GetPlayerState<AWoRPlayerState>() : nullptr;
	if (World && World->GetNetMode() == NM_Client)
	{
		if (Player)
		{
			Player->ServerReviveMiniBoss();
		}
		Refresh();
		return;
	}
	AWoRGameState *GameState = World ? World->GetGameState<AWoRGameState>() : nullptr;
	if (!GameState || !Player)
	{
		CloseMenu();
		WoRRestSpot::ReviveMiniBoss(World); // no game state (a test level): as before
		return;
	}
	if (GameState->StartReviveVote(Player) == EReviveVoteResult::Passed)
	{
		CloseMenu(); // solo, or nobody else outside battle: the level reloads (after the fight, if one runs: Q-187e2)
		return;
	}
	Refresh();
}

void URestSpotWidget::CastReviveVoteFor(APlayerController *PC, bool bYes)
{
	AWoRPlayerState *Player = PC ? PC->GetPlayerState<AWoRPlayerState>() : nullptr;
	UWorld *World = PC ? PC->GetWorld() : nullptr;
	if (!Player || !World)
	{
		return;
	}
	if (World->GetNetMode() == NM_Client)
	{
		Player->ServerCastReviveVote(bYes);
	}
	else if (AWoRGameState *GameState = World->GetGameState<AWoRGameState>())
	{
		GameState->CastReviveVote(Player, bYes);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GTrialVoteCommand(
	TEXT("wor.Trial.Vote"),
	TEXT("B-187e: answer the mini-boss revive vote: wor.Trial.Vote yes | no (placeholder until the real screen)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
	{
		if (Args.Num() == 0 || (!Args[0].Equals(TEXT("yes"), ESearchCase::IgnoreCase) && !Args[0].Equals(TEXT("no"), ESearchCase::IgnoreCase)))
		{
			UE_LOG(LogWoR, Display, TEXT("[ReviveVote] usage: wor.Trial.Vote yes | no"));
			return;
		}
		URestSpotWidget::CastReviveVoteFor(World ? World->GetFirstPlayerController() : nullptr, Args[0].Equals(TEXT("yes"), ESearchCase::IgnoreCase));
	}));

FString URestSpotWidget::GetRestSpotWidgetString() const
{
	return FString::Printf(TEXT("[RestSpotMenu] %s | %s | host revive: %s (%d)"), PayoutText ? *PayoutText->GetText().ToString() : TEXT("-"),
						   bConfirmingLeave ? TEXT("confirming leave") : TEXT("menu"), bHostCanRevive ? TEXT("yes") : TEXT("no"), HostRevivalNumber);
}

void URestSpotWidget::HandleLevelUpClicked()
{
	APlayerController *PC = GetOwningPlayer();
	CloseMenu();
	ULevelDropWidget::Open(PC); // B-363 (D-156): place and buy pillar points here
}

void URestSpotWidget::HandleInventoryClicked()
{
	APlayerController *PC = GetOwningPlayer();
	WoRRestSpot::StepAway(GetWorld()); // review: the rest spot's levelling ends with its menu
	CloseMenu();
	UInventoryWidget::Open(PC);
}

void URestSpotWidget::HandleLeaveClicked()
{
	bConfirmingLeave = true;
	Refresh();
}

void URestSpotWidget::HandleConfirmLeaveClicked()
{
	UWorld *World = GetWorld();
	APlayerController *PC = GetOwningPlayer();
	CloseMenu();
	// B-187e (Q-187e): a client leaves the session, not the host's run (Monster Hunter: one hunter abandons alone).
	if (World && World->GetNetMode() == NM_Client && PC)
	{
		UE_LOG(LogWoR, Log, TEXT("[RestSpot] A client leaves the session from the rest menu"));
		PC->ConsoleCommand(TEXT("disconnect"));
		return;
	}
	WoRRestSpot::LeaveRun(World);
}

void URestSpotWidget::HandleHandOverClicked()
{
	UWorld *World = GetWorld();
	UGameInstance *GI = GetGameInstance();
	UWoRHostHandOverSubsystem *HandOver = GI ? GI->GetSubsystem<UWoRHostHandOverSubsystem>() : nullptr;
	APlayerController *PC = GetOwningPlayer();
	CloseMenu();
	// B-459a: ask first ("Hand over host?"); straight away only where that screen can't open (no root layout).
	if (!UHandOverConfirmWidget::Open(PC) && HandOver)
	{
		HandOver->BeginPlannedHandOver(World);
	}
}

void URestSpotWidget::HandleInviteClicked()
{
	APlayerController *PC = GetOwningPlayer();
	CloseMenu(); // not StepAway: the party stays at the rest spot while friends are seated
	// B-459b: Play together; the old placeholder only where that screen can't open (no root layout).
	if (!UInviteScreenWidget::Open(PC))
	{
		UInviteWidget::Open(PC);
	}
}

void URestSpotWidget::HandleCloseClicked()
{
	WoRRestSpot::StepAway(GetWorld()); // review: closing the menu walks away from the rest spot
	CloseMenu();
}

void URestSpotWidget::CloseMenu()
{
	if (APlayerController *PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

#undef LOCTEXT_NAMESPACE
