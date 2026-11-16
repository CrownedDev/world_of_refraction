// StartingGearSpec.cpp
// B-522 (D-223, AssetOverhaulPlan §1 and §3 step 1, stage 4): a character's StartingGear is minted into the store once,
// each piece's Crystal in its socket at full durability, its StartingEvolution in the seat, and worn. Not a kit piece.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/ClassDefaultLoadoutData.h"
#include "Pool/PoolSubsystem.h"

BEGIN_DEFINE_SPEC(FStartingGearSpec, "WoR.Loadout.StartingGear",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FStartingGearSpec)

void FStartingGearSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
	});

	AfterEach([this]() { TestWorld.Reset(); });

	It("mints the pieces with their crystals and the evolution, and wears them", [this]()
	{
		UPoolSubsystem *Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		if (!TestNotNull(TEXT("pool"), Pool))
		{
			return;
		}
		const FName Key(TEXT("StartingGearHero"));
		FClassDefaultLoadout Gear;
		FClassDefaultPiece &Sword = Gear.Pieces.AddDefaulted_GetRef();
		Sword.Piece = NewObject<UWeaponData>(GetTransientPackage());
		Gear.Pieces.AddDefaulted(); // the Generic's second weapon place, left empty
		FClassDefaultPiece &Ring = Gear.Pieces.AddDefaulted_GetRef();
		Ring.Piece = NewObject<URingData>(GetTransientPackage());
		Ring.Crystal = FCrystalId(ECrystalType::Citrine, EItemTier::F_Tier);
		UEvolutionItemData *Gale = NewObject<UEvolutionItemData>(GetTransientPackage());
		Gale->Tier = EItemTier::D_Tier;

		FWornGear Innate;
		Innate.InnateSpells.Add(nullptr); // stands in for the authored innate list: kept
		Pool->WearStartingGear(Key, Gear, Gale, Innate);

		const FWornGear *Worn = Pool->GetWornGear(Key);
		if (!TestNotNull(TEXT("a worn record"), Worn))
		{
			return;
		}
		TestEqual(TEXT("the innate list kept"), Worn->InnateSpells.Num(), 1);
		const FLoadoutSlot *SwordSlot = Worn->GetEntry(0);
		const FLoadoutSlot *RingSlot = Worn->GetEntry(2);
		if (!TestTrue(TEXT("both places worn"), SwordSlot && RingSlot && SwordSlot->GearID.IsValid() && RingSlot->GearID.IsValid()))
		{
			return;
		}
		TestEqual(TEXT("the sword in hand"), Worn->ActiveGearID, SwordSlot->GearID);
		TestFalse(TEXT("the empty place stays empty"), Worn->GetEntry(1) && Worn->GetEntry(1)->GearID.IsValid());

		const FWeaponInventoryEntry *OwnedSword = Pool->FindWeaponInPool(SwordSlot->GearID);
		const FRingInventoryEntry *OwnedRing = Pool->FindRingInPool(RingSlot->GearID);
		TestTrue(TEXT("the sword's socket empty"), OwnedSword && OwnedSword->AttachedItem.IsEmpty());
		TestTrue(TEXT("the Citrine in the ring"), OwnedRing && OwnedRing->AttachedItem.IsCrystal() &&
													OwnedRing->AttachedItem.Crystal.Id == Ring.Crystal);
		TestTrue(TEXT("at full durability"), OwnedRing && OwnedRing->AttachedItem.GetCurrentDurability() ==
															  ItemIdentity::GetMaxDurability(Ring.Crystal));
		TestEqual(TEXT("the ring is the hero's"), Pool->GetGearOwner(RingSlot->GearID), Key);
		TestFalse(TEXT("not a kit piece"), Pool->IsClassDefault(RingSlot->GearID));

		const FEvolutionInventoryEntry *Seat = Pool->FindEvolutionInPool(Worn->EvolutionID);
		TestTrue(TEXT("Gale owned and in the seat, at its tier"), Seat && Seat->Item == Gale && Seat->Tier == EItemTier::D_Tier);
		TestEqual(TEXT("Gale is the hero's"), Pool->GetGearOwner(Worn->EvolutionID), Key);
	});

	It("wears a place's off hand, in the starting gear and the trial kit (D-227: the sword with its shield)", [this]()
	{
		UPoolSubsystem *Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		if (!TestNotNull(TEXT("pool"), Pool))
		{
			return;
		}
		FClassDefaultLoadout Gear;
		FClassDefaultPiece &Sword = Gear.Pieces.AddDefaulted_GetRef();
		Sword.Piece = NewObject<UWeaponData>(GetTransientPackage());
		UWeaponData *Shield = NewObject<UWeaponData>(GetTransientPackage());
		Shield->WeaponType = EWeaponType::Shield;
		Sword.OffHand = Shield;

		const FName StartKey(TEXT("StartingGearShieldHero"));
		Pool->WearStartingGear(StartKey, Gear, nullptr, FWornGear());
		const FWornGear *Started = Pool->GetWornGear(StartKey);
		const FLoadoutSlot *StartSlot = Started ? Started->GetEntry(0) : nullptr;
		if (TestTrue(TEXT("starting gear: the shield in the off hand"), StartSlot && StartSlot->OffHandID.IsValid()))
		{
			const FWeaponInventoryEntry *Owned = Pool->FindWeaponInPool(StartSlot->OffHandID);
			TestTrue(TEXT("starting gear: the shield owned"), Owned && Owned->Weapon == Shield);
			TestEqual(TEXT("starting gear: the shield is the hero's"), Pool->GetGearOwner(StartSlot->OffHandID), StartKey);
		}

		const FName KitKey(TEXT("KitShieldHero"));
		Pool->MintClassDefaultKit(KitKey, Gear);
		Pool->WearClassDefaultKit(KitKey, Gear);
		const FWornGear *Kit = Pool->GetWornGear(KitKey);
		const FLoadoutSlot *KitSlot = Kit ? Kit->GetEntry(0) : nullptr;
		if (TestTrue(TEXT("trial kit: the shield in the off hand"), KitSlot && KitSlot->OffHandID.IsValid()))
		{
			TestTrue(TEXT("trial kit: the shield is a kit piece"), Pool->IsClassDefault(KitSlot->OffHandID));
		}
	});
}

#endif
