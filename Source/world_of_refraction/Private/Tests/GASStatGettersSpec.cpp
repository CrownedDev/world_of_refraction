// GASStatGettersSpec.cpp
// GAS stage 3 (B-042yb): with `wor.Effects.UseGAS 1` the composed sub-stat getters (spell / raw damage, action /
// spell speed) and the pillar getters return their GAS attribute; with 0 they compose on the game's side. Both
// give the same value, and the debug compare still checks the attribute against the game's composition.
// B-042yc adds luck, reflex, efficiency, status multiplier, Defense, crit chance / damage, turn speed and the
// resistances (per type, general status, blanket bonus).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Combat/Resistance/ResistanceQuery.h"
#include "Combat/TurnManager.h"
#include "Debug/GAS/WoRAttributeDebug.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr float GETTER_TOLERANCE = 0.0001f;
	/** TestEqual on floats defaults to a tolerance; the getters must match to the bit. */
	constexpr float GETTER_EXACT = 0.0f;
	/** Small enough to stay under each attribute's clamp. */
	constexpr float GETTER_EXTRA_SPELL_DAMAGE = 0.05f;
	constexpr float GETTER_EXTRA_MIND = 1.0f;
	/** An odd percentage, so the buff layer isn't a round factor. */
	constexpr float GETTER_BUFF_PERCENT = 13.0f;
	constexpr int32 GETTER_BUFF_ID = 4701;
	constexpr int32 GETTER_BUFF_TURNS = 3;
	constexpr int32 GETTER_CRIT_BUFF_ID = 4702;

	struct FGetterCase
	{
		const TCHAR *Label;
		TFunction<float(const UCharacterDataComponent &)> Read;
	};

	TArray<FGetterCase> GetterCases()
	{
		return {
			{TEXT("SpellDamage"), [](const UCharacterDataComponent &C) { return C.GetEffectiveSpellDamage(); }},
			{TEXT("RawDamage"), [](const UCharacterDataComponent &C) { return C.GetEffectiveRawDamage(); }},
			{TEXT("ActionSpeed"), [](const UCharacterDataComponent &C) { return C.GetEffectiveActionSpeed(); }},
			{TEXT("SpellSpeed"), [](const UCharacterDataComponent &C) { return C.GetEffectiveSpellSpeed(); }},
			{TEXT("Mind"), [](const UCharacterDataComponent &C) { return C.GetEvolutionModifiedMind(); }},
			{TEXT("Body"), [](const UCharacterDataComponent &C) { return C.GetEvolutionModifiedBody(); }},
			{TEXT("Spirit"), [](const UCharacterDataComponent &C) { return C.GetEvolutionModifiedSpirit(); }},
			{TEXT("Luck"), [](const UCharacterDataComponent &C) { return C.GetEquipmentModifiedLuck(); }},
			{TEXT("Reflex"), [](const UCharacterDataComponent &C) { return C.ReflexWindowGearFactor(); }},
			{TEXT("Efficiency"), [](const UCharacterDataComponent &C) { return C.GetEffectiveEfficiencyMultiplier(); }},
			{TEXT("StatusMultiplier"), [](const UCharacterDataComponent &C) { return C.GetEffectiveStatusMultiplier(); }},
		};
	}
}

BEGIN_DEFINE_SPEC(FGASStatGettersSpec, "WoR.GAS.StatGetters",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UAbilitySystemComponent *ASC = nullptr;
	UCharacterDataComponent *Char = nullptr;
	AActor *Hero = nullptr;

	/** An extra GE on Attribute that only GAS knows about: a getter reading GAS sees it. */
	void AddGASOnlyBonus(const FGameplayAttribute &Attribute, float Amount)
	{
		UGameplayEffect *Bonus = NewObject<UGameplayEffect>(GetTransientPackage());
		Bonus->DurationPolicy = EGameplayEffectDurationType::Infinite;
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Amount));
		Bonus->Modifiers.Add(Modifier);
		ASC->ApplyGameplayEffectToSelf(Bonus, 1.0f, ASC->MakeEffectContext());
	}

	/** B-042yc: the getters that live on the subsystems and the resistance query, read for Hero. */
	TArray<FGetterCase> SubsystemGetterCases()
	{
		TArray<FGetterCase> Cases;
		const UDamageCalculator *Calc = TestWorld->GetSubsystem<UDamageCalculator>();
		const UTurnManager *Turns = TestWorld->GetSubsystem<UTurnManager>();
		AActor *Target = Hero;
		if (Calc)
		{
			Cases.Add({TEXT("Defense"), [Calc, Target](const UCharacterDataComponent &) { return Calc->GetDefenderFlatDefense(Target); }});
			Cases.Add({TEXT("CritChance"), [Calc, Target](const UCharacterDataComponent &) { return Calc->GetCriticalChance(Target); }});
			Cases.Add({TEXT("CritDamage"), [Calc, Target](const UCharacterDataComponent &) { return Calc->GetCritDamageMultiplier(Target); }});
		}
		if (Turns)
		{
			Cases.Add({TEXT("TurnSpeed"), [Turns, Target](const UCharacterDataComponent &) { return float(Turns->GetEffectiveTurnSpeed(Target)); }});
		}
		Cases.Add({TEXT("StatusResistance"), [Target](const UCharacterDataComponent &) { return WoRResistance::GetGeneralStatusResistance(Target); }});
		Cases.Add({TEXT("StatusResistBonus"), [Target](const UCharacterDataComponent &) { return WoRResistance::GetStatusResistBonus(Target); }});
		Cases.Add({TEXT("Resist_Fire"), [Target](const UCharacterDataComponent &) { return WoRResistance::GetTypeResistance(Target, EWoRDamageType::Fire); }});
		Cases.Add({TEXT("Resist_Slash"), [Target](const UCharacterDataComponent &) { return WoRResistance::GetTypeResistance(Target, EWoRDamageType::Slash); }});
		return Cases;
	}
END_DEFINE_SPEC(FGASStatGettersSpec)

void FGASStatGettersSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (Hero)
		{
			// Same order as ACombatCharacter: the ASC, then the sync component that fills it.
			ASC = FWoRTestWorld::AddComponent<UAbilitySystemComponent>(Hero);
			FWoRTestWorld::AddComponent<UWoRAttributeSyncComponent>(Hero);
			Char = Hero->FindComponentByClass<UCharacterDataComponent>();
		}
		if (!TestNotNull(TEXT("AbilitySystemComponent"), ASC) || !TestNotNull(TEXT("CharacterDataComponent"), Char))
		{
			ASC = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		UWoREffectMirrorComponent::SetReadsFromGAS(false);
		ASC = nullptr;
		Char = nullptr;
		Hero = nullptr;
		TestWorld.Reset();
	});

	It("returns the same value from each getter with UseGAS 0 and 1", [this]()
	{
		if (!ASC) return;
		TArray<FGetterCase> Cases = GetterCases();
		Cases.Append(SubsystemGetterCases());
		for (const FGetterCase &Case : Cases)
		{
			UWoREffectMirrorComponent::SetReadsFromGAS(false);
			const float OnGameSide = Case.Read(*Char);
			UWoREffectMirrorComponent::SetReadsFromGAS(true);
			// Exactly: a caller may round these (turn speed, max pools), so an ulp could flip a result.
			TestEqual(FString::Printf(TEXT("%s: same on both paths"), Case.Label), Case.Read(*Char), OnGameSide, GETTER_EXACT);
		}
		TestEqual(TEXT("UseGAS 1: attributes match the composition"), UWoRAttributeDebug::GetAttributeMismatchString(Hero), FString());
	});

	It("matches exactly with a buff layer that isn't 1", [this]()
	{
		if (!ASC) return;
		USkillEffectManager *EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		if (!TestNotNull(TEXT("SkillEffectManager"), EffectManager)) return;
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Getter buff"), GETTER_BUFF_ID, ESkillEffectType::SpellDamageBuff,
																	   GETTER_BUFF_PERCENT, GETTER_BUFF_TURNS),
								   Hero);
		UWoREffectMirrorComponent::SetReadsFromGAS(false);
		const float OnGameSide = Char->GetEffectiveSpellDamage();
		UWoREffectMirrorComponent::SetReadsFromGAS(true);
		TestEqual(TEXT("SpellDamage with a buff: same on both paths, exactly"), Char->GetEffectiveSpellDamage(), OnGameSide, GETTER_EXACT);
	});

	It("matches crit chance exactly with its buff layer (B-042yc)", [this]()
	{
		if (!ASC) return;
		USkillEffectManager *EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		const UDamageCalculator *Calc = TestWorld->GetSubsystem<UDamageCalculator>();
		if (!TestNotNull(TEXT("SkillEffectManager"), EffectManager) || !TestNotNull(TEXT("DamageCalculator"), Calc)) return;
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Getter crit buff"), GETTER_CRIT_BUFF_ID,
																	   ESkillEffectType::CritChanceBuff, GETTER_BUFF_PERCENT, GETTER_BUFF_TURNS),
								   Hero);
		UWoREffectMirrorComponent::SetReadsFromGAS(false);
		const float OnGameSide = Calc->GetCriticalChance(Hero);
		UWoREffectMirrorComponent::SetReadsFromGAS(true);
		TestEqual(TEXT("CritChance with a buff: same on both paths, exactly"), Calc->GetCriticalChance(Hero), OnGameSide, GETTER_EXACT);
	});

	It("returns the GAS attribute with UseGAS 1, and the composition with 0", [this]()
	{
		if (!ASC) return;
		const float SpellDamage = Char->GetEffectiveSpellDamage();
		const float Mind = Char->GetEvolutionModifiedMind();
		AddGASOnlyBonus(UWoRAttributeSet_Stats::GetSpellDamageAttribute(), GETTER_EXTRA_SPELL_DAMAGE);
		AddGASOnlyBonus(UWoRAttributeSet_Stats::GetMindAttribute(), GETTER_EXTRA_MIND);

		TestEqual(TEXT("UseGAS 0: SpellDamage composed"), Char->GetEffectiveSpellDamage(), SpellDamage, GETTER_TOLERANCE);
		TestEqual(TEXT("UseGAS 0: Mind composed"), Char->GetEvolutionModifiedMind(), Mind, GETTER_TOLERANCE);
		UWoREffectMirrorComponent::SetReadsFromGAS(true);
		TestTrue(TEXT("UseGAS 1: SpellDamage from GAS"), Char->GetEffectiveSpellDamage() > SpellDamage + GETTER_TOLERANCE);
		TestTrue(TEXT("UseGAS 1: Mind from GAS"), Char->GetEvolutionModifiedMind() > Mind + GETTER_TOLERANCE);
		TestFalse(TEXT("UseGAS 1: the compare still reads the composition, so it sees the extra"),
				  UWoRAttributeDebug::GetAttributeMismatchString(Hero).IsEmpty());
	});

	It("returns GAS's Defense, crit damage and a resistance with UseGAS 1 (B-042yc)", [this]()
	{
		if (!ASC) return;
		const UDamageCalculator *Calc = TestWorld->GetSubsystem<UDamageCalculator>();
		if (!TestNotNull(TEXT("DamageCalculator"), Calc)) return;
		AActor *Target = Hero;
		const float Defense = Calc->GetDefenderFlatDefense(Target);
		const float CritDamage = Calc->GetCritDamageMultiplier(Target);
		const float FireResist = WoRResistance::GetTypeResistance(Target, EWoRDamageType::Fire);
		AddGASOnlyBonus(UWoRAttributeSet_Stats::GetDefenseAttribute(), GETTER_EXTRA_SPELL_DAMAGE);
		AddGASOnlyBonus(UWoRAttributeSet_Stats::GetCritDamageAttribute(), GETTER_EXTRA_SPELL_DAMAGE);
		AddGASOnlyBonus(UWoRAttributeSet_Resistance::GetResist_FireAttribute(), GETTER_EXTRA_SPELL_DAMAGE);

		TestEqual(TEXT("UseGAS 0: Defense composed"), Calc->GetDefenderFlatDefense(Target), Defense, GETTER_TOLERANCE);
		TestEqual(TEXT("UseGAS 0: Fire resistance composed"), WoRResistance::GetTypeResistance(Target, EWoRDamageType::Fire), FireResist,
				  GETTER_TOLERANCE);
		UWoREffectMirrorComponent::SetReadsFromGAS(true);
		TestTrue(TEXT("UseGAS 1: Defense from GAS"), Calc->GetDefenderFlatDefense(Target) > Defense + GETTER_TOLERANCE);
		TestTrue(TEXT("UseGAS 1: crit damage from GAS"), Calc->GetCritDamageMultiplier(Target) > CritDamage + GETTER_TOLERANCE);
		TestTrue(TEXT("UseGAS 1: Fire resistance from GAS"),
				 WoRResistance::GetTypeResistance(Target, EWoRDamageType::Fire) > FireResist + GETTER_TOLERANCE);
	});
}

#endif
