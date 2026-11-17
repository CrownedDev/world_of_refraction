// SpiritLook.cpp

#include "Character/Forms/SpiritLook.h"

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/Forms/CharacterFormData.h"
#include "Character/Forms/CharacterFormSubsystem.h"
#include "Character/PlayerBuild.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "Infusion/ClassColors.h"
#include "Infusion/ElementColors.h"
#include "Inventory/GearGive.h"
#include "Inventory/InventoryComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Party/Party.h"
#include "Party/PartyInteractionComponent.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

namespace
{
	// B-279b: the base bodies (Design/CharacterForms.md section 1). // TODO: V's lighter void material once B-143 lands.
	const TCHAR *const MANNY_MESH = TEXT("/Game/Data/Characters/Models/CharacterBase/SKM_Manny_Base.SKM_Manny_Base");
	const TCHAR *const QUINN_MESH = TEXT("/Game/Data/Characters/Models/CharacterBase/SKM_Quinn.SKM_Quinn");

}

const FName WoRSpiritLook::TINT_PARAMETER(TEXT("Tint"));
const TArray<FName> WoRSpiritLook::CHEST_LIGHT_PARAMETERS = {TEXT("LogoLayer0_Color"), TEXT("LogoLayer1_Color"), TEXT("LogoLayer2_Color")};

FLinearColor WoRSpiritLook::GetBodyColour(const UCharacterData *Character)
{
	return Character && Character->bBrokenDarknessInnate ? ElementColors::BrokenDarkness : ClassColors::GetColorForCharacter(Character);
}

const TCHAR *WoRSpiritLook::GetBodyMeshPath(EPlayerBaseForm Body)
{
	return Body == EPlayerBaseForm::Quinn ? QUINN_MESH : MANNY_MESH;
}

bool WoRSpiritLook::Dress(USkeletalMeshComponent &Mesh, const UCharacterFormData *Form, EPlayerBaseForm Body,
						  const FLinearColor *Tint)
{
	if (Form)
	{
		return Form->DressMesh(Mesh); // its own materials: the body colour is off while a spirit is worn
	}
	USkeletalMesh *BodyMesh = LoadObject<USkeletalMesh>(nullptr, GetBodyMeshPath(Body));
	if (!BodyMesh)
	{
		return false;
	}
	Mesh.EmptyOverrideMaterials();
	Mesh.SetSkeletalMesh(BodyMesh);
	for (int32 Index = 0; Tint && Index < Mesh.GetNumMaterials(); ++Index)
	{
		if (UMaterialInstanceDynamic *Material = Mesh.CreateAndSetMaterialInstanceDynamic(Index))
		{
			Material->SetVectorParameterValue(TINT_PARAMETER, *Tint);
			for (const FName &ChestLight : CHEST_LIGHT_PARAMETERS)
			{
				Material->SetVectorParameterValue(ChestLight, *Tint);
			}
		}
	}
	return true;
}

bool WoRSpiritLook::DressMember(ACharacter &Pawn, const FPartyMember &Member, const UCharacterFormSubsystem *Forms)
{
	USkeletalMeshComponent *Mesh = Pawn.GetMesh();
	if (!Member.Picks.bRecruit || !Mesh)
	{
		return false;
	}
	const UCharacterFormData *Form = Forms && !Member.SpiritForm.IsNone() ? Forms->FindFormByName(Member.SpiritForm) : nullptr;
	UE_CLOG(!Member.SpiritForm.IsNone() && !Form, LogWoR, Warning, TEXT("[SpiritLook] %s wears %s, which is not a known spirit: its body instead"),
			*Member.Picks.Name, *Member.SpiritForm.ToString());
	const FLinearColor Colour = GetBodyColour(Member.CharacterData);
	const bool bDressed = Dress(*Mesh, Form, Member.SpiritBody, &Colour);
	UE_LOG(LogWoR, Log, TEXT("[SpiritLook] %s (%s) %s"), *GetNameSafe(&Pawn), *GetMemberSpiritString(Member), bDressed ? TEXT("dressed") : TEXT("has no mesh to wear"));
	return bDressed;
}

void WoRSpiritLook::DressMemberBodies(UWorld *World, const FPartyMember &Member)
{
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UCharacterFormSubsystem *Forms = GI ? GI->GetSubsystem<UCharacterFormSubsystem>() : nullptr;
	if (!World || !Member.Picks.bRecruit || !Member.CharacterData)
	{
		return;
	}
	const UInventoryComponent *Inventory = WoRGive::FindMemberInventory(World, UPoolSubsystem::GetCharacterKey(Member));
	if (ACharacter *Pawn = Inventory ? Cast<ACharacter>(Inventory->GetOwner()) : nullptr)
	{
		DressMember(*Pawn, Member, Forms);
	}
	// The placed hub recruit stands for its built character (UPartyInteractionComponent): same built name.
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		const UCharacterDataComponent *Data = It->FindComponentByClass<UCharacterDataComponent>();
		const FPlayerPicks *Row = It->FindComponentByClass<UPartyInteractionComponent>() && Data && Data->CharacterData
									  ? WoRPlayerBuild::FindRecruitPicks(*Data->CharacterData, GI) : nullptr;
		if (Row && WoRPlayerBuild::GetBuiltName(*Row) == Member.CharacterData->GetFName())
		{
			DressMember(**It, Member, Forms);
		}
	}
}

FString WoRSpiritLook::GetMemberSpiritString(const FPartyMember &Member)
{
	const FString Body = WoRGameNames::GetBaseFormName(Member.SpiritBody).ToString();
	if (!Member.SpiritForm.IsNone())
	{
		return FString::Printf(TEXT("%s: %s (over %s)"), *Member.Picks.Name, *Member.SpiritForm.ToString(), *Body);
	}
	return FString::Printf(TEXT("%s: %s (its colour %s)"), *Member.Picks.Name, *Body, *GetBodyColour(Member.CharacterData).ToFColor(true).ToHex());
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GSpiritLookPrintCommand(
	TEXT("wor.Party.PrintSpirits"),
	TEXT("B-558 (D-238): each recruit in the party and the spirit it wears."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
		const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
		for (const FPartyMember &Member : Party ? Party->Members : TArray<FPartyMember>())
		{
			UE_LOG(LogWoR, Display, TEXT("[SpiritLook] %s"), Member.Picks.bRecruit ? *WoRSpiritLook::GetMemberSpiritString(Member) : *FString::Printf(TEXT("%s: not a recruit"), *GetNameSafe(Member.CharacterData)));
		}
	}));
#endif
