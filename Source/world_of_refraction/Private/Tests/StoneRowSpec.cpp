// StoneRowSpec.cpp
// Every stat stone, at every tier, used on an ally and on an enemy (B-021): which item
// effect it maps to, which sub-stat it amplifies when attached, its base percent, and the
// exact effect its consumable applies. Written against the hand-written switches and
// Execute*BuffEffect wrappers first, so the stone row table must give the same results.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/CombatConstants.h"
#include "Debug/Equipment/StoneRowDebug.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Inventory/ItemExecutor.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	struct FStoneExpectation
	{
		ECrystalType Type;
		EItemEffectType ItemEffect;
		ESubStat Stat;
		ESkillEffectType AllyEffect;
		ESkillEffectType EnemyEffect;
		const TCHAR *Label;
		/** ResistanceStone encodes direction by sign on one effect type. */
		bool bEnemyMagnitudeNegative;
	};

	using SE = ESkillEffectType;
	using IE = EItemEffectType;

	const FStoneExpectation STONE_EXPECTATIONS[] = {
		{ECrystalType::DamageStone, IE::BuffRawDamage, ESubStat::RawDamage, SE::RawDamageBuff, SE::RawDamageDebuff, TEXT("Raw Damage"), false},
		{ECrystalType::DefenseStone, IE::BuffDefense, ESubStat::Defense, SE::DefenseBuff, SE::DefenseDebuff, TEXT("Defense"), false},
		{ECrystalType::CritStone, IE::BuffCritDamage, ESubStat::CritDamage, SE::ModifyCritDamage, SE::CritDamageDebuff, TEXT("Crit Damage"), false},
		{ECrystalType::TurnSpeedStone, IE::BuffTurnSpeed, ESubStat::TurnSpeed, SE::TurnSpeedBuff, SE::TurnSpeedDebuff, TEXT("Turn Speed"), false},
		{ECrystalType::StatusStone, IE::BuffStatusMultiplier, ESubStat::StatusMultiplier, SE::StatusMultiplierBuff, SE::StatusMultiplierDebuff, TEXT("Status Multiplier"), false},
		{ECrystalType::EfficiencyStone, IE::BuffEfficiency, ESubStat::Efficiency, SE::EfficiencyBuff, SE::EfficiencyDebuff, TEXT("Efficiency"), false},
		{ECrystalType::MaxHPStone, IE::BuffMaxHP, ESubStat::None, SE::MaxHPBuff, SE::MaxHPDebuff, TEXT("Max HP"), false},
		{ECrystalType::MaxEPStone, IE::BuffMaxEP, ESubStat::None, SE::MaxEnergyBuff, SE::MaxEnergyDebuff, TEXT("Max EP"), false},
		{ECrystalType::SpellDamageStone, IE::BuffSpellDamage, ESubStat::SpellDamage, SE::SpellDamageBuff, SE::SpellDamageDebuff, TEXT("Spell Damage"), false},
		{ECrystalType::ResistanceStone, IE::BuffResistance, ESubStat::Resistance, SE::ModifyStatusResist, SE::ModifyStatusResist, TEXT("Status Resistance"), true},
		{ECrystalType::SpellSpeedStone, IE::BuffSpellSpeed, ESubStat::SpellSpeed, SE::SpellSpeedBuff, SE::SpellSpeedDebuff, TEXT("Spell Speed"), false},
		{ECrystalType::ActionSpeedStone, IE::BuffActionSpeed, ESubStat::ActionSpeed, SE::ActionSpeedBuff, SE::ActionSpeedDebuff, TEXT("Action Speed"), false},
		{ECrystalType::LuckStone, IE::BuffLuck, ESubStat::Luck, SE::LuckBuff, SE::LuckDebuff, TEXT("Luck"), false},
		{ECrystalType::ReflexStone, IE::BuffReflex, ESubStat::Reflex, SE::ReflexBuff, SE::ReflexDebuff, TEXT("Reflex"), false},
	};

	/** Stones that are not stat stones: no stat, no base percent. */
	const ECrystalType NON_STAT_STONES[] = {ECrystalType::AbilityStone, ECrystalType::DurabilityStone, ECrystalType::HealingStone};

	const EItemTier STONE_TEST_TIERS[] = {EItemTier::F_Tier, EItemTier::E_Tier, EItemTier::D_Tier, EItemTier::C_Tier,
										  EItemTier::B_Tier, EItemTier::A_Tier, EItemTier::S_Tier};
	constexpr float STONE_TEST_TOLERANCE = 1.e-4f;
}

BEGIN_DEFINE_SPEC(FStoneRowSpec, "WoR.Items.Stones",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;
	UItemExecutor *Executor = nullptr;

	/** Uses the stone on Target and checks the single effect it leaves there. */
	void ExpectUse(const FStoneExpectation &Row, EItemTier Tier, bool bOnAlly)
	{
		AActor *User = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		// No combat teams in the test world, so any other actor counts as an enemy.
		AActor *Target = bOnAlly ? User : TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		const FCrystalId Id(Row.Type, Tier);
		const FString Where = FString::Printf(TEXT("%s %s on %s"), *ItemIdentity::GetDisplayName(Id),
											  *UEnum::GetValueAsString(Tier), bOnAlly ? TEXT("ally") : TEXT("enemy"));

		const FItemUseResult Result = Executor->UseItem(User, Id, Target);
		TestTrue(Where + TEXT(": used"), Result.bSuccess);
		TestEqual(Where + TEXT(": buffs applied"), Result.BuffsApplied, 1);

		const TArray<FActiveSkillEffect> Effects = EffectManager->GetActiveEffects(Target);
		if (!TestEqual(Where + TEXT(": one effect on the target"), Effects.Num(), 1))
		{
			return;
		}
		const FActiveSkillEffect &Effect = Effects[0];
		const float BasePercent = CrystalEffectTable::GetStoneBasePercent(Row.Type, Tier);
		const float ExpectedMagnitude = (!bOnAlly && Row.bEnemyMagnitudeNegative) ? -BasePercent : BasePercent;
		TestEqual(Where + TEXT(": effect type"), Effect.EffectType, bOnAlly ? Row.AllyEffect : Row.EnemyEffect);
		TestEqual(Where + TEXT(": magnitude"), Effect.EffectValue, ExpectedMagnitude, STONE_TEST_TOLERANCE);
		TestEqual(Where + TEXT(": duration"), Effect.RemainingTurns, CombatConstants::AUGMENT_STONE_CONSUMABLE_DURATION);
		TestEqual(Where + TEXT(": name"), Effect.EffectName, FString::Printf(TEXT("%s %s"), *ItemIdentity::GetDisplayName(Id), Row.Label));
		TestEqual(Where + TEXT(": source id"), Effect.EffectID, ItemIdentity::GetEffectSourceID(Id));
		TestEqual(Where + TEXT(": element"), Effect.Element, ItemIdentity::GetElement(Id));
	}
END_DEFINE_SPEC(FStoneRowSpec)

void FStoneRowSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		Executor = TestWorld->GetSubsystem<UItemExecutor>();
		TestNotNull(TEXT("SkillEffectManager"), EffectManager);
		TestNotNull(TEXT("ItemExecutor"), Executor);
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
		EffectManager = nullptr;
		Executor = nullptr;
	});

	It("maps every stat stone to its item effect and attached sub-stat", [this]()
	{
		for (const FStoneExpectation &Row : STONE_EXPECTATIONS)
		{
			const FString Name = UEnum::GetValueAsString(Row.Type);
			TestEqual(Name + TEXT(": item effect"), ItemIdentity::GetItemEffectType(FCrystalId(Row.Type, EItemTier::F_Tier)), Row.ItemEffect);
			TestEqual(Name + TEXT(": attached stat"), CrystalEffectTable::StoneTargetStat(Row.Type), Row.Stat);
		}
	});

	It("gives every stat stone the shared base-percent curve, and other stones none", [this]()
	{
		for (int32 TierIndex = 0; TierIndex < UE_ARRAY_COUNT(STONE_TEST_TIERS); ++TierIndex)
		{
			const float Curve = AugmentStoneConstants::STONE_BASE_PERCENT[TierIndex];
			for (const FStoneExpectation &Row : STONE_EXPECTATIONS)
			{
				TestEqual(UEnum::GetValueAsString(Row.Type) + TEXT(": base percent"),
						  CrystalEffectTable::GetStoneBasePercent(Row.Type, STONE_TEST_TIERS[TierIndex]), Curve, STONE_TEST_TOLERANCE);
			}
			for (const ECrystalType Type : NON_STAT_STONES)
			{
				TestEqual(UEnum::GetValueAsString(Type) + TEXT(": base percent"),
						  CrystalEffectTable::GetStoneBasePercent(Type, STONE_TEST_TIERS[TierIndex]), 0.0f);
			}
		}
		for (const ECrystalType Type : NON_STAT_STONES)
		{
			TestEqual(UEnum::GetValueAsString(Type) + TEXT(": attached stat"), CrystalEffectTable::StoneTargetStat(Type), ESubStat::None);
		}
	});

	It("applies each stone's buff to an ally and its debuff to an enemy, at every tier", [this]()
	{
		if (!Executor || !EffectManager) return;
		for (const FStoneExpectation &Row : STONE_EXPECTATIONS)
		{
			for (const EItemTier Tier : STONE_TEST_TIERS)
			{
				ExpectUse(Row, Tier, true);
				ExpectUse(Row, Tier, false);
			}
		}
	});

	It("prints one wor.PrintStoneRows line per stat stone", [this]()
	{
		TArray<FString> Lines;
		UStoneRowDebug::GetStoneRowsString().ParseIntoArrayLines(Lines);
		TestEqual(TEXT("Lines"), Lines.Num(), static_cast<int32>(UE_ARRAY_COUNT(STONE_EXPECTATIONS)));
		TestTrue(TEXT("ResistanceStone flagged"), UStoneRowDebug::GetStoneRowsString().Contains(TEXT("ResistanceStone: stat Resistance")));
	});

	It("still routes Amber through the defense buff with its own magnitude and duration", [this]()
	{
		if (!Executor || !EffectManager) return;
		AActor *User = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		const FCrystalId Amber(ECrystalType::Amber, EItemTier::C_Tier);
		TestTrue(TEXT("Amber used"), Executor->UseItem(User, Amber, User).bSuccess);
		const TArray<FActiveSkillEffect> Effects = EffectManager->GetActiveEffects(User);
		if (!TestEqual(TEXT("One effect"), Effects.Num(), 1)) return;
		TestEqual(TEXT("Amber type"), Effects[0].EffectType, ESkillEffectType::DefenseBuff);
		TestEqual(TEXT("Amber magnitude"), Effects[0].EffectValue, CrystalEffectTable::GetAmberDefensePercent(Amber), STONE_TEST_TOLERANCE);
		TestEqual(TEXT("Amber duration"), Effects[0].RemainingTurns, CrystalEffectTable::GetCrystalDuration(Amber));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
