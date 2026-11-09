// GASAbilitiesSpec.cpp
// B-043b (Q-043b): a combatant's skills as granted UWoRGameplayAbility specs (one per skill, the skill as source
// object, its Ability.Shape.* tags), granted at fight start and cleared at fight end; with
// `wor.Abilities.UseGAS 1` an action starts through its skill's ability, which runs the same executor call, spends
// the same EP once and ends when the action completes. The action runs headless as in WoR.Balance.Sim.OneFight
// (a montage-less weapon attack, wor.Combat.InstantImpacts 1).
// B-043c (Q-043c): through its ability, the attack's impacts arrive as Event.Combat.Impact at the ability's
// WaitGameplayEvent task and resolve to the same damage as the old path (same battle seed); an impact on an actor
// with no running ability takes the old path.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
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
}

#endif // WITH_DEV_AUTOMATION_TESTS
