// BoundConditionsSpec.cpp
// B-337c (D-130): a bound item's memory conditions check through the one evaluator. Old items read across
// (MemoryUnlock::GetBoundConditions) and give the same answer as the old check (AreConditionsMet) over a grid of
// owners, owner-wearing, facing, upgrade levels, level-ups and fight starts; authored Conditions replace the old
// fields. The skill trigger On Flawless Parry (B-365's first reader) fires an armed effect on the Flawless Parry
// event only, never on a lone perfect defense.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "GAS/WoRTurnEffectTracker.h"
#include "Character/CharacterData.h"
#include "Combat/Defense/EDefenseType.h"
#include "Conditions/WoRConditions.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryUnlock.h"
#include "Skills/Effects/FGatheredEffect.h"
#include "Skills/Effects/FSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 BOUND_UPGRADE_LEVEL = 2;
	constexpr int32 BOUND_MAX_STEPS = 3;
	constexpr int32 BOUND_PARRY_DEF_ID = 96;
	constexpr int32 BOUND_PARRY_ITEM = 7202;
	constexpr float BOUND_PARRY_HEAL = 10.0f;
	constexpr int32 BOUND_PARRY_TURNS = 2;
	constexpr float BOUND_ATTACK_COST = 20.0f;

	EItemTier BoundTierAt(int32 Steps) { return static_cast<EItemTier>(static_cast<int32>(EItemTier::F_Tier) + Steps); }

	/** Gear that heals its wearer on Trigger. */
	FGatheredEffect BoundHealOn(ESkillTrigger Trigger, int32 BundleIndex)
	{
		FSkillEffect Effect;
		Effect.EffectName = TEXT("Bound conditions heal");
		FSkillCondition Condition;
		Condition.Trigger = Trigger;
		Condition.Subject = ECondSubject::Self;
		Effect.Conditions.Add(Condition);
		FSkillEffectPayload Heal;
		Heal.EffectType = ESkillEffectType::HealthRestore;
		Heal.Magnitude = BOUND_PARRY_HEAL;
		Heal.Duration = BOUND_PARRY_TURNS; // turn-counted, so it is stored and can be counted
		Heal.Target = ETargetType::Self;
		Heal.TargetCount = ETargetCount::Single;
		Effect.Payloads.Add(Heal);
		return FGatheredEffect(BOUND_PARRY_DEF_ID, BundleIndex, Effect, BOUND_PARRY_ITEM);
	}
}

BEGIN_DEFINE_SPEC(FBoundConditionsSpec, "WoR.Conditions.Bound", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UCharacterData *Owner = nullptr;
	UCharacterData *Other = nullptr;
	UCharacterData *FacedEnemy = nullptr;
	UCharacterData *OtherEnemy = nullptr;
	AActor *FacedActor = nullptr;
	AActor *OtherEnemyActor = nullptr;
	UWeaponData *Item = nullptr;

	/** The new answer at a level-up of Item to Steps. */
	bool NewAtLevelUp(const FBoundMemory &Bound, int32 Steps) const
	{
		FMemoryEvent LevelUp;
		LevelUp.Source = EMemorySource::BoundGear;
		LevelUp.UpgradedItem = Item;
		LevelUp.UpgradedTier = BoundTierAt(Steps);
		return MemoryUnlock::IsBoundMet(Bound, *Item, WoRConditions::MakeContext(LevelUp, nullptr));
	}

	/** The new answer at a fight start: Wearer wears Item at Steps, facing Foe. */
	bool NewAtFightStart(const FBoundMemory &Bound, const UCharacterData *Wearer, int32 Steps, const UCharacterData *Foe) const
	{
		FMemoryEvent FightStart;
		FightStart.Source = EMemorySource::BoundGear;
		FMemoryEventMember &Member = FightStart.Party.AddDefaulted_GetRef();
		Member.Character = Wearer;
		Member.Worn.Emplace(Item, BoundTierAt(Steps));
		FWoRConditionContext Context = WoRConditions::MakeContext(FightStart, nullptr);
		Context.Trigger = EWoRConditionTrigger::FightStart;
		Context.Opposing.Add(Foe);
		return MemoryUnlock::IsBoundMet(Bound, *Item, Context);
	}
END_DEFINE_SPEC(FBoundConditionsSpec)

void FBoundConditionsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Owner = FWoRTestWorld::NewCharacterData();
		Other = FWoRTestWorld::NewCharacterData();
		FacedEnemy = FWoRTestWorld::NewCharacterData();
		OtherEnemy = FWoRTestWorld::NewCharacterData();
		FacedActor = TestWorld->SpawnCombatant(FacedEnemy, true);
		OtherEnemyActor = TestWorld->SpawnCombatant(OtherEnemy, true);
		Item = NewObject<UWeaponData>(GetTransientPackage());
		Item->Tier = EItemTier::F_Tier;
	});

	AfterEach([this]()
	{
		Owner = Other = FacedEnemy = OtherEnemy = nullptr;
		FacedActor = OtherEnemyActor = nullptr;
		Item = nullptr;
		TestWorld.Reset();
	});

	It("gives the old answer for every old bound memory, at a level-up and at a fight start", [this]()
	{
		if (!TestNotNull(TEXT("faced enemy spawned"), FacedActor) || !TestNotNull(TEXT("other enemy spawned"), OtherEnemyActor)) return;
		int32 Cases = 0;
		for (UCharacterData *BoundOwner : {static_cast<UCharacterData *>(nullptr), Owner})
		{
			Item->BoundOwner = BoundOwner;
			for (const bool bOwnerWearing : {false, true})
			{
				for (UCharacterData *Facing : {static_cast<UCharacterData *>(nullptr), FacedEnemy})
				{
					for (const int32 Level : {0, BOUND_UPGRADE_LEVEL})
					{
						FBoundMemory Bound;
						Bound.Memory = NewObject<UMemoryData>(GetTransientPackage());
						Bound.UpgradeLevel = Level;
						Bound.bNeedsOwnerWearing = bOwnerWearing;
						Bound.FacingCharacter = Facing;
						const FString Shape = FString::Printf(TEXT("owner %s, owner wearing %d, facing %d, level %d"),
															  BoundOwner ? TEXT("set") : TEXT("none"), bOwnerWearing, Facing != nullptr, Level);
						for (int32 Steps = 0; Steps <= BOUND_MAX_STEPS; ++Steps)
						{
							TestEqual(FString::Printf(TEXT("%s: level-up to %d"), *Shape, Steps), NewAtLevelUp(Bound, Steps),
									  MemoryUnlock::AreConditionsMet(Bound, *Item, Steps, nullptr, {}));
							++Cases;
							for (UCharacterData *Wearer : {Owner, Other})
							{
								for (AActor *Foe : {FacedActor, OtherEnemyActor})
								{
									const UCharacterData *FoeData = Foe == FacedActor ? FacedEnemy : OtherEnemy;
									TestEqual(FString::Printf(TEXT("%s: fight, wearer %s at %d, facing %s"), *Shape,
															  Wearer == Owner ? TEXT("owner") : TEXT("other"), Steps,
															  Foe == FacedActor ? TEXT("the named enemy") : TEXT("another")),
											  NewAtFightStart(Bound, Wearer, Steps, FoeData),
											  MemoryUnlock::AreConditionsMet(Bound, *Item, Steps, Wearer, {Foe}));
									++Cases;
								}
							}
						}
					}
				}
			}
		}
		AddInfo(FString::Printf(TEXT("%d parity cases"), Cases));
	});

	It("checks authored Conditions instead of the old fields", [this]()
	{
		FBoundMemory Bound;
		Bound.Memory = NewObject<UMemoryData>(GetTransientPackage());
		Bound.UpgradeLevel = BOUND_MAX_STEPS + 1; // the old field alone would never hold
		FWoRCondition &Facing = Bound.Conditions.Conditions.AddDefaulted_GetRef();
		Facing.Trigger = EWoRConditionTrigger::FightStart;
		Facing.Subject = EWoRConditionSubject::FacingEnemy;
		Facing.Enemy = FacedEnemy;
		TestTrue(TEXT("facing the named enemy holds"), NewAtFightStart(Bound, Other, 0, FacedEnemy));
		TestFalse(TEXT("facing another does not"), NewAtFightStart(Bound, Other, 0, OtherEnemy));
		TestFalse(TEXT("a fight-start condition never holds at a level-up"), NewAtLevelUp(Bound, BOUND_MAX_STEPS));
		Bound.Memory = nullptr;
		TestFalse(TEXT("no memory, nothing to unlock"), NewAtFightStart(Bound, Other, 0, FacedEnemy));
	});

	It("fires an On Flawless Parry effect on the Flawless Parry event only (B-365's first reader)", [this]()
	{
		USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		AActor *Holder = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		FWoRTestWorld::AddComponent<UWoRTurnEffectTracker>(Holder); // B-042g2: fires-once records live on the tracker
		AActor *Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		if (!TestNotNull(TEXT("Effects"), Effects) || !TestNotNull(TEXT("Holder"), Holder) || !TestNotNull(TEXT("Attacker"), Attacker)) return;
		Effects->ResetForNewCombat();
		Effects->ArmConditionalEffects(Holder, {BoundHealOn(ESkillTrigger::OnFlawlessParry, 0)});
		const auto HealsHeld = [&]()
		{
			return Effects->GetActiveEffects(Holder).FilterByPredicate([](const FActiveSkillEffect &E) { return E.EffectType == ESkillEffectType::HealthRestore; }).Num();
		};
		Effects->OnDefenseResolvedHandler(Holder, Attacker, EDefenseType::Block, /*bPerfect=*/true, 0, BOUND_ATTACK_COST);
		TestEqual(TEXT("a lone Perfect Guard does not fire it"), HealsHeld(), 0);
		Effects->OnFlawlessParryHandler(Attacker, Holder);
		TestEqual(TEXT("the attacker's side of someone else's Flawless Parry: the holder did not defend"), HealsHeld(), 0);
		Effects->OnFlawlessParryHandler(Holder, Attacker);
		TestEqual(TEXT("the holder's Flawless Parry fires it"), HealsHeld(), 1);
	});

	It("fires no On Perfect Guard effect again on the Flawless Parry event", [this]()
	{
		USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		AActor *Holder = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		FWoRTestWorld::AddComponent<UWoRTurnEffectTracker>(Holder); // B-042g2: fires-once records live on the tracker
		AActor *Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		if (!TestNotNull(TEXT("Effects"), Effects) || !TestNotNull(TEXT("Holder"), Holder) || !TestNotNull(TEXT("Attacker"), Attacker)) return;
		Effects->ResetForNewCombat();
		Effects->ArmConditionalEffects(Holder, {BoundHealOn(ESkillTrigger::OnPerfectGuard, 1)});
		Effects->OnFlawlessParryHandler(Holder, Attacker);
		TestEqual(TEXT("nothing fired"), Effects->GetActiveEffects(Holder).Num(), 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
