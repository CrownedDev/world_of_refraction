// RunStateComponent.cpp

#include "Player/RunStateComponent.h"

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Party/SeatRules.h"
#include "Player/WoRGameState.h"
#include "Player/WoRPlayerState.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRLog.h"

namespace
{
	const UTrialRunSubsystem *RunStateTrialRunOf(const UObject *WorldContext)
	{
		const UWorld *World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	}

	FAutoConsoleCommandWithWorld GPrintRunStateCmd(
		TEXT("wor.Run.PrintState"),
		TEXT("DEBUG (B-232): the GameState's run state, the side's roster with each member's commander, and the rotation."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			const URunStateComponent *Run = URunStateComponent::Find(World);
			UE_LOG(LogWoR, Log, TEXT("%s"), Run ? *Run->GetDebugString() : TEXT("[RunState] no AWoRGameState run state in this world"));
		}));
}

URunStateComponent::URunStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void URunStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(URunStateComponent, bHasRun);
	DOREPLIFETIME(URunStateComponent, ActiveTrial);
	DOREPLIFETIME(URunStateComponent, RunSeed);
	DOREPLIFETIME(URunStateComponent, VariationIndex);
	DOREPLIFETIME(URunStateComponent, Difficulty);
	DOREPLIFETIME(URunStateComponent, BeatenEncounters);
	DOREPLIFETIME(URunStateComponent, Roster);
	DOREPLIFETIME(URunStateComponent, RotationIndex);
}

URunStateComponent *URunStateComponent::Find(const UObject *WorldContext)
{
	const UWorld *World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AWoRGameState *GameState = World ? World->GetGameState<AWoRGameState>() : nullptr;
	return GameState ? GameState->GetRunState() : nullptr;
}

void URunStateComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		PullFromSubsystems();
		const UWorld *World = GetWorld();
		UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		if (UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr)
		{
			PartySession->OnPartyChanged.AddUniqueDynamic(this, &URunStateComponent::HandlePartyChanged);
		}
	}
}

void URunStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	const UWorld *World = GetWorld();
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	if (UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr)
	{
		PartySession->OnPartyChanged.RemoveDynamic(this, &URunStateComponent::HandlePartyChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void URunStateComponent::HandlePartyChanged(UParty * /*Party*/)
{
	RefreshRoster();
}

void URunStateComponent::RefreshRoster()
{
	const UWorld *World = GetWorld();
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	TArray<FRunRosterEntry> NewRoster;
	if (const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr)
	{
		for (const FPartyMember &Member : Party->Members)
		{
			if (Member.CharacterData)
			{
				FRunRosterEntry &Entry = NewRoster.AddDefaulted_GetRef();
				Entry.Character = Member.CharacterData;
				Entry.bAISeat = Member.bAISeat; // B-346's AI seat (Q-21-audit)
			}
		}
	}
	Roster = MoveTemp(NewRoster);
	UE_LOG(LogWoR, Log, TEXT("[RunState] Roster: %d member(s)"), Roster.Num());
}

void URunStateComponent::ResetRotation()
{
	RotationIndex = 0;
}

void URunStateComponent::PullFromSubsystems()
{
	if (const UTrialRunSubsystem *TrialRun = RunStateTrialRunOf(this))
	{
		bHasRun = TrialRun->IsRunActive();
		ActiveTrial = TrialRun->GetActiveTrial();
		RunSeed = TrialRun->GetRunSeed();
		VariationIndex = TrialRun->GetVariationIndex();
		Difficulty = TrialRun->GetTrialDifficulty();
		BeatenEncounters = TrialRun->GetBeatenEncounters().Array();
	}
	RefreshRoster();
	ResetRotation();
	UE_LOG(LogWoR, Log, TEXT("[RunState] Pulled: run %s (seed %d), %d beaten, %d roster member(s)"), bHasRun ? TEXT("on") : TEXT("off"), RunSeed,
		   BeatenEncounters.Num(), Roster.Num());
}

UTrialData *URunStateComponent::GetActiveTrial(const UObject *WorldContext)
{
	if (const URunStateComponent *Run = Find(WorldContext))
	{
		return Run->ActiveTrial;
	}
	const UTrialRunSubsystem *TrialRun = RunStateTrialRunOf(WorldContext);
	return TrialRun ? TrialRun->GetActiveTrial() : nullptr;
}

int32 URunStateComponent::GetRunSeed(const UObject *WorldContext)
{
	if (const URunStateComponent *Run = Find(WorldContext))
	{
		return Run->RunSeed;
	}
	const UTrialRunSubsystem *TrialRun = RunStateTrialRunOf(WorldContext);
	return TrialRun ? TrialRun->GetRunSeed() : 0;
}

bool URunStateComponent::IsEncounterBeaten(const UObject *WorldContext, FName EncounterId)
{
	if (const URunStateComponent *Run = Find(WorldContext))
	{
		return Run->BeatenEncounters.Contains(EncounterId);
	}
	const UTrialRunSubsystem *TrialRun = RunStateTrialRunOf(WorldContext);
	return TrialRun && TrialRun->IsEncounterBeaten(EncounterId);
}

void URunStateComponent::SetRoster(const TArray<FRunRosterEntry> &InRoster)
{
	Roster = InRoster;
	RotationIndex = 0;
}

const AGameStateBase *URunStateComponent::GetOwningGameState() const
{
	return Cast<AGameStateBase>(GetOwner());
}

const FRunRosterEntry *URunStateComponent::FindRosterEntry(const AActor *Member) const
{
	const UCharacterDataComponent *CharComp = Member ? Member->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	const UCharacterData *Character = CharComp ? CharComp->CharacterData : nullptr;
	return Character ? Roster.FindByPredicate([Character](const FRunRosterEntry &Entry) { return Entry.Character == Character; }) : nullptr;
}

TArray<int32> URunStateComponent::GetSidePlayerIds() const
{
	TArray<int32> Ids;
	if (const AGameStateBase *GameState = GetOwningGameState())
	{
		for (const APlayerState *Player : GameState->PlayerArray)
		{
			if (Player && !Player->IsABot())
			{
				Ids.Add(Player->GetPlayerId());
			}
		}
	}
	Ids.Sort(); // seat order every machine agrees on (PlayerArray order can differ between server and client)
	return Ids;
}

int32 URunStateComponent::GetClaimingPlayerId(const UCharacterData *Character) const
{
	if (const AGameStateBase *GameState = GetOwningGameState())
	{
		for (const APlayerState *Player : GameState->PlayerArray)
		{
			const AWoRPlayerState *WoRPlayer = Cast<AWoRPlayerState>(Player);
			if (WoRPlayer && WoRPlayer->ClaimsCharacter(Character))
			{
				return WoRPlayer->GetPlayerId();
			}
		}
	}
	return SeatRules::UNCLAIMED;
}

int32 URunStateComponent::GetCommander(const AActor *Member) const
{
	const FRunRosterEntry *Entry = FindRosterEntry(Member);
	if (!Entry)
	{
		return SeatRules::AI_COMMANDER;
	}
	return SeatRules::GetCommander(Entry->bAISeat, GetClaimingPlayerId(Entry->Character), GetSidePlayerIds(), RotationIndex);
}

bool URunStateComponent::IsCommandedBy(const AActor *Member, const AController *Sender) const
{
	const APlayerState *Player = Sender ? Sender->PlayerState : nullptr;
	if (!Player || !FindRosterEntry(Member))
	{
		return false;
	}
	const int32 Commander = GetCommander(Member);
	return Commander != SeatRules::AI_COMMANDER && Commander == Player->GetPlayerId();
}

void URunStateComponent::NoteTurnTaken(const AActor *Member)
{
	const FRunRosterEntry *Entry = FindRosterEntry(Member);
	if (!Entry || GetSidePlayerIds().IsEmpty())
	{
		return;
	}
	// A claim by a player who left falls back to the rotation (SeatRules::GetCommander), so it advances it too.
	const int32 Claimed = GetClaimingPlayerId(Entry->Character);
	const bool bClaimHere = Claimed != SeatRules::UNCLAIMED && GetSidePlayerIds().Contains(Claimed);
	if (SeatRules::UsesRotation(Entry->bAISeat, bClaimHere ? Claimed : SeatRules::UNCLAIMED))
	{
		++RotationIndex;
	}
}

FString URunStateComponent::GetDebugString() const
{
	FString Out = FString::Printf(TEXT("[RunState] run %s: trial %s, seed %d, variation %d, %d beaten; rotation %d"),
								  bHasRun ? TEXT("on") : TEXT("off"), *GetNameSafe(ActiveTrial), RunSeed, VariationIndex,
								  BeatenEncounters.Num(), RotationIndex);
	for (const FRunRosterEntry &Entry : Roster)
	{
		const int32 Claimed = GetClaimingPlayerId(Entry.Character);
		Out += FString::Printf(TEXT("\n  %s: %s"), *GetNameSafe(Entry.Character),
							   Entry.bAISeat ? TEXT("AI seat")
							   : Claimed != SeatRules::UNCLAIMED ? *FString::Printf(TEXT("player %d's"), Claimed)
																 : TEXT("unclaimed (rotates)"));
	}
	return Out;
}
