// SignAwareModifierSpec.cpp
// B-267 (D-054, Q-267): Turn Speed and Energy Cost modifiers take their direction from the side. On an ally
// the modifier is a buff (faster turns, cheaper skills) and survives a debuff cleanse; on an enemy it's a
// debuff (slower turns, dearer skills) and a debuff cleanse removes it.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/TurnManager.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Skills/Effects/FSkillEffectPayload.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 SIGN_TEST_EFFECT_ID = 9267;
	constexpr float SIGN_TEST_SIZE = 20.0f;
	constexpr int32 SIGN_TEST_TURNS = 3;
}

BEGIN_DEFINE_SPEC(FSignAwareModifierSpec, "WoR.Effects.SignAwareModifiers",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;
	UTurnManager *TurnManager = nullptr;
	AActor *Caster = nullptr;
	AActor *Ally = nullptr;
	AActor *Enemy = nullptr;

	void ApplyModifier(ESkillEffectType Type, AActor *Target)
	{
		EffectManager->ApplyEffect(Target,
								   FActiveSkillEffect::CreateBuff(TEXT("Sign test"), SIGN_TEST_EFFECT_ID, Type, SIGN_TEST_SIZE, SIGN_TEST_TURNS),
								   Caster, TEXT("SignTest"));
	}
END_DEFINE_SPEC(FSignAwareModifierSpec)

void FSignAwareModifierSpec::Define()
{
	It("classifies the three sign-aware types by sign, and no others", [this]()
	{
		for (const ESkillEffectType Type :
			 {ESkillEffectType::ModifyTurnSpeed, ESkillEffectType::ModifyEnergyCost, ESkillEffectType::ModifyStatusResist})
		{
			const FString Name = UEnum::GetValueAsString(Type);
			TestTrue(Name + TEXT(" is sign-aware"), SkillEffectClassification::IsSignAware(Type));
			TestTrue(Name + TEXT(" + is a buff"), SkillEffectClassification::IsBuff(Type, SIGN_TEST_SIZE));
			TestFalse(Name + TEXT(" + is not a debuff"), SkillEffectClassification::IsDebuff(Type, SIGN_TEST_SIZE));
			TestTrue(Name + TEXT(" - is a debuff"), SkillEffectClassification::IsDebuff(Type, -SIGN_TEST_SIZE));
			TestFalse(Name + TEXT(" - is not a buff"), SkillEffectClassification::IsBuff(Type, -SIGN_TEST_SIZE));
			TestFalse(Name + TEXT(" 0 is neither"),
					  SkillEffectClassification::IsBuff(Type, 0.0f) || SkillEffectClassification::IsDebuff(Type, 0.0f));
		}
		TestFalse(TEXT("TurnSpeedDebuff isn't sign-aware"), SkillEffectClassification::IsSignAware(ESkillEffectType::TurnSpeedDebuff));
		TestTrue(TEXT("TurnSpeedDebuff stays a debuff"), SkillEffectClassification::IsDebuff(ESkillEffectType::TurnSpeedDebuff, SIGN_TEST_SIZE));
	});

	It("classifies an authored payload by its target: aimed at an enemy it reads as a debuff", [this]()
	{
		for (const ESkillEffectType Type : {ESkillEffectType::ModifyEnergyCost, ESkillEffectType::ModifyTurnSpeed})
		{
			FSkillEffectPayload Payload;
			Payload.EffectType = Type;
			Payload.Value = static_cast<int32>(SIGN_TEST_SIZE);
			Payload.Target = ETargetType::Ally;
			TestTrue(UEnum::GetValueAsString(Type) + TEXT(" on an ally: buff"),
					 SkillEffectClassification::IsBuff(Type, Payload.GetClassifyingMagnitude()));
			Payload.Target = ETargetType::Enemy;
			TestTrue(UEnum::GetValueAsString(Type) + TEXT(" on an enemy: debuff"),
					 SkillEffectClassification::IsDebuff(Type, Payload.GetClassifyingMagnitude()));
		}
	});

	Describe("in a fight", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("Test world created"), TestWorld->Create());
			EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
			TurnManager = TestWorld->GetSubsystem<UTurnManager>();
			Caster = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			Ally = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			Enemy = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			if (!TestNotNull(TEXT("SkillEffectManager"), EffectManager) || !TestNotNull(TEXT("TurnManager"), TurnManager))
			{
				EffectManager = nullptr; // each case returns early
				return;
			}
			TurnManager->InitializeCombat({Caster, Ally}, {Enemy});
		});

		AfterEach([this]()
		{
			if (TurnManager && TurnManager->IsCombatActive())
			{
				TurnManager->EndCombat();
			}
			TestWorld.Reset();
			EffectManager = nullptr;
			TurnManager = nullptr;
			Caster = Ally = Enemy = nullptr;
		});

		It("signs by a passed team when there's no source actor (gear reactions)", [this]()
		{
			if (!EffectManager) return;
			EffectManager->ApplyEffect(Enemy,
									   FActiveSkillEffect::CreateBuff(TEXT("Sign test"), SIGN_TEST_EFFECT_ID, ESkillEffectType::ModifyTurnSpeed,
																	  SIGN_TEST_SIZE, SIGN_TEST_TURNS),
									   nullptr, FString(), TurnManager->GetActorTeam(Caster));
			TestEqual(TEXT("slower"), EffectManager->GetTotalStatModifier(Enemy, ESkillEffectType::ModifyTurnSpeed), -SIGN_TEST_SIZE);
		});

		It("keeps a value authored negative on its own side (a self-drawback)", [this]()
		{
			if (!EffectManager) return;
			EffectManager->ApplyEffect(Ally,
									   FActiveSkillEffect::CreateBuff(TEXT("Sign test"), SIGN_TEST_EFFECT_ID, ESkillEffectType::ModifyTurnSpeed,
																	  -SIGN_TEST_SIZE, SIGN_TEST_TURNS),
									   Caster, TEXT("SignTest"));
			TestEqual(TEXT("still slower"), EffectManager->GetTotalStatModifier(Ally, ESkillEffectType::ModifyTurnSpeed), -SIGN_TEST_SIZE);
		});

		for (const ESkillEffectType Type : {ESkillEffectType::ModifyEnergyCost, ESkillEffectType::ModifyTurnSpeed})
		{
			const FString Name = UEnum::GetValueAsString(Type);

			It(Name + TEXT(" on an ally is a buff that a debuff cleanse leaves"), [this, Type]()
			{
				if (!EffectManager) return;
				ApplyModifier(Type, Ally);
				TestEqual(TEXT("positive: cheaper / faster"), EffectManager->GetTotalStatModifier(Ally, Type), SIGN_TEST_SIZE);
				const TArray<FActiveSkillEffect> Effects = EffectManager->GetEffectsByType(Ally, Type);
				TestTrue(TEXT("a buff"), Effects.Num() == 1 && Effects[0].IsBuff() && !Effects[0].IsDebuff());
				EffectManager->RemoveAllDebuffs(Ally);
				TestEqual(TEXT("survives the cleanse"), EffectManager->GetEffectsByType(Ally, Type).Num(), 1);
			});

			It(Name + TEXT(" on an enemy is a debuff that a debuff cleanse removes"), [this, Type]()
			{
				if (!EffectManager) return;
				ApplyModifier(Type, Enemy);
				TestEqual(TEXT("negative: dearer / slower"), EffectManager->GetTotalStatModifier(Enemy, Type), -SIGN_TEST_SIZE);
				const TArray<FActiveSkillEffect> Effects = EffectManager->GetEffectsByType(Enemy, Type);
				TestTrue(TEXT("a debuff"), Effects.Num() == 1 && Effects[0].IsDebuff() && !Effects[0].IsBuff());
				EffectManager->RemoveAllDebuffs(Enemy);
				TestEqual(TEXT("cleansed"), EffectManager->GetEffectsByType(Enemy, Type).Num(), 0);
			});
		}
	});
}

#endif
