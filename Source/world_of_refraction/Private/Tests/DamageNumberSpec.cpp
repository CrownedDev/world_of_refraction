// DamageNumberSpec.cpp
// B-391b (Q-111, Q-391b): the floating damage number's look, headless. Colour per damage type (Physical white),
// size and brightness by the element multiplier, immune grey, heals green, and the Backlog's "done when": a weak,
// a resisted and an immune hit give three distinct styles. Motion and placement are PIE (docs/PIE/B-391b.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRGameNames.h"
#include "Infusion/ElementColors.h"
#include "UI/Combat/Battle/DamageNumberView.h"

namespace
{
	constexpr float DAMAGE_NUMBER_WEAK = 1.5f;
	constexpr float DAMAGE_NUMBER_RESISTED = 0.5f;
	constexpr float DAMAGE_NUMBER_IMMUNE = 0.0f;
	constexpr int32 DAMAGE_NUMBER_AMOUNT = 45;

	FCombatHitShown MakeDamageNumberHit(float Multiplier, EWoRDamageType Type, bool bCritical = false)
	{
		FCombatHitShown Hit;
		Hit.Amount = DAMAGE_NUMBER_AMOUNT;
		Hit.ElementMultiplier = Multiplier;
		Hit.DamageType = Type;
		Hit.bCritical = bCritical;
		return Hit;
	}

	bool DamageNumberSameStyle(const FDamageNumberStyle &A, const FDamageNumberStyle &B)
	{
		return A.Tier == B.Tier && A.FontSize == B.FontSize && A.Colour.Equals(B.Colour) && A.bGlow == B.bGlow;
	}
}

BEGIN_DEFINE_SPEC(FDamageNumberSpec, "WoR.UI.DamageNumber", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDamageNumberSpec)

void FDamageNumberSpec::Define()
{
	It("colours a hit by its damage type, Physical and typeless white", [this]()
	{
		TestTrue(TEXT("Fire"), DamageNumberView::GetDamageTypeColour(EWoRDamageType::Fire).Equals(ElementColors::Fire));
		TestTrue(TEXT("Water"), DamageNumberView::GetDamageTypeColour(EWoRDamageType::Water).Equals(ElementColors::Water));
		TestTrue(TEXT("Darkness"), DamageNumberView::GetDamageTypeColour(EWoRDamageType::Darkness).Equals(ElementColors::Darkness));
		TestTrue(TEXT("Slash is white"), DamageNumberView::GetDamageTypeColour(EWoRDamageType::Slash).Equals(ElementColors::Physical));
		TestTrue(TEXT("Impact is white"), DamageNumberView::GetDamageTypeColour(EWoRDamageType::Impact).Equals(ElementColors::Physical));
		TestTrue(TEXT("typeless is white, not Generic brown"),
				 DamageNumberView::GetDamageTypeColour(EWoRDamageType::None).Equals(ElementColors::Physical));
		TestFalse(TEXT("Ice is its own, not white"), DamageNumberView::GetDamageTypeColour(EWoRDamageType::Ice).Equals(ElementColors::Physical));
	});

	It("grows and brightens with the multiplier, and clamps", [this]()
	{
		TestTrue(TEXT("weak is bigger"), DamageNumberView::GetScale(DAMAGE_NUMBER_WEAK) > DamageNumberView::GetScale(1.0f));
		TestTrue(TEXT("resisted is smaller"), DamageNumberView::GetScale(DAMAGE_NUMBER_RESISTED) < DamageNumberView::GetScale(1.0f));
		TestEqual(TEXT("neutral is 1"), DamageNumberView::GetScale(1.0f), 1.0f);
		TestTrue(TEXT("weak is brighter"), DamageNumberView::GetBrightness(DAMAGE_NUMBER_WEAK) > 1.0f);
		TestTrue(TEXT("resisted is dimmer"), DamageNumberView::GetBrightness(DAMAGE_NUMBER_RESISTED) < 1.0f);
		TestEqual(TEXT("clamped above"), DamageNumberView::GetScale(10.0f), DamageNumberView::MAX_SCALE);
		TestEqual(TEXT("clamped below"), DamageNumberView::GetScale(0.01f), DamageNumberView::MIN_SCALE);
	});

	It("gives a weak, a resisted and an immune hit three distinct styles", [this]()
	{
		const FDamageNumberStyle Weak = DamageNumberView::MakeStyle(MakeDamageNumberHit(DAMAGE_NUMBER_WEAK, EWoRDamageType::Fire));
		const FDamageNumberStyle Neutral = DamageNumberView::MakeStyle(MakeDamageNumberHit(1.0f, EWoRDamageType::Fire));
		const FDamageNumberStyle Resisted = DamageNumberView::MakeStyle(MakeDamageNumberHit(DAMAGE_NUMBER_RESISTED, EWoRDamageType::Fire));
		const FDamageNumberStyle Immune = DamageNumberView::MakeStyle(MakeDamageNumberHit(DAMAGE_NUMBER_IMMUNE, EWoRDamageType::Fire));
		TestTrue(TEXT("weak tier"), Weak.Tier == EDamageNumberTier::Weak);
		TestTrue(TEXT("resisted tier"), Resisted.Tier == EDamageNumberTier::Resisted);
		TestTrue(TEXT("immune tier"), Immune.Tier == EDamageNumberTier::Immune);
		TestFalse(TEXT("weak != resisted"), DamageNumberSameStyle(Weak, Resisted));
		TestFalse(TEXT("weak != immune"), DamageNumberSameStyle(Weak, Immune));
		TestFalse(TEXT("resisted != immune"), DamageNumberSameStyle(Resisted, Immune));
		TestTrue(TEXT("weak glows, bigger than neutral"), Weak.bGlow && Weak.FontSize > Neutral.FontSize);
		TestTrue(TEXT("resisted: no glow, smaller than neutral"), !Resisted.bGlow && Resisted.FontSize < Neutral.FontSize);
		TestTrue(TEXT("neutral is the plain element colour"), Neutral.Colour.Equals(ElementColors::Fire));
		TestEqual(TEXT("the amount, no label"), Weak.Text.ToString(), FText::AsNumber(DAMAGE_NUMBER_AMOUNT).ToString());
	});

	It("shows immune grey and small, whatever the element", [this]()
	{
		const FDamageNumberStyle Fire = DamageNumberView::MakeStyle(MakeDamageNumberHit(DAMAGE_NUMBER_IMMUNE, EWoRDamageType::Fire));
		const FDamageNumberStyle Slash = DamageNumberView::MakeStyle(MakeDamageNumberHit(DAMAGE_NUMBER_IMMUNE, EWoRDamageType::Slash));
		TestTrue(TEXT("grey"), Fire.Colour.Equals(DamageNumberView::IMMUNE_COLOUR) && Slash.Colour.Equals(DamageNumberView::IMMUNE_COLOUR));
		TestEqual(TEXT("small"), Fire.Scale, DamageNumberView::IMMUNE_SCALE);
		FCombatHitShown ZeroImmune = MakeDamageNumberHit(DAMAGE_NUMBER_IMMUNE, EWoRDamageType::Fire);
		ZeroImmune.Amount = 0;
		TestTrue(TEXT("an immune 0 still shows"), DamageNumberView::ShouldShow(ZeroImmune));
		FCombatHitShown Negated = MakeDamageNumberHit(1.0f, EWoRDamageType::Fire);
		Negated.Amount = 0;
		TestFalse(TEXT("a fully negated hit shows nothing"), DamageNumberView::ShouldShow(Negated));
	});

	It("shows a heal green with a plus", [this]()
	{
		FCombatHitShown Heal;
		Heal.Amount = DAMAGE_NUMBER_AMOUNT;
		Heal.bHeal = true;
		const FDamageNumberStyle Style = DamageNumberView::MakeStyle(Heal);
		TestTrue(TEXT("heal tier"), Style.Tier == EDamageNumberTier::Heal);
		TestTrue(TEXT("green"), Style.Colour.Equals(DamageNumberView::HEAL_COLOUR));
		TestTrue(TEXT("a plus"), Style.Text.ToString().StartsWith(TEXT("+")));
	});

	It("makes a crit bigger and outlines Darkness pale", [this]()
	{
		const FDamageNumberStyle Plain = DamageNumberView::MakeStyle(MakeDamageNumberHit(1.0f, EWoRDamageType::Slash));
		const FDamageNumberStyle Crit = DamageNumberView::MakeStyle(MakeDamageNumberHit(1.0f, EWoRDamageType::Slash, true));
		TestTrue(TEXT("crit bigger"), Crit.FontSize > Plain.FontSize);
		const FDamageNumberStyle Dark = DamageNumberView::MakeStyle(MakeDamageNumberHit(1.0f, EWoRDamageType::Darkness));
		TestTrue(TEXT("pale outline"), Dark.OutlineColour.Equals(ElementColors::TextOutline));
	});

	It("dims a resisted dark element toward grey, not black (review)", [this]()
	{
		const FDamageNumberStyle Dark = DamageNumberView::MakeStyle(MakeDamageNumberHit(DAMAGE_NUMBER_RESISTED, EWoRDamageType::Darkness));
		TestTrue(TEXT("lighter than plain Darkness"), Dark.Colour.GetLuminance() > ElementColors::Darkness.GetLuminance());
		const FDamageNumberStyle Fire = DamageNumberView::MakeStyle(MakeDamageNumberHit(DAMAGE_NUMBER_RESISTED, EWoRDamageType::Fire));
		TestFalse(TEXT("still not the immune grey"), Fire.Colour.Equals(DamageNumberView::IMMUNE_COLOUR));
	});

	It("starts a new number above the highest live one on its target (review)", [this]()
	{
		TestEqual(TEXT("none up: row 0"), DamageNumberView::NextStackIndex({}), 0);
		TestEqual(TEXT("rows 0 and 1 up: row 2"), DamageNumberView::NextStackIndex({0, 1}), 2);
		TestEqual(TEXT("row 0 expired, row 1 still up: row 2, not 1"), DamageNumberView::NextStackIndex({1}), 2);
	});

	It("rises and fades over its life", [this]()
	{
		TestEqual(TEXT("starts in place"), DamageNumberView::GetRiseOffset(0.0f), 0.0f);
		TestEqual(TEXT("ends risen"), DamageNumberView::GetRiseOffset(DamageNumberView::LIFETIME_SECONDS), DamageNumberView::RISE_DISTANCE);
		TestEqual(TEXT("opaque at first"), DamageNumberView::GetOpacity(0.0f), 1.0f);
		TestEqual(TEXT("gone at the end"), DamageNumberView::GetOpacity(DamageNumberView::LIFETIME_SECONDS), 0.0f);
	});

	It("tags an elemental number with its letters when asked, never a physical one (B-453f)", [this]()
	{
		const FCombatHitShown FireHit = MakeDamageNumberHit(1.0f, EWoRDamageType::Fire);
		const FString Plain = DamageNumberView::MakeStyle(FireHit).Text.ToString();
		const FString Tagged = DamageNumberView::MakeStyle(FireHit, /*bElementLetters=*/true).Text.ToString();
		TestTrue(TEXT("the number stays"), Tagged.StartsWith(Plain));
		TestTrue(TEXT("Fire's letters after it"), Tagged.EndsWith(WoRGameNames::GetElementLetter(ESpellElement::Fire).ToString()));
		const FCombatHitShown SlashHit = MakeDamageNumberHit(1.0f, EWoRDamageType::Slash);
		TestEqual(TEXT("no tag on a physical hit"), DamageNumberView::MakeStyle(SlashHit, true).Text.ToString(),
				  DamageNumberView::MakeStyle(SlashHit).Text.ToString());
		TestNotEqual(TEXT("Light and Lightning differ"), WoRGameNames::GetElementLetter(ESpellElement::Light).ToString(),
					 WoRGameNames::GetElementLetter(ESpellElement::Lightning).ToString());
	});
}

#endif
