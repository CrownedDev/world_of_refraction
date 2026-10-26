// CleanseGearSpec.cpp
// B-065 / D-015: cleanse (CleanseSelf / CleanseAllies → RemoveAllDebuffs) and Iolite only remove
// effects someone else applied. Effects a character's own gear puts on it are marked bFromGear and
// stay, yours and the enemy's.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Inventory/ItemExecutor.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/FGatheredEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 CLEANSE_TEST_GEAR_DEF_ID = 65001;
	constexpr int32 CLEANSE_TEST_GEAR_SOURCE_KEY = 65002;
	constexpr int32 CLEANSE_TEST_CAST_EFFECT_ID = 65003;
	constexpr int32 CLEANSE_TEST_DURATION_TURNS = 3;
	constexpr float CLEANSE_TEST_VALUE = 10.0f;
	constexpr int32 CLEANSE_TEST_PAYLOAD_VALUE = 10;
	const FCrystalId CLEANSE_TEST_IOLITE(ECrystalType::Iolite, EItemTier::S_Tier);
}

BEGIN_DEFINE_SPEC(FCleanseGearSpec, "WoR.Effects.CleanseGear",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;

	AActor *Spawn()
	{
		return TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
	}

	/** Puts a one-payload gear effect of Type on Wearer, the way equipping it does (its own bundle slot per type, so two differ). */
	void EquipEffect(AActor *Wearer, ESkillEffectType Type)
	{
		FSkillEffectPayload Payload;
		Payload.EffectType = Type;
		Payload.Value = CLEANSE_TEST_PAYLOAD_VALUE;
		Payload.bPermanent = true; // a gear passive, like an authored ring effect
		FSkillEffect Effect;
		Effect.EffectName = TEXT("Cleanse test gear");
		Effect.Payloads.Add(Payload);
		EffectManager->ApplyEquipmentEffects(Wearer, {FGatheredEffect(CLEANSE_TEST_GEAR_DEF_ID, static_cast<int32>(Type), Effect, CLEANSE_TEST_GEAR_SOURCE_KEY)});
	}

	void CastBuff(AActor *Target, ESkillEffectType Type)
	{
		EffectManager->ApplyEffect(Target, FActiveSkillEffect::CreateBuff(TEXT("Cleanse test cast"), CLEANSE_TEST_CAST_EFFECT_ID,
			Type, CLEANSE_TEST_VALUE, CLEANSE_TEST_DURATION_TURNS));
	}

	bool Has(AActor *Actor, ESkillEffectType Type) const
	{
		return EffectManager->HasEffectOfType(Actor, Type);
	}

	bool HasGearEffect(AActor *Actor, ESkillEffectType Type) const
	{
		return EffectManager->GetActiveEffects(Actor).ContainsByPredicate([Type](const FActiveSkillEffect &E)
		{
			return E.EffectType == Type && E.bFromGear;
		});
	}
END_DEFINE_SPEC(FCleanseGearSpec)

void FCleanseGearSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		TestNotNull(TEXT("SkillEffectManager"), EffectManager);
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
		EffectManager = nullptr;
	});

	It("marks effects put on by gear", [this]()
	{
		if (!EffectManager) return;
		AActor *Wearer = Spawn();
		EquipEffect(Wearer, ESkillEffectType::DefenseDebuff);
		TestTrue(TEXT("Gear effect marked"), HasGearEffect(Wearer, ESkillEffectType::DefenseDebuff));
	});

	It("cleanse removes an enemy's Burn but keeps your own gear debuff", [this]()
	{
		if (!EffectManager) return;
		AActor *Holder = Spawn();
		EquipEffect(Holder, ESkillEffectType::ApplyBurnToTarget); // the "attacks apply Burn" ring (D-015)
		EquipEffect(Holder, ESkillEffectType::DefenseDebuff);
		EffectManager->ApplyTriggeredSkillEffect(Spawn(), Holder, ESkillEffectType::DOT, ESpellElement::Fire);
		if (!TestTrue(TEXT("Burned"), Has(Holder, ESkillEffectType::DOT))) return;

		EffectManager->RemoveAllDebuffs(Holder);
		TestFalse(TEXT("Burn cleansed"), Has(Holder, ESkillEffectType::DOT));
		TestTrue(TEXT("Burn ring kept"), HasGearEffect(Holder, ESkillEffectType::ApplyBurnToTarget));
		TestTrue(TEXT("Gear debuff kept"), HasGearEffect(Holder, ESkillEffectType::DefenseDebuff));
	});

	It("a targeted cleanse keeps a gear debuff of that type and removes a cast one", [this]()
	{
		if (!EffectManager) return;
		AActor *Holder = Spawn();
		EquipEffect(Holder, ESkillEffectType::DefenseDebuff);
		EffectManager->ApplyEffect(Holder, FActiveSkillEffect::CreateBuff(TEXT("Cleanse test cast"), CLEANSE_TEST_CAST_EFFECT_ID,
			ESkillEffectType::DefenseDebuff, CLEANSE_TEST_VALUE, CLEANSE_TEST_DURATION_TURNS), Spawn());

		TestEqual(TEXT("Only the cast debuff cleansed"), EffectManager->CleanseEffectsByType(Holder, ESkillEffectType::DefenseDebuff), 1);
		TestTrue(TEXT("Gear debuff kept"), HasGearEffect(Holder, ESkillEffectType::DefenseDebuff));
	});

	It("removing all buffs keeps gear buffs and removes cast ones", [this]()
	{
		if (!EffectManager) return;
		AActor *Holder = Spawn();
		EquipEffect(Holder, ESkillEffectType::RawDamageBuff);
		CastBuff(Holder, ESkillEffectType::DefenseBuff);

		EffectManager->RemoveAllBuffs(Holder);
		TestFalse(TEXT("Cast buff removed"), Has(Holder, ESkillEffectType::DefenseBuff));
		TestTrue(TEXT("Gear buff kept"), HasGearEffect(Holder, ESkillEffectType::RawDamageBuff));
	});

	It("Iolite on an enemy strips its cast buffs but keeps its gear buffs", [this]()
	{
		if (!EffectManager) return;
		UItemExecutor *Items = TestWorld->GetSubsystem<UItemExecutor>();
		if (!TestNotNull(TEXT("ItemExecutor"), Items)) return;
		AActor *User = Spawn();
		AActor *Enemy = Spawn();
		EquipEffect(Enemy, ESkillEffectType::RawDamageBuff);
		CastBuff(Enemy, ESkillEffectType::DefenseBuff);

		const FItemUseResult Result = Items->UseItem(User, CLEANSE_TEST_IOLITE, Enemy);
		TestTrue(FString::Printf(TEXT("Iolite used (%s)"), *Result.ErrorMessage), Result.bSuccess);
		TestFalse(TEXT("Cast buff stripped"), Has(Enemy, ESkillEffectType::DefenseBuff));
		TestTrue(TEXT("Gear buff kept"), HasGearEffect(Enemy, ESkillEffectType::RawDamageBuff));
	});
}

#endif
