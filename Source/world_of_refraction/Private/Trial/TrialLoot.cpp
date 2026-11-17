// TrialLoot.cpp

#include "Trial/TrialLoot.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/EquipmentDataBase.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Actor.h"
#include "Inventory/InventoryComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/SpellData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Trial/PrebuiltFindData.h"
#include "Trial/TrialPool.h"
#include "Trial/TrialRunSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "TrialLoot"

const FGameplayTagContainer *TrialLoot::GetSourceTags(const UObject *Item)
{
	if (const UEquipmentDataBase *Equipment = Cast<UEquipmentDataBase>(Item))
	{
		return &Equipment->SourceTags;
	}
	if (const USkillDataBase *Skill = Cast<USkillDataBase>(Item))
	{
		return &Skill->SourceTags;
	}
	if (const UPrebuiltFindData *Find = Cast<UPrebuiltFindData>(Item))
	{
		return &Find->SourceTags;
	}
	if (const UEvolutionItemData *Evolution = Cast<UEvolutionItemData>(Item))
	{
		return &Evolution->SourceTags; // B-566 (D-225): evolutions are in the pool
	}
	return nullptr;
}

FText TrialLoot::GetItemName(const UObject *Item)
{
	if (const UEquipmentDataBase *Equipment = Cast<UEquipmentDataBase>(Item))
	{
		return FText::FromString(Equipment->Name);
	}
	if (const USkillDataBase *Skill = Cast<USkillDataBase>(Item))
	{
		return FText::FromString(Skill->Name);
	}
	if (const UPrebuiltFindData *Find = Cast<UPrebuiltFindData>(Item))
	{
		const FText Piece = GetItemName(Find->Piece);
		if (Find->Crystal.Type != ECrystalType::None)
		{
			return FText::Format(LOCTEXT("FindWithSocket", "{0} with {1}"), Piece, FText::FromString(ItemIdentity::GetDisplayName(Find->Crystal)));
		}
		return Find->Evolution ? FText::Format(LOCTEXT("FindWithSocket", "{0} with {1}"), Piece, FText::FromString(Find->Evolution->ItemName))
							   : Piece;
	}
	if (const UEvolutionItemData *Evolution = Cast<UEvolutionItemData>(Item))
	{
		return FText::FromString(Evolution->ItemName);
	}
	return FText::GetEmpty();
}

TArray<UObject *> TrialLoot::GatherPool(const FGameplayTag &Pool)
{
	TArray<UObject *> Matches;
	if (!Pool.IsValid())
	{
		return Matches;
	}
	const IAssetRegistry &Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	for (const UClass *LootClass : {UEquipmentDataBase::StaticClass(), USkillDataBase::StaticClass(), UPrebuiltFindData::StaticClass(), UEvolutionItemData::StaticClass()})
	{
		TArray<FAssetData> Assets;
		Registry.GetAssetsByClass(LootClass->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);
		for (const FAssetData &Asset : Assets)
		{
			UObject *Item = Asset.GetAsset();
			const FGameplayTagContainer *Tags = GetSourceTags(Item);
			if (Tags && Tags->HasTag(Pool))
			{
				Matches.Add(Item);
			}
		}
	}
	// The registry's order isn't fixed; a seeded index needs one.
	Matches.Sort([](const UObject &A, const UObject &B) { return A.GetPathName() < B.GetPathName(); });
	return Matches;
}

FString TrialLoot::GetPoolString(const FGameplayTag &Pool)
{
	const TArray<UObject *> Items = GatherPool(Pool);
	TArray<FString> Names;
	for (const UObject *Item : Items)
	{
		Names.Add(GetItemName(Item).ToString());
	}
	return FString::Printf(TEXT("Loot pool %s: %d item(s) [%s]"), Pool.IsValid() ? *Pool.ToString() : TEXT("<none>"),
						   Items.Num(), *FString::Join(Names, TEXT(", ")));
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs GTrialLootPoolCommand(
	TEXT("wor.Trial.Pool"),
	TEXT("wor.Trial.Pool [tag] - list the items a loot pool (source tag) can roll, e.g. wor.Trial.Pool Source.Zone.Tier1; with no tag, the active trial's pool zone by zone (B-566)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
	{
		if (Args.Num() == 0)
		{
			const UGameInstance *Game = World ? World->GetGameInstance() : nullptr;
			const UTrialRunSubsystem *Run = Game ? Game->GetSubsystem<UTrialRunSubsystem>() : nullptr;
			UE_LOG(LogWoREconomy, Log, TEXT("%s"), *TrialPool::GetTrialPoolString(Run ? Run->GetActiveTrial() : nullptr, World));
			return;
		}
		const FGameplayTag Pool = FGameplayTag::RequestGameplayTag(FName(*Args[0]), /*ErrorIfNotFound=*/false);
		UE_LOG(LogWoREconomy, Log, TEXT("%s"), *TrialLoot::GetPoolString(Pool));
	}));
#endif

UObject *TrialLoot::PickFromPool(const TArray<UObject *> &Candidates, const FGameplayTag &Pool, int32 Seed)
{
	TArray<UObject *> Eligible;
	for (UObject *Candidate : Candidates)
	{
		const FGameplayTagContainer *Tags = GetSourceTags(Candidate);
		if (Pool.IsValid() && Tags && Tags->HasTag(Pool) && CanGrant(Candidate))
		{
			Eligible.Add(Candidate);
		}
	}
	if (Eligible.IsEmpty())
	{
		return nullptr;
	}
	const FRandomStream Roll(Seed);
	return Eligible[Roll.RandRange(0, Eligible.Num() - 1)];
}

namespace
{
	/** B-522b: a prebuilt find's gem or evolution into its just-given piece, through the owner's own socket calls (a gem
	 *  is added then socketed at full durability; an evolution becomes the owner's instance, then attached).
	 *  shortcut: a full gem pouch or evolution count leaves the socket empty (the piece still arrives, a gem stays in the
	 *  pouch); write the socket directly if finds must always arrive built. */
	void FillFoundSocket(AActor &Owner, UInventoryComponent &Inventory, const UPrebuiltFindData &Find, const FGuid &PieceID, bool bWeapon)
	{
		bool bFilled = true;
		if (Find.Crystal.Type != ECrystalType::None)
		{
			bFilled = Inventory.AddCrystal(Find.Crystal) &&
					  (bWeapon ? Inventory.AttachCrystalToWeapon(PieceID, Find.Crystal) : Inventory.AttachCrystalToRing(PieceID, Find.Crystal));
		}
		else if (Find.Evolution)
		{
			const UEvolutionInventoryComponent *Evolutions = Owner.FindComponentByClass<UEvolutionInventoryComponent>();
			bFilled = Evolutions && Inventory.AddEvolutionInstance(Find.Evolution);
			if (bFilled)
			{
				const FGuid EvolutionID = Evolutions->Entries.Last().InstanceID;
				bFilled = bWeapon ? Inventory.AttachEvolutionToWeapon(PieceID, EvolutionID) : Inventory.AttachEvolutionToRing(PieceID, EvolutionID);
			}
		}
		if (!bFilled)
		{
			UE_LOG(LogWoREconomy, Warning, TEXT("[TrialLoot] %s: the piece arrived but its socket could not be filled (full pouch or evolutions?)"),
				   *Find.GetName());
			return; // an empty piece is not prebuilt: what the player sockets later stays theirs to take out
		}
		// D-231 (B-532): the piece remembers it came built, and whether its own crystal may come out.
		const auto MarkPrebuilt = [&Find](auto *Piece)
		{
			if (Piece && !Piece->AttachedItem.IsEmpty()) // a find with nothing to socket arrives as a plain piece
			{
				Piece->bPrebuilt = true;
				Piece->bCrystalRemovable = Find.bCrystalRemovable;
			}
		};
		if (bWeapon)
		{
			MarkPrebuilt(Inventory.FindOwnedWeapon(PieceID));
		}
		else
		{
			MarkPrebuilt(Inventory.FindOwnedRing(PieceID));
		}
		Inventory.RefreshLoadoutsFromOwned(PieceID); // the flags to the pool and any loadout copy
	}
}

bool TrialLoot::CanGrant(const UObject *Item)
{
	return Cast<UWeaponData>(Item) || Cast<URingData>(Item) || Cast<USpellData>(Item) || Cast<UAbilityData>(Item) ||
		   Cast<UEvolutionItemData>(Item) || Cast<UPrebuiltFindData>(Item);
}

bool TrialLoot::GrantTo(AActor *Owner, UObject *Item)
{
	UInventoryComponent *Inventory = Owner ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Inventory || !Item)
	{
		return false;
	}
	if (UWeaponData *Weapon = Cast<UWeaponData>(Item))
	{
		return Inventory->AddWeapon(Weapon);
	}
	if (URingData *Ring = Cast<URingData>(Item))
	{
		return Inventory->AddRing(Ring);
	}
	if (USpellData *Spell = Cast<USpellData>(Item))
	{
		return Inventory->LearnSpell(Spell);
	}
	if (UAbilityData *Ability = Cast<UAbilityData>(Item))
	{
		return Inventory->LearnAbility(Ability);
	}
	if (UEvolutionItemData *Evolution = Cast<UEvolutionItemData>(Item))
	{
		return Inventory->AddEvolutionInstance(Evolution); // B-566: a loose evolution, as a prebuilt find's is added
	}
	if (const UPrebuiltFindData *Find = Cast<UPrebuiltFindData>(Item))
	{
		const bool bWeapon = Cast<UWeaponData>(Find->Piece) != nullptr;
		if (!(bWeapon || Cast<URingData>(Find->Piece)) || !GrantTo(Owner, Find->Piece))
		{
			return false;
		}
		// AddWeapon / AddRing append the new piece.
		const FGuid PieceID = bWeapon ? Inventory->GetWeaponAt(Inventory->GetWeaponCount() - 1).PersistentID
									  : Inventory->GetRingAt(Inventory->GetRingCount() - 1).PersistentID;
		FillFoundSocket(*Owner, *Inventory, *Find, PieceID, bWeapon);
		return true;
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
