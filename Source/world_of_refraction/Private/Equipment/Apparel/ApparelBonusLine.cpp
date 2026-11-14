// ApparelBonusLine.cpp

#include "Equipment/Apparel/ApparelBonusLine.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Math/RandomStream.h"
#include "WoRGameNames.h"

#define LOCTEXT_NAMESPACE "WoRApparelBonusLine"

namespace
{
	constexpr int32 APPAREL_PILLAR_COUNT = 3; // Mind, Body, Spirit
}

const TArray<ESubStat> &ApparelBonusLine::GetRollableSubStats()
{
	static const TArray<ESubStat> SubStats = {ESubStat::Efficiency, ESubStat::SpellDamage, ESubStat::StatusMultiplier, ESubStat::CritDamage,
											  ESubStat::SpellSpeed, ESubStat::Defense, ESubStat::ActionSpeed, ESubStat::RawDamage,
											  ESubStat::Resistance, ESubStat::Luck, ESubStat::Reflex};
	return SubStats;
}

FApparelBonusLine ApparelBonusLine::Roll(FRandomStream &Stream)
{
	const TArray<ESubStat> &SubStats = GetRollableSubStats();
	const int32 Pick = Stream.RandRange(0, SubStats.Num() + APPAREL_PILLAR_COUNT - 1);
	FApparelBonusLine Line;
	if (Pick < SubStats.Num())
	{
		Line.Kind = EApparelBonusKind::SubStat;
		Line.SubStat = SubStats[Pick];
		Line.Value = SUB_STAT_LINE_POINTS;
	}
	else
	{
		Line.Kind = EApparelBonusKind::Pillar;
		Line.Pillar = static_cast<EWorldPillar>(Pick - SubStats.Num());
		Line.Value = PILLAR_LINE_PERCENT;
	}
	return Line;
}

FApparelBonusLine ApparelBonusLine::RollFor(const FGuid &PersistentID)
{
	FRandomStream Stream(static_cast<int32>(GetTypeHash(PersistentID)));
	return Roll(Stream);
}

void ApparelBonusLine::AddTo(const FApparelBonusLine &Line, FEquipmentStatBonus &Lines)
{
	if (Line.Kind == EApparelBonusKind::Pillar)
	{
		float &Percent = Line.Pillar == EWorldPillar::Mind ? Lines.BonusMindModifierPercent
						 : Line.Pillar == EWorldPillar::Body ? Lines.BonusBodyModifierPercent
															 : Lines.BonusSpiritModifierPercent;
		Percent += Line.Value;
		return;
	}
	if (Line.Kind != EApparelBonusKind::SubStat)
	{
		return;
	}
	const int32 Points = FMath::RoundToInt(Line.Value);
	switch (Line.SubStat)
	{
	case ESubStat::Efficiency:
		Lines.BonusEfficiency += Points;
		break;
	case ESubStat::SpellDamage:
		Lines.BonusSpellDamage += Points;
		break;
	case ESubStat::StatusMultiplier:
		Lines.BonusStatusMultiplier += Points;
		break;
	case ESubStat::CritDamage:
		Lines.BonusCritDamage += Line.Value;
		break;
	case ESubStat::SpellSpeed:
		Lines.BonusSpellSpeed += Points;
		break;
	case ESubStat::Defense:
		Lines.BonusDefense += Points;
		break;
	case ESubStat::ActionSpeed:
		Lines.BonusActionSpeed += Points;
		break;
	case ESubStat::RawDamage:
		Lines.BonusRawDamage += Points;
		break;
	case ESubStat::Resistance:
		Lines.BonusResistance += Points;
		break;
	case ESubStat::Luck:
		Lines.BonusLuck += Points;
		break;
	case ESubStat::Reflex:
		Lines.BonusReflex += Points;
		break;
	case ESubStat::TurnSpeed: // D-208: never rolled
	case ESubStat::None:
	default:
		break;
	}
}

FText ApparelBonusLine::GetText(const FApparelBonusLine &Line)
{
	switch (Line.Kind)
	{
	case EApparelBonusKind::SubStat:
		return FText::Format(LOCTEXT("SubStatLine", "+{0} {1}"), FText::AsNumber(FMath::RoundToInt(Line.Value)), WoRGameNames::GetSubStatName(Line.SubStat));
	case EApparelBonusKind::Pillar:
		return FText::Format(LOCTEXT("PillarLine", "+{0}% {1}"), FText::AsNumber(FMath::RoundToInt(Line.Value)), WoRGameNames::GetPillarName(Line.Pillar));
	case EApparelBonusKind::None:
	default:
		return FText::GetEmpty();
	}
}

#undef LOCTEXT_NAMESPACE
