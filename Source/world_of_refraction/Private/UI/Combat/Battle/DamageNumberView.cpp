// DamageNumberView.cpp

#include "UI/Combat/Battle/DamageNumberView.h"

#include "Combat/Elements/ElementTable.h"
#include "Infusion/ElementColors.h"

#define LOCTEXT_NAMESPACE "DamageNumber"

EDamageNumberTier DamageNumberView::ClassifyHit(const FCombatHitShown &Hit)
{
	if (Hit.bHeal)
	{
		return EDamageNumberTier::Heal;
	}
	if (Hit.ElementMultiplier <= IMMUNE_MULTIPLIER)
	{
		return EDamageNumberTier::Immune;
	}
	if (Hit.ElementMultiplier > 1.0f + NEUTRAL_BAND)
	{
		return EDamageNumberTier::Weak;
	}
	if (Hit.ElementMultiplier < 1.0f - NEUTRAL_BAND)
	{
		return EDamageNumberTier::Resisted;
	}
	return EDamageNumberTier::Neutral;
}

FLinearColor DamageNumberView::GetDamageTypeColour(EWoRDamageType DamageType)
{
	if (DamageType == EWoRDamageType::None || WoRDamageType::IsPhysical(DamageType))
	{
		return ElementColors::Physical; // the None row paints Generic brown, never a damage-number colour
	}
	return ElementTable::For(DamageType).Colour;
}

float DamageNumberView::GetScale(float ElementMultiplier)
{
	if (ElementMultiplier <= IMMUNE_MULTIPLIER)
	{
		return IMMUNE_SCALE;
	}
	return FMath::Clamp(1.0f + (ElementMultiplier - 1.0f) * SCALE_PER_MULTIPLIER, MIN_SCALE, MAX_SCALE);
}

float DamageNumberView::GetBrightness(float ElementMultiplier)
{
	return FMath::Clamp(1.0f + (ElementMultiplier - 1.0f) * BRIGHTNESS_PER_MULTIPLIER, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
}

namespace
{
	/** Above 1 moves the colour toward white; below 1 toward DIM_COLOUR, so dark elements stay readable. Alpha kept. */
	FLinearColor BrightenDamageColour(const FLinearColor &Colour, float Brightness)
	{
		if (Brightness >= 1.0f)
		{
			FLinearColor Lit = FMath::Lerp(Colour, FLinearColor::White, FMath::Clamp(Brightness - 1.0f, 0.0f, 1.0f));
			Lit.A = Colour.A;
			return Lit;
		}
		FLinearColor Dim = FMath::Lerp(Colour, DamageNumberView::DIM_COLOUR, FMath::Clamp(1.0f - Brightness, 0.0f, 1.0f));
		Dim.A = Colour.A;
		return Dim;
	}
}

FDamageNumberStyle DamageNumberView::MakeStyle(const FCombatHitShown &Hit)
{
	FDamageNumberStyle Style;
	Style.Tier = ClassifyHit(Hit);
	Style.OutlineColour = OUTLINE_COLOUR;
	Style.OutlineSize = OUTLINE_SIZE;

	switch (Style.Tier)
	{
	case EDamageNumberTier::Heal:
		Style.Text = FText::Format(LOCTEXT("Heal", "+{0}"), FText::AsNumber(Hit.Amount));
		Style.Colour = HEAL_COLOUR;
		Style.Scale = HEAL_SCALE;
		break;
	case EDamageNumberTier::Immune:
		Style.Text = FText::AsNumber(Hit.Amount);
		Style.Colour = IMMUNE_COLOUR;
		Style.Scale = IMMUNE_SCALE;
		break;
	default:
	{
		Style.Text = FText::AsNumber(Hit.Amount);
		Style.Brightness = GetBrightness(Hit.ElementMultiplier);
		Style.Colour = BrightenDamageColour(GetDamageTypeColour(Hit.DamageType), Style.Brightness);
		const float CritBonus = Hit.bCritical ? CRIT_SCALE_BONUS : 0.0f;
		Style.Scale = FMath::Clamp(GetScale(Hit.ElementMultiplier) + CritBonus, MIN_SCALE, MAX_SCALE + CRIT_SCALE_BONUS);
		Style.bGlow = Style.Tier == EDamageNumberTier::Weak;
		if (Style.bGlow)
		{
			Style.OutlineColour = GetDamageTypeColour(Hit.DamageType);
			Style.OutlineColour.A = GLOW_ALPHA;
			Style.OutlineSize = GLOW_OUTLINE_SIZE;
		}
		if (ElementColors::NeedsTextOutline(ElementTable::For(Hit.DamageType).Element))
		{
			Style.OutlineColour = ElementColors::TextOutline; // near-black Darkness needs its pale outline (D-106)
		}
		break;
	}
	}
	Style.FontSize = FMath::Max(1, FMath::RoundToInt(BASE_FONT_SIZE * Style.Scale));
	return Style;
}

bool DamageNumberView::ShouldShow(const FCombatHitShown &Hit)
{
	return Hit.Amount > 0 || (!Hit.bHeal && Hit.ElementMultiplier <= IMMUNE_MULTIPLIER);
}

bool DamageNumberView::IsNegatedHit(const FCombatHitShown &Hit, bool bTargetBrokenDarkness)
{
	return bTargetBrokenDarkness && !Hit.bHeal && Hit.Amount <= 0 && Hit.ElementMultiplier > IMMUNE_MULTIPLIER;
}

FText DamageNumberView::GetNegatedLabel()
{
	return LOCTEXT("Negated", "Negated"); // Crown, 2026-11-12 (Q-391c): not "Absorbed"
}

FDamageNumberStyle DamageNumberView::MakeNegatedStyle(const FCombatHitShown &Hit)
{
	FDamageNumberStyle Style;
	Style.Tier = EDamageNumberTier::Negated;
	Style.Text = GetNegatedLabel();
	Style.Colour = GetDamageTypeColour(Hit.DamageType);
	Style.Scale = NEGATED_SCALE;
	Style.OutlineColour = ElementColors::NeedsTextOutline(ElementTable::For(Hit.DamageType).Element) ? ElementColors::TextOutline
																									  : OUTLINE_COLOUR;
	Style.OutlineSize = OUTLINE_SIZE;
	Style.FontSize = FMath::Max(1, FMath::RoundToInt(BASE_FONT_SIZE * Style.Scale));
	return Style;
}

int32 DamageNumberView::NextStackIndex(const TArray<int32> &LiveStackIndices)
{
	int32 Highest = INDEX_NONE;
	for (const int32 Index : LiveStackIndices)
	{
		Highest = FMath::Max(Highest, Index);
	}
	return Highest + 1;
}

float DamageNumberView::GetRiseOffset(float AgeSeconds)
{
	return RISE_DISTANCE * FMath::Clamp(AgeSeconds / LIFETIME_SECONDS, 0.0f, 1.0f);
}

float DamageNumberView::GetOpacity(float AgeSeconds)
{
	const float FadeStart = LIFETIME_SECONDS * FADE_START_FRACTION;
	if (AgeSeconds <= FadeStart)
	{
		return 1.0f;
	}
	return FMath::Clamp(1.0f - (AgeSeconds - FadeStart) / (LIFETIME_SECONDS - FadeStart), 0.0f, 1.0f);
}

FString DamageNumberView::DescribeStyle(const FCombatHitShown &Hit, const FDamageNumberStyle &Style)
{
	static const TCHAR *TierNames[] = {TEXT("weak"), TEXT("neutral"), TEXT("resisted"), TEXT("immune"), TEXT("heal"), TEXT("negated")};
	static_assert(UE_ARRAY_COUNT(TierNames) == static_cast<int32>(EDamageNumberTier::Negated) + 1, "one name per tier");
	const int32 TierIndex = static_cast<int32>(Style.Tier);
	return FString::Printf(TEXT("%s %.2fx %s%s: '%s' size %d bright %.2f%s colour %s"),
						   Hit.bHeal ? TEXT("heal") : *UEnum::GetDisplayValueAsText(Hit.DamageType).ToString(), Hit.ElementMultiplier,
						   TierNames[TierIndex], Hit.bCritical ? TEXT(" crit") : TEXT(""), *Style.Text.ToString(), Style.FontSize,
						   Style.Brightness, Style.bGlow ? TEXT(" glow") : TEXT(""), *Style.Colour.ToFColor(true).ToHex());
}

#undef LOCTEXT_NAMESPACE
