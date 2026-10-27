// TickExecutionSpec.cpp
// GAS stage 3 (B-042n): a DOT's and a HOT's tick on GAS (UWoRTickExecution through the turn tracker) pay what
// the enum path's tick pays: a DOT through the holder's element resistance read at each tick (none for a share
// of max HP), a HOT with the holder's HealingReceivedBuff, both rounded as USkillEffectManager rounds them.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterData.h"
#include "Combat/Resistance/ResistanceQuery.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "GAS/WoREffectSpecBuilder.h"
#include "GAS/WoRTurnEffectTracker.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/TickMath.h"

namespace
{
	constexpr int32 TICK_TEST_TURNS = 3;
	constexpr int32 TICK_TEST_DOT_ID = 4851;
	constexpr int32 TICK_TEST_HOT_ID = 4852;
	constexpr int32 TICK_TEST_BOOST_ID = 4853;
	constexpr int32 TICK_TEST_RESIST_ID = 4854;
	constexpr int32 TICK_TEST_SOURCE = 31;
	/** Values that leave a fraction before the round. */
	constexpr float TICK_TEST_DOT = 13.0f;
	constexpr float TICK_TEST_HOT = 7.0f;
	constexpr float TICK_TEST_SHARE_DOT = 9.6f;
	constexpr float TICK_TEST_HEALING_RECEIVED = 25.0f;
	constexpr float TICK_TEST_FIRE_RESIST_BUFF = 30.0f;
	constexpr float TICK_TEST_GEAR_FIRE = 6.0f;
	constexpr float TICK_TEST_START_HEALTH = 50.0f;
	/** 7 x 1.25 = 8.75, rounded. */
	constexpr int32 TICK_TEST_EXPECTED_HEAL = 9;
}

BEGIN_DEFINE_SPEC(FTickExecutionSpec, "WoR.GAS.TickExecution",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AActor *Hero = nullptr;
	UAbilitySystemComponent *ASC = nullptr;
	UWoRAttributeSyncComponent *Sync = nullptr;
	UWoRTurnEffectTracker *Tracker = nullptr;

	float Health() const { return ASC->GetNumericAttribute(UWoRAttributeSet_Vitals::GetHealthAttribute()); }

	static FActiveSkillEffect MakeTickEffect(ESkillEffectType Type, int32 ID, float Value, ESpellElement Element)
	{
		FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(TEXT("Tick test"), ID, Type, Value, TICK_TEST_TURNS);
		Effect.Element = Element;
		Effect.SourceKey = TICK_TEST_SOURCE;
		return Effect;
	}

	/** The Health one holder turn start's ticks change on GAS. */
	float PayTicks()
	{
		ASC->SetNumericAttributeBase(UWoRAttributeSet_Vitals::GetHealthAttribute(), TICK_TEST_START_HEALTH);
		Tracker->OnHolderTurnStart();
		const float Change = Health() - TICK_TEST_START_HEALTH;
		Tracker->OnHolderTurnEnd();
		return Change;
	}
END_DEFINE_SPEC(FTickExecutionSpec)

void FTickExecutionSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		// A Fire Caster (class table) with a Fire gear roll: Fire resistance in play.
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Caster;
		Data->InnateElement = ESpellElement::Fire;
		const ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, NewObject<UWeaponData>(GetTransientPackage()));
		Hero = Loadout ? Loadout->GetOwner() : nullptr;
		UInventoryComponent *Inventory = Hero ? Hero->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (Inventory && Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
		{
			Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].PrimaryWeapon.WeaponEntry.ResistanceBonus.Fire = TICK_TEST_GEAR_FIRE;
			ASC = FWoRTestWorld::AddComponent<UAbilitySystemComponent>(Hero);
			Sync = FWoRTestWorld::AddComponent<UWoRAttributeSyncComponent>(Hero);
			Tracker = FWoRTestWorld::AddComponent<UWoRTurnEffectTracker>(Hero);
		}
		if (!TestNotNull(TEXT("ASC"), ASC) || !TestNotNull(TEXT("Sync"), Sync) || !TestNotNull(TEXT("Tracker"), Tracker) ||
			!TestNotNull(TEXT("Skill effect manager"), TestWorld->GetSubsystem<USkillEffectManager>()))
		{
			ASC = nullptr; // each case returns early
			return;
		}
		Sync->SyncAttributes();
	});

	AfterEach([this]()
	{
		Hero = nullptr;
		ASC = nullptr;
		Sync = nullptr;
		Tracker = nullptr;
		TestWorld.Reset();
	});

	It("pays a Fire DOT tick through the holder's Fire resistance, as the enum path does", [this]()
	{
		if (!ASC) return;
		const FActiveSkillEffect Dot = MakeTickEffect(ESkillEffectType::DOT, TICK_TEST_DOT_ID, TICK_TEST_DOT, ESpellElement::Fire);
		TestTrue(TEXT("Fire resistance in play"), !FMath::IsNearlyZero(WoRResistance::GetElementDamageResistance(Hero, ESpellElement::Fire)));
		const int32 EnumTick = USkillEffectManager::GetDotTickDamage(Hero, Dot);
		TestNotEqual(TEXT("Resistance moves the tick"), EnumTick, FMath::RoundToInt(TICK_TEST_DOT));
		TestTrue(TEXT("DOT applied on GAS"), FWoREffectSpecBuilder::Apply(Hero, Dot, Hero).IsValid());
		TestEqual(TEXT("GAS tick = enum tick"), PayTicks(), -static_cast<float>(EnumTick));
	});

	It("reads the resistance at each tick, as the enum path does", [this]()
	{
		if (!ASC) return;
		const FActiveSkillEffect Dot = MakeTickEffect(ESkillEffectType::DOT, TICK_TEST_DOT_ID, TICK_TEST_DOT, ESpellElement::Fire);
		FWoREffectSpecBuilder::Apply(Hero, Dot, Hero);
		const float FirstTick = PayTicks();
		FActiveSkillEffect Resist = MakeTickEffect(ESkillEffectType::ResistanceBuff, TICK_TEST_RESIST_ID, TICK_TEST_FIRE_RESIST_BUFF, ESpellElement::Fire);
		Resist.bDelayFirstExecution = true;
		TestWorld->GetSubsystem<USkillEffectManager>()->ApplyEffect(Hero, Resist, Hero);
		Sync->SyncAttributes();
		const int32 EnumTick = USkillEffectManager::GetDotTickDamage(Hero, Dot);
		const float SecondTick = PayTicks();
		TestEqual(TEXT("GAS tick = enum tick after the buff"), SecondTick, -static_cast<float>(EnumTick));
		TestTrue(TEXT("The buff softened the tick"), SecondTick > FirstTick);
	});

	It("pays a share-of-max-HP DOT without resistance", [this]()
	{
		if (!ASC) return;
		FActiveSkillEffect Dot = MakeTickEffect(ESkillEffectType::DOT, TICK_TEST_DOT_ID, TICK_TEST_SHARE_DOT, ESpellElement::Fire);
		Dot.bShareOfMaxHP = true;
		const int32 EnumTick = USkillEffectManager::GetDotTickDamage(Hero, Dot);
		TestEqual(TEXT("Rounded, no resistance"), EnumTick, FMath::RoundToInt(TICK_TEST_SHARE_DOT));
		FWoREffectSpecBuilder::Apply(Hero, Dot, Hero);
		TestEqual(TEXT("GAS tick = enum tick"), PayTicks(), -static_cast<float>(EnumTick));
	});

	It("pays a HOT tick with the holder's healing received, rounded as the enum path's heal", [this]()
	{
		if (!ASC) return;
		FWoREffectSpecBuilder::Apply(Hero, MakeTickEffect(ESkillEffectType::HealingReceivedBuff, TICK_TEST_BOOST_ID, TICK_TEST_HEALING_RECEIVED,
														  ESpellElement::None), Hero);
		FWoREffectSpecBuilder::Apply(Hero, MakeTickEffect(ESkillEffectType::HealthRestore, TICK_TEST_HOT_ID, TICK_TEST_HOT, ESpellElement::None), Hero);
		FTickMathInputs Enum;
		Enum.Value = TICK_TEST_HOT;
		Enum.HealingReceivedPercent = TICK_TEST_HEALING_RECEIVED;
		const int32 EnumHeal = WoRTickMath::GetHotTickHeal(Enum);
		TestEqual(TEXT("The enum heal, rounded"), EnumHeal, TICK_TEST_EXPECTED_HEAL);
		TestEqual(TEXT("GAS heal = enum heal"), PayTicks(), static_cast<float>(EnumHeal));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
