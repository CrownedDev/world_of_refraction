// GearTierDebugSpec.cpp
// B-609a: wor.Debug.SetGearTier sets the owned weapon's or adapter's tier where the truth lives (the owned entry) and
// every loadout slot holding it follows; an S weapon then takes an S crystal (one S crystal per loadout, D-143).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Currency/CurrencyComponent.h"
#include "Debug/Inventory/GearTierDebug.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/LoadoutConstants.h"
#include "Loadout/LoadoutRules.h"

namespace
{
	const TCHAR *GEAR_TIER_SWORD = TEXT("Training Sword");
	const TCHAR *GEAR_TIER_AXE = TEXT("Training Axe");
	const TCHAR *GEAR_TIER_HAND = TEXT("Open Hand");
	const FCrystalId GEAR_TIER_S_EMERALD(ECrystalType::Emerald, EItemTier::S_Tier);
	constexpr int32 GEAR_TIER_LOADOUTS = 2;
}

BEGIN_DEFINE_SPEC(FGearTierDebugSpec, "WoR.Debug.SetGearTier",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UInventoryComponent *Inventory = nullptr;
	FGuid SwordId;

	UWeaponData *MakeWeapon(const TCHAR *Name)
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->Name = Name;
		Weapon->Tier = EItemTier::F_Tier;
		return Weapon;
	}

	const FWeaponInventoryEntry &SlotIn(int32 LoadoutIndex) const
	{
		return Inventory->SavedLoadouts[LoadoutIndex].PrimaryWeapon.WeaponEntry;
	}
END_DEFINE_SPEC(FGearTierDebugSpec)

void FGearTierDebugSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		AActor *Owner = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!Owner)
		{
			return;
		}
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Owner);
		Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Owner);

		UWeaponData *Sword = MakeWeapon(GEAR_TIER_SWORD);
		URingData *Hand = NewObject<URingData>(GetTransientPackage());
		Hand->Name = GEAR_TIER_HAND;
		Hand->Tier = EItemTier::F_Tier;
		TestTrue(TEXT("Sword owned"), Inventory->AddWeapon(Sword));
		TestTrue(TEXT("Axe owned"), Inventory->AddWeapon(MakeWeapon(GEAR_TIER_AXE)));
		TestTrue(TEXT("Hand owned"), Inventory->AddRing(Hand));
		SwordId = Inventory->Weapons.Num() > 0 ? Inventory->Weapons[0].PersistentID : FGuid();

		Inventory->SavedLoadouts.Empty();
		for (int32 Index = 0; Index < GEAR_TIER_LOADOUTS; ++Index)
		{
			FCombatLoadout &Preset = Inventory->SavedLoadouts.AddDefaulted_GetRef();
			Preset.PrimarySlotType = EPrimarySlotType::Weapon;
			Preset.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		}
		Inventory->ActiveLoadoutIndex = 0;
		Inventory->LinkLoadoutsToOwned();
	});

	AfterEach([this]()
	{
		Inventory = nullptr;
		TestWorld.Reset();
	});

	It("sets the named weapon to S, owned and in every loadout, and leaves the rest alone", [this]()
	{
		if (!Inventory || !SwordId.IsValid()) return;
		const TArray<FString> Lines = WoRGearTier::SetOwnedGearTier(*Inventory, TEXT("sword"), EItemTier::S_Tier);
		TestEqual(TEXT("one piece changed"), Lines.Num(), 1);
		TestTrue(TEXT("the line names it and both tiers"), Lines.Num() == 1 && Lines[0] == FString::Printf(TEXT("%s: F -> S"), GEAR_TIER_SWORD));
		TestEqual(TEXT("the owned sword is S"), Inventory->FindOwnedWeapon(SwordId)->Tier, EItemTier::S_Tier);
		TestEqual(TEXT("loadout 0 shows S"), SlotIn(0).Tier, EItemTier::S_Tier);
		TestEqual(TEXT("loadout 1 shows S"), SlotIn(1).Tier, EItemTier::S_Tier);
		TestEqual(TEXT("the axe is untouched"), Inventory->Weapons[1].Tier, EItemTier::F_Tier);
		TestEqual(TEXT("the hand is untouched"), Inventory->Rings[0].Tier, EItemTier::F_Tier);
	});

	It("sets an adapter by its name too", [this]()
	{
		if (!Inventory || Inventory->Rings.Num() == 0) return;
		TestEqual(TEXT("one piece changed"), WoRGearTier::SetOwnedGearTier(*Inventory, TEXT("Hand"), EItemTier::S_Tier).Num(), 1);
		TestEqual(TEXT("the owned hand is S"), Inventory->Rings[0].Tier, EItemTier::S_Tier);
	});

	It("changes nothing for a piece already at the tier, an unknown name or an empty one", [this]()
	{
		if (!Inventory) return;
		WoRGearTier::SetOwnedGearTier(*Inventory, TEXT("Sword"), EItemTier::A_Tier);
		TestEqual(TEXT("already A"), WoRGearTier::SetOwnedGearTier(*Inventory, TEXT("Sword"), EItemTier::A_Tier).Num(), 0);
		TestEqual(TEXT("no such piece"), WoRGearTier::SetOwnedGearTier(*Inventory, TEXT("Halberd"), EItemTier::S_Tier).Num(), 0);
		TestEqual(TEXT("an empty name matches nothing"), WoRGearTier::SetOwnedGearTier(*Inventory, FString(), EItemTier::S_Tier).Num(), 0);
		TestEqual(TEXT("the axe never moved"), Inventory->Weapons[1].Tier, EItemTier::F_Tier);
	});

	It("lets the S sword take an S Emerald, the one S crystal a loadout may hold", [this]()
	{
		if (!Inventory || !SwordId.IsValid()) return;
		WoRGearTier::SetOwnedGearTier(*Inventory, TEXT("Sword"), EItemTier::S_Tier);
		TestTrue(TEXT("an S Emerald owned"), Inventory->AddCrystal(GEAR_TIER_S_EMERALD));
		TestTrue(TEXT("socketed in the S sword"), Inventory->AttachCrystalToWeapon(SwordId, GEAR_TIER_S_EMERALD));
		TestTrue(TEXT("the loadout shows the S Emerald"), SlotIn(0).AttachedItem.IsCrystal() && SlotIn(0).AttachedItem.Crystal.Id == GEAR_TIER_S_EMERALD);
		TestEqual(TEXT("one S crystal equipped, the cap"), FLoadoutRules::CountEquippedSTierCrystals(Inventory->SavedLoadouts[0], ECharacterClass::Generic),
				  LoadoutConstants::MAX_EQUIPPED_S_TIER_CRYSTALS);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
