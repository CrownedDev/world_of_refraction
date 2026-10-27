// GameNamesSpec.cpp
// B-126: every element and status effect has a cooked-safe player-facing name.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRGameNames.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Grid/ECombatRow.h"
#include "Merchant/MerchantData.h"
#include "Skills/Definitions/SpellSchool.h"

BEGIN_DEFINE_SPEC(FGameNamesSpec, "WoR.Text.GameNames",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	/** Each value of TEnum has a non-empty name that isn't its raw identifier (a single-word identifier
	 *  such as "Fire" may be its own name). */
	template <typename TEnum>
	void ExpectEveryValueNamed(TFunctionRef<FText(TEnum)> GetName);
END_DEFINE_SPEC(FGameNamesSpec)

template <typename TEnum>
void FGameNamesSpec::ExpectEveryValueNamed(TFunctionRef<FText(TEnum)> GetName)
{
	const UEnum *Enum = StaticEnum<TEnum>();
	if (!TestNotNull(TEXT("Enum"), Enum))
	{
		return;
	}
	// The last entry is the generated _MAX.
	for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
	{
		const FString Identifier = Enum->GetNameStringByIndex(Index);
		const FString Name = GetName(static_cast<TEnum>(Enum->GetValueByIndex(Index))).ToString();
		TestFalse(FString::Printf(TEXT("%s has a name"), *Identifier), Name.IsEmpty());
		const bool bSingleWord = FName::NameToDisplayString(Identifier, false) == Identifier;
		TestTrue(FString::Printf(TEXT("%s's name '%s' is not its identifier"), *Identifier, *Name),
				 bSingleWord || Name != Identifier);
	}
}

void FGameNamesSpec::Define()
{
	It("names every element", [this]()
	{
		ExpectEveryValueNamed<ESpellElement>(&WoRGameNames::GetElementName);
	});

	It("names every status effect type", [this]()
	{
		ExpectEveryValueNamed<ESkillEffectType>(&WoRGameNames::GetSkillEffectTypeName);
	});

	It("names every character class", [this]()
	{
		ExpectEveryValueNamed<ECharacterClass>(&WoRGameNames::GetCharacterClassName);
	});

	It("names every defense, row, spell school and merchant type (B-207)", [this]()
	{
		ExpectEveryValueNamed<EDefenseType>(&WoRGameNames::GetDefenseTypeName);
		ExpectEveryValueNamed<ECombatRow>(&WoRGameNames::GetCombatRowName);
		ExpectEveryValueNamed<ESpellSchool>(&WoRGameNames::GetSpellSchoolName);
		ExpectEveryValueNamed<EMerchantType>(&WoRGameNames::GetMerchantTypeName);
		TestEqual(TEXT("Block reads Guard"), WoRGameNames::GetDefenseTypeName(EDefenseType::Block).ToString(), FString(TEXT("Guard")));
		TestEqual(TEXT("Legacy Parry reads Guard"), WoRGameNames::GetDefenseTypeName(EDefenseType::Parry).ToString(),
				  FString(TEXT("Guard")));
	});

	It("shows Reality as Unknown (D-095)", [this]()
	{
		TestEqual(TEXT("Element"), WoRGameNames::GetElementName(ESpellElement::Reality).ToString(), FString(TEXT("Unknown")));
		TestFalse(TEXT("Immunity"),
				  WoRGameNames::GetSkillEffectTypeName(ESkillEffectType::GrantRealityImmunity).ToString().Contains(TEXT("Reality")));
	});

	It("uses the player-facing word, not the code name", [this]()
	{
		TestEqual(TEXT("DOT"), WoRGameNames::GetSkillEffectTypeName(ESkillEffectType::DOT).ToString(),
				  FString(TEXT("Damage Over Time")));
		TestEqual(TEXT("SkipTurn"), WoRGameNames::GetSkillEffectTypeName(ESkillEffectType::SkipTurn).ToString(),
				  FString(TEXT("Skip Turn")));
		TestEqual(TEXT("Caster"), WoRGameNames::GetCharacterClassName(ECharacterClass::Caster).ToString(), FString(TEXT("Refractor")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
