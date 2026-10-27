// EffectSpecBuilderSpec.cpp
// GAS stage 3 (B-042b): every context is an FWoREffectContext; FWoREffectSpecBuilder builds each
// ported family from an enum-path effect (tags, magnitude, DOT / HOT ticks through the tracker),
// replaces the same identity, keeps other sources apart, and sums stat modifiers the way
// USkillEffectManager::GetTotalStatModifier does.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoREffectContext.h"
#include "GAS/WoREffectSpecBuilder.h"
#include "GAS/WoRTurnEffectTracker.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Tags/WoRGameplayTags.h"

namespace
{
	constexpr int32 BUILDER_TURNS = 2;
	constexpr int32 BUILDER_EFFECT_ID = 4201;
	constexpr int32 BUILDER_SOURCE_A = 11;
	constexpr int32 BUILDER_SOURCE_B = 22;
	constexpr float BUILDER_START_HEALTH = 100.0f;
	constexpr float BUILDER_BUFF = 20.0f;
	constexpr float BUILDER_STRONGER_BUFF = 30.0f;
	constexpr float BUILDER_DOT = 5.0f;
	constexpr float BUILDER_HOT = 7.0f;

	FActiveSkillEffect MakeBuilderEffect(ESkillEffectType Type, float Value, int32 SourceKey = BUILDER_SOURCE_A)
	{
		FActiveSkillEffect Effect;
		Effect.EffectName = UEnum::GetValueAsString(Type);
		Effect.EffectType = Type;
		Effect.EffectValue = Value;
		Effect.EffectID = BUILDER_EFFECT_ID;
		Effect.SourceKey = SourceKey;
		Effect.RemainingTurns = BUILDER_TURNS;
		return Effect;
	}
}

BEGIN_DEFINE_SPEC(FEffectSpecBuilderSpec, "WoR.GAS.EffectBuilder",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UAbilitySystemComponent *ASC = nullptr;
	UWoRTurnEffectTracker *Tracker = nullptr;
	AActor *Hero = nullptr;

	float Health() const { return ASC->GetNumericAttribute(UWoRAttributeSet_Vitals::GetHealthAttribute()); }
	bool Has(const FGameplayTag &Tag) const { return ASC->HasMatchingGameplayTag(Tag); }
END_DEFINE_SPEC(FEffectSpecBuilderSpec)

void FEffectSpecBuilderSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (Hero)
		{
			ASC = FWoRTestWorld::AddComponent<UAbilitySystemComponent>(Hero);
			ASC->InitAbilityActorInfo(Hero, Hero);
			ASC->AddAttributeSetSubobject(NewObject<UWoRAttributeSet_Vitals>(Hero));
			ASC->SetNumericAttributeBase(UWoRAttributeSet_Vitals::GetHealthAttribute(), BUILDER_START_HEALTH);
			Tracker = FWoRTestWorld::AddComponent<UWoRTurnEffectTracker>(Hero);
		}
		TestNotNull(TEXT("AbilitySystemComponent"), ASC);
		TestNotNull(TEXT("Tracker"), Tracker);
	});

	AfterEach([this]()
	{
		ASC = nullptr;
		Tracker = nullptr;
		Hero = nullptr;
		TestWorld.Reset();
	});

	It("makes every effect context an FWoREffectContext", [this]()
	{
		if (!ASC)
		{
			return;
		}
		TestNotNull(TEXT("WoR context (DefaultGame.ini globals class)"), FWoREffectContext::Get(ASC->MakeEffectContext()));
	});

	It("grants the status and immunity tags", [this]()
	{
		if (!ASC)
		{
			return;
		}
		TestTrue(TEXT("Stun applied"), FWoREffectSpecBuilder::Apply(Hero, MakeBuilderEffect(ESkillEffectType::Stun, 0.0f), Hero).IsValid());
		TestTrue(TEXT("Silence applied"), FWoREffectSpecBuilder::Apply(Hero, MakeBuilderEffect(ESkillEffectType::Silenced, 0.0f, BUILDER_SOURCE_B), Hero).IsValid());
		FActiveSkillEffect Immunity = MakeBuilderEffect(ESkillEffectType::GrantFireImmunity, 0.0f);
		Immunity.EffectID = BUILDER_EFFECT_ID + 1;
		TestTrue(TEXT("Immunity applied"), FWoREffectSpecBuilder::Apply(Hero, Immunity, Hero).IsValid());
		TestTrue(TEXT("Status.Stun"), Has(WoRTags::Status_Stun));
		TestTrue(TEXT("Status.Silence"), Has(WoRTags::Status_Silence));
		TestTrue(TEXT("Immunity.Element.Fire"), Has(WoRTags::Immunity_Element_Fire));
		TestFalse(TEXT("No HealBlock"), Has(WoRTags::Status_HealBlock));
	});

	It("sums a stat modifier the way the effect manager does", [this]()
	{
		USkillEffectManager *EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		if (!ASC || !EffectManager)
		{
			return;
		}
		const FActiveSkillEffect Buff = MakeBuilderEffect(ESkillEffectType::DefenseBuff, BUILDER_BUFF);
		FWoREffectSpecBuilder::Apply(Hero, Buff, Hero);
		EffectManager->ApplyEffect(Hero, Buff, Hero);
		TestEqual(TEXT("GAS sum = enum sum"), FWoREffectSpecBuilder::SumMagnitude(*ASC, ESkillEffectType::DefenseBuff),
				  EffectManager->GetTotalStatModifier(Hero, ESkillEffectType::DefenseBuff));
		TestEqual(TEXT("The buff's value"), FWoREffectSpecBuilder::SumMagnitude(*ASC, ESkillEffectType::DefenseBuff), BUILDER_BUFF);
	});

	It("replaces the same identity and keeps another source apart", [this]()
	{
		if (!ASC)
		{
			return;
		}
		FWoREffectSpecBuilder::Apply(Hero, MakeBuilderEffect(ESkillEffectType::DefenseBuff, BUILDER_BUFF), Hero);
		FWoREffectSpecBuilder::Apply(Hero, MakeBuilderEffect(ESkillEffectType::DefenseBuff, BUILDER_STRONGER_BUFF), Hero);
		TestEqual(TEXT("Replaced, not added"), FWoREffectSpecBuilder::SumMagnitude(*ASC, ESkillEffectType::DefenseBuff), BUILDER_STRONGER_BUFF);
		TestEqual(TEXT("One tracked GE"), Tracker->NumTracked(), 1);
		FWoREffectSpecBuilder::Apply(Hero, MakeBuilderEffect(ESkillEffectType::DefenseBuff, BUILDER_BUFF, BUILDER_SOURCE_B), Hero);
		TestEqual(TEXT("Another source adds"), FWoREffectSpecBuilder::SumMagnitude(*ASC, ESkillEffectType::DefenseBuff), BUILDER_STRONGER_BUFF + BUILDER_BUFF);
		TestEqual(TEXT("Two tracked GEs"), Tracker->NumTracked(), 2);
	});

	It("pays a DOT and a HOT at each holder turn start it lasts", [this]()
	{
		if (!ASC)
		{
			return;
		}
		FWoREffectSpecBuilder::Apply(Hero, MakeBuilderEffect(ESkillEffectType::DOT, BUILDER_DOT), Hero);
		TestTrue(TEXT("Status.DOT"), Has(WoRTags::Status_DOT));
		Tracker->OnHolderTurnStart();
		TestEqual(TEXT("One DOT tick"), Health(), BUILDER_START_HEALTH - BUILDER_DOT);
		Tracker->OnHolderTurnEnd();

		FActiveSkillEffect Heal = MakeBuilderEffect(ESkillEffectType::HealthRestore, BUILDER_HOT, BUILDER_SOURCE_B);
		Heal.EffectID = BUILDER_EFFECT_ID + 1;
		FWoREffectSpecBuilder::Apply(Hero, Heal, Hero);
		Tracker->OnHolderTurnStart();
		TestEqual(TEXT("A DOT tick and a HOT tick"), Health(), BUILDER_START_HEALTH - 2.0f * BUILDER_DOT + BUILDER_HOT);
		Tracker->OnHolderTurnEnd();
		Tracker->OnHolderTurnStart();
		TestFalse(TEXT("The DOT ended after its 2 turns"), Has(WoRTags::Status_DOT));
		TestEqual(TEXT("No tick from the ended DOT"), Health(), BUILDER_START_HEALTH - 2.0f * BUILDER_DOT + 2.0f * BUILDER_HOT);
	});

	It("marks gear effects and applies a permanent one untracked", [this]()
	{
		if (!ASC)
		{
			return;
		}
		FActiveSkillEffect Gear = MakeBuilderEffect(ESkillEffectType::LuckBuff, BUILDER_BUFF);
		Gear.bFromGear = true;
		Gear.bPermanent = true;
		const FActiveGameplayEffectHandle Handle = FWoREffectSpecBuilder::Apply(Hero, Gear, Hero);
		const FActiveGameplayEffect *Active = ASC->GetActiveGameplayEffect(Handle);
		TestNotNull(TEXT("Applied"), Active);
		TestTrue(TEXT("Effect.Origin.Gear"), Active && Active->Spec.GetDynamicAssetTags().HasTagExact(WoRTags::Effect_Origin_Gear));
		TestEqual(TEXT("Not turn-counted"), Tracker->NumTracked(), 0);
		const FWoREffectContext *Context = Active ? FWoREffectContext::Get(Active->Spec.GetContext()) : nullptr;
		TestTrue(TEXT("Identity travels in the context"), Context && Context->Identity.bFromGear && Context->Identity.EffectID == BUILDER_EFFECT_ID);
	});

	It("builds nothing for an unported type or an instant effect", [this]()
	{
		if (!ASC)
		{
			return;
		}
		TestFalse(TEXT("Unported type"), FWoREffectSpecBuilder::Apply(Hero, MakeBuilderEffect(ESkillEffectType::ExtraAction, 1.0f), Hero).IsValid());
		FActiveSkillEffect Instant = MakeBuilderEffect(ESkillEffectType::DefenseBuff, BUILDER_BUFF);
		Instant.RemainingTurns = 0;
		TestFalse(TEXT("Instant"), FWoREffectSpecBuilder::Apply(Hero, Instant, Hero).IsValid());
		TestEqual(TEXT("Nothing on the ASC"), FWoREffectSpecBuilder::GetDebugString(*ASC), FString(TEXT("no active GameplayEffects")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
