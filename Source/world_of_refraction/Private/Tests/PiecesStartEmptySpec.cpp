// PiecesStartEmptySpec.cpp
// B-521 (D-224, AssetOverhaulPlan §3 step 2, stage 3): a new piece (bought, found, given, restored) starts with an empty
// socket; the asset's baked crystal is read only for the old authored layout (CreateFromAuthored*) until stage 6. A
// merchant's price no longer adds the baked crystal.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Currency/EconomyService.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Currency/EconomyYield.h"
#include "Loadout/Entries/FRingInventoryEntry.h"
#include "Loadout/Entries/FWeaponInventoryEntry.h"
#include "Merchant/MerchantData.h"

namespace
{
	template <typename TAsset>
	TAsset *WithBakedGarnet(EItemTier Tier)
	{
		TAsset *Asset = NewObject<TAsset>(GetTransientPackage());
		Asset->Tier = Tier;
		Asset->AttachedItem.Kind = EAttachedItemKind::Crystal;
		Asset->AttachedItem.CrystalType = ECrystalType::Garnet;
		Asset->AttachedItem.CrystalTier = EItemTier::S_Tier;
		return Asset;
	}
}

BEGIN_DEFINE_SPEC(FPiecesStartEmptySpec, "WoR.Loadout.PiecesStartEmpty",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FPiecesStartEmptySpec)

void FPiecesStartEmptySpec::Define()
{
	It("mints a new weapon and ring with an empty socket, whatever the asset has baked in", [this]()
	{
		const FWeaponInventoryEntry Weapon = FWeaponInventoryEntry::CreateFromWeapon(WithBakedGarnet<UWeaponData>(EItemTier::F_Tier));
		const FRingInventoryEntry Ring = FRingInventoryEntry::CreateFromRing(WithBakedGarnet<URingData>(EItemTier::F_Tier));
		TestTrue(TEXT("weapon: empty"), Weapon.AttachedItem.IsEmpty());
		TestTrue(TEXT("ring: empty"), Ring.AttachedItem.IsEmpty());
		TestEqual(TEXT("ring: no old spell list"), Ring.AssignedSpells.Num(), 0);
	});

	It("keeps the baked crystal for the old authored layout only", [this]()
	{
		const FWeaponInventoryEntry Weapon =
			FWeaponInventoryEntry::CreateFromAuthoredWeapon(WithBakedGarnet<UWeaponData>(EItemTier::F_Tier));
		const FRingInventoryEntry Ring = FRingInventoryEntry::CreateFromAuthoredRing(WithBakedGarnet<URingData>(EItemTier::F_Tier));
		TestTrue(TEXT("weapon: the Garnet"), Weapon.AttachedItem.IsCrystal() && Weapon.AttachedItem.Crystal.Id.Type == ECrystalType::Garnet);
		TestTrue(TEXT("ring: the Garnet"), Ring.AttachedItem.IsCrystal() && Ring.AttachedItem.Crystal.Id.Type == ECrystalType::Garnet);
	});

	It("prices a merchant's piece by its own tier, without the baked crystal", [this]()
	{
		FWoRTestWorld TestWorld;
		TestTrue(TEXT("test world created"), TestWorld.Create());
		const UEconomyService *Economy = TestWorld.GetSubsystem<UEconomyService>();
		if (!TestNotNull(TEXT("economy"), Economy))
		{
			return;
		}
		FMerchantStockEntry Entry;
		Entry.Asset = WithBakedGarnet<URingData>(EItemTier::C_Tier);
		const FPurchaseCost Cost = Economy->PreviewCartCost({Entry});
		TestEqual(TEXT("the ring's tier base only"), Cost.Prisms, EconomyYield::GetPrismsBaseForTier(EItemTier::C_Tier));
	});
}

#endif
