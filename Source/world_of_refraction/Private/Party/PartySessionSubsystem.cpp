// PartySessionSubsystem.cpp

#include "Party/PartySessionSubsystem.h"

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Combat/Grid/CombatGridConstants.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Party/Party.h"
#include "Pool/PoolSubsystem.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace PartySessionDebug
{
	UPartySessionSubsystem *FindPartySession(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	}

	void SetControl(const TArray<FString> &Args, UWorld *World)
	{
		UPartySessionSubsystem *PartySession = FindPartySession(World);
		if (!PartySession || Args.Num() < 2)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PartySession] Usage: wor.Party.SetControl <slot> <Player|AI>"));
			return;
		}
		PartySession->SetMemberPlayerControlled(FCString::Atoi(*Args[0]), !Args[1].Equals(TEXT("AI"), ESearchCase::IgnoreCase));
		PartySession->LogPartyState();
	}

	void SetLetAIFinish(const TArray<FString> &Args, UWorld *World)
	{
		UPartySessionSubsystem *PartySession = FindPartySession(World);
		if (!PartySession || Args.Num() < 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PartySession] Usage: wor.Party.LetAIFinish <0|1>"));
			return;
		}
		PartySession->SetLetAIFinish(FCString::Atoi(*Args[0]) != 0);
	}

	void SetFormation(const TArray<FString> &Args, UWorld *World)
	{
		UPartySessionSubsystem *PartySession = FindPartySession(World);
		const UEnum *RowEnum = StaticEnum<ECombatRow>();
		const int64 RowValue = Args.Num() >= 3 && RowEnum ? RowEnum->GetValueByNameString(Args[1]) : INDEX_NONE;
		if (!PartySession || RowValue == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PartySession] Usage: wor.Party.SetFormation <slot> <Back|Middle|Front> <column 0-2>"));
			return;
		}
		PartySession->SetMemberFormation(FCString::Atoi(*Args[0]), static_cast<ECombatRow>(RowValue), FCString::Atoi(*Args[2]));
		PartySession->LogPartyState();
	}

	void ReviveFallen(UWorld *World)
	{
		if (UPartySessionSubsystem *PartySession = FindPartySession(World))
		{
			PartySession->ReviveFallenMembers();
			PartySession->LogPartyState();
		}
	}

	void RecruitForRun(const TArray<FString> &Args, UWorld *World)
	{
		UPartySessionSubsystem *PartySession = FindPartySession(World);
		UCharacterData *Data = Args.Num() >= 1 ? LoadObject<UCharacterData>(nullptr, *Args[0]) : nullptr;
		const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
		if (!Data || !Party || Party->Members.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PartySession] Usage: wor.Party.RecruitForRun <CharacterData path> (needs a party)"));
			return;
		}
		// The leader's pawn class carries the found character until trial maps place their own.
		PartySession->InviteRunMember(Party->Members[PartyConstants::LEADER_SLOT].PawnClass, Data);
		PartySession->LogPartyState();
	}

	void Print(UWorld *World)
	{
		if (const UPartySessionSubsystem *PartySession = FindPartySession(World))
		{
			PartySession->LogPartyState();
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GPartySetControlCommand(
	TEXT("wor.Party.SetControl"),
	TEXT("wor.Party.SetControl <slot> <Player|AI> - who plays a party member in battle (D-004; the leader is always Player)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PartySessionDebug::SetControl));

static FAutoConsoleCommandWithWorldAndArgs GPartyLetAIFinishCommand(
	TEXT("wor.Party.LetAIFinish"),
	TEXT("wor.Party.LetAIFinish <0|1> - 1: the AI finishes a fight once your Player members are down; 0: you take over (D-045)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PartySessionDebug::SetLetAIFinish));

static FAutoConsoleCommandWithWorldAndArgs GPartySetFormationCommand(
	TEXT("wor.Party.SetFormation"),
	TEXT("wor.Party.SetFormation <slot> <Back|Middle|Front> <column 0-2> - the member's start cell (D-042); a taken cell swaps the two."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PartySessionDebug::SetFormation));

static FAutoConsoleCommandWithWorld GPartyReviveFallenCommand(
	TEXT("wor.Party.ReviveFallen"),
	TEXT("wor.Party.ReviveFallen - a rest spot's revive (D-043): fallen members come back at full HP/EP."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&PartySessionDebug::ReviveFallen));

static FAutoConsoleCommandWithWorldAndArgs GPartyRecruitForRunCommand(
	TEXT("wor.Party.RecruitForRun"),
	TEXT("wor.Party.RecruitForRun <CharacterData path> - a character found in the trial joins for this run only, ungeared (B-076, D-026)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PartySessionDebug::RecruitForRun));

static FAutoConsoleCommandWithWorld GPartyPrintCommand(
	TEXT("wor.Party.Print"),
	TEXT("Print the local party: members, formation, carried vitals, control."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&PartySessionDebug::Print));

void UPartySessionSubsystem::Deinitialize()
{
	Parties.Empty();
	Super::Deinitialize();
}

UParty *UPartySessionSubsystem::GetLocalParty() const
{
	return Parties.IsValidIndex(0) ? Parties[0].Get() : nullptr;
}

UParty *UPartySessionSubsystem::CreateSoloParty(APlayerController *Leader, TSoftClassPtr<ACombatCharacter> LeaderPawnClass)
{
	// Re-entry (hub reload) rebinds rather than rebuilds — a party assembled
	// earlier this session must survive returning to the hub.
	if (UParty *Existing = GetLocalParty())
	{
		RebindLeader(Leader);
		UE_LOG(LogTemp, Log, TEXT("[PartySession] Local party already exists (%d members) - leader rebound"),
			   Existing->GetMemberCount());
		return Existing;
	}

	// ⚠️ Outer is the GameInstance, NOT the world: a world-outered UObject is
	// destroyed by OpenLevel and the party would empty on the first transition.
	UParty *NewParty = NewObject<UParty>(GetGameInstance());
	NewParty->SetLeader(Leader);
	NewParty->DisplayName = MakeDefaultDisplayName(Leader);
	Parties.Add(NewParty);

	// Internal add: the creation broadcast below covers the seeded member too, so
	// one logical operation fires one delegate.
	if (!LeaderPawnClass.IsNull())
	{
		InviteMemberInternal(LeaderPawnClass);
	}

	UE_LOG(LogTemp, Log, TEXT("[PartySession] Created solo party '%s' (%d members)"),
		   *NewParty->DisplayName.ToString(), NewParty->GetMemberCount());

	// Broadcast after all initial state is set — subscribers see a valid party,
	// not a half-built one.
	OnPartyChanged.Broadcast(NewParty);
	return NewParty;
}

UParty *UPartySessionSubsystem::RestoreLocalParty(const FText &DisplayName, bool bLetAIFinish,
												  const TArray<FPartyMember> &Members)
{
	if (GetLocalParty() || Members.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PartySession] Saved party not restored: %s"),
			   GetLocalParty() ? TEXT("a local party already exists") : TEXT("no members"));
		return nullptr;
	}

	// GameInstance outer, as in CreateSoloParty: the party must outlive OpenLevel.
	UParty *Restored = NewObject<UParty>(GetGameInstance());
	Restored->DisplayName = DisplayName;
	Restored->bLetAIFinish = bLetAIFinish;
	Restored->Members = Members;
	if (Restored->Members.Num() > PartyConstants::MAX_PARTY_MEMBERS)
	{
		Restored->Members.SetNum(PartyConstants::MAX_PARTY_MEMBERS);
	}
	// D-004: the leader is always Player-controlled.
	Restored->Members[0].bPlayerControlled = true;
	Parties.Add(Restored);

	UE_LOG(LogTemp, Log, TEXT("[PartySession] Restored saved party '%s' (%d members)"),
		   *Restored->DisplayName.ToString(), Restored->GetMemberCount());
	OnPartyChanged.Broadcast(Restored);
	return Restored;
}

UParty *UPartySessionSubsystem::EnsureLocalParty(APlayerController *PC, TSoftClassPtr<ACombatCharacter> PawnClass)
{
	if (UParty *Existing = GetLocalParty())
	{
		RebindLeader(PC);
		return Existing;
	}

	// Resolution chain. PC's current pawn is the common case: the player is
	// standing in the world as their character when this fires.
	TSoftClassPtr<ACombatCharacter> Resolved = PawnClass;
	if (Resolved.IsNull() && PC)
	{
		if (const APawn *CurrentPawn = PC->GetPawn())
		{
			Resolved = TSoftClassPtr<ACombatCharacter>(CurrentPawn->GetClass()->GetPathName());
		}
	}
	if (Resolved.IsNull())
	{
		Resolved = DefaultSoloPawnClass;
	}
	if (Resolved.IsNull())
	{
		UE_LOG(LogTemp, Error,
			   TEXT("[PartySession] Cannot create Trial Party - no pawn class from argument, PC0 pawn, or "
					"DefaultSoloPawnClass (author it in DefaultGame.ini)"));
		return nullptr;
	}

	UParty *Party = CreateSoloParty(PC, Resolved);
	UE_LOG(LogTemp, Log, TEXT("[PartySession] Trial Party lazily created with %d member(s) from %s"),
		   Party ? Party->GetMemberCount() : 0, *Resolved.ToString());
	return Party;
}

bool UPartySessionSubsystem::IsTrialPartyMember(const AActor *Actor) const
{
	if (!Actor)
	{
		return false;
	}

	// CharacterData comparison, not pawn class: a level-placed instance may override
	// its CharacterData away from the Blueprint CDO, and the overridden asset is the
	// character it actually is.
	const UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (!Comp || !Comp->CharacterData)
	{
		return false;
	}
	return GetMemberSlotByData(Comp->CharacterData) != INDEX_NONE;
}

int32 UPartySessionSubsystem::GetMemberSlotByData(UCharacterData *CharacterData) const
{
	const UParty *Party = GetLocalParty();
	if (!CharacterData || !Party)
	{
		return INDEX_NONE;
	}

	// Pointer equality is correct: the same asset path resolves to one UObject
	// in memory, and FPartyMember pins it with a hard ref.
	for (int32 SlotIndex = 0; SlotIndex < Party->Members.Num(); ++SlotIndex)
	{
		if (Party->Members[SlotIndex].CharacterData == CharacterData)
		{
			return SlotIndex;
		}
	}
	return INDEX_NONE;
}

bool UPartySessionSubsystem::InviteMember(TSoftClassPtr<ACombatCharacter> PawnClass)
{
	const bool bAdded = InviteMemberInternal(PawnClass);
	if (bAdded)
	{
		OnPartyChanged.Broadcast(GetLocalParty());
	}
	return bAdded;
}

bool UPartySessionSubsystem::InviteMemberWithData(TSoftClassPtr<ACombatCharacter> PawnClass, UCharacterData *Data)
{
	if (!Data)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PartySession] InviteMemberWithData with no CharacterData - invite rejected"));
		return false;
	}
	const bool bAdded = InviteMemberInternal(PawnClass, Data);
	if (bAdded)
	{
		OnPartyChanged.Broadcast(GetLocalParty());
	}
	return bAdded;
}

bool UPartySessionSubsystem::InviteMemberInternal(TSoftClassPtr<ACombatCharacter> PawnClass, UCharacterData *DataOverride)
{
	UParty *Party = GetLocalParty();
	if (!Party)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PartySession] InviteMember with no local party - call CreateSoloParty first"));
		return false;
	}

	if (Party->IsFull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PartySession] Party full (%d) - invite rejected"),
			   PartyConstants::MAX_PARTY_MEMBERS);
		return false;
	}

	if (PawnClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PartySession] InviteMember with null pawn class"));
		return false;
	}

	UCharacterData *Data = DataOverride ? DataOverride : ResolveCharacterData(PawnClass);
	if (!Data)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PartySession] %s carries no CharacterData - invite rejected"),
			   *PawnClass.ToString());
		return false;
	}

	// UNIQUENESS: CharacterData IS the identity, so two members sharing one asset
	// collapse to a single slot in GetMemberSlotByData — which returns the FIRST
	// match. Downstream that breaks two things at once: the vitals carry (both
	// pawns read slot N's HP/EP in UCharacterDataComponent::BeginPlay) and the
	// formation stamp (both pawns resolve to one PreferredRow/Column and spawn in
	// the same cell). Rejecting the duplicate here makes the "first match"
	// contract sound for every downstream consumer, rather than each one needing
	// its own guard.
	//
	// This deliberately forecloses a duplicate-lord party. Per-instance identity
	// — the banked PvP arc — is where this guard gets lifted; until then, slot
	// identity and asset identity are the same thing by construction.
	const int32 ExistingSlot = GetMemberSlotByData(Data);
	if (ExistingSlot != INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("[PartySession] %s is already in the party at slot %d - duplicate invite rejected"),
			   *Data->Name, ExistingSlot);
		return false;
	}

	FPartyMember Member;
	Member.PawnClass = PawnClass;
	Member.CharacterData = Data;
	Party->Members.Add(Member);

	UE_LOG(LogTemp, Log, TEXT("[PartySession] Invited %s (%d/%d)"),
		   *Data->Name, Party->GetMemberCount(), PartyConstants::MAX_PARTY_MEMBERS);
	return true;
}

bool UPartySessionSubsystem::DismissMember(int32 SlotIndex)
{
	if (SlotIndex == 0)
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("[PartySession] Refused to dismiss slot 0 (party leader) - leader is protected. "
					"Use RebindLeader to change leadership."));
		return false;
	}

	UParty *Party = GetLocalParty();
	if (!Party || !Party->Members.IsValidIndex(SlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PartySession] DismissMember: invalid slot %d"), SlotIndex);
		return false;
	}

	Party->Members.RemoveAt(SlotIndex);
	UE_LOG(LogTemp, Log, TEXT("[PartySession] Dismissed slot %d (%d remaining)"),
		   SlotIndex, Party->GetMemberCount());

	OnPartyChanged.Broadcast(Party);
	return true;
}

bool UPartySessionSubsystem::InviteRunMember(TSoftClassPtr<ACombatCharacter> PawnClass, UCharacterData *Data)
{
	// A run-only member never leads: DismissRunMembers would take the leader with it.
	const UParty *Party = GetLocalParty();
	if (!Data || !Party || Party->Members.Num() == 0 || !InviteMemberInternal(PawnClass, Data))
	{
		return false;
	}
	FPartyMember &Member = GetLocalParty()->Members.Last();
	Member.bRunOnly = true;
	// The rest of the party was seeded full at trial entry; the newcomer joins the same way.
	SetMemberVitalsFull(Member);
	UE_LOG(LogTemp, Log, TEXT("[PartySession] %s joins for this run only, ungeared"), *Data->Name);
	OnPartyChanged.Broadcast(GetLocalParty());
	return true;
}

int32 UPartySessionSubsystem::DismissRunMembers()
{
	UParty *Party = GetLocalParty();
	if (UPoolSubsystem *Pool = Party ? GetGameInstance()->GetSubsystem<UPoolSubsystem>() : nullptr)
	{
		// What a leaver wore is free again, and its clash lines go with it.
		for (const FPartyMember &Member : Party->Members)
		{
			if (!Member.bRunOnly)
			{
				continue;
			}
			const FName Key = UPoolSubsystem::GetCharacterKey(Member.CharacterData);
			TArray<FGuid> Worn;
			for (const TPair<FGuid, FName> &Pair : Pool->GetWearers())
			{
				if (Pair.Value == Key)
				{
					Worn.Add(Pair.Key);
				}
			}
			for (const FGuid &GearID : Worn)
			{
				Pool->ReleaseWearer(GearID);
			}
			Pool->NoteClashes(Key, {});
		}
	}
	const int32 Left = Party ? Party->Members.RemoveAll([](const FPartyMember &Member) { return Member.bRunOnly; }) : 0;
	if (Left > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[PartySession] The run is over: %d run-only member(s) leave"), Left);
		OnPartyChanged.Broadcast(Party);
	}
	return Left;
}

bool UPartySessionSubsystem::SetRunMemberGear(int32 SlotIndex, const FGuid &Weapon, const FGuid &Ring)
{
	UParty *Party = GetLocalParty();
	if (!Party || !Party->Members.IsValidIndex(SlotIndex) || !Party->Members[SlotIndex].bRunOnly)
	{
		return false;
	}
	Party->Members[SlotIndex].RunWeapon = Weapon;
	Party->Members[SlotIndex].RunRing = Ring;
	OnPartyChanged.Broadcast(Party);
	return true;
}

void UPartySessionSubsystem::SetDisplayName(const FText &NewName)
{
	if (UParty *Party = GetLocalParty())
	{
		Party->DisplayName = NewName;
		// Broadcast so a bound title refreshes. Composition is unchanged, but
		// OnPartyChanged is the one "re-read the party" signal consumers have.
		OnPartyChanged.Broadcast(Party);
	}
}

void UPartySessionSubsystem::SetMemberFormation(int32 SlotIndex, ECombatRow Row, int32 Column)
{
	UParty *Party = GetLocalParty();
	if (!Party || !Party->Members.IsValidIndex(SlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PartySession] SetMemberFormation: invalid slot %d"), SlotIndex);
		return;
	}

	if (Column < 0 || Column >= CombatGridConstants::GRID_COLUMNS)
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("[PartySession] SetMemberFormation: column %d out of range [0, %d) - slot %d unchanged"),
			   Column, CombatGridConstants::GRID_COLUMNS, SlotIndex);
		return;
	}

	FPartyMember &Member = Party->Members[SlotIndex];

	// Already there: nothing moved, so nothing to broadcast.
	if (Member.HasFormation() && Member.PreferredRow == Row && Member.PreferredColumn == Column)
	{
		return;
	}

	// Captured BEFORE the write — this is what any displaced member inherits.
	const ECombatRow PreviousRow = Member.PreferredRow;
	const int32 PreviousColumn = Member.PreferredColumn;

	// At most one member can hold the target cell: this loop is the only writer of
	// a placed cell, and it maintains that invariant on every pass. Unplaced
	// members are skipped via HasFormation() — UNSET_FORMATION is not a cell, so
	// any number of members may share it.
	for (int32 OtherIndex = 0; OtherIndex < Party->Members.Num(); ++OtherIndex)
	{
		if (OtherIndex == SlotIndex)
		{
			continue;
		}

		FPartyMember &Other = Party->Members[OtherIndex];
		if (Other.HasFormation() && Other.PreferredRow == Row && Other.PreferredColumn == Column)
		{
			Other.PreferredRow = PreviousRow;
			Other.PreferredColumn = PreviousColumn;
			UE_LOG(LogTemp, Log, TEXT("[PartySession] Formation swap: slot %d <-> slot %d"),
				   SlotIndex, OtherIndex);
			break;
		}
	}

	Member.PreferredRow = Row;
	Member.PreferredColumn = Column;

	UE_LOG(LogTemp, Log, TEXT("[PartySession] Slot %d placed at %s row, column %d"),
		   SlotIndex, *CombatRowHelpers::GetRowName(Row), Column);

	OnPartyChanged.Broadcast(Party);
}

bool UPartySessionSubsystem::GetMemberFormation(int32 SlotIndex, ECombatRow &OutRow, int32 &OutColumn) const
{
	const UParty *Party = GetLocalParty();
	if (!Party || !Party->Members.IsValidIndex(SlotIndex))
	{
		return false;
	}

	const FPartyMember &Member = Party->Members[SlotIndex];
	if (!Member.HasFormation())
	{
		return false;
	}

	OutRow = Member.PreferredRow;
	OutColumn = Member.PreferredColumn;
	return true;
}

bool UPartySessionSubsystem::SetMemberPlayerControlled(int32 SlotIndex, bool bPlayerControlled)
{
	UParty *Party = GetLocalParty();
	if (!Party || !Party->Members.IsValidIndex(SlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PartySession] SetMemberPlayerControlled: invalid slot %d"), SlotIndex);
		return false;
	}
	if (SlotIndex == PartyConstants::LEADER_SLOT && !bPlayerControlled)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PartySession] Refused to hand the leader to the AI - the leader is always Player (D-004)"));
		return false;
	}

	FPartyMember &Member = Party->Members[SlotIndex];
	if (Member.bPlayerControlled != bPlayerControlled)
	{
		Member.bPlayerControlled = bPlayerControlled;
		UE_LOG(LogTemp, Log, TEXT("[PartySession] Slot %d now %s-controlled"), SlotIndex,
			   bPlayerControlled ? TEXT("Player") : TEXT("AI"));
		OnPartyChanged.Broadcast(Party);
	}
	return true;
}

bool UPartySessionSubsystem::IsMemberPlayerControlled(int32 SlotIndex) const
{
	const UParty *Party = GetLocalParty();
	return !Party || !Party->Members.IsValidIndex(SlotIndex) || Party->Members[SlotIndex].bPlayerControlled;
}

void UPartySessionSubsystem::SetLetAIFinish(bool bLetAIFinish)
{
	if (UParty *Party = GetLocalParty(); Party && Party->bLetAIFinish != bLetAIFinish)
	{
		Party->bLetAIFinish = bLetAIFinish;
		UE_LOG(LogTemp, Log, TEXT("[PartySession] When your fighters fall: %s"),
			   bLetAIFinish ? TEXT("let AI finish") : TEXT("take over"));
		OnPartyChanged.Broadcast(Party);
	}
}

bool UPartySessionSubsystem::GetLetAIFinish() const
{
	const UParty *Party = GetLocalParty();
	return Party && Party->bLetAIFinish;
}

void UPartySessionSubsystem::RebindLeader(APlayerController *Leader)
{
	UParty *Party = GetLocalParty();
	if (!Party)
	{
		return;
	}

	Party->SetLeader(Leader);
	// Only re-derive the name while it is still the generated default — an
	// explicit SetDisplayName must not be clobbered by a level transition.
	if (Party->DisplayName.IsEmpty())
	{
		Party->DisplayName = MakeDefaultDisplayName(Leader);
	}
}

void UPartySessionSubsystem::SetMemberVitals(int32 SlotIndex, int32 HP, int32 EP)
{
	UParty *Party = GetLocalParty();
	if (!Party || !Party->Members.IsValidIndex(SlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PartySession] SetMemberVitals: invalid slot %d"), SlotIndex);
		return;
	}

	FPartyMember &Member = Party->Members[SlotIndex];
	Member.CurrentHP = HP;
	Member.CurrentEP = EP;
}

void UPartySessionSubsystem::SetMemberCarriedState(int32 SlotIndex, const FCarriedGearState &Gear, const FCarriedAbsorptionState &Absorption)
{
	UParty *Party = GetLocalParty();
	if (!Party || !Party->Members.IsValidIndex(SlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PartySession] SetMemberCarriedState: invalid slot %d"), SlotIndex);
		return;
	}
	Party->Members[SlotIndex].CarriedGear = Gear;
	Party->Members[SlotIndex].CarriedAbsorption = Absorption;
}

const FPartyMember *UPartySessionSubsystem::GetMemberCarriedState(int32 SlotIndex) const
{
	const UParty *Party = GetLocalParty();
	return Party && Party->Members.IsValidIndex(SlotIndex) ? &Party->Members[SlotIndex] : nullptr;
}

bool UPartySessionSubsystem::GetMemberVitals(int32 SlotIndex, int32 &OutHP, int32 &OutEP) const
{
	const UParty *Party = GetLocalParty();
	if (!Party || !Party->Members.IsValidIndex(SlotIndex))
	{
		return false;
	}

	const FPartyMember &Member = Party->Members[SlotIndex];
	if (!Member.HasCarriedVitals())
	{
		return false;
	}

	OutHP = Member.CurrentHP;
	OutEP = Member.CurrentEP;
	return true;
}

void UPartySessionSubsystem::SetAllMembersFull()
{
	UParty *Party = GetLocalParty();
	if (!Party)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PartySession] SetAllMembersFull with no local party"));
		return;
	}

	for (FPartyMember &Member : Party->Members)
	{
		SetMemberVitalsFull(Member);
		Member.CarriedGear = FCarriedGearState();
		Member.CarriedAbsorption = FCarriedAbsorptionState();
	}

	UE_LOG(LogTemp, Log, TEXT("[PartySession] Vitals seeded to full for %d member(s)"), Party->GetMemberCount());
}

void UPartySessionSubsystem::SetMemberVitalsFull(FPartyMember &Member)
{
	if (!Member.CharacterData)
	{
		return;
	}

	// COMPUTED accessors, not the raw MaxHealth/MaxEnergy sub-stats — those are
	// inputs to the calculation, not the resulting pool sizes.
	Member.CurrentHP = Member.CharacterData->CalculateMaxHealth();
	Member.CurrentEP = Member.CharacterData->bBrokenDarknessInnate
						   ? 0
						   : Member.CharacterData->CalculateMaxEnergy();
}

int32 UPartySessionSubsystem::ReviveFallenMembers()
{
	UParty *Party = GetLocalParty();
	if (!Party)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PartySession] ReviveFallenMembers with no local party"));
		return 0;
	}

	int32 RevivedCount = 0;
	for (FPartyMember &Member : Party->Members)
	{
		if (Member.IsFallen() && Member.CharacterData)
		{
			SetMemberVitalsFull(Member);
			++RevivedCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[PartySession] Revived %d fallen member(s)"), RevivedCount);
	if (RevivedCount > 0)
	{
		OnPartyChanged.Broadcast(Party); // the party HUD re-reads vitals on it
	}
	return RevivedCount;
}

bool UPartySessionSubsystem::IsMemberFallen(int32 SlotIndex) const
{
	const UParty *Party = GetLocalParty();
	return Party && Party->Members.IsValidIndex(SlotIndex) && Party->Members[SlotIndex].IsFallen();
}

void UPartySessionSubsystem::ClearAllMemberVitals()
{
	UParty *Party = GetLocalParty();
	if (!Party)
	{
		return;
	}

	for (FPartyMember &Member : Party->Members)
	{
		Member.CurrentHP = PartyConstants::NO_CARRIED_VITALS;
		Member.CurrentEP = PartyConstants::NO_CARRIED_VITALS;
		Member.CarriedGear = FCarriedGearState();
		Member.CarriedAbsorption = FCarriedAbsorptionState();
	}

	UE_LOG(LogTemp, Log, TEXT("[PartySession] Carried vitals cleared for %d member(s)"), Party->GetMemberCount());
}

FString UPartySessionSubsystem::GetPartyStateString() const
{
	const UParty *Party = GetLocalParty();
	if (!Party)
	{
		return TEXT("[PartySession] No local party");
	}

	FString Out = FString::Printf(TEXT("[PartySession] '%s' - %d/%d member(s), leader %s\n"),
								  *Party->DisplayName.ToString(), Party->GetMemberCount(),
								  PartyConstants::MAX_PARTY_MEMBERS,
								  Party->Leader.IsValid() ? *Party->Leader->GetName() : TEXT("<stale>"));

	for (int32 SlotIndex = 0; SlotIndex < Party->Members.Num(); ++SlotIndex)
	{
		const FPartyMember &Member = Party->Members[SlotIndex];
		const FString MemberName = Member.CharacterData ? Member.CharacterData->Name : TEXT("<no data>");

		// Formation reads "unplaced" rather than a cell when the member falls
		// through to the centre-out auto-assignment at spawn.
		const FString Formation =
			Member.HasFormation()
				? FString::Printf(TEXT("%s/%d"), *CombatRowHelpers::GetRowName(Member.PreferredRow),
								  Member.PreferredColumn)
				: FString(TEXT("unplaced"));
		FString ControlLabel = Member.bPlayerControlled ? TEXT("Player") : TEXT("AI");
		if (Member.bRunOnly)
		{
			ControlLabel += TEXT(", run only");
		}
		const TCHAR *Control = *ControlLabel;

		if (!Member.HasCarriedVitals())
		{
			// No carry: show what display would fall back to, so the two cases are
			// comparable at a glance.
			const int32 FallbackHP = Member.CharacterData ? Member.CharacterData->CalculateMaxHealth() : 0;
			const int32 FallbackEP = Member.CharacterData ? Member.CharacterData->CalculateMaxEnergy() : 0;
			Out += FString::Printf(TEXT("  [%d] %s - no carry (falls back to HP %d / EP %d) - formation %s - %s\n"),
								   SlotIndex, *MemberName, FallbackHP, FallbackEP, *Formation, Control);
		}
		else
		{
			Out += FString::Printf(TEXT("  [%d] %s - carried HP %d / EP %d%s - formation %s - %s\n"),
								   SlotIndex, *MemberName, Member.CurrentHP, Member.CurrentEP,
								   Member.IsFallen() ? TEXT(" (FALLEN - sits out until revived)") : TEXT(""),
								   *Formation, Control);
		}
		// B-115: the crystal and absorption state the next fight starts with.
		if (Member.CarriedGear.bCarried)
		{
			FString Durabilities;
			for (const FCarriedAttachment &Attachment : Member.CarriedGear.Attachments)
			{
				Durabilities += Attachment.Attached.IsEmpty()
									? FString(TEXT(" empty"))
									: FString::Printf(TEXT(" %d"), Attachment.Attached.GetCurrentDurability());
			}
			Out += FString::Printf(TEXT("      carried crystals:%s%s\n"), *Durabilities,
								   Member.CarriedGear.bHasPrimaryEvolution
									   ? *FString::Printf(TEXT(" - evolution %d"), Member.CarriedGear.PrimaryEvolution.CurrentDurability)
									   : TEXT(""));
		}
		if (Member.CarriedAbsorption.bCarried)
		{
			Out += FString::Printf(TEXT("      carried absorption: %d stacks, overload %d EP\n"),
								   Member.CarriedAbsorption.AbsorptionStacks, Member.CarriedAbsorption.OverloadEP);
		}
	}
	Out += FString::Printf(TEXT("  When your fighters fall: %s\n"),
						   Party->bLetAIFinish ? TEXT("let AI finish") : TEXT("take over"));
	return Out;
}

void UPartySessionSubsystem::LogPartyState() const
{
	UE_LOG(LogTemp, Log, TEXT("%s"), *GetPartyStateString());
}

UCharacterData *UPartySessionSubsystem::ResolveCharacterData(const TSoftClassPtr<ACombatCharacter> &PawnClass)
{
	UClass *Loaded = PawnClass.LoadSynchronous();
	if (!Loaded)
	{
		return nullptr;
	}

	const ACombatCharacter *DefaultPawn = Loaded->GetDefaultObject<ACombatCharacter>();
	if (!DefaultPawn || !DefaultPawn->CharacterDataComponent)
	{
		return nullptr;
	}
	return DefaultPawn->CharacterDataComponent->CharacterData;
}

FText UPartySessionSubsystem::MakeDefaultDisplayName(const APlayerController *Leader)
{
	// "<Leader>'s Party" when a player name exists. PlayerState is often null this
	// early in a solo session, so fall back to a neutral label rather than
	// producing "'s Party".
	if (Leader && Leader->PlayerState)
	{
		const FString PlayerName = Leader->PlayerState->GetPlayerName();
		if (!PlayerName.IsEmpty())
		{
			return FText::Format(NSLOCTEXT("Party", "NamedPartyName", "{0}'s Party"),
								 FText::FromString(PlayerName));
		}
	}
	return NSLOCTEXT("Party", "DefaultPartyName", "Party");
}
