// AgentTestKitDebug.cpp

#include "Debug/Inventory/AgentTestKitDebug.h"

#if !UE_BUILD_SHIPPING

#include "AssetRegistry/IAssetRegistry.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Debug/Inventory/GiveTestWeaponsDebug.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Rings/RingData.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryConstants.h"
#include "Loadout/Entries/FApparelEntry.h"
#include "Party/Party.h"
#include "Party/PartyInteractionComponent.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Skills/Definitions/AbilityData.h"
#include "WoRLog.h"

namespace
{
	constexpr float TEST_KIT_SCREEN_SECONDS = 10.0f;
	/** The most assets of one kind the kit loads. */
	constexpr int32 MAX_ASSETS_PER_KIND = 30;

	/** The assets of a kind, by name; bLongestNameFirst puts the longest asset names first. */
	template <class TAsset>
	TArray<TAsset *> LoadKind(bool bLongestNameFirst = false)
	{
		TArray<FAssetData> Found;
		IAssetRegistry::GetChecked().GetAssetsByClass(TAsset::StaticClass()->GetClassPathName(), Found, /*bSearchSubClasses=*/true);
		Found.Sort([bLongestNameFirst](const FAssetData &A, const FAssetData &B)
		{
			const int32 LengthA = A.AssetName.GetStringLength();
			const int32 LengthB = B.AssetName.GetStringLength();
			return bLongestNameFirst && LengthA != LengthB ? LengthA > LengthB : A.AssetName.LexicalLess(B.AssetName);
		});
		TArray<TAsset *> Loaded;
		for (int32 Index = 0; Index < Found.Num() && Loaded.Num() < MAX_ASSETS_PER_KIND; ++Index)
		{
			if (TAsset *Asset = Cast<TAsset>(Found[Index].GetAsset()))
			{
				Loaded.Add(Asset);
			}
		}
		return Loaded;
	}

	FString GiveApparel(UPoolSubsystem &Pool, FName OwnerKey)
	{
		const TArray<UApparelData *> Pieces = LoadKind<UApparelData>();
		const int32 Missing = WoRAgentTestKit::APPAREL_PIECES - Pool.GetOwnedApparel().Num();
		if (Pieces.IsEmpty() || Missing <= 0)
		{
			return Pieces.IsEmpty() ? TEXT("apparel: no apparel asset found") : FString::Printf(TEXT("apparel: %d already owned"), Pool.GetOwnedApparel().Num());
		}
		for (int32 Index = 0; Index < Missing; ++Index)
		{
			Pool.AddApparelToPool(FApparelEntry::CreateFromApparel(Pieces[Index % Pieces.Num()]), OwnerKey);
		}
		return FString::Printf(TEXT("apparel: %d piece(s) given (%d kind(s)), %d owned"), Missing, FMath::Min(Missing, Pieces.Num()), Pool.GetOwnedApparel().Num());
	}

	FString GiveEvolutions(UInventoryComponent &Inventory)
	{
		const TArray<UEvolutionItemData *> Items = LoadKind<UEvolutionItemData>();
		int32 Given = 0;
		for (int32 Index = 0; Index < Items.Num() && Given < WoRAgentTestKit::EVOLUTIONS; ++Index)
		{
			Given += Inventory.AddEvolutionInstance(Items[Index]) ? 1 : 0;
		}
		return FString::Printf(TEXT("evolution: %d given%s"), Given, Items.IsEmpty() ? TEXT(" (no evolution asset found)") : Given == 0 ? TEXT(" (no room)") : TEXT(""));
	}

	FString LearnSkills(UPoolSubsystem &Pool, FName CharacterKey)
	{
		TArray<FString> Learned;
		for (const UAbilityData *Ability : LoadKind<UAbilityData>(/*bLongestNameFirst=*/true))
		{
			if (Learned.Num() >= WoRAgentTestKit::SKILLS)
			{
				break;
			}
			if (Pool.LearnSkillCopy(CharacterKey, Ability))
			{
				Learned.Add(Ability->Name);
			}
		}
		return Learned.IsEmpty() ? FString(TEXT("skill: none learned (no ability asset the character can still learn, or its skills are not in the store yet)"))
								 : FString::Printf(TEXT("skill: learned %s"), *FString::Join(Learned, TEXT(", ")));
	}

	FString AddMember(UWorld &World, const APawn &Pawn, UPartySessionSubsystem &Session)
	{
		const UParty *Party = Session.GetLocalParty();
		if (Party && Party->GetMemberCount() >= WoRAgentTestKit::PARTY_MEMBERS)
		{
			return FString::Printf(TEXT("member: the party already has %d"), Party->GetMemberCount());
		}
		// A recruitable character standing here joins as talking to it would have it join.
		for (TActorIterator<ACombatCharacter> It(&World); It; ++It)
		{
			const UCharacterDataComponent *DataComponent = It->FindComponentByClass<UCharacterDataComponent>();
			UCharacterData *Data = DataComponent ? DataComponent->CharacterData : nullptr;
			if (Data && It->FindComponentByClass<UPartyInteractionComponent>() && Session.GetMemberSlotByData(Data) == INDEX_NONE &&
				Session.InviteMemberWithData(TSoftClassPtr<ACombatCharacter>(It->GetClass()->GetPathName()), Data))
			{
				return FString::Printf(TEXT("member: %s joined (the character standing in this level)"), *Data->Name);
			}
		}
		if (!Pawn.IsA<ACombatCharacter>())
		{
			return TEXT("member: no recruitable character in this level, and the played character can't lend its body");
		}
		const TSoftClassPtr<ACombatCharacter> BodyClass(Pawn.GetClass()->GetPathName());
		for (UCharacterData *Data : LoadKind<UCharacterData>())
		{
			if (Session.GetMemberSlotByData(Data) == INDEX_NONE && Session.InviteMemberWithData(BodyClass, Data))
			{
				return FString::Printf(TEXT("member: %s joined in the played character's body (no recruitable character in this level)"), *Data->Name);
			}
		}
		return TEXT("member: nobody could join");
	}

	FAutoConsoleCommandWithWorldAndArgs GGiveTestKitCmd(
		TEXT("wor.Debug.GiveTestKit"),
		TEXT("DEBUG (B-573): test setup for agent play, found by kind (no asset path). [apparel] [adapters] [evolution] [skill] [member] [weapons]; "
			 "no argument gives all. Hub only."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				const FString Report = WoRAgentTestKit::GiveTestKit(World, Args);
				UE_LOG(LogWoR, Log, TEXT("[GiveTestKit]\n%s"), *Report);
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, TEST_KIT_SCREEN_SECONDS, FColor::Cyan, Report);
				}
			}));
}

int32 WoRAgentTestKit::GiveRings(UInventoryComponent &Inventory, const TArray<URingData *> &Rings)
{
	int32 Given = 0;
	int32 RefusedInARow = 0;
	// D-250: no room refuses any more, so the kit stops at its own count of owned adapters (a second give adds none).
	for (int32 Index = 0; RefusedInARow < Rings.Num() && Inventory.Rings.Num() < ADAPTERS; ++Index)
	{
		const bool bAdded = Inventory.AddRing(Rings[Index % Rings.Num()]);
		Given += bAdded ? 1 : 0;
		RefusedInARow = bAdded ? 0 : RefusedInARow + 1;
	}
	return Given;
}

FString WoRAgentTestKit::GiveTestKit(UWorld *World, const TArray<FString> &Parts)
{
	const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn *Pawn = PC ? PC->GetPawn() : nullptr;
	UInventoryComponent *Inventory = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	const UCharacterDataComponent *DataComponent = Pawn ? Pawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
	UPoolSubsystem *Pool = GameInstance ? GameInstance->GetSubsystem<UPoolSubsystem>() : nullptr;
	UPartySessionSubsystem *Session = GameInstance ? GameInstance->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	if (!Inventory || !DataComponent || !Pool || !Session)
	{
		return TEXT("no played character with an inventory, a pool and a party here");
	}
	if (World->GetNetMode() == NM_Client)
	{
		return TEXT("only the host can give the test kit");
	}
	if (Pool->IsRunLayerActive())
	{
		return TEXT("a run is open: gear given now would be run-only; use it in the hub");
	}

	const auto Wants = [&Parts](const TCHAR *Part)
	{
		return Parts.IsEmpty() || Parts.ContainsByPredicate([Part](const FString &Asked) { return Asked.Equals(Part, ESearchCase::IgnoreCase); });
	};
	const FName CharacterKey = UPoolSubsystem::GetCharacterKey(DataComponent);
	TArray<FString> Lines;
	if (Wants(TEXT("weapons")))
	{
		Lines.Add(Inventory->Weapons.Num() < static_cast<int32>(UE_ARRAY_COUNT(WoRGiveTestWeapons::TEST_WEAPONS))
					  ? FString::Printf(TEXT("weapons: %s"), *WoRGiveTestWeapons::GiveTestWeapons(World))
					  : FString::Printf(TEXT("weapons: %d already owned"), Inventory->Weapons.Num()));
	}
	if (Wants(TEXT("adapters")))
	{
		Lines.Add(FString::Printf(TEXT("adapters: %d given, %d owned"), GiveRings(*Inventory, LoadKind<URingData>()), Inventory->Rings.Num()));
	}
	if (Wants(TEXT("apparel")))
	{
		Lines.Add(GiveApparel(*Pool, CharacterKey));
	}
	if (Wants(TEXT("evolution")))
	{
		Lines.Add(GiveEvolutions(*Inventory));
	}
	if (Wants(TEXT("skill")))
	{
		Lines.Add(LearnSkills(*Pool, CharacterKey));
	}
	if (Wants(TEXT("member")))
	{
		Lines.Add(AddMember(*World, *Pawn, *Session));
	}
	if (Lines.IsEmpty())
	{
		return TEXT("nothing asked: apparel | adapters | evolution | skill | member | weapons (or no argument for all)");
	}
	return FString::Join(Lines, TEXT("\n"));
}

#endif // !UE_BUILD_SHIPPING
