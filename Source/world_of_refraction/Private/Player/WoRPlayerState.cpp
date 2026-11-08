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
#include "Player/RunStateComponent.h"
#include "Save/SaveSubsystem.h"
#include "Trial/TrialMapFeature.h"
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
	Super::CopyProperties(PlayerState);
	AWoRPlayerState *Next = Cast<AWoRPlayerState>(PlayerState);
	if (Next && Next->Wallet && Wallet)
	{
		Next->Wallet->CopyBalancesFrom(*Wallet);
	}
	if (Next)
	{
		Next->SetForm(WornForm, BaseForm);
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

void AWoRPlayerState::Client_ShowFeatureMessage_Implementation(const FText &Message)
{
	ATrialMapFeature::ShowMessage(Message); // B-235b
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
