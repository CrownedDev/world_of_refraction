// ItemSlotsSpec.cpp
// B-203 / B-322 (D-097, D-117, Q-203, Q-322): each party member carries its own pouch. D-250 (B-575): it holds
// any number of kinds and any count; the item slots (10 each) limit only the fight-item pick, and nothing
// overflows to the one storage (Store / Take still move stacks until part 3). A member's crystal inventory
// reads its own pouch. The pick itself is in OneInventorySpec.
// B-313 (D-111): a fight uses the pouch as it is (no auto-equip into the preset's item slots).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryConstants.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"

namespace
{
	/** The Index-th gem type (Garnet first) at tier F: ten distinct item types fill ten slots. */
	FCrystalId ItemSlotsType(int32 Index)
	{
		return FCrystalId(static_cast<ECrystalType>(static_cast<int32>(ECrystalType::Garnet) + Index), EItemTier::F_Tier);
	}

	const FCrystalId ITEM_SLOTS_ELEVENTH(ECrystalType::DamageStone, EItemTier::F_Tier);
	constexpr int32 ITEM_SLOTS_STARTING_COUNT = 3;
}

BEGIN_DEFINE_SPEC(FItemSlotsSpec, "WoR.Pool.ItemSlots",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	TArray<UCharacterData *> Members;

	FName KeyOf(int32 Index) const { return UPoolSubsystem::GetCharacterKey(Members[Index]); }

	/** Spawns party member Index as each level does: its authored items, then the shared draw. */
	UCrystalInventoryComponent *SpawnMember(int32 Index, const FCrystalId &Starting = FCrystalId(), int32 StartingCount = 0)
	{
		AActor *Actor = TestWorld->SpawnCombatant(Members[Index]);
		if (!Actor)
		{
			return nullptr;
		}
		UCrystalInventoryComponent *Crystals = FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Actor);
		UInventoryComponent *Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Actor);
		if (StartingCount > 0)
		{
			Crystals->AddCount(Starting, StartingCount);
		}
		Inventory->SavedLoadouts = {FCombatLoadout()};
		Inventory->DrawSharedGear();
		return Crystals;
	}
END_DEFINE_SPEC(FItemSlotsSpec)

void FItemSlotsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Pool"), Pool)) return;
		Party->Members.Empty();
		for (int32 Index = 0; Index < 2; ++Index)
		{
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->Name = FString::Printf(TEXT("Member %d"), Index);
			FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
			Member.PawnClass = ACombatCharacter::StaticClass();
			Member.CharacterData = Data;
			Members.Add(Data);
		}
	});

	AfterEach([this]()
	{
		Pool = nullptr;
		Members.Empty();
		TestWorld.Reset();
	});

	It("carries an 11th item type in A's pouch, unpicked; storage only by Store, and Take needs no free slot (D-250)", [this]()
	{
		if (!Pool) return;
		const FName A = KeyOf(0);
		const FName B = KeyOf(1);
		for (int32 Index = 0; Index < InventoryConstants::ITEM_SLOTS_START; ++Index)
		{
			TestEqual(TEXT("Carried by A"), Pool->CarryOrStore(A, ItemSlotsType(Index), 1), 0);
		}
		TestEqual(TEXT("All ten of A's fight-item slots in use"), Pool->GetUsedItemSlots(A), InventoryConstants::ITEM_SLOTS_START);
		TestEqual(TEXT("B's pouch is empty"), Pool->GetUsedItemSlots(B), 0);
		TestEqual(TEXT("The 11th type is carried too, not stored"), Pool->CarryOrStore(A, ITEM_SLOTS_ELEVENTH, 1), 0);
		TestEqual(TEXT("Nothing in storage"), Pool->GetStoredCount(ITEM_SLOTS_ELEVENTH), 0);
		TestEqual(TEXT("A carries it"), Pool->GetCarriedCount(A, ITEM_SLOTS_ELEVENTH), 1);
		TestFalse(TEXT("But it is no fight item: the slots are taken"), Pool->IsFightItem(A, ITEM_SLOTS_ELEVENTH));
		TestEqual(TEXT("B doesn't carry it"), Pool->GetCarriedCount(B, ITEM_SLOTS_ELEVENTH), 0);

		// The old Store / Take still work until the chest goes (D-250 part 3).
		TestTrue(TEXT("A stores it"), Pool->MoveToStorage(A, ITEM_SLOTS_ELEVENTH, 1));
		TestEqual(TEXT("It waits in storage"), Pool->GetStoredCount(ITEM_SLOTS_ELEVENTH), 1);
		TestTrue(TEXT("Taking it back needs no free slot"), Pool->MoveToCarried(A, ITEM_SLOTS_ELEVENTH, 1));
		TestEqual(TEXT("A carries it"), Pool->GetCarriedCount(A, ITEM_SLOTS_ELEVENTH), 1);
		TestEqual(TEXT("Nothing left in storage"), Pool->GetStoredCount(ITEM_SLOTS_ELEVENTH), 0);
	});

	It("moves each pouch to and from the one storage", [this]()
	{
		if (!Pool) return;
		const FName A = KeyOf(0);
		const FName B = KeyOf(1);
		const FCrystalId Garnet = ItemSlotsType(0);
		Pool->CarryOrStore(A, Garnet, 2);
		TestTrue(TEXT("A puts one in storage"), Pool->MoveToStorage(A, Garnet, 1));
		TestEqual(TEXT("A keeps one"), Pool->GetCarriedCount(A, Garnet), 1);
		TestEqual(TEXT("Storage holds one"), Pool->GetStoredCount(Garnet), 1);
		TestTrue(TEXT("B takes it from storage"), Pool->MoveToCarried(B, Garnet, 1));
		TestEqual(TEXT("B carries it"), Pool->GetCarriedCount(B, Garnet), 1);
		TestEqual(TEXT("Storage is empty again"), Pool->GetStoredCount(Garnet), 0);
		TestFalse(TEXT("B can't take what isn't stored"), Pool->MoveToCarried(B, Garnet, 1));
		TestFalse(TEXT("A can't store more than it carries"), Pool->MoveToStorage(A, Garnet, 2));
	});

	It("carries past the old stack size: nothing overflows to storage (D-250)", [this]()
	{
		if (!Pool) return;
		const FName A = KeyOf(0);
		const FCrystalId Garnet = ItemSlotsType(0);
		TestEqual(TEXT("None goes to storage"), Pool->CarryOrStore(A, Garnet, InventoryConstants::ITEM_STACK_MAX + 2), 0);
		TestEqual(TEXT("All of it carried"), Pool->GetCarriedCount(A, Garnet), InventoryConstants::ITEM_STACK_MAX + 2);
		TestEqual(TEXT("Storage is empty"), Pool->GetStoredCount(Garnet), 0);
	});

	It("gives each member its own pouch: what one finds the other doesn't see", [this]()
	{
		if (!Pool) return;
		UCrystalInventoryComponent *First = SpawnMember(0);
		UCrystalInventoryComponent *Second = SpawnMember(1);
		if (!First || !Second) return;
		const FCrystalId Garnet = ItemSlotsType(0);
		TestTrue(TEXT("Both carry a pouch"), First->IsSharingItems() && Second->IsSharingItems());
		TestEqual(TEXT("The first mirrors its own"), First->GetPouchCharacterKey(), KeyOf(0));
		TestEqual(TEXT("The second mirrors its own"), Second->GetPouchCharacterKey(), KeyOf(1));

		TestTrue(TEXT("The first member finds two"), First->AddCount(Garnet, 2));
		TestEqual(TEXT("They are in the first's pouch"), Pool->GetCarriedCount(KeyOf(0), Garnet), 2);
		TestEqual(TEXT("The second member sees none"), Second->GetCount(Garnet), 0);
		TestEqual(TEXT("The second can't use one"), Second->RemoveCount(Garnet, 1), 0);
		TestEqual(TEXT("The first uses one"), First->RemoveCount(Garnet, 1), 1);
		TestEqual(TEXT("The first has one left"), First->GetCount(Garnet), 1);
	});

	It("never refuses a member's add for space: all of it is carried (D-250)", [this]()
	{
		if (!Pool) return;
		UCrystalInventoryComponent *Member = SpawnMember(0);
		if (!Member) return;
		const FCrystalId Garnet = ItemSlotsType(0);
		const int32 Many = InventoryConstants::CRYSTAL_PER_TIER_CAP + 1;
		TestTrue(TEXT("Room is never the reason to refuse"), Member->CanAddCount(Garnet, Many));
		TestTrue(TEXT("Added"), Member->AddCount(Garnet, Many));
		TestEqual(TEXT("All of it carried"), Member->GetCount(Garnet), Many);
		TestEqual(TEXT("None stored"), Pool->GetStoredCount(Garnet), 0);
	});

	It("banks a member's starting items once, and a refill doesn't reach the party's", [this]()
	{
		if (!Pool) return;
		const FCrystalId Garnet = ItemSlotsType(0);
		UCrystalInventoryComponent *First = SpawnMember(0, Garnet, ITEM_SLOTS_STARTING_COUNT);
		if (!First) return;
		TestEqual(TEXT("Starting items in its pouch"), Pool->GetCarriedCount(KeyOf(0), Garnet), ITEM_SLOTS_STARTING_COUNT);
		TestTrue(TEXT("Banked"), Pool->HasBankedItems(KeyOf(0)));

		// The next level spawns the same character with its authored items again.
		UCrystalInventoryComponent *Again = SpawnMember(0, Garnet, ITEM_SLOTS_STARTING_COUNT);
		if (!Again) return;
		TestEqual(TEXT("Not banked twice"), Pool->GetCarriedCount(KeyOf(0), Garnet), ITEM_SLOTS_STARTING_COUNT);
		TestEqual(TEXT("It sees its own items"), Again->GetCount(Garnet), ITEM_SLOTS_STARTING_COUNT);

		Again->ClearAll();
		TestFalse(TEXT("A re-init unbinds"), Again->IsSharingItems());
		Again->AddCount(Garnet, 1);
		TestEqual(TEXT("The pouch is untouched"), Pool->GetCarriedCount(KeyOf(0), Garnet), ITEM_SLOTS_STARTING_COUNT);
	});

	It("fights with the pouch as it is: no auto-equip, and the preset's item slots are not its items (B-313)", [this]()
	{
		if (!Pool) return;
		UCrystalInventoryComponent *Crystals = SpawnMember(0);
		if (!Crystals) return;
		AActor *Owner = Crystals->GetOwner();
		UInventoryComponent *Inventory = Owner->FindComponentByClass<UInventoryComponent>();
		ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>();
		const FCrystalId Garnet = ItemSlotsType(0);
		const FCrystalId Sapphire = ItemSlotsType(1);
		Pool->CarryOrStore(KeyOf(0), Garnet, InventoryConstants::ITEM_STACK_MAX);

		FCombatLoadout Battle;
		Battle.bAutoEquipItemsOnCombatStart = true;
		Battle.ItemSlots.SetNum(InventoryConstants::MAX_ITEM_LOADOUT_SLOTS);
		Battle.ItemSlots[0].CrystalId = Garnet;
		// Two authored Sapphire slots: the duplicate is dropped, and its crystals were never carried.
		Battle.ItemSlots[1].CrystalId = Sapphire;
		Battle.ItemSlots[1].Quantity = 1;
		Battle.ItemSlots[2].CrystalId = Sapphire;
		Battle.ItemSlots[2].Quantity = 1;
		Inventory->SavedLoadouts = {Battle};
		Inventory->ActiveLoadoutIndex = 0;
		if (!TestTrue(TEXT("Ready"), Loadout->PrepareForBattle(Inventory))) return;

		TestTrue(TEXT("It carries a pouch"), Loadout->UsesPouchItems());
		TestEqual(TEXT("Nothing left its pouch"), Pool->GetCarriedCount(KeyOf(0), Garnet), InventoryConstants::ITEM_STACK_MAX);
		TestEqual(TEXT("The preset's Garnet slot stays empty"), Inventory->SavedLoadouts[0].ItemSlots[0].Quantity, 0);
		TestEqual(TEXT("The dropped duplicate made no Sapphire"), Pool->GetCarriedCount(KeyOf(0), Sapphire) + Pool->GetStoredCount(Sapphire), 0);
		const TArray<FItemLoadoutSlot> Usable = Loadout->GetUsableItems();
		if (!TestEqual(TEXT("The fight lists the pouch only"), Usable.Num(), 1)) return;
		TestTrue(TEXT("Its Garnet"), Usable[0].CrystalId == Garnet);
		TestEqual(TEXT("The whole stack: no tier cap (B-354)"), Usable[0].Quantity, InventoryConstants::ITEM_STACK_MAX);

		Loadout->ResetBattleState();
		TestEqual(TEXT("Still all carried"), Pool->GetCarriedCount(KeyOf(0), Garnet), InventoryConstants::ITEM_STACK_MAX);
	});

	It("keeps an enemy's crystals its own", [this]()
	{
		if (!Pool) return;
		AActor *Enemy = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("Enemy"), Enemy)) return;
		UCrystalInventoryComponent *Crystals = FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Enemy);
		TestTrue(TEXT("Added"), Crystals->AddCount(ItemSlotsType(0), 2));
		TestFalse(TEXT("Not shared"), Crystals->IsSharingItems());
		TestEqual(TEXT("No pouch has anything"), Pool->GetPouches().Num(), 0);
		TestFalse(TEXT("Its own tier cap still applies"), Crystals->CanAddCount(ItemSlotsType(1), InventoryConstants::CRYSTAL_PER_TIER_CAP));
	});

	It("keeps each pouch, its slot count and who is banked through a save (v9)", [this]()
	{
		if (!Pool) return;
		Pool->BankStartingItems(KeyOf(0), {{ItemSlotsType(0), ITEM_SLOTS_STARTING_COUNT}});
		Pool->CarryOrStore(KeyOf(1), ItemSlotsType(1), InventoryConstants::ITEM_STACK_MAX + 1);
		const FSavedPool Saved = USaveSubsystem::CapturePool(*Pool);
		TestEqual(TEXT("Two pouches written"), Saved.Pouches.Num(), 2);

		FWoRTestWorld After;
		UPoolSubsystem *Restored = After.Create() ? After.GetSubsystem<UPoolSubsystem>() : nullptr;
		if (!TestNotNull(TEXT("Pool after"), Restored)) return;
		USaveSubsystem::RestorePool(Saved, *Restored);
		TestEqual(TEXT("A's stack"), Restored->GetCarriedCount(KeyOf(0), ItemSlotsType(0)), ITEM_SLOTS_STARTING_COUNT);
		TestEqual(TEXT("B's whole stack, past the old stack size"), Restored->GetCarriedCount(KeyOf(1), ItemSlotsType(1)), InventoryConstants::ITEM_STACK_MAX + 1);
		TestEqual(TEXT("Not in A's"), Restored->GetCarriedCount(KeyOf(0), ItemSlotsType(1)), 0);
		TestEqual(TEXT("Nothing stored (D-250: no overflow)"), Restored->GetStoredCount(ItemSlotsType(1)), 0);
		TestEqual(TEXT("Same slot count"), Restored->GetItemSlotCount(KeyOf(1)), InventoryConstants::ITEM_SLOTS_START);
		TestTrue(TEXT("Still banked"), Restored->HasBankedItems(KeyOf(0)));
	});

	It("hands a run-only member's pouch to the host when the run ends (D-250: not to storage)", [this]()
	{
		if (!Pool) return;
		UParty *Party = TestWorld->GetSubsystem<UPartySessionSubsystem>()->GetLocalParty();
		if (!TestNotNull(TEXT("Party"), Party)) return;
		Party->Members[1].bRunOnly = true;
		const FCrystalId Garnet = ItemSlotsType(0);
		Pool->CarryOrStore(KeyOf(1), Garnet, 2);
		TestEqual(TEXT("One run-only member leaves"), TestWorld->GetSubsystem<UPartySessionSubsystem>()->DismissRunMembers(), 1);
		TestEqual(TEXT("The host carries its items"), Pool->GetCarriedCount(KeyOf(0), Garnet), 2);
		TestEqual(TEXT("None in storage"), Pool->GetStoredCount(Garnet), 0);
		TestFalse(TEXT("Its pouch is gone"), Pool->GetPouches().Contains(KeyOf(1)));
	});

}

#endif // WITH_DEV_AUTOMATION_TESTS
