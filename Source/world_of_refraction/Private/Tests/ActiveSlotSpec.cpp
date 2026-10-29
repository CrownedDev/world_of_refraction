// ActiveSlotSpec.cpp
// B-304 (D-110 unified loadout, step 4): ULoadoutComponent::GetActiveSlot(Kind) is the one active-gear rule and the old
// getters forward to it. The weapon in hand follows the shown weapon; an empty or stale active slot falls back to the
// first usable one of its kind (no primary rule); a piece another member wears is skipped (B-221b); a loadout edited
// without an announcement is still read right (the mirror heals). A Generic's secondary weapon gets the primary's checks.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/LoadoutRules.h"
#include "Pool/PoolSubsystem.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	const FName ACTIVE_SLOT_TEST_OTHER_WEARER(TEXT("ActiveSlotTest_OtherMember"));
	const FCrystalId ACTIVE_SLOT_TEST_FIRE_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	constexpr int32 ACTIVE_SLOT_TEST_DURABILITY = 5;
	constexpr int32 ACTIVE_SLOT_TEST_STALE_RING = 9;
}

BEGIN_DEFINE_SPEC(FActiveSlotSpec, "WoR.Loadout.ActiveSlot", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ULoadoutComponent *Loadout = nullptr;
	UWeaponData *Primary = nullptr;
	UWeaponData *Secondary = nullptr;

	FCombatLoadout &Active() const
	{
		UInventoryComponent *Inventory = Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>();
		return Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	}

	/** A Generic with two weapons, each with its own owned GUID; the primary shown. */
	bool SpawnGeneric()
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Generic;
		Primary = NewObject<UWeaponData>(GetTransientPackage());
		Secondary = NewObject<UWeaponData>(GetTransientPackage());
		Loadout = TestWorld->SpawnCombatantWithLoadout(Data, Primary);
		if (!Loadout)
		{
			return false;
		}
		FCombatLoadout &L = Active();
		L.PrimaryWeapon.WeaponEntry.PersistentID = FGuid::NewGuid();
		L.SecondarySlotType = ESecondarySlotType::Weapon;
		L.SecondaryWeapon.WeaponEntry.Weapon = Secondary;
		L.SecondaryWeapon.WeaponEntry.PersistentID = FGuid::NewGuid();
		L.bShowPrimary = true;
		return true;
	}
END_DEFINE_SPEC(FActiveSlotSpec)

void FActiveSlotSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		Loadout = nullptr;
		TestWorld.Reset();
	});

	It("holds the shown weapon, and reads a field edit without an announcement", [this]()
	{
		if (!TestTrue(TEXT("Generic"), SpawnGeneric())) return;
		TestTrue(TEXT("primary in hand"), Loadout->GetActiveWeapon() == Primary);
		Active().bShowPrimary = false; // no announcement: the mirror must still follow
		TestTrue(TEXT("secondary in hand"), Loadout->GetActiveWeapon() == Secondary);
		TestTrue(TEXT("its old entry"), Loadout->GetActiveWeaponLoadout() == &Active().SecondaryWeapon);
		UWeaponData *Swapped = NewObject<UWeaponData>(GetTransientPackage());
		Active().SecondaryWeapon.WeaponEntry.Weapon = Swapped;
		TestTrue(TEXT("a swapped weapon is read at once"), Loadout->GetActiveWeapon() == Swapped);
	});

	It("falls back to the first usable weapon when the shown slot is empty (D-110)", [this]()
	{
		if (!TestTrue(TEXT("Generic"), SpawnGeneric())) return;
		Active().SecondarySlotType = ESecondarySlotType::None;
		Active().bShowPrimary = false; // showing an empty secondary
		TestTrue(TEXT("the primary is in hand"), Loadout->GetActiveWeapon() == Primary);
	});

	It("skips a weapon another member wears (B-221b)", [this]()
	{
		if (!TestTrue(TEXT("Generic"), SpawnGeneric())) return;
		UPoolSubsystem *Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		Pool->SetWearer(Active().PrimaryWeapon.WeaponEntry.PersistentID, ACTIVE_SLOT_TEST_OTHER_WEARER);
		TestTrue(TEXT("the free weapon is in hand"), Loadout->GetActiveWeapon() == Secondary);
		Pool->ReleaseWearer(Active().PrimaryWeapon.WeaponEntry.PersistentID);
		TestTrue(TEXT("back once it's free"), Loadout->GetActiveWeapon() == Primary);
	});

	It("uses a Resonator's first adapter when its active index is stale (D-110)", [this]()
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Resonator;
		Loadout = TestWorld->SpawnCombatantWithLoadout(Data);
		if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
		URingData *First = NewObject<URingData>(GetTransientPackage());
		FRingLoadoutEntry Entry;
		Entry.RingEntry.Ring = First;
		Active().RingLoadout = {Entry};
		Active().ActiveRingIndex = ACTIVE_SLOT_TEST_STALE_RING;
		TestTrue(TEXT("first adapter"), Loadout->GetActiveRing() == First);
		TestTrue(TEXT("its old entry"), Loadout->GetActiveRingLoadout() == &Active().RingLoadout[0]);
		TestTrue(TEXT("not a primary ring"), Loadout->GetPrimaryRing() == nullptr);
	});

	It("checks a Generic's secondary weapon's spells like the primary's", [this]()
	{
		FCombatLoadout L;
		L.PrimarySlotType = EPrimarySlotType::Weapon;
		L.PrimaryWeapon.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
		L.SecondarySlotType = ESecondarySlotType::Weapon;
		L.SecondaryWeapon.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
		L.SecondaryWeapon.WeaponEntry.AttachedItem.Kind = EAttachedItemKind::Crystal;
		L.SecondaryWeapon.WeaponEntry.AttachedItem.Crystal = FCrystalAttachment(ACTIVE_SLOT_TEST_FIRE_GEM, ACTIVE_SLOT_TEST_DURABILITY);
		USpellData *WaterSpell = NewObject<USpellData>(GetTransientPackage());
		// Any element but the gem's (and not Generic, which fits every host).
		WaterSpell->Element = ItemIdentity::GetElement(ACTIVE_SLOT_TEST_FIRE_GEM) == ESpellElement::Water ? ESpellElement::Fire : ESpellElement::Water;
		L.SecondaryWeapon.AssignedSpells.Add(WaterSpell);
		FLoadoutRuleContext Context;
		Context.CharacterClass = ECharacterClass::Generic;
		const TArray<FInvalidSlotFinding> Findings = FLoadoutRules::CollectFindings(L, Context);
		TestTrue(TEXT("the secondary's mismatched spell is found, aimed at the secondary"),
				 Findings.ContainsByPredicate([](const FInvalidSlotFinding &F)
											  { return F.SlotType == ELoadoutSlotType::WeaponSpell && F.SlotIndex == 0 &&
													   F.SubIndex == LoadoutAbilityOverflowIndex::SECONDARY_WEAPON; }));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
