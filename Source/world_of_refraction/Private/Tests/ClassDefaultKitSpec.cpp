// ClassDefaultKitSpec.cpp
// B-511 (D-221 stage 1c): a character's class default kit is minted into the store once, as its own: a second mint
// reuses the pieces, a changed place replaces its piece, a removed place takes it out; kit pieces can't be given or put in
// the chest, and the kit is saved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/GearGive.h"
#include "Loadout/ClassDefaultLoadoutData.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"

namespace
{
	const FName CLASS_KIT_KEY(TEXT("ClassKitTester"));
	const FName CLASS_KIT_OTHER_KEY(TEXT("ClassKitOther"));
}

BEGIN_DEFINE_SPEC(FClassDefaultKitSpec, "WoR.Pool.ClassDefaultKit", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UWeaponData *Sword = nullptr;
	UWeaponData *Axe = nullptr;
	URingData *Ring = nullptr;
	UApparelData *Coat = nullptr;
	FClassDefaultLoadout Kit;
END_DEFINE_SPEC(FClassDefaultKitSpec)

void FClassDefaultKitSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Axe = NewObject<UWeaponData>(GetTransientPackage());
		Ring = NewObject<URingData>(GetTransientPackage());
		Coat = NewObject<UApparelData>(GetTransientPackage());
		Kit = FClassDefaultLoadout();
		Kit.Pieces.AddDefaulted_GetRef().Piece = Sword;
		Kit.Pieces.AddDefaulted(); // the Generic's second weapon place, empty
		Kit.Pieces.AddDefaulted_GetRef().Piece = Ring;
		Kit.Apparel.Add(Coat);
	});

	AfterEach([this]()
	{
		Pool = nullptr;
		TestWorld.Reset();
	});

	It("mints each authored piece once, owned by the character", [this]()
	{
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		const FClassDefaultKitIDs First = Pool->MintClassDefaultKit(CLASS_KIT_KEY, Kit);
		TestEqual(TEXT("one ID per place"), First.Pieces.Num(), 3);
		TestNotNull(TEXT("the sword is owned"), Pool->FindWeaponInPool(First.Pieces[0]));
		TestFalse(TEXT("the empty place mints nothing"), First.Pieces[1].IsValid());
		TestNotNull(TEXT("the adapter is owned"), Pool->FindRingInPool(First.Pieces[2]));
		TestTrue(TEXT("the apparel is owned"), First.Apparel.Num() == 1 && Pool->FindApparelInPool(First.Apparel[0]) != nullptr);
		TestEqual(TEXT("the character owns it"), Pool->GetGearOwner(First.Pieces[0]), CLASS_KIT_KEY);
		TestTrue(TEXT("a kit piece"), Pool->IsClassDefault(First.Pieces[0]) && Pool->IsClassDefault(First.Apparel[0]));

		const int32 WeaponsBefore = Pool->GetOwnedWeapons().Num();
		const FClassDefaultKitIDs Second = Pool->MintClassDefaultKit(CLASS_KIT_KEY, Kit);
		TestTrue(TEXT("the same pieces again"), Second.Pieces == First.Pieces && Second.Apparel == First.Apparel);
		TestEqual(TEXT("nothing piles up"), Pool->GetOwnedWeapons().Num(), WeaponsBefore);
	});

	It("replaces a changed place's piece and takes out a removed place's", [this]()
	{
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		const FClassDefaultKitIDs First = Pool->MintClassDefaultKit(CLASS_KIT_KEY, Kit);
		Kit.Pieces[0].Piece = Axe;
		Kit.Pieces.SetNum(1);
		const FClassDefaultKitIDs Second = Pool->MintClassDefaultKit(CLASS_KIT_KEY, Kit);
		TestNull(TEXT("the old sword left the store"), Pool->FindWeaponInPool(First.Pieces[0]));
		const FWeaponInventoryEntry *New = Pool->FindWeaponInPool(Second.Pieces[0]);
		TestTrue(TEXT("the axe is minted"), New && New->Weapon == Axe);
		TestNull(TEXT("the removed adapter left the store"), Pool->FindRingInPool(First.Pieces[2]));
		TestFalse(TEXT("and is no kit piece"), Pool->IsClassDefault(First.Pieces[2]));
	});

	It("refuses to give a kit piece or put it in the chest", [this]()
	{
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		const FGuid SwordID = Pool->MintClassDefaultKit(CLASS_KIT_KEY, Kit).Pieces[0];
		TestFalse(TEXT("not given"), WoRGive::TryGiveGear(TestWorld->GetWorld(), CLASS_KIT_KEY, CLASS_KIT_OTHER_KEY, SwordID).IsEmpty());
		TestFalse(TEXT("not put in the chest"), WoRGive::TryPutInChest(TestWorld->GetWorld(), CLASS_KIT_KEY, SwordID).IsEmpty());
		TestEqual(TEXT("still the character's"), Pool->GetGearOwner(SwordID), CLASS_KIT_KEY);
	});

	It("saves each character's kit", [this]()
	{
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		const FClassDefaultKitIDs Minted = Pool->MintClassDefaultKit(CLASS_KIT_KEY, Kit);
		const FSavedPool Saved = USaveSubsystem::CapturePool(*Pool);
		Pool->RestoreClassDefaultKits({});
		TestFalse(TEXT("cleared"), Pool->IsClassDefault(Minted.Pieces[0]));
		USaveSubsystem::RestorePool(Saved, *Pool);
		const FClassDefaultKitIDs *Loaded = Pool->GetClassDefaultKit(CLASS_KIT_KEY);
		TestTrue(TEXT("back after a load"), Loaded && Loaded->Pieces == Minted.Pieces && Loaded->Apparel == Minted.Apparel);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
