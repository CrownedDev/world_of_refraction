// GenericSpellPriceSpec.cpp
// B-296 (D-105, Crown A): Quartz essence buys Generic spells, as each element's essence buys its spells (one rule).
// A Generic spell costs its tier's Quartz essence (EEssenceType::Generic) and no Skill essence, the same amount a
// Fire spell of that tier costs in Fire essence.

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

	static int32 TypedOf(const FPurchaseCost &Cost, EEssenceType Type)
	{
		const int32 *Amount = Cost.Typed.Find(Type);
		return Amount ? *Amount : 0;
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

	It("prices a Generic spell in its tier's Quartz essence, like an elemental one (D-105)", [this]()
	{
		if (!TestNotNull(TEXT("EconomyService"), Economy)) return;
		for (const EItemTier Tier : GENERIC_PRICE_TEST_TIERS)
		{
			const FString TierName = UEnum::GetValueAsString(Tier);
			const FPurchaseCost Generic = CostOf(ESpellElement::Generic, Tier);
			const FPurchaseCost Fire = CostOf(ESpellElement::Fire, Tier);
			TestEqual(TierName + TEXT(": Quartz essence"), TypedOf(Generic, EEssenceType::Generic),
					  EconomyYield::GetTypedEssencePurchaseCostForTier(Tier));
			TestEqual(TierName + TEXT(": same amount as Fire's"), TypedOf(Generic, EEssenceType::Generic),
					  TypedOf(Fire, EEssenceType::Fire));
			TestEqual(TierName + TEXT(": no Skill essence"), Generic.SkillEssence, 0);
			TestEqual(TierName + TEXT(": same Prisms as Fire"), Generic.Prisms, Fire.Prisms);
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
