// GASEffectsByTypeSpec.cpp
// B-042g (part 1): USkillEffectManager::GetEffectsByType reads the GEs, and its views read the same as the stored
// copies: keys (element), one application's value (the Ignore Defense flag test) and the stacked value. B-042g2: the
// stored side is read straight from the store (StoredOfType), the switch being gone.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Debug/GAS/WoREffectDebug.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 BY_TYPE_TEST_FLAG_ID = 9421;
	constexpr int32 BY_TYPE_TEST_REFLECT_ID = 9422;
	/** An Ignore Defense effect authored as a flag (D-086: 1 = on). */
	constexpr float BY_TYPE_TEST_FLAG_VALUE = 1.0f;
	constexpr int32 BY_TYPE_TEST_STACKS = 2;
	constexpr float BY_TYPE_TEST_REFLECT_VALUE = 20.0f;
	constexpr int32 BY_TYPE_TEST_TURNS = 3;
	/** A weaker application stacking onto a stronger one keeps the stacked value (20 x 1 = 10 x 2). */
	constexpr float BY_TYPE_TEST_STRONG_VALUE = 20.0f;
	constexpr float BY_TYPE_TEST_WEAK_VALUE = 10.0f;
	constexpr int32 BY_TYPE_TEST_WEAK_ID = 9423;
}

BEGIN_DEFINE_SPEC(FGASEffectsByTypeSpec, "WoR.GAS.EffectsByType", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *Effects = nullptr;
	AActor *Holder = nullptr;

	/** The store's copies of Type, read directly (GetEffectsByType reads the GEs). */
	TArray<FActiveSkillEffect> StoredOfType(ESkillEffectType Type) const
	{
		return Effects->GetActiveEffects(Holder).FilterByPredicate([Type](const FActiveSkillEffect &Effect) { return Effect.EffectType == Type; });
	}
END_DEFINE_SPEC(FGASEffectsByTypeSpec)

void FGASEffectsByTypeSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		Holder = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		FWoRTestWorld::AddEffectMirror(Holder);
		if (!TestNotNull(TEXT("effects"), Effects) || !TestNotNull(TEXT("holder"), Holder))
		{
			Effects = nullptr;
			return;
		}
		FActiveSkillEffect Flag = FActiveSkillEffect::CreateBuff(TEXT("By-type flag"), BY_TYPE_TEST_FLAG_ID, ESkillEffectType::IgnoreDefense,
																  BY_TYPE_TEST_FLAG_VALUE, BY_TYPE_TEST_TURNS);
		Flag.bCanStack = true;
		Flag.MaxStacks = BY_TYPE_TEST_STACKS;
		for (int32 Index = 0; Index < BY_TYPE_TEST_STACKS; ++Index)
		{
			Effects->ApplyEffect(Holder, Flag);
		}
		FActiveSkillEffect Reflect = FActiveSkillEffect::CreateBuff(TEXT("By-type reflect"), BY_TYPE_TEST_REFLECT_ID,
																	 ESkillEffectType::DamageReflect, BY_TYPE_TEST_REFLECT_VALUE, BY_TYPE_TEST_TURNS);
		Reflect.Element = ESpellElement::Fire;
		Effects->ApplyEffect(Holder, Reflect);
	});

	AfterEach([this]()
	{
		Effects = nullptr;
		Holder = nullptr;
		TestWorld.Reset();
	});

	It("reads the same views on GAS as the store holds", [this]()
	{
		if (!Effects)
		{
			return;
		}
		using FViews = TPair<TArray<FActiveSkillEffect>, TArray<FActiveSkillEffect>>;
		const FViews Enum(StoredOfType(ESkillEffectType::IgnoreDefense), StoredOfType(ESkillEffectType::DamageReflect));
		const FViews GAS(Effects->GetEffectsByType(Holder, ESkillEffectType::IgnoreDefense),
						 Effects->GetEffectsByType(Holder, ESkillEffectType::DamageReflect));
		TestEqual(TEXT("the mirror matches"), UWoREffectDebug::GetEffectMismatchString(Holder), FString());
		if (!TestEqual(TEXT("one flag effect on GAS"), GAS.Key.Num(), 1) || !TestEqual(TEXT("one reflect on GAS"), GAS.Value.Num(), 1) ||
			!TestEqual(TEXT("one flag effect stored"), Enum.Key.Num(), 1) || !TestEqual(TEXT("one reflect stored"), Enum.Value.Num(), 1))
		{
			return;
		}
		TestEqual(TEXT("the flag reads as one application of 1 (D-086)"), GAS.Key[0].EffectValue, BY_TYPE_TEST_FLAG_VALUE);
		TestEqual(TEXT("its stacked value, as stored"), GAS.Key[0].GetStackedValue(), Enum.Key[0].GetStackedValue());
		TestEqual(TEXT("the reflect keeps its element"), GAS.Value[0].Element, ESpellElement::Fire);
		TestEqual(TEXT("and its value"), GAS.Value[0].GetStackedValue(), Enum.Value[0].GetStackedValue());
	});

	It("follows a weaker application stacking on (the same stacked value, a new per-application value)", [this]()
	{
		if (!Effects)
		{
			return;
		}
		FActiveSkillEffect Strong = FActiveSkillEffect::CreateBuff(TEXT("By-type stack"), BY_TYPE_TEST_WEAK_ID, ESkillEffectType::ApplyDOTOnHit,
																	BY_TYPE_TEST_STRONG_VALUE, BY_TYPE_TEST_TURNS);
		Strong.bCanStack = true;
		Strong.MaxStacks = BY_TYPE_TEST_STACKS;
		Effects->ApplyEffect(Holder, Strong);
		Effects->GetEffectsByType(Holder, ESkillEffectType::ApplyDOTOnHit); // result unused: this read syncs the mirror so the GE exists before the weaker apply
		FActiveSkillEffect Weak = Strong;
		Weak.EffectValue = BY_TYPE_TEST_WEAK_VALUE;
		Effects->ApplyEffect(Holder, Weak);
		const TArray<FActiveSkillEffect> Stored = StoredOfType(ESkillEffectType::ApplyDOTOnHit);
		const TArray<FActiveSkillEffect> OnGAS = Effects->GetEffectsByType(Holder, ESkillEffectType::ApplyDOTOnHit);
		if (TestEqual(TEXT("one stored"), Stored.Num(), 1) && TestEqual(TEXT("one on GAS"), OnGAS.Num(), 1))
		{
			TestEqual(TEXT("the per-application value follows the store"), OnGAS[0].EffectValue, Stored[0].EffectValue);
			TestEqual(TEXT("and the stacked value"), OnGAS[0].GetStackedValue(), Stored[0].GetStackedValue());
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
