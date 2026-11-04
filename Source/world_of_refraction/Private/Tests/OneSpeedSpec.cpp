// OneSpeedSpec.cpp
// B-331 (D-123, Q-331): Action Speed and Spell Speed merge into one Speed. One Speed narrows the defender's window the
// same on a physical attack and a spell, and an old asset's Action Speed and Spell Speed points both count toward it.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/BarCapTriggerResolver.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 SPEED_TEST_OLD_ACTION_POINTS = 4;
	constexpr int32 SPEED_TEST_OLD_SPELL_POINTS = 6;
	constexpr int32 SPEED_TEST_MERGED_POINTS = 10;
	constexpr float SPEED_TEST_BUFF_PERCENT = 20.0f;
	constexpr float SPEED_TEST_BUFFED_FACTOR = 1.2f;
	constexpr int32 SPEED_TEST_EFFECT_ID = 9701;
	constexpr int32 SPEED_TEST_DURATION_TURNS = 3;
	constexpr float SPEED_TEST_TOLERANCE = 0.0001f;
	/** SB-22: one Water break slows the window by its 30%, once (not once per old speed). */
	constexpr float SPEED_TEST_WATER_SLOWED_FACTOR = 0.70f;
}

BEGIN_DEFINE_SPEC(FOneSpeedSpec, "WoR.Stats.OneSpeed", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	static UCharacterData *OldData()
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->ActionSpeed = SPEED_TEST_OLD_ACTION_POINTS;
		Data->SpellSpeed = SPEED_TEST_OLD_SPELL_POINTS;
		return Data;
	}

	static UCharacterData *NewData()
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->Speed = SPEED_TEST_MERGED_POINTS;
		return Data;
	}

	static UCharacterDataComponent *CharOf(AActor *Actor)
	{
		return Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	}
END_DEFINE_SPEC(FOneSpeedSpec)

void FOneSpeedSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("maps an old asset's Action Speed and Spell Speed onto Speed", [this]()
	{
		TestEqual(TEXT("4 + 6 old points read as 10 Speed"), OldData()->GetTotalSpeed(), SPEED_TEST_MERGED_POINTS);
		TestEqual(TEXT("a new asset's Speed reads as itself"), NewData()->GetTotalSpeed(), SPEED_TEST_MERGED_POINTS);
	});

	It("narrows a physical window and a spell window by the same Speed", [this]()
	{
		const UCharacterData *Data = NewData();
		const float Expected = FMath::Min(Data->GetEffectiveBody() * SPEED_TEST_MERGED_POINTS * CombatConstants::WINDOW_PER_SPEED_POINT,
										  CombatConstants::WINDOW_CAP_SECONDS);
		TestTrue(TEXT("Speed narrows the window"), Expected > 0.0f);
		TestEqual(TEXT("physical"), Data->CalculateSpeedWindowPenalty(EActionType::Ability), Expected, SPEED_TEST_TOLERANCE);
		TestEqual(TEXT("spell"), Data->CalculateSpeedWindowPenalty(EActionType::Spell), Expected, SPEED_TEST_TOLERANCE);
		TestEqual(TEXT("old data narrows it the same"), OldData()->CalculateSpeedWindowPenalty(EActionType::Spell), Expected, SPEED_TEST_TOLERANCE);
	});

	It("scales both windows with an old Spell Speed buff", [this]()
	{
		AActor *Attacker = TestWorld->SpawnCombatant(NewData());
		UCharacterDataComponent *Char = CharOf(Attacker);
		USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		if (!TestNotNull(TEXT("Attacker"), Char) || !TestNotNull(TEXT("SkillEffectManager"), Effects)) return;

		TestEqual(TEXT("bare: the same factor for both"), Char->SpeedWindowGearFactor(EActionType::Ability),
				  Char->SpeedWindowGearFactor(EActionType::Spell), SPEED_TEST_TOLERANCE);
		const float Bare = Char->SpeedWindowGearFactor(EActionType::Ability);

		FActiveSkillEffect Buff = FActiveSkillEffect::CreateBuff(TEXT("Speed test"), SPEED_TEST_EFFECT_ID, ESkillEffectType::SpellSpeedBuff,
																 SPEED_TEST_BUFF_PERCENT, SPEED_TEST_DURATION_TURNS);
		Effects->ApplyEffect(Attacker, Buff);
		TestEqual(TEXT("a physical attack gains the Spell Speed buff"), Char->SpeedWindowGearFactor(EActionType::Ability),
				  Bare * SPEED_TEST_BUFFED_FACTOR, SPEED_TEST_TOLERANCE);
		TestEqual(TEXT("a spell gains it the same"), Char->SpeedWindowGearFactor(EActionType::Spell),
				  Bare * SPEED_TEST_BUFFED_FACTOR, SPEED_TEST_TOLERANCE);
	});

	It("slows the window by a Water break's 30% once, not once per old speed", [this]()
	{
		AActor *Attacker = TestWorld->SpawnCombatant(NewData());
		UCharacterDataComponent *Char = CharOf(Attacker);
		USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		if (!TestNotNull(TEXT("Attacker"), Char) || !TestNotNull(TEXT("SkillEffectManager"), Effects)) return;
		TestNearlyEqual(TEXT("the rule's slow is 30%"), 1.0f - CombatConstants::WATER_BREAK_SPEED_SLOW_PERCENT / CombatConstants::STAT_PERCENT_DIVISOR,
						SPEED_TEST_WATER_SLOWED_FACTOR, SPEED_TEST_TOLERANCE);
		const float Bare = Char->SpeedWindowGearFactor(EActionType::Ability);

		const ESkillEffectType WaterBreak = BarCapTriggerResolver::ResolveTrigger(ESpellElement::Water, EPhysicalDamageType::None, false);
		Effects->ApplyTriggeredSkillEffect(TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData()), Attacker, WaterBreak, ESpellElement::Water);
		TestEqual(TEXT("x0.70, not x0.40"), Char->SpeedWindowGearFactor(EActionType::Ability), Bare * SPEED_TEST_WATER_SLOWED_FACTOR,
				  SPEED_TEST_TOLERANCE);
	});

	It("gives a defender the same window against a physical attack and a spell", [this]()
	{
		UDefenseSystem *Defense = TestWorld->GetSubsystem<UDefenseSystem>();
		AActor *Attacker = TestWorld->SpawnCombatant(OldData());
		AActor *Defender = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("DefenseSystem"), Defense) || !TestNotNull(TEXT("Attacker"), Attacker) || !TestNotNull(TEXT("Defender"), Defender)) return;
		TestEqual(TEXT("one window"), Defense->GetEffectiveDefenseInputWindow(Defender, Attacker, EActionType::Ability),
				  Defense->GetEffectiveDefenseInputWindow(Defender, Attacker, EActionType::Spell), SPEED_TEST_TOLERANCE);
	});
}

#endif
