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

	FAutoConsoleCommandWithWorld GPrintJoinRequestsCmd(
		TEXT("wor.Party.PrintJoinRequests"),
		TEXT("D-150: the players waiting to join this run and the AI seats they could take."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				const URunStateComponent *Run = URunStateComponent::Find(World);
				if (!Run)
				{
					UE_LOG(LogWoR, Display, TEXT("[RunState] no GameState run state here"));
					return;
				}
				FString Seats;
				for (const int32 Slot : Run->GetFreeAISeats())
				{
					Seats += FString::Printf(TEXT(" %d"), Slot);
				}
				UE_LOG(LogWoR, Display, TEXT("[RunState] %d join request(s); AI seats:%s"), Run->GetJoinRequests().Num(), Seats.IsEmpty() ? TEXT(" none") : *Seats);
				for (const FRunJoinRequest &Request : Run->GetJoinRequests())
				{
					UE_LOG(LogWoR, Display, TEXT("  player %d with %s"), Request.PlayerId, *GetNameSafe(Request.Character));
				}
			}));

	FAutoConsoleCommandWithWorldAndArgs GAcceptJoinCmd(
		TEXT("wor.Party.AcceptJoin"),
		TEXT("D-150, host only: wor.Party.AcceptJoin <player id> <slot> - that player's character takes the AI seat in <slot>; the seat's gear and pouch go to the host."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				URunStateComponent *Run = URunStateComponent::Find(World);
				if (!Run || Args.Num() < 2 || !Args[0].IsNumeric() || !Args[1].IsNumeric())
				{
					UE_LOG(LogWoR, Display, TEXT("[RunState] usage: wor.Party.AcceptJoin <player id> <slot> (on the host)"));
					return;
				}
				Run->AcceptJoin(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]));
			}));

	FAutoConsoleCommandWithWorldAndArgs GRefuseJoinCmd(
		TEXT("wor.Party.RefuseJoin"),
		TEXT("D-150, host only: wor.Party.RefuseJoin <player id> - drop that player's join request."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				URunStateComponent *Run = URunStateComponent::Find(World);
				if (!Run || Args.Num() < 1 || !Args[0].IsNumeric())
				{
					UE_LOG(LogWoR, Display, TEXT("[RunState] usage: wor.Party.RefuseJoin <player id> (on the host)"));
					return;
				}
				Run->RefuseJoin(FCString::Atoi(*Args[0]));
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
	if (!Entry || Entry->bAISeat)
	{
		return SeatRules::AI_COMMANDER;
	}
	const TArray<int32> Side = GetSidePlayerIds();
	if (Side.IsEmpty())
	{
		return SeatRules::AI_COMMANDER;
	}
	const int32 Claimed = GetClaimingPlayerId(Entry->Character);
	if (Claimed != SeatRules::UNCLAIMED && Side.Contains(Claimed))
	{
		return Claimed;
	}
	// D-150: no unclaimed members in multiplayer (every slot is a player's character or an AI seat), so an unclaimed
	// member is the host's: solo, that is the one player; in co-op the lowest id (the listen server's). No rotation.
	return Side[0];
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

void URunStateComponent::NoteTurnTaken(const AActor * /*Member*/)
{
	// OBSOLETE (B-360, D-150): nothing rotates any more.
}

TArray<int32> URunStateComponent::GetFreeAISeats() const
{
	TArray<int32> Seats;
	const UWorld *World = GetWorld();
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	if (const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr)
	{
		for (int32 Slot = 0; Slot < Party->Members.Num(); ++Slot)
		{
			if (Slot != PartyConstants::LEADER_SLOT && Party->Members[Slot].bAISeat)
			{
				Seats.Add(Slot);
			}
		}
	}
	return Seats;
}

bool URunStateComponent::RequestJoin(APlayerState *Joiner, UCharacterData *Character, TSoftClassPtr<ACombatCharacter> PawnClass)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Joiner || !Character)
	{
		return false;
	}
	const UWorld *World = GetWorld();
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	if (!PartySession || !PartySession->GetLocalParty())
	{
		UE_LOG(LogWoR, Warning, TEXT("[RunState] Join refused for player %d: no party here"), Joiner->GetPlayerId());
		return false;
	}
	if (GetFreeAISeats().IsEmpty())
	{
		UE_LOG(LogWoR, Warning, TEXT("[RunState] Join refused for player %d: the party is full (no AI seat to take, D-150)"), Joiner->GetPlayerId());
		return false;
	}
	if (PartySession->GetMemberSlotByData(Character) != INDEX_NONE)
	{
		// TODO: B-360b (D-150 point 2): two players may bring the same character as separate copies.
		UE_LOG(LogWoR, Warning, TEXT("[RunState] Join refused for player %d: %s is already in the party (separate copies: B-360b)"),
			   Joiner->GetPlayerId(), *GetNameSafe(Character));
		return false;
	}
	FRunJoinRequest *Existing = JoinRequests.FindByPredicate([Joiner](const FRunJoinRequest &R) { return R.PlayerId == Joiner->GetPlayerId(); });
	FRunJoinRequest &Request = Existing ? *Existing : JoinRequests.AddDefaulted_GetRef();
	Request.PlayerId = Joiner->GetPlayerId();
	Request.Character = Character;
	Request.PawnClass = PawnClass;
	UE_LOG(LogWoR, Log, TEXT("[RunState] Player %d (%s) asks to join with %s; free AI seats: %d. Host: wor.Party.AcceptJoin %d <slot> (or wor.UI.JoinSeat)"),
		   Request.PlayerId, *Joiner->GetPlayerName(), *GetNameSafe(Character), GetFreeAISeats().Num(), Request.PlayerId);
	return true;
}

bool URunStateComponent::AcceptJoin(int32 PlayerId, int32 ReplaceSlot)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}
	const int32 Index = JoinRequests.IndexOfByPredicate([PlayerId](const FRunJoinRequest &R) { return R.PlayerId == PlayerId; });
	if (Index == INDEX_NONE)
	{
		UE_LOG(LogWoR, Warning, TEXT("[RunState] AcceptJoin: player %d has no request"), PlayerId);
		return false;
	}
	const FRunJoinRequest Request = JoinRequests[Index];
	const UWorld *World = GetWorld();
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	if (!PartySession || !PartySession->ReplaceAISeatWithPlayer(ReplaceSlot, Request.PawnClass, Request.Character))
	{
		return false;
	}
	if (const AGameStateBase *GameState = GetOwningGameState())
	{
		for (APlayerState *Player : GameState->PlayerArray)
		{
			if (AWoRPlayerState *WoRPlayer = Cast<AWoRPlayerState>(Player); WoRPlayer && WoRPlayer->GetPlayerId() == PlayerId)
			{
				WoRPlayer->ClaimCharacter(Request.Character);
			}
		}
	}
	JoinRequests.RemoveAt(Index);
	RefreshRoster();
	UE_LOG(LogWoR, Log, TEXT("[RunState] Player %d joined: %s took slot %d"), PlayerId, *GetNameSafe(Request.Character), ReplaceSlot);
	return true;
}

bool URunStateComponent::RefuseJoin(int32 PlayerId)
{
	const int32 Removed = JoinRequests.RemoveAll([PlayerId](const FRunJoinRequest &R) { return R.PlayerId == PlayerId; });
	if (Removed > 0)
	{
		UE_LOG(LogWoR, Log, TEXT("[RunState] Player %d's join request refused by the host"), PlayerId);
	}
	return Removed > 0;
}

FString URunStateComponent::GetDebugString() const
{
	FString Out = FString::Printf(TEXT("[RunState] run %s: trial %s, seed %d, variation %d, %d beaten; %d player(s)"),
								  bHasRun ? TEXT("on") : TEXT("off"), *GetNameSafe(ActiveTrial), RunSeed, VariationIndex,
								  BeatenEncounters.Num(), GetSidePlayerIds().Num());
	for (const FRunRosterEntry &Entry : Roster)
	{
		const int32 Claimed = GetClaimingPlayerId(Entry.Character);
		Out += FString::Printf(TEXT("\n  %s: %s"), *GetNameSafe(Entry.Character),
							   Entry.bAISeat ? TEXT("AI seat")
							   : Claimed != SeatRules::UNCLAIMED ? *FString::Printf(TEXT("player %d's"), Claimed)
																 : TEXT("unclaimed (the host's, D-150)"));
	}
	for (const FRunJoinRequest &Request : JoinRequests)
	{
		Out += FString::Printf(TEXT("\n  join request: player %d with %s"), Request.PlayerId, *GetNameSafe(Request.Character));
	}
	return Out;
}
