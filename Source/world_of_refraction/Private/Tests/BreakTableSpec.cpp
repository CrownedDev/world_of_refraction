// BreakTableSpec.cpp
// B-274 (S8): every damage type's break comes from one row, the rows carry the named constants, and a
// capped bar applies what its row says.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"
#include "Combat/Elements/ElementTable.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/BreakTable.h"
#include "Skills/Effects/SkillEffectManager.h"

BEGIN_DEFINE_SPEC(FBreakTableSpec, "WoR.Status.BreakTable",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *Effects = nullptr;
	AActor *Holder = nullptr;

	/** EffectValue's answer when the holder has no effect of the type. */
	static constexpr float NO_EFFECT = -1.0f;

	float EffectValue(ESkillEffectType Type) const
	{
		const TArray<FActiveSkillEffect> Found = Effects->GetEffectsByType(Holder, Type);
		return Found.Num() > 0 ? Found[0].EffectValue : NO_EFFECT;
	}
END_DEFINE_SPEC(FBreakTableSpec)

void FBreakTableSpec::Define()
{
	It("has a row for every damage type's break and BD's drain", [this]()
	{
		for (const FElementRow &Row : ElementTable::Rows())
		{
			if (Row.BreakTrigger != ESkillEffectType::None)
			{
				TestNotNull(*FString::Printf(TEXT("break row for type %d"), static_cast<int32>(Row.Type)), BreakTable::For(Row.BreakTrigger));
			}
		}
		TestNotNull(TEXT("BD drain"), BreakTable::For(ESkillEffectType::DrainEnergy));
		TestNull(TEXT("no row for a non-break"), BreakTable::For(ESkillEffectType::None));
	});

	It("carries the named sizes", [this]()
	{
		TestEqual(TEXT("Burn"), BreakTable::For(ESkillEffectType::DOT)->Size, CombatConstants::BURN_BREAK_MAX_HP_FRACTION);
		TestEqual(TEXT("Earth"), BreakTable::For(ESkillEffectType::DefenseDebuff)->Size, CombatConstants::EARTH_BREAK_DEFENSE_DOWN_PERCENT);
		TestEqual(TEXT("Reality"), BreakTable::For(ESkillEffectType::BurstDamage)->Size, CombatConstants::REALITY_BREAK_MAX_HP_FRACTION);
		// B-170h2b (Q-170e): Wind takes an action at the next turn instead of a place; Water slows Spell Speed only.
		TestEqual(TEXT("Wind's action next turn"), BreakTable::For(ESkillEffectType::SkipTurn)->ActionsLostNextTurn, CombatConstants::WIND_BREAK_ACTIONS_LOST);
		TestEqual(TEXT("Water slows spells"), BreakTable::For(ESkillEffectType::ActionSpeedDebuff)->Applied, ESkillEffectType::SpellSpeedDebuff);
		TestEqual(TEXT("Water has no second effect"), BreakTable::For(ESkillEffectType::ActionSpeedDebuff)->AlsoApplied, ESkillEffectType::None);
		TestFalse(TEXT("printable"), BreakTable::GetBreakTableString().IsEmpty());
	});

	Describe("a capped bar", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("test world created"), TestWorld->Create());
			Effects = TestWorld->GetSubsystem<USkillEffectManager>();
			Holder = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		});

		AfterEach([this]()
		{
			Effects = nullptr;
			Holder = nullptr;
			TestWorld.Reset();
		});

		It("applies its row's size and second effect", [this]()
		{
			if (!TestNotNull(TEXT("SkillEffectManager"), Effects) || !TestNotNull(TEXT("Holder"), Holder)) return;
			const UCharacterDataComponent *HolderComp = Holder->FindComponentByClass<UCharacterDataComponent>();
			if (!TestNotNull(TEXT("CharacterDataComponent"), HolderComp)) return;

			Effects->ApplyTriggeredSkillEffect(nullptr, Holder, ESkillEffectType::DefenseDebuff, ESpellElement::Earth);
			TestEqual(TEXT("Earth: defense down"), EffectValue(ESkillEffectType::DefenseDebuff), CombatConstants::EARTH_BREAK_DEFENSE_DOWN_PERCENT);

			Effects->ApplyTriggeredSkillEffect(nullptr, Holder, ESkillEffectType::DOT, ESpellElement::Fire);
			TestEqual(TEXT("Fire: burn per payout"), EffectValue(ESkillEffectType::DOT), HolderComp->MaxHP * CombatConstants::BURN_BREAK_MAX_HP_FRACTION);

			Effects->ApplyTriggeredSkillEffect(nullptr, Holder, ESkillEffectType::ActionSpeedDebuff, ESpellElement::Water);
			TestEqual(TEXT("Water: spell speed"), EffectValue(ESkillEffectType::SpellSpeedDebuff), CombatConstants::WATER_BREAK_SPEED_SLOW_PERCENT);
			TestEqual(TEXT("Water: no action speed slow (B-170d, SB-22)"), EffectValue(ESkillEffectType::ActionSpeedDebuff), NO_EFFECT);
		});
	});
}

#endif
