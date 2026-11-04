// CombatInputRoutingSpec.cpp
// B-229: player inputs reach the server through ACombatPlayerController. On authority a pick runs the server-side
// handler straight away; the server refuses a pick for a character the sender doesn't control or that doesn't
// have the turn; a pick packs into FCombatPickRequest and back into the same FAction. (A real client's Server RPC
// is the 2-player PIE check, docs/PIE/B-229.md: a standalone test world is never a client.)

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/CombatPlayerController.h"
#include "Combat/Actions/CombatPickRequest.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Infusion/InfusionConstants.h"

namespace
{
	constexpr int32 INPUT_ROUTING_SPELL_LEVEL = 2;
	constexpr int32 INPUT_ROUTING_ABILITY_LEVEL = 1;
	/** Above InfusionConstants::MAX_INFUSION_LEVEL: unpacking clamps it. */
	constexpr int32 INPUT_ROUTING_OVER_MAX_LEVEL = 7;

	FAction InputRoutingAbilityOn(AActor *Target)
	{
		FAction Action;
		Action.ActionType = EActionType::Ability;
		Action.SkillData = NewObject<UAbilityData>(GetTransientPackage());
		Action.Targets.Add(Target);
		return Action;
	}
}

BEGIN_DEFINE_SPEC(FCombatInputRoutingSpec, "WoR.Coop.InputRouting",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	ACombatPlayerController *Controller = nullptr;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;

	ECombatInputVerdict LastVerdict() const
	{
		const TArray<FCombatInputRecord> &Inputs = Controller->GetRecentInputs();
		return Inputs.Num() > 0 ? Inputs.Last().Verdict : ECombatInputVerdict::NoActor;
	}
END_DEFINE_SPEC(FCombatInputRoutingSpec)

void FCombatInputRoutingSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		UWorld *World = TestWorld->GetWorld();
		Orchestrator = World ? World->SpawnActor<ACombatOrchestrator>() : nullptr;
		Controller = World ? World->SpawnActor<ACombatPlayerController>() : nullptr;
		TestNotNull(TEXT("Orchestrator"), Orchestrator);
		TestNotNull(TEXT("Controller"), Controller);
	});

	AfterEach([this]()
	{
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		Orchestrator = nullptr;
		Controller = nullptr;
		Hero = Foe = nullptr;
		TestWorld.Reset();
	});

	// A fight with a player controller in this headless world overflows the stack at combat start (engine frames only,
	// 2026-11-04), so the accepted pick and the not-their-turn refusal are PIE checks (docs/PIE/B-229.md); the
	// verdicts that come before the orchestrator is asked are pinned here.
	It("refuses a pick for a character the sender doesn't control", [this]()
	{
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Bot = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		if (!TestTrue(TEXT("combatants spawned"), Hero && Bot && Controller))
		{
			return;
		}
		TestFalse(TEXT("a bot-driven pawn is not controlled"), ACombatPlayerController::ControlsActor(Controller, Bot));

		const ECombatInputVerdict Verdict =
			Controller->HandleMenuPick(FCombatPickRequest::FromAction(Bot, InputRoutingAbilityOn(Hero)), true);
		TestTrue(TEXT("not controlled"), Verdict == ECombatInputVerdict::NotControlled);
		TestTrue(TEXT("recorded as a client's input"), Controller->GetRecentInputs().Num() > 0 && Controller->GetRecentInputs().Last().bFromClient);
	});

	It("refuses a pick while no fight is running", [this]()
	{
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("combatants spawned"), Hero && Foe && Controller))
		{
			return;
		}
		const ECombatInputVerdict Verdict =
			Controller->HandleMenuPick(FCombatPickRequest::FromAction(Hero, InputRoutingAbilityOn(Foe)), true);
		TestTrue(TEXT("no combat"), Verdict == ECombatInputVerdict::NoCombat);
		TestFalse(TEXT("nothing runs"), Orchestrator->IsWaitingForAsyncAction());
	});

	It("records a defense press with no open window as nothing to defend", [this]()
	{
		Controller->RequestDefenseInput(EDefenseType::Block, EDefenseDirection::None);
		TestTrue(TEXT("no defender"), LastVerdict() == ECombatInputVerdict::NoActor);
		TestFalse(TEXT("handled here"), Controller->GetRecentInputs().Num() > 0 && Controller->GetRecentInputs().Last().bFromClient);
	});

	It("packs a spell pick and unpacks the same FAction", [this]()
	{
		AActor *Caster = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Target = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		FAction Action;
		Action.ActionType = EActionType::Spell;
		Action.SpellData = NewObject<USpellData>(GetTransientPackage());
		Action.SpellSource = ESpellSource::RingCrystal;
		Action.SelectedSource = EInfusionSourceOption::WeaponCrystal;
		Action.SpellInfusionLevel = INPUT_ROUTING_SPELL_LEVEL;
		Action.Targets.Add(Target);

		const FCombatPickRequest Pick = FCombatPickRequest::FromAction(Caster, Action);
		TestEqual(TEXT("actor"), Pick.Actor.Get(), Caster);
		TestTrue(TEXT("valid enums"), Pick.HasValidEnums());

		const FAction Back = Pick.ToAction();
		TestTrue(TEXT("type"), Back.ActionType == Action.ActionType);
		TestEqual(TEXT("spell"), Back.SpellData, Action.SpellData);
		TestTrue(TEXT("spell source"), Back.SpellSource == Action.SpellSource);
		TestTrue(TEXT("infusion source"), Back.SelectedSource == Action.SelectedSource);
		TestEqual(TEXT("spell level"), Back.SpellInfusionLevel, Action.SpellInfusionLevel);
		TestEqual(TEXT("no ability level"), Back.AbilityInfusionLevel, 0);
		if (TestEqual(TEXT("target count"), Back.Targets.Num(), 1))
		{
			TestEqual(TEXT("target"), Back.Targets[0], Target);
		}
	});

	It("packs an ability and an item pick and unpacks the same FAction", [this]()
	{
		AActor *Target = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		FAction Ability = InputRoutingAbilityOn(Target);
		Ability.SelectedSource = EInfusionSourceOption::WeaponCrystal;
		Ability.AbilityInfusionLevel = INPUT_ROUTING_ABILITY_LEVEL;
		const FAction AbilityBack = FCombatPickRequest::FromAction(nullptr, Ability).ToAction();
		TestEqual(TEXT("skill"), AbilityBack.SkillData, Ability.SkillData);
		TestEqual(TEXT("ability level"), AbilityBack.AbilityInfusionLevel, Ability.AbilityInfusionLevel);
		TestEqual(TEXT("no spell level"), AbilityBack.SpellInfusionLevel, 0);

		FAction Item;
		Item.ActionType = EActionType::Item;
		Item.ItemData = FCrystalId(ECrystalType::Garnet, EItemTier::F_Tier);
		Item.Targets.Add(Target);
		const FAction ItemBack = FCombatPickRequest::FromAction(nullptr, Item).ToAction();
		TestTrue(TEXT("item type"), ItemBack.ActionType == EActionType::Item);
		TestTrue(TEXT("item id"), ItemBack.ItemData == Item.ItemData);
		TestTrue(TEXT("an item carries no infusion"), ItemBack.SelectedSource == EInfusionSourceOption::None);
	});

	It("clamps an infusion level above the maximum", [this]()
	{
		FCombatPickRequest Pick;
		Pick.ActionType = EActionType::Spell;
		Pick.InfusionLevel = INPUT_ROUTING_OVER_MAX_LEVEL;
		TestEqual(TEXT("clamped"), Pick.ToAction().SpellInfusionLevel, InfusionConstants::MAX_INFUSION_LEVEL);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
