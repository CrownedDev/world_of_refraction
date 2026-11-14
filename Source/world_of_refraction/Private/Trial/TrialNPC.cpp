// TrialNPC.cpp

#include "Trial/TrialNPC.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Trial/RunRewards.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Hub/HubHUDModel.h"
#include "UI/Trial/TrialNPCWidget.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "TrialNPC"

namespace TrialNPCConstants
{
	/** wor.Trial.SpawnV: how far in front of the player V appears (cm). */
	constexpr float SPAWN_AHEAD_CM = 200.0f;
}

ATrialNPC::ATrialNPC()
{
	DisplayName = LOCTEXT("V", "V");
}

void ATrialNPC::Interact(APawn *Pawn)
{
	APlayerController *PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (PC && PC->IsLocalController())
	{
		UTrialNPCWidget::Open(PC, this);
		return;
	}
	// A client's press arrives on the host (ServerInteractFeature). The run is the host's, so only the host extracts.
	// TODO: B-447 / co-op: a client's own talk panel (a Client RPC, as the rest menu's Client_OpenRestMenu).
	ShowMessageTo(Pawn, FText::Format(LOCTEXT("HostTalks", "Only the host can talk to {0} for now."), DisplayName));
}

FString ATrialNPC::GetHintText() const
{
	return WoRHubHUD::FormatPromptLine(GetPrompt()).ToString();
}

FText WoRTrialNPC::GetExtractRefusal(const UWorld *World)
{
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	if (!Run || !Run->IsRunActive())
	{
		return LOCTEXT("NoRun", "Not in a trial.");
	}
	if (World->GetNetMode() == NM_Client)
	{
		return LOCTEXT("HostExtracts", "Only the host can extract.");
	}
	return FText::GetEmpty();
}

bool WoRTrialNPC::Extract(UWorld *World)
{
	const FText Refusal = GetExtractRefusal(World);
	if (!Refusal.IsEmpty())
	{
		UE_LOG(LogWoR, Log, TEXT("[TrialNPC] Extract refused: %s"), *Refusal.ToString());
		return false;
	}
	UE_LOG(LogWoR, Log, TEXT("[TrialNPC] The party extracts at an NPC: the run pays %d%% (D-186, Q-449)"),
		   FMath::RoundToInt(RunRewards::LEAVE_EARLY_SHARE * 100.0f));
	World->GetGameInstance()->GetSubsystem<UTrialRunSubsystem>()->LeaveRunEarly();
	return true;
}

FString WoRTrialNPC::GetTrialNPCsString(const UWorld *World)
{
	TArray<FString> Lines;
	if (World)
	{
		for (TActorIterator<ATrialNPC> It(World); It; ++It)
		{
			Lines.Add(FString::Printf(TEXT("%s (%s)%s at %s"), *It->GetDisplayName().ToString(), *It->GetFeatureId().ToString(),
									  It->ExtractsParty() ? TEXT(" extracts") : TEXT(""), *It->GetActorLocation().ToCompactString()));
		}
	}
	return FString::Printf(TEXT("[TrialNPC] %d placed: %s; extract now: %s"), Lines.Num(), Lines.IsEmpty() ? TEXT("none") : *FString::Join(Lines, TEXT(" | ")),
						   GetExtractRefusal(World).IsEmpty() ? TEXT("yes") : *GetExtractRefusal(World).ToString());
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GTrialPrintNPCsCommand(
	TEXT("wor.Trial.PrintNPCs"),
	TEXT("B-449: the trial NPCs placed in this level (name, what each offers, where) and whether the party can extract now."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		UE_LOG(LogWoR, Display, TEXT("%s"), *WoRTrialNPC::GetTrialNPCsString(World));
	}));

static FAutoConsoleCommandWithWorld GTrialSpawnVCommand(
	TEXT("wor.Trial.SpawnV"),
	TEXT("B-449 DEBUG: puts V (the NPC who extracts the party) in front of the player, for a PIE check before V is placed in the trial maps."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
		const APawn *Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogWoR, Warning, TEXT("[TrialNPC] wor.Trial.SpawnV: no pawn here, or a client (the host places V)"));
			return;
		}
		const FVector Where = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * TrialNPCConstants::SPAWN_AHEAD_CM;
		const ATrialNPC *V = World->SpawnActor<ATrialNPC>(ATrialNPC::StaticClass(), FTransform(Where));
		UE_LOG(LogWoR, Display, TEXT("[TrialNPC] wor.Trial.SpawnV: %s"), V ? *WoRTrialNPC::GetTrialNPCsString(World) : TEXT("the spawn failed"));
	}));
#endif

#undef LOCTEXT_NAMESPACE
