// IceSpec.cpp
// B-064a (D-029, D-090, Q-145b, Q-064a): Ice, a Water damage type. An Ice spell is keyed Water (a Water source unlocks
// it, a Fire one refuses it); Ice resistance resists Ice and Water, Water resistance doesn't resist Ice; a full Ice bar
// fires IceBreak: Water's Action Speed + Spell Speed slow plus a share of max HP; Water immunity blocks Ice buildup.
// Mirrors the deckbuilder's TestIceReach (09-fixtures.md) where this game has the piece; Water Amp against Ice is PIE
// B-064a. B-064c: Ice is a resistance column (gear lines, the profile asset); a resistance buff or Ward keyed Ice covers
// Ice and Water, a Water one Water only; Water-scoped damage boosts and Water StatusMultiplier don't reach an Ice hit.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Combat/Damage/DamageMath.h"
#include "Combat/Damage/WoRHitKind.h"
#include "Combat/Elements/ElementTable.h"
#include "Combat/Resistance/ResistanceColumns.h"
#include "Combat/Resistance/ResistanceProfileData.h"
#include "Combat/Resistance/ResistanceQuery.h"
#include "Equipment/FResistanceBonus.h"
#include "GAS/WoRAbsorb.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "GAS/WoRDamageExecution.h"
#include "Skills/Definitions/ElementHelpers.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/BreakTable.h"
#include "Skills/Effects/BuildupMath.h"
#include "Skills/Effects/EDamageScope.h"
#include "Skills/Effects/FSkillEffectPayload.h"
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
	/** B-064c. Percent, as gear lines, profile columns and buff values are. */
	constexpr float ICE_SPEC_ICE_LINE_PERCENT = 12.0f;
	constexpr float ICE_SPEC_ICE_BUFF_PERCENT = 25.0f;
	constexpr float ICE_SPEC_DAMAGE_BUFF_PERCENT = 50.0f;
	constexpr float ICE_SPEC_STATUS_MULT_PERCENT = 40.0f;
	constexpr int32 ICE_SPEC_ICE_BUFF_ID = 6403;
	constexpr int32 ICE_SPEC_DAMAGE_BUFF_ID = 6404;
	constexpr int32 ICE_SPEC_STATUS_MULT_ID = 6405;
	constexpr int32 ICE_SPEC_PAYLOAD_ID = 6406;
	constexpr int32 ICE_SPEC_BASE_DAMAGE = 100;
	constexpr int32 ICE_SPEC_WARD_MAGNITUDE = 30;
	constexpr int32 ICE_SPEC_HIT_DAMAGE = 50;
	constexpr float ICE_SPEC_BUILDUP_AMOUNT = 10.0f;

	FWoRAbsorbCharge IceSpecWaterWard(bool bKeyedIce)
	{
		FWoRAbsorbCharge Ward;
		Ward.Verb = EWoRAbsorbVerb::Ward;
		Ward.Scope = EWoRWardScope::Element;
		Ward.Element = ESpellElement::Water;
		Ward.bKeyedIce = bKeyedIce;
		Ward.Magnitude = ICE_SPEC_WARD_MAGNITUDE;
		Ward.Fill();
		return Ward;
	}

	FActiveSkillEffect IceSpecFromPayload(ESkillEffectType Type, ESpellElement Element, bool bKeyedIce)
	{
		FSkillEffectPayload P;
		P.EffectType = Type;
		P.Element = Element;
		P.bKeyedIce = bKeyedIce;
		P.Value = FMath::RoundToInt(ICE_SPEC_ICE_BUFF_PERCENT);
		P.Duration = ICE_SPEC_TURNS;
		return FActiveSkillEffect::BuildRuntimeFromPayload(TEXT("Ice key"), ICE_SPEC_PAYLOAD_ID, P, {}, false, 1, false, Element);
	}

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

	void GiveEffect(int32 ID, ESkillEffectType Type, float Value, ESpellElement Element, bool bKeyedIce = false,
					AActor *Holder = nullptr, EDamageScope Scope = EDamageScope::All)
	{
		AActor *To = Holder ? Holder : Target;
		FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(TEXT("Ice spec"), ID, Type, Value, ICE_SPEC_TURNS);
		Effect.Element = Element;
		Effect.bKeyedIce = bKeyedIce;
		Effect.DamageScope = Scope;
		Effect.bDelayFirstExecution = true;
		Effects->ApplyEffect(To, Effect, To);
	}

	/** B-064c: one spell hit's final damage from Attacker on Target, resistance ignored (only the boosts differ). */
	int32 SpellHit(AActor *Attacker, const FWoRHitKind &Hit)
	{
		UDamageCalculator *Calculator = TestWorld->GetSubsystem<UDamageCalculator>();
		if (!Calculator)
		{
			AddError(TEXT("no damage calculator"));
			return INDEX_NONE;
		}
		FDamageCalculationInput Input;
		Input.BaseDamage = ICE_SPEC_BASE_DAMAGE;
		Input.ActionType = EActionType::Spell;
		Input.Element = Hit.Element;
		Input.DealtTypeOverride = FWoRHitKind::GetDealtTypeOverride(Hit);
		Input.bIgnoreResistance = true;
		Input.OverrideCritChance = 0.0f;
		return Calculator->CalculateDamage(Attacker, Target, Input).FinalDamage;
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

	It("carries Ice through the profile asset and gear lines (B-064c)", [this]()
	{
		FResistanceProfileRow Profile;
		Profile.Ice = ICE_SPEC_ICE_LINE_PERCENT;
		TestEqual(TEXT("the asset's row keeps Ice (ResolveRow)"), Profile.ToTableRow().Ice, ICE_SPEC_ICE_LINE_PERCENT);
		TestEqual(TEXT("and back"), FResistanceProfileRow::FromTableRow(Profile.ToTableRow()).Ice, ICE_SPEC_ICE_LINE_PERCENT);
		TestEqual(TEXT("a new asset starts from the C++ table's Ice (0, TODO tune)"), UResistanceProfileData::StaticClass()
					  ->GetDefaultObject<UResistanceProfileData>()->GenericClass.Ice, 0.0f);
		FResistanceBonus Gear;
		Gear.Ice = ICE_SPEC_ICE_LINE_PERCENT;
		FResistanceBonus Total;
		Total.Accumulate(Gear);
		Total.Accumulate(Gear);
		TestEqual(TEXT("gear Ice lines sum"), ResistanceColumns::Get(Total, EWoRDamageType::Ice), ICE_SPEC_ICE_LINE_PERCENT * 2.0f);
		TestEqual(TEXT("an Ice line isn't a Water line"), ResistanceColumns::Get(Total, EWoRDamageType::Water), 0.0f);
	});

	It("keys a resistance buff or Ward Ice only on Water (Q-064c)", [this]()
	{
		TestTrue(TEXT("a Water resistance buff keyed Ice"), IceSpecFromPayload(ESkillEffectType::ResistanceBuff, ESpellElement::Water, true).bKeyedIce);
		TestTrue(TEXT("a Water Ward keyed Ice"), IceSpecFromPayload(ESkillEffectType::NegateElement, ESpellElement::Water, true).bKeyedIce);
		TestFalse(TEXT("a Fire one: ignored"), IceSpecFromPayload(ESkillEffectType::ResistanceBuff, ESpellElement::Fire, true).bKeyedIce);
		TestFalse(TEXT("a damage buff: ignored"), IceSpecFromPayload(ESkillEffectType::DamageBuff, ESpellElement::Water, true).bKeyedIce);
		TestFalse(TEXT("unkeyed by default"), IceSpecFromPayload(ESkillEffectType::ResistanceBuff, ESpellElement::Water, false).bKeyedIce);
	});

	It("wards by the deckbuilder's reach: a Water Ward catches Water only, one keyed Ice catches both (B-064c)", [this]()
	{
		const FWoRAbsorbCharge Water = IceSpecWaterWard(false);
		const FWoRAbsorbCharge IceKeyed = IceSpecWaterWard(true);
		TestTrue(TEXT("Water Ward, Water hit"), Water.Catches(ESpellElement::Water, EPhysicalDamageType::None));
		TestFalse(TEXT("Water Ward, Ice hit"), Water.Catches(ESpellElement::Water, EPhysicalDamageType::None, EWoRDamageType::Ice));
		TestTrue(TEXT("Ice Ward, Water hit"), IceKeyed.Catches(ESpellElement::Water, EPhysicalDamageType::None));
		TestTrue(TEXT("Ice Ward, Ice hit"), IceKeyed.Catches(ESpellElement::Water, EPhysicalDamageType::None, EWoRDamageType::Ice));
		TestFalse(TEXT("Ice Ward, Fire hit"), IceKeyed.Catches(ESpellElement::Fire, EPhysicalDamageType::None));
		FWoRAbsorbCharge Elemental = IceSpecWaterWard(false);
		Elemental.Scope = EWoRWardScope::Elemental;
		TestTrue(TEXT("an Elemental Ward catches Ice"), Elemental.Catches(ESpellElement::Water, EPhysicalDamageType::None, EWoRDamageType::Ice));

		FWoRAbsorbCharge Spent = IceSpecWaterWard(false);
		TArray<FWoRAbsorbCharge *> Charges{&Spent};
		TestEqual(TEXT("Spend: a Water Ward negates nothing of an Ice hit"),
				  WoRAbsorb::Spend(Charges, ICE_SPEC_HIT_DAMAGE, ESpellElement::Water, EPhysicalDamageType::None, EWoRDamageType::Ice), 0);
		TestEqual(TEXT("Spend: but its Magnitude of a Water hit"),
				  WoRAbsorb::Spend(Charges, ICE_SPEC_HIT_DAMAGE, ESpellElement::Water, EPhysicalDamageType::None), ICE_SPEC_WARD_MAGNITUDE);
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

		It("resists Ice and Water once each with an Ice-keyed resistance buff (B-064c)", [this]()
		{
			if (!Effects) return;
			const FWoRHitKind WaterHit = FWoRHitKind::FromPair(ESpellElement::Water, EPhysicalDamageType::None);
			const float IceBefore = WoRResistance::GetHitDamageResistance(Target, FWoRHitKind::Ice());
			const float WaterBefore = WoRResistance::GetHitDamageResistance(Target, WaterHit);
			const float FireBefore = WoRResistance::GetHitDamageResistance(Target, FWoRHitKind::FromPair(ESpellElement::Fire, EPhysicalDamageType::None));
			GiveEffect(ICE_SPEC_ICE_BUFF_ID, ESkillEffectType::ResistanceBuff, ICE_SPEC_ICE_BUFF_PERCENT, ESpellElement::Water, true);
			const float Buff = ICE_SPEC_ICE_BUFF_PERCENT / CombatConstants::STAT_PERCENT_DIVISOR;
			TestEqual(TEXT("an Ice hit gains it"), WoRResistance::GetHitDamageResistance(Target, FWoRHitKind::Ice()), IceBefore + Buff,
					  ICE_SPEC_TOLERANCE);
			TestEqual(TEXT("a Water hit gains it once"), WoRResistance::GetHitDamageResistance(Target, WaterHit), WaterBefore + Buff,
					  ICE_SPEC_TOLERANCE);
			TestEqual(TEXT("an Ice hit's buildup too"), WoRResistance::GetHitTypeResistance(Target, FWoRHitKind::Ice()),
					  WoRResistance::GetTypeResistance(Target, EWoRDamageType::Ice), ICE_SPEC_TOLERANCE);
			TestEqual(TEXT("a Fire hit doesn't"),
					  WoRResistance::GetHitDamageResistance(Target, FWoRHitKind::FromPair(ESpellElement::Fire, EPhysicalDamageType::None)),
					  FireBefore, ICE_SPEC_TOLERANCE);
		});

		It("doesn't let a Water-scoped damage buff boost an Ice hit (B-064c)", [this]()
		{
			if (!Effects) return;
			AActor *Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), false);
			if (!TestNotNull(TEXT("Attacker"), Attacker)) return;
			const FWoRHitKind WaterHit = FWoRHitKind::FromPair(ESpellElement::Water, EPhysicalDamageType::None);
			const int32 WaterPlain = SpellHit(Attacker, WaterHit);
			const int32 IcePlain = SpellHit(Attacker, FWoRHitKind::Ice());
			GiveEffect(ICE_SPEC_DAMAGE_BUFF_ID, ESkillEffectType::DamageBuff, ICE_SPEC_DAMAGE_BUFF_PERCENT, ESpellElement::Water, false,
					   Attacker, EDamageScope::Element);
			TestTrue(TEXT("a Water hit is raised"), SpellHit(Attacker, WaterHit) > WaterPlain);
			TestEqual(TEXT("an Ice hit isn't"), SpellHit(Attacker, FWoRHitKind::Ice()), IcePlain);
		});

		It("doesn't let a Water StatusMultiplier boost Ice buildup (B-064c)", [this]()
		{
			if (!Effects) return;
			AActor *Source = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), false);
			if (!TestNotNull(TEXT("Source"), Source)) return;
			GiveEffect(ICE_SPEC_STATUS_MULT_ID, ESkillEffectType::StatusMultiplierBuff, ICE_SPEC_STATUS_MULT_PERCENT, ESpellElement::Water,
					   false, Source);
			const FWoRHitKind WaterHit = FWoRHitKind::FromPair(ESpellElement::Water, EPhysicalDamageType::None);
			TestTrue(TEXT("a Water hit is raised"),
					 Buildup->GatherBuildupMathInputs(Source, Target, ICE_SPEC_BUILDUP_AMOUNT, WaterHit, false).ElementStatusMultiplier > 1.0f);
			TestEqual(TEXT("an Ice hit isn't"),
					  Buildup->GatherBuildupMathInputs(Source, Target, ICE_SPEC_BUILDUP_AMOUNT, FWoRHitKind::Ice(), false).ElementStatusMultiplier,
					  1.0f, ICE_SPEC_TOLERANCE);
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
			// B-170h2b (Q-170e, SB-22): Water's break slows Spell Speed only.
			TestEqual(TEXT("Spell Speed slowed"), FirstEffectValue(*Effects, Target, ESkillEffectType::SpellSpeedDebuff),
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
