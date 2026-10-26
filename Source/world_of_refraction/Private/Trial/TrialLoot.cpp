// TrialLoot.cpp

#include "Trial/TrialLoot.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Equipment/EquipmentDataBase.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Actor.h"
#include "Inventory/InventoryComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/SpellData.h"

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
	for (const UClass *LootClass : {UEquipmentDataBase::StaticClass(), USkillDataBase::StaticClass()})
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
	return Matches;
}

UObject *TrialLoot::PickFromPool(const TArray<UObject *> &Candidates, const FGameplayTag &Pool, int32 Seed)
{
	TArray<UObject *> Eligible;
	for (UObject *Candidate : Candidates)
	{
		const FGameplayTagContainer *Tags = GetSourceTags(Candidate);
		if (Pool.IsValid() && Tags && Tags->HasTag(Pool))
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
	return false;
}
