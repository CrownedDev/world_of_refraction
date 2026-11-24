// WeaponTrailSpec.cpp
// Crown 2026-11-24: the weapon trail's rules: which weapon types trail, its tint, the fallback window (first Impact's
// wind-up to the last Impact), the long-axis ends when the weapon has no trail sockets, the per-element, Physical and Raw
// glow lookup, and the per-level look (brighter and bigger at higher levels).

#include "Combat/Presentation/WeaponTrail.h"
#include "Combat/Presentation/ImpactVisualDefaults.h"
#include "NiagaraSystem.h"
#include "Infusion/ElementColors.h"
#include "Misc/AutomationTest.h"

BEGIN_DEFINE_SPEC(FWeaponTrailSpec, "WoR.Presentation.WeaponTrail",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FWeaponTrailSpec)

void FWeaponTrailSpec::Define()
{
	It("gives every melee weapon the one trail, and none to bows, guns and shields", [this]()
	{
		for (const EWeaponType Type : {EWeaponType::Sword, EWeaponType::Greatsword, EWeaponType::Axe, EWeaponType::Scythe, EWeaponType::Spear,
									   EWeaponType::Dagger, EWeaponType::Hammer, EWeaponType::Staff, EWeaponType::Gauntlets})
		{
			TestTrue(UEnum::GetValueAsString(Type), WeaponTrail::HasTrail(Type));
		}
		TestFalse(TEXT("bow"), WeaponTrail::HasTrail(EWeaponType::Bow));
		TestFalse(TEXT("gun"), WeaponTrail::HasTrail(EWeaponType::Gun));
		TestFalse(TEXT("shield"), WeaponTrail::HasTrail(EWeaponType::Shield));
	});

	It("finds an element's default infusion glow, and none for an element without one", [this]()
	{
		UImpactVisualDefaults *Defaults = NewObject<UImpactVisualDefaults>();
		const FSoftObjectPath FireGlow(TEXT("/Game/Test/NS_FireGlow.NS_FireGlow"));
		Defaults->ElementInfusionGlowVFX.Add(ESpellElement::Fire, TSoftObjectPtr<UNiagaraSystem>(FireGlow));
		TestEqual(TEXT("fire's glow"), Defaults->GetInfusionGlowVFX(ESpellElement::Fire).ToSoftObjectPath(), FireGlow);
		TestTrue(TEXT("water has none"), Defaults->GetInfusionGlowVFX(ESpellElement::Water).IsNull());
	});

	It("gives Physical and Raw infusions their own default glows", [this]()
	{
		UImpactVisualDefaults *Defaults = NewObject<UImpactVisualDefaults>();
		const FSoftObjectPath FireGlow(TEXT("/Game/Test/NS_FireGlow.NS_FireGlow"));
		const FSoftObjectPath PhysicalGlow(TEXT("/Game/Test/NS_PhysicalGlow.NS_PhysicalGlow"));
		const FSoftObjectPath RawGlow(TEXT("/Game/Test/NS_RawGlow.NS_RawGlow"));
		Defaults->ElementInfusionGlowVFX.Add(ESpellElement::Fire, TSoftObjectPtr<UNiagaraSystem>(FireGlow));
		Defaults->PhysicalInfusionGlowVFX = TSoftObjectPtr<UNiagaraSystem>(PhysicalGlow);
		Defaults->RawInfusionGlowVFX = TSoftObjectPtr<UNiagaraSystem>(RawGlow);
		TestEqual(TEXT("no element is Physical"), Defaults->GetInfusionGlowVFX(ESpellElement::None).ToSoftObjectPath(), PhysicalGlow);
		TestEqual(TEXT("an unresolved Generic is Physical"), Defaults->GetInfusionGlowVFX(ESpellElement::Generic).ToSoftObjectPath(), PhysicalGlow);
		TestEqual(TEXT("Raw is Raw's, whatever the element"), Defaults->GetInfusionGlowVFX(ESpellElement::Fire, true).ToSoftObjectPath(), RawGlow);
		TestEqual(TEXT("fire keeps fire's"), Defaults->GetInfusionGlowVFX(ESpellElement::Fire).ToSoftObjectPath(), FireGlow);
	});

	It("gives a Water infusion that deals Ice the Ice glow, and only Water", [this]()
	{
		UImpactVisualDefaults *Defaults = NewObject<UImpactVisualDefaults>();
		const FSoftObjectPath WaterGlow(TEXT("/Game/Test/NS_WaterGlow.NS_WaterGlow"));
		const FSoftObjectPath FireGlow(TEXT("/Game/Test/NS_FireGlow.NS_FireGlow"));
		const FSoftObjectPath IceGlow(TEXT("/Game/Test/NS_IceGlow.NS_IceGlow"));
		const FSoftObjectPath RawGlow(TEXT("/Game/Test/NS_RawGlow.NS_RawGlow"));
		Defaults->ElementInfusionGlowVFX.Add(ESpellElement::Water, TSoftObjectPtr<UNiagaraSystem>(WaterGlow));
		Defaults->ElementInfusionGlowVFX.Add(ESpellElement::Fire, TSoftObjectPtr<UNiagaraSystem>(FireGlow));
		Defaults->IceInfusionGlowVFX = TSoftObjectPtr<UNiagaraSystem>(IceGlow);
		Defaults->RawInfusionGlowVFX = TSoftObjectPtr<UNiagaraSystem>(RawGlow);
		TestEqual(TEXT("Water dealing Ice is Ice's"), Defaults->GetInfusionGlowVFX(ESpellElement::Water, false, true).ToSoftObjectPath(), IceGlow);
		TestEqual(TEXT("plain Water keeps Water's"), Defaults->GetInfusionGlowVFX(ESpellElement::Water).ToSoftObjectPath(), WaterGlow);
		TestEqual(TEXT("Ice needs Water"), Defaults->GetInfusionGlowVFX(ESpellElement::Fire, false, true).ToSoftObjectPath(), FireGlow);
		TestEqual(TEXT("Raw wins"), Defaults->GetInfusionGlowVFX(ESpellElement::Water, true, true).ToSoftObjectPath(), RawGlow);

		Defaults->IceInfusionGlowVFX.Reset();
		TestEqual(TEXT("no Ice row yet: Water's"), Defaults->GetInfusionGlowVFX(ESpellElement::Water, false, true).ToSoftObjectPath(), WaterGlow);
	});

	It("plays a higher infusion level brighter and bigger, from one table", [this]()
	{
		const UImpactVisualDefaults *Defaults = NewObject<UImpactVisualDefaults>();
		const FInfusionLevelVisual Off = Defaults->GetInfusionLevelVisual(0);
		const FInfusionLevelVisual One = Defaults->GetInfusionLevelVisual(1);
		const FInfusionLevelVisual Two = Defaults->GetInfusionLevelVisual(2);
		TestEqual(TEXT("level 0 is plain brightness"), Off.Intensity, 1.0f);
		TestEqual(TEXT("level 0 is plain size"), Off.Size, 1.0f);
		TestTrue(TEXT("level 1 is bigger than none"), One.Size > Off.Size);
		TestTrue(TEXT("level 2 is brighter than level 1"), Two.Intensity > One.Intensity);
		TestTrue(TEXT("level 2 is bigger than level 1"), Two.Size > One.Size);
		TestEqual(TEXT("a level past the table uses the last row"), Defaults->GetInfusionLevelVisual(5).Size, Two.Size);

		UImpactVisualDefaults *Empty = NewObject<UImpactVisualDefaults>();
		Empty->InfusionLevelVisuals.Empty();
		TestEqual(TEXT("no rows: plain size"), Empty->GetInfusionLevelVisual(2).Size, 1.0f);

		const FLinearColor Brightened = Two.Brighten(FLinearColor(0.2f, 0.4f, 0.1f, 0.5f));
		TestEqual(TEXT("brightens red by the intensity"), Brightened.R, 0.2f * Two.Intensity);
		TestEqual(TEXT("keeps the alpha"), Brightened.A, 0.5f);
	});

	It("tints the trail at level 0 exactly as its element's colour", [this]()
	{
		const FLinearColor Plain(0.5f, 0.6f, 0.7f, 1.0f);
		TestEqual(TEXT("fire at level 0"), WeaponTrail::TintFor(ESpellElement::Fire, 0, Plain), WeaponTrail::ColorFor(ESpellElement::Fire, Plain));
		TestEqual(TEXT("plain at level 0"), WeaponTrail::TintFor(ESpellElement::None, 0, Plain), Plain);
	});

	It("tints by the infusion's element, and plain with no infusion, a Physical or a Raw one", [this]()
	{
		const FLinearColor Plain(0.5f, 0.6f, 0.7f, 1.0f);
		TestEqual(TEXT("no element is plain"), WeaponTrail::ColorFor(ESpellElement::None, Plain), Plain);
		TestEqual(TEXT("generic is plain"), WeaponTrail::ColorFor(ESpellElement::Generic, Plain), Plain);
		TestEqual(TEXT("fire is fire's colour"), WeaponTrail::ColorFor(ESpellElement::Fire, Plain), ElementColors::GetColorForElement(ESpellElement::Fire));
	});

	It("runs a montage without a window from the first Impact's wind-up to the last Impact", [this]()
	{
		float Start = -1.0f, End = -1.0f;
		TestTrue(TEXT("two impacts"), WeaponTrail::FindFallbackWindow({0.5f, 0.9f}, 0.25f, Start, End));
		TestEqual(TEXT("starts a wind-up before the first"), Start, 0.25f);
		TestEqual(TEXT("ends on the last"), End, 0.9f);

		TestTrue(TEXT("an early impact"), WeaponTrail::FindFallbackWindow({0.1f}, 0.25f, Start, End));
		TestEqual(TEXT("never starts before the montage"), Start, 0.0f);
		TestEqual(TEXT("ends on it"), End, 0.1f);

		TestFalse(TEXT("no impact, no trail"), WeaponTrail::FindFallbackWindow({}, 0.25f, Start, End));
		TestFalse(TEXT("an impact on the first frame leaves no window"), WeaponTrail::FindFallbackWindow({0.0f}, 0.25f, Start, End));
	});

	It("runs the ribbon along the weapon's longest axis when it has no trail sockets", [this]()
	{
		FVector Start, End;
		WeaponTrail::LongAxisPoints(FBox(FVector(-2.0f, -1.0f, 0.0f), FVector(2.0f, 1.0f, 100.0f)), Start, End);
		TestEqual(TEXT("from the bottom of the blade's axis"), Start, FVector(0.0f, 0.0f, 0.0f));
		TestEqual(TEXT("to its tip"), End, FVector(0.0f, 0.0f, 100.0f));

		WeaponTrail::LongAxisPoints(FBox(FVector(-60.0f, -3.0f, -3.0f), FVector(20.0f, 3.0f, 3.0f)), Start, End);
		TestEqual(TEXT("a spear along X, start"), Start, FVector(-60.0f, 0.0f, 0.0f));
		TestEqual(TEXT("a spear along X, end"), End, FVector(20.0f, 0.0f, 0.0f));
	});
}
