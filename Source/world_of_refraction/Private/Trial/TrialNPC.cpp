// TrialNPC.cpp

#include "Trial/TrialNPC.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Player/RunStateComponent.h"
#include "Trial/LevelDropPlacement.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Hub/HubHUDModel.h"
#include "UI/Trial/TrialNPCWidget.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "TrialNPC"

namespace TrialNPCConstants
{
	/** wor.Trial.SpawnV: how far in front of the player V appears (cm). */
	constexpr float SPAWN_AHEAD_CM = 200.0f;

	/** B-447: mixed into the run's seed so the seller's pick isn't the same roll as anything else the seed decides. */
	const TCHAR *const SELLER_SALT = TEXT("PillarPointSeller");
}

ATrialNPC::ATrialNPC()
{
	DisplayName = LOCTEXT("V", "V");
}

void ATrialNPC::Interact(APawn *Pawn)
{
	// The press is the pressing machine's own (HubInteract routes only chests, drops and rest spots to the host), so a
	// guest opens its own panel; there the offer is greyed with "Only the host can extract" (the run is the host's).
	APlayerController *PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (PC && PC->IsLocalController())
	{
		UTrialNPCWidget::Open(PC, this);
	}
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
	UTrialRunSubsystem *Run = World->GetGameInstance()->GetSubsystem<UTrialRunSubsystem>();
	Run->LeaveRunEarly();
	// LeaveRunEarly does nothing when the run has no hub to go back to (it logs why): then nothing was left.
	UE_LOG(LogWoR, Log, TEXT("[TrialNPC] Extract at an NPC (D-186, Q-449): %s"), Run->IsLeavingEarly() ? TEXT("the run ends at the leave share") : TEXT("the run couldn't be left"));
	return Run->IsLeavingEarly();
}

int32 WoRTrialNPC::PickSellerIndex(int32 RunSeed, int32 Count)
{
	// The seed alone decides, so the pick holds for the whole run (and a reloaded level) and changes between runs.
	return Count > 1 ? static_cast<int32>(HashCombine(GetTypeHash(RunSeed), FCrc::StrCrc32(TrialNPCConstants::SELLER_SALT)) % static_cast<uint32>(Count)) : 0;
}

const ATrialNPC *WoRTrialNPC::FindPointSeller(const UWorld *World)
{
	// The run's seed as this machine knows it (the GameState's copy on a guest, review): a guest sees the same seller
	// as the host, and is refused the buy itself (TryBuyPointAt).
	const int32 RunSeed = World && URunStateComponent::GetActiveTrial(World) ? URunStateComponent::GetRunSeed(World) : 0;
	if (RunSeed == 0) // as UTrialRunSubsystem::IsRunActive: no trial, or no seed yet
	{
		return nullptr;
	}
	TArray<const ATrialNPC *> Placed;
	for (TActorIterator<ATrialNPC> It(World); It; ++It)
	{
		Placed.Add(*It);
	}
	// By id, as text: the same order every time the level loads (an FName's own order changes between launches).
	Placed.Sort([](const ATrialNPC &A, const ATrialNPC &B) { return A.GetFeatureId().ToString() < B.GetFeatureId().ToString(); });
	return Placed.IsEmpty() ? nullptr : Placed[PickSellerIndex(RunSeed, Placed.Num())];
}

FText WoRTrialNPC::TryBuyPointAt(UWorld *World, const ATrialNPC &NPC, const FPartyMember &Member, UCurrencyComponent *Wallet)
{
	if (FindPointSeller(World) != &NPC)
	{
		return FText::Format(LOCTEXT("NotTheSeller", "{0} has no points to sell."), NPC.GetDisplayName());
	}
	if (World->GetNetMode() == NM_Client)
	{
		return LOCTEXT("HostBuys", "Only the host can buy for now."); // the run's points and prices live on the host
	}
	return WoRLevelDrops::TryBuyPoint(World, Member, Wallet);
}

FString WoRTrialNPC::GetTrialNPCsString(const UWorld *World)
{
	TArray<FString> Lines;
	if (World)
	{
		const ATrialNPC *Seller = FindPointSeller(World);
		for (TActorIterator<ATrialNPC> It(World); It; ++It)
		{
			Lines.Add(FString::Printf(TEXT("%s (%s)%s%s at %s"), *It->GetDisplayName().ToString(), *It->GetFeatureId().ToString(),
									  It->ExtractsParty() ? TEXT(" extracts") : TEXT(""), *It == Seller ? TEXT(" sells points this run") : TEXT(""),
									  *It->GetActorLocation().ToCompactString()));
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

static FAutoConsoleCommandWithWorldAndArgs GTrialSpawnNPCCommand(
	TEXT("wor.Trial.SpawnNPC"),
	TEXT("B-447 DEBUG: wor.Trial.SpawnNPC <name>: puts another trial character (one who doesn't extract) in front of the player. With two or "
		 "more NPCs in the level, one of them sells pillar points this run (wor.Trial.PrintNPCs says which)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
	{
		const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
		const APawn *Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn || Args.IsEmpty() || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogWoR, Warning, TEXT("[TrialNPC] wor.Trial.SpawnNPC <name>: needs a name and a pawn, on the host"));
			return;
		}
		const FVector Where = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * TrialNPCConstants::SPAWN_AHEAD_CM;
		if (ATrialNPC *NPC = World->SpawnActor<ATrialNPC>(ATrialNPC::StaticClass(), FTransform(Where)))
		{
			NPC->SetIdentity(FText::FromString(Args[0]), /*bInExtractsParty=*/false);
		}
		UE_LOG(LogWoR, Display, TEXT("[TrialNPC] wor.Trial.SpawnNPC: %s"), *WoRTrialNPC::GetTrialNPCsString(World));
	}));
#endif

#undef LOCTEXT_NAMESPACE
