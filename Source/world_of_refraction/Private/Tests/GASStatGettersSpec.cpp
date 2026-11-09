// GASStatGettersSpec.cpp
// GAS stage 3 (B-042yb): the composed sub-stat getters (spell / raw damage, action / spell speed) and the pillar
// getters return their GAS attribute, and the debug compare checks the attribute against the game's composition.
// B-042g2: the exact UseGAS 0 vs 1 comparisons went with the switch (D-140); the attribute compare stays.
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
		ASC = nullptr;
		Char = nullptr;
		Hero = nullptr;
		TestWorld.Reset();
	});

	It("reads each getter from GAS, and the attributes match the composition", [this]()
	{
		if (!ASC) return;
		TArray<FGetterCase> Cases = GetterCases();
		Cases.Append(SubsystemGetterCases());
		for (const FGetterCase &Case : Cases)
		{
			TestTrue(FString::Printf(TEXT("%s: a finite read"), Case.Label), FMath::IsFinite(Case.Read(*Char)));
		}
		TestEqual(TEXT("attributes match the composition"), UWoRAttributeDebug::GetAttributeMismatchString(Hero), FString());
	});

	It("matches the composition with a buff layer that isn't 1", [this]()
	{
		if (!ASC) return;
		USkillEffectManager *EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		if (!TestNotNull(TEXT("SkillEffectManager"), EffectManager)) return;
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Getter buff"), GETTER_BUFF_ID, ESkillEffectType::SpellDamageBuff,
																	   GETTER_BUFF_PERCENT, GETTER_BUFF_TURNS),
								   Hero);
		TestTrue(TEXT("SpellDamage with a buff: a finite read"), FMath::IsFinite(Char->GetEffectiveSpellDamage()));
		TestEqual(TEXT("attributes match the composition with a buff"), UWoRAttributeDebug::GetAttributeMismatchString(Hero), FString());
	});

	It("matches the composition with a crit chance buff layer (B-042yc)", [this]()
	{
		if (!ASC) return;
		USkillEffectManager *EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		const UDamageCalculator *Calc = TestWorld->GetSubsystem<UDamageCalculator>();
		if (!TestNotNull(TEXT("SkillEffectManager"), EffectManager) || !TestNotNull(TEXT("DamageCalculator"), Calc)) return;
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Getter crit buff"), GETTER_CRIT_BUFF_ID,
																	   ESkillEffectType::CritChanceBuff, GETTER_BUFF_PERCENT, GETTER_BUFF_TURNS),
								   Hero);
		TestTrue(TEXT("CritChance with a buff: a finite read"), FMath::IsFinite(Calc->GetCriticalChance(Hero)));
		TestEqual(TEXT("attributes match the composition with a crit buff"), UWoRAttributeDebug::GetAttributeMismatchString(Hero), FString());
	});

	It("returns the GAS attribute", [this]()
	{
		if (!ASC) return;
		const float SpellDamage = Char->GetEffectiveSpellDamage();
		const float Mind = Char->GetEvolutionModifiedMind();
		AddGASOnlyBonus(UWoRAttributeSet_Stats::GetSpellDamageAttribute(), GETTER_EXTRA_SPELL_DAMAGE);
		AddGASOnlyBonus(UWoRAttributeSet_Stats::GetMindAttribute(), GETTER_EXTRA_MIND);

		TestTrue(TEXT("SpellDamage from GAS"), Char->GetEffectiveSpellDamage() > SpellDamage + GETTER_TOLERANCE);
		TestTrue(TEXT("Mind from GAS"), Char->GetEvolutionModifiedMind() > Mind + GETTER_TOLERANCE);
		TestFalse(TEXT("the compare still reads the composition, so it sees the extra"),
				  UWoRAttributeDebug::GetAttributeMismatchString(Hero).IsEmpty());
	});

	It("returns GAS's Defense, crit damage and a resistance (B-042yc)", [this]()
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

		TestTrue(TEXT("Defense from GAS"), Calc->GetDefenderFlatDefense(Target) > Defense + GETTER_TOLERANCE);
		TestTrue(TEXT("crit damage from GAS"), Calc->GetCritDamageMultiplier(Target) > CritDamage + GETTER_TOLERANCE);
		TestTrue(TEXT("Fire resistance from GAS"),
				 WoRResistance::GetTypeResistance(Target, EWoRDamageType::Fire) > FireResist + GETTER_TOLERANCE);
	});
}

#endif
