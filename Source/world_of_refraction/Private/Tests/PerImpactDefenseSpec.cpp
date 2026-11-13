// PerImpactDefenseSpec.cpp
// B-032d (Q-032d): per-impact defense for every spell shape (the only path since B-032b). A spell's impacts are its
// arrivals on each target, summed over its cast entries (SpellImpactPlan); each one is defended on its own, so a
// guard on the first impact leaves the second untouched. A window that times out lands its impacts one by one, an
// ability whose animation ends before its Impact notifies lands the rest as fallback impacts, and when nothing is
// defended a support spell of several hits (timer close) lands the same hits at its close. A spell's window stays open past the cast
// animation for projectiles still flying. Cast headless as in WoR.GAS.Abilities
// (montage-less skills, wor.Combat.InstantImpacts 1); the foe is player-driven, so no defense is rolled for it.
// Through the running ability (wor.Abilities.UseGAS 1) every arrival is still taken exactly once (B-043d/e).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "UObject/StrongObjectPtr.h"
#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "WoRTestWorld.h"
#include "DefenseOutcomeRecorder.h"
#include "ElementDamageRecorder.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Actions/SpellImpactPlan.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Combat/Defense/GuardRules.h"
#include "Combat/Projectile/SkillProjectile.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GAS/Abilities/WoRGameplayAbility.h"
#include "GAS/Abilities/WoRSkillAbilities.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	/** Even, so a two-impact split is exact either way. */
	constexpr int32 PER_IMPACT_SPEC_SPELL_DAMAGE = 40;
	constexpr int32 PER_IMPACT_SPEC_SPELL_EP = 5;
	constexpr int32 PER_IMPACT_SPEC_STRIKE_DAMAGE = 30;
	constexpr int32 PER_IMPACT_SPEC_STRIKE_EP = 6;
	constexpr int32 PER_IMPACT_SPEC_START_EP = 40;
	constexpr int32 PER_IMPACT_SPEC_TWO_IMPACTS = 2;
	constexpr int32 PER_IMPACT_SPEC_BURST_OF_THREE = 3;
	/** One AOE entry and one Instant entry: one arrival each. */
	constexpr int32 PER_IMPACT_SPEC_AOE_AND_INSTANT = 2;
	constexpr int32 PER_IMPACT_SPEC_THREE_HITS = 3;
	constexpr float PER_IMPACT_SPEC_BURST_INTERVAL = 0.05f;
	constexpr int32 PER_IMPACT_SPEC_TOUGH_HP = 10000;
	/** A 100% Crit Chance Debuff and Luck Debuff: no crit and no luck dodge, so both paths roll nothing that matters. */
	constexpr float PER_IMPACT_SPEC_FULL_DEBUFF_PERCENT = 100.0f;
	constexpr int32 PER_IMPACT_SPEC_EFFECT_TURNS = 3;
	constexpr int32 PER_IMPACT_SPEC_NO_CRIT_ID = 0;
	constexpr int32 PER_IMPACT_SPEC_NO_LUCK_ID = 1;
	constexpr int32 PER_IMPACT_SPEC_BATTLE_SEED = 3204;
	/** About 0.33 s of flight at the default projectile speed (as WoR.GAS.Abilities). */
	constexpr float PER_IMPACT_SPEC_DUEL_RANGE = 500.0f;
	/** About 0.1 s of flight: a press at the cast is inside the 0.5 s base window of the first impact. */
	constexpr float PER_IMPACT_SPEC_CLOSE_RANGE = 150.0f;
	/** A melee strike's foe stands where the hero does (as WoR.GAS.Abilities' Strike). */
	constexpr float PER_IMPACT_SPEC_SAME_SPOT = 0.0f;
	/** Spans the executor's 10 s action timeout at 0.01 s a frame; a run stops once the turn moves on. */
	constexpr int32 PER_IMPACT_SPEC_SETTLE_FRAMES = 1100;
	/** Frames a press may wait for the foe's window to open after the submit. */
	constexpr int32 PER_IMPACT_SPEC_WINDOW_WAIT_FRAMES = 3;
	/** Every projectile of the burst is out by frame 5 and none lands before frame 30 at the duel range. */
	constexpr int32 PER_IMPACT_SPEC_SHOOT_DOWN_FRAMES = 20;
	/** Keeps the 8 s window failsafe from closing first, so the action's 10 s timeout lands the impacts. */
	constexpr float PER_IMPACT_SPEC_NO_FAILSAFE_SECONDS = 100.0f;
	/** A few frames for a fallback impact's close to finish the action. */
	constexpr int32 PER_IMPACT_SPEC_ACTION_FRAMES = 10;
	/** DamageSplit table entries for the slice checks (percent). */
	constexpr float PER_IMPACT_SPEC_HALF_PERCENT = 50.0f;
	constexpr int32 PER_IMPACT_SPEC_ODD_BASE = 41;
	constexpr int32 PER_IMPACT_SPEC_THIRDS_BASE = 50;

	void SetPerImpactSpecCVar(const TCHAR *Name, int32 Value)
	{
		if (IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CVar->Set(Value, ECVF_SetByCode);
		}
	}

	FSkillCastEntry MakeCastEntry(ESpellDeliveryType Delivery, int32 Count = 1)
	{
		FSkillCastEntry Entry;
		Entry.DeliveryType = Delivery;
		Entry.Count = Count;
		Entry.BurstInterval = PER_IMPACT_SPEC_BURST_INTERVAL;
		return Entry;
	}
}

BEGIN_DEFINE_SPEC(FPerImpactDefenseSpec, "WoR.Combat.PerImpactDefense",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	UAbilityData *Strike = nullptr;
	USpellData *Spell = nullptr;

	/** What one cast did to the foe. */
	struct FCastOutcome
	{
		bool bRan = false;
		FString Refusal;
		int32 FoeDamage = 0;
		/** Each hit's damage, in order (OnElementDamageDealt). */
		TArray<int32> Hits;
		/** The impacts resolved per impact on the foe, in order (OnDefenseResolved). */
		TArray<int32> DefendedImpacts;
		TArray<EDefenseType> Defenses;
		bool bTurnMovedOn = false;
		/** Arrivals the hero's running ability took (AOE resolves + projectile arrivals). */
		int32 AbilityArrivals = 0;
	};

	enum class ECastTwist : uint8
	{
		None,
		/** The foe presses Guard at the cast: in range of the first impact only. */
		GuardAtCast,
		/** Every projectile is destroyed in flight, so the action times out. */
		ShootDown,
		/** The cast animation ends (the Skill-end reconcile) while the projectiles still fly. */
		AnimationEndsInFlight,
	};

	/** A rooted, montage-less Generic spell at an enemy (Generic: castable by the test character, a Caster). */
	USpellData *MakeSpell(const TArray<FSkillCastEntry> &Entries, int32 HitCount)
	{
		USpellData *NewSpell = NewObject<USpellData>(GetTransientPackage());
		NewSpell->Name = TEXT("Montage-less Per-Impact Spell");
		NewSpell->BaseDamage = PER_IMPACT_SPEC_SPELL_DAMAGE;
		NewSpell->BaseEnergyCost = PER_IMPACT_SPEC_SPELL_EP;
		NewSpell->ExecutionType = EAbilityExecutionType::Ranged;
		NewSpell->TargetType = ETargetType::Enemy;
		NewSpell->Element = ESpellElement::Generic;
		NewSpell->HitCount = HitCount;
		NewSpell->CastArray = Entries;
		if (Entries.Num() > 0)
		{
			NewSpell->DeliveryType = Entries[0].DeliveryType;
		}
		NewSpell->AddToRoot();
		return NewSpell;
	}

	/** HitCount 2, delivered as a burst of two projectiles: the multi-hit spell B-032 left lumped. */
	USpellData *MakeTwoHitSpell()
	{
		return MakeSpell({MakeCastEntry(ESpellDeliveryType::Projectile, PER_IMPACT_SPEC_TWO_IMPACTS)}, PER_IMPACT_SPEC_TWO_IMPACTS);
	}

	/** A combatant whose owned weapon's attack is Strike (as WoR.GAS.Abilities arms its fighters). */
	AActor *SpawnArmed()
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->WeaponAttack = Strike;
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(FWoRTestWorld::NewCharacterData(), Weapon, /*bAIControlled*/ false);
		AActor *Actor = Loadout ? Loadout->GetOwner() : nullptr;
		if (UInventoryComponent *Inventory = Actor ? Actor->FindComponentByClass<UInventoryComponent>() : nullptr)
		{
			FWeaponInventoryEntry Owned;
			Owned.Weapon = Weapon;
			Inventory->Weapons.Add(Owned);
		}
		return Actor;
	}

	static UCharacterDataComponent *StatsOf(const AActor *Actor)
	{
		return Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	}

	static int32 CurrentHPOf(const AActor *Actor)
	{
		const UCharacterDataComponent *Stats = StatsOf(Actor);
		return Stats ? Stats->CurrentHP : 0;
	}

	static void PlaceAt(AActor *Actor, const FVector &Location)
	{
		if (!Actor->GetRootComponent())
		{
			USceneComponent *Root = NewObject<USceneComponent>(Actor);
			Actor->SetRootComponent(Root);
			Root->RegisterComponent();
		}
		Actor->SetActorLocation(Location);
	}

	/** Sum of Count over the instances of Actor's ability for Skill (as WoR.GAS.Abilities). */
	template <typename CountType>
	static int32 SumOverInstances(AActor *Actor, const USkillDataBase *Skill, CountType Count)
	{
		int32 Total = 0;
		if (FGameplayAbilitySpec *Spec = WoRSkillAbilities::FindSkillSpec(Actor, Skill))
		{
			for (UGameplayAbility *Instance : Spec->GetAbilityInstances())
			{
				if (const UWoRGameplayAbility *Ability = Cast<UWoRGameplayAbility>(Instance))
				{
					Total += Count(*Ability);
				}
			}
		}
		return Total;
	}

	/** A fresh world with the hero to act against a tough, player-driven foe at Range; false when it isn't the hero's
	 *  turn. bThroughAbility: the hero has an ASC and actions start through its skill abilities (wor.Abilities.UseGAS 1). */
	bool StartDuel(float Range, AActor *&OutHero, AActor *&OutFoe, bool bThroughAbility = false)
	{
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		Orchestrator = nullptr;
		TestWorld = MakeUnique<FWoRTestWorld>();
		if (!TestWorld->Create() || !TestWorld->GetWorld())
		{
			return false;
		}
		Orchestrator = TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>();
		WoRSkillAbilities::SetUsesAbilities(bThroughAbility);
		OutHero = SpawnArmed();
		OutFoe = SpawnArmed();
		if (bThroughAbility && OutHero)
		{
			FWoRTestWorld::AddComponent<UAbilitySystemComponent>(OutHero)->InitAbilityActorInfo(OutHero, OutHero);
		}
		UCharacterDataComponent *HeroStats = StatsOf(OutHero);
		UCharacterDataComponent *FoeStats = StatsOf(OutFoe);
		USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		if (!Orchestrator || !HeroStats || !FoeStats || !Effects)
		{
			return false;
		}
		Orchestrator->StartCombat({OutHero}, {OutFoe});
		if (Orchestrator->GetCurrentActor() != OutHero)
		{
			return false;
		}
		HeroStats->MaxEP = PER_IMPACT_SPEC_START_EP;
		HeroStats->CurrentEP = PER_IMPACT_SPEC_START_EP;
		FoeStats->MaxHP = PER_IMPACT_SPEC_TOUGH_HP;
		FoeStats->CurrentHP = PER_IMPACT_SPEC_TOUGH_HP;
		Effects->ApplyEffect(OutHero, FActiveSkillEffect::CreateBuff(TEXT("No crit"), PER_IMPACT_SPEC_NO_CRIT_ID, ESkillEffectType::CritChanceDebuff,
																	  PER_IMPACT_SPEC_FULL_DEBUFF_PERCENT, PER_IMPACT_SPEC_EFFECT_TURNS), OutHero);
		Effects->ApplyEffect(OutFoe, FActiveSkillEffect::CreateBuff(TEXT("No luck"), PER_IMPACT_SPEC_NO_LUCK_ID, ESkillEffectType::LuckDebuff,
																	 PER_IMPACT_SPEC_FULL_DEBUFF_PERCENT, PER_IMPACT_SPEC_EFFECT_TURNS), OutFoe);
		PlaceAt(OutHero, FVector::ZeroVector);
		PlaceAt(OutFoe, FVector(Range, 0.0f, 0.0f));
		return true;
	}

	/** Hero uses Skill on the foe, until the turn moves on. */
	FCastOutcome RunCast(USkillDataBase *Skill, ECastTwist Twist = ECastTwist::None, float Range = PER_IMPACT_SPEC_DUEL_RANGE,
						 bool bThroughAbility = false)
	{
		FCastOutcome Outcome;
		AActor *Hero = nullptr;
		AActor *Foe = nullptr;
		if (!Skill || !StartDuel(Range, Hero, Foe, bThroughAbility))
		{
			return Outcome;
		}
		UDefenseSystem *Defense = TestWorld->GetSubsystem<UDefenseSystem>();
		UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
		if (!Defense || !Executor)
		{
			return Outcome;
		}
		if (Twist == ECastTwist::ShootDown)
		{
			Defense->MaxWindowDuration = PER_IMPACT_SPEC_NO_FAILSAFE_SECONDS;
		}
		TStrongObjectPtr<UDefenseOutcomeRecorder> Defenses(NewObject<UDefenseOutcomeRecorder>());
		Defenses->Listen(Defense, Foe);
		TStrongObjectPtr<UElementDamageRecorder> Hits(NewObject<UElementDamageRecorder>());
		Hits->Listen(Executor);

		FAction Action;
		if (USpellData *AsSpell = Cast<USpellData>(Skill))
		{
			Action.ActionType = EActionType::Spell;
			Action.SpellData = AsSpell;
		}
		else
		{
			Action.ActionType = EActionType::Ability;
			Action.SkillData = Skill;
		}
		Action.Targets.Add(Foe);
		Outcome.Refusal = Executor->ValidateAction(Hero, Action).ErrorMessage;
		const int32 FoeHPBefore = CurrentHPOf(Foe);
		Orchestrator->SubmitActionAsync(Action);

		if (Twist == ECastTwist::GuardAtCast)
		{
			for (int32 Frame = 0; Frame < PER_IMPACT_SPEC_WINDOW_WAIT_FRAMES && !Defense->IsDefenseWindowOpen(Foe); ++Frame)
			{
				TestWorld->Tick();
			}
			Defense->SubmitDefenseInput(Foe, EDefenseType::Block); // stamped now
		}
		if (Twist == ECastTwist::AnimationEndsInFlight)
		{
			Executor->ReconcileDefenseWindowsAtAnimationEnd(); // what Skill-end runs
		}
		for (int32 Frame = 0; Frame < PER_IMPACT_SPEC_SETTLE_FRAMES && Orchestrator->GetCurrentActor() == Hero; ++Frame)
		{
			if (Twist == ECastTwist::ShootDown && Frame < PER_IMPACT_SPEC_SHOOT_DOWN_FRAMES)
			{
				for (TActorIterator<ASkillProjectile> It(TestWorld->GetWorld()); It; ++It)
				{
					It->Destroy();
				}
			}
			TestWorld->Tick();
		}

		Outcome.bRan = true;
		Outcome.FoeDamage = FoeHPBefore - CurrentHPOf(Foe);
		for (const UElementDamageRecorder::FEntry &Entry : Hits->Entries)
		{
			Outcome.Hits.Add(Entry.Damage);
		}
		Outcome.DefendedImpacts = Defenses->ImpactIndices;
		Outcome.Defenses = Defenses->DefenseTypes;
		Outcome.bTurnMovedOn = Orchestrator->GetCurrentActor() != Hero;
		Outcome.AbilityArrivals = SumOverInstances(Hero, Skill, [](const UWoRGameplayAbility &Ability)
		{
			return Ability.GetAOEResolves() + Ability.GetProjectileEventsResolved();
		});
		return Outcome;
	}

	/** The cast happened and dealt damage (the refusal and the hit count say why not). */
	bool TestCastLanded(const TCHAR *What, const FCastOutcome &Outcome)
	{
		return TestTrue(FString::Printf(TEXT("%s: cast and dealt damage (hits %d; refusal: %s)"), What, Outcome.Hits.Num(), *Outcome.Refusal),
						Outcome.bRan && Outcome.FoeDamage > 0);
	}
END_DEFINE_SPEC(FPerImpactDefenseSpec)

void FPerImpactDefenseSpec::Define()
{
	BeforeEach([this]()
	{
		Strike = NewObject<UAbilityData>(GetTransientPackage());
		Strike->Name = TEXT("Montage-less Strike");
		Strike->BaseDamage = PER_IMPACT_SPEC_STRIKE_DAMAGE;
		Strike->BaseEnergyCost = PER_IMPACT_SPEC_STRIKE_EP;
		Strike->ExecutionType = EAbilityExecutionType::Melee;
		Strike->bIsAttack = true;
		Strike->AddToRoot();
		SetPerImpactSpecCVar(TEXT("wor.Combat.InstantImpacts"), 1);
		// One battle seed for every run, so no roll left by an earlier spec decides a damage check.
		SetPerImpactSpecCVar(TEXT("wor.Battle.Seed"), PER_IMPACT_SPEC_BATTLE_SEED);
	});

	AfterEach([this]()
	{
		WoRSkillAbilities::SetUsesAbilities(false);
		SetPerImpactSpecCVar(TEXT("wor.Combat.InstantImpacts"), 0);
		SetPerImpactSpecCVar(TEXT("wor.Battle.Seed"), 0);
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		Orchestrator = nullptr;
		TestWorld.Reset();
		if (Strike)
		{
			Strike->RemoveFromRoot();
			Strike = nullptr;
		}
		if (Spell)
		{
			Spell->RemoveFromRoot();
			Spell = nullptr;
		}
	});

	Describe("the impact plan", [this]()
	{
		It("counts one impact per arrival, summed over the cast entries", [this]()
		{
			Spell = MakeSpell({MakeCastEntry(ESpellDeliveryType::Projectile, PER_IMPACT_SPEC_BURST_OF_THREE),
							   MakeCastEntry(ESpellDeliveryType::AOE), MakeCastEntry(ESpellDeliveryType::Instant)},
							  /*HitCount*/ 1);
			TestEqual(TEXT("A burst of 3, an AOE and an Instant"), SpellImpactPlan::Make(*Spell).ImpactsPerTarget,
					  PER_IMPACT_SPEC_BURST_OF_THREE + PER_IMPACT_SPEC_AOE_AND_INSTANT);
			TestEqual(TEXT("A projectile entry with Count 0 still lands once"),
					  SpellImpactPlan::ArrivalsOf(MakeCastEntry(ESpellDeliveryType::Projectile, 0)), 1);
			Spell->CastArray.Reset();
			TestEqual(TEXT("No cast entries: one arrival (the loose path)"), SpellImpactPlan::Make(*Spell).ImpactsPerTarget, 1);
		});

		It("finds the cast entry each impact belongs to, in arrival order", [this]()
		{
			Spell = MakeSpell({MakeCastEntry(ESpellDeliveryType::Projectile, PER_IMPACT_SPEC_TWO_IMPACTS), MakeCastEntry(ESpellDeliveryType::AOE)},
							  /*HitCount*/ 1);
			TestEqual(TEXT("Burst arrival 1: entry 0"), SpellImpactPlan::CastEntryOfImpact(*Spell, 0), 0);
			TestEqual(TEXT("Burst arrival 2: entry 0"), SpellImpactPlan::CastEntryOfImpact(*Spell, 1), 0);
			TestEqual(TEXT("The AOE: entry 1"), SpellImpactPlan::CastEntryOfImpact(*Spell, PER_IMPACT_SPEC_TWO_IMPACTS), 1);
			TestEqual(TEXT("Past the plan: none"), SpellImpactPlan::CastEntryOfImpact(*Spell, PER_IMPACT_SPEC_BURST_OF_THREE), INDEX_NONE);
		});

		It("uses the DamageSplit only when HitCount equals the impacts", [this]()
		{
			Spell = MakeTwoHitSpell();
			TestTrue(TEXT("HitCount 2, two arrivals"), SpellImpactPlan::Make(*Spell).bUsesDamageSplit);
			Spell->HitCount = PER_IMPACT_SPEC_THREE_HITS;
			TestFalse(TEXT("HitCount 3, two arrivals: even split"), SpellImpactPlan::Make(*Spell).bUsesDamageSplit);
		});

		It("widens offensive shapes only, and leaves B-032's shape where it was", [this]()
		{
			Spell = MakeSpell({MakeCastEntry(ESpellDeliveryType::AOE)}, /*HitCount*/ 1);
			TestTrue(TEXT("One entry, HitCount 1: B-032's shape"), SpellImpactPlan::IsSingleEntrySingleHit(*Spell));
			TestFalse(TEXT("... not widened"), SpellImpactPlan::IsWidened(*Spell));
			Spell->CastArray.Add(MakeCastEntry(ESpellDeliveryType::AOE));
			TestTrue(TEXT("Two entries at an enemy: widened"), SpellImpactPlan::IsWidened(*Spell));
			Spell->TargetType = ETargetType::Ally;
			TestFalse(TEXT("A support spell: not widened"), SpellImpactPlan::IsWidened(*Spell));
		});

		It("cuts the lumped path's slices when the table fits, and an exact even split otherwise", [this]()
		{
			const TArray<float> Halves = {PER_IMPACT_SPEC_HALF_PERCENT, PER_IMPACT_SPEC_HALF_PERCENT};
			// The lumped tail floored FinalDamage x percent + 0.01 per hit: 41 -> 20 and 20.
			const int32 LumpedHalf = PER_IMPACT_SPEC_ODD_BASE / PER_IMPACT_SPEC_TWO_IMPACTS;
			TestEqual(TEXT("Table, impact 0"), SpellImpactPlan::ImpactBaseSlice(PER_IMPACT_SPEC_ODD_BASE, PER_IMPACT_SPEC_TWO_IMPACTS, Halves, 0), LumpedHalf);
			TestEqual(TEXT("Table, impact 1"), SpellImpactPlan::ImpactBaseSlice(PER_IMPACT_SPEC_ODD_BASE, PER_IMPACT_SPEC_TWO_IMPACTS, Halves, 1), LumpedHalf);
			// No table of the right size: 50 over 3 is 17, 17, 16.
			int32 Sum = 0;
			for (int32 Index = 0; Index < PER_IMPACT_SPEC_THREE_HITS; ++Index)
			{
				Sum += SpellImpactPlan::ImpactBaseSlice(PER_IMPACT_SPEC_THIRDS_BASE, PER_IMPACT_SPEC_THREE_HITS, Halves, Index);
			}
			TestEqual(TEXT("An even split sums to the base"), Sum, PER_IMPACT_SPEC_THIRDS_BASE);
			TestEqual(TEXT("The remainder goes to the first impact"),
					  SpellImpactPlan::ImpactBaseSlice(PER_IMPACT_SPEC_THIRDS_BASE, PER_IMPACT_SPEC_THREE_HITS, Halves, 0),
					  PER_IMPACT_SPEC_THIRDS_BASE / PER_IMPACT_SPEC_THREE_HITS + 1);
		});
	});

	Describe("casts", [this]()
	{
		It("opens one defense per impact for a 2-hit spell (none on the lumped path)", [this]()
		{
			Spell = MakeTwoHitSpell();
			const FCastOutcome PerImpact = RunCast(Spell);
			if (!TestCastLanded(TEXT("Per impact"), PerImpact))
			{
				return;
			}
			TestEqual(TEXT("Two impacts defended, in order"), PerImpact.DefendedImpacts, TArray<int32>({0, 1}));
			TestEqual(TEXT("Two hits"), PerImpact.Hits.Num(), PER_IMPACT_SPEC_TWO_IMPACTS);
			TestTrue(TEXT("The turn moved on"), PerImpact.bTurnMovedOn);
		});

		It("opens one defense per impact for a 2-entry spell", [this]()
		{
			Spell = MakeSpell({MakeCastEntry(ESpellDeliveryType::AOE), MakeCastEntry(ESpellDeliveryType::AOE)}, /*HitCount*/ 1);
			const FCastOutcome PerImpact = RunCast(Spell);
			if (!TestCastLanded(TEXT("Per impact"), PerImpact))
			{
				return;
			}
			TestEqual(TEXT("One impact per entry, in order"), PerImpact.DefendedImpacts, TArray<int32>({0, 1}));
			TestEqual(TEXT("Two hits"), PerImpact.Hits.Num(), PER_IMPACT_SPEC_TWO_IMPACTS);
			TestTrue(TEXT("The turn moved on"), PerImpact.bTurnMovedOn);
		});

		It("lets a guard on impact 1 leave impact 2 untouched", [this]()
		{
			Spell = MakeTwoHitSpell();
			const FCastOutcome Plain = RunCast(Spell, ECastTwist::None, PER_IMPACT_SPEC_CLOSE_RANGE);
			const FCastOutcome Guarded = RunCast(Spell, ECastTwist::GuardAtCast, PER_IMPACT_SPEC_CLOSE_RANGE);
			if (!TestCastLanded(TEXT("Plain"), Plain) || !TestCastLanded(TEXT("Guarded"), Guarded) ||
				!TestEqual(TEXT("Two impacts resolved"), Guarded.Defenses.Num(), PER_IMPACT_SPEC_TWO_IMPACTS))
			{
				return;
			}
			TestTrue(TEXT("Impact 1 was guarded"), GuardRules::IsGuard(Guarded.Defenses[0]));
			TestEqual(TEXT("Impact 2 was not (the press was used)"), Guarded.Defenses[1], EDefenseType::None);
			TestTrue(TEXT("The guard took damage off"), Guarded.FoeDamage < Plain.FoeDamage);
			if (TestTrue(TEXT("Both runs landed impact 2"), Plain.Hits.Num() > 0 && Guarded.Hits.Num() > 0))
			{
				TestEqual(TEXT("Impact 2 dealt what it does unguarded"), Guarded.Hits.Last(), Plain.Hits.Last());
			}
		});

		It("lands each impact when the action times out", [this]()
		{
			Spell = MakeTwoHitSpell();
			const FCastOutcome Arrived = RunCast(Spell);
			const FCastOutcome TimedOut = RunCast(Spell, ECastTwist::ShootDown);
			if (!TestCastLanded(TEXT("Arrived"), Arrived) || !TestCastLanded(TEXT("Timed out"), TimedOut))
			{
				return;
			}
			TestEqual(TEXT("No impact resolved at an arrival"), TimedOut.DefendedImpacts.Num(), 0);
			TestEqual(TEXT("Both impacts landed, one by one"), TimedOut.Hits, Arrived.Hits);
			TestEqual(TEXT("The same damage as when they arrive"), TimedOut.FoeDamage, Arrived.FoeDamage);
			TestTrue(TEXT("The turn moved on"), TimedOut.bTurnMovedOn);
		});

		It("keeps a spell's window open past the cast animation for projectiles still flying", [this]()
		{
			Spell = MakeTwoHitSpell();
			const FCastOutcome Arrived = RunCast(Spell);
			const FCastOutcome EndedEarly = RunCast(Spell, ECastTwist::AnimationEndsInFlight);
			if (!TestCastLanded(TEXT("Arrived"), Arrived) || !TestCastLanded(TEXT("Animation ended in flight"), EndedEarly))
			{
				return;
			}
			TestEqual(TEXT("Both impacts still resolved at their arrivals"), EndedEarly.DefendedImpacts, TArray<int32>({0, 1}));
			TestEqual(TEXT("The same hits"), EndedEarly.Hits, Arrived.Hits);
			TestTrue(TEXT("The turn moved on"), EndedEarly.bTurnMovedOn);
		});

		It("takes each arrival exactly once through the running ability (wor.Abilities.UseGAS 1)", [this]()
		{
			for (const bool bTwoEntries : {false, true})
			{
				Spell = bTwoEntries ? MakeSpell({MakeCastEntry(ESpellDeliveryType::AOE), MakeCastEntry(ESpellDeliveryType::AOE)}, /*HitCount*/ 1)
									: MakeTwoHitSpell();
				const TCHAR *Shape = bTwoEntries ? TEXT("2-entry AOE") : TEXT("2-hit burst");
				const FCastOutcome OldPath = RunCast(Spell);
				const FCastOutcome ThroughAbility = RunCast(Spell, ECastTwist::None, PER_IMPACT_SPEC_DUEL_RANGE, /*bThroughAbility*/ true);
				Spell->RemoveFromRoot();
				Spell = nullptr;
				if (!TestCastLanded(Shape, OldPath) || !TestCastLanded(Shape, ThroughAbility))
				{
					continue;
				}
				TestEqual(FString::Printf(TEXT("%s: every arrival through the ability, once"), Shape), ThroughAbility.AbilityArrivals, PER_IMPACT_SPEC_TWO_IMPACTS);
				TestEqual(FString::Printf(TEXT("%s: none through an ability on the old path"), Shape), OldPath.AbilityArrivals, 0);
				TestEqual(FString::Printf(TEXT("%s: the same hits"), Shape), ThroughAbility.Hits, OldPath.Hits);
				TestEqual(FString::Printf(TEXT("%s: two impacts defended"), Shape), ThroughAbility.DefendedImpacts, TArray<int32>({0, 1}));
			}
		});

		It("lands a 2-hit support spell's hits at its window's close, the same hits as the per-impact cast (B-032b)", [this]()
		{
			Spell = MakeTwoHitSpell();
			const FCastOutcome PerImpact = RunCast(Spell);
			Spell->TargetType = ETargetType::Ally; // not widened: its window keeps the timer close
			const FCastOutcome AtClose = RunCast(Spell);
			if (!TestCastLanded(TEXT("Per impact"), PerImpact) || !TestCastLanded(TEXT("At the close"), AtClose))
			{
				return;
			}
			TestEqual(TEXT("No impact resolved on its own"), AtClose.DefendedImpacts.Num(), 0);
			TestEqual(TEXT("Same hits, one by one"), AtClose.Hits, PerImpact.Hits);
			TestEqual(TEXT("Same total"), AtClose.FoeDamage, PerImpact.FoeDamage);
		});
	});

	Describe("abilities", [this]()
	{
		It("lands an ability's impacts that no Impact notify landed when its animation ends", [this]()
		{
			Strike->HitCount = PER_IMPACT_SPEC_TWO_IMPACTS;
			// Each impact at once (InstantImpacts 1), as notifies would land them.
			const FCastOutcome Notified = RunCast(Strike, ECastTwist::None, PER_IMPACT_SPEC_SAME_SPOT);

			// No montage and no instant impacts: nothing lands until the animation-end reconcile.
			SetPerImpactSpecCVar(TEXT("wor.Combat.InstantImpacts"), 0);
			AActor *Hero = nullptr;
			AActor *Foe = nullptr;
			if (!TestTrue(TEXT("Duel started"), StartDuel(PER_IMPACT_SPEC_SAME_SPOT, Hero, Foe)))
			{
				return;
			}
			UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
			UDefenseSystem *Defense = TestWorld->GetSubsystem<UDefenseSystem>();
			if (!TestTrue(TEXT("Subsystems"), Executor && Defense))
			{
				return;
			}
			TStrongObjectPtr<UDefenseOutcomeRecorder> Defenses(NewObject<UDefenseOutcomeRecorder>());
			Defenses->Listen(Defense, Foe);
			FAction Action;
			Action.ActionType = EActionType::Ability;
			Action.SkillData = Strike;
			Action.Targets.Add(Foe);
			const int32 FoeHPBefore = CurrentHPOf(Foe);
			Orchestrator->SubmitActionAsync(Action);
			TestWorld->Tick();
			TestEqual(TEXT("Nothing landed before the animation's end"), CurrentHPOf(Foe), FoeHPBefore);

			Executor->ReconcileDefenseWindowsAtAnimationEnd(); // what Skill-end runs
			TestWorld->Tick(PER_IMPACT_SPEC_ACTION_FRAMES);

			TestCastLanded(TEXT("Notified"), Notified);
			TestEqual(TEXT("Both impacts resolved as fallback impacts, in order"), Defenses->ImpactIndices, TArray<int32>({0, 1}));
			TestEqual(TEXT("The same damage as impact frames"), FoeHPBefore - CurrentHPOf(Foe), Notified.FoeDamage);
			TestTrue(TEXT("The action finished (the turn moved on)"), Orchestrator->GetCurrentActor() != Hero);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
