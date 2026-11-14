// LoadoutEntriesSpec.cpp
// B-480 (D-210): loadout entries. Each class's entry limits: a Generic's two weapon entries and one adapter entry (the
// adapter beside two weapons is its adapter in use), a Resonator's one weapon and up to five adapters, a Refractor's one
// entry. An entry's two augment sockets take two stones, which stack (the same stat twice) and cost one each. Where an
// evolution may go: never on a Resonator's weapon or a Generic's adapter, cleared by taking it off.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/StatStone.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/LoadoutConstants.h"
#include "Loadout/LoadoutRules.h"

namespace
{
	const FCrystalId ENTRIES_TEST_STONE = StatStone::Make(ESubStat::Resistance, EItemTier::C_Tier);
	constexpr int32 ENTRIES_TEST_STONES_ON_ONE_ENTRY = 2;
	/** EntriesHas: any SlotIndex (LoadoutGearEvolutionIndex uses -1 and -2). */
	constexpr int32 ENTRIES_ANY_SLOT = TNumericLimits<int32>::Min();

	FRuntimeAttachedItem EntriesTestStone()
	{
		FRuntimeAttachedItem Stone;
		Stone.Kind = EAttachedItemKind::AugmentStone;
		Stone.Crystal.Id = ENTRIES_TEST_STONE;
		return Stone;
	}

	FRuntimeAttachedItem EntriesTestEvolution()
	{
		FRuntimeAttachedItem Evolution;
		Evolution.Kind = EAttachedItemKind::Evolution;
		Evolution.Evolution.Item = NewObject<UEvolutionItemData>(GetTransientPackage());
		return Evolution;
	}

	FWeaponLoadoutEntry EntriesTestWeapon(const FRuntimeAttachedItem &Attached = FRuntimeAttachedItem())
	{
		FWeaponLoadoutEntry Entry;
		Entry.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Entry.WeaponEntry.PersistentID = FGuid::NewGuid();
		Entry.WeaponEntry.AttachedItem = Attached;
		return Entry;
	}

	FRingLoadoutEntry EntriesTestAdapter(const FRuntimeAttachedItem &Attached = FRuntimeAttachedItem())
	{
		FRingLoadoutEntry Entry;
		Entry.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
		Entry.RingEntry.PersistentID = FGuid::NewGuid();
		Entry.RingEntry.AttachedItem = Attached;
		return Entry;
	}

	/** A Generic with two weapon entries and an adapter entry beside them. */
	FCombatLoadout EntriesTestGeneric()
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon = EntriesTestWeapon();
		Loadout.SecondarySlotType = ESecondarySlotType::Weapon;
		Loadout.SecondaryWeapon = EntriesTestWeapon();
		Loadout.RingLoadout.Add(EntriesTestAdapter());
		return Loadout;
	}

	TArray<FInvalidSlotFinding> EntriesFindings(const FCombatLoadout &Loadout, ECharacterClass Class)
	{
		FLoadoutRuleContext Context;
		Context.CharacterClass = Class;
		return FLoadoutRules::CollectFindings(Loadout, Context);
	}

	bool EntriesHas(const TArray<FInvalidSlotFinding> &Findings, ELoadoutSlotType Type, int32 SlotIndex = ENTRIES_ANY_SLOT)
	{
		return Findings.ContainsByPredicate([Type, SlotIndex](const FInvalidSlotFinding &F)
											{ return F.SlotType == Type && F.bClearable && (SlotIndex == ENTRIES_ANY_SLOT || F.SlotIndex == SlotIndex); });
	}
}

BEGIN_DEFINE_SPEC(FLoadoutEntriesSpec, "WoR.Loadout.Entries", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FLoadoutEntriesSpec)

void FLoadoutEntriesSpec::Define()
{
	It("lets a Generic hold two weapon entries and an adapter entry, the adapter in use (D-210)", [this]()
	{
		FCombatLoadout Loadout = EntriesTestGeneric();
		TestEqual(TEXT("no finding"), EntriesFindings(Loadout, ECharacterClass::Generic).Num(), 0);
		Loadout.RebuildSlotsFromLegacy(ECharacterClass::Generic);
		TestEqual(TEXT("three slots: two weapons and the adapter"), Loadout.Slots.Num(), 3);
		const FRingLoadoutEntry *InUse = Loadout.GetLegacyRingEntry(Loadout.ActiveAdapterSlot);
		TestTrue(TEXT("the adapter entry is the adapter in use"), InUse == &Loadout.RingLoadout[0]);
	});

	It("refuses a Generic's second adapter, dropping the adapter entry (D-210)", [this]()
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Ring;
		Loadout.PrimaryRing = EntriesTestAdapter();
		Loadout.SecondarySlotType = ESecondarySlotType::Weapon;
		Loadout.SecondaryWeapon = EntriesTestWeapon();
		Loadout.RingLoadout.Add(EntriesTestAdapter());
		TestTrue(TEXT("two adapters: the adapter entry goes"), EntriesHas(EntriesFindings(Loadout, ECharacterClass::Generic), ELoadoutSlotType::RingOverflow));
	});

	It("holds a Resonator to one weapon and five adapters, a Refractor to one entry (D-210)", [this]()
	{
		FCombatLoadout Resonator;
		Resonator.PrimarySlotType = EPrimarySlotType::Weapon;
		Resonator.PrimaryWeapon = EntriesTestWeapon();
		for (int32 Index = 0; Index < LoadoutConstants::MAX_RESONATOR_ADAPTERS; ++Index)
		{
			Resonator.RingLoadout.Add(EntriesTestAdapter());
		}
		TestEqual(TEXT("Resonator: a weapon and five adapters fit"), FLoadoutRules::CollectClassLimitFindings(Resonator, ECharacterClass::Resonator).Num(), 0);
		Resonator.RingLoadout.Add(EntriesTestAdapter());
		TestTrue(TEXT("Resonator: a sixth adapter goes"), EntriesHas(FLoadoutRules::CollectClassLimitFindings(Resonator, ECharacterClass::Resonator), ELoadoutSlotType::RingOverflow));

		FCombatLoadout Refractor = Resonator;
		TestEqual(TEXT("Refractor: only its one entry is read"), FLoadoutRules::CollectClassLimitFindings(Refractor, ECharacterClass::Caster).Num(), 0);
		Refractor.RebuildSlotsFromLegacy(ECharacterClass::Caster);
		TestEqual(TEXT("Refractor: one slot"), Refractor.Slots.Num(), 1);
	});

	It("costs two stones on one entry one each (D-206, D-210)", [this]()
	{
		FWeaponLoadoutEntry Weapon = EntriesTestWeapon(EntriesTestStone());
		Weapon.WeaponEntry.StoneSocket = EntriesTestStone();
		TestEqual(TEXT("two stones: 2"), FLoadoutRules::SlotCost(FLoadoutSlot::FromWeaponLoadoutEntry(Weapon)),
				  ENTRIES_TEST_STONES_ON_ONE_ENTRY * LoadoutConstants::LOADOUT_STONE_COST);
	});

	It("refuses an evolution on a Resonator's weapon and on a Generic's adapter; a Refractor may do either (D-210)", [this]()
	{
		FCombatLoadout ResonatorWeapon;
		ResonatorWeapon.PrimarySlotType = EPrimarySlotType::Weapon;
		ResonatorWeapon.PrimaryWeapon = EntriesTestWeapon(EntriesTestEvolution());
		ResonatorWeapon.RingLoadout.Add(EntriesTestAdapter(EntriesTestEvolution()));
		const TArray<FInvalidSlotFinding> Resonator = EntriesFindings(ResonatorWeapon, ECharacterClass::Resonator);
		TestTrue(TEXT("Resonator: not on its weapon"), EntriesHas(Resonator, ELoadoutSlotType::GearEvolution, LoadoutGearEvolutionIndex::PRIMARY_WEAPON));
		TestFalse(TEXT("Resonator: on an adapter is fine"), EntriesHas(Resonator, ELoadoutSlotType::GearEvolution, 0));

		FCombatLoadout Generic = EntriesTestGeneric();
		Generic.PrimaryWeapon.WeaponEntry.AttachedItem = EntriesTestEvolution();
		Generic.RingLoadout[0].RingEntry.AttachedItem = EntriesTestEvolution();
		const TArray<FInvalidSlotFinding> GenericFindings = EntriesFindings(Generic, ECharacterClass::Generic);
		TestTrue(TEXT("Generic: not on its adapter entry"), EntriesHas(GenericFindings, ELoadoutSlotType::GearEvolution, 0));
		TestFalse(TEXT("Generic: on a weapon is fine"), EntriesHas(GenericFindings, ELoadoutSlotType::GearEvolution, LoadoutGearEvolutionIndex::PRIMARY_WEAPON));

		FCombatLoadout GenericRingPrimary;
		GenericRingPrimary.PrimarySlotType = EPrimarySlotType::Ring;
		GenericRingPrimary.PrimaryRing = EntriesTestAdapter(EntriesTestEvolution());
		TestTrue(TEXT("Generic: not on an adapter primary either"),
				 EntriesHas(EntriesFindings(GenericRingPrimary, ECharacterClass::Generic), ELoadoutSlotType::GearEvolution, LoadoutGearEvolutionIndex::PRIMARY_RING));

		TestFalse(TEXT("Refractor: a weapon may be evolved"), EntriesHas(EntriesFindings(ResonatorWeapon, ECharacterClass::Caster), ELoadoutSlotType::GearEvolution));
		TestFalse(TEXT("Refractor: an adapter may be evolved"), EntriesHas(EntriesFindings(GenericRingPrimary, ECharacterClass::Caster), ELoadoutSlotType::GearEvolution));
	});

	Describe("in a fight", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("Test world created"), TestWorld->Create());
		});

		AfterEach([this]() { TestWorld.Reset(); });

		It("stacks two stones of the same stat on one entry (D-210)", [this]()
		{
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->CharacterClass = ECharacterClass::Generic;
			ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, NewObject<UWeaponData>(GetTransientPackage()));
			if (!TestNotNull(TEXT("Loadout"), Loadout))
			{
				return;
			}
			UInventoryComponent *Inventory = Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>();
			FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
			const float OneStone = CrystalEffectTable::GetAttachedStonePercent(EntriesTestStone(), ESubStat::Resistance);
			TestTrue(TEXT("the stone gives something"), OneStone > 0.0f);
			Active.PrimaryWeapon.WeaponEntry.StoneSocket = EntriesTestStone();
			TestEqual(TEXT("one stone"), Loadout->GetAttachedStonePercent(ESubStat::Resistance), OneStone);
			Active.PrimaryWeapon.WeaponEntry.AttachedItem = EntriesTestStone();
			TestEqual(TEXT("two stones of one stat stack"), Loadout->GetAttachedStonePercent(ESubStat::Resistance), ENTRIES_TEST_STONES_ON_ONE_ENTRY * OneStone);
		});

		It("takes an evolution off a Resonator's weapon before the fight, leaving the weapon (D-210)", [this]()
		{
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->CharacterClass = ECharacterClass::Resonator;
			ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, NewObject<UWeaponData>(GetTransientPackage()));
			if (!TestNotNull(TEXT("Loadout"), Loadout))
			{
				return;
			}
			UInventoryComponent *Inventory = Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>();
			FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
			Active.PrimaryWeapon.WeaponEntry.AttachedItem = EntriesTestEvolution();
			const TArray<FInvalidSlotFinding> Dropped = Loadout->DropInvalidParts(Inventory->ActiveLoadoutIndex, {ELoadoutSlotType::GearEvolution});
			TestTrue(TEXT("the evolution finding was cleared"), EntriesHas(Dropped, ELoadoutSlotType::GearEvolution));
			TestFalse(TEXT("the evolution is off"), Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].PrimaryWeapon.WeaponEntry.AttachedItem.IsEvolution());
			TestTrue(TEXT("the weapon stays"), Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].PrimaryWeapon.IsValid());
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
