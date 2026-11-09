// GASAbilitiesSpec.cpp
// B-043b (Q-043b): a combatant's skills as granted UWoRGameplayAbility specs (one per skill, the skill as source
// object, its Ability.Shape.* tags), granted at fight start and cleared at fight end; with
// `wor.Abilities.UseGAS 1` an action starts through its skill's ability, which runs the same executor call, spends
// the same EP once and ends when the action completes. The action runs headless as in WoR.Balance.Sim.OneFight
// (a montage-less weapon attack, wor.Combat.InstantImpacts 1).
// B-043c (Q-043c): through its ability, the attack's impacts arrive as Event.Combat.Impact at the ability's
// WaitGameplayEvent task and resolve to the same damage as the old path (same battle seed); an impact on an actor
// with no running ability takes the old path.

// B-043e (Q-043e): an AOE spell, an Instant spell and a support (self) spell, cast the same headless way, each
// resolve through one Event.Combat.SpellResolve.AOE / .Instant / .Support at their ability's WaitGameplayEvent task,
// with the same result as the old path.
// B-043f (Q-043f): through its ability, an action's EP is committed and paid by the ability (once, the same amount),
// and an all-AI 1v1 runs to the same end at both switch values, every impact through an ability.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "BalanceSimSignalRecorder.h"
#include "UObject/StrongObjectPtr.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "Character/CharacterDataComponent.h"
#include "Components/SceneComponent.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GAS/Abilities/WoRGameplayAbility.h"
#include "GAS/Abilities/WoRSkillAbilities.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Tags/WoRGameplayTags.h"

namespace
{
	constexpr int32 ABILITIES_SPEC_STRIKE_DAMAGE = 30;
	constexpr int32 ABILITIES_SPEC_STRIKE_EP = 6;
	constexpr int32 ABILITIES_SPEC_START_EP = 40;
	/** Next-tick timers only once the instant-impact seam is on: a few frames settle one action. */
	constexpr int32 ABILITIES_SPEC_ACTION_FRAMES = 10;
	/** B-043c: a multi-hit strike, so every impact's routing is checked. */
	constexpr int32 ABILITIES_SPEC_MULTI_HIT_COUNT = 3;
	/** B-043c: one battle seed for both paths, so their rolls match. */
	constexpr int32 ABILITIES_SPEC_BATTLE_SEED = 4307;
	/** B-043d: a projectile spell; a burst of three, spaced well inside the frame budget. */
	constexpr int32 ABILITIES_SPEC_BOLT_DAMAGE = 24;
	constexpr int32 ABILITIES_SPEC_BOLT_EP = 5;
	constexpr int32 ABILITIES_SPEC_BURST_COUNT = 3;
	constexpr float ABILITIES_SPEC_BURST_INTERVAL = 0.05f;
	/** Test-world frames are 0.01 s: the combatants share a spot, so a projectile lands in a few frames and a
	 *  burst's last spawn comes after 0.1 s; this budget is several times that. */
	constexpr int32 ABILITIES_SPEC_PROJECTILE_FRAMES = 200;
	/** Units between the duellists: about 0.33 s of flight at the default 1500 units/s. */
	constexpr float ABILITIES_SPEC_DUEL_RANGE = 500.0f;
	/** Inside the burst's first interval (5 frames): the first projectile is out, the second not yet. */
	constexpr int32 ABILITIES_SPEC_BURST_CANCEL_FRAMES = 2;
	/** B-043e: upper bound for one spell-shape action to settle (test ticks are 0.01s; spans the executor's 10s
	 *  timeout, as WoR.Skills.SupportNoBuildup); the run stops as soon as the turn moves on. */
	constexpr int32 ABILITIES_SPEC_SHAPE_SETTLE_FRAMES = 1100;
	/** B-043f: a whole all-AI 1v1 at ~3 frames a turn (as WoR.Balance.Sim.OneFight); reaching it means a stall. */
	constexpr int32 ABILITIES_SPEC_FIGHT_TICK_CAP = 3000;

	void SetAbilitiesSpecCVar(const TCHAR *Name, int32 Value)
	{
		if (IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CVar->Set(Value, ECVF_SetByCode);
		}
	}
}

BEGIN_DEFINE_SPEC(FGASAbilitiesSpec, "WoR.GAS.Abilities",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	UAbilityData *Strike = nullptr;
	USpellData *Bolt = nullptr;
	/** B-043e: the AOE / Instant / support spell of the current case. */
	USpellData *ShapeSpell = nullptr;
	int32 Activations = 0;
	int32 Ends = 0;
	/** B-043f: every ability instance that activated (fight end clears the specs, so the counts are read here). */
	TArray<TStrongObjectPtr<UWoRGameplayAbility>> ActivatedAbilities;

	/** An initialised ASC on Actor that counts its abilities' activations and ends. */
	UAbilitySystemComponent *GiveASC(AActor *Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}
		UAbilitySystemComponent *ASC = FWoRTestWorld::AddComponent<UAbilitySystemComponent>(Actor);
		ASC->InitAbilityActorInfo(Actor, Actor);
		ASC->AbilityActivatedCallbacks.AddLambda([this](UGameplayAbility *Ability)
		{
			++Activations;
			UWoRGameplayAbility *SkillAbility = Cast<UWoRGameplayAbility>(Ability);
			if (SkillAbility && !ActivatedAbilities.ContainsByPredicate([SkillAbility](const TStrongObjectPtr<UWoRGameplayAbility> &Seen) { return Seen.Get() == SkillAbility; }))
			{
				ActivatedAbilities.Emplace(SkillAbility);
			}
		});
		ASC->AbilityEndedCallbacks.AddLambda([this](UGameplayAbility *) { ++Ends; });
		return ASC;
	}

	/** A combatant whose owned weapon's attack is Strike (as WoR.Balance.Sim.OneFight arms its fighters). */
	AActor *SpawnArmed(bool bAIControlled)
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->WeaponAttack = Strike;
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(FWoRTestWorld::NewCharacterData(), Weapon, bAIControlled);
		AActor *Actor = Loadout ? Loadout->GetOwner() : nullptr;
		if (UInventoryComponent *Inventory = Actor ? Actor->FindComponentByClass<UInventoryComponent>() : nullptr)
		{
			FWeaponInventoryEntry Owned;
			Owned.Weapon = Weapon;
			Inventory->Weapons.Add(Owned);
		}
		return Actor;
	}

	static int32 CurrentHPOf(const AActor *Actor)
	{
		const UCharacterDataComponent *Char = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		return Char ? Char->CurrentHP : 0;
	}

	/** Gives Actor a scene root (if it has none) and moves it to Location. */
	static void PlaceAt(AActor *Actor, const FVector &Location)
	{
		if (!Actor)
		{
			return;
		}
		if (!Actor->GetRootComponent())
		{
			USceneComponent *Root = NewObject<USceneComponent>(Actor);
			Actor->SetRootComponent(Root);
			Root->RegisterComponent();
		}
		Actor->SetActorLocation(Location);
	}

	/** Sum of Count over the instances of Actor's ability for Skill. */
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

	/** Impacts Actor's Strike ability has taken as Event.Combat.Impact (B-043c). */
	int32 ImpactEventsOf(AActor *Actor) const
	{
		return SumOverInstances(Actor, Strike, [](const UWoRGameplayAbility &Ability) { return Ability.GetImpactEventsResolved(); });
	}

	struct FStrikeOutcome
	{
		bool bRan = false;
		int32 FoeDamage = 0;
		int32 ImpactEvents = 0;
		bool bStillListening = false;
		/** B-043d */
		int32 EPSpent = 0;
		/** Why the executor refuses the action (empty when it is valid). */
		FString Refusal;
		int32 ProjectileEvents = 0;
		int32 BurstSpawns = 0;
		bool bStillListeningForProjectiles = false;
		/** The turn moved on from the hero: the action completed. */
		bool bTurnMovedOn = false;
		/** B-043f: costs the hero's ability for the skill committed and paid. */
		int32 CostsCommitted = 0;
	};

	/** B-043c: Hero's Strike on Foe in a fresh world (the battle seed set by the caller), one switch value. */
	FStrikeOutcome RunStrike(bool bThroughAbility)
	{
		return RunSkill(bThroughAbility, Strike, /*bAIFoe*/ true, ABILITIES_SPEC_ACTION_FRAMES);
	}

	/** B-043d: Hero's Bolt on a foe that does not defend (no random defense roll), one switch value. */
	FStrikeOutcome RunBolt(bool bThroughAbility)
	{
		return RunSkill(bThroughAbility, Bolt, /*bAIFoe*/ false, ABILITIES_SPEC_PROJECTILE_FRAMES);
	}

	/** B-043d review: Hero's Bolt through its ability, the ability cancelled CancelAfterFrames into the action. */
	FStrikeOutcome RunBoltCancelled(int32 CancelAfterFrames)
	{
		return RunSkill(/*bThroughAbility*/ true, Bolt, /*bAIFoe*/ false, ABILITIES_SPEC_PROJECTILE_FRAMES, CancelAfterFrames);
	}

	/** Hero's Skill on Foe in a fresh world, one switch value; Frames ticked after the submit. */
	FStrikeOutcome RunSkill(bool bThroughAbility, USkillDataBase *Skill, bool bAIFoe, int32 Frames, int32 CancelAfterFrames = INDEX_NONE)
	{
		FStrikeOutcome Outcome;
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		Orchestrator = nullptr;
		TestWorld = MakeUnique<FWoRTestWorld>();
		if (!TestWorld->Create() || !TestWorld->GetWorld())
		{
			return Outcome;
		}
		Orchestrator = TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>();
		WoRSkillAbilities::SetUsesAbilities(bThroughAbility);
		AActor *Hero = SpawnArmed(false);
		GiveASC(Hero);
		AActor *Foe = SpawnArmed(bAIFoe);
		UCharacterDataComponent *HeroStats = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!Orchestrator || !Foe || !HeroStats || !Skill)
		{
			return Outcome;
		}
		Orchestrator->StartCombat({Hero}, {Foe});
		if (Orchestrator->GetCurrentActor() != Hero)
		{
			return Outcome;
		}
		HeroStats->MaxEP = ABILITIES_SPEC_START_EP;
		HeroStats->CurrentEP = ABILITIES_SPEC_START_EP;
		const int32 FoeHPBefore = CurrentHPOf(Foe);
		// A projectile needs a distance to fly (the combatants have no root, so both sit at the origin).
		if (Skill->IsA<USpellData>())
		{
			PlaceAt(Hero, FVector::ZeroVector);
			PlaceAt(Foe, FVector(ABILITIES_SPEC_DUEL_RANGE, 0.0f, 0.0f));
		}

		FAction Action;
		if (USpellData *Spell = Cast<USpellData>(Skill))
		{
			Action.ActionType = EActionType::Spell;
			Action.SpellData = Spell;
		}
		else
		{
			Action.ActionType = EActionType::Ability;
			Action.SkillData = Skill;
		}
		Action.Targets.Add(Foe);
		if (const UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>())
		{
			Outcome.Refusal = Executor->ValidateAction(Hero, Action).ErrorMessage;
		}
		Orchestrator->SubmitActionAsync(Action);
		if (CancelAfterFrames != INDEX_NONE)
		{
			TestWorld->Tick(CancelAfterFrames);
			UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Hero);
			if (const FGameplayAbilitySpec *Spec = WoRSkillAbilities::FindSkillSpec(Hero, Skill); ASC && Spec)
			{
				ASC->CancelAbilityHandle(Spec->Handle);
			}
		}
		TestWorld->Tick(Frames);

		Outcome.bRan = true;
		Outcome.EPSpent = ABILITIES_SPEC_START_EP - HeroStats->CurrentEP;
		Outcome.FoeDamage = FoeHPBefore - CurrentHPOf(Foe);
		Outcome.ImpactEvents = ImpactEventsOf(Hero);
		Outcome.bStillListening = WoRSkillAbilities::FindImpactListener(Hero) != nullptr;
		Outcome.ProjectileEvents = SumOverInstances(Hero, Skill, [](const UWoRGameplayAbility &Ability) { return Ability.GetProjectileEventsResolved(); });
		Outcome.BurstSpawns = SumOverInstances(Hero, Skill, [](const UWoRGameplayAbility &Ability) { return Ability.GetBurstSpawnsDriven(); });
		Outcome.bStillListeningForProjectiles = WoRSkillAbilities::FindProjectileListener(Hero) != nullptr;
		Outcome.bTurnMovedOn = Orchestrator->GetCurrentActor() != Hero;
		Outcome.CostsCommitted = SumOverInstances(Hero, Skill, [](const UWoRGameplayAbility &Ability) { return Ability.GetCostsCommitted(); });
		return Outcome;
	}

	struct FFightOutcome
	{
		bool bEnded = false;
		int32 HeroHP = 0;
		int32 FoeHP = 0;
		int32 Rounds = 0;
		int32 ImpactFrames = 0;
		int32 ImpactEvents = 0;
		int32 Activations = 0;
		int32 Ends = 0;
		int32 CostsCommitted = 0;
	};

	/** B-043f: an all-AI 1v1 of Strike fighters, both with an ASC, in a fresh world at one switch value, run to its
	 *  end (as WoR.Balance.Sim.OneFight; the caller sets wor.AI.ThinkDelayScale 0). */
	FFightOutcome RunFight(bool bThroughAbility)
	{
		FFightOutcome Outcome;
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		Orchestrator = nullptr;
		TestWorld = MakeUnique<FWoRTestWorld>();
		if (!TestWorld->Create() || !TestWorld->GetWorld())
		{
			return Outcome;
		}
		Orchestrator = TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>();
		WoRSkillAbilities::SetUsesAbilities(bThroughAbility);
		AActor *Hero = SpawnArmed(true);
		AActor *Foe = SpawnArmed(true);
		GiveASC(Hero);
		GiveASC(Foe);
		if (!Orchestrator || !Hero || !Foe)
		{
			return Outcome;
		}
		TStrongObjectPtr<UBalanceSimSignalRecorder> Recorder(NewObject<UBalanceSimSignalRecorder>());
		Recorder->Listen(TestWorld->GetSubsystem<UActionExecutor>(), TestWorld->GetSubsystem<UTurnManager>(),
						 TestWorld->GetSubsystem<UStatusBuildupManager>());
		Activations = 0;
		Ends = 0;
		ActivatedAbilities.Reset();
		Orchestrator->StartCombat({Hero}, {Foe});
		for (int32 Tick = 0; Tick < ABILITIES_SPEC_FIGHT_TICK_CAP && Orchestrator->GetCombatState() == ECombatState::InProgress; ++Tick)
		{
			TestWorld->Tick();
		}

		Outcome.bEnded = Orchestrator->GetCombatState() != ECombatState::InProgress;
		Outcome.HeroHP = CurrentHPOf(Hero);
		Outcome.FoeHP = CurrentHPOf(Foe);
		Outcome.Rounds = Recorder->Rounds;
		Outcome.ImpactFrames = Recorder->ImpactIndices.Num();
		Outcome.Activations = Activations;
		Outcome.Ends = Ends;
		for (const TStrongObjectPtr<UWoRGameplayAbility> &Ability : ActivatedAbilities)
		{
			Outcome.ImpactEvents += Ability->GetImpactEventsResolved();
			Outcome.CostsCommitted += Ability->GetCostsCommitted();
		}
		return Outcome;
	}
	/** B-043e: a montage-less Generic spell of one flightless shape (rooted; AfterEach removes it). AOE / Instant: one
	 *  enemy-targeted cast entry of that delivery (as the sim's Instant spell, B-293). Support: a self-targeted spell
	 *  with no cast entries and no damage (the loose path, SpawnSpellDelivery's support branch). */
	USpellData *MakeShapeSpell(EWoRSpellResolveKind Kind)
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Name = TEXT("Montage-less Shape Spell");
		Spell->BaseEnergyCost = ABILITIES_SPEC_BOLT_EP;
		// Generic: castable by the test character (a Caster), as Bolt.
		Spell->Element = ESpellElement::Generic;
		if (Kind == EWoRSpellResolveKind::Support)
		{
			Spell->TargetType = ETargetType::Self;
			Spell->BaseDamage = 0;
		}
		else
		{
			const ESpellDeliveryType Delivery = Kind == EWoRSpellResolveKind::AOE ? ESpellDeliveryType::AOE : ESpellDeliveryType::Instant;
			Spell->TargetType = ETargetType::Enemy;
			Spell->BaseDamage = ABILITIES_SPEC_BOLT_DAMAGE;
			Spell->DeliveryType = Delivery;
			Spell->CastArray.AddDefaulted_GetRef().DeliveryType = Delivery;
		}
		Spell->AddToRoot();
		return Spell;
	}

	struct FShapeOutcome
	{
		bool bRan = false;
		int32 FoeDamage = 0;
		int32 HeroHPChange = 0;
		int32 EPSpent = 0;
		FString Refusal;
		int32 AOEEvents = 0;
		int32 InstantEvents = 0;
		int32 SupportEvents = 0;
		bool bStillListening = false;
		bool bTurnMovedOn = false;
	};

	/** B-043e: Hero casts ShapeSpell in a fresh world (at Foe, or at itself for a support spell), one switch value,
	 *  until the turn moves on. */
	FShapeOutcome RunShape(bool bThroughAbility)
	{
		FShapeOutcome Outcome;
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		Orchestrator = nullptr;
		TestWorld = MakeUnique<FWoRTestWorld>();
		if (!ShapeSpell || !TestWorld->Create() || !TestWorld->GetWorld())
		{
			return Outcome;
		}
		Orchestrator = TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>();
		WoRSkillAbilities::SetUsesAbilities(bThroughAbility);
		AActor *Hero = SpawnArmed(false);
		GiveASC(Hero);
		// Not AI-driven: no defense is rolled, so every hit lands (as the bolt cases).
		AActor *Foe = SpawnArmed(false);
		UCharacterDataComponent *HeroStats = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!Orchestrator || !Foe || !HeroStats)
		{
			return Outcome;
		}
		Orchestrator->StartCombat({Hero}, {Foe});
		if (Orchestrator->GetCurrentActor() != Hero)
		{
			return Outcome;
		}
		HeroStats->MaxEP = ABILITIES_SPEC_START_EP;
		HeroStats->CurrentEP = ABILITIES_SPEC_START_EP;
		const int32 FoeHPBefore = CurrentHPOf(Foe);
		const int32 HeroHPBefore = CurrentHPOf(Hero);

		FAction Action;
		Action.ActionType = EActionType::Spell;
		Action.SpellData = ShapeSpell;
		Action.Targets.Add(ShapeSpell->TargetType == ETargetType::Enemy ? Foe : Hero);
		if (const UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>())
		{
			Outcome.Refusal = Executor->ValidateAction(Hero, Action).ErrorMessage;
		}
		Orchestrator->SubmitActionAsync(Action);
		for (int32 Frame = 0; Frame < ABILITIES_SPEC_SHAPE_SETTLE_FRAMES && Orchestrator->GetCurrentActor() == Hero; ++Frame)
		{
			TestWorld->Tick();
		}

		Outcome.bRan = true;
		Outcome.EPSpent = ABILITIES_SPEC_START_EP - HeroStats->CurrentEP;
		Outcome.FoeDamage = FoeHPBefore - CurrentHPOf(Foe);
		Outcome.HeroHPChange = CurrentHPOf(Hero) - HeroHPBefore;
		Outcome.AOEEvents = SumOverInstances(Hero, ShapeSpell, [](const UWoRGameplayAbility &Ability) { return Ability.GetAOEResolves(); });
		Outcome.InstantEvents = SumOverInstances(Hero, ShapeSpell, [](const UWoRGameplayAbility &Ability) { return Ability.GetInstantResolves(); });
		Outcome.SupportEvents = SumOverInstances(Hero, ShapeSpell, [](const UWoRGameplayAbility &Ability) { return Ability.GetSupportResolves(); });
		Outcome.bStillListening = WoRSkillAbilities::FindSpellResolveListener(Hero) != nullptr;
		Outcome.bTurnMovedOn = Orchestrator->GetCurrentActor() == Foe;
		return Outcome;
	}

	/** B-043e: one shape's case: the same result at wor.Abilities.UseGAS 0 and 1, and exactly one event of its kind
	 *  through the ability (none on the old path). */
	void CheckShapeParity(EWoRSpellResolveKind Kind)
	{
		ShapeSpell = MakeShapeSpell(Kind);
		const FShapeOutcome OldPath = RunShape(false);
		const FShapeOutcome ThroughAbility = RunShape(true);
		if (!TestTrue(TEXT("Both runs reached the cast"), OldPath.bRan && ThroughAbility.bRan))
		{
			return;
		}
		TestTrue(FString::Printf(TEXT("The old path cast it (EP spent %d; refusal: %s)"), OldPath.EPSpent, *OldPath.Refusal),
				 OldPath.EPSpent > 0);
		if (Kind != EWoRSpellResolveKind::Support)
		{
			TestTrue(TEXT("The old path dealt damage"), OldPath.FoeDamage > 0);
		}
		TestEqual(TEXT("Same EP through the ability"), ThroughAbility.EPSpent, OldPath.EPSpent);
		TestEqual(TEXT("Same damage to the foe through the ability"), ThroughAbility.FoeDamage, OldPath.FoeDamage);
		TestEqual(TEXT("Same change to the caster's HP through the ability"), ThroughAbility.HeroHPChange, OldPath.HeroHPChange);
		TestEqual(TEXT("No spell-resolve events on the old path"),
				  OldPath.AOEEvents + OldPath.InstantEvents + OldPath.SupportEvents, 0);
		TestEqual(TEXT("One AOE event"), ThroughAbility.AOEEvents, Kind == EWoRSpellResolveKind::AOE ? 1 : 0);
		TestEqual(TEXT("One Instant event"), ThroughAbility.InstantEvents, Kind == EWoRSpellResolveKind::Instant ? 1 : 0);
		TestEqual(TEXT("One Support event"), ThroughAbility.SupportEvents, Kind == EWoRSpellResolveKind::Support ? 1 : 0);
		TestFalse(TEXT("Nothing listens once the action is done"), ThroughAbility.bStillListening);
		TestTrue(TEXT("The turn moved on (both paths)"), OldPath.bTurnMovedOn && ThroughAbility.bTurnMovedOn);
	}
END_DEFINE_SPEC(FGASAbilitiesSpec)

void FGASAbilitiesSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		Strike = NewObject<UAbilityData>(GetTransientPackage());
		Strike->Name = TEXT("Montage-less Strike");
		Strike->BaseDamage = ABILITIES_SPEC_STRIKE_DAMAGE;
		Strike->BaseEnergyCost = ABILITIES_SPEC_STRIKE_EP;
		Strike->ExecutionType = EAbilityExecutionType::Melee;
		Strike->bIsAttack = true;
		Strike->AddToRoot();
		// B-043d: an offensive projectile spell, one Projectile cast entry (a burst sets its Count).
		Bolt = NewObject<USpellData>(GetTransientPackage());
		Bolt->Name = TEXT("Montage-less Bolt");
		Bolt->BaseDamage = ABILITIES_SPEC_BOLT_DAMAGE;
		Bolt->BaseEnergyCost = ABILITIES_SPEC_BOLT_EP;
		Bolt->ExecutionType = EAbilityExecutionType::Ranged;
		Bolt->TargetType = ETargetType::Enemy;
		// Generic (inherit) is castable by any Caster (the test character's class), so no element gate refuses it.
		Bolt->Element = ESpellElement::Generic;
		Bolt->DeliveryType = ESpellDeliveryType::Projectile;
		FSkillCastEntry &BoltCast = Bolt->CastArray.AddDefaulted_GetRef();
		BoltCast.DeliveryType = ESpellDeliveryType::Projectile;
		BoltCast.BurstInterval = ABILITIES_SPEC_BURST_INTERVAL;
		Bolt->AddToRoot();
		Activations = 0;
		Ends = 0;
		SetAbilitiesSpecCVar(TEXT("wor.Combat.InstantImpacts"), 1);
		// One battle seed for every case: no random roll (or a seed left by an earlier spec) decides a damage check.
		SetAbilitiesSpecCVar(TEXT("wor.Battle.Seed"), ABILITIES_SPEC_BATTLE_SEED);
	});

	AfterEach([this]()
	{
		WoRSkillAbilities::SetUsesAbilities(false);
		ActivatedAbilities.Reset();
		SetAbilitiesSpecCVar(TEXT("wor.Combat.InstantImpacts"), 0);
		SetAbilitiesSpecCVar(TEXT("wor.Battle.Seed"), 0);
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		if (Strike)
		{
			Strike->RemoveFromRoot();
			Strike = nullptr;
		}
		if (Bolt)
		{
			Bolt->RemoveFromRoot();
			Bolt = nullptr;
		}
		if (ShapeSpell)
		{
			ShapeSpell->RemoveFromRoot();
			ShapeSpell = nullptr;
		}
		Orchestrator = nullptr;
		TestWorld.Reset();
	});

	Describe("granting", [this]()
	{
		It("gives one spec per skill, with the skill as its source object and its shape tags", [this]()
		{
			AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			if (!TestNotNull(TEXT("ASC"), GiveASC(Hero)))
			{
				return;
			}
			UAbilityData *Shout = NewObject<UAbilityData>(GetTransientPackage());
			Shout->ExecutionType = EAbilityExecutionType::SelfCast;
			USpellData *Burst = NewObject<USpellData>(GetTransientPackage());
			Burst->ExecutionType = EAbilityExecutionType::Ranged;
			FSkillCastEntry AreaCast;
			AreaCast.DeliveryType = ESpellDeliveryType::AOE;
			Burst->CastArray.Add(AreaCast);

			TestEqual(TEXT("Three new specs (repeats and nulls skipped)"),
					  WoRSkillAbilities::GrantSkills(Hero, {Strike, Shout, Burst, Strike, nullptr}), 3);
			TestEqual(TEXT("Three skill specs"), WoRSkillAbilities::GetSkillSpecs(Hero).Num(), 3);

			const FGameplayAbilitySpec *StrikeSpec = WoRSkillAbilities::FindSkillSpec(Hero, Strike);
			if (TestNotNull(TEXT("Strike's spec"), StrikeSpec))
			{
				TestTrue(TEXT("Strike is the source object"), StrikeSpec->SourceObject.Get() == Strike);
				TestTrue(TEXT("Strike is Melee"), StrikeSpec->GetDynamicSpecSourceTags().HasTagExact(WoRTags::Ability_Shape_Melee));
			}
			const FGameplayAbilitySpec *ShoutSpec = WoRSkillAbilities::FindSkillSpec(Hero, Shout);
			if (TestNotNull(TEXT("Shout's spec"), ShoutSpec))
			{
				TestTrue(TEXT("Shout is SelfCast"), ShoutSpec->GetDynamicSpecSourceTags().HasTagExact(WoRTags::Ability_Shape_SelfCast));
				TestFalse(TEXT("Shout is not Melee"), ShoutSpec->GetDynamicSpecSourceTags().HasTagExact(WoRTags::Ability_Shape_Melee));
			}
			const FGameplayAbilitySpec *BurstSpec = WoRSkillAbilities::FindSkillSpec(Hero, Burst);
			if (TestNotNull(TEXT("Burst's spec"), BurstSpec))
			{
				const FGameplayTagContainer &Tags = BurstSpec->GetDynamicSpecSourceTags();
				TestTrue(TEXT("Burst is Ranged"), Tags.HasTagExact(WoRTags::Ability_Shape_Ranged));
				TestTrue(TEXT("Burst is AOE"), Tags.HasTagExact(WoRTags::Ability_Shape_AOE));
				TestFalse(TEXT("Burst is not Projectile"), Tags.HasTagExact(WoRTags::Ability_Shape_Projectile));
			}

			TestEqual(TEXT("Granting again adds nothing"), WoRSkillAbilities::GrantSkills(Hero, {Strike, Shout, Burst}), 0);
			TestEqual(TEXT("Cleared"), WoRSkillAbilities::ClearSkills(Hero), 3);
			TestEqual(TEXT("No skill specs left"), WoRSkillAbilities::GetSkillSpecs(Hero).Num(), 0);
		});

		It("grants nothing without an ability system component", [this]()
		{
			AActor *Bare = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			TestEqual(TEXT("No spec"), WoRSkillAbilities::GrantSkills(Bare, {Strike}), 0);
			TestFalse(TEXT("No action through an ability"),
					  WoRSkillAbilities::TryExecuteThroughAbility(Bare, WoRSkillAbilities::MakePlainAction(Strike), FOnActionComplete()));
		});

		It("ends at once when activated with no action", [this]()
		{
			AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			UAbilitySystemComponent *ASC = GiveASC(Hero);
			const FGameplayAbilitySpecHandle Handle = WoRSkillAbilities::GrantSkill(Hero, Strike);
			UCharacterDataComponent *HeroStats = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
			if (!TestNotNull(TEXT("ASC"), ASC) || !TestTrue(TEXT("Granted"), Handle.IsValid()) || !TestNotNull(TEXT("Stats"), HeroStats))
			{
				return;
			}
			// CheckCost prices the plain cast with no action: enough EP for it.
			HeroStats->MaxEP = ABILITIES_SPEC_START_EP;
			HeroStats->CurrentEP = ABILITIES_SPEC_START_EP;
			ASC->TryActivateAbility(Handle);
			TestEqual(TEXT("Activated once"), Activations, 1);
			TestEqual(TEXT("Ended once"), Ends, 1);
			const FGameplayAbilitySpec *Spec = WoRSkillAbilities::FindSkillSpec(Hero, Strike);
			TestTrue(TEXT("Not active"), Spec && !Spec->IsActive());
		});

		It("grants the loadout's weapon attack at fight start and clears it at fight end", [this]()
		{
			if (!TestNotNull(TEXT("Orchestrator"), Orchestrator))
			{
				return;
			}
			AActor *Hero = SpawnArmed(false);
			GiveASC(Hero);
			AActor *Foe = SpawnArmed(true);
			if (!TestTrue(TEXT("Combatants spawned"), Hero && Foe))
			{
				return;
			}
			TestTrue(TEXT("The loadout lists Strike"), WoRSkillAbilities::GetLoadoutSkills(Hero).Contains(Strike));
			Orchestrator->StartCombat({Hero}, {Foe});
			TestNotNull(TEXT("Strike granted at fight start"), WoRSkillAbilities::FindSkillSpec(Hero, Strike));
			Orchestrator->ForceEndCombat();
			TestEqual(TEXT("Cleared at fight end"), WoRSkillAbilities::GetSkillSpecs(Hero).Num(), 0);
		});
	});

	for (const bool bThroughAbility : {false, true})
	{
		Describe(bThroughAbility ? TEXT("with wor.Abilities.UseGAS 1") : TEXT("with wor.Abilities.UseGAS 0"),
				 [this, bThroughAbility]()
		{
			It("spends the action's EP once, and an ability runs it and ends when it completes", [this, bThroughAbility]()
			{
				if (!TestNotNull(TEXT("Orchestrator"), Orchestrator))
				{
					return;
				}
				WoRSkillAbilities::SetUsesAbilities(bThroughAbility);
				AActor *Hero = SpawnArmed(false);
				GiveASC(Hero);
				// Not AI-driven: no defense is rolled for it, so the single hit always lands (an AI foe could block
				// it on some seeds). Its turn then waits for input, far past these frames.
				AActor *Foe = SpawnArmed(false);
				UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
				UCharacterDataComponent *HeroStats = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
				if (!TestTrue(TEXT("Set up"), Hero && Foe && Executor && HeroStats))
				{
					return;
				}
				// Equal stats: the tie-break gives the local party (Hero) turn 1.
				Orchestrator->StartCombat({Hero}, {Foe});
				if (!TestTrue(TEXT("Turn 1 is Hero's"), Orchestrator->GetCurrentActor() == Hero))
				{
					return;
				}
				HeroStats->MaxEP = ABILITIES_SPEC_START_EP;
				HeroStats->CurrentEP = ABILITIES_SPEC_START_EP;
				const int32 FoeHPBefore = CurrentHPOf(Foe);

				FAction Action;
				Action.ActionType = EActionType::Ability;
				Action.SkillData = Strike;
				Action.Targets.Add(Foe);
				const int32 ExpectedCost = Executor->CalculateActionEnergyCost(Hero, Action);
				TestTrue(TEXT("Strike costs EP"), ExpectedCost > 0);

				Orchestrator->SubmitActionAsync(Action);
				TestWorld->Tick(ABILITIES_SPEC_ACTION_FRAMES);

				TestEqual(TEXT("EP spent once, the cost formula's"), HeroStats->CurrentEP, ABILITIES_SPEC_START_EP - ExpectedCost);
				TestTrue(FString::Printf(TEXT("The foe took damage (HP %d -> %d, %d hit(s), turn of %s)"), FoeHPBefore,
										 CurrentHPOf(Foe), Strike->HitCount, *GetNameSafe(Orchestrator->GetCurrentActor())),
						 CurrentHPOf(Foe) < FoeHPBefore);
				TestTrue(TEXT("The turn moved on to the foe"), Orchestrator->GetCurrentActor() == Foe);
				TestEqual(TEXT("Ability activations"), Activations, bThroughAbility ? 1 : 0);
				TestEqual(TEXT("Ability ends"), Ends, bThroughAbility ? 1 : 0);
				const FGameplayAbilitySpec *Spec = WoRSkillAbilities::FindSkillSpec(Hero, Strike);
				if (TestNotNull(TEXT("Strike's spec granted"), Spec))
				{
					TestFalse(TEXT("The ability is not active after the action"), Spec->IsActive());
				}
				TestEqual(TEXT("Impacts through Event.Combat.Impact (B-043c)"), ImpactEventsOf(Hero),
						  bThroughAbility ? Strike->HitCount : 0);
				TestEqual(TEXT("The cost paid through the ability's commit (B-043f)"),
						  SumOverInstances(Hero, Strike, [](const UWoRGameplayAbility &Ability) { return Ability.GetCostsCommitted(); }),
						  bThroughAbility ? 1 : 0);
			});
		});
	}

	Describe("impacts (B-043c)", [this]()
	{
		It("lands a multi-hit attack's impacts through Event.Combat.Impact, with the old path's damage", [this]()
		{
			Strike->HitCount = ABILITIES_SPEC_MULTI_HIT_COUNT;
			const FStrikeOutcome OldPath = RunStrike(false);
			const FStrikeOutcome ThroughAbility = RunStrike(true);
			if (!TestTrue(TEXT("Both runs reached the strike"), OldPath.bRan && ThroughAbility.bRan))
			{
				return;
			}
			TestTrue(TEXT("The old path dealt damage"), OldPath.FoeDamage > 0);
			TestEqual(TEXT("Same damage through the ability"), ThroughAbility.FoeDamage, OldPath.FoeDamage);
			TestEqual(TEXT("No impact events on the old path"), OldPath.ImpactEvents, 0);
			TestEqual(TEXT("Every impact through the ability's event"), ThroughAbility.ImpactEvents, ABILITIES_SPEC_MULTI_HIT_COUNT);
			TestFalse(TEXT("Nothing listens once the action is done"), ThroughAbility.bStillListening);
		});

		It("leaves an impact on an actor with no running ability to the old path", [this]()
		{
			AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			if (!TestNotNull(TEXT("ASC"), GiveASC(Hero)) || !TestTrue(TEXT("Granted"), WoRSkillAbilities::GrantSkill(Hero, Strike).IsValid()))
			{
				return;
			}
			TestNull(TEXT("An idle ability is not listening"), WoRSkillAbilities::FindImpactListener(Hero));
			TestFalse(TEXT("The impact is not taken"), WoRSkillAbilities::SendImpactEvent(Hero, 0));
			AActor *Bare = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			TestFalse(TEXT("No ASC: not taken"), WoRSkillAbilities::SendImpactEvent(Bare, 0));
			TestFalse(TEXT("No actor: not taken"), WoRSkillAbilities::SendImpactEvent(nullptr, 0));
		});

		It("listens only for a non-spell action that lands impacts", [this]()
		{
			TestTrue(TEXT("A melee attack"), UWoRGameplayAbility::ShouldListenForImpacts(WoRSkillAbilities::MakePlainAction(Strike)));
			UAbilityData *NoHits = NewObject<UAbilityData>(GetTransientPackage());
			NoHits->HitCount = 0;
			TestFalse(TEXT("No impacts"), UWoRGameplayAbility::ShouldListenForImpacts(WoRSkillAbilities::MakePlainAction(NoHits)));
			TestFalse(TEXT("A spell"), UWoRGameplayAbility::ShouldListenForImpacts(WoRSkillAbilities::MakePlainAction(Bolt)));
		});
	});

	Describe("projectiles (B-043d)", [this]()
	{
		It("lands a projectile spell through Event.Combat.Projectile.Arrived, with the old path's damage", [this]()
		{
			const FStrikeOutcome OldPath = RunBolt(false);
			const FStrikeOutcome ThroughAbility = RunBolt(true);
			if (!TestTrue(TEXT("Both runs reached the cast"), OldPath.bRan && ThroughAbility.bRan))
			{
				return;
			}
			TestTrue(FString::Printf(TEXT("The old path dealt damage (EP spent %d, through the ability %d; refusal: %s)"), OldPath.EPSpent, ThroughAbility.EPSpent, *OldPath.Refusal), OldPath.FoeDamage > 0);
			TestEqual(TEXT("Same damage through the ability"), ThroughAbility.FoeDamage, OldPath.FoeDamage);
			TestEqual(TEXT("No projectile events on the old path"), OldPath.ProjectileEvents, 0);
			TestEqual(TEXT("The arrival through the ability's event"), ThroughAbility.ProjectileEvents, 1);
			TestEqual(TEXT("No burst for a single projectile"), ThroughAbility.BurstSpawns, 0);
			TestFalse(TEXT("Nothing listens once the action is done"), ThroughAbility.bStillListeningForProjectiles);
		});

		It("runs a burst's stagger through the ability's WaitDelay chain, with the old path's damage", [this]()
		{
			Bolt->CastArray[0].Count = ABILITIES_SPEC_BURST_COUNT;
			const FStrikeOutcome OldPath = RunBolt(false);
			const FStrikeOutcome ThroughAbility = RunBolt(true);
			if (!TestTrue(TEXT("Both runs reached the cast"), OldPath.bRan && ThroughAbility.bRan))
			{
				return;
			}
			TestTrue(FString::Printf(TEXT("The old path dealt damage (EP spent %d, through the ability %d; refusal: %s)"), OldPath.EPSpent, ThroughAbility.EPSpent, *OldPath.Refusal), OldPath.FoeDamage > 0);
			TestEqual(TEXT("Same damage through the ability"), ThroughAbility.FoeDamage, OldPath.FoeDamage);
			TestEqual(TEXT("No burst spawns by an ability on the old path"), OldPath.BurstSpawns, 0);
			TestEqual(TEXT("Every arrival through the ability's event"), ThroughAbility.ProjectileEvents, ABILITIES_SPEC_BURST_COUNT);
			TestEqual(TEXT("The chain spawned every projectile after the first"), ThroughAbility.BurstSpawns,
					  ABILITIES_SPEC_BURST_COUNT - 1);
			TestFalse(TEXT("Nothing listens once the action is done"), ThroughAbility.bStillListeningForProjectiles);
		});

		It("finishes a burst on the executor's timer when the ability is cancelled mid-burst", [this]()
		{
			Bolt->CastArray[0].Count = ABILITIES_SPEC_BURST_COUNT;
			const FStrikeOutcome OldPath = RunBolt(false);
			const FStrikeOutcome Cancelled = RunBoltCancelled(ABILITIES_SPEC_BURST_CANCEL_FRAMES);
			if (!TestTrue(TEXT("Both runs reached the cast"), OldPath.bRan && Cancelled.bRan))
			{
				return;
			}
			TestTrue(TEXT("The old path dealt damage"), OldPath.FoeDamage > 0);
			TestEqual(TEXT("Same damage after the cancel"), Cancelled.FoeDamage, OldPath.FoeDamage);
			TestEqual(TEXT("The chain spawned nothing before the cancel"), Cancelled.BurstSpawns, 0);
			TestTrue(TEXT("The action completed (the turn moved on)"), Cancelled.bTurnMovedOn);
		});

		It("leaves an arrival or a burst on an actor with no running ability to the old path", [this]()
		{
			AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			if (!TestNotNull(TEXT("ASC"), GiveASC(Hero)) || !TestTrue(TEXT("Granted"), WoRSkillAbilities::GrantSkill(Hero, Bolt).IsValid()))
			{
				return;
			}
			TestNull(TEXT("An idle ability is not listening"), WoRSkillAbilities::FindProjectileListener(Hero));
			TestFalse(TEXT("The arrival is not taken"),
					  WoRSkillAbilities::SendProjectileEvent(Hero, WoRTags::Event_Combat_Projectile_Arrived, Foe, 0));
			TestFalse(TEXT("The dodge is not taken"),
					  WoRSkillAbilities::SendProjectileEvent(Hero, WoRTags::Event_Combat_Projectile_Dodged, Foe, INDEX_NONE));
			TestFalse(TEXT("The burst is not taken"), WoRSkillAbilities::StartBurstThroughAbility(Hero, ABILITIES_SPEC_BURST_INTERVAL));
			TestFalse(TEXT("No actor: not taken"),
					  WoRSkillAbilities::SendProjectileEvent(nullptr, WoRTags::Event_Combat_Projectile_Arrived, Foe, 0));
		});

		It("listens only for an offensive skill with a Projectile delivery", [this]()
		{
			TestTrue(TEXT("A projectile spell"), UWoRGameplayAbility::ShouldListenForProjectiles(WoRSkillAbilities::MakePlainAction(Bolt)));
			TestFalse(TEXT("A melee attack"), UWoRGameplayAbility::ShouldListenForProjectiles(WoRSkillAbilities::MakePlainAction(Strike)));
			Bolt->TargetType = ETargetType::Ally;
			TestFalse(TEXT("A support cast"), UWoRGameplayAbility::ShouldListenForProjectiles(WoRSkillAbilities::MakePlainAction(Bolt)));
			Bolt->TargetType = ETargetType::Enemy;
			Bolt->CastArray[0].DeliveryType = ESpellDeliveryType::AOE;
			TestFalse(TEXT("An AOE spell"), UWoRGameplayAbility::ShouldListenForProjectiles(WoRSkillAbilities::MakePlainAction(Bolt)));
		});
	});

	Describe("AOE, instant and support resolutions (B-043e)", [this]()
	{
		It("listens only for a skill with a flightless resolution", [this]()
		{
			ShapeSpell = MakeShapeSpell(EWoRSpellResolveKind::AOE);
			TestTrue(TEXT("An AOE spell"), UWoRGameplayAbility::ShouldListenForSpellResolves(WoRSkillAbilities::MakePlainAction(ShapeSpell)));
			USpellData *Instant = NewObject<USpellData>(GetTransientPackage());
			FSkillCastEntry InstantCast;
			InstantCast.DeliveryType = ESpellDeliveryType::Instant;
			Instant->CastArray.Add(InstantCast);
			TestTrue(TEXT("An Instant spell"), UWoRGameplayAbility::ShouldListenForSpellResolves(WoRSkillAbilities::MakePlainAction(Instant)));
			USpellData *LooseAOE = NewObject<USpellData>(GetTransientPackage());
			LooseAOE->DeliveryType = ESpellDeliveryType::AOE;
			TestTrue(TEXT("A spell with no cast entries, AOE delivery"),
					 UWoRGameplayAbility::ShouldListenForSpellResolves(WoRSkillAbilities::MakePlainAction(LooseAOE)));
			USpellData *Support = NewObject<USpellData>(GetTransientPackage());
			Support->TargetType = ETargetType::Ally;
			Support->DeliveryType = ESpellDeliveryType::Projectile;
			TestTrue(TEXT("A support spell (any delivery)"),
					 UWoRGameplayAbility::ShouldListenForSpellResolves(WoRSkillAbilities::MakePlainAction(Support)));
			TestFalse(TEXT("A projectile spell (B-043d)"), UWoRGameplayAbility::ShouldListenForSpellResolves(WoRSkillAbilities::MakePlainAction(Bolt)));
			TestFalse(TEXT("A melee attack"), UWoRGameplayAbility::ShouldListenForSpellResolves(WoRSkillAbilities::MakePlainAction(Strike)));
			TestFalse(TEXT("No skill"), UWoRGameplayAbility::ShouldListenForSpellResolves(FAction()));
		});

		It("names each kind with its own event under Event.Combat.SpellResolve", [this]()
		{
			for (const EWoRSpellResolveKind Kind : {EWoRSpellResolveKind::AOE, EWoRSpellResolveKind::Instant, EWoRSpellResolveKind::Support})
			{
				const FGameplayTag Tag = WoRSkillAbilities::GetSpellResolveEventTag(Kind);
				EWoRSpellResolveKind RoundTrip = EWoRSpellResolveKind::AOE;
				TestTrue(TEXT("Under Event.Combat.SpellResolve"), Tag.MatchesTag(WoRTags::Event_Combat_SpellResolve));
				TestTrue(TEXT("Read back"), WoRSkillAbilities::GetSpellResolveKind(Tag, RoundTrip) && RoundTrip == Kind);
			}
			EWoRSpellResolveKind Unused = EWoRSpellResolveKind::AOE;
			TestFalse(TEXT("A projectile end is no spell resolve"), WoRSkillAbilities::GetSpellResolveKind(WoRTags::Event_Combat_Projectile_Arrived, Unused));
		});

		It("leaves a resolution on an actor with no running ability to the old path", [this]()
		{
			ShapeSpell = MakeShapeSpell(EWoRSpellResolveKind::AOE);
			AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			if (!TestNotNull(TEXT("ASC"), GiveASC(Hero)) || !TestTrue(TEXT("Granted"), WoRSkillAbilities::GrantSkill(Hero, ShapeSpell).IsValid()))
			{
				return;
			}
			FWoRSpellResolve Resolve;
			Resolve.Target = Foe;
			TestNull(TEXT("An idle ability is not listening"), WoRSkillAbilities::FindSpellResolveListener(Hero));
			TestFalse(TEXT("The AOE hit is not taken"), WoRSkillAbilities::SendSpellResolveEvent(Hero, Resolve));
			Resolve.Kind = EWoRSpellResolveKind::Support;
			Resolve.Targets.Add(Hero);
			TestFalse(TEXT("The support cast is not taken"), WoRSkillAbilities::SendSpellResolveEvent(Hero, Resolve));
			TestFalse(TEXT("No ASC: not taken"), WoRSkillAbilities::SendSpellResolveEvent(Foe, Resolve));
			TestFalse(TEXT("No actor: not taken"), WoRSkillAbilities::SendSpellResolveEvent(nullptr, Resolve));
		});

		It("lands an AOE spell through one Event.Combat.SpellResolve.AOE, with the old path's damage", [this]()
		{
			CheckShapeParity(EWoRSpellResolveKind::AOE);
		});

		It("lands an Instant spell through one Event.Combat.SpellResolve.Instant, with the old path's damage", [this]()
		{
			CheckShapeParity(EWoRSpellResolveKind::Instant);
		});

		It("resolves a support spell through one Event.Combat.SpellResolve.Support, with the old path's result", [this]()
		{
			CheckShapeParity(EWoRSpellResolveKind::Support);
		});
	});

	Describe("costs and whole fights (B-043f)", [this]()
	{
		It("pays a spell's EP once, through its ability's commit, the old path's amount", [this]()
		{
			const FStrikeOutcome OldPath = RunBolt(false);
			const FStrikeOutcome ThroughAbility = RunBolt(true);
			if (!TestTrue(TEXT("Both runs reached the cast"), OldPath.bRan && ThroughAbility.bRan))
			{
				return;
			}
			TestTrue(FString::Printf(TEXT("The old path paid EP (refusal: %s)"), *OldPath.Refusal), OldPath.EPSpent > 0);
			TestEqual(TEXT("Same EP through the ability"), ThroughAbility.EPSpent, OldPath.EPSpent);
			TestEqual(TEXT("No ability commit on the old path"), OldPath.CostsCommitted, 0);
			TestEqual(TEXT("One commit through the ability"), ThroughAbility.CostsCommitted, 1);
		});

		It("leaves the cost to the executor when no ability runs the action", [this]()
		{
			AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			UCharacterDataComponent *HeroStats = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
			if (!TestNotNull(TEXT("ASC"), GiveASC(Hero)) || !TestNotNull(TEXT("Stats"), HeroStats) ||
				!TestTrue(TEXT("Granted"), WoRSkillAbilities::GrantSkill(Hero, Strike).IsValid()))
			{
				return;
			}
			HeroStats->CurrentEP = ABILITIES_SPEC_START_EP;
			const FAction Action = WoRSkillAbilities::MakePlainAction(Strike);
			TestNull(TEXT("An idle ability runs no action"), WoRSkillAbilities::FindActionRunner(Hero, Action));
			TestFalse(TEXT("The cost is not taken"), WoRSkillAbilities::CommitCostThroughAbility(Hero, Action, ABILITIES_SPEC_STRIKE_EP));
			TestEqual(TEXT("Nothing paid"), HeroStats->CurrentEP, ABILITIES_SPEC_START_EP);
			TestFalse(TEXT("No actor: not taken"), WoRSkillAbilities::CommitCostThroughAbility(nullptr, Action, ABILITIES_SPEC_STRIKE_EP));
		});

		It("runs an all-AI 1v1 to the same end through abilities, every impact and cost through them", [this]()
		{
			SetAbilitiesSpecCVar(TEXT("wor.AI.ThinkDelayScale"), 0);
			const FFightOutcome OldPath = RunFight(false);
			const FFightOutcome ThroughAbility = RunFight(true);
			SetAbilitiesSpecCVar(TEXT("wor.AI.ThinkDelayScale"), 1);
			if (!TestTrue(TEXT("Both fights ended (not stalled at the tick cap)"), OldPath.bEnded && ThroughAbility.bEnded))
			{
				return;
			}
			TestTrue(TEXT("One side is down"), ThroughAbility.HeroHP <= 0 || ThroughAbility.FoeHP <= 0);
			TestEqual(TEXT("Same rounds"), ThroughAbility.Rounds, OldPath.Rounds);
			TestEqual(TEXT("Same hero HP at the end"), ThroughAbility.HeroHP, OldPath.HeroHP);
			TestEqual(TEXT("Same foe HP at the end"), ThroughAbility.FoeHP, OldPath.FoeHP);
			TestEqual(TEXT("No ability ran on the old path"), OldPath.Activations, 0);
			TestTrue(TEXT("Abilities ran the fight"), ThroughAbility.Activations > 0);
			TestEqual(TEXT("Every ability ended"), ThroughAbility.Ends, ThroughAbility.Activations);
			// Strict on purpose: one battle seed, and Strike passes every gate, so each activation reaches its pay point;
			// a mismatch means an action paid outside its ability or not at all.
			TestEqual(TEXT("Every action's cost paid through its ability"), ThroughAbility.CostsCommitted, ThroughAbility.Activations);
			TestTrue(TEXT("Impacts landed"), ThroughAbility.ImpactFrames > 0);
			TestEqual(TEXT("Every impact through an ability"), ThroughAbility.ImpactEvents, ThroughAbility.ImpactFrames);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
