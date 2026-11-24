// WeaponTrailSpec.cpp
// Crown 2026-11-24: the weapon trail's rules: which weapon types trail, its tint, the fallback window (first Impact's
// wind-up to the last Impact), the long-axis ends when the weapon has no trail sockets, and the per-element glow lookup.

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
