// ElementTableDebug.cpp

#include "Debug/Combat/ElementTableDebug.h"

#include "Combat/Elements/ElementTable.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Currency/CurrencyTypes.h"
#include "Currency/EconomyYield.h"
#include "Engine/Engine.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Infusion/ElementColors.h"
#include "Infusion/HybridSpellColors.h"
#include "Skills/Effects/BarCapTriggerResolver.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "WoRLog.h"

namespace
{
	constexpr float ELEMENTS_SCREEN_SECONDS = 8.0f;
	/** The gems that carry an element (Quartz carries None); stones are not gems. */
	constexpr ECrystalType FIRST_GEM = ECrystalType::Garnet;
	constexpr ECrystalType LAST_GEM = ECrystalType::Quartz;

	FAutoConsoleCommand GPrintElementsCmd(
		TEXT("wor.Elements.Print"),
		TEXT("DEBUG: print every row of the element table."),
		FConsoleCommandDelegate::CreateLambda([]() { UE_LOG(LogWoR, Display, TEXT("%s"), *UElementTableDebug::GetElementTableString()); }));

	FAutoConsoleCommand GCompareElementsCmd(
		TEXT("wor.Elements.Compare"),
		TEXT("DEBUG: compare the element table with the old per-element functions."),
		FConsoleCommandDelegate::CreateLambda([]() { UElementTableDebug::PrintCompareElements(); }));

	template <typename TEnum>
	FString ElementEnumName(TEnum Value)
	{
		return StaticEnum<TEnum>()->GetNameStringByValue(static_cast<int64>(Value));
	}

	template <typename TValue>
	void ExpectElement(TArray<FString> &Out, const FString &What, TValue Table, TValue Old)
	{
		if (Table != Old)
		{
			Out.Add(What);
		}
	}
}

FString UElementTableDebug::GetElementTableString()
{
	FString Out = TEXT("[ElementTable]");
	for (const FElementRow &Row : ElementTable::Rows())
	{
		Out += FString::Printf(TEXT("\n  %s: element %s, physical %s, also %s, gem %s, essence %s, break %s, immunity %s, colour %s%s%s"),
							   *ElementEnumName(Row.Type), *ElementEnumName(Row.Element), *ElementEnumName(Row.Physical), *ElementEnumName(Row.AlsoResistedBy),
							   *ElementEnumName(Row.Gem), *ElementEnumName(Row.Essence), *ElementEnumName(Row.BreakTrigger), *ElementEnumName(Row.Immunity),
							   *Row.Colour.ToString(), Row.bBDAbsorbable ? TEXT(", BD absorbs") : TEXT(""),
							   Row.bBDForbidden ? TEXT(", BD forbidden") : TEXT(""));
	}
	return Out;
}

TArray<FString> UElementTableDebug::CompareElements()
{
	TArray<FString> Mismatches;
	const TArray<FElementRow> &Rows = ElementTable::Rows();
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		ExpectElement(Mismatches, FString::Printf(TEXT("row %d is %s"), Index, *ElementEnumName(Rows[Index].Type)), static_cast<int32>(Rows[Index].Type), Index);
	}
	ExpectElement(Mismatches, TEXT("one row per damage type"), Rows.Num(), WoRDamageType::All().Num());

	const UEnum *Elements = StaticEnum<ESpellElement>();
	for (int32 Index = 0; Index < Elements->NumEnums() - 1; ++Index)
	{
		const ESpellElement Element = static_cast<ESpellElement>(Elements->GetValueByIndex(Index));
		const FElementRow &Row = ElementTable::ForElement(Element);
		const FString Name = ElementEnumName(Element);
		ExpectElement(Mismatches, Name + TEXT(" essence"), Row.Essence, EconomyYield::ElementToEssenceType(Element));
		ExpectElement(Mismatches, Name + TEXT(" immunity"), Row.Immunity, SkillEffectClassification::GetElementImmunityType(Element));
		ExpectElement(Mismatches, Name + TEXT(" colour"), Row.Colour, ElementColors::GetColorForElement(Element));
		ExpectElement(Mismatches, Name + TEXT(" break trigger"), Row.BreakTrigger,
			   BarCapTriggerResolver::ResolveTrigger(Element, EPhysicalDamageType::None, false));
		ExpectElement(Mismatches, Name + TEXT(" BD absorbs"), Row.bBDAbsorbable, UBrokenDarknessManager::CanAbsorbElement(Element));
		ExpectElement(Mismatches, Name + TEXT(" BD forbidden"), Row.bBDForbidden, UBrokenDarknessManager::IsForbiddenElement(Element));
		ExpectElement(Mismatches, Name + TEXT(" BD forbidden (colours)"), Row.bBDForbidden, UHybridSpellColors::IsForbiddenElement(Element));
	}

	for (const EPhysicalDamageType Physical : {EPhysicalDamageType::Slash, EPhysicalDamageType::Pierce, EPhysicalDamageType::Impact})
	{
		const FElementRow &Row = ElementTable::ForPhysical(Physical);
		ExpectElement(Mismatches, ElementEnumName(Physical) + TEXT(" physical axis"), Row.Physical, Physical);
		ExpectElement(Mismatches, ElementEnumName(Physical) + TEXT(" break trigger"), Row.BreakTrigger,
			   BarCapTriggerResolver::ResolveTrigger(ESpellElement::None, Physical, false));
	}

	for (uint8 Gem = static_cast<uint8>(FIRST_GEM); Gem <= static_cast<uint8>(LAST_GEM); ++Gem)
	{
		const ECrystalType Crystal = static_cast<ECrystalType>(Gem);
		ExpectElement(Mismatches, ElementEnumName(Crystal) + TEXT(" gem"), ElementTable::ForElement(CrystalTypeHelpers::GetElement(Crystal)).Gem, Crystal);
	}
	return Mismatches;
}

void UElementTableDebug::PrintCompareElements()
{
	const TArray<FString> Mismatches = CompareElements();
	const FString Summary = Mismatches.IsEmpty() ? TEXT("[CompareElements] the table agrees with every old function")
												 : FString::Printf(TEXT("[CompareElements] %d mismatch(es): %s"), Mismatches.Num(),
																   *FString::Join(Mismatches, TEXT(", ")));
	UE_LOG(LogWoR, Display, TEXT("%s"), *Summary);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, ELEMENTS_SCREEN_SECONDS, Mismatches.IsEmpty() ? FColor::Green : FColor::Red, Summary);
	}
}
