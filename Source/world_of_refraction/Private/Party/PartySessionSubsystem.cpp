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
	NewParty->Leader = Leader;
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

bool UPartySessionSubsystem::InviteMemberInternal(TSoftClassPtr<ACombatCharacter> PawnClass)
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

	UCharacterData *Data = ResolveCharacterData(PawnClass);
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

void UPartySessionSubsystem::RebindLeader(APlayerController *Leader)
{
	UParty *Party = GetLocalParty();
	if (!Party)
	{
		return;
	}

	Party->Leader = Leader;
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
		if (!Member.CharacterData)
		{
			continue;
		}

		// COMPUTED accessors, not the raw MaxHealth/MaxEnergy sub-stats — those are
		// inputs to the calculation, not the resulting pool sizes.
		Member.CurrentHP = Member.CharacterData->CalculateMaxHealth();
		Member.CurrentEP = Member.CharacterData->bBrokenDarknessInnate
							   ? 0
							   : Member.CharacterData->CalculateMaxEnergy();
	}

	UE_LOG(LogTemp, Log, TEXT("[PartySession] Vitals seeded to full for %d member(s)"), Party->GetMemberCount());
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

		if (!Member.HasCarriedVitals())
		{
			// No carry: show what display would fall back to, so the two cases are
			// comparable at a glance.
			const int32 FallbackHP = Member.CharacterData ? Member.CharacterData->CalculateMaxHealth() : 0;
			const int32 FallbackEP = Member.CharacterData ? Member.CharacterData->CalculateMaxEnergy() : 0;
			Out += FString::Printf(TEXT("  [%d] %s - no carry (falls back to HP %d / EP %d) - formation %s\n"),
								   SlotIndex, *MemberName, FallbackHP, FallbackEP, *Formation);
		}
		else
		{
			Out += FString::Printf(TEXT("  [%d] %s - carried HP %d / EP %d - formation %s\n"),
								   SlotIndex, *MemberName, Member.CurrentHP, Member.CurrentEP, *Formation);
		}
	}
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
