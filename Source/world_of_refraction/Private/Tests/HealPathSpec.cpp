// HealPathSpec.cpp
// B-247 (DRY audit A6): one heal boost and HealBlock gate. HealActor amplifies by the recipient's healing
// received exactly as the HOT tick does and is refused while the recipient is HealBlocked. (Its
// UActionExecutor::OnHealingDone broadcast is a dynamic delegate a spec can't bind a lambda to.)

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/TickMath.h"

namespace
{
	constexpr int32 HEAL_TEST_AMOUNT = 20;
	constexpr float HEAL_TEST_BOOST_PERCENT = 25.0f;
	constexpr int32 HEAL_TEST_DAMAGE = 60;
	constexpr int32 HEAL_TEST_EFFECT_ID = 9501;
	constexpr int32 HEAL_TEST_TURNS = 3;
}

BEGIN_DEFINE_SPEC(FHealPathSpec, "WoR.Effects.HealPath", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *Effects = nullptr;
	UActionExecutor *Executor = nullptr;
	AActor *Hero = nullptr;
	UCharacterDataComponent *Char = nullptr;
END_DEFINE_SPEC(FHealPathSpec)

void FHealPathSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		Executor = TestWorld->GetSubsystem<UActionExecutor>();
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Char = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("Effects"), Effects) || !TestNotNull(TEXT("Executor"), Executor) || !TestNotNull(TEXT("Char"), Char))
		{
			Effects = nullptr; // each case returns early
			return;
		}
		Char->ServerTakeDamage(HEAL_TEST_DAMAGE);
	});

	AfterEach([this]() { TestWorld.Reset(); });

	It("boosts a heal by healing received, as the HOT tick does", [this]()
	{
		if (!Effects) return;
		Effects->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Heal boost"), HEAL_TEST_EFFECT_ID, ESkillEffectType::HealingReceivedBuff,
																 HEAL_TEST_BOOST_PERCENT, HEAL_TEST_TURNS), Hero);
		FTickMathInputs Tick;
		Tick.Value = HEAL_TEST_AMOUNT;
		Tick.HealingReceivedPercent = Effects->GetTotalStatModifier(Hero, ESkillEffectType::HealingReceivedBuff);
		const int32 HPBefore = Char->CurrentHP;
		const int32 Healed = Effects->HealActor(Hero, HEAL_TEST_AMOUNT, Hero);
		TestEqual(TEXT("same boost as a HOT tick"), Healed, WoRTickMath::GetHotTickHeal(Tick));
		TestTrue(TEXT("boosted"), Healed > HEAL_TEST_AMOUNT);
		TestEqual(TEXT("HP went up by it"), Char->CurrentHP, HPBefore + Healed);
	});

	It("is refused while HealBlocked", [this]()
	{
		if (!Effects) return;
		Effects->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Heal block"), HEAL_TEST_EFFECT_ID, ESkillEffectType::HealBlock, 0.0f,
																 HEAL_TEST_TURNS), Hero);
		const int32 HPBefore = Char->CurrentHP;
		TestEqual(TEXT("nothing healed"), Effects->HealActor(Hero, HEAL_TEST_AMOUNT, Hero), 0);
		TestEqual(TEXT("HP unchanged"), Char->CurrentHP, HPBefore);
	});
}

#endif
