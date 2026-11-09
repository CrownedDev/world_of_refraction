// GASAbilitiesSpec.cpp
// B-043b (Q-043b): a combatant's skills as granted UWoRGameplayAbility specs (one per skill, the skill as source
// object, its Ability.Shape.* tags), granted at fight start and cleared at fight end; with
// `wor.Abilities.UseGAS 1` an action starts through its skill's ability, which runs the same executor call, spends
// the same EP once and ends when the action completes. The action runs headless as in WoR.Balance.Sim.OneFight
// (a montage-less weapon attack, wor.Combat.InstantImpacts 1).
// B-043c (Q-043c): through its ability, the attack's impacts arrive as Event.Combat.Impact at the ability's
// WaitGameplayEvent task and resolve to the same damage as the old path (same battle seed); an impact on an actor
// with no running ability takes the old path.
// B-043d (Q-043d): a projectile spell's ends arrive as Event.Combat.Projectile.Arrived / .Dodged at its ability's
// WaitGameplayEvent task. Run headless like the sim's Instant spell (B-293): a Caster's innate, montage-less spell
// released at once (wor.Combat.InstantImpacts 1); caster and foe stand at the origin, so each projectile reaches its
// target on its first tick. A dodge moves the foe away while its projectile is in flight.
// B-043e (Q-043e): an AOE spell, an Instant spell and a support (self) spell, cast the same headless way, each
// resolve through one Event.Combat.SpellResolve.AOE / .Instant / .Support at their ability's WaitGameplayEvent task,
// with the same result as the old path.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "Combat/Projectile/SkillProjectile.h"
#include "Loadout/FCombatLoadout.h"
#include "Skills/Definitions/SkillCastEntry.h"
#include "Abilities/GameplayAbility.h"
#include "Character/CharacterDataComponent.h"
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
	/** B-043d: the projectile spell and its burst. */
	constexpr int32 ABILITIES_SPEC_BOLT_DAMAGE = 30;
	constexpr int32 ABILITIES_SPEC_BURST_COUNT = 3;
	constexpr float ABILITIES_SPEC_BURST_INTERVAL = 0.05f;
	/** Enough frames for the release, every burst spawn and every flight (each lands on its first tick). */
	constexpr int32 ABILITIES_SPEC_PROJECTILE_FRAMES = 30;
	/** Far past any impact radius: the foe has left the zone its projectile aims at. */
	constexpr float ABILITIES_SPEC_DODGE_DISTANCE = 100000.0f;
	/** B-043e: upper bound for one spell-shape action to settle (test ticks are 0.01s; spans the executor's 10s
	 *  timeout, as WoR.Skills.SupportNoBuildup); the run stops as soon as the turn moves on. */
	constexpr int32 ABILITIES_SPEC_SHAPE_SETTLE_FRAMES = 1100;

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
	USpellData *BoltSpell = nullptr;
	/** B-043e: the AOE / Instant / support spell of the current case. */
	USpellData *ShapeSpell = nullptr;
	int32 Activations = 0;
	int32 Ends = 0;

	/** An initialised ASC on Actor that counts its abilities' activations and ends. */
	UAbilitySystemComponent *GiveASC(AActor *Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}
		UAbilitySystemComponent *ASC = FWoRTestWorld::AddComponent<UAbilitySystemComponent>(Actor);
		ASC->InitAbilityActorInfo(Actor, Actor);
		ASC->AbilityActivatedCallbacks.AddLambda([this](UGameplayAbility *) { ++Activations; });
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

	/** Impacts Actor's Strike ability has taken as Event.Combat.Impact (B-043c). */
	int32 ImpactEventsOf(AActor *Actor) const
	{
		int32 Total = 0;
		if (FGameplayAbilitySpec *Spec = WoRSkillAbilities::FindSkillSpec(Actor, Strike))
		{
			for (UGameplayAbility *Instance : Spec->GetAbilityInstances())
			{
				if (const UWoRGameplayAbility *Ability = Cast<UWoRGameplayAbility>(Instance))
				{
					Total += Ability->GetImpactEventsResolved();
				}
			}
		}
		return Total;
	}

	struct FStrikeOutcome
	{
		bool bRan = false;
		int32 FoeDamage = 0;
		int32 ImpactEvents = 0;
		bool bStillListening = false;
	};

	/** B-043c: Hero's Strike on Foe in a fresh world (the battle seed set by the caller), one switch value. */
	FStrikeOutcome RunStrike(bool bThroughAbility)
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
		AActor *Foe = SpawnArmed(true);
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

		FAction Action;
		Action.ActionType = EActionType::Ability;
		Action.SkillData = Strike;
		Action.Targets.Add(Foe);
		Orchestrator->SubmitActionAsync(Action);
		TestWorld->Tick(ABILITIES_SPEC_ACTION_FRAMES);

		Outcome.bRan = true;
		Outcome.FoeDamage = FoeHPBefore - CurrentHPOf(Foe);
		Outcome.ImpactEvents = ImpactEventsOf(Hero);
		Outcome.bStillListening = WoRSkillAbilities::FindImpactListener(Hero) != nullptr;
		return Outcome;
	}

	/** B-043d: a projectile spell with one cast entry of Count projectiles (rooted; AfterEach removes it). */
	USpellData *MakeBolt(int32 Count)
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Name = TEXT("Montage-less Bolt");
		Spell->BaseDamage = ABILITIES_SPEC_BOLT_DAMAGE;
		Spell->BaseEnergyCost = ABILITIES_SPEC_STRIKE_EP;
		Spell->Element = ESpellElement::Fire;
		Spell->DeliveryType = ESpellDeliveryType::Projectile;
		FSkillCastEntry &Cast = Spell->CastArray.AddDefaulted_GetRef();
		Cast.DeliveryType = ESpellDeliveryType::Projectile;
		Cast.Count = Count;
		Cast.BurstInterval = ABILITIES_SPEC_BURST_INTERVAL;
		Spell->AddToRoot();
		return Spell;
	}

	/** B-043d: a Fire Caster with BoltSpell innate (as the sim gives a Caster its spell, B-293). */
	AActor *SpawnBoltCaster()
	{
		return SpawnSpellCaster(BoltSpell);
	}

	/** B-043d/e: a Fire Caster with Spell innate. */
	AActor *SpawnSpellCaster(USpellData *Spell)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Caster;
		Data->InnateElement = ESpellElement::Fire;
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->WeaponAttack = Strike;
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, Weapon, false);
		AActor *Actor = Loadout ? Loadout->GetOwner() : nullptr;
		UInventoryComponent *Inventory = Actor ? Actor->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!Inventory || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
		{
			return nullptr;
		}
		Inventory->LearnSpell(Spell);
		Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].InnateSpells.Add(Spell);
		Loadout->NotifyActiveWeaponChanged();
		return Actor;
	}

	/** The live (not yet landed) projectile in the test world, or null. */
	ASkillProjectile *FindFlyingProjectile() const
	{
		for (TActorIterator<ASkillProjectile> It(TestWorld->GetWorld()); It; ++It)
		{
			if (IsValid(*It) && !It->HasImpacted())
			{
				return *It;
			}
		}
		return nullptr;
	}

	struct FBoltOutcome
	{
		bool bRan = false;
		bool bSawProjectile = false;
		int32 FoeDamage = 0;
		int32 Arrivals = 0;
		int32 Dodges = 0;
		bool bStillListening = false;
		bool bTurnMovedOn = false;
	};

	/** B-043d: Hero casts BoltSpell at Foe in a fresh world, one switch value; with bDodge the foe steps out of the
	 *  impact zone once the first projectile is in flight. */
	FBoltOutcome RunBolt(bool bThroughAbility, bool bDodge)
	{
		FBoltOutcome Outcome;
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
		AActor *Hero = SpawnBoltCaster();
		GiveASC(Hero);
		// Not AI-driven: no defense is rolled, so every arrival lands (as the UseGAS 0/1 strike cases).
		AActor *Foe = SpawnArmed(false);
		UCharacterDataComponent *HeroStats = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!Orchestrator || !Foe || !HeroStats)
		{
			return Outcome;
		}
		if (bDodge)
		{
			// A plain actor has no root, so it could not move: give it one, at the origin.
			USceneComponent *Root = FWoRTestWorld::AddComponent<USceneComponent>(Foe);
			Foe->SetRootComponent(Root);
		}
		Orchestrator->StartCombat({Hero}, {Foe});
		if (Orchestrator->GetCurrentActor() != Hero)
		{
			return Outcome;
		}
		HeroStats->MaxEP = ABILITIES_SPEC_START_EP;
		HeroStats->CurrentEP = ABILITIES_SPEC_START_EP;
		const int32 FoeHPBefore = CurrentHPOf(Foe);

		FAction Action;
		Action.ActionType = EActionType::Spell;
		Action.SpellData = BoltSpell;
		Action.SpellSource = ESpellSource::Innate;
		Action.Targets.Add(Foe);
		Orchestrator->SubmitActionAsync(Action);
		Outcome.bSawProjectile = FindFlyingProjectile() != nullptr;
		for (int32 Frame = 0; Frame < ABILITIES_SPEC_PROJECTILE_FRAMES; ++Frame)
		{
			if (bDodge && !Outcome.bSawProjectile && FindFlyingProjectile())
			{
				Outcome.bSawProjectile = true;
			}
			if (bDodge && Outcome.bSawProjectile && Foe->GetActorLocation().IsNearlyZero())
			{
				Foe->SetActorLocation(FVector(ABILITIES_SPEC_DODGE_DISTANCE, 0.0f, 0.0f));
			}
			TestWorld->Tick();
			Outcome.bSawProjectile = Outcome.bSawProjectile || FindFlyingProjectile() != nullptr;
		}

		Outcome.bRan = true;
		Outcome.FoeDamage = FoeHPBefore - CurrentHPOf(Foe);
		if (FGameplayAbilitySpec *Spec = WoRSkillAbilities::FindSkillSpec(Hero, BoltSpell))
		{
			for (UGameplayAbility *Instance : Spec->GetAbilityInstances())
			{
				if (const UWoRGameplayAbility *Ability = Cast<UWoRGameplayAbility>(Instance))
				{
					Outcome.Arrivals += Ability->GetProjectileArrivalsResolved();
					Outcome.Dodges += Ability->GetProjectileDodgesResolved();
				}
			}
		}
		Outcome.bStillListening = WoRSkillAbilities::FindProjectileListener(Hero) != nullptr;
		Outcome.bTurnMovedOn = Orchestrator->GetCurrentActor() == Foe;
		return Outcome;
	}

	/** B-043e: a montage-less Fire spell of one flightless shape (rooted; AfterEach removes it). AOE / Instant: one
	 *  enemy-targeted cast entry of that delivery (as the sim's Instant spell, B-293). Support: a self-targeted spell
	 *  with no cast entries and no damage (the loose path, SpawnSpellDelivery's support branch). */
	USpellData *MakeShapeSpell(EWoRSpellResolveKind Kind)
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Name = TEXT("Montage-less Shape Spell");
		Spell->BaseEnergyCost = ABILITIES_SPEC_STRIKE_EP;
		Spell->Element = ESpellElement::Fire;
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
		AActor *Hero = SpawnSpellCaster(ShapeSpell);
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
		Action.SpellSource = ESpellSource::Innate;
		Action.Targets.Add(ShapeSpell->TargetType == ETargetType::Enemy ? Foe : Hero);
		Orchestrator->SubmitActionAsync(Action);
		for (int32 Frame = 0; Frame < ABILITIES_SPEC_SHAPE_SETTLE_FRAMES && Orchestrator->GetCurrentActor() == Hero; ++Frame)
		{
			TestWorld->Tick();
		}

		Outcome.bRan = true;
		Outcome.FoeDamage = FoeHPBefore - CurrentHPOf(Foe);
		Outcome.HeroHPChange = CurrentHPOf(Hero) - HeroHPBefore;
		if (FGameplayAbilitySpec *Spec = WoRSkillAbilities::FindSkillSpec(Hero, ShapeSpell))
		{
			for (UGameplayAbility *Instance : Spec->GetAbilityInstances())
			{
				if (const UWoRGameplayAbility *Ability = Cast<UWoRGameplayAbility>(Instance))
				{
					Outcome.AOEEvents += Ability->GetAOEResolves();
					Outcome.InstantEvents += Ability->GetInstantResolves();
					Outcome.SupportEvents += Ability->GetSupportResolves();
				}
			}
		}
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
		if (Kind != EWoRSpellResolveKind::Support)
		{
			TestTrue(TEXT("The old path dealt damage"), OldPath.FoeDamage > 0);
		}
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
		Activations = 0;
		Ends = 0;
		SetAbilitiesSpecCVar(TEXT("wor.Combat.InstantImpacts"), 1);
		// One battle seed for every case: no random roll (or a seed left by an earlier spec) decides a damage check.
		SetAbilitiesSpecCVar(TEXT("wor.Battle.Seed"), ABILITIES_SPEC_BATTLE_SEED);
	});

	AfterEach([this]()
	{
		WoRSkillAbilities::SetUsesAbilities(false);
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
		if (BoltSpell)
		{
			BoltSpell->RemoveFromRoot();
			BoltSpell = nullptr;
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
			USpellData *Bolt = NewObject<USpellData>(GetTransientPackage());
			TestFalse(TEXT("A spell"), UWoRGameplayAbility::ShouldListenForImpacts(WoRSkillAbilities::MakePlainAction(Bolt)));
		});
	});

	Describe("projectiles (B-043d)", [this]()
	{
		It("listens only for a skill that fires projectiles", [this]()
		{
			BoltSpell = MakeBolt(1);
			TestTrue(TEXT("A projectile spell"), UWoRGameplayAbility::ShouldListenForProjectiles(WoRSkillAbilities::MakePlainAction(BoltSpell)));
			USpellData *Loose = NewObject<USpellData>(GetTransientPackage());
			Loose->DeliveryType = ESpellDeliveryType::Projectile;
			TestTrue(TEXT("A spell with no cast entries, Projectile delivery"),
					 UWoRGameplayAbility::ShouldListenForProjectiles(WoRSkillAbilities::MakePlainAction(Loose)));
			USpellData *Area = NewObject<USpellData>(GetTransientPackage());
			FSkillCastEntry AreaCast;
			AreaCast.DeliveryType = ESpellDeliveryType::AOE;
			Area->CastArray.Add(AreaCast);
			TestFalse(TEXT("An AOE spell (B-043e)"), UWoRGameplayAbility::ShouldListenForProjectiles(WoRSkillAbilities::MakePlainAction(Area)));
			TestFalse(TEXT("A melee attack"), UWoRGameplayAbility::ShouldListenForProjectiles(WoRSkillAbilities::MakePlainAction(Strike)));
			TestFalse(TEXT("No skill"), UWoRGameplayAbility::ShouldListenForProjectiles(FAction()));
		});

		It("names an arrival and a dodge with their own events", [this]()
		{
			const FGameplayTag Arrived = WoRSkillAbilities::GetProjectileEventTag(false);
			const FGameplayTag Dodged = WoRSkillAbilities::GetProjectileEventTag(true);
			TestTrue(TEXT("Arrived"), Arrived == WoRTags::Event_Combat_Projectile_Arrived);
			TestTrue(TEXT("Dodged"), Dodged == WoRTags::Event_Combat_Projectile_Dodged);
			TestTrue(TEXT("Both under Event.Combat.Projectile"),
					 Arrived.MatchesTag(WoRTags::Event_Combat_Projectile) && Dodged.MatchesTag(WoRTags::Event_Combat_Projectile));
			TestFalse(TEXT("Not an impact frame"), Arrived.MatchesTag(WoRTags::Event_Combat_Impact));
		});

		It("leaves a projectile on an actor with no running ability to the old path", [this]()
		{
			BoltSpell = MakeBolt(1);
			AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			if (!TestNotNull(TEXT("ASC"), GiveASC(Hero)) || !TestTrue(TEXT("Granted"), WoRSkillAbilities::GrantSkill(Hero, BoltSpell).IsValid()))
			{
				return;
			}
			FWoRProjectileArrival Arrival;
			Arrival.Target = Foe;
			TestNull(TEXT("An idle ability is not listening"), WoRSkillAbilities::FindProjectileListener(Hero));
			TestFalse(TEXT("The arrival is not taken"), WoRSkillAbilities::SendProjectileEvent(Hero, Arrival));
			Arrival.bDodged = true;
			TestFalse(TEXT("The dodge is not taken"), WoRSkillAbilities::SendProjectileEvent(Hero, Arrival));
			TestFalse(TEXT("No ASC: not taken"), WoRSkillAbilities::SendProjectileEvent(Foe, Arrival));
			TestFalse(TEXT("No actor: not taken"), WoRSkillAbilities::SendProjectileEvent(nullptr, Arrival));
		});

		It("lands a projectile spell through Event.Combat.Projectile.Arrived, with the old path's damage", [this]()
		{
			BoltSpell = MakeBolt(1);
			const FBoltOutcome OldPath = RunBolt(false, false);
			const FBoltOutcome ThroughAbility = RunBolt(true, false);
			if (!TestTrue(TEXT("Both runs reached the cast"), OldPath.bRan && ThroughAbility.bRan))
			{
				return;
			}
			TestTrue(TEXT("The old path dealt damage"), OldPath.FoeDamage > 0);
			TestEqual(TEXT("Same damage through the ability"), ThroughAbility.FoeDamage, OldPath.FoeDamage);
			TestEqual(TEXT("No projectile events on the old path"), OldPath.Arrivals, 0);
			TestEqual(TEXT("One arrival through the ability's event"), ThroughAbility.Arrivals, 1);
			TestEqual(TEXT("No dodge"), ThroughAbility.Dodges, 0);
			TestFalse(TEXT("Nothing listens once the action is done"), ThroughAbility.bStillListening);
			TestTrue(TEXT("The turn moved on (both paths)"), OldPath.bTurnMovedOn && ThroughAbility.bTurnMovedOn);
		});

		It("lands every projectile of a burst once, with the old path's damage", [this]()
		{
			BoltSpell = MakeBolt(ABILITIES_SPEC_BURST_COUNT);
			const FBoltOutcome OldPath = RunBolt(false, false);
			const FBoltOutcome ThroughAbility = RunBolt(true, false);
			if (!TestTrue(TEXT("Both runs reached the cast"), OldPath.bRan && ThroughAbility.bRan))
			{
				return;
			}
			TestTrue(TEXT("The old path dealt damage"), OldPath.FoeDamage > 0);
			TestEqual(TEXT("Same damage through the ability"), ThroughAbility.FoeDamage, OldPath.FoeDamage);
			TestEqual(TEXT("Every projectile arrived once through the ability"), ThroughAbility.Arrivals, ABILITIES_SPEC_BURST_COUNT);
			TestFalse(TEXT("Nothing listens once the burst is done"), ThroughAbility.bStillListening);
			TestTrue(TEXT("The turn moved on (both paths)"), OldPath.bTurnMovedOn && ThroughAbility.bTurnMovedOn);
		});

		It("resolves a dodged projectile as a dodge through Event.Combat.Projectile.Dodged", [this]()
		{
			BoltSpell = MakeBolt(1);
			const FBoltOutcome OldPath = RunBolt(false, true);
			const FBoltOutcome ThroughAbility = RunBolt(true, true);
			if (!TestTrue(TEXT("Both runs reached the cast"), OldPath.bRan && ThroughAbility.bRan))
			{
				return;
			}
			if (!TestTrue(TEXT("A projectile was in flight to dodge"), OldPath.bSawProjectile && ThroughAbility.bSawProjectile))
			{
				return;
			}
			TestEqual(TEXT("No damage on the old path"), OldPath.FoeDamage, 0);
			TestEqual(TEXT("No damage through the ability"), ThroughAbility.FoeDamage, 0);
			TestEqual(TEXT("One dodge through the ability's event"), ThroughAbility.Dodges, 1);
			TestEqual(TEXT("No arrival"), ThroughAbility.Arrivals, 0);
			TestTrue(TEXT("The turn moved on (both paths)"), OldPath.bTurnMovedOn && ThroughAbility.bTurnMovedOn);
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
			BoltSpell = MakeBolt(1);
			TestFalse(TEXT("A projectile spell (B-043d)"), UWoRGameplayAbility::ShouldListenForSpellResolves(WoRSkillAbilities::MakePlainAction(BoltSpell)));
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
			TestTrue(TEXT("AOE"), WoRSkillAbilities::GetSpellResolveEventTag(EWoRSpellResolveKind::AOE) == WoRTags::Event_Combat_SpellResolve_AOE);
			TestTrue(TEXT("Instant"), WoRSkillAbilities::GetSpellResolveEventTag(EWoRSpellResolveKind::Instant) == WoRTags::Event_Combat_SpellResolve_Instant);
			TestTrue(TEXT("Support"), WoRSkillAbilities::GetSpellResolveEventTag(EWoRSpellResolveKind::Support) == WoRTags::Event_Combat_SpellResolve_Support);
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
}

#endif // WITH_DEV_AUTOMATION_TESTS
