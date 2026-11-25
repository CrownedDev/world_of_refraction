// CharacterSheetModel.cpp

#include "UI/Menus/CharacterSheetModel.h"

#include "Character/CharacterDataComponent.h"
#include "Character/EWorldPillar.h"
#include "Character/StatLayers.h"
#include "Combat/Actions/ActionPipsRules.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "Combat/CombatConstants.h"
#include "Combat/Damage/DamageMath.h"
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
									 {ESubStat::Luck, EStatUnit::Multiplier}}; // a score its users scale (1.0 at full stat), not a chance

	// The game's element order (ESpellElement), the ones a hit can carry.
	const ESpellElement RESISTED_ELEMENTS[] = {ESpellElement::Fire, ESpellElement::Water, ESpellElement::Earth,
											   ESpellElement::Wind, ESpellElement::Lightning, ESpellElement::Light,
											   ESpellElement::Darkness, ESpellElement::Void, ESpellElement::Reality};

	/** The value the fight uses for Stat; false when the character has none (no character data). */
	bool GetComposedStat(const UCharacterDataComponent &Character, ESubStat Stat, float &OutValue)
	{
		// The fight's own getters where a stat has one: they read the GAS attribute when UseGAS is on, so the
		// sheet can't disagree with the fight. The rest compose their layers, which is what their getters return.
		switch (Stat)
		{
		case ESubStat::Resistance: // the Spirit stat's buildup resistance (stat, gear, stones)
			OutValue = WoRResistance::GetGeneralStatusResistance(Character.GetOwner());
			return true;
		case ESubStat::SpellDamage:
			OutValue = Character.GetEffectiveSpellDamage();
			return true;
		case ESubStat::RawDamage:
			OutValue = Character.GetEffectiveRawDamage();
			return true;
		case ESubStat::ActionSpeed:
			OutValue = Character.GetEffectiveActionSpeed();
			return true;
		case ESubStat::SpellSpeed:
			OutValue = Character.GetEffectiveSpellSpeed();
			return true;
		case ESubStat::Luck:
			OutValue = Character.GetEquipmentModifiedLuck();
			return true;
		default:
			break;
		}
		FWoRStatLayers Layers;
		if (!Character.GetStatLayers(Stat, Layers))
		{
			return false;
		}
		OutValue = Layers.Compose();
		return true;
	}

	FText FormatValue(float Value, EStatUnit Unit)
	{
		return Unit == EStatUnit::Multiplier ? WoRCharacterSheet::FormatMultiplier(Value) : WoRCharacterSheet::FormatPercent(Value, /*bSigned=*/false);
	}

	/** B-635: what each sheet stat does, in the Profile's Details. */
	FText GetStatDescription(ESubStat Stat)
	{
		switch (Stat)
		{
		case ESubStat::Efficiency:
			return LOCTEXT("DescEfficiency", "Lowers the EP cost of spells and skills.");
		case ESubStat::SpellDamage:
			return LOCTEXT("DescSpellDamage", "Multiplies the damage of spells.");
		case ESubStat::CritDamage:
			return LOCTEXT("DescCritDamage", "How hard a critical hit lands, as a multiple of a normal hit.");
		case ESubStat::SpellSpeed:
			return LOCTEXT("DescSpellSpeed", "How fast spells are cast; a faster cast gives the target less time to defend.");
		case ESubStat::Defense:
			return LOCTEXT("DescDefense", "Cuts the damage of every hit taken.");
		case ESubStat::ActionSpeed:
			return LOCTEXT("DescActionSpeed", "How fast weapon actions play; a faster attack gives the target less time to defend.");
		case ESubStat::RawDamage:
			return LOCTEXT("DescRawDamage", "Multiplies the damage of weapon hits and physical skills.");
		case ESubStat::Reflex:
			return LOCTEXT("DescReflex", "Widens the window to dodge or parry a hit.");
		case ESubStat::Resistance:
			return LOCTEXT("DescResistance", "Slows the status buildup hits put on you.");
		case ESubStat::StatusMultiplier:
			return LOCTEXT("DescStatusMultiplier", "Multiplies the status buildup your hits put on a target.");
		case ESubStat::Luck:
			return LOCTEXT("DescLuck", "Helps crits, dodges and drops.");
		default:
			return FText::GetEmpty();
		}
	}

	/** A layer's factor as its share: x1.05 reads "+5%"; none when it changes nothing. */
	void AddFactor(TArray<FCharacterSheetSource> &Sources, const FText &Label, float Factor)
	{
		if (!FMath::IsNearlyEqual(Factor, 1.0f))
		{
			Sources.Add({Label, WoRCharacterSheet::FormatPercent(Factor - 1.0f, /*bSigned=*/true)});
		}
	}

	/** B-635: the stat's bar, its layers and what it does. Stats with no layers (Resistance) show only their total. */
	void FillStatDetails(const UCharacterDataComponent &Character, const FCharacterSheetPillar &Pillar, const FStatRow &Row, float Value,
						 FCharacterSheetStat &Stat)
	{
		Stat.Description = GetStatDescription(Row.Stat);
		FWoRStatLayers Layers;
		const bool bHasLayers = Character.GetStatLayers(Row.Stat, Layers);
		// The bar is full at the stat's ceiling; an unbounded stat (Reflex) uses the usual modifier ceiling.
		const bool bBounded = bHasLayers && Layers.Max < TNumericLimits<float>::Max();
		const float BarMax = bBounded ? Layers.Max : Row.Unit == EStatUnit::Percent ? 1.0f : CombatConstants::STAT_MODIFIER_MAX;
		Stat.Fill = FMath::Clamp(Value / FMath::Max(BarMax, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
		if (!bHasLayers)
		{
			return;
		}
		Stat.Sources.Add({FText::Format(LOCTEXT("SourcePillar", "{0} {1}"), Pillar.Name, FText::AsNumber(Pillar.Level)), FormatValue(Layers.StatBase, Row.Unit)});
		AddFactor(Stat.Sources, LOCTEXT("SourceGear", "Gear"), Layers.GearFactor);
		AddFactor(Stat.Sources, LOCTEXT("SourceStone", "Stone"), Layers.StoneFactor);
		AddFactor(Stat.Sources, LOCTEXT("SourceBuffs", "Buffs"), Layers.TransientFactor);
		const float Unclamped = Layers.StatBase * Layers.GearFactor * Layers.StoneFactor * Layers.TransientFactor;
		if (Layers.Clamp(Unclamped) < Unclamped - UE_KINDA_SMALL_NUMBER)
		{
			Stat.Sources.Add({LOCTEXT("SourceCap", "Capped at"), FormatValue(Layers.Max, Row.Unit)});
		}
	}

	/** B-635: each pillar's Profile row and heading line. */
	void SetPillarLines(EWorldPillar Pillar, FCharacterSheetPillar &Out)
	{
		switch (Pillar)
		{
		case EWorldPillar::Mind:
			Out.Blurb = LOCTEXT("BlurbMind", "Efficiency, spell damage, crits");
			Out.Subtitle = LOCTEXT("SubtitleMind", "Spell force and cost");
			break;
		case EWorldPillar::Body:
			Out.Blurb = LOCTEXT("BlurbBody", "Defence, speed, raw damage");
			Out.Subtitle = LOCTEXT("SubtitleBody", "Toughness and weapon force");
			break;
		case EWorldPillar::Spirit:
			Out.Blurb = LOCTEXT("BlurbSpirit", "Resistance, status, luck");
			Out.Subtitle = LOCTEXT("SubtitleSpirit", "Status and fortune");
			break;
		default:
			break;
		}
	}

	template <int32 Count>
	FCharacterSheetPillar MakePillar(const UCharacterDataComponent &Character, EWorldPillar Pillar, const FStatRow (&Rows)[Count])
	{
		FCharacterSheetPillar Out;
		Out.Name = WoRLevelDrops::GetPillarName(Pillar);
		Out.Level = Character.GetWorldStat(Pillar);
		Out.MaxLevel = WorldStatConstants::LIVE_MAX;
		SetPillarLines(Pillar, Out);
		for (const FStatRow &Row : Rows)
		{
			float Value = 0.0f;
			const bool bHasValue = GetComposedStat(Character, Row.Stat, Value);
			FCharacterSheetStat &Stat = Out.Stats.Add_GetRef({WoRGameNames::GetSubStatName(Row.Stat), bHasValue ? FormatValue(Value, Row.Unit) : LOCTEXT("NoValue", "-")});
			if (bHasValue)
			{
				FillStatDetails(Character, Out, Row, Value, Stat);
			}
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
	// Each line's bar is full at its own cap: Defence's ceiling, and the cap the scope and element lines share.
	const float TypeCap = WoRDamageMath::ELEMENT_DAMAGE_RESIST_CAP;
	Lines.Add({LOCTEXT("ResistAll", "All"), Defence, ESpellElement::None, CombatConstants::DEFENSE_REDUCTION_CEILING});
	Lines.Add({LOCTEXT("ResistPhysical", "Physical"), WoRResistance::GetScopeLineResistance(Character, /*bHitIsSpell=*/false), ESpellElement::None, TypeCap});
	Lines.Add({LOCTEXT("ResistSpell", "Spell"), WoRResistance::GetScopeLineResistance(Character, /*bHitIsSpell=*/true), ESpellElement::None, TypeCap});
	OutScopeLineCount = Lines.Num();
	static_assert(UE_ARRAY_COUNT(CharacterSheetBuild::RESISTED_ELEMENTS) == ELEMENT_LINE_COUNT, "ELEMENT_LINE_COUNT is the element list's length");
	checkSlow(OutScopeLineCount == SCOPE_LINE_COUNT);
	for (const ESpellElement Element : CharacterSheetBuild::RESISTED_ELEMENTS)
	{
		Lines.Add({WoRGameNames::GetElementName(Element), WoRResistance::GetElementDamageResistance(Character, Element), Element, TypeCap});
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

	const FPartyMember *Member = FindPartyMember(World, MemberIndex);
	const UCharacterDataComponent *Character = Member ? WoRLevelDrops::FindSpawnedMember(World, *Member) : nullptr;
	if (!Character)
	{
		Page.Refusal = FText::Format(LOCTEXT("MemberNotHere", "{0} isn't here."), Page.Who.Name);
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

TArray<FCharacterSheetGroup> WoRCharacterSheet::BuildGroups(const FCharacterSheetPage &Page)
{
	TArray<FCharacterSheetGroup> Groups;
	if (!Page.Refusal.IsEmpty())
	{
		return Groups;
	}
	for (const FCharacterSheetPillar &Pillar : Page.Pillars)
	{
		Groups.Add({Pillar.Name, FText::AsNumber(Pillar.Level), Pillar.Blurb, Pillar.Subtitle, Pillar.Stats});
	}

	FCharacterSheetGroup Resistances{LOCTEXT("GroupResistances", "Resistances"), FText::GetEmpty(), LOCTEXT("BlurbResistances", "Physical, spell, each element"),
									 LOCTEXT("SubtitleResistances", "Less damage taken, by type")};
	for (int32 Index = 0; Index < Page.Resistances.Num(); ++Index)
	{
		const FCharacterSheetResistance &Line = Page.Resistances[Index];
		FCharacterSheetStat &Entry = Resistances.Entries.AddDefaulted_GetRef();
		Entry.Label = Line.Label;
		Entry.Value = FormatPercent(Line.Fraction, /*bSigned=*/true);
		Entry.Fill = FMath::Clamp(FMath::Abs(Line.Fraction) / FMath::Max(Line.Cap, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
		Entry.bWeak = Line.Fraction < 0.0f;
		const FText Cap = FormatPercent(Line.Cap, /*bSigned=*/false);
		Entry.Description = Index == 0 ? FText::Format(LOCTEXT("DescResistAll", "Your Defence: cuts the damage of every hit. It stops at {0}."), Cap)
									   : FText::Format(LOCTEXT("DescResistLine", "How much less {0} damage you take; below zero, how much more. It stops at {1}."),
													   Line.Label, Cap);
	}
	Groups.Add(MoveTemp(Resistances));

	FCharacterSheetGroup Curses{LOCTEXT("GroupCurses", "Curses"), Page.Curses.IsEmpty() ? FText::GetEmpty() : FText::AsNumber(Page.Curses.Num()),
								LOCTEXT("BlurbCurses", "Held by you; on every sheet"), LOCTEXT("SubtitleCurses", "Cleared at the Spiritualist")};
	UE_CLOG(Page.Curses.Num() > MAX_GROUP_ENTRIES, LogWoRUI, Warning, TEXT("[CharacterSheet] %d curses held; the Profile lists the first %d"),
			Page.Curses.Num(), MAX_GROUP_ENTRIES);
	for (int32 Index = 0; Index < Page.Curses.Num() && Index < MAX_GROUP_ENTRIES; ++Index)
	{
		const FCharacterSheetCurse &Curse = Page.Curses[Index];
		FCharacterSheetStat &Entry = Curses.Entries.AddDefaulted_GetRef();
		Entry.Label = Curse.Title;
		Entry.Description = Curse.Description;
	}
	Groups.Add(MoveTemp(Curses));
	checkSlow(Page.Pillars.IsEmpty() || Groups.Num() == GROUP_COUNT);
	return Groups;
}

namespace CharacterSheetBuild
{
	static const UParty *FindLocalParty(UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
		return PartySession ? PartySession->GetLocalParty() : nullptr;
	}
}

const FPartyMember *WoRCharacterSheet::FindPartyMember(UWorld *World, int32 MemberIndex)
{
	// BuildParty skips members with no character data, so the same walk finds this row's member.
	const UParty *Party = CharacterSheetBuild::FindLocalParty(World);
	if (!Party)
	{
		return nullptr;
	}
	int32 Shown = 0;
	for (const FPartyMember &Candidate : Party->Members)
	{
		if (Candidate.CharacterData && Shown++ == MemberIndex)
		{
			return &Candidate;
		}
	}
	return nullptr;
}

int32 WoRCharacterSheet::FindMemberIndex(UWorld *World, int32 PartySlot)
{
	const UParty *Party = CharacterSheetBuild::FindLocalParty(World);
	if (!Party || !Party->Members.IsValidIndex(PartySlot) || !Party->Members[PartySlot].CharacterData)
	{
		return INDEX_NONE;
	}
	int32 MemberIndex = 0;
	for (int32 Slot = 0; Slot < PartySlot; ++Slot)
	{
		MemberIndex += Party->Members[Slot].CharacterData ? 1 : 0;
	}
	return MemberIndex;
}

FString WoRCharacterSheet::GetCharacterSheetString(UWorld *World)
{
	FString Out = TEXT("[CharacterSheet]");
	const int32 MemberCount = WoRPauseMenu::BuildParty(World).Num();
	for (int32 Index = 0; Index < MemberCount; ++Index)
	{
		const FCharacterSheetPage Page = BuildPage(World, Index);
		Out += FString::Printf(TEXT("\n  %s (%s): HP %d/%d, EP %d/%d, Max Actions %d (+%d/%d), regain %d%s"), *Page.Who.Name.ToString(),
							   *Page.Who.SheetLine.ToString().Replace(TEXT("\n"), TEXT(" / ")), Page.Who.HP, Page.Who.MaxHP, Page.Who.EP, Page.Who.MaxEP, Page.MaxActions,
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
