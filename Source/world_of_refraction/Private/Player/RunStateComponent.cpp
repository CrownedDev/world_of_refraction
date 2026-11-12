// RunStateComponent.cpp

#include "Player/RunStateComponent.h"
#include "EngineUtils.h"
#include "Trial/TrialMapFeature.h"
#include "Memory/MemoryData.h"

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/TurnManager.h"
#include "Combat/WoRFightSubsystem.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
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

bool RunJoinRules::CanSeatJoiner(bool bInRun, bool bHostAtRestSpot, bool bOwnHeldSeat)
{
	return !bInRun || bHostAtRestSpot || bOwnHeldSeat;
}

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
	DOREPLIFETIME(URunStateComponent, FoundFeatureIds); // B-232d
	DOREPLIFETIME(URunStateComponent, RunMemories);
	DOREPLIFETIME(URunStateComponent, UnspentLevels);
	DOREPLIFETIME(URunStateComponent, RotationIndex);
}

namespace
{
	/** B-232d (review): the GameState's copy answers only on a client. The host and solo read the run itself: the
	 *  copy is filled at the component's BeginPlay, after placed actors may already have asked. */
	const URunStateComponent *RunStateCopyFor(const UObject *WorldContext)
	{
		const UWorld *World = WorldContext ? WorldContext->GetWorld() : nullptr;
		return World && World->GetNetMode() == NM_Client ? URunStateComponent::Find(WorldContext) : nullptr;
	}
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
		// B-233b: a logout is a player really leaving (seamless travel keeps its controller, so none fires then).
		LogoutHandle = FGameModeEvents::GameModeLogoutEvent.AddUObject(this, &URunStateComponent::HandleLogout);
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
	FGameModeEvents::GameModeLogoutEvent.Remove(LogoutHandle);
	if (UTurnManager *Turns = WoRFight::Get<UTurnManager>(this))
	{
		Turns->OnCombatEnded.RemoveDynamic(this, &URunStateComponent::HandleCombatEndedForDrops);
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
				Entry.MemberId = Member.MemberId; // B-360b
			}
		}
	}
	Roster = MoveTemp(NewRoster);
	UE_LOG(LogWoR, Log, TEXT("[RunState] Roster: %d member(s)"), Roster.Num());

	// B-360b: a claim on a member that left the party (dismissed, replaced) is released, so that player can take a seat again.
	if (const AGameStateBase *GameState = GetOwner() && GetOwner()->HasAuthority() ? GetOwningGameState() : nullptr)
	{
		for (APlayerState *Player : GameState->PlayerArray)
		{
			AWoRPlayerState *WoRPlayer = Cast<AWoRPlayerState>(Player);
			if (!WoRPlayer)
			{
				continue;
			}
			TArray<FGuid> Stale;
			for (const FGuid &Claimed : WoRPlayer->GetClaimedMembers())
			{
				if (!Roster.ContainsByPredicate([&Claimed](const FRunRosterEntry &Entry) { return Entry.MemberId == Claimed; }))
				{
					Stale.Add(Claimed);
				}
			}
			for (const FGuid &Gone : Stale)
			{
				WoRPlayer->ReleaseMember(Gone);
			}
		}
	}
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
	PullRunProgress();
	RefreshRoster();
	ResetRotation();
	UE_LOG(LogWoR, Log, TEXT("[RunState] Pulled: run %s (seed %d), %d beaten, %d roster member(s)"), bHasRun ? TEXT("on") : TEXT("off"), RunSeed,
		   BeatenEncounters.Num(), Roster.Num());
}

void URunStateComponent::PullRunProgress()
{
	const UTrialRunSubsystem *TrialRun = RunStateTrialRunOf(this);
	if (!TrialRun || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	TrialRun->GetFoundFeatures().GetKeys(FoundFeatureIds);
	RunMemories = TrialRun->GetRunMemories();
	UnspentLevels.Reset();
	for (const TPair<FName, int32> &Pair : TrialRun->GetAllUnspentLevels())
	{
		FRunUnspentLevels &Entry = UnspentLevels.AddDefaulted_GetRef();
		Entry.CharacterKey = Pair.Key;
		Entry.Count = Pair.Value;
	}
}

void URunStateComponent::OnRep_RunProgress()
{
	// B-232d (review): a feature that checked the run at its BeginPlay, before this arrived, takes its state now.
	for (TActorIterator<ATrialMapFeature> It(GetWorld()); It; ++It)
	{
		It->RefreshFromRun();
	}
}

bool URunStateComponent::HasFoundFeature(const UObject *WorldContext, FName FeatureId)
{
	if (const URunStateComponent *Run = RunStateCopyFor(WorldContext))
	{
		return Run->FoundFeatureIds.Contains(FeatureId);
	}
	const UTrialRunSubsystem *TrialRun = RunStateTrialRunOf(WorldContext);
	return TrialRun && TrialRun->HasFound(FeatureId);
}

TArray<UMemoryData *> URunStateComponent::GetRunMemories(const UObject *WorldContext)
{
	if (const URunStateComponent *Run = RunStateCopyFor(WorldContext))
	{
		return TArray<UMemoryData *>(Run->RunMemories);
	}
	const UTrialRunSubsystem *TrialRun = RunStateTrialRunOf(WorldContext);
	return TrialRun ? TArray<UMemoryData *>(TrialRun->GetRunMemories()) : TArray<UMemoryData *>();
}

int32 URunStateComponent::GetUnspentLevels(const UObject *WorldContext, FName CharacterKey)
{
	if (const URunStateComponent *Run = RunStateCopyFor(WorldContext))
	{
		const FRunUnspentLevels *Entry = Run->UnspentLevels.FindByPredicate([CharacterKey](const FRunUnspentLevels &Each) { return Each.CharacterKey == CharacterKey; });
		return Entry ? Entry->Count : 0;
	}
	const UTrialRunSubsystem *TrialRun = RunStateTrialRunOf(WorldContext);
	return TrialRun ? TrialRun->GetUnspentLevels(CharacterKey) : 0;
}

UTrialData *URunStateComponent::GetActiveTrial(const UObject *WorldContext)
{
	if (const URunStateComponent *Run = RunStateCopyFor(WorldContext))
	{
		return Run->ActiveTrial;
	}
	const UTrialRunSubsystem *TrialRun = RunStateTrialRunOf(WorldContext);
	return TrialRun ? TrialRun->GetActiveTrial() : nullptr;
}

int32 URunStateComponent::GetRunSeed(const UObject *WorldContext)
{
	if (const URunStateComponent *Run = RunStateCopyFor(WorldContext))
	{
		return Run->RunSeed;
	}
	const UTrialRunSubsystem *TrialRun = RunStateTrialRunOf(WorldContext);
	return TrialRun ? TrialRun->GetRunSeed() : 0;
}

bool URunStateComponent::IsEncounterBeaten(const UObject *WorldContext, FName EncounterId)
{
	if (const URunStateComponent *Run = RunStateCopyFor(WorldContext))
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
	// B-360b: a battle pawn carries its member's id; two copies of one character are two entries.
	if (CharComp && CharComp->PartyMemberId.IsValid())
	{
		const FGuid MemberId = CharComp->PartyMemberId;
		if (const FRunRosterEntry *ById = Roster.FindByPredicate([&MemberId](const FRunRosterEntry &Entry) { return Entry.MemberId == MemberId; }))
		{
			return ById;
		}
	}
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

int32 URunStateComponent::GetClaimingPlayerId(const FRunRosterEntry &Entry) const
{
	if (const AGameStateBase *GameState = GetOwningGameState())
	{
		for (const APlayerState *Player : GameState->PlayerArray)
		{
			const AWoRPlayerState *WoRPlayer = Cast<AWoRPlayerState>(Player);
			// B-360b: a member claim first (a copy's own); the claim by character stands for a roster the party did not build.
			if (WoRPlayer && (WoRPlayer->ClaimsMember(Entry.MemberId) || (!Entry.MemberId.IsValid() && WoRPlayer->ClaimsCharacter(Entry.Character))))
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
	const int32 Claimed = GetClaimingPlayerId(*Entry);
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
	if (PartySession->IsFightRunning())
	{
		UE_LOG(LogWoR, Warning, TEXT("[RunState] Join refused for player %d: seats are fixed during a fight (Q-21-audit)"), Joiner->GetPlayerId());
		return false;
	}
	const AWoRPlayerState *WoRJoiner = Cast<AWoRPlayerState>(Joiner);
	if (WoRJoiner && WoRJoiner->HasAnyClaim())
	{
		UE_LOG(LogWoR, Warning, TEXT("[RunState] Join refused for player %d: it already has a seat"), Joiner->GetPlayerId());
		return false;
	}
	if (GetFreeAISeats().IsEmpty())
	{
		UE_LOG(LogWoR, Warning, TEXT("[RunState] Join refused for player %d: the party is full (no AI seat to take, D-150)"), Joiner->GetPlayerId());
		return false;
	}
	// TODO: co-op trust (B-233+): the client says this is its own character; the host does not check it against the client's save.
	// B-360b (D-150 point 2): a character already in the party is fine; the joiner's is a separate copy with its own slot and key.
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
	// The joiner first: a player who left after asking takes no seat (its request is dropped, nothing moves).
	AWoRPlayerState *JoinerState = nullptr;
	if (const AGameStateBase *GameState = GetOwningGameState())
	{
		for (APlayerState *Player : GameState->PlayerArray)
		{
			if (AWoRPlayerState *WoRPlayer = Cast<AWoRPlayerState>(Player); WoRPlayer && WoRPlayer->GetPlayerId() == PlayerId)
			{
				JoinerState = WoRPlayer;
			}
		}
	}
	if (!JoinerState)
	{
		JoinRequests.RemoveAt(Index);
		UE_LOG(LogWoR, Warning, TEXT("[RunState] AcceptJoin: player %d has left; its request is dropped"), PlayerId);
		return false;
	}
	const UWorld *World = GetWorld();
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
	const FGuid SeatMember = Party && Party->Members.IsValidIndex(ReplaceSlot) ? Party->Members[ReplaceSlot].MemberId : FGuid();
	// B-233b: a friend taking back the seat it left (held for its id) keeps its member and gear: no swap.
	const FString JoinerId = JoinerState->GetUniqueId().IsValid() ? JoinerState->GetUniqueId().ToString() : FString();
	const bool bOwnHeldSeat = SeatMember.IsValid() && !JoinerId.IsEmpty() && GetHeldFor(SeatMember) == JoinerId;
	if (!bOwnHeldSeat && !CanSeatNewJoinerNow())
	{
		UE_LOG(LogWoR, Warning, TEXT("[RunState] AcceptJoin: in a run, friends are seated at a rest spot (rest at one first, Q-233b)"));
		return false;
	}
	JoinRequests.RemoveAt(Index); // before the party broadcast, so the placeholder's refresh shows no stale line
	if (bOwnHeldSeat)
	{
		if (!PartySession->IsMemberAISeat(ReplaceSlot) || !PartySession->SetMemberAISeat(ReplaceSlot, false))
		{
			JoinRequests.Add(Request);
			return false;
		}
		HeldSeats.Remove(SeatMember);
		JoinerState->ClaimMember(SeatMember);
		RefreshRoster();
		UE_LOG(LogWoR, Log, TEXT("[RunState] Player %d took back its seat: slot %d (member %s)"), PlayerId, ReplaceSlot, *SeatMember.ToString(EGuidFormats::Short));
		return true;
	}
	if (!PartySession || !PartySession->ReplaceAISeatWithPlayer(ReplaceSlot, Request.PawnClass, Request.Character))
	{
		JoinRequests.Add(Request); // still waiting: the host may pick another seat
		return false;
	}
	HeldSeats.Remove(SeatMember); // another player took it: the hold is gone with the member
	// B-360b: the joiner commands the member it became (its copy), not every copy of that character.
	if (Party && Party->Members.IsValidIndex(ReplaceSlot))
	{
		JoinerState->ClaimMember(Party->Members[ReplaceSlot].MemberId);
	}
	RefreshRoster();
	UE_LOG(LogWoR, Log, TEXT("[RunState] Player %d joined: %s took slot %d"), PlayerId, *GetNameSafe(Request.Character), ReplaceSlot);
	return true;
}

bool URunStateComponent::RefuseJoin(int32 PlayerId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}
	const int32 Removed = JoinRequests.RemoveAll([PlayerId](const FRunJoinRequest &R) { return R.PlayerId == PlayerId; });
	if (Removed > 0)
	{
		UE_LOG(LogWoR, Log, TEXT("[RunState] Player %d's join request refused by the host"), PlayerId);
	}
	return Removed > 0;
}

bool URunStateComponent::CanSeatNewJoinerNow() const
{
	const UTrialRunSubsystem *Run = RunStateTrialRunOf(this);
	return RunJoinRules::CanSeatJoiner(Run && Run->IsRunActive(), Run && Run->IsAtRestSpot(), /*bOwnHeldSeat=*/false);
}

FString URunStateComponent::GetHeldFor(const FGuid &MemberId) const
{
	const FString *Held = HeldSeats.Find(MemberId);
	return Held ? *Held : FString();
}

void URunStateComponent::HandleLogout(AGameModeBase *GameMode, AController *Exiting)
{
	const APlayerController *PC = Cast<APlayerController>(Exiting);
	// Only this world's remote players: the host's own controller and other worlds' (PIE) are not a friend leaving.
	if (!PC || PC->IsLocalController() || PC->GetWorld() != GetWorld())
	{
		return;
	}
	HandlePlayerLeft(PC->PlayerState);
}

void URunStateComponent::HandlePlayerLeft(APlayerState *Leaver)
{
	const AWoRPlayerState *WoRLeaver = Cast<AWoRPlayerState>(Leaver);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !WoRLeaver)
	{
		return;
	}
	const int32 LeaverId = WoRLeaver->GetPlayerId();
	JoinRequests.RemoveAll([LeaverId](const FRunJoinRequest &R) { return R.PlayerId == LeaverId; });
	const FString HeldFor = WoRLeaver->GetUniqueId().IsValid() ? WoRLeaver->GetUniqueId().ToString() : FString();
	const UWorld *World = GetWorld();
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	const bool bFighting = PartySession && PartySession->IsFightRunning();
	for (const FGuid &MemberId : WoRLeaver->GetClaimedMembers())
	{
		if (!HeldFor.IsEmpty())
		{
			HeldSeats.Add(MemberId, HeldFor);
		}
		if (bFighting)
		{
			// Seats are fixed during a fight (Q-21-audit): the host commands the member (unclaimed) until it ends.
			PendingSeatDrops.AddUnique(MemberId);
		}
		else
		{
			DropToAISeat(MemberId);
		}
	}
	if (!PendingSeatDrops.IsEmpty())
	{
		if (UTurnManager *Turns = WoRFight::Get<UTurnManager>(this))
		{
			Turns->OnCombatEnded.AddUniqueDynamic(this, &URunStateComponent::HandleCombatEndedForDrops);
		}
	}
	UE_LOG(LogWoR, Log, TEXT("[RunState] Player %d (%s) left: %d member(s) to the AI%s"), LeaverId, *WoRLeaver->GetPlayerName(),
		   WoRLeaver->GetClaimedMembers().Num(), bFighting ? TEXT(" when this fight ends (the host commands them until then)") : TEXT(""));
	RefreshRoster();
}

void URunStateComponent::HandleCombatEndedForDrops(int32 /*FinalTurnCount*/)
{
	if (UTurnManager *Turns = WoRFight::Get<UTurnManager>(this))
	{
		Turns->OnCombatEnded.RemoveDynamic(this, &URunStateComponent::HandleCombatEndedForDrops);
	}
	// The fight reads as running until EndCombat returns (it broadcasts first): drop the seats on the next tick.
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &URunStateComponent::ApplyPendingSeatDrops));
	}
}

void URunStateComponent::ApplyPendingSeatDrops()
{
	TArray<FGuid> Drops = MoveTemp(PendingSeatDrops);
	PendingSeatDrops.Reset();
	for (const FGuid &MemberId : Drops)
	{
		if (!DropToAISeat(MemberId))
		{
			UE_LOG(LogWoR, Warning, TEXT("[RunState] Member %s stays the host's: its seat could not turn AI"), *MemberId.ToString(EGuidFormats::Short));
		}
	}
	RefreshRoster();
}

bool URunStateComponent::DropToAISeat(const FGuid &MemberId)
{
	const UWorld *World = GetWorld();
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
	const int32 Slot = Party ? Party->Members.IndexOfByPredicate([&MemberId](const FPartyMember &M) { return M.MemberId == MemberId; }) : INDEX_NONE;
	if (Slot == INDEX_NONE)
	{
		return true; // gone from the party already: nothing to drop
	}
	// The leader is never an AI seat (Q-21-audit): a leader's player leaving leaves it to the host (unclaimed).
	return Slot == PartyConstants::LEADER_SLOT || PartySession->SetMemberAISeat(Slot, true);
}

FString URunStateComponent::GetDebugString() const
{
	FString Out = FString::Printf(TEXT("[RunState] run %s: trial %s, seed %d, variation %d, %d beaten; %d player(s)"),
								  bHasRun ? TEXT("on") : TEXT("off"), *GetNameSafe(ActiveTrial), RunSeed, VariationIndex,
								  BeatenEncounters.Num(), GetSidePlayerIds().Num());
	for (const FRunRosterEntry &Entry : Roster)
	{
		const int32 Claimed = GetClaimingPlayerId(Entry);
		Out += FString::Printf(TEXT("\n  %s (member %s): %s"), *GetNameSafe(Entry.Character), *Entry.MemberId.ToString(EGuidFormats::Short),
							   Entry.bAISeat ? TEXT("AI seat")
							   : Claimed != SeatRules::UNCLAIMED ? *FString::Printf(TEXT("player %d's"), Claimed)
																 : TEXT("unclaimed (the host's, D-150)"));
	}
	for (const FRunJoinRequest &Request : JoinRequests)
	{
		Out += FString::Printf(TEXT("\n  join request: player %d with %s"), Request.PlayerId, *GetNameSafe(Request.Character));
	}
	// B-233b
	Out += FString::Printf(TEXT("\n  seating new joiners: %s"), CanSeatNewJoinerNow() ? TEXT("open") : TEXT("closed (in a run, away from a rest spot)"));
	for (const TPair<FGuid, FString> &Held : HeldSeats)
	{
		Out += FString::Printf(TEXT("\n  seat held: member %s for %s"), *Held.Key.ToString(EGuidFormats::Short), *Held.Value);
	}
	for (const FGuid &MemberId : PendingSeatDrops)
	{
		Out += FString::Printf(TEXT("\n  to the AI when this fight ends: member %s"), *MemberId.ToString(EGuidFormats::Short));
	}
	return Out;
}
