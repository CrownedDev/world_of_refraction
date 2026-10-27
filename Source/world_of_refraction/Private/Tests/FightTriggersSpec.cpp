// FightTriggersSpec.cpp
// B-212 (Q-212): reactions and boss triggers fire mid-fight at their When, once per fight: an HP
// line fires when crossed (not at the start, not twice), a hit fires on its side only, a boss
// trigger at the boss's turn start keeps the boss's share of HP; fight-start ones fire at once.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Boss/BossData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/LoadoutComponent.h"
#include "Reaction/FightTriggers.h"

namespace
{
	const FName FIGHT_TRIGGER_TEST_ID(TEXT("SpecFightTrigger"));
	constexpr float FIGHT_TRIGGER_TEST_LINE = 50.0f;
	constexpr float FIGHT_TRIGGER_TEST_BELOW = 0.4f;
	constexpr float FIGHT_TRIGGER_TEST_HALF = 0.5f;
}

BEGIN_DEFINE_SPEC(FFightTriggersSpec, "WoR.Reaction.FightTriggers",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UCharacterData *Lord = nullptr;
	UCharacterData *BossCharacter = nullptr;
	UWeaponData *Sword = nullptr;
	UFightTriggers *Triggers = nullptr;

	/** "Lord wears the sword facing the boss", firing at When. */
	FReactionTrigger SwordReaction(EFightMoment Moment, EFightMomentSide Side) const
	{
		FReactionTrigger Reaction;
		Reaction.TriggerId = FIGHT_TRIGGER_TEST_ID;
		Reaction.RequiredItems.Add(Sword);
		Reaction.FacingCharacter = BossCharacter;
		Reaction.When.Moment = Moment;
		Reaction.When.Side = Side;
		Reaction.When.ThresholdPercent = FIGHT_TRIGGER_TEST_LINE;
		return Reaction;
	}

	AActor *SpawnWearer()
	{
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Lord, Sword);
		return Loadout ? Loadout->GetOwner() : nullptr;
	}

	static UCharacterDataComponent *StatsOf(const AActor *Actor)
	{
		return Actor->FindComponentByClass<UCharacterDataComponent>();
	}

	static void SetHPShare(const AActor *Actor, float Share)
	{
		UCharacterDataComponent *Stats = StatsOf(Actor);
		Stats->CurrentHP = FMath::RoundToInt(Share * Stats->MaxHP);
	}
END_DEFINE_SPEC(FFightTriggersSpec)

void FFightTriggersSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Lord = FWoRTestWorld::NewCharacterData();
		Lord->CharacterClass = ECharacterClass::Generic;
		BossCharacter = FWoRTestWorld::NewCharacterData();
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Triggers = NewObject<UFightTriggers>(GetTransientPackage());
	});

	AfterEach([this]()
	{
		if (Triggers)
		{
			Triggers->Watch(false);
		}
		Triggers = nullptr;
		Lord = nullptr;
		BossCharacter = nullptr;
		Sword = nullptr;
		TestWorld.Reset();
	});

	It("fires a 'boss below 50% HP' reaction once when crossed, not at the start", [this]()
	{
		AActor *Wearer = SpawnWearer();
		AActor *Boss = TestWorld->SpawnCombatant(BossCharacter, true);
		if (!TestNotNull(TEXT("Combatants"), Wearer) || !TestNotNull(TEXT("Boss"), Boss))
		{
			return;
		}
		const TArray<FFiredReaction> Now =
			Triggers->StartReactions({SwordReaction(EFightMoment::HPBelow, EFightMomentSide::Theirs)}, {Wearer}, {Boss});
		TestEqual(TEXT("Nothing at fight start"), Now.Num(), 0);
		TestEqual(TEXT("It waits"), Triggers->GetWaitingCount(), 1);
		Triggers->Watch(true);

		Triggers->HandleHPChanged(0, 0);
		TestEqual(TEXT("At full HP: not yet"), Triggers->GetFiredMidFightCount(), 0);
		SetHPShare(Boss, FIGHT_TRIGGER_TEST_BELOW);
		Triggers->HandleHPChanged(0, 0);
		TestEqual(TEXT("Crossed: fires"), Triggers->GetFiredMidFightCount(), 1);
		SetHPShare(Boss, 1.0f);
		Triggers->HandleHPChanged(0, 0);
		SetHPShare(Boss, FIGHT_TRIGGER_TEST_BELOW);
		Triggers->HandleHPChanged(0, 0);
		TestEqual(TEXT("Crossed again: once per fight"), Triggers->GetFiredMidFightCount(), 1);
		TestEqual(TEXT("Nothing left waiting"), Triggers->GetWaitingCount(), 0);
	});

	It("doesn't fire an HP line the boss is already past at the start", [this]()
	{
		AActor *Wearer = SpawnWearer();
		AActor *Boss = TestWorld->SpawnCombatant(BossCharacter, true);
		if (!TestNotNull(TEXT("Combatants"), Wearer) || !TestNotNull(TEXT("Boss"), Boss))
		{
			return;
		}
		Triggers->StartReactions({SwordReaction(EFightMoment::HPBelow, EFightMomentSide::Theirs)}, {Wearer}, {Boss});
		SetHPShare(Boss, FIGHT_TRIGGER_TEST_BELOW);
		Triggers->Watch(true);
		Triggers->HandleHPChanged(0, 0);
		TestEqual(TEXT("Already below: not crossed"), Triggers->GetFiredMidFightCount(), 0);
		SetHPShare(Boss, 1.0f);
		Triggers->HandleHPChanged(0, 0);
		SetHPShare(Boss, FIGHT_TRIGGER_TEST_BELOW);
		Triggers->HandleHPChanged(0, 0);
		TestEqual(TEXT("Back above, then below: crossed"), Triggers->GetFiredMidFightCount(), 1);
	});

	It("fires a hit on its own side only, and fight-start reactions at once", [this]()
	{
		AActor *Wearer = SpawnWearer();
		AActor *Boss = TestWorld->SpawnCombatant(BossCharacter, true);
		if (!TestNotNull(TEXT("Combatants"), Wearer) || !TestNotNull(TEXT("Boss"), Boss))
		{
			return;
		}
		const TArray<FFiredReaction> Now = Triggers->StartReactions(
			{SwordReaction(EFightMoment::FightStart, EFightMomentSide::Ours), SwordReaction(EFightMoment::Hit, EFightMomentSide::Ours)},
			{Wearer}, {Boss});
		TestEqual(TEXT("The fight-start one is returned to apply now"), Now.Num(), 1);
		TestEqual(TEXT("The hit one waits"), Triggers->GetWaitingCount(), 1);
		Triggers->Watch(true);
		Triggers->HandleDamageDealt(Boss, Wearer, 1, false);
		TestEqual(TEXT("The boss hitting: not ours"), Triggers->GetFiredMidFightCount(), 0);
		Triggers->HandleDamageDealt(Wearer, Boss, 1, false);
		TestEqual(TEXT("The wearer hitting: fires"), Triggers->GetFiredMidFightCount(), 1);
	});

	It("listens to the real HP delegate while watching, and not after", [this]()
	{
		AActor *Wearer = SpawnWearer();
		AActor *Boss = TestWorld->SpawnCombatant(BossCharacter, true);
		if (!TestNotNull(TEXT("Combatants"), Wearer) || !TestNotNull(TEXT("Boss"), Boss))
		{
			return;
		}
		Triggers->StartReactions({SwordReaction(EFightMoment::HPBelow, EFightMomentSide::Theirs)}, {Wearer}, {Boss});
		UFightTriggers *Unwatched = NewObject<UFightTriggers>(GetTransientPackage());
		Unwatched->StartReactions({SwordReaction(EFightMoment::HPBelow, EFightMomentSide::Theirs)}, {Wearer}, {Boss});
		Triggers->Watch(true);
		Unwatched->Watch(true);
		Unwatched->Watch(false);
		UCharacterDataComponent *Stats = StatsOf(Boss);
		SetHPShare(Boss, FIGHT_TRIGGER_TEST_BELOW);
		Stats->OnHPChanged.Broadcast(Stats->CurrentHP, Stats->MaxHP);
		TestEqual(TEXT("Bound: fires"), Triggers->GetFiredMidFightCount(), 1);
		TestEqual(TEXT("Unbound: doesn't"), Unwatched->GetFiredMidFightCount(), 0);
	});

	It("does nothing on a killing blow, and a dead boss stays dead", [this]()
	{
		UBossData *Data = NewObject<UBossData>(GetTransientPackage());
		Data->Boss = BossCharacter;
		FBossTriggerCondition InParty;
		InParty.Kind = EBossTriggerConditionKind::PartyMember;
		InParty.Character = Lord;
		FBossTrigger Trigger;
		Trigger.TriggerId = FIGHT_TRIGGER_TEST_ID;
		Trigger.Conditions.Add(InParty);
		Trigger.Results.Add(EBossTriggerResult::Strengthen);
		Trigger.When.Moment = EFightMoment::HPBelow;
		Trigger.When.Side = EFightMomentSide::Theirs;
		Trigger.When.ThresholdPercent = FIGHT_TRIGGER_TEST_LINE;
		Data->Triggers.Add(Trigger);
		Trigger.When.Moment = EFightMoment::DamageTaken;
		Data->Triggers.Add(Trigger);

		AActor *Member = TestWorld->SpawnCombatant(Lord);
		AActor *Boss = TestWorld->SpawnCombatant(BossCharacter, true);
		Triggers->StartBoss(Boss, *Data, {Member});
		Triggers->Watch(true);
		StatsOf(Boss)->CurrentHP = 0;
		Triggers->HandleDamageDealt(Member, Boss, 1, false);
		TestEqual(TEXT("Killed outright: nothing fires"), Triggers->GetFiredMidFightCount(), 0);
		TestEqual(TEXT("Still dead"), StatsOf(Boss)->CurrentHP, 0);
	});

	It("takes a mid-fight weaken back off when a strengthen follows", [this]()
	{
		UBossData *Data = NewObject<UBossData>(GetTransientPackage());
		Data->Boss = BossCharacter;
		FBossTriggerCondition InParty;
		InParty.Kind = EBossTriggerConditionKind::PartyMember;
		InParty.Character = Lord;
		FBossTrigger Weaken;
		Weaken.TriggerId = FIGHT_TRIGGER_TEST_ID;
		Weaken.Conditions.Add(InParty);
		Weaken.Results.Add(EBossTriggerResult::Weaken);
		Weaken.When.Moment = EFightMoment::TurnStart;
		Weaken.When.Side = EFightMomentSide::Theirs;
		FBossTrigger Strengthen = Weaken;
		Strengthen.Results = {EBossTriggerResult::Strengthen};
		Strengthen.When.Moment = EFightMoment::Hit;
		Strengthen.When.Side = EFightMomentSide::Ours;
		Data->Triggers.Add(Weaken);
		Data->Triggers.Add(Strengthen);

		AActor *Member = TestWorld->SpawnCombatant(Lord);
		AActor *Boss = TestWorld->SpawnCombatant(BossCharacter, true);
		const int32 BaseMaxHP = StatsOf(Boss)->MaxHP;
		Triggers->StartBoss(Boss, *Data, {Member});
		Triggers->Watch(true);
		Triggers->HandleTurnStarted(Boss, 1);
		TestTrue(TEXT("Weakened"), StatsOf(Boss)->MaxHP < BaseMaxHP);
		Triggers->HandleDamageDealt(Member, Boss, 1, false);
		TestEqual(TEXT("Net 0: back to its own max HP"), StatsOf(Boss)->MaxHP, BaseMaxHP);
		TestEqual(TEXT("Both fired"), Triggers->GetFiredMidFightCount(), 2);
	});

	It("weakens a boss at its turn start and keeps its share of HP", [this]()
	{
		UBossData *Data = NewObject<UBossData>(GetTransientPackage());
		Data->Boss = BossCharacter;
		FBossTriggerCondition InParty;
		InParty.Kind = EBossTriggerConditionKind::PartyMember;
		InParty.Character = Lord;
		FBossTrigger Trigger;
		Trigger.TriggerId = FIGHT_TRIGGER_TEST_ID;
		Trigger.Conditions.Add(InParty);
		Trigger.Results.Add(EBossTriggerResult::Weaken);
		Trigger.When.Moment = EFightMoment::TurnStart;
		Trigger.When.Side = EFightMomentSide::Theirs;
		Data->Triggers.Add(Trigger);

		AActor *Member = TestWorld->SpawnCombatant(Lord);
		AActor *Boss = TestWorld->SpawnCombatant(BossCharacter, true);
		const int32 BaseMaxHP = StatsOf(Boss)->MaxHP;
		TestEqual(TEXT("Nothing at fight start"), Triggers->StartBoss(Boss, *Data, {Member}), 0);
		TestEqual(TEXT("Unchanged before its moment"), StatsOf(Boss)->MaxHP, BaseMaxHP);
		SetHPShare(Boss, FIGHT_TRIGGER_TEST_HALF);
		Triggers->Watch(true);

		Triggers->HandleTurnStarted(Member, 1);
		TestEqual(TEXT("A party member's turn: not the boss's"), Triggers->GetFiredMidFightCount(), 0);
		Triggers->HandleTurnStarted(Boss, 1);
		TestEqual(TEXT("The boss's turn: fires"), Triggers->GetFiredMidFightCount(), 1);
		const UCharacterDataComponent *Stats = StatsOf(Boss);
		TestTrue(TEXT("Weaker"), Stats->MaxHP < BaseMaxHP);
		TestTrue(TEXT("Still about half its HP"), FMath::Abs(Stats->CurrentHP - FMath::RoundToInt(FIGHT_TRIGGER_TEST_HALF * Stats->MaxHP)) <= 1);
		Triggers->HandleTurnStarted(Boss, 2);
		TestEqual(TEXT("Once per fight"), Triggers->GetFiredMidFightCount(), 1);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
