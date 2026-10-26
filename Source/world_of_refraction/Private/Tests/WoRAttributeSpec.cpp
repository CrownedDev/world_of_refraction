// WoRAttributeSpec.cpp
// GAS stage 2 (B-041): a combatant's GAS attributes equal the stat getters they mirror, and stay
// equal when HP changes or a stat buff lands (UWoRAttributeDebug::GetAttributeMismatchString).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterDataComponent.h"
#include "Debug/GAS/WoRAttributeDebug.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 ATTRIBUTE_TEST_BUFF_ID = 9301;
	constexpr int32 ATTRIBUTE_TEST_BUFF_TURNS = 2;
	constexpr float ATTRIBUTE_TEST_BUFF_PERCENT = 20.0f;
	constexpr int32 ATTRIBUTE_TEST_DAMAGE = 7;
	constexpr float ATTRIBUTE_TEST_DRIFT = 123.0f;

	template <typename TComponent>
	TComponent *AttributeTestAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}
}

BEGIN_DEFINE_SPEC(FWoRAttributeSpec, "WoR.GAS.Attributes",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;
	UAbilitySystemComponent *ASC = nullptr;
	UCharacterDataComponent *Char = nullptr;
	AActor *Hero = nullptr;

	void TestAttributesMatch(const TCHAR *When)
	{
		const FString Mismatches = UWoRAttributeDebug::GetAttributeMismatchString(Hero);
		TestTrue(FString::Printf(TEXT("%s: attributes match [%s]"), When, *Mismatches), Mismatches.IsEmpty());
	}
END_DEFINE_SPEC(FWoRAttributeSpec)

void FWoRAttributeSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (Hero)
		{
			// Same order as ACombatCharacter: the ASC, then the sync component that fills it.
			ASC = AttributeTestAddComponent<UAbilitySystemComponent>(Hero);
			AttributeTestAddComponent<UWoRAttributeSyncComponent>(Hero);
			Char = Hero->FindComponentByClass<UCharacterDataComponent>();
		}
		TestNotNull(TEXT("SkillEffectManager"), EffectManager);
		TestNotNull(TEXT("AbilitySystemComponent"), ASC);
		TestNotNull(TEXT("CharacterDataComponent"), Char);
	});

	AfterEach([this]()
	{
		EffectManager = nullptr;
		ASC = nullptr;
		Char = nullptr;
		Hero = nullptr;
		TestWorld.Reset();
	});

	It("gives the ASC both attribute sets, filled from the getters", [this]()
	{
		if (!ASC || !Char)
		{
			return;
		}
		TestNotNull(TEXT("Vitals set"), ASC->GetSet<UWoRAttributeSet_Vitals>());
		TestNotNull(TEXT("Stats set"), ASC->GetSet<UWoRAttributeSet_Stats>());
		TestEqual(TEXT("Health = CurrentHP"), ASC->GetNumericAttribute(UWoRAttributeSet_Vitals::GetHealthAttribute()), static_cast<float>(Char->CurrentHP));
		TestTrue(TEXT("Health is set"), Char->CurrentHP > 0);
		TestAttributesMatch(TEXT("at begin play"));
	});

	It("follows HP when the character takes damage", [this]()
	{
		if (!ASC || !Char)
		{
			return;
		}
		Char->ServerTakeDamage(ATTRIBUTE_TEST_DAMAGE);
		TestEqual(TEXT("Health = CurrentHP"), ASC->GetNumericAttribute(UWoRAttributeSet_Vitals::GetHealthAttribute()), static_cast<float>(Char->CurrentHP));
		TestAttributesMatch(TEXT("after damage"));
	});

	It("follows a stat buff when it lands and when it goes", [this]()
	{
		if (!ASC || !Char || !EffectManager)
		{
			return;
		}
		const float Before = ASC->GetNumericAttribute(UWoRAttributeSet_Stats::GetSpellDamageAttribute());
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Focus"), ATTRIBUTE_TEST_BUFF_ID, ESkillEffectType::SpellDamageBuff,
																		ATTRIBUTE_TEST_BUFF_PERCENT, ATTRIBUTE_TEST_BUFF_TURNS), Hero);
		TestTrue(TEXT("Spell Damage went up"), ASC->GetNumericAttribute(UWoRAttributeSet_Stats::GetSpellDamageAttribute()) > Before);
		TestAttributesMatch(TEXT("with the buff"));

		EffectManager->RemoveAllEffects(Hero);
		TestEqual(TEXT("Spell Damage back"), ASC->GetNumericAttribute(UWoRAttributeSet_Stats::GetSpellDamageAttribute()), Before);
		TestAttributesMatch(TEXT("after the buff"));
	});

	It("reports an attribute that drifted from its getter", [this]()
	{
		if (!ASC)
		{
			return;
		}
		ASC->SetNumericAttributeBase(UWoRAttributeSet_Stats::GetLuckAttribute(), ATTRIBUTE_TEST_DRIFT);
		TestTrue(TEXT("Luck is named"), UWoRAttributeDebug::GetAttributeMismatchString(Hero).Contains(TEXT("Luck")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
