// StatModifierSpec.cpp
// Transient buff/debuff reads of every sub-stat (B-020): a buff and its debuff move each
// stat getter by exactly (buff - debuff) percent, so the stat-modifier table can replace the
// hand-written pairs with the same numbers. Written against the hand-written pairs first;
// the table rows and the wor.CompareStats snapshot are checked at the end.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Debug/Character/StatModifierDebug.h"

namespace
{
	constexpr float STAT_TEST_BUFF_PERCENT = 20.0f;
	constexpr float STAT_TEST_DEBUFF_PERCENT = 5.0f;
	/** 1 + (20 - 5) / 100: what every multiplicative transient layer becomes. */
	constexpr float STAT_TEST_NET_FACTOR = 1.15f;
	/** ModifyStatusResist is additive in percent points: 15 points = +0.15. */
	constexpr float STAT_TEST_RESIST_MODIFY = 15.0f;
	constexpr float STAT_TEST_RESIST_DELTA = 0.15f;
	/** Below max investment, so no stat sits at its cap. */
	constexpr int32 STAT_TEST_PARTIAL_POINTS = 50;
	constexpr int32 STAT_TEST_DURATION_TURNS = 2;
	constexpr int32 STAT_TEST_FIRST_EFFECT_ID = 9201;
	constexpr float STAT_TEST_TOLERANCE = 1.e-4f;
}

BEGIN_DEFINE_SPEC(FStatModifierSpec, "WoR.Stats.Modifiers",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;
	UDamageCalculator *Calculator = nullptr;
	UCharacterData *Data = nullptr;
	int32 NextEffectID = STAT_TEST_FIRST_EFFECT_ID;

	AActor *Spawn()
	{
		return TestWorld->SpawnCombatant(Data);
	}

	static UCharacterDataComponent *CharOf(AActor *Actor)
	{
		return Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	}

	void Apply(AActor *Target, ESkillEffectType Type, float Value, ESpellElement Element = ESpellElement::None)
	{
		FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(TEXT("Stat test"), NextEffectID++, Type, Value, STAT_TEST_DURATION_TURNS);
		Effect.Element = Element;
		EffectManager->ApplyEffect(Target, Effect);
	}

	void ApplyPair(AActor *Target, ESkillEffectType Buff, ESkillEffectType Debuff)
	{
		Apply(Target, Buff, STAT_TEST_BUFF_PERCENT);
		Apply(Target, Debuff, STAT_TEST_DEBUFF_PERCENT);
	}

	/** Reads the stat bare, applies the pair, reads it again; the buffed value must be bare x 1.15. */
	void ExpectScaledByNet(const TCHAR *What, TFunctionRef<float(AActor *)> Read, ESkillEffectType Buff, ESkillEffectType Debuff)
	{
		AActor *Actor = Spawn();
		if (!TestNotNull(TEXT("Combatant"), CharOf(Actor)))
		{
			return;
		}
		const float Bare = Read(Actor);
		TestTrue(FString::Printf(TEXT("%s is above zero bare (%f)"), What, Bare), Bare > KINDA_SMALL_NUMBER);
		ApplyPair(Actor, Buff, Debuff);
		TestEqual(What, Read(Actor), Bare * STAT_TEST_NET_FACTOR, STAT_TEST_TOLERANCE);
	}
END_DEFINE_SPEC(FStatModifierSpec)

void FStatModifierSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		Calculator = TestWorld->GetSubsystem<UDamageCalculator>();
		TestNotNull(TEXT("SkillEffectManager"), EffectManager);
		TestNotNull(TEXT("DamageCalculator"), Calculator);
		Data = FWoRTestWorld::NewCharacterData();
		Data->Efficiency = STAT_TEST_PARTIAL_POINTS;
		Data->Luck = STAT_TEST_PARTIAL_POINTS;
		Data->Defense = STAT_TEST_PARTIAL_POINTS;
		Data->CritDamage = STAT_TEST_PARTIAL_POINTS;
		Data->Resistance = STAT_TEST_PARTIAL_POINTS;
		NextEffectID = STAT_TEST_FIRST_EFFECT_ID;
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
		EffectManager = nullptr;
		Calculator = nullptr;
	});

	Describe("A buff and its debuff scale the stat by their net", [this]()
	{
		It("Action Speed", [this]()
		{
			if (!EffectManager) return;
			ExpectScaledByNet(TEXT("GetEffectiveActionSpeed"), [](AActor *A) { return CharOf(A)->GetEffectiveActionSpeed(); },
							  ESkillEffectType::ActionSpeedBuff, ESkillEffectType::ActionSpeedDebuff);
		});

		It("Spell Speed", [this]()
		{
			if (!EffectManager) return;
			ExpectScaledByNet(TEXT("GetEffectiveSpellSpeed"), [](AActor *A) { return CharOf(A)->GetEffectiveSpellSpeed(); },
							  ESkillEffectType::SpellSpeedBuff, ESkillEffectType::SpellSpeedDebuff);
		});

		It("Spell Speed and Action Speed defense windows", [this]()
		{
			if (!EffectManager) return;
			ExpectScaledByNet(TEXT("SpeedWindowGearFactor(Spell)"), [](AActor *A) { return CharOf(A)->SpeedWindowGearFactor(EActionType::Spell); },
							  ESkillEffectType::SpellSpeedBuff, ESkillEffectType::SpellSpeedDebuff);
			ExpectScaledByNet(TEXT("SpeedWindowGearFactor(Ability)"), [](AActor *A) { return CharOf(A)->SpeedWindowGearFactor(EActionType::Ability); },
							  ESkillEffectType::ActionSpeedBuff, ESkillEffectType::ActionSpeedDebuff);
		});

		It("Spell Damage", [this]()
		{
			if (!EffectManager) return;
			ExpectScaledByNet(TEXT("GetTransientSpellDamageFactor"), [](AActor *A) { return CharOf(A)->GetTransientSpellDamageFactor(); },
							  ESkillEffectType::SpellDamageBuff, ESkillEffectType::SpellDamageDebuff);
		});

		It("Raw Damage", [this]()
		{
			if (!EffectManager) return;
			ExpectScaledByNet(TEXT("GetTransientRawDamageFactor"), [](AActor *A) { return CharOf(A)->GetTransientRawDamageFactor(); },
							  ESkillEffectType::RawDamageBuff, ESkillEffectType::RawDamageDebuff);
		});

		It("Luck", [this]()
		{
			if (!EffectManager) return;
			ExpectScaledByNet(TEXT("GetEquipmentModifiedLuck"), [](AActor *A) { return CharOf(A)->GetEquipmentModifiedLuck(); },
							  ESkillEffectType::LuckBuff, ESkillEffectType::LuckDebuff);
		});

		It("Reflex", [this]()
		{
			if (!EffectManager) return;
			ExpectScaledByNet(TEXT("ReflexWindowGearFactor"), [](AActor *A) { return CharOf(A)->ReflexWindowGearFactor(); },
							  ESkillEffectType::ReflexBuff, ESkillEffectType::ReflexDebuff);
		});

		It("Status Multiplier", [this]()
		{
			if (!EffectManager) return;
			ExpectScaledByNet(TEXT("GetEffectiveStatusMultiplier"), [](AActor *A) { return CharOf(A)->GetEffectiveStatusMultiplier(); },
							  ESkillEffectType::StatusMultiplierBuff, ESkillEffectType::StatusMultiplierDebuff);
		});

		It("Efficiency (scales the cost reduction, so 1 - multiplier)", [this]()
		{
			if (!EffectManager) return;
			ExpectScaledByNet(TEXT("1 - GetEffectiveEfficiencyMultiplier"), [](AActor *A) { return 1.0f - CharOf(A)->GetEffectiveEfficiencyMultiplier(); },
							  ESkillEffectType::EfficiencyBuff, ESkillEffectType::EfficiencyDebuff);
		});

		It("Defense", [this]()
		{
			if (!EffectManager || !Calculator) return;
			UDamageCalculator *Calc = Calculator;
			ExpectScaledByNet(TEXT("GetDefenderFlatDefense"), [Calc](AActor *A) { return Calc->GetDefenderFlatDefense(A); },
							  ESkillEffectType::DefenseBuff, ESkillEffectType::DefenseDebuff);
		});

		It("Crit Damage (Crit Damage Up is its buff)", [this]()
		{
			if (!EffectManager || !Calculator) return;
			UDamageCalculator *Calc = Calculator;
			ExpectScaledByNet(TEXT("GetCritDamageMultiplier"), [Calc](AActor *A) { return Calc->GetCritDamageMultiplier(A); },
							  ESkillEffectType::ModifyCritDamage, ESkillEffectType::CritDamageDebuff);
		});
	});

	Describe("Reads that are not a plain buff/debuff pair", [this]()
	{
		It("Resistance adds Status Resist Up only, not the element-matched Resistance Buff", [this]()
		{
			if (!EffectManager) return;
			AActor *Actor = Spawn();
			if (!TestNotNull(TEXT("Combatant"), CharOf(Actor))) return;
			const float Bare = CharOf(Actor)->GetEffectiveResistance();
			Apply(Actor, ESkillEffectType::ResistanceBuff, STAT_TEST_BUFF_PERCENT);
			TestEqual(TEXT("Resistance Buff ignored"), CharOf(Actor)->GetEffectiveResistance(), Bare, STAT_TEST_TOLERANCE);
			Apply(Actor, ESkillEffectType::ModifyStatusResist, STAT_TEST_RESIST_MODIFY);
			TestEqual(TEXT("Status Resist Up added"), CharOf(Actor)->GetEffectiveResistance(), Bare + STAT_TEST_RESIST_DELTA, STAT_TEST_TOLERANCE);
		});

		It("Status Multiplier ignores an element-keyed buff (that one applies at the buildup site)", [this]()
		{
			if (!EffectManager) return;
			AActor *Actor = Spawn();
			if (!TestNotNull(TEXT("Combatant"), CharOf(Actor))) return;
			const float Bare = CharOf(Actor)->GetEffectiveStatusMultiplier();
			Apply(Actor, ESkillEffectType::StatusMultiplierBuff, STAT_TEST_BUFF_PERCENT, ESpellElement::Fire);
			TestEqual(TEXT("Fire-keyed buff ignored"), CharOf(Actor)->GetEffectiveStatusMultiplier(), Bare, STAT_TEST_TOLERANCE);
		});

		It("Turn Speed nets Turn Speed Up + buff - debuff", [this]()
		{
			if (!EffectManager) return;
			AActor *Actor = Spawn();
			if (!TestNotNull(TEXT("Combatant"), Actor)) return;
			Apply(Actor, ESkillEffectType::ModifyTurnSpeed, STAT_TEST_RESIST_MODIFY);
			ApplyPair(Actor, ESkillEffectType::TurnSpeedBuff, ESkillEffectType::TurnSpeedDebuff);
			TestEqual(TEXT("GetNetStatModifier(TurnSpeed)"), EffectManager->GetNetStatModifier(Actor, ESubStat::TurnSpeed),
					  STAT_TEST_RESIST_MODIFY + STAT_TEST_BUFF_PERCENT - STAT_TEST_DEBUFF_PERCENT, STAT_TEST_TOLERANCE);
		});

		It("the None sub-stat has no channels", [this]()
		{
			if (!EffectManager) return;
			AActor *Actor = Spawn();
			if (!TestNotNull(TEXT("Combatant"), Actor)) return;
			ApplyPair(Actor, ESkillEffectType::LuckBuff, ESkillEffectType::LuckDebuff);
			TestEqual(TEXT("GetNetStatModifier(None)"), EffectManager->GetNetStatModifier(Actor, ESubStat::None), 0.0f);
		});

		It("a negative Crit Damage Up counts as zero", [this]()
		{
			if (!EffectManager || !Calculator) return;
			AActor *Actor = Spawn();
			if (!TestNotNull(TEXT("Combatant"), CharOf(Actor))) return;
			const float Bare = Calculator->GetCritDamageMultiplier(Actor);
			Apply(Actor, ESkillEffectType::ModifyCritDamage, -STAT_TEST_BUFF_PERCENT);
			TestEqual(TEXT("GetCritDamageMultiplier"), Calculator->GetCritDamageMultiplier(Actor), Bare, STAT_TEST_TOLERANCE);
		});
	});

	Describe("wor.CompareStats snapshot", [this]()
	{
		It("is identical when nothing changed and differs once a buff lands", [this]()
		{
			if (!EffectManager) return;
			AActor *Actor = Spawn();
			if (!TestNotNull(TEXT("Combatant"), Actor)) return;
			const FString First = UStatModifierDebug::GetStatsSnapshotString(Actor);
			TestTrue(TEXT("Snapshot lists Luck"), First.Contains(TEXT("Luck=")));
			TestEqual(TEXT("Same snapshot twice"), UStatModifierDebug::GetStatsSnapshotString(Actor), First);
			ApplyPair(Actor, ESkillEffectType::LuckBuff, ESkillEffectType::LuckDebuff);
			TestNotEqual(TEXT("Snapshot after a Luck buff"), UStatModifierDebug::GetStatsSnapshotString(Actor), First);
			TestTrue(TEXT("Modifier string names Luck"), UStatModifierDebug::GetStatModifierString(Actor).Contains(TEXT("Luck +15.0%")));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
