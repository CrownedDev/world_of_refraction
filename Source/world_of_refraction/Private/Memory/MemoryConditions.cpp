// MemoryConditions.cpp

#include "Memory/MemoryConditions.h"

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionStructs.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/EquipmentDataBase.h"
#include "Equipment/Weapons/WeaponData.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryUnlock.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRLog.h"

namespace
{
	bool IsInParty(const UCharacterData *Character, const FMemoryEvent &Event)
	{
		return Event.Party.ContainsByPredicate([Character](const FMemoryEventMember &Member) { return Member.Character == Character; });
	}

	/** A level-up's wearer can be Condition.Character: whoever levelled it, or (unknown) anyone in the party. */
	bool IsUpgradedBy(const FMemoryCondition &Condition, const FMemoryEvent &Event)
	{
		return !Condition.Character || (Event.UpgradedBy ? Event.UpgradedBy == Condition.Character : IsInParty(Condition.Character, Event));
	}

	/** Who levelled it, as far as requirements go (unknown and no character named: null). */
	const UCharacterData *GetUpgrader(const FMemoryCondition &Condition, const FMemoryEvent &Event)
	{
		return Event.UpgradedBy ? Event.UpgradedBy : Condition.Character.Get();
	}

	bool IsGearMet(const FMemoryCondition &Condition, const FMemoryEvent &Event)
	{
		if (!Condition.Item)
		{
			return false;
		}
		const auto IsLevelled = [&Condition](EItemTier Tier)
		{
			return MemoryUnlock::GetUpgradeSteps(*Condition.Item, Tier) >= Condition.UpgradeLevel;
		};
		for (const FMemoryEventMember &Member : Event.Party)
		{
			if (Condition.Character && Member.Character != Condition.Character)
			{
				continue;
			}
			for (const TPair<const UEquipmentDataBase *, EItemTier> &Worn : Member.Worn)
			{
				if (Worn.Key == Condition.Item && IsLevelled(Worn.Value) &&
					MemoryConditions::FitsRequirements(Condition, &Condition.Item->Requirements, Member.Character))
				{
					return true;
				}
			}
		}
		// A gear upgrade: the levelled item counts as worn by whoever levelled it (unknown: anyone in
		// the party).
		return Event.UpgradedItem == Condition.Item && IsLevelled(Event.UpgradedTier) && IsUpgradedBy(Condition, Event) &&
			   MemoryConditions::FitsRequirements(Condition, &Condition.Item->Requirements, GetUpgrader(Condition, Event));
	}

	/** Some party member (Condition.Character when set) satisfies Holds. */
	template <class TPredicate>
	bool AnyMember(const FMemoryCondition &Condition, const FMemoryEvent &Event, TPredicate Holds)
	{
		return Event.Party.ContainsByPredicate([&](const FMemoryEventMember &Member)
		{
			return (!Condition.Character || Member.Character == Condition.Character) && Holds(Member);
		});
	}

	bool IsEvolutionMet(const FMemoryCondition &Condition, const FMemoryEvent &Event)
	{
		if (!Condition.Evolution)
		{
			return false;
		}
		const auto IsLevelled = [&Condition](EItemTier Tier)
		{
			return static_cast<int32>(Tier) - static_cast<int32>(Condition.Evolution->Tier) >= Condition.UpgradeLevel;
		};
		if (AnyMember(Condition, Event, [&](const FMemoryEventMember &Member)
		{
			return Member.Evolution == Condition.Evolution && IsLevelled(Member.EvolutionTier) &&
				   MemoryConditions::FitsRequirements(Condition, nullptr, Member.Character);
		}))
		{
			return true;
		}
		// An evolution level-up, as for gear: whoever levelled it (unknown: anyone in the party).
		if (Event.UpgradedEvolution != Condition.Evolution || !IsLevelled(Event.UpgradedTier))
		{
			return false;
		}
		return IsUpgradedBy(Condition, Event) && MemoryConditions::FitsRequirements(Condition, nullptr, GetUpgrader(Condition, Event));
	}

	/** The evolution in Actor's primary slot and its level: the owned entry's, since a level-up
	 *  writes that and not the loadout's copy. TODO(B-085): loadouts will point at owned gear. */
	TPair<const UEvolutionItemData *, EItemTier> GetPrimaryEvolution(const AActor *Actor)
	{
		const UInventoryComponent *Inventory = Actor ? Actor->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!Inventory || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
		{
			return {nullptr, EItemTier::F_Tier};
		}
		const FCombatLoadout &Loadout = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		if (Loadout.PrimarySlotType != EPrimarySlotType::Evolution || !Loadout.PrimaryEvolution.Item)
		{
			return {nullptr, EItemTier::F_Tier};
		}
		const UEvolutionInventoryComponent *Owned = Actor->FindComponentByClass<UEvolutionInventoryComponent>();
		const FEvolutionInventoryEntry *Entry = Owned && Loadout.PrimaryEvolutionInstance.IsValid()
			? Owned->Entries.FindByPredicate([&Loadout](const FEvolutionInventoryEntry &Candidate)
			  {
				  return Candidate.InstanceID == Loadout.PrimaryEvolutionInstance;
			  })
			: nullptr;
		return {Loadout.PrimaryEvolution.Item, Entry ? Entry->Tier : Loadout.PrimaryEvolution.Tier};
	}
}

FMemoryEvent MemoryConditions::MakeEvent(const UGameInstance *GameInstance, EMemorySource Source)
{
	FMemoryEvent Event;
	Event.Source = Source;
	const UPartySessionSubsystem *PartySession = GameInstance ? GameInstance->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	if (const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr)
	{
		for (const FPartyMember &Member : Party->Members)
		{
			if (Member.CharacterData)
			{
				Event.Party.Add({Member.CharacterData, {}});
			}
		}
	}
	const UTrialRunSubsystem *TrialRun = GameInstance ? GameInstance->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	if (const UTrialData *Trial = TrialRun ? TrialRun->GetActiveTrial() : nullptr)
	{
		Event.Locations.Add(Trial->GetFName());
	}
	return Event;
}

FMemoryEventMember MemoryConditions::MakeMember(const AActor *Combatant)
{
	const UCharacterDataComponent *DataComponent = Combatant ? Combatant->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	FMemoryEventMember Member{DataComponent ? DataComponent->CharacterData : nullptr, MemoryUnlock::GetWornWithTiers(Combatant)};
	const TPair<const UEvolutionItemData *, EItemTier> Evolution = GetPrimaryEvolution(Combatant);
	Member.Evolution = Evolution.Key;
	Member.EvolutionTier = Evolution.Value;
	if (const ULoadoutComponent *Loadout = Combatant ? Combatant->FindComponentByClass<ULoadoutComponent>() : nullptr)
	{
		for (const USkillDataBase *Attack : Loadout->GetAllWeaponAttacks())
		{
			Member.Slotted.AddUnique(Attack);
		}
		for (const USkillDataBase *Ability : Loadout->GetAvailableAbilities())
		{
			Member.Slotted.AddUnique(Ability);
		}
		for (const USkillDataBase *Spell : Loadout->GetAvailableSpells())
		{
			Member.Slotted.AddUnique(Spell);
		}
		Member.Slotted.Remove(nullptr);
	}
	return Member;
}

void MemoryConditions::RecordAction(FMemoryFightUsage &Usage, const AActor *Actor, const FAction &Action)
{
	// A deferred spell reports when armed and again when it fires: AddUnique keeps one.
	const USkillDataBase *Skill = Action.SpellData ? Action.SpellData : Action.SkillData;
	if (Skill)
	{
		Usage.Skills.AddUnique(Skill);
	}
	// Breakthrough spells come from the evolution in the primary slot.
	if (Action.SpellData && Action.SpellSource == ESpellSource::Evolution)
	{
		if (const UEvolutionItemData *Evolution = GetPrimaryEvolution(Actor).Key)
		{
			Usage.Breakthroughs.AddUnique(Evolution);
		}
	}
}

void MemoryConditions::RecordKill(FMemoryFightUsage &Usage, const AActor *Killer)
{
	const ULoadoutComponent *Loadout = Killer ? Killer->FindComponentByClass<ULoadoutComponent>() : nullptr;
	if (const UWeaponData *Weapon = Loadout ? Loadout->GetActiveWeapon() : nullptr)
	{
		Usage.FinishingWeapons.AddUnique(Weapon);
	}
}

bool MemoryConditions::IsMet(const FMemoryCondition &Condition, const FMemoryEvent &Event, const UMemoryJournalSubsystem *Journal)
{
	switch (Condition.Kind)
	{
	case EMemoryConditionKind::Character:
		return Condition.Character && IsInParty(Condition.Character, Event);
	case EMemoryConditionKind::GearEquipped:
		return IsGearMet(Condition, Event);
	case EMemoryConditionKind::Location:
		return !Condition.LocationId.IsNone() && Event.Locations.Contains(Condition.LocationId);
	case EMemoryConditionKind::EnemyBeaten:
		return Condition.Enemy && Event.Beaten.Contains(Condition.Enemy.Get());
	case EMemoryConditionKind::NoDamageTaken:
		return Event.bNoDamageTaken;
	case EMemoryConditionKind::PriorMemory:
		return Condition.PriorMemory && Journal && Journal->HasMemory(Condition.PriorMemory);
	case EMemoryConditionKind::Hidden:
		return false;
	case EMemoryConditionKind::EvolutionEquipped:
		return IsEvolutionMet(Condition, Event);
	case EMemoryConditionKind::SkillSlotted:
		return Condition.Skill && AnyMember(Condition, Event, [&](const FMemoryEventMember &Member)
		{
			return Member.Slotted.Contains(Condition.Skill.Get()) && FitsRequirements(Condition, &Condition.Skill->Requirements, Member.Character);
		});
	case EMemoryConditionKind::SkillUsed:
		return Condition.Skill && AnyMember(Condition, Event, [&](const FMemoryEventMember &Member) { return Member.Used.Skills.Contains(Condition.Skill.Get()); });
	case EMemoryConditionKind::BreakthroughUsed:
		return Condition.Evolution &&
			   AnyMember(Condition, Event, [&](const FMemoryEventMember &Member) { return Member.Used.Breakthroughs.Contains(Condition.Evolution.Get()); });
	case EMemoryConditionKind::FinishingBlow:
		return Condition.Item &&
			   AnyMember(Condition, Event, [&](const FMemoryEventMember &Member) { return Member.Used.FinishingWeapons.Contains(Condition.Item.Get()); });
	}
	return false;
}

bool MemoryConditions::FitsRequirements(const FMemoryCondition &Condition, const FWorldStatRequirements *Requirements,
										 const UCharacterData *Wearer)
{
	if (Condition.RequirementFit == EMemoryRequirementFit::Any)
	{
		return true;
	}
	if (!Wearer)
	{
		return false;
	}
	if (!Requirements || !Requirements->HasRequirements())
	{
		return Condition.RequirementFit == EMemoryRequirementFit::Met;
	}
	// Per pillar: the wearer's world level, and what is required. TODO: the asset's levels, as the
	// D-082 gap reads today; switch both to the run's live stats (UCharacterDataComponent::GetWorldStat)
	// so a pillar raised mid-run counts.
	const TPair<int32, int32> Pillars[] = {{Wearer->WorldMindLevel, Requirements->RequiredWorldMind},
										   {Wearer->WorldBodyLevel, Requirements->RequiredWorldBody},
										   {Wearer->WorldSpiritLevel, Requirements->RequiredWorldSpirit}};
	switch (Condition.RequirementFit)
	{
	case EMemoryRequirementFit::Any:
		return true;
	case EMemoryRequirementFit::Met:
		return Requirements->MeetsRequirements(Wearer);
	case EMemoryRequirementFit::Above:
		return Requirements->MeetsRequirements(Wearer) && Algo::AllOf(Pillars, [&Condition](const TPair<int32, int32> &Pillar)
		{
			return Pillar.Value == 0 || Pillar.Key - Pillar.Value >= Condition.RequirementMargin;
		});
	case EMemoryRequirementFit::Under:
	{
		const int32 Short = FMath::Max(Condition.RequirementMargin, 1);
		return Algo::AnyOf(Pillars, [Short](const TPair<int32, int32> &Pillar) { return Pillar.Value - Pillar.Key >= Short; });
	}
	}
	return false;
}

bool MemoryConditions::AreMet(const UMemoryData &Memory, const FMemoryEvent &Event, const UMemoryJournalSubsystem *Journal)
{
	// Q-209c: any one route opens it; none authored: only the source's naming it counts.
	const TArray<TArray<FMemoryCondition>> Sets = Memory.GetConditionSets();
	return Sets.IsEmpty() || Sets.ContainsByPredicate([&](const TArray<FMemoryCondition> &Set)
	{
		return Algo::AllOf(Set, [&](const FMemoryCondition &Condition) { return IsMet(Condition, Event, Journal); });
	});
}

TArray<UMemoryData *> MemoryConditions::GatherConditional()
{
	const IAssetRegistry &Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UMemoryData::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);
	// The registry's order isn't fixed; a chain should unlock the same way every time.
	Assets.Sort([](const FAssetData &A, const FAssetData &B) { return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString(); });

	TArray<UMemoryData *> Conditional;
	for (const FAssetData &Asset : Assets)
	{
		UMemoryData *Memory = Cast<UMemoryData>(Asset.GetAsset());
		if (Memory && Memory->HasConditions())
		{
			Conditional.Add(Memory);
		}
	}
	return Conditional;
}

int32 MemoryConditions::CheckEvent(UGameInstance *GameInstance, const FMemoryEvent &Event, const TArray<UMemoryData *> &Conditional)
{
	const UMemoryJournalSubsystem *Journal = GameInstance ? GameInstance->GetSubsystem<UMemoryJournalSubsystem>() : nullptr;
	if (!Journal)
	{
		return 0;
	}
	TArray<UMemoryData *> Candidates = Event.Named;
	for (UMemoryData *Memory : Conditional)
	{
		Candidates.AddUnique(Memory);
	}
	int32 Unlocked = 0;
	// Each pass can earn a memory another one needs (PriorMemory); stop once a pass earns nothing.
	for (bool bEarned = true; bEarned;)
	{
		bEarned = false;
		for (UMemoryData *Memory : Candidates)
		{
			if (Memory && !Journal->HasMemory(Memory) && AreMet(*Memory, Event, Journal) &&
				MemoryUnlock::Unlock(GameInstance, Memory, Event.Source))
			{
				++Unlocked;
				bEarned = true;
			}
		}
	}
	if (Unlocked > 0)
	{
		UE_LOG(LogWoR, Log, TEXT("[MemoryConditions] %s: %d memory(ies) unlocked"),
			   *UEnum::GetDisplayValueAsText(Event.Source).ToString(), Unlocked);
	}
	return Unlocked;
}

int32 MemoryConditions::CheckEvent(UGameInstance *GameInstance, const FMemoryEvent &Event)
{
	return CheckEvent(GameInstance, Event, GatherConditional());
}

FString MemoryConditions::GetConditionsString(const UMemoryData &Memory, const FMemoryEvent &Event, const UMemoryJournalSubsystem *Journal)
{
	TArray<FString> Routes;
	for (const TArray<FMemoryCondition> &Set : Memory.GetConditionSets())
	{
		Routes.Add(FString::JoinBy(Set, TEXT(" + "), [&](const FMemoryCondition &Condition)
		{
			return FString::Printf(TEXT("%s [%s]"), *UMemoryData::GetConditionString(Condition),
								   IsMet(Condition, Event, Journal) ? TEXT("met") : TEXT("not yet"));
		}));
	}
	const bool bCollected = Journal && Journal->HasMemory(&Memory);
	return FString::Printf(TEXT("%s%s: %s"), *Memory.GetMemoryId().ToString(), bCollected ? TEXT(" (collected)") : TEXT(""),
						   Routes.IsEmpty() ? TEXT("by its source only") : *FString::Join(Routes, TEXT(" OR ")));
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GMemoryConditionsCommand(
	TEXT("wor.Memory.Conditions"),
	TEXT("Print every memory with unlock conditions, each marked met or not for the party now, out of a fight (B-209)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		const UMemoryJournalSubsystem *Journal = GameInstance ? GameInstance->GetSubsystem<UMemoryJournalSubsystem>() : nullptr;
		const FMemoryEvent Now = MemoryConditions::MakeEvent(GameInstance, EMemorySource::Fragment);
		const TArray<UMemoryData *> Conditional = MemoryConditions::GatherConditional();
		UE_LOG(LogWoR, Display, TEXT("%d memory(ies) with unlock conditions (party of %d)"), Conditional.Num(), Now.Party.Num());
		for (const UMemoryData *Memory : Conditional)
		{
			UE_LOG(LogWoR, Display, TEXT("  %s"), *MemoryConditions::GetConditionsString(*Memory, Now, Journal));
		}
	}));
#endif
