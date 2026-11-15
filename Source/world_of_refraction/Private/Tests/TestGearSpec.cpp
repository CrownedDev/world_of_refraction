// TestGearSpec.cpp
// B-503n (Q-503n): which test crystal the quick fill of wor.Debug.TestGear puts in a socket. The grant and the fill
// themselves run on the played character through the real screen API: PIE (docs/PIE/B-503n.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Debug/Inventory/TestGearDebug.h"

BEGIN_DEFINE_SPEC(FTestGearSpec, "WoR.Debug.TestGear", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TArray<ECrystalType> AllThree = {ECrystalType::Garnet, ECrystalType::Sapphire, ECrystalType::Citrine};
END_DEFINE_SPEC(FTestGearSpec)

void FTestGearSpec::Define()
{
	Describe("the crystal a socket takes", [this]()
	{
		It("gives a weapon Fire and an adapter Water first", [this]()
		{
			TestEqual(TEXT("weapon"), WoRTestGear::PickTestCrystal(ELoadoutScreenRowKind::Weapon, AllThree, {}), ECrystalType::Garnet);
			TestEqual(TEXT("adapter"), WoRTestGear::PickTestCrystal(ELoadoutScreenRowKind::Adapter, AllThree, {}), ECrystalType::Sapphire);
		});

		It("skips an element the loadout already has, so the second weapon gets Lightning", [this]()
		{
			TestEqual(TEXT("second weapon"), WoRTestGear::PickTestCrystal(ELoadoutScreenRowKind::Weapon, AllThree, {ESpellElement::Fire}), ECrystalType::Citrine);
			TestEqual(TEXT("adapter after Water"), WoRTestGear::PickTestCrystal(ELoadoutScreenRowKind::Adapter, AllThree, {ESpellElement::Water}), ECrystalType::Garnet);
		});

		It("takes only what the socket list offers", [this]()
		{
			TestEqual(TEXT("only Water offered"), WoRTestGear::PickTestCrystal(ELoadoutScreenRowKind::Weapon, {ECrystalType::Sapphire}, {}), ECrystalType::Sapphire);
		});

		It("gives nothing when every offered element is taken or nothing is offered", [this]()
		{
			TestEqual(TEXT("all taken"),
					  WoRTestGear::PickTestCrystal(ELoadoutScreenRowKind::Weapon, AllThree, {ESpellElement::Fire, ESpellElement::Water, ESpellElement::Lightning}),
					  ECrystalType::None);
			TestEqual(TEXT("none offered"), WoRTestGear::PickTestCrystal(ELoadoutScreenRowKind::Adapter, {}, {}), ECrystalType::None);
		});
	});
}

#endif
