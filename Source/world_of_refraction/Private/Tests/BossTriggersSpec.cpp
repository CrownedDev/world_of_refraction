// BossTriggersSpec.cpp
// B-211 (D-100 part 1, Q-211): a boss's own trigger fires when every condition holds in the fight
// (who is in the party, what they wear and its level): the right wearer and item weaken the boss,
// the wrong wearer doesn't; steps add up; phases change; a memory no longer changes the boss.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Boss/BossData.h"
#include "Combat/Boss/BossPhaseComponent.h"
#include "Combat/Boss/BossTriggers.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryUnlock.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	const FName BOSS_TRIGGER_TEST_ID(TEXT("SpecBossTrigger"));

	template <class TComponent>
	TComponent *BossTriggerAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}
}

BEGIN_DEFINE_SPEC(FBossTriggersSpec, "WoR.Boss.Triggers",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UCharacterData *Lord = nullptr;
	UWeaponData *OldSword = nullptr;
	UBossData *BossData = nullptr;

	/** A party member for Data wearing Weapon as its primary. */
	AActor *SpawnWearing(UCharacterData *Data, UWeaponData *Weapon)
	{
		AActor *Actor = TestWorld->SpawnCombatant(Data);
		UInventoryComponent *Inventory = BossTriggerAddComponent<UInventoryComponent>(Actor);
		ULoadoutComponent *Loadout = BossTriggerAddComponent<ULoadoutComponent>(Actor);
		Loadout->InitializeFromCharacterData(Data, Inventory);
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.PrimarySlotType = EPrimarySlotType::Weapon;
		Active.PrimaryWeapon.WeaponEntry.Weapon = Weapon;
		return Actor;
	}

	/** "If Lord wears the old sword -> Results". */
	FBossTrigger LordWithOldSword(const TArray<EBossTriggerResult> &Results) const
	{
		FBossTriggerCondition Worn;
		Worn.Kind = EBossTriggerConditionKind::GearWorn;
		Worn.Character = Lord;
		Worn.Item = OldSword;
		FBossTrigger Trigger;
		Trigger.TriggerId = BOSS_TRIGGER_TEST_ID;
		Trigger.Conditions.Add(Worn);
		Trigger.Results = Results;
		return Trigger;
	}

	AActor *SpawnBoss()
	{
		return TestWorld->SpawnCombatant(BossData->Boss, true);
	}

	int32 MaxHPOf(const AActor *Actor) const
	{
		return Actor->FindComponentByClass<UCharacterDataComponent>()->MaxHP;
	}
END_DEFINE_SPEC(FBossTriggersSpec)

void FBossTriggersSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Lord = FWoRTestWorld::NewCharacterData();
		Lord->CharacterClass = ECharacterClass::Generic;
		OldSword = NewObject<UWeaponData>(GetTransientPackage());
		BossData = NewObject<UBossData>(GetTransientPackage());
		BossData->Boss = FWoRTestWorld::NewCharacterData();
	});

	AfterEach([this]()
	{
		Lord = nullptr;
		OldSword = nullptr;
		BossData = nullptr;
		TestWorld.Reset();
	});

	It("weakens the boss when its character wears the item, not on someone else", [this]()
	{
		BossData->Triggers.Add(LordWithOldSword({EBossTriggerResult::Weaken}));
		UCharacterData *Other = FWoRTestWorld::NewCharacterData();
		Other->CharacterClass = ECharacterClass::Generic;

		AActor *Untouched = SpawnBoss();
		const int32 BaseMaxHP = MaxHPOf(Untouched);
		TestEqual(TEXT("Someone else wears it: none"), BossTriggers::ApplyTo(Untouched, *BossData, {SpawnWearing(Other, OldSword)}), 0);
		TestEqual(TEXT("Untouched"), MaxHPOf(Untouched), BaseMaxHP);

		AActor *Boss = SpawnBoss();
		TestEqual(TEXT("Its character wears it: fires"), BossTriggers::ApplyTo(Boss, *BossData, {SpawnWearing(Lord, OldSword)}), 1);
		TestTrue(TEXT("The boss is weaker"), MaxHPOf(Boss) < BaseMaxHP);
		const UCharacterDataComponent *Stats = Boss->FindComponentByClass<UCharacterDataComponent>();
		TestEqual(TEXT("It starts at full HP"), Stats->CurrentHP, Stats->MaxHP);
	});

	It("adds up: weaken and strengthen cancel; a party member alone can fire one", [this]()
	{
		BossData->Triggers.Add(LordWithOldSword({EBossTriggerResult::Weaken}));
		FBossTriggerCondition InParty;
		InParty.Kind = EBossTriggerConditionKind::PartyMember;
		InParty.Character = Lord;
		FBossTrigger Present;
		Present.Conditions.Add(InParty);
		Present.Results.Add(EBossTriggerResult::Strengthen);
		BossData->Triggers.Add(Present);

		AActor *Boss = SpawnBoss();
		const int32 BaseMaxHP = MaxHPOf(Boss);
		TestEqual(TEXT("Both fire"), BossTriggers::ApplyTo(Boss, *BossData, {SpawnWearing(Lord, OldSword)}), 2);
		TestEqual(TEXT("Net zero: max HP as it was"), MaxHPOf(Boss), BaseMaxHP);
	});

	It("never fires with no conditions or a hidden one", [this]()
	{
		FBossTrigger Empty;
		Empty.Results.Add(EBossTriggerResult::Weaken);
		BossData->Triggers.Add(Empty);
		FBossTrigger Hidden = Empty;
		Hidden.Conditions.Add({EBossTriggerConditionKind::Hidden});
		BossData->Triggers.Add(Hidden);
		TestEqual(TEXT("None"), BossTriggers::ApplyTo(SpawnBoss(), *BossData, {SpawnWearing(Lord, OldSword)}), 0);
	});

	It("adds a phase to a boss with phases", [this]()
	{
		BossData->Triggers.Add(LordWithOldSword({EBossTriggerResult::AddPhase, EBossTriggerResult::Dialogue}));
		AActor *Boss = SpawnBoss();
		UBossPhaseComponent *Phases = BossTriggerAddComponent<UBossPhaseComponent>(Boss);
		Phases->Setup(BossData);
		const int32 BasePhases = Phases->GetPhaseCount();
		TestEqual(TEXT("Fires"), BossTriggers::ApplyTo(Boss, *BossData, {SpawnWearing(Lord, OldSword)}), 1);
		TestEqual(TEXT("One phase more"), Phases->GetPhaseCount(), BasePhases + 1);
	});

	It("adds or skips a phase at most once per boss, however many triggers ask", [this]()
	{
		BossData->Triggers.Add(LordWithOldSword({EBossTriggerResult::AddPhase}));
		BossData->Triggers.Add(LordWithOldSword({EBossTriggerResult::AddPhase}));
		AActor *Boss = SpawnBoss();
		UBossPhaseComponent *Phases = BossTriggerAddComponent<UBossPhaseComponent>(Boss);
		Phases->Setup(BossData);
		const int32 BasePhases = Phases->GetPhaseCount();
		TestEqual(TEXT("Both fire"), BossTriggers::ApplyTo(Boss, *BossData, {SpawnWearing(Lord, OldSword)}), 2);
		TestEqual(TEXT("Still one phase more"), Phases->GetPhaseCount(), BasePhases + 1);
	});

	It("leaves a boss alone that only a memory in the run names (memories no longer change bosses)", [this]()
	{
		UMemoryData *Memory = NewObject<UMemoryData>(GetTransientPackage());
		Memory->BossModifiers.Add({BossData->Boss, EMemoryBossModifierKind::Weaken});
		UGameInstance *GameInstance = TestWorld->GetWorld()->GetGameInstance();
		TestTrue(TEXT("Collected this run"), MemoryUnlock::Unlock(GameInstance, Memory, EMemorySource::Fragment) &&
												 TestWorld->GetSubsystem<UTrialRunSubsystem>()->GetRunMemories().Contains(Memory));

		AActor *Boss = SpawnBoss();
		const int32 BaseMaxHP = MaxHPOf(Boss);
		TestEqual(TEXT("No trigger fires"), BossTriggers::ApplyTo(Boss, *BossData, {SpawnWearing(Lord, OldSword)}), 0);
		TestEqual(TEXT("Max HP as it was"), MaxHPOf(Boss), BaseMaxHP);
	});

	It("reads the fight, not memories: an event with no party fires nothing", [this]()
	{
		BossData->Triggers.Add(LordWithOldSword({EBossTriggerResult::Weaken}));
		const FMemoryEvent NoParty;
		TestFalse(TEXT("No wearer"), BossTriggers::Fires(BossData->Triggers[0], NoParty));
		TestTrue(TEXT("Debug string names it"), UBossData::GetTriggerString(BossData->Triggers[0]).Contains(BOSS_TRIGGER_TEST_ID.ToString()));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
