// WoRPlayerState.cpp

#include "Player/WoRPlayerState.h"

#include "Character/Forms/CharacterFormSubsystem.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Currency/CurrencyComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Save/SaveSubsystem.h"
#include "WoRLog.h"

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
	}
}

void AWoRPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWoRPlayerState, WornForm);
	DOREPLIFETIME(AWoRPlayerState, BaseForm);
	DOREPLIFETIME(AWoRPlayerState, ClaimedCharacters);
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
	ClaimedCharacters.Add(Character);
	UE_LOG(LogWoR, Log, TEXT("[PlayerState] %s claims %s (B-232)"), *GetPlayerName(), *GetNameSafe(Character));
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
	// B-232 (Q-21-audit): the player's own character is the one its pawn plays.
	const UCharacterDataComponent *CharComp = NewPawn ? NewPawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (CharComp && CharComp->CharacterData)
	{
		ClaimCharacter(CharComp->CharacterData);
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
	// The default look leaves an authored pawn alone until this PlayerState has dressed it.
	if (!Mesh || (!WornForm && BaseForm == EPlayerBaseForm::Manny && DressedPawn.Get() != Character))
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
}

bool AWoRPlayerState::IsLocalPlayerWallet() const
{
	const APlayerController *PC = Cast<APlayerController>(GetOwner());
	return PC && PC->IsLocalController();
}
