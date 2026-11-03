// GenericSpellPriceSpec.cpp
// B-296 (D-105), updated by B-333 (D-125): Quartz folded into Elemental, so a Generic spell costs exactly what a
// Fire spell of its tier costs: Skill + Elemental essence at the tier, and the same Prisms.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Currency/EconomyService.h"
#include "Currency/EconomyYield.h"
#include "Merchant/MerchantData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	const EItemTier GENERIC_PRICE_TEST_TIERS[] = {EItemTier::F_Tier, EItemTier::C_Tier, EItemTier::S_Tier};
}

BEGIN_DEFINE_SPEC(FGenericSpellPriceSpec, "WoR.Economy.GenericSpellPrice",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UEconomyService *Economy = nullptr;

	FPurchaseCost CostOf(ESpellElement Element, EItemTier Tier) const
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Element = Element;
		Spell->Tier = Tier;
		FMerchantStockEntry Entry;
		Entry.Asset = Spell;
		return Economy->PreviewCartCost({Entry});
	}
END_DEFINE_SPEC(FGenericSpellPriceSpec)

void FGenericSpellPriceSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Economy = TestWorld->GetSubsystem<UEconomyService>();
	});

	AfterEach([this]()
	{
		Economy = nullptr;
		TestWorld.Reset();
	});

	It("prices a Generic spell in Elemental essence, like an elemental one (D-125)", [this]()
	{
		if (!TestNotNull(TEXT("EconomyService"), Economy)) return;
		for (const EItemTier Tier : GENERIC_PRICE_TEST_TIERS)
		{
			const FString TierName = UEnum::GetValueAsString(Tier);
			const FPurchaseCost Generic = CostOf(ESpellElement::Generic, Tier);
			const FPurchaseCost Fire = CostOf(ESpellElement::Fire, Tier);
			TestEqual(TierName + TEXT(": Elemental essence"), Generic.ElementalEssence,
					  EconomyYield::GetTypedEssencePurchaseCostForTier(Tier));
			TestEqual(TierName + TEXT(": same Elemental as Fire's"), Generic.ElementalEssence, Fire.ElementalEssence);
			TestEqual(TierName + TEXT(": same Skill as Fire's"), Generic.SkillEssence, Fire.SkillEssence);
			TestEqual(TierName + TEXT(": same Prisms as Fire"), Generic.Prisms, Fire.Prisms);
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
