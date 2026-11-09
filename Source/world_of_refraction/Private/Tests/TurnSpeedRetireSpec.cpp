// TurnSpeedRetireSpec.cpp
// B-170g (ActionPips.md §3.4, Q-170h, Q-170g2): the Turn Speed family is retired for new authoring. Data validation
// warns (never refuses, while the speed timeline still reads it at wor.Turns.ActionPips 0) on a Turn Speed effect
// payload, a skill's StatScaling on Turn Speed, a character's TurnSpeed points and gear / evolution BonusTurnSpeed,
// and on nothing else; wor.Data.ListTurnSpeed's per-asset string names the same fields.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Skills/Effects/ESkillEffectType.h"
#if WITH_EDITOR
#include "Character/CharacterData.h"
#include "Debug/Character/TurnSpeedRetireDebug.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Misc/DataValidation.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Effects/EffectDefinition.h"
#endif

namespace
{
	constexpr int32 RETIRE_TEST_TURN_SPEED_POINTS = 5;

#if WITH_EDITOR
	UEffectDefinition *NewTurnSpeedTestDefinition(ESkillEffectType Type)
	{
		UEffectDefinition *Definition = NewObject<UEffectDefinition>(GetTransientPackage());
		FSkillEffect Effect;
		FSkillEffectPayload Payload;
		Payload.EffectType = Type;
		Effect.Payloads.Add(Payload);
		Definition->Effects.Add(Effect);
		return Definition;
	}

	struct FTurnSpeedValidation
	{
		EDataValidationResult Result = EDataValidationResult::NotValidated;
		uint32 Warnings = 0;
	};

	FTurnSpeedValidation ValidateForTurnSpeed(const UObject *Asset)
	{
		FDataValidationContext Context;
		FTurnSpeedValidation Validation;
		Validation.Result = Asset->IsDataValid(Context);
		Validation.Warnings = Context.GetNumWarnings();
		return Validation;
	}
#endif
}

BEGIN_DEFINE_SPEC(FTurnSpeedRetireSpec, "WoR.Data.TurnSpeedRetire",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTurnSpeedRetireSpec)

void FTurnSpeedRetireSpec::Define()
{
	It("names the three Turn Speed effect types as the retired family, and no other speed type", [this]()
	{
		TestTrue(TEXT("TurnSpeedBuff"), SkillEffectClassification::IsTurnSpeedFamily(ESkillEffectType::TurnSpeedBuff));
		TestTrue(TEXT("TurnSpeedDebuff"), SkillEffectClassification::IsTurnSpeedFamily(ESkillEffectType::TurnSpeedDebuff));
		TestTrue(TEXT("ModifyTurnSpeed"), SkillEffectClassification::IsTurnSpeedFamily(ESkillEffectType::ModifyTurnSpeed));
		TestFalse(TEXT("ActionSpeedDebuff"), SkillEffectClassification::IsTurnSpeedFamily(ESkillEffectType::ActionSpeedDebuff));
		TestFalse(TEXT("SpellSpeedBuff"), SkillEffectClassification::IsTurnSpeedFamily(ESkillEffectType::SpellSpeedBuff));
		TestFalse(TEXT("SpeedBuff (D-056, refused separately)"), SkillEffectClassification::IsTurnSpeedFamily(ESkillEffectType::SpeedBuff));
		TestFalse(TEXT("GainActions"), SkillEffectClassification::IsTurnSpeedFamily(ESkillEffectType::GainActions));
		TestFalse(TEXT("not D-056 retired"), SkillEffectClassification::IsRetired(ESkillEffectType::ModifyTurnSpeed));
	});

#if WITH_EDITOR
	Describe("data validation", [this]()
	{
		It("warns once on each Turn Speed effect type and never refuses it", [this]()
		{
			const FTurnSpeedValidation Baseline = ValidateForTurnSpeed(NewTurnSpeedTestDefinition(ESkillEffectType::ActionSpeedBuff));
			for (const ESkillEffectType Type : {ESkillEffectType::TurnSpeedBuff, ESkillEffectType::TurnSpeedDebuff, ESkillEffectType::ModifyTurnSpeed})
			{
				const FString Name = UEnum::GetValueAsString(Type);
				const FTurnSpeedValidation Validation = ValidateForTurnSpeed(NewTurnSpeedTestDefinition(Type));
				TestNotEqual(Name + TEXT(" not refused"), Validation.Result, EDataValidationResult::Invalid);
				TestEqual(Name + TEXT(" warns once"), Validation.Warnings, Baseline.Warnings + 1u);
			}
		});

		It("doesn't warn on the other speed effects", [this]()
		{
			const FTurnSpeedValidation Baseline = ValidateForTurnSpeed(NewTurnSpeedTestDefinition(ESkillEffectType::DefenseBuff));
			for (const ESkillEffectType Type : {ESkillEffectType::ActionSpeedBuff, ESkillEffectType::ActionSpeedDebuff,
												ESkillEffectType::SpellSpeedBuff, ESkillEffectType::SpellSpeedDebuff})
			{
				TestEqual(UEnum::GetValueAsString(Type) + TEXT(" no extra warning"),
						  ValidateForTurnSpeed(NewTurnSpeedTestDefinition(Type)).Warnings, Baseline.Warnings);
			}
		});

		It("warns on a skill scaling with Turn Speed, not with Reflex", [this]()
		{
			FStatScaling Scaling;
			Scaling.Stat = ESubStat::TurnSpeed;
			UAbilityData *TurnSpeedSkill = NewObject<UAbilityData>(GetTransientPackage());
			TurnSpeedSkill->StatScaling.Add(Scaling);
			Scaling.Stat = ESubStat::Reflex;
			UAbilityData *ReflexSkill = NewObject<UAbilityData>(GetTransientPackage());
			ReflexSkill->StatScaling.Add(Scaling);
			const FTurnSpeedValidation TurnSpeed = ValidateForTurnSpeed(TurnSpeedSkill);
			TestEqual(TEXT("one more warning"), TurnSpeed.Warnings, ValidateForTurnSpeed(ReflexSkill).Warnings + 1u);
		});

		It("warns on a character's Turn Speed points and on no other sub-stat", [this]()
		{
			UCharacterData *Plain = NewObject<UCharacterData>(GetTransientPackage());
			UCharacterData *WithLuck = NewObject<UCharacterData>(GetTransientPackage());
			WithLuck->Luck = RETIRE_TEST_TURN_SPEED_POINTS;
			UCharacterData *WithTurnSpeed = NewObject<UCharacterData>(GetTransientPackage());
			WithTurnSpeed->TurnSpeed = RETIRE_TEST_TURN_SPEED_POINTS;
			const uint32 PlainWarnings = ValidateForTurnSpeed(Plain).Warnings;
			TestEqual(TEXT("Luck adds none"), ValidateForTurnSpeed(WithLuck).Warnings, PlainWarnings);
			TestEqual(TEXT("Turn Speed adds one"), ValidateForTurnSpeed(WithTurnSpeed).Warnings, PlainWarnings + 1u);
		});

		It("warns on gear and evolution BonusTurnSpeed", [this]()
		{
			UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
			const uint32 WeaponWarnings = ValidateForTurnSpeed(Weapon).Warnings;
			Weapon->BaseStatBonus.BonusTurnSpeed = RETIRE_TEST_TURN_SPEED_POINTS;
			TestEqual(TEXT("weapon"), ValidateForTurnSpeed(Weapon).Warnings, WeaponWarnings + 1u);

			UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
			const uint32 EvolutionWarnings = ValidateForTurnSpeed(Evolution).Warnings;
			Evolution->BaseStatBonus.BonusTurnSpeed = RETIRE_TEST_TURN_SPEED_POINTS;
			TestEqual(TEXT("evolution"), ValidateForTurnSpeed(Evolution).Warnings, EvolutionWarnings + 1u);
		});
	});

	Describe("wor.Data.ListTurnSpeed", [this]()
	{
		It("names what an asset authors, and nothing for an asset without Turn Speed", [this]()
		{
			TestTrue(TEXT("effect named"), TurnSpeedRetireDebug::GetTurnSpeedUsesString(
											   NewTurnSpeedTestDefinition(ESkillEffectType::TurnSpeedDebuff)).Contains(TEXT("TurnSpeedDebuff")));
			TestTrue(TEXT("other effect empty"),
					 TurnSpeedRetireDebug::GetTurnSpeedUsesString(NewTurnSpeedTestDefinition(ESkillEffectType::ActionSpeedDebuff)).IsEmpty());

			UCharacterData *Character = NewObject<UCharacterData>(GetTransientPackage());
			TestTrue(TEXT("no points, empty"), TurnSpeedRetireDebug::GetTurnSpeedUsesString(Character).IsEmpty());
			Character->TurnSpeed = RETIRE_TEST_TURN_SPEED_POINTS;
			TestTrue(TEXT("points named"), TurnSpeedRetireDebug::GetTurnSpeedUsesString(Character).Contains(
											   FString::Printf(TEXT("TurnSpeed = %d"), RETIRE_TEST_TURN_SPEED_POINTS)));

			TestTrue(TEXT("null is empty"), TurnSpeedRetireDebug::GetTurnSpeedUsesString(nullptr).IsEmpty());
		});
	});
#endif
}

#endif
