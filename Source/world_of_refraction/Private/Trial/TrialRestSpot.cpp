// TrialRestSpot.cpp

#include "Trial/TrialRestSpot.h"

#include "Player/WoRPlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Merchant/MerchantData.h"
#include "Player/RunStateComponent.h"
#include "Trial/LevelDropPlacement.h"
#include "WoRGameNames.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Character/Forms/CharacterFormSubsystem.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Hub/HubHUDModel.h"
#include "UI/Hub/RestSpotWidget.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "TrialRestSpot"

namespace RestSpotConstants
{
	/** Mixed into the run's seed so the shrine's pick isn't the same roll as anything else the seed decides. The
	 *  text is B-447's, so a run keeps its pick across the move from NPCs to shrines. */
	const TCHAR *const SHRINE_SALT = TEXT("PillarPointSeller");
}

bool WoRRestSpot::RestsMember(const FPartyMember &Member, const AWoRPlayerState *Presser, bool bPresserIsHost,
							  const TArray<const AWoRPlayerState *> &Players)
{
	const auto Claims = [&Member](const AWoRPlayerState *Player)
	{
		return Player && (Player->ClaimsMember(Member.MemberId) || Player->ClaimsCharacter(Member.CharacterData));
	};
	if (Claims(Presser))
	{
		return true;
	}
	return bPresserIsHost && !Players.ContainsByPredicate(Claims);
}

int32 WoRRestSpot::Rest(UWorld *World, FName RestSpotId, const AWoRPlayerState *Presser)
{
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	// Desktop review: the host rests the party (a client has no run; its press reaches the host in B-187b, D-103).
	if (!Run || !Run->IsRunActive() || !PartySession || World->GetNetMode() == NM_Client)
	{
		return INDEX_NONE;
	}
	// Q-187d (Crown, 2026-11-08): only the presser's characters rest; D-043: its fallen come back here.
	TArray<const AWoRPlayerState *> Players;
	if (const AGameStateBase *GameState = World->GetGameState())
	{
		for (const APlayerState *Player : GameState->PlayerArray)
		{
			Players.Add(Cast<AWoRPlayerState>(Player));
		}
	}
	const APlayerController *PresserPC = Presser ? Cast<APlayerController>(Presser->GetOwner()) : nullptr;
	const bool bPresserIsHost = !PresserPC || PresserPC->IsLocalController();
	const int32 Revived = Presser ? PartySession->RestMembersWhere([Presser, bPresserIsHost, &Players](const FPartyMember &Member)
														   { return RestsMember(Member, Presser, bPresserIsHost, Players); })
								  : PartySession->RestMembersWhere([](const FPartyMember &) { return true; });
	Run->NoteRestSpotUsed(RestSpotId); // a wipe sends the party back here (D-155)
	// Merge B-187e + B-363: a client's press runs here on the host; the host's own rest-spot flag stays as it is (only
	// the client's menu would close it). The presser is the host when its controller is local (or with no presser).
	if (bPresserIsHost)
	{
		Run->SetAtRestSpot(true); // B-363 (D-156): pillar points are placed here; the skin switch reads it too
	}
	if (USaveSubsystem *Save = GI->GetSubsystem<USaveSubsystem>())
	{
		Save->SaveNow(World, true); // desktop review: a rest is a checkpoint (the heal and the rest spot survive a quit)
	}
	UE_LOG(LogWoR, Log, TEXT("[RestSpot] Rested at %s (%s): %d revived"), *RestSpotId.ToString(),
		   Presser ? *Presser->GetPlayerName() : TEXT("whole party"), Revived);
	return Revived;
}

bool WoRRestSpot::CanReviveHere(const UWorld *World)
{
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	return Run && Run->CanReviveEncounter(Run->GetLastRestSpot());
}

bool WoRRestSpot::ReviveMiniBoss(UWorld *World)
{
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	// Desktop review: only when the level can reload, so a revive is never spent for nothing.
	const UTrialData *Trial = Run ? Run->GetActiveTrial() : nullptr;
	if (!Run || World->GetNetMode() == NM_Client || !Trial || Trial->Level.IsNull() || !Run->ReviveEncounter(Run->GetLastRestSpot()))
	{
		return false;
	}
	if (USaveSubsystem *Save = GI->GetSubsystem<USaveSubsystem>())
	{
		Save->SaveNow(World, true); // desktop review: quitting right after a revive can't skip the mini-boss
	}
	// B-187c: the reload puts the party back where it stood to rest (ATrialGameMode::RestartPlayer).
	FTransform Stand;
	if (Run->GetRestStand(Stand))
	{
		Run->SetPendingReturnStand(Stand);
	}
	StepAway(World); // desktop review: the party leaves the spot with the reload
	Run->ReloadTrialLevel();
	return true;
}

void WoRRestSpot::StepAway(UWorld *World)
{
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	if (UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr)
	{
		Run->SetAtRestSpot(false);
	}
	if (UCharacterFormSubsystem *Forms = GI ? GI->GetSubsystem<UCharacterFormSubsystem>() : nullptr)
	{
		Forms->SetAtRestSpot(false);
	}
}

bool WoRRestSpot::CanExtractHere(const UWorld *World)
{
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	if (!Run || !Run->IsRunActive())
	{
		return false;
	}
	const ATrialRestSpot *Spot = FindRestingSpot(World);
	return Run->IsBeatenMiniBoss(Run->GetLastRestSpot()) || (Spot && Spot->GetService() == ERestService::Extract); // B-502: V's rest point
}

const ATrialRestSpot *WoRRestSpot::FindRestingSpot(const UWorld *World)
{
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	const ATrialRestSpot *Spot = Run && Run->IsAtRestSpot() ? FindLastRestSpot(World) : nullptr;
	// The last rest spot is the whole party's (a guest's rest moves it): the host's own pawn must stand in this one.
	const APlayerController *Host = Spot ? World->GetFirstPlayerController() : nullptr;
	const APawn *HostPawn = Host ? Host->GetPawn() : nullptr;
	return Spot && (!HostPawn || Spot->IsWithinReach(HostPawn)) ? Spot : nullptr;
}

const ATrialRestSpot *WoRRestSpot::FindLastRestSpot(const UWorld *World)
{
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	const FName LastId = Run && Run->IsRunActive() ? Run->GetLastRestSpot() : NAME_None;
	if (LastId.IsNone())
	{
		return nullptr;
	}
	for (TActorIterator<ATrialRestSpot> It(World); It; ++It)
	{
		if (It->GetRestSpotId() == LastId)
		{
			return *It;
		}
	}
	return nullptr;
}

bool WoRRestSpot::GetServiceVendor(ERestService Service, EMerchantType &OutVendor)
{
	switch (Service)
	{
	case ERestService::Blacksmith:
		OutVendor = EMerchantType::Blacksmith;
		return true;
	case ERestService::Spiritualist:
		OutVendor = EMerchantType::Spiritualist;
		return true;
	case ERestService::Jeweler:
		OutVendor = EMerchantType::Jeweler;
		return true;
	default:
		return false;
	}
}

int32 WoRRestSpot::PickShrineIndex(int32 RunSeed, int32 Count)
{
	// The seed alone decides, so the pick holds for the whole run (and a reloaded level) and changes between runs.
	return Count > 1 ? static_cast<int32>(HashCombine(GetTypeHash(RunSeed), FCrc::StrCrc32(RestSpotConstants::SHRINE_SALT)) % static_cast<uint32>(Count)) : 0;
}

const ATrialRestSpot *WoRRestSpot::FindAwakeShrine(const UWorld *World)
{
	// The run's seed as this machine knows it (the GameState's copy on a guest): a guest sees the same shrine as
	// the host, and is refused the buy itself (TryBuyPointHere).
	const int32 RunSeed = World && URunStateComponent::GetActiveTrial(World) ? URunStateComponent::GetRunSeed(World) : 0;
	if (RunSeed == 0) // as UTrialRunSubsystem::IsRunActive: no trial, or no seed yet
	{
		return nullptr;
	}
	TArray<const ATrialRestSpot *> Shrines;
	for (TActorIterator<ATrialRestSpot> It(World); It; ++It)
	{
		if (It->GetService() == ERestService::PillarShrine)
		{
			Shrines.Add(*It);
		}
	}
	// By id, as text: the same order every time the level loads (an FName's own order changes between launches).
	Shrines.Sort([](const ATrialRestSpot &A, const ATrialRestSpot &B) { return A.GetRestSpotId().ToString() < B.GetRestSpotId().ToString(); });
	return Shrines.IsEmpty() ? nullptr : Shrines[PickShrineIndex(RunSeed, Shrines.Num())];
}

bool WoRRestSpot::SellsPointsHere(const UWorld *World)
{
	const ATrialRestSpot *Spot = FindRestingSpot(World);
	return Spot && Spot == FindAwakeShrine(World);
}

FText WoRRestSpot::TryBuyPointHere(UWorld *World, const FPartyMember &Member, UCurrencyComponent *Wallet)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return LOCTEXT("HostBuys", "Only the host can buy for now."); // the run's points and prices live on the host
	}
	if (!SellsPointsHere(World))
	{
		return LOCTEXT("NoPointsHere", "No pillar points are sold here.");
	}
	return WoRLevelDrops::TryBuyPoint(World, Member, Wallet);
}

FString WoRRestSpot::GetRestSpotsString(const UWorld *World)
{
	TArray<FString> Lines;
	if (World)
	{
		const ATrialRestSpot *Shrine = FindAwakeShrine(World);
		const ATrialRestSpot *Last = FindLastRestSpot(World);
		for (TActorIterator<ATrialRestSpot> It(World); It; ++It)
		{
			Lines.Add(FString::Printf(TEXT("%s: %s%s%s at %s"), *It->GetRestSpotId().ToString(),
									  *StaticEnum<ERestService>()->GetNameStringByValue(static_cast<int64>(It->GetService())),
									  *It == Shrine ? TEXT(" (sells points this run)") : TEXT(""), *It == Last ? TEXT(" (last rested here)") : TEXT(""),
									  *It->GetActorLocation().ToCompactString()));
		}
	}
	return FString::Printf(TEXT("[RestSpot] %d placed: %s; extract here: %s; points sold here: %s"), Lines.Num(),
						   Lines.IsEmpty() ? TEXT("none") : *FString::Join(Lines, TEXT(" | ")), CanExtractHere(World) ? TEXT("yes") : TEXT("no"),
						   SellsPointsHere(World) ? TEXT("yes") : TEXT("no"));
}

bool WoRRestSpot::LeaveRun(UWorld *World)
{
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	if (!Run || !Run->IsRunActive() || World->GetNetMode() == NM_Client)
	{
		return false;
	}
	if (!CanExtractHere(World))
	{
		UE_LOG(LogWoR, Log, TEXT("[RestSpot] Extract refused: rest spot %s is not one a beaten boss left (D-186) nor V's (D-214)"), *Run->GetLastRestSpot().ToString());
		return false;
	}
	Run->LeaveRunEarly();
	return true;
}

FText WoRRestSpot::GetLeaveConfirmText()
{
	return LOCTEXT("ExtractConfirm", "Extract? You keep part of what you earned. Full rewards only for finishing the trial or falling in battle.");
}

void ATrialRestSpot::Interact(APawn *Pawn)
{
	const AWoRPlayerState *Presser = Pawn ? Pawn->GetPlayerState<AWoRPlayerState>() : nullptr;
	if (WoRRestSpot::Rest(GetWorld(), GetRestSpotId(), Presser) == INDEX_NONE)
	{
		return;
	}
	if (UTrialRunSubsystem *Run = Pawn ? GetTrialRun() : nullptr)
	{
		Run->NoteRestStand(Pawn->GetActorTransform()); // B-187c: a revive brings the party back here
	}
	ShowMessageTo(Pawn, LOCTEXT("Rested", "The party is restored."));
	APlayerController *PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (PC && PC->IsLocalController())
	{
		URestSpotWidget::Open(PC);
	}
	else if (AWoRPlayerState *Player = PC ? PC->GetPlayerState<AWoRPlayerState>() : nullptr)
	{
		// B-187d: a client's press (ServerInteractFeature) opens its own menu; B-187e: with the host's revive answer.
		Player->Client_OpenRestMenu(WoRRestSpot::CanReviveHere(GetWorld()), WoRRestSpot::GetNextRevivalNumber(GetWorld()),
									WoRRestSpot::GetReviveOpponentName(GetWorld()));
	}
	RefreshHint();
}

int32 WoRRestSpot::GetNextRevivalNumber(const UWorld *World)
{
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	return Run ? Run->GetEncounterRevivals(Run->GetLastRestSpot()) + 1 : 0;
}

FText WoRRestSpot::GetReviveOpponentName(const UWorld *World)
{
	const ATrialRestSpot *Spot = FindLastRestSpot(World);
	return Spot && !Spot->GetOpponentName().IsEmpty() ? Spot->GetOpponentName() : LOCTEXT("ReviveOpponentUnnamed", "opponent");
}

FText WoRRestSpot::MakeReviveRowText(const FText &OpponentName, int32 RevivalNumber)
{
	return FText::Format(LOCTEXT("ReviveRow", "Revive the {0} ({1} of {2})"), OpponentName, FText::AsNumber(RevivalNumber),
						 FText::AsNumber(MiniBossRevival::MAX_REVIVALS));
}

ATrialRestSpot *ATrialRestSpot::SpawnForMiniBoss(UWorld *World, const FTransform &Where, FName EncounterId, const FText &InOpponentName)
{
	if (!World || EncounterId.IsNone())
	{
		return nullptr;
	}
	ATrialRestSpot *Spot = World->SpawnActorDeferred<ATrialRestSpot>(ATrialRestSpot::StaticClass(), Where, nullptr, nullptr,
																	 ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Spot)
	{
		return nullptr;
	}
	Spot->MiniBossEncounterId = EncounterId; // before BeginPlay, so the hint and id are right from the start
	Spot->OpponentName = InOpponentName;
	Spot->FinishSpawning(Where);
	UE_LOG(LogWoR, Log, TEXT("[RestSpot] A rest spot where mini-boss %s (%s) fell"), *EncounterId.ToString(), *InOpponentName.ToString());
	return Spot;
}

FString ATrialRestSpot::GetHintText() const
{
	return WoRHubHUD::FormatPromptLine(GetPrompt()).ToString(); // B-491 (D-213): "E  Rest"
}

FInteractPrompt ATrialRestSpot::GetPrompt() const
{
	return FInteractPrompt(EInteractVerb::Rest, WoRGameNames::GetRestServiceName(GetService())); // B-502: "Rest · Anvil"
}

void ATrialRestSpot::SetService(ERestService InService)
{
	Service = InService;
	RefreshHint();
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GTrialPrintRestSpotsCommand(
	TEXT("wor.Trial.PrintRestSpots"),
	TEXT("B-502: the rest spots placed in this level (service, where, which shrine sells points this run) and what the last rested one offers."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		UE_LOG(LogWoR, Display, TEXT("%s"), *WoRRestSpot::GetRestSpotsString(World));
	}));

static FAutoConsoleCommandWithWorldAndArgs GTrialSetRestServiceCommand(
	TEXT("wor.Trial.SetRestService"),
	TEXT("B-502 DEBUG: wor.Trial.SetRestService <none|anvil|tree|pedestal|shrine|v>: gives the rest spot nearest the player that service, for a PIE "
		 "check before the services are set in the trial maps. Not saved; the host's machine only."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
	{
		static const TMap<FString, ERestService> Names = {{TEXT("none"), ERestService::None}, {TEXT("anvil"), ERestService::Blacksmith},
														  {TEXT("tree"), ERestService::Spiritualist}, {TEXT("pedestal"), ERestService::Jeweler},
														  {TEXT("shrine"), ERestService::PillarShrine}, {TEXT("v"), ERestService::Extract}};
		const ERestService *Wanted = Args.Num() > 0 ? Names.Find(Args[0].ToLower()) : nullptr;
		const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
		const APawn *Pawn = PC ? PC->GetPawn() : nullptr;
		ATrialRestSpot *Nearest = nullptr;
		if (Wanted && Pawn)
		{
			for (TActorIterator<ATrialRestSpot> It(World); It; ++It)
			{
				if (!Nearest || FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation()) <
									FVector::DistSquared(Nearest->GetActorLocation(), Pawn->GetActorLocation()))
				{
					Nearest = *It;
				}
			}
		}
		if (!Nearest)
		{
			UE_LOG(LogWoR, Warning, TEXT("[RestSpot] wor.Trial.SetRestService <none|anvil|tree|pedestal|shrine|v>: needs a service, a pawn and a rest spot in the level"));
			return;
		}
		Nearest->SetService(*Wanted);
		UE_LOG(LogWoR, Display, TEXT("%s"), *WoRRestSpot::GetRestSpotsString(World));
	}));
#endif

static FAutoConsoleCommandWithWorld GTrialRestCommand(
	TEXT("wor.Trial.Rest"),
	TEXT("B-187: rest the party as at a rest spot (full HP / EP, the fallen revived, the last rest spot noted) and open the rest menu."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const int32 Revived = WoRRestSpot::Rest(World, TEXT("ConsoleRest"));
		if (Revived == INDEX_NONE)
		{
			UE_LOG(LogWoR, Display, TEXT("[RestSpot] wor.Trial.Rest: no run"));
			return;
		}
		URestSpotWidget::Open(World->GetFirstPlayerController());
	}));

#undef LOCTEXT_NAMESPACE
