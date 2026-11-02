// AIInfusionLevelSpec.cpp
// B-241 (DRY audit A4): one infusion-level decision for spells and abilities. With the same damage estimates a
// spell and an ability pick the same level; a combatant with Max EP 0 reads as no energy instead of dividing
// by zero.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AI/AIDecisionManager.h"
#include "Character/CharacterDataComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	/** Far above any test HP: a hit that size kills. */
	constexpr int32 INFUSION_TEST_KILLING_DAMAGE = 100000;
	constexpr int32 INFUSION_TEST_WEAK_DAMAGE = 1;
}

BEGIN_DEFINE_SPEC(FAIInfusionLevelSpec, "WoR.AI.InfusionLevel", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UAIDecisionManager *AI = nullptr;
	AActor *Attacker = nullptr;
	AActor *Target = nullptr;

	int32 Decide(const USkillDataBase *Skill, int32 L0Damage, int32 L2Damage) const
	{
		return AI->DecideInfusionLevel(Attacker, Target, Skill, EInfusionSourceOption::None,
									   [L0Damage, L2Damage](int32 Level) { return Level == 0 ? L0Damage : L2Damage; });
	}
END_DEFINE_SPEC(FAIInfusionLevelSpec)

void FAIInfusionLevelSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		AI = TestWorld->GetSubsystem<UAIDecisionManager>();
		Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		Target = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("AI"), AI) || !TestNotNull(TEXT("Attacker"), Attacker) || !TestNotNull(TEXT("Target"), Target))
		{
			AI = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		AI = nullptr;
		TestWorld.Reset();
	});

	It("picks the same level for a spell and an ability with the same estimates", [this]()
	{
		if (!AI) return;
		const USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		const UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		TestEqual(TEXT("L0 kills: same"), Decide(Spell, INFUSION_TEST_KILLING_DAMAGE, INFUSION_TEST_KILLING_DAMAGE),
				  Decide(Ability, INFUSION_TEST_KILLING_DAMAGE, INFUSION_TEST_KILLING_DAMAGE));
		TestEqual(TEXT("only L2 kills: same"), Decide(Spell, INFUSION_TEST_WEAK_DAMAGE, INFUSION_TEST_KILLING_DAMAGE),
				  Decide(Ability, INFUSION_TEST_WEAK_DAMAGE, INFUSION_TEST_KILLING_DAMAGE));
		TestEqual(TEXT("nothing kills: same"), Decide(Spell, INFUSION_TEST_WEAK_DAMAGE, INFUSION_TEST_WEAK_DAMAGE),
				  Decide(Ability, INFUSION_TEST_WEAK_DAMAGE, INFUSION_TEST_WEAK_DAMAGE));
		TestEqual(TEXT("L0 kills: no infusion"), Decide(Spell, INFUSION_TEST_KILLING_DAMAGE, INFUSION_TEST_KILLING_DAMAGE), 0);
		// No combat set: Medium difficulty, and a fresh attacker has full EP.
		TestEqual(TEXT("only L2 kills: level 2"), Decide(Spell, INFUSION_TEST_WEAK_DAMAGE, INFUSION_TEST_KILLING_DAMAGE), 2);
	});

	It("reads Max EP 0 as no energy", [this]()
	{
		if (!AI) return;
		UCharacterDataComponent *Char = Attacker->FindComponentByClass<UCharacterDataComponent>();
		if (!TestNotNull(TEXT("Character data"), Char)) return;
		Char->MaxEP = 0;
		Char->CurrentEP = 0;
		TestEqual(TEXT("fraction 0"), AI->GetEnergyFraction(Attacker), 0.0f);
		const UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		TestEqual(TEXT("no energy: no L2 even if it would kill"), Decide(Ability, INFUSION_TEST_WEAK_DAMAGE, INFUSION_TEST_KILLING_DAMAGE),
				  0);
	});
}

#endif
