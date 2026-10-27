// MemoryConditionsSpec.cpp
// B-209 (D-100 part 2, Q-209b): a memory unlocks when every unlock condition holds at an event:
// one spec per condition kind, a two-memory chain in one event, a source's own memory, once only;
// Q-209c: alternative condition sets (any one opens it) and rewards (logged on unlock);
// Q-209d: evolutions and skills equipped, slotted and used, and a fight's usage record;
// Q-209e: equipped while meeting, over or under the requirements.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/Actions/ActionStructs.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	const FName CONDITIONS_TEST_MEMORY_NAME(TEXT("Memory_ConditionsSpec"));
	const FName CONDITIONS_TEST_LOCATION(TEXT("Spot_ConditionsSpec"));
	const FName CONDITIONS_TEST_TITLE(TEXT("Title_ConditionsSpec"));
	constexpr int32 CONDITIONS_TEST_UPGRADE_LEVEL = 2;

	EItemTier ConditionsTestTierAbove(EItemTier Base, int32 Steps)
	{
		return static_cast<EItemTier>(FMath::Min(static_cast<int32>(Base) + Steps, static_cast<int32>(EItemTier::S_Tier)));
	}

	template <class TComponent>
	TComponent *ConditionsTestAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}
}

BEGIN_DEFINE_SPEC(FMemoryConditionsSpec, "WoR.Memory.Conditions",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UGameInstance *GameInstance = nullptr;
	UMemoryJournalSubsystem *Journal = nullptr;
	UCharacterData *Lord = nullptr;

	static UMemoryData *NewMemory(const TArray<FMemoryCondition> &Conditions)
	{
		UMemoryData *Memory = NewObject<UMemoryData>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(),
													 UMemoryData::StaticClass(), CONDITIONS_TEST_MEMORY_NAME));
		Memory->UnlockConditions = Conditions;
		return Memory;
	}

	static FMemoryCondition Condition(EMemoryConditionKind Kind)
	{
		FMemoryCondition Out;
		Out.Kind = Kind;
		return Out;
	}

	FMemoryEvent EventWithLord() const
	{
		FMemoryEvent Event;
		Event.Source = EMemorySource::FightWon;
		Event.Party.Add({Lord, {}});
		return Event;
	}

	/** True when Event unlocks Memory (and only it), checked on its own. */
	bool Unlocks(const FMemoryEvent &Event, UMemoryData *Memory)
	{
		return MemoryConditions::CheckEvent(GameInstance, Event, {Memory}) == 1 && Journal->HasMemory(Memory);
	}
END_DEFINE_SPEC(FMemoryConditionsSpec)

void FMemoryConditionsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		GameInstance = TestWorld->GetWorld() ? TestWorld->GetWorld()->GetGameInstance() : nullptr;
		Journal = TestWorld->GetSubsystem<UMemoryJournalSubsystem>();
		Lord = FWoRTestWorld::NewCharacterData();
		TestTrue(TEXT("Subsystems"), GameInstance && Journal);
	});

	AfterEach([this]()
	{
		GameInstance = nullptr;
		Journal = nullptr;
		Lord = nullptr;
		TestWorld.Reset();
	});

	It("needs its character in the party", [this]()
	{
		FMemoryCondition InParty = Condition(EMemoryConditionKind::Character);
		InParty.Character = Lord;
		UMemoryData *Memory = NewMemory({InParty});
		FMemoryEvent Stranger;
		Stranger.Party.Add({FWoRTestWorld::NewCharacterData(), {}});
		TestEqual(TEXT("Another party: none"), MemoryConditions::CheckEvent(GameInstance, Stranger, {Memory}), 0);
		TestTrue(TEXT("Its character: unlocked"), Unlocks(EventWithLord(), Memory));
		TestEqual(TEXT("From the event's source"), Journal->GetSource(Memory), TOptional<EMemorySource>(EMemorySource::FightWon));
		TestEqual(TEXT("Once only"), MemoryConditions::CheckEvent(GameInstance, EventWithLord(), {Memory}), 0);
	});

	It("needs gear worn by the right character at its upgrade level, or the upgraded item", [this]()
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		FMemoryCondition Gear = Condition(EMemoryConditionKind::GearEquipped);
		Gear.Item = Weapon;
		Gear.Character = Lord;
		Gear.UpgradeLevel = CONDITIONS_TEST_UPGRADE_LEVEL;
		const EItemTier Reached = ConditionsTestTierAbove(Weapon->Tier, CONDITIONS_TEST_UPGRADE_LEVEL);
		const EItemTier Short = ConditionsTestTierAbove(Weapon->Tier, CONDITIONS_TEST_UPGRADE_LEVEL - 1);

		FMemoryEvent OneShort = EventWithLord();
		OneShort.Party[0].Worn.Add({Weapon, Short});
		TestFalse(TEXT("One level short"), MemoryConditions::IsMet(Gear, OneShort, Journal));
		FMemoryEvent OnSomeoneElse;
		OnSomeoneElse.Party.Add({FWoRTestWorld::NewCharacterData(), {{Weapon, Reached}}});
		TestFalse(TEXT("Worn by someone else"), MemoryConditions::IsMet(Gear, OnSomeoneElse, Journal));
		FMemoryEvent Worn = EventWithLord();
		Worn.Party[0].Worn.Add({Weapon, Reached});
		TestTrue(TEXT("Worn by its character, levelled"), MemoryConditions::IsMet(Gear, Worn, Journal));

		FMemoryEvent Upgrade = EventWithLord();
		Upgrade.UpgradedItem = Weapon;
		Upgrade.UpgradedTier = Reached;
		TestTrue(TEXT("Upgraded with its character in the party"), MemoryConditions::IsMet(Gear, Upgrade, Journal));
		Upgrade.UpgradedBy = FWoRTestWorld::NewCharacterData();
		TestFalse(TEXT("Upgraded by someone else"), MemoryConditions::IsMet(Gear, Upgrade, Journal));
		Upgrade.UpgradedBy = Lord;
		TestTrue(TEXT("Upgraded by its character"), MemoryConditions::IsMet(Gear, Upgrade, Journal));
		Upgrade.UpgradedBy = nullptr;
		Upgrade.Party.Reset();
		TestFalse(TEXT("Upgraded without its character"), MemoryConditions::IsMet(Gear, Upgrade, Journal));
	});

	It("reads a combatant's worn gear and its level", [this]()
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		AActor *Actor = TestWorld->SpawnCombatant(Lord);
		UInventoryComponent *Inventory = ConditionsTestAddComponent<UInventoryComponent>(Actor);
		ULoadoutComponent *Loadout = ConditionsTestAddComponent<ULoadoutComponent>(Actor);
		Loadout->InitializeFromCharacterData(Lord, Inventory);
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.PrimarySlotType = EPrimarySlotType::Weapon;
		Active.PrimaryWeapon.WeaponEntry.Weapon = Weapon;
		Active.PrimaryWeapon.WeaponEntry.Tier = ConditionsTestTierAbove(Weapon->Tier, CONDITIONS_TEST_UPGRADE_LEVEL);

		const FMemoryEventMember Member = MemoryConditions::MakeMember(Actor);
		TestEqual(TEXT("Its character"), Member.Character, static_cast<const UCharacterData *>(Lord));
		TestTrue(TEXT("Its weapon at its level"), Member.Worn.Contains(TPair<const UEquipmentDataBase *, EItemTier>(
													  Weapon, Active.PrimaryWeapon.WeaponEntry.Tier)));
	});

	It("needs its location, a beaten enemy and no damage taken", [this]()
	{
		FMemoryCondition At = Condition(EMemoryConditionKind::Location);
		At.LocationId = CONDITIONS_TEST_LOCATION;
		UCharacterData *Rival = FWoRTestWorld::NewCharacterData();
		FMemoryCondition Beaten = Condition(EMemoryConditionKind::EnemyBeaten);
		Beaten.Enemy = Rival;
		const FMemoryCondition Unhurt = Condition(EMemoryConditionKind::NoDamageTaken);
		UMemoryData *Memory = NewMemory({At, Beaten, Unhurt});

		FMemoryEvent Event = EventWithLord();
		Event.Locations.Add(CONDITIONS_TEST_LOCATION);
		Event.Beaten.Add(Rival);
		TestFalse(TEXT("Hurt: none"), MemoryConditions::AreMet(*Memory, Event, Journal));
		Event.bNoDamageTaken = true;
		Event.Locations.Reset();
		TestFalse(TEXT("Elsewhere: none"), MemoryConditions::AreMet(*Memory, Event, Journal));
		Event.Locations.Add(CONDITIONS_TEST_LOCATION);
		Event.Beaten.Reset();
		TestFalse(TEXT("Rival not beaten: none"), MemoryConditions::AreMet(*Memory, Event, Journal));
		Event.Beaten.Add(Rival);
		TestTrue(TEXT("All three: unlocked"), Unlocks(Event, Memory));
	});

	It("chains: a memory earned in an event meets the next one's prior memory", [this]()
	{
		FMemoryCondition InParty = Condition(EMemoryConditionKind::Character);
		InParty.Character = Lord;
		UMemoryData *First = NewMemory({InParty});
		FMemoryCondition After = Condition(EMemoryConditionKind::PriorMemory);
		After.PriorMemory = First;
		UMemoryData *Second = NewMemory({After});

		FMemoryEvent Empty;
		TestEqual(TEXT("Nothing met: none"), MemoryConditions::CheckEvent(GameInstance, Empty, {Second, First}), 0);
		// Listed second-first: the chain must not depend on the order.
		TestEqual(TEXT("Both in one event"), MemoryConditions::CheckEvent(GameInstance, EventWithLord(), {Second, First}), 2);
		TestTrue(TEXT("The second too"), Journal->HasMemory(Second));
	});

	It("opens by any one route: the first set or an alternative", [this]()
	{
		FMemoryCondition At = Condition(EMemoryConditionKind::Location);
		At.LocationId = CONDITIONS_TEST_LOCATION;
		FMemoryCondition InParty = Condition(EMemoryConditionKind::Character);
		InParty.Character = Lord;
		UMemoryData *Memory = NewMemory({At});
		Memory->AlternativeConditionSets.Add({{InParty}});

		FMemoryEvent Stranger;
		Stranger.Party.Add({FWoRTestWorld::NewCharacterData(), {}});
		TestFalse(TEXT("Neither route"), MemoryConditions::AreMet(*Memory, Stranger, Journal));
		Stranger.Locations.Add(CONDITIONS_TEST_LOCATION);
		TestTrue(TEXT("The first route"), MemoryConditions::AreMet(*Memory, Stranger, Journal));
		TestTrue(TEXT("The alternative route"), MemoryConditions::AreMet(*Memory, EventWithLord(), Journal));

		UMemoryData *OnlyAlternative = NewMemory({});
		OnlyAlternative->AlternativeConditionSets.Add({{InParty}});
		TestTrue(TEXT("Alternatives alone count as conditions"), OnlyAlternative->HasConditions());
		FMemoryEvent Pickup;
		Pickup.Named.Add(OnlyAlternative);
		TestEqual(TEXT("Named but unmet: not opened"), MemoryConditions::CheckEvent(GameInstance, Pickup, {}), 0);
		TestTrue(TEXT("Debug string shows both routes"), MemoryConditions::GetConditionsString(*Memory, Stranger, Journal).Contains(TEXT(" OR ")));
	});

	It("unlocks a memory with rewards and lists them", [this]()
	{
		UMemoryData *Memory = NewMemory({});
		FMemoryReward Title;
		Title.Kind = EMemoryRewardKind::Title;
		Title.RewardId = CONDITIONS_TEST_TITLE;
		Memory->Rewards.Add(Title);
		FMemoryEvent Pickup;
		Pickup.Named.Add(Memory);
		TestEqual(TEXT("Unlocked (the reward is logged)"), MemoryConditions::CheckEvent(GameInstance, Pickup, {}), 1);
		TestTrue(TEXT("Debug string lists the reward"), Memory->GetMemoryString().Contains(CONDITIONS_TEST_TITLE.ToString()));
	});

	It("meets evolution, skill, Breakthrough and finishing-blow conditions", [this]()
	{
		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		FMemoryCondition Equipped = Condition(EMemoryConditionKind::EvolutionEquipped);
		Equipped.Evolution = Evolution;
		Equipped.Character = Lord;
		Equipped.UpgradeLevel = CONDITIONS_TEST_UPGRADE_LEVEL;
		FMemoryCondition Slotted = Condition(EMemoryConditionKind::SkillSlotted);
		Slotted.Skill = Spell;
		FMemoryCondition Used = Condition(EMemoryConditionKind::SkillUsed);
		Used.Skill = Spell;
		FMemoryCondition Breakthrough = Condition(EMemoryConditionKind::BreakthroughUsed);
		Breakthrough.Evolution = Evolution;
		FMemoryCondition Finisher = Condition(EMemoryConditionKind::FinishingBlow);
		Finisher.Item = Weapon;

		FMemoryEvent Event = EventWithLord();
		for (const FMemoryCondition &Each : {Equipped, Slotted, Used, Breakthrough, Finisher})
		{
			TestFalse(TEXT("Nothing equipped or used yet"), MemoryConditions::IsMet(Each, Event, Journal));
		}
		FMemoryEventMember &Member = Event.Party[0];
		Member.Evolution = Evolution;
		Member.EvolutionTier = ConditionsTestTierAbove(Evolution->Tier, CONDITIONS_TEST_UPGRADE_LEVEL - 1);
		TestFalse(TEXT("Evolution a level short"), MemoryConditions::IsMet(Equipped, Event, Journal));
		Member.EvolutionTier = ConditionsTestTierAbove(Evolution->Tier, CONDITIONS_TEST_UPGRADE_LEVEL);
		TestTrue(TEXT("Evolution equipped, levelled"), MemoryConditions::IsMet(Equipped, Event, Journal));
		Member.Slotted.Add(Spell);
		TestTrue(TEXT("Skill slotted"), MemoryConditions::IsMet(Slotted, Event, Journal));
		TestFalse(TEXT("Slotted is not used"), MemoryConditions::IsMet(Used, Event, Journal));
		Member.Used.Skills.Add(Spell);
		Member.Used.Breakthroughs.Add(Evolution);
		Member.Used.FinishingWeapons.Add(Weapon);
		TestTrue(TEXT("Skill used"), MemoryConditions::IsMet(Used, Event, Journal));
		TestTrue(TEXT("Breakthrough used"), MemoryConditions::IsMet(Breakthrough, Event, Journal));
		TestTrue(TEXT("Finishing blow"), MemoryConditions::IsMet(Finisher, Event, Journal));

		FMemoryEvent Stranger;
		Stranger.Party.Add(Member);
		Stranger.Party[0].Character = FWoRTestWorld::NewCharacterData();
		TestFalse(TEXT("Equipped by someone else"), MemoryConditions::IsMet(Equipped, Stranger, Journal));
		TestTrue(TEXT("Anyone's use counts when no character is set"), MemoryConditions::IsMet(Used, Stranger, Journal));

		FMemoryEvent Upgrade = EventWithLord();
		Upgrade.UpgradedEvolution = Evolution;
		Upgrade.UpgradedTier = ConditionsTestTierAbove(Evolution->Tier, CONDITIONS_TEST_UPGRADE_LEVEL);
		Upgrade.UpgradedBy = Lord;
		TestTrue(TEXT("A level-up counts the evolution as equipped"), MemoryConditions::IsMet(Equipped, Upgrade, Journal));
		Upgrade.UpgradedBy = Stranger.Party[0].Character;
		TestFalse(TEXT("Levelled by someone else"), MemoryConditions::IsMet(Equipped, Upgrade, Journal));
	});

	It("records what a combatant used in a fight", [this]()
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Lord, Weapon);
		if (!TestNotNull(TEXT("Combatant"), Loadout))
		{
			return;
		}
		AActor *Actor = Loadout->GetOwner();
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		FAction Cast;
		Cast.SpellData = Spell;
		Cast.SpellSource = ESpellSource::Evolution;

		FMemoryFightUsage Usage;
		MemoryConditions::RecordAction(Usage, Actor, Cast);
		MemoryConditions::RecordAction(Usage, Actor, Cast);
		TestEqual(TEXT("The spell, once"), Usage.Skills.Num(), 1);
		TestTrue(TEXT("No evolution in the primary slot: no Breakthrough"), Usage.Breakthroughs.IsEmpty());
		MemoryConditions::RecordKill(Usage, Actor);
		TestTrue(TEXT("The weapon it held for the kill"), Usage.FinishingWeapons.Contains(Weapon));

		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		UInventoryComponent *Inventory = Actor->FindComponentByClass<UInventoryComponent>();
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.PrimarySlotType = EPrimarySlotType::Evolution;
		Active.PrimaryEvolution.Item = Evolution;
		Active.PrimaryEvolution.Tier = ConditionsTestTierAbove(Evolution->Tier, CONDITIONS_TEST_UPGRADE_LEVEL);
		MemoryConditions::RecordAction(Usage, Actor, Cast);
		TestTrue(TEXT("A Breakthrough spell names its evolution"), Usage.Breakthroughs.Contains(Evolution));
		const FMemoryEventMember Member = MemoryConditions::MakeMember(Actor);
		TestEqual(TEXT("Its primary-slot evolution"), Member.Evolution, static_cast<const UEvolutionItemData *>(Evolution));
		TestEqual(TEXT("At its level"), Member.EvolutionTier, Active.PrimaryEvolution.Tier);
	});

	It("measures an equip condition against its requirements", [this]()
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->Requirements.RequiredWorldMind = 3;
		FMemoryCondition Gear = Condition(EMemoryConditionKind::GearEquipped);
		Gear.Item = Weapon;
		Lord->WorldMindLevel = 1;
		FMemoryEvent Event = EventWithLord();
		Event.Party[0].Worn.Add({Weapon, Weapon->Tier});
		TestTrue(TEXT("Any: requirements don't matter"), MemoryConditions::IsMet(Gear, Event, Journal));
		Gear.RequirementFit = EMemoryRequirementFit::Met;
		TestFalse(TEXT("Two short: not met"), MemoryConditions::IsMet(Gear, Event, Journal));
		Gear.RequirementFit = EMemoryRequirementFit::Under;
		Gear.RequirementMargin = 2;
		TestTrue(TEXT("Two under"), MemoryConditions::IsMet(Gear, Event, Journal));
		Gear.RequirementMargin = 3;
		TestFalse(TEXT("Not three under"), MemoryConditions::IsMet(Gear, Event, Journal));

		Lord->WorldMindLevel = 5;
		TestFalse(TEXT("Over it: not under"), MemoryConditions::IsMet(Gear, Event, Journal));
		Gear.RequirementFit = EMemoryRequirementFit::Met;
		TestTrue(TEXT("Met"), MemoryConditions::IsMet(Gear, Event, Journal));
		Gear.RequirementFit = EMemoryRequirementFit::Above;
		Gear.RequirementMargin = 2;
		TestTrue(TEXT("Two over"), MemoryConditions::IsMet(Gear, Event, Journal));
		Gear.RequirementMargin = 3;
		TestFalse(TEXT("Not three over"), MemoryConditions::IsMet(Gear, Event, Journal));

		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Requirements.RequiredWorldBody = 2;
		Lord->WorldBodyLevel = 0;
		FMemoryCondition Slotted = Condition(EMemoryConditionKind::SkillSlotted);
		Slotted.Skill = Spell;
		Slotted.RequirementFit = EMemoryRequirementFit::Under;
		Event.Party[0].Slotted.Add(Spell);
		TestTrue(TEXT("A spell slotted while under its requirements"), MemoryConditions::IsMet(Slotted, Event, Journal));

		FMemoryCondition NoRequirements = Condition(EMemoryConditionKind::EvolutionEquipped);
		NoRequirements.RequirementFit = EMemoryRequirementFit::Met;
		TestTrue(TEXT("Nothing required: always met"), MemoryConditions::FitsRequirements(NoRequirements, nullptr, Lord));
		NoRequirements.RequirementFit = EMemoryRequirementFit::Above;
		TestFalse(TEXT("Nothing required: never above"), MemoryConditions::FitsRequirements(NoRequirements, nullptr, Lord));
		TestFalse(TEXT("Unknown wearer: only Any fits"), MemoryConditions::FitsRequirements(Gear, &Weapon->Requirements, nullptr));
	});

	It("never meets a hidden condition (not built)", [this]()
	{
		UMemoryData *Memory = NewMemory({Condition(EMemoryConditionKind::Hidden)});
		TestEqual(TEXT("None"), MemoryConditions::CheckEvent(GameInstance, EventWithLord(), {Memory}), 0);
	});

	It("unlocks a source's own memory: always with no conditions, else when they hold", [this]()
	{
		UMemoryData *Plain = NewMemory({});
		FMemoryCondition InParty = Condition(EMemoryConditionKind::Character);
		InParty.Character = Lord;
		UMemoryData *ForLord = NewMemory({InParty});

		FMemoryEvent Pickup;
		Pickup.Named = {Plain, ForLord};
		TestEqual(TEXT("No conditions: unlocked; unmet: not"), MemoryConditions::CheckEvent(GameInstance, Pickup, {}), 1);
		TestTrue(TEXT("The plain one"), Journal->HasMemory(Plain));
		TestFalse(TEXT("Not the lord's"), Journal->HasMemory(ForLord));
		TestTrue(TEXT("Debug string marks it"), MemoryConditions::GetConditionsString(*ForLord, Pickup, Journal).Contains(TEXT("[not yet]")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
