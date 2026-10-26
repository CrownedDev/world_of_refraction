// ReactionTrigger.cpp

#include "Reaction/ReactionTrigger.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/EquipmentDataBase.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "HAL/IConsoleManager.h"
#include "Loadout/LoadoutComponent.h"
#include "Memory/MemoryData.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/EffectIdentity.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "WoRLog.h"

namespace ReactionTriggerConstants
{
	/** Reaction effect ids: base + the trigger's index, packed with the effect and payload index. */
	constexpr int32 REACTION_EFFECT_SOURCE_ID_BASE = 191000;
	constexpr int32 MEMORY_BOSS_HP_EFFECT_ID = 191001;
	constexpr int32 MEMORY_BOSS_DAMAGE_EFFECT_ID = 191002;
	/** One memory step on a boss's max HP and damage. TODO: tune. */
	constexpr float MEMORY_BOSS_STEP_PERCENT = 15.0f;
}

namespace
{
	const UCharacterData *GetReactionCharacter(const AActor *Actor)
	{
		const UCharacterDataComponent *CharComp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		return CharComp ? CharComp->CharacterData : nullptr;
	}

	USkillEffectManager *GetReactionEffectManager(const AActor *Actor)
	{
		const UGameInstance *GameInstance = Actor && Actor->GetWorld() ? Actor->GetWorld()->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<USkillEffectManager>() : nullptr;
	}

	/** Max HP may have changed: refresh the pools (current HP is left as it is). */
	void RecomputeReactionPools(AActor *Actor)
	{
		if (UCharacterDataComponent *CharComp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr)
		{
			CharComp->RecomputeMaxPools();
		}
	}
}

void UReactionTriggerData::PrintTriggers() const
{
	for (const FReactionTrigger &Trigger : Triggers)
	{
		UE_LOG(LogWoR, Log, TEXT("%s"), *ReactionTriggers::GetTriggerString(Trigger));
	}
}

TArray<FReactionTrigger> ReactionTriggers::GatherTriggers()
{
	const IAssetRegistry &Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UReactionTriggerData::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);
	// The registry's order isn't fixed; effect ids come from the index.
	Assets.Sort([](const FAssetData &A, const FAssetData &B) { return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString(); });

	TArray<FReactionTrigger> Triggers;
	for (const FAssetData &Asset : Assets)
	{
		if (const UReactionTriggerData *Data = Cast<UReactionTriggerData>(Asset.GetAsset()))
		{
			Triggers.Append(Data->Triggers);
		}
	}
	return Triggers;
}

TArray<const UEquipmentDataBase *> ReactionTriggers::GetWornItems(const AActor *Actor)
{
	TArray<const UEquipmentDataBase *> Worn;
	const ULoadoutComponent *Loadout = Actor ? Actor->FindComponentByClass<ULoadoutComponent>() : nullptr;
	if (!Loadout)
	{
		return Worn;
	}
	for (const UEquipmentDataBase *Item : TArray<const UEquipmentDataBase *>{
			 Loadout->GetPrimaryWeapon(), Loadout->GetSecondaryWeapon(), Loadout->GetPrimaryRing(), Loadout->GetActiveRing()})
	{
		if (Item)
		{
			Worn.AddUnique(Item);
		}
	}
	return Worn;
}

bool ReactionTriggers::Matches(const FReactionTrigger &Trigger, const AActor *Wearer, const AActor *Faced)
{
	if (Trigger.RequiredItems.IsEmpty() || !Trigger.FacingCharacter || GetReactionCharacter(Faced) != Trigger.FacingCharacter)
	{
		return false;
	}
	const TArray<const UEquipmentDataBase *> Worn = GetWornItems(Wearer);
	for (const UEquipmentDataBase *Required : Trigger.RequiredItems)
	{
		if (!Required || !Worn.Contains(Required))
		{
			return false;
		}
	}
	return true;
}

TArray<FFiredReaction> ReactionTriggers::FindFired(const TArray<FReactionTrigger> &Triggers, const TArray<AActor *> &Party,
												   const TArray<AActor *> &Opposing)
{
	TArray<FFiredReaction> Fired;
	for (int32 Index = 0; Index < Triggers.Num(); ++Index)
	{
		const FReactionTrigger &Trigger = Triggers[Index];
		bool bFound = false;
		for (AActor *Wearer : Party)
		{
			for (AActor *Faced : Opposing)
			{
				if (!bFound && Matches(Trigger, Wearer, Faced))
				{
					Fired.Add({&Trigger, Index, Wearer, Faced});
					bFound = true;
				}
			}
		}
	}
	return Fired;
}

int32 ReactionTriggers::ApplyFired(const TArray<FFiredReaction> &Fired)
{
	int32 Applied = 0;
	for (const FFiredReaction &Reaction : Fired)
	{
		USkillEffectManager *Effects = GetReactionEffectManager(Reaction.Wearer);
		if (!Reaction.Trigger || !Effects)
		{
			continue;
		}
		const FReactionTrigger &Trigger = *Reaction.Trigger;
		const int32 SourceId = ReactionTriggerConstants::REACTION_EFFECT_SOURCE_ID_BASE + Reaction.TriggerIndex;
		for (int32 EffectIndex = 0; EffectIndex < Trigger.Effects.Num(); ++EffectIndex)
		{
			const FSkillEffect &Effect = Trigger.Effects[EffectIndex];
			const FString Name = Effect.EffectName.IsEmpty() ? Trigger.TriggerId.ToString() : Effect.EffectName;
			for (int32 PayloadIndex = 0; PayloadIndex < Effect.Payloads.Num(); ++PayloadIndex)
			{
				const FSkillEffectPayload &Payload = Effect.Payloads[PayloadIndex];
				if (Payload.EffectType == ESkillEffectType::None)
				{
					continue;
				}
				// TODO: TargetCount (an ally, all enemies) - for now the wearer or the faced character.
				AActor *Target = Payload.Target == ETargetType::Enemy ? Reaction.Faced : Reaction.Wearer;
				FActiveSkillEffect Runtime = FActiveSkillEffect::BuildRuntimeFromPayload(
					Name, EffectIdentity::PackEffectID(SourceId, EffectIndex, PayloadIndex), Payload, Effect.Conditions,
					Effect.bStackable, Effect.MaxStacks, Effect.bFiresOncePerMatch, Payload.Element);
				// Comes from worn gear: no cleanse strips it (D-015).
				Runtime.bFromGear = true;
				Effects->ApplyEffect(Target, Runtime);
				RecomputeReactionPools(Target);
				++Applied;
			}
		}
		UE_LOG(LogWoR, Log, TEXT("[Reaction] %s fired: %s wears the items, facing %s (quest %s, dialogue %s)"),
			   *Trigger.TriggerId.ToString(), *GetNameSafe(Reaction.Wearer), *GetNameSafe(Reaction.Faced),
			   *Trigger.QuestId.ToString(), *Trigger.DialogueId.ToString());
	}
	return Applied;
}

int32 ReactionTriggers::ApplyMemoryBossModifiers(const TArray<TObjectPtr<UMemoryData>> &RunMemories, const TArray<AActor *> &Opposing)
{
	int32 Applied = 0;
	for (const UMemoryData *Memory : RunMemories)
	{
		if (!Memory)
		{
			continue;
		}
		for (const FMemoryBossModifier &Modifier : Memory->BossModifiers)
		{
			for (AActor *Boss : Opposing)
			{
				if (!Modifier.Boss || GetReactionCharacter(Boss) != Modifier.Boss)
				{
					continue;
				}
				USkillEffectManager *Effects = GetReactionEffectManager(Boss);
				const bool bWeaken = Modifier.Kind == EMemoryBossModifierKind::Weaken;
				if (!Effects || (!bWeaken && Modifier.Kind != EMemoryBossModifierKind::Strengthen))
				{
					// TODO(B-193): SkipPhase / AddPhase change the boss's phases.
					UE_LOG(LogWoR, Log, TEXT("[Reaction] Memory %s: %s on %s waits for boss phases (B-193)"),
						   *Memory->GetMemoryId().ToString(), *UEnum::GetValueAsString(Modifier.Kind), *GetNameSafe(Boss));
					continue;
				}
				FActiveSkillEffect HP = FActiveSkillEffect::CreatePersistent(TEXT("Memory (boss HP)"),
					ReactionTriggerConstants::MEMORY_BOSS_HP_EFFECT_ID,
					bWeaken ? ESkillEffectType::MaxHPDebuff : ESkillEffectType::MaxHPBuff,
					ReactionTriggerConstants::MEMORY_BOSS_STEP_PERCENT);
				HP.bFromGear = true;
				FActiveSkillEffect Damage = FActiveSkillEffect::CreatePersistent(TEXT("Memory (boss damage)"),
					ReactionTriggerConstants::MEMORY_BOSS_DAMAGE_EFFECT_ID,
					bWeaken ? ESkillEffectType::DamageDebuff : ESkillEffectType::DamageBuff,
					ReactionTriggerConstants::MEMORY_BOSS_STEP_PERCENT);
				Damage.bFromGear = true;
				Effects->ApplyEffect(Boss, HP);
				Effects->ApplyEffect(Boss, Damage);
				if (UCharacterDataComponent *CharComp = Boss->FindComponentByClass<UCharacterDataComponent>())
				{
					// Before the fight: the boss starts at its new full HP, like the trial's enemy strength.
					CharComp->RecomputeMaxPools();
					CharComp->CurrentHP = CharComp->MaxHP;
				}
				UE_LOG(LogWoR, Log, TEXT("[Reaction] Memory %s %s %s"), *Memory->GetMemoryId().ToString(),
					   bWeaken ? TEXT("weakens") : TEXT("strengthens"), *GetNameSafe(Boss));
				++Applied;
			}
		}
	}
	return Applied;
}

FString ReactionTriggers::GetTriggerString(const FReactionTrigger &Trigger)
{
	TArray<FString> Items;
	for (const UEquipmentDataBase *Item : Trigger.RequiredItems)
	{
		Items.Add(Item ? Item->GetDisplayName() : TEXT("<none>"));
	}
	return FString::Printf(TEXT("Reaction %s: wearing [%s] facing %s -> %d effect(s), quest %s, dialogue %s"),
						   *Trigger.TriggerId.ToString(), *FString::Join(Items, TEXT(", ")),
						   Trigger.FacingCharacter ? *Trigger.FacingCharacter->Name : TEXT("<none>"), Trigger.Effects.Num(),
						   *Trigger.QuestId.ToString(), *Trigger.DialogueId.ToString());
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommand GReactionPrintCommand(
	TEXT("wor.Reaction.Print"),
	TEXT("List every authored reaction trigger: the items, the faced character, the effects, quest and dialogue (B-191)."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		const TArray<FReactionTrigger> Triggers = ReactionTriggers::GatherTriggers();
		UE_LOG(LogWoR, Display, TEXT("%d reaction trigger(s)"), Triggers.Num());
		for (const FReactionTrigger &Trigger : Triggers)
		{
			UE_LOG(LogWoR, Display, TEXT("  %s"), *ReactionTriggers::GetTriggerString(Trigger));
		}
	}));
#endif
