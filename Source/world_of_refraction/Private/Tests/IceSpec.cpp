// IceSpec.cpp
// B-064a (D-029, D-090, Q-145b, Q-064a): Ice, a Water damage type. An Ice spell is keyed Water (a Water source unlocks
// it, a Fire one refuses it); Ice resistance resists Ice and Water, Water resistance doesn't resist Ice; a full Ice bar
// fires IceBreak: Water's Action Speed + Spell Speed slow plus a share of max HP; Water immunity blocks Ice buildup.
// Mirrors the deckbuilder's TestIceReach (09-fixtures.md) where this game has the piece; Ice wards and Ice-keyed
// buffs are B-064c, Water Amp against Ice is PIE B-064a.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"
#include "Combat/Damage/WoRHitKind.h"
#include "Combat/Elements/ElementTable.h"
#include "Combat/Resistance/ResistanceQuery.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "GAS/WoRDamageExecution.h"
#include "Skills/Definitions/ElementHelpers.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/BreakTable.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Tags/WoRGameplayTags.h"
#include "WoRGameNames.h"

namespace
{
	/** Fractions, as the Resist_* attributes hold. */
	constexpr float ICE_SPEC_WATER_RESIST = 0.2f;
	constexpr float ICE_SPEC_ICE_RESIST = 0.15f;
	/** A percent, as ResistanceBuff values are. */
	constexpr float ICE_SPEC_WATER_BUFF_PERCENT = 20.0f;
	constexpr int32 ICE_SPEC_WATER_BUFF_ID = 6401;
	constexpr int32 ICE_SPEC_WATER_IMMUNITY_ID = 6402;
	constexpr int32 ICE_SPEC_TURNS = 3;
	/** Far past the bar's threshold, whatever the target's resistance (below RESISTANCE_MAX). */
	constexpr float ICE_SPEC_OVERWHELMING_FACTOR = 100.0f;
	constexpr float ICE_SPEC_OVERWHELMING_HIT = CombatConstants::STATUS_EFFECT_THRESHOLD * ICE_SPEC_OVERWHELMING_FACTOR;
	constexpr float ICE_SPEC_TOLERANCE = 1.0e-4f;

	USpellData *NewIceSpell(ESpellElement Element)
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Element = Element;
		Spell->bDealsIce = true;
		return Spell;
	}

	float FirstEffectValue(const USkillEffectManager &Effects, AActor *Holder, ESkillEffectType Type)
	{
		const TArray<FActiveSkillEffect> Found = Effects.GetEffectsByType(Holder, Type);
		return Found.Num() > 0 ? Found[0].EffectValue : -1.0f;
	}
}

BEGIN_DEFINE_SPEC(FIceSpec, "WoR.Elements.Ice", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *Effects = nullptr;
	UStatusBuildupManager *Buildup = nullptr;
	AActor *Target = nullptr;

	void GiveEffect(int32 ID, ESkillEffectType Type, float Value, ESpellElement Element)
	{
		FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(TEXT("Ice spec"), ID, Type, Value, ICE_SPEC_TURNS);
		Effect.Element = Element;
		Effect.bDelayFirstExecution = true;
		Effects->ApplyEffect(Target, Effect, Target);
	}
END_DEFINE_SPEC(FIceSpec)

void FIceSpec::Define()
{
	It("needs a Water source: a Water-keyed Ice spell fits a Water ring, not a Fire one (D-029)", [this]()
	{
		const USpellData *Ice = NewIceSpell(ESpellElement::Water);
		TestTrue(TEXT("valid key"), Ice->IsIceKeyValid());
		TestTrue(TEXT("a Water ring takes it"), ElementHelpers::SpellElementMatchesHost(Ice->Element, ESpellElement::Water));
		TestFalse(TEXT("a Fire ring refuses it"), ElementHelpers::SpellElementMatchesHost(Ice->Element, ESpellElement::Fire));
		TestEqual(TEXT("cast as Water, it deals Ice"), Ice->GetDealtTypeOverride(ESpellElement::Water), EWoRDamageType::Ice);
		TestEqual(TEXT("a cast that resolved elsewhere deals no Ice"), Ice->GetDealtTypeOverride(ESpellElement::Fire), EWoRDamageType::None);

		TestFalse(TEXT("an Ice flag on a Fire spell gets rule 75's note"), NewIceSpell(ESpellElement::Fire)->IsIceKeyValid());
		TestEqual(TEXT("a Fire-keyed flag deals nothing (it never casts as Water)"),
				  NewIceSpell(ESpellElement::Fire)->GetDealtTypeOverride(ESpellElement::Fire), EWoRDamageType::None);
		TestTrue(TEXT("a Generic spell may deal Ice (rule 75)"), NewIceSpell(ESpellElement::Generic)->IsIceKeyValid());
		TestEqual(TEXT("a Generic Ice spell resolved to Water deals Ice"),
				  NewIceSpell(ESpellElement::Generic)->GetDealtTypeOverride(ESpellElement::Water), EWoRDamageType::Ice);
		TestEqual(TEXT("resolved to Fire, it deals Fire"),
				  NewIceSpell(ESpellElement::Generic)->GetDealtTypeOverride(ESpellElement::Fire), EWoRDamageType::None);
		USpellData *Water = NewObject<USpellData>(GetTransientPackage());
		Water->Element = ESpellElement::Water;
		TestEqual(TEXT("a plain Water spell deals Water"), Water->GetDealtTypeOverride(ESpellElement::Water), EWoRDamageType::None);
	});

	It("is heard by On Water hit, while On Ice hit doesn't hear Water (TestIceReach)", [this]()
	{
		const FGameplayTag IceTag = WoRTags::DamageTypeToTag(EWoRDamageType::Ice);
		const FGameplayTag WaterTag = WoRTags::DamageTypeToTag(EWoRDamageType::Water);
		TestTrue(TEXT("On Water hit hears Ice"), IceTag.MatchesTag(WaterTag));
		TestFalse(TEXT("On Ice hit doesn't hear Water"), WaterTag.MatchesTag(IceTag));
	});

	It("breaks as Ice: Water's slow plus damage, from its one row (D-090, Q-064a)", [this]()
	{
		TestEqual(TEXT("the Ice row's break"), ElementTable::For(EWoRDamageType::Ice).BreakTrigger, ESkillEffectType::IceBreak);
		const FBreakRow *Row = BreakTable::For(ESkillEffectType::IceBreak);
		if (!TestNotNull(TEXT("Ice break row"), Row)) return;
		TestEqual(TEXT("slows actions"), Row->Applied, ESkillEffectType::ActionSpeedDebuff);
		TestEqual(TEXT("and spells"), Row->AlsoApplied, ESkillEffectType::SpellSpeedDebuff);
		TestEqual(TEXT("slow size"), Row->Size, CombatConstants::ICE_BREAK_SPEED_SLOW_PERCENT);
		TestEqual(TEXT("damage share"), Row->DamageShareOfMaxHP, CombatConstants::ICE_BREAK_DAMAGE_MAX_HP_FRACTION);
		TestEqual(TEXT("no other break deals extra damage"), BreakTable::For(ESkillEffectType::ActionSpeedDebuff)->DamageShareOfMaxHP, 0.0f);
		TestFalse(TEXT("named"), WoRGameNames::GetSkillEffectTypeName(ESkillEffectType::IceBreak).IsEmpty());
	});

	Describe("in a fight", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("test world created"), TestWorld->Create());
			Effects = TestWorld->GetSubsystem<USkillEffectManager>();
			Buildup = TestWorld->GetSubsystem<UStatusBuildupManager>();
			Target = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
			if (!TestNotNull(TEXT("SkillEffectManager"), Effects) || !TestNotNull(TEXT("StatusBuildupManager"), Buildup) ||
				!TestNotNull(TEXT("Target"), Target))
			{
				Effects = nullptr; // each case returns early
			}
		});

		AfterEach([this]()
		{
			Effects = nullptr;
			Buildup = nullptr;
			Target = nullptr;
			TestWorld.Reset();
		});

		It("resists by the deckbuilder's reach: Water resistance doesn't reach an Ice hit (Q-145b)", [this]()
		{
			if (!Effects) return;
			const float IceBefore = WoRResistance::GetHitDamageResistance(Target, FWoRHitKind::Ice());
			const float WaterBefore = WoRResistance::GetHitDamageResistance(Target, FWoRHitKind::FromPair(ESpellElement::Water, EPhysicalDamageType::None));
			GiveEffect(ICE_SPEC_WATER_BUFF_ID, ESkillEffectType::ResistanceBuff, ICE_SPEC_WATER_BUFF_PERCENT, ESpellElement::Water);
			const float Buff = ICE_SPEC_WATER_BUFF_PERCENT / CombatConstants::STAT_PERCENT_DIVISOR;
			TestEqual(TEXT("a Water hit gains the Water buff"),
					  WoRResistance::GetHitDamageResistance(Target, FWoRHitKind::FromPair(ESpellElement::Water, EPhysicalDamageType::None)),
					  WaterBefore + Buff, ICE_SPEC_TOLERANCE);
			TestEqual(TEXT("an Ice hit doesn't"), WoRResistance::GetHitDamageResistance(Target, FWoRHitKind::Ice()), IceBefore, ICE_SPEC_TOLERANCE);
			TestEqual(TEXT("an Ice hit reads Ice resistance alone"), WoRResistance::GetHitDamageResistance(Target, FWoRHitKind::Ice()),
					  WoRResistance::GetTypeResistance(Target, EWoRDamageType::Ice), ICE_SPEC_TOLERANCE);
			TestEqual(TEXT("its buildup too"), WoRResistance::GetHitTypeResistance(Target, FWoRHitKind::Ice()),
					  WoRResistance::GetTypeResistance(Target, EWoRDamageType::Ice), ICE_SPEC_TOLERANCE);
		});

		It("resists Ice and Water with Ice resistance, on the attributes (Q-145b)", [this]()
		{
			if (!Effects) return;
			UAbilitySystemComponent *ASC = FWoRTestWorld::AddComponent<UAbilitySystemComponent>(Target);
			FWoRTestWorld::AddComponent<UWoRAttributeSyncComponent>(Target);
			ASC->SetNumericAttributeBase(UWoRAttributeSet_Resistance::GetTypeAttribute(EWoRDamageType::Water), ICE_SPEC_WATER_RESIST);
			ASC->SetNumericAttributeBase(UWoRAttributeSet_Resistance::GetTypeAttribute(EWoRDamageType::Ice), ICE_SPEC_ICE_RESIST);
			float Water = 0.0f;
			float Ice = 0.0f;
			TestTrue(TEXT("Water read"), UWoRDamageExecution::ReadHitDamageResistance(ASC, FWoRHitKind::FromPair(ESpellElement::Water, EPhysicalDamageType::None), Water));
			TestTrue(TEXT("Ice read"), UWoRDamageExecution::ReadHitDamageResistance(ASC, FWoRHitKind::Ice(), Ice));
			TestEqual(TEXT("Ice Res resists Water"), Water, ICE_SPEC_WATER_RESIST + ICE_SPEC_ICE_RESIST, ICE_SPEC_TOLERANCE);
			TestEqual(TEXT("Ice Res resists Ice, Water Res doesn't"), Ice, ICE_SPEC_ICE_RESIST, ICE_SPEC_TOLERANCE);
		});

		It("fires IceBreak from a full Ice bar: the slow for its turn, then a share of max HP", [this]()
		{
			if (!Effects) return;
			UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();
			if (!TestNotNull(TEXT("CharacterDataComponent"), TargetComp)) return;
			const int32 HPBefore = TargetComp->CurrentHP;
			const int32 ExpectedDamage = FMath::RoundToInt(TargetComp->MaxHP * CombatConstants::ICE_BREAK_DAMAGE_MAX_HP_FRACTION);

			TestTrue(TEXT("the bar breaks"), Buildup->AddStatusBuildupForHit(nullptr, Target, ICE_SPEC_OVERWHELMING_HIT, FWoRHitKind::Ice()));
			TestEqual(TEXT("Action Speed slowed"), FirstEffectValue(*Effects, Target, ESkillEffectType::ActionSpeedDebuff),
					  CombatConstants::ICE_BREAK_SPEED_SLOW_PERCENT);
			TestEqual(TEXT("Spell Speed slowed"), FirstEffectValue(*Effects, Target, ESkillEffectType::SpellSpeedDebuff),
					  CombatConstants::ICE_BREAK_SPEED_SLOW_PERCENT);
			TestEqual(TEXT("damage dealt"), HPBefore - TargetComp->CurrentHP, ExpectedDamage);
			TestTrue(TEXT("some damage at all"), ExpectedDamage > 0);
		});

		It("leaves Water's break without damage", [this]()
		{
			if (!Effects) return;
			UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();
			if (!TestNotNull(TEXT("CharacterDataComponent"), TargetComp)) return;
			const int32 HPBefore = TargetComp->CurrentHP;
			TestTrue(TEXT("the Water bar breaks"),
					 Buildup->AddStatusBuildup(nullptr, Target, ICE_SPEC_OVERWHELMING_HIT, ESpellElement::Water, EPhysicalDamageType::None));
			TestEqual(TEXT("Action Speed slowed"), FirstEffectValue(*Effects, Target, ESkillEffectType::ActionSpeedDebuff),
					  CombatConstants::WATER_BREAK_SPEED_SLOW_PERCENT);
			TestEqual(TEXT("no damage"), TargetComp->CurrentHP, HPBefore);
		});

		It("lets Water immunity block Ice buildup (an Ice hit is keyed Water, Q-064a)", [this]()
		{
			if (!Effects) return;
			GiveEffect(ICE_SPEC_WATER_IMMUNITY_ID, ESkillEffectType::GrantWaterImmunity, 0.0f, ESpellElement::None);
			TestFalse(TEXT("nothing fires"), Buildup->AddStatusBuildupForHit(nullptr, Target, ICE_SPEC_OVERWHELMING_HIT, FWoRHitKind::Ice()));
			TestEqual(TEXT("bar untouched"), Buildup->GetStatusBarBuildup(Target), 0.0f, ICE_SPEC_TOLERANCE);
		});
	});
}

#endif
