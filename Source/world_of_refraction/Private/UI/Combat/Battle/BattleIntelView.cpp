// BattleIntelView.cpp

#include "UI/Combat/Battle/BattleIntelView.h"

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/EWorldPillar.h"
#include "Character/PlayerBuild.h"
#include "Character/StatLayers.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "Combat/Damage/DamageMath.h"
#include "Combat/Resistance/ResistanceQuery.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Player/RunStateComponent.h"
#include "Pool/PoolSubsystem.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Combat/Battle/BattleHUDCard.h"
#include "UI/Menus/CharacterSheetModel.h"
#include "WoRGameNames.h"

#define LOCTEXT_NAMESPACE "BattleIntel"

namespace
{
	/** A stat the effects moved less than this is shown as unchanged (float noise from composing the layers). */
	constexpr float STAT_CHANGE_EPSILON = 0.005f;
	/** An element's damage multiplier this close to 1 is neither a weakness nor a resistance. */
	constexpr float ELEMENT_NEUTRAL_EPSILON = 0.005f;
	constexpr float PERCENT = 100.0f;

	struct FIntelStat
	{
		ESubStat Stat;
		bool bPercent;
	};

	// The Character sheet's eleven, in its order (Rules CL-39; CharacterSheetModel.cpp's tables).
	// shortcut: a copy of the sheet's private tables, share them if a third screen lists the stats.
	const FIntelStat INTEL_STATS[] = {
		{ESubStat::Efficiency, true}, {ESubStat::SpellDamage, false}, {ESubStat::CritDamage, false}, {ESubStat::SpellSpeed, false},
		{ESubStat::Defense, true}, {ESubStat::ActionSpeed, false}, {ESubStat::RawDamage, false}, {ESubStat::Reflex, false},
		{ESubStat::Resistance, true}, {ESubStat::StatusMultiplier, false}, {ESubStat::Luck, false}};

	// The elements a hit can carry, in the game's order (CharacterSheetModel's RESISTED_ELEMENTS).
	const ESpellElement INTEL_ELEMENTS[] = {ESpellElement::Fire, ESpellElement::Water, ESpellElement::Earth,
											ESpellElement::Wind, ESpellElement::Lightning, ESpellElement::Light,
											ESpellElement::Darkness, ESpellElement::Void, ESpellElement::Reality};

	FText FormatStat(float Value, bool bPercent)
	{
		return bPercent ? WoRCharacterSheet::FormatPercent(Value, /*bSigned=*/false) : WoRCharacterSheet::FormatMultiplier(Value);
	}

	FText SignedPercent(float Percent, bool bNegative)
	{
		const int32 Rounded = FMath::RoundToInt(FMath::Abs(Percent));
		return bNegative ? FText::Format(LOCTEXT("MinusPercent", "-{0}%"), FText::AsNumber(Rounded))
						 : FText::Format(LOCTEXT("PlusPercent", "+{0}%"), FText::AsNumber(Rounded));
	}

	FText FormatTurns(const FActiveSkillEffect &Effect)
	{
		return Effect.bPermanent ? LOCTEXT("UntilRemoved", "until removed")
								 : FText::Format(LOCTEXT("TurnsLeft", "{0} {0}|plural(one=turn,other=turns)"), FText::AsNumber(Effect.RemainingTurns));
	}

	/** Q-586 (D-261): an effect with no number says what it does, in short. Empty: it has a number. */
	FText DescribeNoNumber(ESkillEffectType Type)
	{
		switch (Type)
		{
		case ESkillEffectType::Stun:
		case ESkillEffectType::SkipTurn:
			return LOCTEXT("CantAct", "can't act");
		case ESkillEffectType::Silenced:
			return LOCTEXT("CantCast", "can't cast");
		case ESkillEffectType::HealBlock:
			return LOCTEXT("NoHealing", "no healing");
		default:
			return FText::GetEmpty();
		}
	}

	FText DescribeRestore(ESkillEffectType Type, float Value)
	{
		const FText Amount = FText::AsNumber(FMath::RoundToInt(FMath::Abs(Value)));
		switch (Type)
		{
		case ESkillEffectType::HealthRestore:
			return FText::Format(LOCTEXT("HPATurn", "+{0} HP a turn"), Amount);
		case ESkillEffectType::RestoreHPPercent:
			return FText::Format(LOCTEXT("HPPercentATurn", "+{0}% HP a turn"), Amount);
		case ESkillEffectType::EnergyRestore:
			return FText::Format(LOCTEXT("EPATurn", "+{0} EP a turn"), Amount);
		case ESkillEffectType::RestoreEnergyPercent:
			return FText::Format(LOCTEXT("EPPercentATurn", "+{0}% EP a turn"), Amount);
		default:
			return FText::GetEmpty();
		}
	}
}

FBattleIntelRow BattleIntelView::MakeStatRow(const FText &Label, float Before, float Now, bool bPercent)
{
	FBattleIntelRow Row;
	Row.Label = Label;
	Row.Now = FormatStat(Now, bPercent);
	if (!FMath::IsNearlyEqual(Before, Now, STAT_CHANGE_EPSILON))
	{
		Row.Before = FormatStat(Before, bPercent);
		Row.Tone = Now > Before ? EBattleIntelTone::Better : EBattleIntelTone::Worse;
	}
	return Row;
}

TArray<FBattleIntelRow> BattleIntelView::BuildStats(AActor *Actor)
{
	TArray<FBattleIntelRow> Rows;
	const UCharacterDataComponent *Character = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	for (const FIntelStat &Entry : INTEL_STATS)
	{
		const FText Label = WoRGameNames::GetSubStatName(Entry.Stat);
		if (Entry.Stat == ESubStat::Resistance)
		{
			// No transient layer: the fight's effects reach it as the ModifyStatusResist bonus (the boss's rise kept apart).
			const float Before = WoRResistance::GetGeneralStatusResistance(Actor);
			const float Now = Before + WoRResistance::GetStatusResistBonusSources(Actor).Effect;
			Rows.Add(MakeStatRow(Label, Before, Now, Entry.bPercent));
			continue;
		}
		FWoRStatLayers Layers;
		if (!Character || !Character->GetStatLayers(Entry.Stat, Layers))
		{
			FBattleIntelRow Empty;
			Empty.Label = Label;
			Empty.Now = INVTEXT("-");
			Rows.Add(Empty);
			continue;
		}
		const float Now = Layers.Compose();
		Layers.TransientFactor = 1.0f; // without the fight's effects
		Rows.Add(MakeStatRow(Label, Layers.Compose(), Now, Entry.bPercent));
	}
	return Rows;
}

TArray<FBattleIntelRow> BattleIntelView::BuildPillars(const UCharacterDataComponent &Character)
{
	TArray<FBattleIntelRow> Rows;
	for (const EWorldPillar Pillar : {EWorldPillar::Mind, EWorldPillar::Body, EWorldPillar::Spirit})
	{
		FBattleIntelRow Row;
		Row.Label = WoRGameNames::GetPillarName(Pillar);
		Row.Now = FText::AsNumber(Character.GetWorldStat(Pillar));
		Rows.Add(Row);
	}
	return Rows;
}

FBattleIntelRow BattleIntelView::DescribeEffect(AActor *Holder, const FActiveSkillEffect &Effect)
{
	FBattleIntelRow Row;
	Row.Label = WoRGameNames::GetSkillEffectTypeName(Effect.EffectType);
	Row.Turns = FormatTurns(Effect);
	Row.Tone = Effect.IsDebuff() ? EBattleIntelTone::Worse : Effect.IsBuff() ? EBattleIntelTone::Better : EBattleIntelTone::Plain;

	if (Effect.IsDOT() || Effect.DealsDamage())
	{
		Row.Now = FText::Format(LOCTEXT("DamageATurn", "-{0} HP a turn"), FText::AsNumber(USkillEffectManager::GetDotTickDamage(Holder, Effect)));
		Row.Tone = EBattleIntelTone::Worse;
		return Row;
	}
	if (const FText Restore = DescribeRestore(Effect.EffectType, Effect.GetStackedValue()); !Restore.IsEmpty())
	{
		Row.Now = Restore;
		Row.Tone = EBattleIntelTone::Better;
		return Row;
	}
	if (const FText NoNumber = DescribeNoNumber(Effect.EffectType); !NoNumber.IsEmpty())
	{
		Row.Now = NoNumber;
		Row.Tone = EBattleIntelTone::Worse;
		return Row;
	}
	const float Value = Effect.GetStackedValue();
	if (SkillEffectClassification::CountsWholeActions(Effect.EffectType))
	{
		const int32 Actions = FMath::RoundToInt(FMath::Abs(Value));
		Row.Now = Effect.IsDebuff() ? FText::Format(LOCTEXT("LessActions", "-{0} {0}|plural(one=action,other=actions)"), FText::AsNumber(Actions))
									: FText::Format(LOCTEXT("MoreActions", "+{0} {0}|plural(one=action,other=actions)"), FText::AsNumber(Actions));
		return Row;
	}
	if (!FMath::IsNearlyZero(Value))
	{
		Row.Now = SignedPercent(Value, Effect.IsDebuff() || Value < 0.0f);
		return Row;
	}
	Row.Now = FText::FromString(Effect.Description); // a number-less effect the list above doesn't name
	return Row;
}

FName BattleIntelView::GetEnemyTypeKey(const AActor *Enemy)
{
	const UCharacterDataComponent *Character = Enemy ? Enemy->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	return Character && Character->CharacterData ? UPoolSubsystem::GetCharacterKey(Character) : NAME_None;
}

void BattleIntelView::BuildElementLines(AActor *Enemy, TFunctionRef<bool(EWoRDamageType)> IsLearned,
										TArray<FBattleIntelRow> &OutWeakTo, TArray<FBattleIntelRow> &OutResists)
{
	OutWeakTo.Reset();
	OutResists.Reset();
	bool bAnyUnknown = false;
	for (const ESpellElement Element : INTEL_ELEMENTS)
	{
		if (!IsLearned(WoRDamageType::ElementToDamageType(Element)))
		{
			bAnyUnknown = true;
			continue;
		}
		// The damage step's element multiplier, capped there (the scope line, spell or physical, is left out).
		const float Multiplier = WoRDamageMath::GetElementMultiplier(WoRResistance::GetElementDamageResistance(Enemy, Element), 0.0f);
		const float Change = Multiplier - 1.0f;
		if (FMath::Abs(Change) < ELEMENT_NEUTRAL_EPSILON)
		{
			continue;
		}
		FBattleIntelRow Row;
		Row.Label = WoRGameNames::GetElementName(Element);
		Row.Now = SignedPercent(Change * PERCENT, Change < 0.0f);
		// Coloured for the player: a weakness is good news (green), a resistance bad (red), as the picture.
		Row.Tone = Change > 0.0f ? EBattleIntelTone::Better : EBattleIntelTone::Worse;
		(Change > 0.0f ? OutWeakTo : OutResists).Add(Row);
	}
	for (TArray<FBattleIntelRow> *Lines : {&OutWeakTo, &OutResists})
	{
		if (bAnyUnknown)
		{
			FBattleIntelRow Rest;
			Rest.Label = Lines->IsEmpty() ? LOCTEXT("NotLearned", "Not learned yet") : LOCTEXT("TheRest", "The rest");
			Rest.Now = INVTEXT("?");
			Rest.Tone = EBattleIntelTone::Unknown;
			Lines->Add(Rest);
		}
		else if (Lines->IsEmpty())
		{
			FBattleIntelRow None;
			None.Label = LOCTEXT("NoneLearned", "None");
			Lines->Add(None);
		}
	}
}

bool BattleIntelView::IsYou(const AActor *Actor)
{
	const UCharacterDataComponent *Character = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (!Character || !WoRPlayerBuild::IsPlayerCharacter(Character->CharacterData))
	{
		return false;
	}
	// D-261: another co-op player's character keeps its name; off the roster (a test) the player character is yours.
	const URunStateComponent *Run = URunStateComponent::Find(Actor);
	const UWorld *World = Actor->GetWorld();
	if (Run && World && Run->FindRosterEntry(Actor))
	{
		return Run->IsCommandedBy(Actor, World->GetFirstPlayerController());
	}
	return true;
}

FBattleIntelPage BattleIntelView::BuildPage(const FBattleHUDCard &Card, const TArray<FActiveSkillEffect> &Effects,
											const UTrialRunSubsystem *Run)
{
	FBattleIntelPage Page;
	AActor *Actor = Card.Actor.Get();
	Page.Actor = Actor;
	Page.bEnemy = !Card.bParty;
	Page.Heading = Card.Name;
	Page.Tab = !Page.bEnemy && IsYou(Actor) ? LOCTEXT("YouTab", "You") : Card.Name;
	Page.HPFraction = Card.HPFraction;
	Page.HPLine = Page.bEnemy ? FText::GetEmpty() : Card.HPLine; // Q-391: an enemy's HP number stays hidden
	Page.bShowEP = !Page.bEnemy && Card.bShowEP;
	Page.EPFraction = Card.EPFraction;
	Page.EPLine = Card.EPLine;
	Page.BuildupFraction = Card.BuildupFraction;
	Page.BuildupLine = Card.BuildupLine;
	for (const FActiveSkillEffect &Effect : Effects)
	{
		Page.Effects.Add(DescribeEffect(Actor, Effect));
	}

	const UCharacterDataComponent *Character = Card.CharData.Get();
	if (Page.bEnemy)
	{
		Page.Subline = LOCTEXT("EnemySubline", "? means not learned yet");
		const FName TypeKey = GetEnemyTypeKey(Actor);
		BuildElementLines(Actor, [Run, TypeKey](EWoRDamageType Type) { return Run && Run->IsIntelLearned(TypeKey, Type); },
						  Page.WeakTo, Page.Resists);
		return Page;
	}
	if (Character)
	{
		Page.Subline = WoRPlayerBuild::GetDetailLine(Character->CharacterData, Character->PartyMemberId, Actor ? Actor->GetWorld() : nullptr);
		Page.Pillars = BuildPillars(*Character);
	}
	Page.Stats = BuildStats(Actor);
	return Page;
}

FString BattleIntelView::GetPageString(const FBattleIntelPage &Page)
{
	FString Out = FString::Printf(TEXT("tab=%s heading=%s%s"), *Page.Tab.ToString(), *Page.Heading.ToString(),
								  Page.bEnemy ? TEXT(" (enemy)") : TEXT(""));
	const auto AddSection = [&Out](const TCHAR *Name, const TArray<FBattleIntelRow> &Rows)
	{
		for (const FBattleIntelRow &Row : Rows)
		{
			Out += FString::Printf(TEXT("\n  %s: %s %s%s%s"), Name, *Row.Label.ToString(),
								   Row.Before.IsEmpty() ? TEXT("") : *(Row.Before.ToString() + TEXT(" -> ")), *Row.Now.ToString(),
								   Row.Turns.IsEmpty() ? TEXT("") : *(TEXT(", ") + Row.Turns.ToString()));
		}
	};
	AddSection(TEXT("pillar"), Page.Pillars);
	AddSection(TEXT("stat"), Page.Stats);
	AddSection(TEXT("weak"), Page.WeakTo);
	AddSection(TEXT("resists"), Page.Resists);
	AddSection(TEXT("effect"), Page.Effects);
	return Out;
}

#undef LOCTEXT_NAMESPACE
