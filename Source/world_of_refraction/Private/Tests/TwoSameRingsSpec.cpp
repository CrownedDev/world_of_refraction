// TwoSameRingsSpec.cpp
// B-516 (D-224, asset overhaul stage 2): every adapter can be one asset (DA_Ring_Adapter), so two Lightning rings are
// two pieces of one asset. They read apart ("Lightning Ring", "Lightning Ring 2"), a cast reads the ring whose list
// holds its spell, and a preset keeps both by their piece ids.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Skills/Definitions/SpellData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/FSavedLoadout.h"
#include "Loadout/LoadoutComponent.h"
#include "UI/Menus/LoadoutV2Model.h"
#include "WoRGameNames.h"

namespace
{
	constexpr int32 TWO_RINGS_TEST_DURABILITY = 10;
	constexpr int32 TWO_RINGS_TEST_ACTIVE = 1;
	const FCrystalId TWO_RINGS_TEST_LIGHTNING(ECrystalType::Citrine, EItemTier::F_Tier);
	const FCrystalId TWO_RINGS_TEST_FIRE(ECrystalType::Garnet, EItemTier::F_Tier);

	FRingLoadoutEntry TwoRingsTestRing(URingData *Ring, const FGuid &GearID, const FCrystalId &Gem, USpellData *Pick)
	{
		FRingLoadoutEntry Entry;
		Entry.RingEntry.Ring = Ring;
		Entry.RingEntry.PersistentID = GearID;
		Entry.RingEntry.AttachedItem.Kind = EAttachedItemKind::Crystal;
		Entry.RingEntry.AttachedItem.Crystal = FCrystalAttachment(Gem, TWO_RINGS_TEST_DURABILITY);
		Entry.RingEntry.AssignedSpells = {Pick};
		return Entry;
	}

	TArray<FText> TwoRingsTestNames(std::initializer_list<const TCHAR *> Names)
	{
		TArray<FText> Out;
		for (const TCHAR *Name : Names)
		{
			Out.Add(FText::FromString(Name));
		}
		return Out;
	}
}

BEGIN_DEFINE_SPEC(FTwoSameRingsSpec, "WoR.Loadout.TwoSameRings", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	URingData *Adapter = nullptr;
	USpellData *Bolt = nullptr;
	USpellData *Arc = nullptr;
	FGuid FirstID;
	FGuid SecondID;

	/** Two Lightning rings of the one adapter asset: the first picks Bolt, the second Arc. */
	FCombatLoadout TwoLightningRings() const
	{
		FCombatLoadout Loadout;
		Loadout.RingLoadout = {TwoRingsTestRing(Adapter, FirstID, TWO_RINGS_TEST_LIGHTNING, Bolt), TwoRingsTestRing(Adapter, SecondID, TWO_RINGS_TEST_LIGHTNING, Arc)};
		return Loadout;
	}

	/** A Resonator wearing Loadout, the second ring in use. */
	ULoadoutComponent *SpawnWearing(const FCombatLoadout &Loadout)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Resonator;
		ULoadoutComponent *Component = TestWorld->SpawnCombatantWithLoadout(Data);
		UInventoryComponent *Inventory = Component ? Component->GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!Inventory || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
		{
			return nullptr;
		}
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.RingLoadout = Loadout.RingLoadout;
		Active.ActiveRingIndex = TWO_RINGS_TEST_ACTIVE;
		return Component;
	}

	static FCombatLoadout &ActiveOf(ULoadoutComponent *Component)
	{
		UInventoryComponent *Inventory = Component->GetOwner()->FindComponentByClass<UInventoryComponent>();
		return Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	}
END_DEFINE_SPEC(FTwoSameRingsSpec)

void FTwoSameRingsSpec::Define()
{
	BeforeEach([this]()
	{
		Adapter = NewObject<URingData>(GetTransientPackage());
		Adapter->Form = EAdapterForm::Ring;
		Bolt = NewObject<USpellData>(GetTransientPackage());
		Arc = NewObject<USpellData>(GetTransientPackage());
		FirstID = FGuid::NewGuid();
		SecondID = FGuid::NewGuid();
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
		Adapter = nullptr;
		Bolt = nullptr;
		Arc = nullptr;
	});

	Describe("the repeat number", [this]()
	{
		It("counts a repeated name from the second on, in list order, and leaves others and empties alone", [this]()
		{
			TArray<FText> Names = TwoRingsTestNames({TEXT("Lightning Ring"), TEXT("Fire Ring"), TEXT(""), TEXT("Lightning Ring"), TEXT(""), TEXT("Lightning Ring")});
			WoRGameNames::NumberRepeatedNames(Names);
			TestEqual(TEXT("first keeps its name"), Names[0].ToString(), FString(TEXT("Lightning Ring")));
			TestEqual(TEXT("a lone name stays"), Names[1].ToString(), FString(TEXT("Fire Ring")));
			TestTrue(TEXT("an empty name stays empty"), Names[2].IsEmpty() && Names[4].IsEmpty());
			TestEqual(TEXT("second gets 2"), Names[3].ToString(), FString(TEXT("Lightning Ring 2")));
			TestEqual(TEXT("third gets 3"), Names[5].ToString(), FString(TEXT("Lightning Ring 3")));
		});

		// B-516b (D-223): the tier tells them apart first; a number only when the name and the tier both repeat.
		It("numbers only a name and tier that both repeat", [this]()
		{
			TArray<FText> Names = TwoRingsTestNames({TEXT("Lightning Ring"), TEXT("Lightning Ring"), TEXT("Lightning Ring")});
			const TArray<EItemTier> Tiers = {EItemTier::C_Tier, EItemTier::F_Tier, EItemTier::C_Tier};
			WoRGameNames::NumberRepeatedNames(Names, Tiers);
			TestEqual(TEXT("a C keeps its name"), Names[0].ToString(), FString(TEXT("Lightning Ring")));
			TestEqual(TEXT("an F of the same name: no number"), Names[1].ToString(), FString(TEXT("Lightning Ring")));
			TestEqual(TEXT("a second C gets 2"), Names[2].ToString(), FString(TEXT("Lightning Ring 2")));
		});

		It("puts the tier letter on the strip's names only where one name has two tiers", [this]()
		{
			TArray<FText> Names = TwoRingsTestNames({TEXT("Lightning Ring"), TEXT("Lightning Ring"), TEXT("Fire Ring"), TEXT("Lightning Ring"), TEXT("Fire Ring")});
			const TArray<EItemTier> Tiers = {EItemTier::C_Tier, EItemTier::F_Tier, EItemTier::C_Tier, EItemTier::C_Tier, EItemTier::C_Tier};
			WoRGameNames::NumberRepeatedNames(Names, Tiers, true);
			const FString C = WoRGameNames::GetTierLetter(EItemTier::C_Tier).ToString();
			const FString F = WoRGameNames::GetTierLetter(EItemTier::F_Tier).ToString();
			TestEqual(TEXT("the C one"), Names[0].ToString(), FString::Printf(TEXT("Lightning Ring  %s"), *C));
			TestEqual(TEXT("the F one"), Names[1].ToString(), FString::Printf(TEXT("Lightning Ring  %s"), *F));
			TestEqual(TEXT("one tier only: no letter"), Names[2].ToString(), FString(TEXT("Fire Ring")));
			TestEqual(TEXT("a second C: letter and number"), Names[3].ToString(), FString::Printf(TEXT("Lightning Ring  %s 2"), *C));
			TestEqual(TEXT("same name and tier: number only"), Names[4].ToString(), FString(TEXT("Fire Ring 2")));
		});
	});

	It("lists two Lightning rings as separate Equipment entries, each its own place", [this]()
	{
		const FLoadoutV2WeaponsPage Page = WoRLoadoutV2::BuildWeaponsPage(TwoLightningRings(), ECharacterClass::Resonator);
		const TArray<const FLoadoutV2Entry *> Rings = [&Page]()
		{
			TArray<const FLoadoutV2Entry *> Out;
			for (const FLoadoutV2Entry &Entry : Page.Entries)
			{
				if (Entry.Kind == ELoadoutV2EntryKind::Adapter)
				{
					Out.Add(&Entry);
				}
			}
			return Out;
		}();
		if (!TestEqual(TEXT("two adapter entries"), Rings.Num(), 2)) return;
		TestEqual(TEXT("the first reads Lightning Ring"), Rings[0]->Name.ToString(), FString(TEXT("Lightning Ring")));
		TestEqual(TEXT("the second reads Lightning Ring 2"), Rings[1]->Name.ToString(), FString(TEXT("Lightning Ring 2")));
		TestTrue(TEXT("each its own place"), Rings[0]->AdapterIndex == 0 && Rings[1]->AdapterIndex == 1);
	});

	It("tells two Lightning rings of different tiers apart by the tier, with no number", [this]()
	{
		FCombatLoadout Loadout = TwoLightningRings();
		Loadout.RingLoadout[1].RingEntry.Tier = EItemTier::C_Tier;
		const FLoadoutV2WeaponsPage Page = WoRLoadoutV2::BuildWeaponsPage(Loadout, ECharacterClass::Resonator);
		TArray<const FLoadoutV2Entry *> Rings;
		for (const FLoadoutV2Entry &Entry : Page.Entries)
		{
			if (Entry.Kind == ELoadoutV2EntryKind::Adapter)
			{
				Rings.Add(&Entry);
			}
		}
		if (!TestEqual(TEXT("two adapter entries"), Rings.Num(), 2)) return;
		TestEqual(TEXT("the F one: its name"), Rings[0]->Name.ToString(), FString(TEXT("Lightning Ring")));
		TestEqual(TEXT("the C one: its name, no number"), Rings[1]->Name.ToString(), FString(TEXT("Lightning Ring")));
		TestTrue(TEXT("each carries its own tier for the badge"), Rings[0]->Tier == EItemTier::F_Tier && Rings[1]->Tier == EItemTier::C_Tier);
	});

	Describe("a cast", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("Test world created"), TestWorld->Create());
		});

		It("reads the ring whose own picks hold the spell, not the one in use", [this]()
		{
			ULoadoutComponent *Component = SpawnWearing(TwoLightningRings());
			if (!TestNotNull(TEXT("Loadout"), Component)) return;
			FCombatLoadout &Active = ActiveOf(Component);
			Active.RingLoadout[0].RingEntry.AttachedItem.Crystal = FCrystalAttachment(TWO_RINGS_TEST_FIRE, TWO_RINGS_TEST_DURABILITY);
			TestTrue(TEXT("Bolt (the first ring's pick) reads the first ring"),
					 Component->FindSpellCatalystAttachment(Bolt) == &Active.RingLoadout[0].RingEntry.AttachedItem);
			TestTrue(TEXT("Arc (the ring in use's pick) reads the ring in use"),
					 Component->FindSpellCatalystAttachment(Arc) == &Active.RingLoadout[TWO_RINGS_TEST_ACTIVE].RingEntry.AttachedItem);
		});

		It("reads the ring in use when both rings pick the same spell", [this]()
		{
			FCombatLoadout Loadout = TwoLightningRings();
			Loadout.RingLoadout[TWO_RINGS_TEST_ACTIVE].RingEntry.AssignedSpells = {Bolt};
			ULoadoutComponent *Component = SpawnWearing(Loadout);
			if (!TestNotNull(TEXT("Loadout"), Component)) return;
			TestTrue(TEXT("the ring in use"),
					 Component->FindSpellCatalystAttachment(Bolt) == &ActiveOf(Component).RingLoadout[TWO_RINGS_TEST_ACTIVE].RingEntry.AttachedItem);
		});

		It("wears the ring in use, not the first copy of the asset", [this]()
		{
			ULoadoutComponent *Component = SpawnWearing(TwoLightningRings());
			if (!TestNotNull(TEXT("Loadout"), Component)) return;
			TestTrue(TEXT("the ring in use's crystal"),
					 Component->FindAttachedItemByHolder(Adapter) == &ActiveOf(Component).RingLoadout[TWO_RINGS_TEST_ACTIVE].RingEntry.AttachedItem);
		});
	});

	It("keeps both rings, each with its own picks, through a preset save and reload", [this]()
	{
		const FCombatLoadout Loadout = TwoLightningRings();
		UInventoryComponent *Owned = NewObject<UInventoryComponent>(GetTransientPackage());
		Owned->Rings = {Loadout.RingLoadout[0].RingEntry, Loadout.RingLoadout[1].RingEntry};
		const FSavedLoadout Saved = Loadout.ToSavedLoadout(ECharacterClass::Resonator);
		if (!TestEqual(TEXT("saved: both rings"), Saved.EquippedRings.Num(), 2)) return;
		TestTrue(TEXT("saved: by piece id"), Saved.EquippedRings[0].RingInstance == FirstID && Saved.EquippedRings[1].RingInstance == SecondID);
		const FCombatLoadout Back = FCombatLoadout::CreateFromSavedLoadout(Saved, Owned, nullptr);
		if (!TestEqual(TEXT("reloaded: both rings"), Back.RingLoadout.Num(), 2)) return;
		TestTrue(TEXT("reloaded: each its own piece"),
				 Back.RingLoadout[0].RingEntry.PersistentID == FirstID && Back.RingLoadout[1].RingEntry.PersistentID == SecondID);
		TestTrue(TEXT("reloaded: each its own picks"),
				 Back.RingLoadout[0].GetAllSpells().Contains(Bolt) && !Back.RingLoadout[0].GetAllSpells().Contains(Arc) &&
					 Back.RingLoadout[1].GetAllSpells().Contains(Arc));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
