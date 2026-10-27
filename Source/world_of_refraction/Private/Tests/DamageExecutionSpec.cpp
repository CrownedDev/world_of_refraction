// DamageExecutionSpec.cpp
// GAS stage 3 (B-042e): UWoRDamageExecution reads the attacker's crit attributes and the
// defender's Defense and element resistance from the ASCs and runs B-219's core: the same damage
// as UDamageCalculator for the same gathered request, and applied as Health -= that damage.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterData.h"
#include "Combat/Resistance/EWoRDamageType.h"
#include "Combat/Damage/DamageCalculator.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "GAS/WoRDamageExecution.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 EXECUTION_BASE_DAMAGE = 100;
	constexpr int32 EXECUTION_DEFENSE_POINTS = 50;
	constexpr int32 EXECUTION_RESIST_ID = 4501;
	/** A fraction, as the Resist_* attributes hold. */
	constexpr float EXECUTION_ICE_RESIST = 0.15f;
	constexpr int32 EXECUTION_TURNS = 3;
	/** A percent, as ResistanceBuff values are. */
	constexpr float EXECUTION_RESIST_PERCENT = 20.0f;
	constexpr float EXECUTION_FORCED_CRIT_CHANCE = 1.0f;
}

BEGIN_DEFINE_SPEC(FDamageExecutionSpec, "WoR.GAS.DamageExecution",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UDamageCalculator *Calculator = nullptr;
	AActor *Attacker = nullptr;
	AActor *Defender = nullptr;
	UAbilitySystemComponent *AttackerASC = nullptr;
	UAbilitySystemComponent *DefenderASC = nullptr;

	/** ASC + attribute sync: the attributes hold what the getters return (B-041). */
	static UAbilitySystemComponent *GiveAttributes(AActor *Actor)
	{
		UAbilitySystemComponent *ASC = FWoRTestWorld::AddComponent<UAbilitySystemComponent>(Actor);
		FWoRTestWorld::AddComponent<UWoRAttributeSyncComponent>(Actor);
		return ASC;
	}

	static FDamageCalculationInput Hit(EActionType ActionType, ESpellElement Element)
	{
		FDamageCalculationInput Input;
		Input.BaseDamage = EXECUTION_BASE_DAMAGE;
		Input.ActionType = ActionType;
		Input.Element = Element;
		return Input;
	}

	/** The execution's damage for Request, over the ASCs' attributes. */
	FDamageCalculationResult GASDamage(const FWoRDamageRequest &Request) const
	{
		return WoRDamageMath::Compute(UWoRDamageExecution::ReadAttributes(Request, AttackerASC, DefenderASC));
	}
END_DEFINE_SPEC(FDamageExecutionSpec)

void FDamageExecutionSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Calculator = TestWorld->GetSubsystem<UDamageCalculator>();
		UCharacterData *DefenderData = FWoRTestWorld::NewCharacterData();
		DefenderData->Defense = EXECUTION_DEFENSE_POINTS;
		Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Defender = TestWorld->SpawnCombatant(DefenderData, true);
		if (Attacker && Defender)
		{
			AttackerASC = GiveAttributes(Attacker);
			DefenderASC = GiveAttributes(Defender);
			// A Fire resistance, so the element step reads a real Resist_Fire attribute.
			if (USkillEffectManager *SEM = TestWorld->GetSubsystem<USkillEffectManager>())
			{
				FActiveSkillEffect Resist = FActiveSkillEffect::CreateBuff(TEXT("Execution fire resist"), EXECUTION_RESIST_ID,
																		   ESkillEffectType::ResistanceBuff, EXECUTION_RESIST_PERCENT,
																		   EXECUTION_TURNS);
				Resist.Element = ESpellElement::Fire;
				SEM->ApplyEffect(Defender, Resist, Defender);
			}
		}
		if (!TestNotNull(TEXT("DamageCalculator"), Calculator) || !TestNotNull(TEXT("Attacker ASC"), AttackerASC) ||
			!TestNotNull(TEXT("Defender ASC"), DefenderASC))
		{
			Calculator = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		Calculator = nullptr;
		Attacker = nullptr;
		Defender = nullptr;
		AttackerASC = nullptr;
		DefenderASC = nullptr;
		TestWorld.Reset();
	});

	It("deals the same damage as the damage calculator for the same request", [this]()
	{
		if (!Calculator) return;
		for (const EActionType ActionType : {EActionType::Ability, EActionType::Spell})
		{
			for (const ESpellElement Element : {ESpellElement::None, ESpellElement::Fire, ESpellElement::Water})
			{
				const FString Case = FString::Printf(TEXT("%s %s"), *UEnum::GetValueAsString(ActionType), *UEnum::GetValueAsString(Element));
				const FWoRDamageRequest Request = UWoRDamageExecution::MakeRequest(*Calculator, Attacker, Defender, Hit(ActionType, Element));
				const FDamageCalculationResult Expected = WoRDamageMath::Compute(Request.Inputs);
				const FDamageCalculationResult Actual = GASDamage(Request);
				TestEqual(*(Case + TEXT(": damage")), Actual.FinalDamage, Expected.FinalDamage);
				TestEqual(*(Case + TEXT(": crit")), Actual.bWasCritical, Expected.bWasCritical);
				TestEqual(*(Case + TEXT(": blocked")), Actual.DamageBlockedByDefense, Expected.DamageBlockedByDefense);
			}
		}
		const FWoRDamageRequest Fire = UWoRDamageExecution::MakeRequest(*Calculator, Attacker, Defender, Hit(EActionType::Spell, ESpellElement::Fire));
		TestTrue(TEXT("The Fire case reads a real resistance"), Fire.Inputs.ElementResistance > 0.0f);
		TestTrue(TEXT("Defense is on"), Fire.Inputs.bApplyDefense && Fire.Inputs.DefenderDefense > 0.0f);
	});

	It("reads the attributes, not the gathered numbers", [this]()
	{
		if (!Calculator) return;
		FWoRDamageRequest Request = UWoRDamageExecution::MakeRequest(*Calculator, Attacker, Defender, Hit(EActionType::Spell, ESpellElement::Fire));
		const float GatheredDefense = Request.Inputs.DefenderDefense;
		Request.Inputs.DefenderDefense = 0.0f;
		Request.Inputs.ElementResistance = 0.0f;
		const FDamageMathInputs Read = UWoRDamageExecution::ReadAttributes(Request, AttackerASC, DefenderASC);
		TestEqual(TEXT("Defense from the Defense attribute"), Read.DefenderDefense, GatheredDefense);
		TestTrue(TEXT("Resistance from Resist_Fire"), Read.ElementResistance > 0.0f);

		// Ice also resists Water (Q-145b). Nothing grants Ice resistance today, so set the attribute.
		const FWoRDamageRequest Water = UWoRDamageExecution::MakeRequest(*Calculator, Attacker, Defender, Hit(EActionType::Spell, ESpellElement::Water));
		DefenderASC->SetNumericAttributeBase(UWoRAttributeSet_Resistance::GetTypeAttribute(EWoRDamageType::Ice), EXECUTION_ICE_RESIST);
		const float ReadWater = UWoRDamageExecution::ReadAttributes(Water, AttackerASC, DefenderASC).ElementResistance;
		TestEqual(TEXT("Water: Resist_Water + Resist_Ice"), ReadWater, Water.Inputs.ElementResistance + EXECUTION_ICE_RESIST);
	});

	It("keeps the gathered numbers when the attributes aren't synced", [this]()
	{
		if (!Calculator) return;
		// An ASC holding the sets but no attribute sync: its attributes are 0, not the getters' values.
		AActor *Unsynced = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		UAbilitySystemComponent *UnsyncedASC = Unsynced ? FWoRTestWorld::AddComponent<UAbilitySystemComponent>(Unsynced) : nullptr;
		if (!TestNotNull(TEXT("Unsynced ASC"), UnsyncedASC)) return;
		UnsyncedASC->AddAttributeSetSubobject(NewObject<UWoRAttributeSet_Stats>(Unsynced));
		UnsyncedASC->AddAttributeSetSubobject(NewObject<UWoRAttributeSet_Resistance>(Unsynced));
		const FWoRDamageRequest Request = UWoRDamageExecution::MakeRequest(*Calculator, Attacker, Defender, Hit(EActionType::Spell, ESpellElement::Fire));
		const FDamageMathInputs Read = UWoRDamageExecution::ReadAttributes(Request, UnsyncedASC, UnsyncedASC);
		TestEqual(TEXT("Crit damage kept"), Read.CritDamageMultiplier, Request.Inputs.CritDamageMultiplier);
		TestEqual(TEXT("Defense kept"), Read.DefenderDefense, Request.Inputs.DefenderDefense);
		TestEqual(TEXT("Resistance kept"), Read.ElementResistance, Request.Inputs.ElementResistance);
	});

	It("keeps an overridden crit chance and an ignored resistance", [this]()
	{
		if (!Calculator) return;
		FDamageCalculationInput Input = Hit(EActionType::Spell, ESpellElement::Fire);
		Input.OverrideCritChance = EXECUTION_FORCED_CRIT_CHANCE;
		Input.bIgnoreResistance = true;
		const FWoRDamageRequest Request = UWoRDamageExecution::MakeRequest(*Calculator, Attacker, Defender, Input);
		const FDamageMathInputs Read = UWoRDamageExecution::ReadAttributes(Request, AttackerASC, DefenderASC);
		TestEqual(TEXT("Override kept"), Read.CritChance, EXECUTION_FORCED_CRIT_CHANCE);
		TestEqual(TEXT("No resistance"), Read.ElementResistance, 0.0f);
	});

	It("takes the damage off Health when the spec is applied", [this]()
	{
		if (!Calculator) return;
		const FWoRDamageRequest Request = UWoRDamageExecution::MakeRequest(*Calculator, Attacker, Defender, Hit(EActionType::Ability, ESpellElement::Fire));
		const int32 Damage = GASDamage(Request).FinalDamage;
		const FGameplayAttribute Health = UWoRAttributeSet_Vitals::GetHealthAttribute();
		const float Before = DefenderASC->GetNumericAttribute(Health);
		const FGameplayEffectSpecHandle Spec = UWoRDamageExecution::MakeDamageSpec(*AttackerASC, Request);
		if (!TestTrue(TEXT("Spec built"), Spec.IsValid())) return;
		AttackerASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), DefenderASC);
		TestTrue(TEXT("Some damage"), Damage > 0);
		TestEqual(TEXT("Health dropped by the damage"), DefenderASC->GetNumericAttribute(Health), Before - Damage);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
