// RitualInfusionSpec.cpp
// B-624 (Crown, 2026-11-25: "if i use the skill the ritual cast will be infused then if i turn infusion of when the
// ritual activates it will auto infuse for the attack"): a weapon ability armed as a ritual locks the infusion picked at
// the arm. The arm reports (and its VFX tints with) that element and pays all the EP, infusion included; the fire
// strikes with it, cost-free, even after the player's next action went out uninfused (infusion turned off). Driven
// through a real ACombatOrchestrator on a bare 1v1. Unproven here: the colour on screen. These skills have no montage, so
// the arm context the VFX reads is gone before a check; the arm's result carries the same value (docs/PIE/B-624.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "ActionExecutedRecorder.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/TurnManager.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Equipment/Weapons/WeaponData.h"
#include "HAL/IConsoleManager.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	constexpr int32 RITUAL_INFUSION_DELAY_TURNS = 3;
	constexpr int32 RITUAL_INFUSION_DAMAGE = 30;
	constexpr int32 RITUAL_INFUSION_EP = 6;
	constexpr int32 RITUAL_INFUSION_START_EP = 60;
	constexpr int32 RITUAL_INFUSION_MAX_STEPS = 10;
	/** Spans the executor's 10 s action timeout at 0.01 s a frame. */
	constexpr int32 RITUAL_INFUSION_SETTLE_FRAMES = 1100;
	constexpr int32 RITUAL_INFUSION_BATTLE_SEED = 6240;

	void SetRitualInfusionCVar(const TCHAR *Name, int32 Value)
	{
		if (IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CVar->Set(Value, ECVF_SetByCode);
		}
	}
}

BEGIN_DEFINE_SPEC(FRitualInfusionSpec, "WoR.Combat.RitualInfusion",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	UActionExecutor *Executor = nullptr;
	UActionExecutedRecorder *Recorder = nullptr;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;
	UAbilityData *Ritual = nullptr;
	UAbilityData *PlainStrike = nullptr;

	FAction MakeStrike(UAbilityData *Ability, EInfusionSourceOption Source, int32 Level) const
	{
		FAction Action;
		Action.ActionType = EActionType::Ability;
		Action.SkillData = Ability;
		Action.SelectedSource = Source;
		Action.AbilityInfusionLevel = Level;
		Action.Targets.Add(Foe);
		return Action;
	}

	int32 FoeHP() const
	{
		const UCharacterDataComponent *Stats = Foe ? Foe->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		return Stats ? Stats->CurrentHP : 0;
	}

	/** Hero's recorded results, in order. */
	TArray<FActionResult> HeroResults() const
	{
		TArray<FActionResult> Out;
		for (const UActionExecutedRecorder::FEntry &Entry : Recorder->Entries)
		{
			if (Entry.Actor.Get() == Hero && Entry.ActionType == EActionType::Ability)
			{
				Out.Add(Entry.Result);
			}
		}
		return Out;
	}

	/** Plays turns until the ritual has fired (Hero's result after the arm that dealt damage) or the cap. On Hero's
	 *  first normal turn after the arm it plays PlainStrike with no infusion: the player turned infusion off. */
	void PlayUntilFired(int32 FoeHPAfterArm, bool &bPlayedPlain)
	{
		UTurnManager *TurnManager = TestWorld->GetSubsystem<UTurnManager>();
		for (int32 Step = 0; Step < RITUAL_INFUSION_MAX_STEPS && FoeHP() == FoeHPAfterArm &&
							 Orchestrator->GetCombatState() == ECombatState::InProgress; ++Step)
		{
			const bool bHeroNormalTurn = Orchestrator->GetCurrentActor() == Hero && TurnManager &&
										 TurnManager->GetCurrentTurnType() != ETurnType::Execution;
			if (bHeroNormalTurn && !bPlayedPlain)
			{
				bPlayedPlain = true;
				Orchestrator->SubmitActionAsync(MakeStrike(PlainStrike, EInfusionSourceOption::None, 0));
				TestWorld->Tick(RITUAL_INFUSION_SETTLE_FRAMES);
				if (Orchestrator->GetCurrentActor() != Hero)
				{
					continue; // the strike ended the turn
				}
			}
			Orchestrator->OnActionCompleted(); // ends the current normal turn; an Execution turn fires inside the advance
			TestWorld->Tick(RITUAL_INFUSION_SETTLE_FRAMES);
		}
	}
END_DEFINE_SPEC(FRitualInfusionSpec)

void FRitualInfusionSpec::Define()
{
	BeforeEach([this]()
	{
		SetRitualInfusionCVar(TEXT("wor.Combat.InstantImpacts"), 1);
		SetRitualInfusionCVar(TEXT("wor.Battle.Seed"), RITUAL_INFUSION_BATTLE_SEED);
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		Executor = TestWorld->GetSubsystem<UActionExecutor>();

		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Caster; // an Innate source to infuse with
		Data->InnateElement = ESpellElement::Fire;
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, NewObject<UWeaponData>(GetTransientPackage()));
		Hero = Loadout ? Loadout->GetOwner() : nullptr;
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());

		Ritual = NewObject<UAbilityData>(GetTransientPackage());
		Ritual->Name = TEXT("Montage-less Ritual Strike");
		Ritual->BaseDamage = RITUAL_INFUSION_DAMAGE;
		Ritual->BaseEnergyCost = RITUAL_INFUSION_EP;
		Ritual->ExecutionType = EAbilityExecutionType::Melee;
		Ritual->bIsAttack = true;
		Ritual->ActivationDelay = RITUAL_INFUSION_DELAY_TURNS;
		Ritual->AddToRoot();
		PlainStrike = NewObject<UAbilityData>(GetTransientPackage());
		PlainStrike->Name = TEXT("Montage-less Plain Strike");
		PlainStrike->BaseDamage = 0; // only the ritual's fire moves the foe's HP
		PlainStrike->ExecutionType = EAbilityExecutionType::Melee;
		PlainStrike->bIsAttack = true;
		PlainStrike->AddToRoot();

		if (!TestNotNull(TEXT("Orchestrator"), Orchestrator) || !TestNotNull(TEXT("Executor"), Executor) || !TestNotNull(TEXT("Hero"), Hero) ||
			!TestNotNull(TEXT("Foe"), Foe))
		{
			return;
		}
		Orchestrator->StartCombat({Hero}, {Foe});
		Recorder = NewObject<UActionExecutedRecorder>();
		Recorder->AddToRoot();
		Recorder->Listen(Orchestrator);
		if (UCharacterDataComponent *Stats = Hero->FindComponentByClass<UCharacterDataComponent>())
		{
			Stats->MaxEP = Stats->CurrentEP = RITUAL_INFUSION_START_EP;
		}
	});

	AfterEach([this]()
	{
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		if (Recorder)
		{
			Recorder->RemoveFromRoot();
			Recorder = nullptr;
		}
		if (Ritual)
		{
			Ritual->RemoveFromRoot();
			Ritual = nullptr;
		}
		if (PlainStrike)
		{
			PlainStrike->RemoveFromRoot();
			PlainStrike = nullptr;
		}
		Orchestrator = nullptr;
		Executor = nullptr;
		Hero = Foe = nullptr;
		TestWorld.Reset();
		SetRitualInfusionCVar(TEXT("wor.Combat.InstantImpacts"), 0);
		SetRitualInfusionCVar(TEXT("wor.Battle.Seed"), 0);
	});

	It("locks the armed infusion into the fire, even with infusion off in between, and takes its EP once, at the arm", [this]()
	{
		if (!Recorder || !TestTrue(TEXT("Hero has the first turn"), Orchestrator->GetCurrentActor() == Hero)) return;
		UCharacterDataComponent *HeroStats = Hero->FindComponentByClass<UCharacterDataComponent>();
		const FAction Armed = MakeStrike(Ritual, EInfusionSourceOption::Innate, 1);
		const int32 InfusedCost = Executor->CalculateActionEnergyCost(Hero, Armed);
		TestTrue(TEXT("the infusion costs EP"), InfusedCost > Executor->CalculateActionEnergyCost(Hero, MakeStrike(Ritual, EInfusionSourceOption::None, 0)));

		const int32 FoeHPBefore = FoeHP();
		Orchestrator->SubmitActionAsync(Armed);
		TestWorld->Tick(RITUAL_INFUSION_SETTLE_FRAMES);
		TArray<FActionResult> Results = HeroResults();
		if (!TestEqual(TEXT("the arm is recorded"), Results.Num(), 1)) return;
		TestTrue(FString::Printf(TEXT("armed (%s)"), *Results[0].ErrorMessage), Results[0].bSuccess);
		TestTrue(TEXT("the arm's look is the infusion's Fire"), Results[0].AttackElement == ESpellElement::Fire);
		TestEqual(TEXT("the arm pays all the EP, infusion included"), Results[0].EnergySpent, InfusedCost);
		TestEqual(TEXT("the EP is spent at the arm"), HeroStats->CurrentEP, RITUAL_INFUSION_START_EP - InfusedCost);
		TestEqual(TEXT("no damage yet"), FoeHP(), FoeHPBefore);

		bool bPlayedPlain = false;
		PlayUntilFired(FoeHPBefore, bPlayedPlain);
		TestTrue(TEXT("Hero played an uninfused strike before the fire"), bPlayedPlain);
		TestTrue(TEXT("the ritual fired and hit"), FoeHP() < FoeHPBefore);
		Results = HeroResults();
		if (!TestTrue(TEXT("arm, plain strike and fire are recorded"), Results.Num() >= 3)) return;
		TestTrue(TEXT("the plain strike went out uninfused"), Results[1].AttackElement == ESpellElement::None);
		const FActionResult &Fire = Results.Last();
		TestTrue(FString::Printf(TEXT("the fire ran (%s)"), *Fire.ErrorMessage), Fire.bSuccess);
		TestTrue(TEXT("the fire strikes with the locked Fire"), Fire.AttackElement == ESpellElement::Fire);
		TestEqual(TEXT("the fire pays nothing"), Fire.EnergySpent, 0);
	});

	It("arms a Generic spell ritual with its source's element, not Generic or None", [this]()
	{
		if (!Recorder || !TestTrue(TEXT("Hero has the first turn"), Orchestrator->GetCurrentActor() == Hero)) return;
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Name = TEXT("Montage-less Ritual Spell");
		Spell->BaseDamage = RITUAL_INFUSION_DAMAGE;
		Spell->BaseEnergyCost = RITUAL_INFUSION_EP;
		Spell->ExecutionType = EAbilityExecutionType::Ranged;
		Spell->TargetType = ETargetType::Enemy;
		Spell->Element = ESpellElement::Generic; // inherit: takes the caster's source element
		Spell->ActivationDelay = RITUAL_INFUSION_DELAY_TURNS;
		FAction Action;
		Action.ActionType = EActionType::Spell;
		Action.SpellData = Spell;
		Action.SelectedSource = EInfusionSourceOption::Innate;
		Action.Targets.Add(Foe);
		Orchestrator->SubmitActionAsync(Action);
		TestWorld->Tick(RITUAL_INFUSION_SETTLE_FRAMES);
		const UActionExecutedRecorder::FEntry *Arm = Recorder->Entries.FindByPredicate(
			[this](const UActionExecutedRecorder::FEntry &Entry) { return Entry.Actor.Get() == Hero && Entry.ActionType == EActionType::Spell; });
		if (!TestNotNull(TEXT("the arm is recorded"), Arm)) return;
		TestTrue(FString::Printf(TEXT("armed (%s)"), *Arm->Result.ErrorMessage), Arm->bSuccess);
		TestTrue(TEXT("the arm's look is the Innate Fire"), Arm->Result.AttackElement == ESpellElement::Fire);
	});

	It("arms and fires clear with no infusion picked", [this]()
	{
		if (!Recorder || !TestTrue(TEXT("Hero has the first turn"), Orchestrator->GetCurrentActor() == Hero)) return;
		const int32 FoeHPBefore = FoeHP();
		Orchestrator->SubmitActionAsync(MakeStrike(Ritual, EInfusionSourceOption::None, 0));
		TestWorld->Tick(RITUAL_INFUSION_SETTLE_FRAMES);
		bool bPlayedPlain = false;
		PlayUntilFired(FoeHPBefore, bPlayedPlain);
		const TArray<FActionResult> Results = HeroResults();
		if (!TestTrue(TEXT("arm and fire are recorded"), Results.Num() >= 2)) return;
		TestTrue(TEXT("the arm is clear"), Results[0].AttackElement == ESpellElement::None);
		TestTrue(TEXT("the ritual fired and hit"), FoeHP() < FoeHPBefore);
		TestTrue(TEXT("the fire is clear"), Results.Last().AttackElement == ESpellElement::None);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
