// EffectMirrorSpec.cpp
// GAS stage 3 (B-042c): UWoREffectMirrorComponent keeps one GameplayEffect per stored, ported
// enum-path effect (value, stacks, same-source replace, removal, expiry, clear), skips instant
// effects, mirrors every effect type (B-042k), and `wor.Effects.UseGAS 1` makes the effect manager's reads use those GEs.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "Debug/GAS/WoREffectDebug.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "GAS/WoREffectSpecBuilder.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Tags/WoRGameplayTags.h"

namespace
{
	constexpr int32 MIRROR_BUFF_ID = 4301;
	constexpr int32 MIRROR_STUN_ID = 4302;
	constexpr int32 MIRROR_DOT_ID = 4303;
	constexpr int32 MIRROR_TURNS = 2;
	constexpr int32 MIRROR_MAX_STACKS = 3;
	constexpr float MIRROR_BUFF = 20.0f;
	constexpr float MIRROR_STRONGER_BUFF = 30.0f;
	constexpr float MIRROR_DOT = 5.0f;
	/** The every-type sweep's IDs start here (one per enum index) and its effects hold this value. */
	constexpr int32 MIRROR_TYPE_SWEEP_ID = 4400;
	constexpr float MIRROR_SWEEP_VALUE = 1.0f;
}

BEGIN_DEFINE_SPEC(FEffectMirrorSpec, "WoR.GAS.EffectMirror",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;
	UWoREffectMirrorComponent *Mirror = nullptr;
	UAbilitySystemComponent *ASC = nullptr;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;

	float GASTotal(ESkillEffectType Type) const { return FWoREffectSpecBuilder::SumMagnitude(*ASC, Type); }

	void TestMatches(const TCHAR *When)
	{
		TestEqual(FString::Printf(TEXT("Mirror matches the stored effects (%s)"), When),
				  UWoREffectDebug::GetEffectMismatchString(Hero), FString());
	}

	static FActiveSkillEffect Buff(float Value = MIRROR_BUFF)
	{
		return FActiveSkillEffect::CreateBuff(TEXT("Mirror buff"), MIRROR_BUFF_ID, ESkillEffectType::DefenseBuff, Value, MIRROR_TURNS);
	}
END_DEFINE_SPEC(FEffectMirrorSpec)

void FEffectMirrorSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		Mirror = FWoRTestWorld::AddEffectMirror(Hero);
		ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Hero);
		if (!TestNotNull(TEXT("SkillEffectManager"), EffectManager) || !TestNotNull(TEXT("Mirror"), Mirror) ||
			!TestNotNull(TEXT("AbilitySystemComponent"), ASC))
		{
			EffectManager = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		UWoREffectMirrorComponent::SetReadsFromGAS(false);
		EffectManager = nullptr;
		Mirror = nullptr;
		ASC = nullptr;
		Hero = nullptr;
		Foe = nullptr;
		TestWorld.Reset();
	});

	It("mirrors a stored buff with its value and drops it on removal", [this]()
	{
		if (!EffectManager) return;
		EffectManager->ApplyEffect(Hero, Buff(), Foe);
		TestEqual(TEXT("One GE"), Mirror->NumMirrored(), 1);
		TestEqual(TEXT("GAS total"), GASTotal(ESkillEffectType::DefenseBuff), MIRROR_BUFF);
		TestMatches(TEXT("applied"));

		EffectManager->RemoveEffectByID(Hero, MIRROR_BUFF_ID);
		TestEqual(TEXT("No GE"), Mirror->NumMirrored(), 0);
		TestEqual(TEXT("GAS total cleared"), GASTotal(ESkillEffectType::DefenseBuff), 0.0f);
	});

	It("follows stacks and a same-source replace", [this]()
	{
		if (!EffectManager) return;
		FActiveSkillEffect Stacking = Buff();
		Stacking.bCanStack = true;
		Stacking.MaxStacks = MIRROR_MAX_STACKS;
		EffectManager->ApplyEffect(Hero, Stacking, Foe);
		EffectManager->ApplyEffect(Hero, Stacking, Foe);
		TestEqual(TEXT("Two stacks"), GASTotal(ESkillEffectType::DefenseBuff), MIRROR_BUFF * 2);
		TestMatches(TEXT("stacked"));

		EffectManager->RemoveEffectByID(Hero, MIRROR_BUFF_ID);
		EffectManager->ApplyEffect(Hero, Buff(), Foe);
		EffectManager->ApplyEffect(Hero, Buff(MIRROR_STRONGER_BUFF), Foe);
		TestEqual(TEXT("Still one GE"), Mirror->NumMirrored(), 1);
		TestEqual(TEXT("Replaced value"), GASTotal(ESkillEffectType::DefenseBuff), MIRROR_STRONGER_BUFF);
		TestMatches(TEXT("replaced"));
	});

	It("keeps another source's copy apart", [this]()
	{
		if (!EffectManager) return;
		EffectManager->ApplyEffect(Hero, Buff(), Foe);
		EffectManager->ApplyEffect(Hero, Buff(MIRROR_STRONGER_BUFF), Hero);
		TestEqual(TEXT("Two GEs"), Mirror->NumMirrored(), 2);
		TestEqual(TEXT("Both count"), GASTotal(ESkillEffectType::DefenseBuff), MIRROR_BUFF + MIRROR_STRONGER_BUFF);
		TestMatches(TEXT("two sources"));
	});

	It("grants a stored stun's tag and drops it when the stun expires", [this]()
	{
		if (!EffectManager) return;
		FActiveSkillEffect Stun = FActiveSkillEffect::CreateBuff(TEXT("Mirror stun"), MIRROR_STUN_ID, ESkillEffectType::Stun, 0.0f, 1);
		EffectManager->ApplyEffect(Hero, Stun, Foe);
		TestTrue(TEXT("Status.Stun"), ASC->HasMatchingGameplayTag(WoRTags::Status_Stun));
		TestMatches(TEXT("stunned"));

		EffectManager->ProcessEndOfTurnEffects(Hero);
		TestFalse(TEXT("Stun stored no more"), EffectManager->GetActiveEffects(Hero).ContainsByPredicate(
			[](const FActiveSkillEffect &E) { return E.EffectType == ESkillEffectType::Stun; }));
		TestFalse(TEXT("Tag gone with it"), ASC->HasMatchingGameplayTag(WoRTags::Status_Stun));
		TestMatches(TEXT("expired"));
	});

	It("mirrors a DOT's tag and element, and clears with RemoveAllEffects", [this]()
	{
		if (!EffectManager) return;
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateDOT(TEXT("Mirror burn"), MIRROR_DOT_ID, MIRROR_DOT, MIRROR_TURNS, ESpellElement::Fire), Foe);
		EffectManager->ApplyEffect(Hero, Buff(), Foe);
		TestTrue(TEXT("Status.DOT"), ASC->HasMatchingGameplayTag(WoRTags::Status_DOT));
		TestEqual(TEXT("Fire DOT total"), FWoREffectSpecBuilder::SumMagnitudeForElement(*ASC, ESkillEffectType::DOT, ESpellElement::Fire), MIRROR_DOT);
		TestEqual(TEXT("Two GEs"), Mirror->NumMirrored(), 2);
		TestMatches(TEXT("burning"));

		EffectManager->RemoveAllEffects(Hero);
		TestEqual(TEXT("Mirror empty"), Mirror->NumMirrored(), 0);
		TestFalse(TEXT("DOT tag gone"), ASC->HasMatchingGameplayTag(WoRTags::Status_DOT));
	});

	It("skips instant effects", [this]()
	{
		if (!EffectManager) return;
		FActiveSkillEffect Instant = Buff();
		Instant.RemainingTurns = 0;
		EffectManager->ApplyEffect(Hero, Instant, Foe);
		TestEqual(TEXT("Nothing mirrored"), Mirror->NumMirrored(), 0);
		TestMatches(TEXT("instant"));
	});

	It("mirrors every effect type, and UseGAS 1 reads each the same as 0 (B-042k)", [this]()
	{
		if (!EffectManager) return;
		const UEnum *Types = StaticEnum<ESkillEffectType>();
		for (int32 Index = 0; Index < Types->NumEnums() - 1; ++Index)
		{
			const ESkillEffectType Type = static_cast<ESkillEffectType>(Types->GetValueByIndex(Index));
			if (Type == ESkillEffectType::None)
			{
				continue;
			}
			// Held, not run: no window fires before the checks (an immunity may still turn one away).
			FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(UEnum::GetValueAsString(Type), MIRROR_TYPE_SWEEP_ID + Index,
																	   Type, MIRROR_SWEEP_VALUE, MIRROR_TURNS);
			Effect.ProcessTiming = ESkillEffectTiming::EndOfOwnTurn;
			Effect.bDelayFirstExecution = true;
			EffectManager->ApplyEffect(Hero, Effect, Foe);
		}
		TestMatches(TEXT("every type"));

		for (int32 Index = 0; Index < Types->NumEnums() - 1; ++Index)
		{
			const ESkillEffectType Type = static_cast<ESkillEffectType>(Types->GetValueByIndex(Index));
			if (Type == ESkillEffectType::None)
			{
				continue;
			}
			const FString Name = UEnum::GetValueAsString(Type);
			UWoREffectMirrorComponent::SetReadsFromGAS(false);
			const bool bEnumHas = EffectManager->HasEffectOfType(Hero, Type);
			const float EnumTotal = EffectManager->GetTotalStatModifier(Hero, Type);
			UWoREffectMirrorComponent::SetReadsFromGAS(true);
			TestEqual(*FString::Printf(TEXT("%s: has"), *Name), EffectManager->HasEffectOfType(Hero, Type), bEnumHas);
			TestEqual(*FString::Printf(TEXT("%s: total"), *Name), EffectManager->GetTotalStatModifier(Hero, Type), EnumTotal);
		}
	});

	It("reads from GAS with wor.Effects.UseGAS 1 and from the enum path with 0", [this]()
	{
		if (!EffectManager) return;
		// A GE put straight on the ASC, with nothing stored: only a GAS read sees it.
		FActiveSkillEffect Stun = FActiveSkillEffect::CreateBuff(TEXT("GAS-only stun"), MIRROR_STUN_ID, ESkillEffectType::Stun, 0.0f, MIRROR_TURNS);
		ASC->ApplyGameplayEffectSpecToSelf(*FWoREffectSpecBuilder::Build(*ASC, Stun, Foe).Spec.Data.Get());
		ASC->ApplyGameplayEffectSpecToSelf(*FWoREffectSpecBuilder::Build(*ASC, Buff(), Foe).Spec.Data.Get());

		TestFalse(TEXT("UseGAS 0: not stunned"), EffectManager->IsStunned(Hero));
		TestEqual(TEXT("UseGAS 0: no buff"), EffectManager->GetTotalStatModifier(Hero, ESkillEffectType::DefenseBuff), 0.0f);

		UWoREffectMirrorComponent::SetReadsFromGAS(true);
		TestTrue(TEXT("UseGAS 1: stunned"), EffectManager->IsStunned(Hero));
		TestTrue(TEXT("UseGAS 1: HasEffectOfType"), EffectManager->HasEffectOfType(Hero, ESkillEffectType::Stun));
		TestEqual(TEXT("UseGAS 1: buff"), EffectManager->GetTotalStatModifier(Hero, ESkillEffectType::DefenseBuff), MIRROR_BUFF);
		TestEqual(TEXT("UseGAS 1: generic-element buff"),
				  EffectManager->GetTotalStatModifierForElement(Hero, ESkillEffectType::DefenseBuff, Buff().Element), MIRROR_BUFF);
		TestFalse(TEXT("UseGAS 1: an actor without a mirror stays on the enum path"), EffectManager->IsStunned(Foe));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
