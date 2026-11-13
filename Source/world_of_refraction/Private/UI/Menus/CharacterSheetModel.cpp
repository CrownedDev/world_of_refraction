// CharacterSheetModel.cpp

#include "UI/Menus/CharacterSheetModel.h"

#include "Character/CharacterDataComponent.h"
#include "Character/EWorldPillar.h"
#include "Character/StatLayers.h"
#include "Combat/Actions/ActionPipsRules.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "Combat/Resistance/ResistanceQuery.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Memory/CurseData.h"
#include "Memory/CurseSubsystem.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Trial/LevelDropPlacement.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRCharacterSheet"

namespace CharacterSheetBuild
{
	constexpr float PERCENT = 100.0f;

	/** How a sub-stat's composed value reads: a multiplier ("x1.48") or a share ("9%"). */
	enum class EStatUnit : uint8
	{
		Multiplier,
		Percent
	};

	struct FStatRow
	{
		ESubStat Stat;
		EStatUnit Unit;
	};

	// The grouping of Rules CL-39 (ESubStat's own order is its save order, not this one).
	const FStatRow MIND_STATS[] = {{ESubStat::Efficiency, EStatUnit::Percent},
								   {ESubStat::SpellDamage, EStatUnit::Multiplier},
								   {ESubStat::CritDamage, EStatUnit::Multiplier},
								   {ESubStat::SpellSpeed, EStatUnit::Multiplier}};
	const FStatRow BODY_STATS[] = {{ESubStat::Defense, EStatUnit::Percent},
								   {ESubStat::ActionSpeed, EStatUnit::Multiplier},
								   {ESubStat::RawDamage, EStatUnit::Multiplier},
								   {ESubStat::Reflex, EStatUnit::Multiplier}};
	const FStatRow SPIRIT_STATS[] = {{ESubStat::Resistance, EStatUnit::Percent},
									 {ESubStat::StatusMultiplier, EStatUnit::Multiplier},
									 {ESubStat::Luck, EStatUnit::Percent}};

	// The game's element order (ESpellElement), the ones a hit can carry.
	const ESpellElement RESISTED_ELEMENTS[] = {ESpellElement::Fire, ESpellElement::Water, ESpellElement::Earth,
											   ESpellElement::Wind, ESpellElement::Lightning, ESpellElement::Light,
											   ESpellElement::Darkness, ESpellElement::Void, ESpellElement::Reality};

	/** The value the fight uses for Stat; false when the character has none (no character data). */
	bool GetComposedStat(const UCharacterDataComponent &Character, ESubStat Stat, float &OutValue)
	{
		if (Stat == ESubStat::Resistance)
		{
			// The Spirit stat's buildup resistance (stat, gear, stones); it has no GetStatLayers entry.
			OutValue = WoRResistance::GetGeneralStatusResistance(Character.GetOwner());
			return true;
		}
		FWoRStatLayers Layers;
		if (!Character.GetStatLayers(Stat, Layers))
		{
			return false;
		}
		OutValue = Layers.Compose();
		return true;
	}

	template <int32 Count>
	FCharacterSheetPillar MakePillar(const UCharacterDataComponent &Character, EWorldPillar Pillar, const FStatRow (&Rows)[Count])
	{
		FCharacterSheetPillar Out;
		Out.Name = WoRLevelDrops::GetPillarName(Pillar);
		Out.Level = Character.GetWorldStat(Pillar);
		Out.MaxLevel = WorldStatConstants::LIVE_MAX;
		for (const FStatRow &Row : Rows)
		{
			float Value = 0.0f;
			const bool bHasValue = GetComposedStat(Character, Row.Stat, Value);
			const FText Shown = !bHasValue ? LOCTEXT("NoValue", "-")
											: Row.Unit == EStatUnit::Multiplier ? WoRCharacterSheet::FormatMultiplier(Value)
																				: WoRCharacterSheet::FormatPercent(Value, /*bSigned=*/false);
			Out.Stats.Add({WoRGameNames::GetSubStatName(Row.Stat), Shown});
		}
		return Out;
	}
}

FText WoRCharacterSheet::FormatMultiplier(float Multiplier)
{
	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = 2;
	Options.MaximumFractionalDigits = 2;
	return FText::Format(LOCTEXT("Multiplier", "x{0}"), FText::AsNumber(Multiplier, &Options));
}

FText WoRCharacterSheet::FormatPercent(float Fraction, bool bSigned)
{
	const int32 Percent = FMath::RoundToInt(Fraction * CharacterSheetBuild::PERCENT);
	return bSigned && Percent > 0 ? FText::Format(LOCTEXT("PercentPlus", "+{0}%"), FText::AsNumber(Percent))
								  : FText::Format(LOCTEXT("Percent", "{0}%"), FText::AsNumber(Percent));
}

TArray<FCharacterSheetPillar> WoRCharacterSheet::BuildPillars(const UCharacterDataComponent &Character)
{
	using namespace CharacterSheetBuild;
	TArray<FCharacterSheetPillar> Pillars;
	Pillars.Add(MakePillar(Character, EWorldPillar::Mind, MIND_STATS));
	Pillars.Add(MakePillar(Character, EWorldPillar::Body, BODY_STATS));
	Pillars.Add(MakePillar(Character, EWorldPillar::Spirit, SPIRIT_STATS));
	return Pillars;
}

TArray<FCharacterSheetResistance> WoRCharacterSheet::BuildResistances(AActor *Character, int32 &OutScopeLineCount)
{
	TArray<FCharacterSheetResistance> Lines;
	const UCharacterDataComponent *Data = Character ? Character->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	float Defence = 0.0f;
	if (Data)
	{
		CharacterSheetBuild::GetComposedStat(*Data, ESubStat::Defense, Defence);
	}
	// D-168: Defence is the All line; Physical and Spell are the scope lines; then each element.
	Lines.Add({LOCTEXT("ResistAll", "All"), Defence, ESpellElement::None});
	Lines.Add({LOCTEXT("ResistPhysical", "Physical"), WoRResistance::GetScopeLineResistance(Character, /*bHitIsSpell=*/false), ESpellElement::None});
	Lines.Add({LOCTEXT("ResistSpell", "Spell"), WoRResistance::GetScopeLineResistance(Character, /*bHitIsSpell=*/true), ESpellElement::None});
	OutScopeLineCount = Lines.Num();
	static_assert(UE_ARRAY_COUNT(CharacterSheetBuild::RESISTED_ELEMENTS) == ELEMENT_LINE_COUNT, "ELEMENT_LINE_COUNT is the element list's length");
	checkSlow(OutScopeLineCount == SCOPE_LINE_COUNT);
	for (const ESpellElement Element : CharacterSheetBuild::RESISTED_ELEMENTS)
	{
		Lines.Add({WoRGameNames::GetElementName(Element), WoRResistance::GetElementDamageResistance(Character, Element), Element});
	}
	return Lines;
}

FCharacterSheetPage WoRCharacterSheet::BuildPage(UWorld *World, int32 MemberIndex)
{
	FCharacterSheetPage Page;
	const TArray<FPauseMenuMember> Party = WoRPauseMenu::BuildParty(World);
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	const UParty *LocalParty = PartySession ? PartySession->GetLocalParty() : nullptr;
	if (!Party.IsValidIndex(MemberIndex) || !LocalParty)
	{
		Page.Refusal = LOCTEXT("NoParty", "There is no party to show.");
		return Page;
	}
	Page.Who = Party[MemberIndex];
	Page.PointsPerAction = ActionPipsConstants::ACTION_POINTS_PER_ACTION;

	// BuildParty skips members with no character data, so the same walk finds this row's member.
	const FPartyMember *Member = nullptr;
	int32 Shown = 0;
	for (const FPartyMember &Candidate : LocalParty->Members)
	{
		if (Candidate.CharacterData && Shown++ == MemberIndex)
		{
			Member = &Candidate;
			break;
		}
	}
	const UCharacterDataComponent *Character = Member ? WoRLevelDrops::FindSpawnedMember(World, *Member) : nullptr;
	if (!Character)
	{
		Page.Refusal = FText::Format(LOCTEXT("MemberNotHere", "{0} isn't here, so their stats can't be shown."), Page.Who.Name);
		return Page;
	}
	Page.MaxActions = Character->GetMaxActions();
	Page.ActionPointProgress = Character->GetActionPointProgress();
	Page.ActionRegain = Character->GetActionRegain();
	Page.Pillars = BuildPillars(*Character);
	Page.Resistances = BuildResistances(Character->GetOwner(), Page.ScopeLineCount);

	// Curses are held by the player, not by one member (UCurseSubsystem), so every sheet lists them.
	if (const UCurseSubsystem *Curses = GI->GetSubsystem<UCurseSubsystem>())
	{
		for (const FName &CurseId : Curses->GetHeldCurses())
		{
			if (const UCurseData *Curse = Curses->FindCurseAsset(CurseId))
			{
				Page.Curses.Add({Curse->Title, Curse->Description});
			}
		}
	}
	return Page;
}

FString WoRCharacterSheet::GetCharacterSheetString(UWorld *World)
{
	FString Out = TEXT("[CharacterSheet]");
	const int32 MemberCount = WoRPauseMenu::BuildParty(World).Num();
	for (int32 Index = 0; Index < MemberCount; ++Index)
	{
		const FCharacterSheetPage Page = BuildPage(World, Index);
		Out += FString::Printf(TEXT("\n  %s (%s): HP %d/%d, EP %d/%d, Max Actions %d (+%d/%d), regain %d%s"), *Page.Who.Name.ToString(),
							   *Page.Who.ClassLine.ToString(), Page.Who.HP, Page.Who.MaxHP, Page.Who.EP, Page.Who.MaxEP, Page.MaxActions,
							   Page.ActionPointProgress, Page.PointsPerAction, Page.ActionRegain,
							   Page.Refusal.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Page.Refusal.ToString()));
		for (const FCharacterSheetPillar &Pillar : Page.Pillars)
		{
			Out += FString::Printf(TEXT("\n    %s %d/%d:"), *Pillar.Name.ToString(), Pillar.Level, Pillar.MaxLevel);
			for (const FCharacterSheetStat &Stat : Pillar.Stats)
			{
				Out += FString::Printf(TEXT(" %s %s;"), *Stat.Label.ToString(), *Stat.Value.ToString());
			}
		}
		Out += TEXT("\n    Resistance:");
		for (const FCharacterSheetResistance &Line : Page.Resistances)
		{
			Out += FString::Printf(TEXT(" %s %s;"), *Line.Label.ToString(), *FormatPercent(Line.Fraction, /*bSigned=*/true).ToString());
		}
		for (const FCharacterSheetCurse &Curse : Page.Curses)
		{
			Out += FString::Printf(TEXT("\n    Curse: %s"), *Curse.Title.ToString());
		}
	}
	return Out;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GCharacterSheetPrintCommand(
	TEXT("wor.UI.PrintSheet"),
	TEXT("DEBUG (B-442): what the character sheet would show for each party member."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		UE_LOG(LogWoRUI, Display, TEXT("%s"), *WoRCharacterSheet::GetCharacterSheetString(World));
	}));
#endif

#undef LOCTEXT_NAMESPACE
