// WoRPlayerState.cpp

#include "Player/WoRPlayerState.h"

#include "Character/Forms/CharacterFormSubsystem.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Character/Outfit/OutfitComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Currency/CurrencyComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Party/PartyInteractionComponent.h"
#include "Player/RunStateComponent.h"
#include "Player/WoRGameState.h"
#include "Online/HostSuccession.h"
#include "Save/SaveSubsystem.h"
#include "Save/CoopCheckpoint.h"
#include "Kismet/GameplayStatics.h"
#include "Loadout/LoadoutComponent.h" // B-392b
#include "Engine/LocalPlayer.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Memory/MemoryUnlock.h"
#include "Memory/HiddenCounterSubsystem.h"
#include "Memory/HiddenQuestSubsystem.h"
#include "Trial/TrialMapFeature.h"
#include "Memory/MemorySpot.h"
#include "Memory/MemoryFragment.h"
#include "EngineUtils.h"
#include "UI/Hub/RestSpotWidget.h"
#include "Trial/TrialRestSpot.h"
#include "Trial/TrialRunSubsystem.h"
#include "Combat/TurnManager.h"
#include "Combat/WoRFightSubsystem.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "WoRLog.h"

namespace
{
	/** D-150: a player asks the host for a seat with the character asset named (its path). */
	FAutoConsoleCommandWithWorldAndArgs GRequestJoinCmd(
		TEXT("wor.Party.RequestJoin"),
		TEXT("D-150: wor.Party.RequestJoin <CharacterData asset path> - ask the host for a seat in its run with that character (your own)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
				AWoRPlayerState *Player = PC ? PC->GetPlayerState<AWoRPlayerState>() : nullptr;
				if (!Player || Args.Num() < 1)
				{
					UE_LOG(LogWoR, Display, TEXT("[PlayerState] usage: wor.Party.RequestJoin <CharacterData asset path>"));
					return;
				}
				UCharacterData *Character = LoadObject<UCharacterData>(nullptr, *Args[0]);
				if (!Character)
				{
					UE_LOG(LogWoR, Display, TEXT("[PlayerState] no CharacterData at '%s'"), *Args[0]);
					return;
				}
				Player->RequestJoin(Character);
			}));

	FAutoConsoleCommandWithWorld GMemorySentCmd(
		TEXT("wor.Memory.Sent"),
		TEXT("B-235d (host): each player's own characters and the memories they have (reported, found or sent; B-235e)."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				const AGameStateBase *GameState = World ? World->GetGameState() : nullptr;
				if (!GameState)
				{
					UE_LOG(LogWoR, Display, TEXT("[PlayerState] wor.Memory.Sent: no game state"));
					return;
				}
				for (const APlayerState *Player : GameState->PlayerArray)
				{
					if (const AWoRPlayerState *WoRPlayer = Cast<AWoRPlayerState>(Player))
					{
						UE_LOG(LogWoR, Display, TEXT("[PlayerState] %s"), *WoRPlayer->GetSharedMemoriesString());
					}
				}
			}));
}

namespace PlayerFormLook
{
    // B-279b: the base bodies (Design/CharacterForms.md §1). // TODO: V's lighter void material once B-143 lands.
    const TCHAR *const MANNY_MESH = TEXT("/Game/Data/Characters/Models/CharacterBase/SKM_Manny_Base.SKM_Manny_Base");
    const TCHAR *const QUINN_MESH = TEXT("/Game/Data/Characters/Models/CharacterBase/SKM_Quinn.SKM_Quinn");
}

AWoRPlayerState::AWoRPlayerState()
{
	Wallet = CreateDefaultSubobject<UCurrencyComponent>(TEXT("Wallet"));
}

void AWoRPlayerState::BeginPlay()
{
	Super::BeginPlay();
	const UGameInstance *GI = GetGameInstance();
	if (const USaveSubsystem *Save = GI && IsLocalPlayerWallet() ? GI->GetSubsystem<USaveSubsystem>() : nullptr)
	{
		Save->ApplyWalletTo(Wallet);
	}
	OnPawnSet.AddUniqueDynamic(this, &AWoRPlayerState::HandlePawnSet);
	HandlePawnSet(this, GetPawn(), nullptr); // a pawn set before BeginPlay is claimed too (B-232)
	FollowLocalForm();
}

void AWoRPlayerState::ClientInitialize(AController *Controller)
{
	Super::ClientInitialize(Controller);
	// On a client the owner may arrive after BeginPlay; follow the local form once it does.
	FollowLocalForm();
	// B-235e: the host reads this player's own memories (PriorMemory conditions, nothing sent twice).
	const UWorld *World = GetWorld();
	const UGameInstance *GI = GetGameInstance();
	const UMemoryJournalSubsystem *Journal = GI ? GI->GetSubsystem<UMemoryJournalSubsystem>() : nullptr;
	if (Journal && World && World->GetNetMode() == NM_Client)
	{
		TArray<FName> MemoryIds;
		for (const UMemoryData *Memory : Journal->GetMemories())
		{
			if (Memory)
			{
				MemoryIds.Add(Memory->GetMemoryId());
			}
		}
		ServerReportJournal(MemoryIds);
		if (const UHiddenQuestSubsystem *Quests = GI->GetSubsystem<UHiddenQuestSubsystem>())
		{
			ServerReportQuests(Quests->Capture().Quests); // B-377b
		}
	}
}

void AWoRPlayerState::FollowLocalForm()
{
	const UGameInstance *GI = GetGameInstance();
	if (UCharacterFormSubsystem *Forms = GI && IsLocalPlayerWallet() ? GI->GetSubsystem<UCharacterFormSubsystem>() : nullptr)
	{
		Forms->OnWornFormChanged.AddUniqueDynamic(this, &AWoRPlayerState::HandleLocalFormChanged);
		// B-279c: the outfit follows the local player's current set. // TODO: co-op replicates it (B-228+).
		Forms->OnOutfitChanged.RemoveAll(this);
		Forms->OnOutfitChanged.AddUObject(this, &AWoRPlayerState::ApplyFormLook);
		HandleLocalFormChanged(Forms->GetWornForm());
	}
}

void AWoRPlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	const UGameInstance *GI = GetGameInstance();
	if (USaveSubsystem *Save = GI && IsLocalPlayerWallet() ? GI->GetSubsystem<USaveSubsystem>() : nullptr)
	{
		Save->BankWallet(Wallet);
	}
	if (UCharacterFormSubsystem *Forms = GI ? GI->GetSubsystem<UCharacterFormSubsystem>() : nullptr)
	{
		Forms->OnWornFormChanged.RemoveDynamic(this, &AWoRPlayerState::HandleLocalFormChanged);
		Forms->OnOutfitChanged.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AWoRPlayerState::CopyProperties(APlayerState *PlayerState)
{
	// B-234d: the join place first: Super sets the next PlayerState's id, and its OnSetUniqueId rebuilds the join
	// order, which must find the place already there (else the player would be given a new, last place).
	if (AWoRPlayerState *NextPlace = Cast<AWoRPlayerState>(PlayerState))
	{
		NextPlace->SetJoinSequence(JoinSequence);
	}
	Super::CopyProperties(PlayerState);
	AWoRPlayerState *Next = Cast<AWoRPlayerState>(PlayerState);
	if (Next && Next->Wallet && Wallet)
	{
		Next->Wallet->CopyBalancesFrom(*Wallet);
	}
	if (Next)
	{
		Next->SetForm(WornForm, BaseForm);
		Next->KnownMemoryIds = KnownMemoryIds; // B-235d: before the claims below, so nothing is sent again
		Next->ReportedQuests = ReportedQuests; // B-377b
		Next->OfferedQuests = OfferedQuests;
		// B-232: the player keeps commanding its characters in the next level.
		for (UCharacterData *Character : ClaimedCharacters)
		{
			Next->ClaimCharacter(Character);
		}
		for (const FGuid &MemberId : ClaimedMembers)
		{
			Next->ClaimMember(MemberId); // B-360b
		}
	}
}

void AWoRPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWoRPlayerState, WornForm);
	DOREPLIFETIME(AWoRPlayerState, BaseForm);
	DOREPLIFETIME(AWoRPlayerState, ClaimedCharacters);
	DOREPLIFETIME(AWoRPlayerState, ClaimedMembers);
}

bool AWoRPlayerState::ClaimsMember(const FGuid &MemberId) const
{
	return MemberId.IsValid() && ClaimedMembers.Contains(MemberId);
}

void AWoRPlayerState::ClaimMember(const FGuid &MemberId)
{
	if (!HasAuthority() || !MemberId.IsValid() || ClaimedMembers.Contains(MemberId))
	{
		return;
	}
	const UWorld *World = GetWorld();
	if (const AGameStateBase *GameState = World ? World->GetGameState() : nullptr)
	{
		for (const APlayerState *Other : GameState->PlayerArray)
		{
			const AWoRPlayerState *OtherPlayer = Cast<AWoRPlayerState>(Other);
			if (OtherPlayer && OtherPlayer != this && OtherPlayer->ClaimsMember(MemberId))
			{
				UE_LOG(LogWoR, Warning, TEXT("[PlayerState] %s cannot claim member %s: %s already commands it (B-360b)"), *GetPlayerName(),
					   *MemberId.ToString(EGuidFormats::Short), *OtherPlayer->GetPlayerName());
				return;
			}
		}
	}
	ClaimedMembers.Add(MemberId);
	UE_LOG(LogWoR, Log, TEXT("[PlayerState] %s claims member %s (B-360b)"), *GetPlayerName(), *MemberId.ToString(EGuidFormats::Short));
}

TArray<UCharacterData *> AWoRPlayerState::GetOwnCharacters() const
{
	TArray<UCharacterData *> Own(ClaimedCharacters);
	const UGameInstance *GI = GetGameInstance();
	const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	if (const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr)
	{
		for (const FPartyMember &Member : Party->Members)
		{
			if (Member.CharacterData && ClaimedMembers.Contains(Member.MemberId))
			{
				Own.AddUnique(Member.CharacterData);
			}
		}
	}
	Own.Remove(nullptr);
	return Own;
}

FString AWoRPlayerState::GetSharedMemoriesString() const
{
	TArray<FString> Characters;
	for (const UCharacterData *Character : GetOwnCharacters())
	{
		Characters.Add(GetNameSafe(Character));
	}
	TArray<FString> Sent;
	for (const FName &MemoryId : KnownMemoryIds)
	{
		Sent.Add(MemoryId.ToString());
	}
	Sent.Sort();
	return FString::Printf(TEXT("%s: characters [%s], memories known [%s]"), *GetPlayerName(), *FString::Join(Characters, TEXT(", ")),
						   *FString::Join(Sent, TEXT(", ")));
}

void AWoRPlayerState::ReleaseMember(const FGuid &MemberId)
{
	if (!HasAuthority() || ClaimedMembers.Remove(MemberId) == 0)
	{
		return;
	}
	UE_LOG(LogWoR, Log, TEXT("[PlayerState] %s releases member %s (B-360b)"), *GetPlayerName(), *MemberId.ToString(EGuidFormats::Short));
}

bool AWoRPlayerState::ClaimsCharacter(const UCharacterData *Character) const
{
	return Character && ClaimedCharacters.Contains(Character);
}

void AWoRPlayerState::ClaimCharacter(UCharacterData *Character)
{
	if (!HasAuthority() || !Character || ClaimedCharacters.Contains(Character))
	{
		return;
	}
	const UWorld *World = GetWorld();
	if (const AGameStateBase *GameState = World ? World->GetGameState() : nullptr)
	{
		for (const APlayerState *Other : GameState->PlayerArray)
		{
			const AWoRPlayerState *OtherPlayer = Cast<AWoRPlayerState>(Other);
			if (OtherPlayer && OtherPlayer != this && OtherPlayer->ClaimsCharacter(Character))
			{
				UE_LOG(LogWoR, Warning, TEXT("[PlayerState] %s cannot claim %s: %s already commands it (B-232)"), *GetPlayerName(),
					   *GetNameSafe(Character), *OtherPlayer->GetPlayerName());
				return;
			}
		}
	}
	ClaimedCharacters.Add(Character);
	UE_LOG(LogWoR, Log, TEXT("[PlayerState] %s claims %s (B-232)"), *GetPlayerName(), *GetNameSafe(Character));
}

void AWoRPlayerState::ReleaseCharacter(UCharacterData *Character)
{
	if (!HasAuthority() || !Character || ClaimedCharacters.Remove(Character) == 0)
	{
		return;
	}
	UE_LOG(LogWoR, Log, TEXT("[PlayerState] %s releases %s (B-232)"), *GetPlayerName(), *GetNameSafe(Character));
}

void AWoRPlayerState::RequestJoin(UCharacterData *Character)
{
	if (!Character)
	{
		UE_LOG(LogWoR, Warning, TEXT("[PlayerState] RequestJoin with no character (D-150: pick your own character first)"));
		return;
	}
	if (HasAuthority())
	{
		ServerRequestJoin_Implementation(Character);
	}
	else
	{
		ServerRequestJoin(Character);
	}
}

void AWoRPlayerState::ServerRequestJoin_Implementation(UCharacterData *Character)
{
	if (URunStateComponent *Run = URunStateComponent::Find(this))
	{
		Run->RequestJoin(this, Character, TSoftClassPtr<ACombatCharacter>());
	}
	else
	{
		UE_LOG(LogWoR, Warning, TEXT("[PlayerState] %s asked to join but there is no run state here"), *GetPlayerName());
	}
}

void AWoRPlayerState::SetForm(UCharacterFormData *InWornForm, EPlayerBaseForm InBaseForm)
{
	if (!HasAuthority() || (WornForm == InWornForm && BaseForm == InBaseForm))
	{
		return;
	}
	WornForm = InWornForm;
	BaseForm = InBaseForm;
	OnRep_Form();
}

void AWoRPlayerState::ServerSetForm_Implementation(UCharacterFormData *InWornForm, EPlayerBaseForm InBaseForm)
{
	// TODO: co-op trust: the client's own save says the form is unlocked; the host doesn't re-check (B-233+).
	SetForm(InWornForm, InBaseForm);
}

void AWoRPlayerState::HandleLocalFormChanged(UCharacterFormData *Form)
{
	const UGameInstance *GI = GetGameInstance();
	const UCharacterFormSubsystem *Forms = GI ? GI->GetSubsystem<UCharacterFormSubsystem>() : nullptr;
	const EPlayerBaseForm Base = Forms ? Forms->GetBaseForm() : BaseForm;
	if (HasAuthority())
	{
		SetForm(Form, Base);
	}
	else
	{
		ServerSetForm(Form, Base);
	}
}

void AWoRPlayerState::HandlePawnSet(APlayerState *Player, APawn *NewPawn, APawn *OldPawn)
{
	ApplyFormLook();
	// B-232 (Q-21-audit): the player's own character is the one its pawn plays; a pawn swap releases the old one.
	// B-360b: by party member when the party knows the pawn (a battle pawn's stamped id, a hub pawn's first copy), so
	// two copies of one character are two claims; by character otherwise.
	const UCharacterDataComponent *NewComp = NewPawn ? NewPawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	const UCharacterDataComponent *OldComp = OldPawn ? OldPawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	const UGameInstance *GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
	auto MemberOf = [&](const UCharacterDataComponent *Comp)
	{
		const int32 Slot = Party ? PartySession->GetMemberSlotOf(Comp) : INDEX_NONE;
		return Party && Party->Members.IsValidIndex(Slot) ? Party->Members[Slot].MemberId : FGuid();
	};
	const FGuid NewMember = MemberOf(NewComp);
	const FGuid OldMember = MemberOf(OldComp);
	UCharacterData *NewData = NewComp ? NewComp->CharacterData : nullptr;
	UCharacterData *OldData = OldComp ? OldComp->CharacterData : nullptr;
	if (OldMember.IsValid() && OldMember != NewMember)
	{
		ReleaseMember(OldMember);
	}
	else if (!OldMember.IsValid() && OldData && OldData != NewData)
	{
		ReleaseCharacter(OldData);
	}
	// An unstamped pawn (the hub) means the first copy; a player that already holds a seat (a joiner whose copy is a
	// later one) does not reach for it.
	if (NewMember.IsValid() && (NewComp->PartyMemberId.IsValid() || !HasAnyClaim()))
	{
		ClaimMember(NewMember);
	}
	else if (!NewMember.IsValid() && NewData)
	{
		ClaimCharacter(NewData);
	}
}

void AWoRPlayerState::OnRep_Form()
{
	ApplyFormLook();
}

void AWoRPlayerState::ApplyFormLook()
{
	const ACharacter *Character = Cast<ACharacter>(GetPawn());
	USkeletalMeshComponent *Mesh = Character ? Character->GetMesh() : nullptr;
	// The default look leaves an authored pawn alone until this PlayerState has dressed it (unless an outfit is worn).
	const UGameInstance *OutfitGI = GetGameInstance();
	const UCharacterFormSubsystem *LocalForms = OutfitGI && IsLocalPlayerWallet() ? OutfitGI->GetSubsystem<UCharacterFormSubsystem>() : nullptr;
	// TODO: co-op (D-103): a remote player's outfit isn't known here, so its pawn is stripped; replicate the set.
	const bool bWearsOutfit = LocalForms && !LocalForms->GetCurrentOutfit().IsEmpty();
	// An outfit emptied (the last piece cleared, a save without one) takes its meshes off, even on the default look.
	if (UOutfitComponent *Worn = Mesh && !bWearsOutfit && Character ? Character->FindComponentByClass<UOutfitComponent>() : nullptr)
	{
		Worn->ApplyOutfit(FOutfitSet(), Mesh);
	}
	if (!Mesh || (!WornForm && BaseForm == EPlayerBaseForm::Manny && DressedPawn.Get() != Character && !bWearsOutfit))
	{
		return;
	}
	bool bDressed = false;
	if (WornForm)
	{
		bDressed = WornForm->DressMesh(*Mesh);
	}
	else if (USkeletalMesh *BaseMesh = LoadObject<USkeletalMesh>(nullptr, BaseForm == EPlayerBaseForm::Quinn ? PlayerFormLook::QUINN_MESH : PlayerFormLook::MANNY_MESH))
	{
		Mesh->EmptyOverrideMaterials();
		Mesh->SetSkeletalMesh(BaseMesh);
		bDressed = true;
	}
	if (!bDressed)
	{
		UE_LOG(LogWoR, Warning, TEXT("[CharacterForms] %s has no mesh to wear"), WornForm ? *WornForm->GetName() : *UEnum::GetValueAsString(BaseForm));
		return;
	}
	DressedPawn = Character;
	UE_LOG(LogWoR, Log, TEXT("[CharacterForms] %s dressed as %s"), *GetNameSafe(Character), *GetNameSafe(Mesh->GetSkeletalMeshAsset()));
	// B-279c: the local player's current outfit over the body (Leader Pose). // TODO: co-op guests read it replicated.
	if (bWearsOutfit)
	{
		AActor *Pawn = const_cast<ACharacter *>(Character);
		UOutfitComponent *Outfit = Pawn->FindComponentByClass<UOutfitComponent>();
		if (!Outfit)
		{
			Outfit = NewObject<UOutfitComponent>(Pawn);
			Pawn->AddInstanceComponent(Outfit);
			Outfit->RegisterComponent();
		}
		Outfit->ApplyOutfit(LocalForms->GetCurrentOutfit(), Mesh);
	}
}

bool AWoRPlayerState::IsLocalPlayerWallet() const
{
	const APlayerController *PC = Cast<APlayerController>(GetOwner());
	return PC && PC->IsLocalController();
}

void AWoRPlayerState::ServerRecruit_Implementation(AActor *RecruitOwner)
{
	// B-232e: the recruit must still be there and within reach of this player's pawn (the client's trigger said so).
	UPartyInteractionComponent *Recruit = RecruitOwner ? RecruitOwner->FindComponentByClass<UPartyInteractionComponent>() : nullptr;
	const APawn *Pawn = GetPawn();
	if (!Recruit)
	{
		// Review: a recruit actor that does not replicate arrives here as null.
		UE_LOG(LogWoR, Warning, TEXT("[PartyInteraction] %s's recruit refused: the recruit did not reach the server (%s)"),
			   *GetPlayerName(), RecruitOwner ? TEXT("no party interaction") : TEXT("not replicated?"));
		return;
	}
	if (!Pawn || !Recruit->IsWithinReach(Pawn))
	{
		UE_LOG(LogWoR, Log, TEXT("[PartyInteraction] %s's recruit of %s refused: out of reach"), *GetPlayerName(), *GetNameSafe(RecruitOwner));
		return;
	}
	Recruit->InviteForClient();
}

bool AWoRPlayerState::CommandsFighter(const AActor *Fighter) const
{
	const UCharacterDataComponent *CharComp = Fighter ? Fighter->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (!CharComp)
	{
		return false;
	}
	return CharComp->PartyMemberId.IsValid() ? ClaimsMember(CharComp->PartyMemberId) : ClaimsCharacter(CharComp->CharacterData);
}

namespace WoRGuardSync
{
	/** B-392b: the host's copy of Fighter's loadout when Player commands it; null (logged) otherwise. */
	ULoadoutComponent *FindCommandedLoadout(const AWoRPlayerState &Player, AActor *Fighter, const TCHAR *What)
	{
		ULoadoutComponent *Loadout = Fighter ? Fighter->FindComponentByClass<ULoadoutComponent>() : nullptr;
		if (!Loadout || !Player.CommandsFighter(Fighter))
		{
			// Review: a fighter that does not replicate arrives here as null.
			UE_LOG(LogWoR, Warning, TEXT("[Guard] %s's %s for %s refused: %s"), *Player.GetPlayerName(), What, *GetNameSafe(Fighter),
				   !Fighter ? TEXT("the fighter did not reach the server (not replicated?)") : !Loadout ? TEXT("no loadout") : TEXT("not their fighter"));
			return nullptr;
		}
		return Loadout;
	}
}

void AWoRPlayerState::ServerSetGuard_Implementation(AActor *Fighter, EGuardStyle Style)
{
	if (!StaticEnum<EGuardStyle>()->IsValidEnumValue(static_cast<int64>(Style)))
	{
		UE_LOG(LogWoR, Warning, TEXT("[Guard] %s sent an unknown guard (%d)"), *GetPlayerName(), static_cast<int32>(Style));
		return;
	}
	if (ULoadoutComponent *Loadout = WoRGuardSync::FindCommandedLoadout(*this, Fighter, TEXT("guard")))
	{
		Loadout->SetGuardStyle(Style); // the host's own rules (a weapon in hand, its id) decide; it writes the host's record
	}
}

void AWoRPlayerState::ServerSetParryPriority_Implementation(AActor *Fighter, EParryElementPriority Priority)
{
	if (!StaticEnum<EParryElementPriority>()->IsValidEnumValue(static_cast<int64>(Priority)))
	{
		UE_LOG(LogWoR, Warning, TEXT("[Guard] %s sent an unknown parry priority (%d)"), *GetPlayerName(), static_cast<int32>(Priority));
		return;
	}
	if (ULoadoutComponent *Loadout = WoRGuardSync::FindCommandedLoadout(*this, Fighter, TEXT("parry priority")))
	{
		Loadout->SetParryElementPriority(Priority);
	}
}
void AWoRPlayerState::SendCheckpoint(int32 CheckpointId, const TArray<uint8> &Compressed, int32 UncompressedSize)
{
	if (!HasAuthority())
	{
		return;
	}
	const TArray<TArray<uint8>> Chunks = WoRCheckpoint::Split(Compressed);
	for (int32 Index = 0; Index < Chunks.Num(); ++Index)
	{
		Client_ReceiveCheckpointChunk(CheckpointId, Index, Chunks.Num(), UncompressedSize, Chunks[Index]);
	}
}

void AWoRPlayerState::Client_ReceiveCheckpointChunk_Implementation(int32 CheckpointId, int32 ChunkIndex, int32 ChunkCount, int32 UncompressedSize,
																	const TArray<uint8> &Chunk)
{
	if (!CheckpointAssembler.Add(CheckpointId, ChunkIndex, ChunkCount, Chunk))
	{
		return;
	}
	TArray<uint8> Bytes;
	if (UncompressedSize <= 0 || !WoRCheckpoint::Decompress(CheckpointAssembler.GetBytes(), UncompressedSize, Bytes))
	{
		UE_LOG(LogWoR, Warning, TEXT("[Checkpoint] Copy %d did not decompress (%d bytes, expected %d); not kept"), CheckpointId,
			   CheckpointAssembler.GetBytes().Num(), UncompressedSize);
		return;
	}
	// Review fix: only a copy that loads as a save replaces the kept one.
	if (!UGameplayStatics::LoadGameFromMemory(Bytes))
	{
		UE_LOG(LogWoR, Warning, TEXT("[Checkpoint] Copy %d is not a save (%d bytes); not kept"), CheckpointId, Bytes.Num());
		return;
	}
	const APlayerController *PC = Cast<APlayerController>(GetOwner());
	const ULocalPlayer *Local = PC ? PC->GetLocalPlayer() : nullptr;
	const int32 UserIndex = Local ? Local->GetPlatformUserIndex() : 0;
	if (!UGameplayStatics::SaveDataToSlot(Bytes, WoRCheckpoint::SLOT_NAME, UserIndex))
	{
		UE_LOG(LogWoR, Warning, TEXT("[Checkpoint] Could not keep copy %d (%d bytes) in '%s'"), CheckpointId, Bytes.Num(), WoRCheckpoint::SLOT_NAME);
		return;
	}
	KeptCheckpointId = CheckpointId;
	KeptCheckpointBytes = Bytes.Num();
	KeptCheckpointTime = FDateTime::Now();
	UE_LOG(LogWoR, Log, TEXT("[Checkpoint] Kept the host's copy %d (%d bytes) in '%s'"), CheckpointId, Bytes.Num(), WoRCheckpoint::SLOT_NAME);
}

void AWoRPlayerState::OnSetUniqueId()
{
	Super::OnSetUniqueId();
	if (AWoRGameState *GameState = GetWorld() ? GetWorld()->GetGameState<AWoRGameState>() : nullptr)
	{
		GameState->UpdateHostOrder(this, /*bJoined=*/true); // server only (UpdateHostOrder checks)
	}
}

void AWoRPlayerState::Client_BeginPlannedHandOver_Implementation()
{
	UGameInstance *GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UWoRHostHandOverSubsystem *HandOver = GI ? GI->GetSubsystem<UWoRHostHandOverSubsystem>() : nullptr)
	{
		HandOver->HandlePlannedHandOver(GetWorld());
	}
}

FString AWoRPlayerState::GetCheckpointString() const
{
	if (KeptCheckpointId == INDEX_NONE)
	{
		return FString::Printf(TEXT("%s: no checkpoint copy kept this session (receiving %d / %d of copy %d)"), *GetPlayerName(),
							   CheckpointAssembler.GetReceivedCount(), CheckpointAssembler.GetChunkCount(), CheckpointAssembler.GetCheckpointId());
	}
	return FString::Printf(TEXT("%s: copy %d kept (%d bytes) at %s in '%s'"), *GetPlayerName(), KeptCheckpointId, KeptCheckpointBytes,
						   *KeptCheckpointTime.ToString(TEXT("%H:%M:%S")), WoRCheckpoint::SLOT_NAME);
}

static FAutoConsoleCommandWithWorld GCoopPrintCheckpointCommand(
	TEXT("wor.Coop.PrintCheckpoint"),
	TEXT("B-234: the host's last checkpoint copy this machine kept (a client), and whether one is on disk."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
		const AWoRPlayerState *Player = PC ? PC->GetPlayerState<AWoRPlayerState>() : nullptr;
		const ULocalPlayer *Local = PC ? PC->GetLocalPlayer() : nullptr;
		const bool bOnDisk = UGameplayStatics::DoesSaveGameExist(WoRCheckpoint::SLOT_NAME, Local ? Local->GetPlatformUserIndex() : 0);
		UE_LOG(LogWoR, Display, TEXT("[Checkpoint] %s; on disk: %s"), Player ? *Player->GetCheckpointString() : TEXT("no player state"),
			   bOnDisk ? TEXT("yes") : TEXT("no"));
	}));
void AWoRPlayerState::Client_ShowFeatureMessage_Implementation(const FText &Message)
{
	ATrialMapFeature::ShowMessage(Message); // B-235b
}

void AWoRPlayerState::Client_OpenRestMenu_Implementation(bool bCanRevive, int32 RevivalNumber)
{
	APlayerController *PC = Cast<APlayerController>(GetOwner());
	if (URestSpotWidget *Menu = PC && PC->IsLocalController() ? URestSpotWidget::Open(PC) : nullptr)
	{
		Menu->SetHostRevive(bCanRevive, RevivalNumber);
	}
}

void AWoRPlayerState::ServerReviveMiniBoss_Implementation()
{
	UWorld *World = GetWorld();
	// B-187e review: the client's menu has closed, so a refusal says why on its screen.
	const FText Refused = NSLOCTEXT("WoRPlayerState", "ReviveRefused", "The mini-boss can't be revived from here right now.");
	if (!WoRRestSpot::CanReviveHere(World))
	{
		UE_LOG(LogWoR, Log, TEXT("[RestSpot] %s's revive refused: no revivable mini-boss at the last rest spot"), *GetPlayerName());
		Client_ShowFeatureMessage(Refused);
		return;
	}
	// Review: never from a player in a fight (Q-187e: a vote can't start mid-fight for the starter), and only from beside
	// the rest stand. Another player's fight doesn't refuse it: a passed vote waits for that fight to end (Q-187e2).
	const AWoRGameState *VoteState = World ? World->GetGameState<AWoRGameState>() : nullptr;
	if (VoteState && VoteState->IsPlayerInBattle(this))
	{
		UE_LOG(LogWoR, Log, TEXT("[RestSpot] %s's revive refused: in a fight"), *GetPlayerName());
		Client_ShowFeatureMessage(Refused);
		return;
	}
	const UGameInstance *GI = GetGameInstance();
	const UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	FTransform Stand;
	const APawn *Pawn = GetPawn();
	if (!Pawn || !Run || !Run->GetRestStand(Stand) ||
		FVector::Dist(Pawn->GetActorLocation(), Stand.GetLocation()) > WoRRestSpot::CLIENT_REVIVE_REACH_CM)
	{
		UE_LOG(LogWoR, Log, TEXT("[RestSpot] %s's revive refused: not at the rest spot"), *GetPlayerName());
		Client_ShowFeatureMessage(Refused);
		return;
	}
	// Q-187e (Crown, 2026-11-08): a vote of every player outside battle; solo it revives at once.
	UE_LOG(LogWoR, Log, TEXT("[RestSpot] %s asks to revive the mini-boss (B-187e vote)"), *GetPlayerName());
	if (AWoRGameState *GameState = World ? World->GetGameState<AWoRGameState>() : nullptr)
	{
		GameState->StartReviveVote(this);
	}
}

void AWoRPlayerState::ServerCastReviveVote_Implementation(bool bYes)
{
	if (AWoRGameState *GameState = GetWorld() ? GetWorld()->GetGameState<AWoRGameState>() : nullptr)
	{
		GameState->CastReviveVote(this, bYes);
	}
}

void AWoRPlayerState::ServerInteractFeature_Implementation(ATrialMapFeature *Feature)
{
	APawn *Pawn = GetPawn();
	if (!Feature || !Pawn || !Feature->IsWithinReach(Pawn))
	{
		UE_LOG(LogWoR, Log, TEXT("[TrialMapFeature] %s's use of %s refused: out of reach"), *GetPlayerName(), *GetNameSafe(Feature));
		return;
	}
	Feature->Interact(Pawn); // B-235c: the host runs it; per-player features key it to this player
}

void AWoRPlayerState::ServerReportJournal_Implementation(const TArray<FName> &MemoryIds)
{
	// B-235e review: a bounded report, so one message can't grow the host's list without end.
	constexpr int32 MaxJournalReport = 4096; // TODO: tune to the authored memory count once the catalogue is in
	const int32 Taken = FMath::Min(MemoryIds.Num(), MaxJournalReport);
	for (int32 Index = 0; Index < Taken; ++Index)
	{
		KnownMemoryIds.Add(MemoryIds[Index]);
	}
	UE_LOG(LogWoR, Log, TEXT("[MemoryUnlock] %s reported %d memory(ies) in their journal (B-235e)%s"), *GetPlayerName(), Taken,
		   Taken < MemoryIds.Num() ? TEXT(", the rest dropped (over the cap)") : TEXT(""));
}

void AWoRPlayerState::ServerReportQuests_Implementation(const TArray<FSavedQuestProgress> &Quests)
{
	ReportedQuests.Reset();
	// B-377b review: a bounded report, as ServerReportJournal's.
	constexpr int32 MaxQuestReport = 1024; // TODO: tune to the authored quest count once the catalogue is in
	const int32 Taken = FMath::Min(Quests.Num(), MaxQuestReport);
	if (Taken < Quests.Num())
	{
		UE_LOG(LogWoR, Warning, TEXT("[HiddenQuest] %s reported %d quests; the %d over the cap are dropped"), *GetPlayerName(), Quests.Num(),
			   Quests.Num() - Taken);
	}
	for (int32 Index = 0; Index < Taken; ++Index)
	{
		const FSavedQuestProgress &Quest = Quests[Index];
		if (Quest.QuestId.IsNone())
		{
			continue;
		}
		if (!Quest.bFinished)
		{
			ReportedQuests.Add(Quest.QuestId, Quest.StepsDone);
			OfferedQuests.Remove(Quest.QuestId); // reported: started on their side
		}
		else
		{
			OfferedQuests.Add(Quest.QuestId); // finished: never offered again
		}
	}
}

void AWoRPlayerState::Client_QuestStepMet_Implementation(FName QuestId, int32 StepIndex)
{
	if (UHiddenQuestSubsystem *Quests = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHiddenQuestSubsystem>() : nullptr)
	{
		Quests->ApplyStepMetFromHost(QuestId, StepIndex);
	}
}

void AWoRPlayerState::Client_StartQuest_Implementation(UHiddenQuestData *Quest)
{
	if (UHiddenQuestSubsystem *Quests = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHiddenQuestSubsystem>() : nullptr)
	{
		Quests->StartQuest(Quest);
	}
}

namespace
{
	/** B-433 review: how near a map spot or fragment the reporting pawn must be on the host. A distance, not the trigger
	 *  overlap: the client's reliable report can arrive before its last move does. */
	constexpr float MEMORY_REPORT_REACH_CM = 600.0f; // TODO: tune

	/** B-433 (server): Pawn is near some map spot or fragment. Any of them, not only the one holding Memory: a spot or
	 *  fragment event also unlocks the other memories whose conditions it meets (MemoryConditions). */
	bool IsPawnAtMemoryPlace(const APawn *Pawn)
	{
		const UWorld *World = Pawn ? Pawn->GetWorld() : nullptr;
		if (!World)
		{
			return false;
		}
		const FVector Where = Pawn->GetActorLocation();
		const auto IsNear = [&Where](const AActor *Place)
		{ return FVector::Dist(Where, Place->GetActorLocation()) <= MEMORY_REPORT_REACH_CM; };
		for (TActorIterator<AMemorySpot> Spot(World); Spot; ++Spot)
		{
			if (IsNear(*Spot))
			{
				return true;
			}
		}
		for (TActorIterator<AMemoryFragment> Fragment(World); Fragment; ++Fragment)
		{
			if (IsNear(*Fragment))
			{
				return true;
			}
		}
		return false;
	}
}

void AWoRPlayerState::ServerReportFoundMemory_Implementation(UMemoryData *Memory, EMemorySource Source)
{
	if (!Memory)
	{
		return;
	}
	// B-235e review: a character's memory only from a player who plays that character.
	if (!MemoryUnlock::QualifiesFor(Memory, GetOwnCharacters()))
	{
		UE_LOG(LogWoR, Warning, TEXT("[MemoryUnlock] %s's find of %s refused: none of their characters has it"), *GetPlayerName(),
			   *Memory->GetMemoryId().ToString());
		return;
	}
	// B-433: a find at a map spot or fragment needs the player's pawn there, so a report from out of reach unlocks nothing.
	// TODO: the other sources (bound gear, a quest's end) are trusted as reported, co-op only (WithValidation later).
	if ((Source == EMemorySource::MapSpot || Source == EMemorySource::Fragment) && !IsPawnAtMemoryPlace(GetPawn()))
	{
		UE_LOG(LogWoR, Warning, TEXT("[MemoryUnlock] %s's find of %s refused: their pawn isn't near a memory spot or fragment"),
			   *GetPlayerName(), *Memory->GetMemoryId().ToString());
		return;
	}
	AddMemoryId(Memory->GetMemoryId());
	if (!Memory->CharacterDepth.IsEmpty())
	{
		// Their character's own find (Q-235d rule 1): into the run like a friend's find on the host (UnlockFor), not
		// into the host's journal.
		const UGameInstance *GI = GetGameInstance();
		if (UTrialRunSubsystem *TrialRun = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr)
		{
			TrialRun->CollectMemory(Memory);
		}
		return;
	}
	TArray<AWoRPlayerState *> Others = MemoryUnlock::GetRemotePlayers(GetGameInstance());
	Others.Remove(this);
	MemoryUnlock::UnlockFor(GetGameInstance(), Memory, Source, /*bHostReceives=*/true, Others);
}

#define LOCTEXT_NAMESPACE "WoRPlayerState"

void AWoRPlayerState::Client_AddCounter_Implementation(FName Counter, UCharacterData *Character)
{
	if (UHiddenCounterSubsystem *Counters = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHiddenCounterSubsystem>() : nullptr)
	{
		Counters->Add(Counter, Character);
	}
}

void AWoRPlayerState::Client_UnlockMemory_Implementation(UMemoryData *Memory, EMemorySource Source)
{
	UMemoryJournalSubsystem *Journal = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMemoryJournalSubsystem>() : nullptr;
	if (!Memory || !Journal || !Journal->RecordMemory(Memory, Source))
	{
		return; // already in this player's journal
	}
	ATrialMapFeature::ShowMessage(FText::Format(LOCTEXT("MemoryUnlocked", "New memory: {0}"), Memory->Title)); // private
	UE_LOG(LogWoR, Log, TEXT("[MemoryUnlock] %s recorded in this player's journal"), *Memory->GetMemoryId().ToString());
}

#undef LOCTEXT_NAMESPACE
