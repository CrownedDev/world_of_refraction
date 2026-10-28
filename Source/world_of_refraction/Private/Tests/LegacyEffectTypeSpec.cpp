// LegacyEffectTypeSpec.cpp
// B-268: authorable effect types that did nothing. A legacy CritDebuff / EnergyDebuff is stored as the type
// that's read (CritChanceDebuff / EnergyDrain), so its crit or energy read changes. The retired generic Speed
// Buff / Debuff (D-056) are refused by EffectDefinition validation; the legacy aliases only warn.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Skills/Effects/SkillEffectManager.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "Skills/Effects/EffectDefinition.h"
#endif

namespace
{
	constexpr int32 LEGACY_TEST_EFFECT_ID = 9268;
	constexpr float LEGACY_TEST_VALUE = 30.0f;
	constexpr int32 LEGACY_TEST_TURNS = 3;

#if WITH_EDITOR
	EDataValidationResult ValidateWith(ESkillEffectType Type)
	{
		UEffectDefinition *Definition = NewObject<UEffectDefinition>(GetTransientPackage());
		FSkillEffect Effect;
		FSkillEffectPayload Payload;
		Payload.EffectType = Type;
		Effect.Payloads.Add(Payload);
		Definition->Effects.Add(Effect);
		FDataValidationContext Context;
		return Definition->IsDataValid(Context);
	}
#endif
}

BEGIN_DEFINE_SPEC(FLegacyEffectTypeSpec, "WoR.Effects.LegacyTypes",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FLegacyEffectTypeSpec)

void FLegacyEffectTypeSpec::Define()
{
	It("stores a legacy CritDebuff / EnergyDebuff as the type that's read", [this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		if (!TestTrue(TEXT("Test world created"), TestWorld->Create())) return;
		USkillEffectManager *EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		AActor *Holder = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("SkillEffectManager"), EffectManager) || !TestNotNull(TEXT("Holder"), Holder)) return;

		const TPair<ESkillEffectType, ESkillEffectType> Aliases[] = {
			{ESkillEffectType::CritDebuff, ESkillEffectType::CritChanceDebuff},
			{ESkillEffectType::EnergyDebuff, ESkillEffectType::EnergyDrain},
		};
		int32 Offset = 0;
		for (const TPair<ESkillEffectType, ESkillEffectType> &Alias : Aliases)
		{
			EffectManager->ApplyEffect(Holder,
									   FActiveSkillEffect::CreateBuff(TEXT("Legacy"), LEGACY_TEST_EFFECT_ID + Offset++, Alias.Key,
																	  LEGACY_TEST_VALUE, LEGACY_TEST_TURNS),
									   Holder, TEXT("LegacyTest"));
			const FString Name = UEnum::GetValueAsString(Alias.Key);
			TestEqual(Name + TEXT(" stored as its alias"), EffectManager->GetEffectsByType(Holder, Alias.Value).Num(), 1);
			TestEqual(Name + TEXT(" not stored as itself"), EffectManager->GetEffectsByType(Holder, Alias.Key).Num(), 0);
		}
		TestEqual(TEXT("the crit read sees it"), EffectManager->GetTotalStatModifier(Holder, ESkillEffectType::CritChanceDebuff),
				  LEGACY_TEST_VALUE);
		TestWorld.Reset();
	});

	It("retires only the generic Speed Buff / Debuff (D-056)", [this]()
	{
		TestTrue(TEXT("SpeedBuff retired"), SkillEffectClassification::IsRetired(ESkillEffectType::SpeedBuff));
		TestTrue(TEXT("SpeedDebuff retired"), SkillEffectClassification::IsRetired(ESkillEffectType::SpeedDebuff));
		TestFalse(TEXT("TurnSpeedDebuff not retired"), SkillEffectClassification::IsRetired(ESkillEffectType::TurnSpeedDebuff));
		TestEqual(TEXT("a current type has no alias"), SkillEffectClassification::ResolveLegacyAlias(ESkillEffectType::DefenseDebuff),
				  ESkillEffectType::DefenseDebuff);
	});

#if WITH_EDITOR
	It("refuses a retired type in an effect definition and lets a legacy alias through with a warning", [this]()
	{
		TestEqual(TEXT("SpeedDebuff refused"), ValidateWith(ESkillEffectType::SpeedDebuff), EDataValidationResult::Invalid);
		TestEqual(TEXT("SpeedBuff refused"), ValidateWith(ESkillEffectType::SpeedBuff), EDataValidationResult::Invalid);
		TestNotEqual(TEXT("CritDebuff allowed"), ValidateWith(ESkillEffectType::CritDebuff), EDataValidationResult::Invalid);
		TestNotEqual(TEXT("TurnSpeedDebuff allowed"), ValidateWith(ESkillEffectType::TurnSpeedDebuff), EDataValidationResult::Invalid);
	});
#endif
}

#endif
