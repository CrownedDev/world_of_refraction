// GASStatGettersSpec.cpp
// GAS stage 3 (B-042yb): with `wor.Effects.UseGAS 1` the composed sub-stat getters (spell / raw damage, action /
// spell speed) and the pillar getters return their GAS attribute; with 0 they compose on the game's side. Both
// give the same value, and the debug compare still checks the attribute against the game's composition.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Character/CharacterDataComponent.h"
#include "Debug/GAS/WoRAttributeDebug.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "GAS/WoREffectMirrorComponent.h"

namespace
{
	constexpr float GETTER_TOLERANCE = 0.0001f;
	/** Small enough to stay under each attribute's clamp. */
	constexpr float GETTER_EXTRA_SPELL_DAMAGE = 0.05f;
	constexpr float GETTER_EXTRA_MIND = 1.0f;

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
		for (const FGetterCase &Case : GetterCases())
		{
			UWoREffectMirrorComponent::SetReadsFromGAS(false);
			const float OnGameSide = Case.Read(*Char);
			UWoREffectMirrorComponent::SetReadsFromGAS(true);
			TestEqual(FString::Printf(TEXT("%s: same on both paths"), Case.Label), Case.Read(*Char), OnGameSide, GETTER_TOLERANCE);
		}
		TestEqual(TEXT("UseGAS 1: attributes match the composition"), UWoRAttributeDebug::GetAttributeMismatchString(Hero), FString());
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
}

#endif
