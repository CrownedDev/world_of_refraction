// OrchestratorActionsSpec.cpp
// B-165 (D-067): what the legacy ACombatOrchestratorTestActor checked by hand through the old sync router
// (Test_SpellExecution, Test_EnergyCost, Test_MultiTargetAction, Test_StatusEffectFromAction), now through
// SubmitActionAsync on a real ACombatOrchestrator: a spell damages its target and reports that damage, the
// energy it reports is what the caster lost, a spell on three targets hits and counts each, and Defend reports
// itself as a successful Defend. Cast headless as in WoR.Combat.PerImpactDefense (montage-less skills,
// wor.Combat.InstantImpacts 1); the foes are player-driven, so no defense is rolled for them.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "Components/SceneComponent.h"
#include "UObject/StrongObjectPtr.h"
#include "WoRTestWorld.h"
#include "ActionExecutedRecorder.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatOrchestrator.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 ORCHESTRATOR_ACTIONS_SPELL_DAMAGE = 40;
	constexpr int32 ORCHESTRATOR_ACTIONS_SPELL_EP = 5;
	constexpr int32 ORCHESTRATOR_ACTIONS_START_EP = 40;
	constexpr int32 ORCHESTRATOR_ACTIONS_TOUGH_HP = 10000;
	constexpr int32 ORCHESTRATOR_ACTIONS_THREE_FOES = 3;
	/** A 100% Crit Chance Debuff on the hero and Luck Debuff on each foe: no crit and no luck dodge. */
	constexpr float ORCHESTRATOR_ACTIONS_FULL_DEBUFF_PERCENT = 100.0f;
	constexpr int32 ORCHESTRATOR_ACTIONS_EFFECT_TURNS = 3;
	constexpr int32 ORCHESTRATOR_ACTIONS_NO_CRIT_ID = 0;
	constexpr int32 ORCHESTRATOR_ACTIONS_NO_LUCK_ID = 1;
	constexpr int32 ORCHESTRATOR_ACTIONS_BATTLE_SEED = 1650;
	constexpr float ORCHESTRATOR_ACTIONS_FOE_SPACING = 200.0f;
	/** Spans the executor's 10 s action timeout at 0.01 s a frame; a run stops once the action is reported. */
	constexpr int32 ORCHESTRATOR_ACTIONS_SETTLE_FRAMES = 1100;

	void SetOrchestratorActionsCVar(const TCHAR *Name, int32 Value)
	{
		if (IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CVar->Set(Value, ECVF_SetByCode);
		}
	}
}

BEGIN_DEFINE_SPEC(FOrchestratorActionsSpec, "WoR.Combat.OrchestratorActions",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	USpellData *Spell = nullptr;
	AActor *Hero = nullptr;
	TArray<AActor *> Foes;
	TStrongObjectPtr<UActionExecutedRecorder> Recorder;

	static UCharacterDataComponent *StatsOf(const AActor *Actor)
	{
		return Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
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

	/** A rooted, montage-less Generic instant spell at enemies (Generic: castable by the test character, a Caster). */
	USpellData *MakeSpell(ETargetCount TargetCount)
	{
		FSkillCastEntry Entry;
		Entry.DeliveryType = ESpellDeliveryType::Instant;
		USpellData *NewSpell = NewObject<USpellData>(GetTransientPackage());
		NewSpell->Name = TEXT("Montage-less Instant Spell");
		NewSpell->BaseDamage = ORCHESTRATOR_ACTIONS_SPELL_DAMAGE;
		NewSpell->BaseEnergyCost = ORCHESTRATOR_ACTIONS_SPELL_EP;
		NewSpell->ExecutionType = EAbilityExecutionType::Ranged;
		NewSpell->TargetType = ETargetType::Enemy;
		NewSpell->TargetCount = TargetCount;
		NewSpell->Element = ESpellElement::Generic;
		NewSpell->CastArray = {Entry};
		NewSpell->DeliveryType = Entry.DeliveryType;
		NewSpell->AddToRoot();
		return NewSpell;
	}

	/** A fresh fight: the hero to act against FoeCount tough foes; false when it isn't the hero's turn. */
	bool StartFight(int32 FoeCount)
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		if (!TestWorld->Create() || !TestWorld->GetWorld())
		{
			return false;
		}
		Orchestrator = TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>();
		USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foes.Reset();
		for (int32 Index = 0; Index < FoeCount; ++Index)
		{
			Foes.Add(TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData()));
		}
		if (!Orchestrator || !Effects || !Hero || Foes.Contains(nullptr))
		{
			return false;
		}
		Orchestrator->StartCombat({Hero}, Foes);
		if (Orchestrator->GetCurrentActor() != Hero)
		{
			return false;
		}
		Recorder.Reset(NewObject<UActionExecutedRecorder>());
		Recorder->Listen(Orchestrator);

		UCharacterDataComponent *HeroStats = StatsOf(Hero);
		HeroStats->MaxEP = ORCHESTRATOR_ACTIONS_START_EP;
		HeroStats->CurrentEP = ORCHESTRATOR_ACTIONS_START_EP;
		Effects->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("No crit"), ORCHESTRATOR_ACTIONS_NO_CRIT_ID, ESkillEffectType::CritChanceDebuff,
																  ORCHESTRATOR_ACTIONS_FULL_DEBUFF_PERCENT, ORCHESTRATOR_ACTIONS_EFFECT_TURNS), Hero);
		PlaceAt(Hero, FVector::ZeroVector);
		for (int32 Index = 0; Index < Foes.Num(); ++Index)
		{
			UCharacterDataComponent *FoeStats = StatsOf(Foes[Index]);
			FoeStats->MaxHP = ORCHESTRATOR_ACTIONS_TOUGH_HP;
			FoeStats->CurrentHP = ORCHESTRATOR_ACTIONS_TOUGH_HP;
			Effects->ApplyEffect(Foes[Index], FActiveSkillEffect::CreateBuff(TEXT("No luck"), ORCHESTRATOR_ACTIONS_NO_LUCK_ID, ESkillEffectType::LuckDebuff,
																			 ORCHESTRATOR_ACTIONS_FULL_DEBUFF_PERCENT, ORCHESTRATOR_ACTIONS_EFFECT_TURNS), Foes[Index]);
			PlaceAt(Foes[Index], FVector(ORCHESTRATOR_ACTIONS_FOE_SPACING, ORCHESTRATOR_ACTIONS_FOE_SPACING * Index, 0.0f));
		}
		return true;
	}

	/** Submits the hero's action through the async path and ticks until it is reported; null when it never was. */
	const FActionResult *Submit(const FAction &Action)
	{
		Orchestrator->SubmitActionAsync(Action);
		for (int32 Frame = 0; Frame < ORCHESTRATOR_ACTIONS_SETTLE_FRAMES && Recorder->Entries.Num() == 0; ++Frame)
		{
			TestWorld->Tick();
		}
		return Recorder->Entries.Num() > 0 ? &Recorder->Entries[0].Result : nullptr;
	}

	FAction SpellOn(const TArray<AActor *> &Targets) const
	{
		FAction Action;
		Action.ActionType = EActionType::Spell;
		Action.SpellData = Spell;
		Action.Targets = Targets;
		return Action;
	}

	int32 DamageTaken(const AActor *Foe) const
	{
		return ORCHESTRATOR_ACTIONS_TOUGH_HP - StatsOf(Foe)->CurrentHP;
	}
END_DEFINE_SPEC(FOrchestratorActionsSpec)

void FOrchestratorActionsSpec::Define()
{
	BeforeEach([this]()
	{
		SetOrchestratorActionsCVar(TEXT("wor.Combat.InstantImpacts"), 1);
		// One battle seed for every run, so no roll left by an earlier spec decides a damage check.
		SetOrchestratorActionsCVar(TEXT("wor.Battle.Seed"), ORCHESTRATOR_ACTIONS_BATTLE_SEED);
	});

	AfterEach([this]()
	{
		SetOrchestratorActionsCVar(TEXT("wor.Combat.InstantImpacts"), 0);
		SetOrchestratorActionsCVar(TEXT("wor.Battle.Seed"), 0);
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		Orchestrator = nullptr;
		Recorder.Reset();
		Hero = nullptr;
		Foes.Reset();
		TestWorld.Reset();
		if (Spell)
		{
			Spell->RemoveFromRoot();
			Spell = nullptr;
		}
	});

	Describe("a spell", [this]()
	{
		It("damages its target and reports that damage (Test_SpellExecution)", [this]()
		{
			Spell = MakeSpell(ETargetCount::Single);
			if (!TestTrue(TEXT("The fight starts on the hero's turn"), StartFight(1)))
			{
				return;
			}
			const FActionResult *Result = Submit(SpellOn(Foes));
			if (!TestNotNull(TEXT("The action is reported"), Result))
			{
				return;
			}
			TestTrue(TEXT("Succeeded"), Result->bSuccess);
			TestEqual(TEXT("Reported as a spell"), Result->ActionType, EActionType::Spell);
			TestTrue(TEXT("The target lost HP"), DamageTaken(Foes[0]) > 0);
			TestEqual(TEXT("The reported damage is the HP lost"), Result->TotalDamageDealt, DamageTaken(Foes[0]));
		});

		It("reports the energy the caster lost (Test_EnergyCost)", [this]()
		{
			Spell = MakeSpell(ETargetCount::Single);
			if (!TestTrue(TEXT("The fight starts on the hero's turn"), StartFight(1)))
			{
				return;
			}
			const FActionResult *Result = Submit(SpellOn(Foes));
			if (!TestNotNull(TEXT("The action is reported"), Result))
			{
				return;
			}
			TestTrue(TEXT("Energy was spent"), Result->EnergySpent > 0);
			TestEqual(TEXT("The caster lost what was reported"), ORCHESTRATOR_ACTIONS_START_EP - StatsOf(Hero)->CurrentEP, Result->EnergySpent);
		});

		It("on three targets hits and counts each (Test_MultiTargetAction)", [this]()
		{
			Spell = MakeSpell(ETargetCount::All);
			if (!TestTrue(TEXT("The fight starts on the hero's turn"), StartFight(ORCHESTRATOR_ACTIONS_THREE_FOES)))
			{
				return;
			}
			const FActionResult *Result = Submit(SpellOn(Foes));
			if (!TestNotNull(TEXT("The action is reported"), Result))
			{
				return;
			}
			int32 TotalTaken = 0;
			for (const AActor *Foe : Foes)
			{
				TestTrue(TEXT("Each target lost HP"), DamageTaken(Foe) > 0);
				TotalTaken += DamageTaken(Foe);
			}
			TestEqual(TEXT("Three affected targets"), Result->AffectedTargets.Num(), ORCHESTRATOR_ACTIONS_THREE_FOES);
			TestEqual(TEXT("The reported damage is the HP lost over all three"), Result->TotalDamageDealt, TotalTaken);
		});
	});

	Describe("Defend", [this]()
	{
		It("is reported as a successful Defend (Test_StatusEffectFromAction)", [this]()
		{
			if (!TestTrue(TEXT("The fight starts on the hero's turn"), StartFight(1)))
			{
				return;
			}
			FAction Defend;
			Defend.ActionType = EActionType::Defend;
			Defend.Targets.Add(Hero);
			const FActionResult *Result = Submit(Defend);
			if (!TestNotNull(TEXT("The action is reported"), Result))
			{
				return;
			}
			TestTrue(TEXT("Succeeded"), Result->bSuccess);
			TestEqual(TEXT("Reported as Defend"), Result->ActionType, EActionType::Defend);
			TestTrue(TEXT("The hero is among the affected"), Result->AffectedTargets.Contains(Hero));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
