// MemoryConditionsSpec.cpp
// B-209 (D-100 part 2, Q-209b): a memory unlocks when every unlock condition holds at an event:
// one spec per condition kind, a two-memory chain in one event, a source's own memory, once only.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryJournalSubsystem.h"

namespace
{
	const FName CONDITIONS_TEST_MEMORY_NAME(TEXT("Memory_ConditionsSpec"));
	const FName CONDITIONS_TEST_LOCATION(TEXT("Spot_ConditionsSpec"));
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
