// ItemSlotsSpec.cpp
// B-203 (D-097, Q-203): the party carries one inventory of item slots (one crystal type and tier
// per slot, stacked to ITEM_STACK_MAX); what doesn't fit goes to unlimited storage, and moving it
// back fails until there's room. Every party member's crystal inventory mirrors the carried slots.

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

	It("stores an 11th item type, and moving it back fails until a slot is free", [this]()
	{
		if (!Pool) return;
		for (int32 Index = 0; Index < InventoryConstants::ITEM_SLOTS_START; ++Index)
		{
			TestEqual(TEXT("Fits the carried slots"), Pool->CarryOrStore(ItemSlotsType(Index), 1), 0);
		}
		TestEqual(TEXT("All ten slots in use"), Pool->GetUsedItemSlots(), InventoryConstants::ITEM_SLOTS_START);
		TestEqual(TEXT("The 11th type goes to storage"), Pool->CarryOrStore(ITEM_SLOTS_ELEVENTH, 1), 1);
		TestEqual(TEXT("It waits there"), Pool->GetStoredCount(ITEM_SLOTS_ELEVENTH), 1);
		TestEqual(TEXT("Not carried"), Pool->GetCarriedCount(ITEM_SLOTS_ELEVENTH), 0);

		TestFalse(TEXT("No slot to move it back to"), Pool->MoveToCarried(ITEM_SLOTS_ELEVENTH, 1));
		TestEqual(TEXT("Still in storage"), Pool->GetStoredCount(ITEM_SLOTS_ELEVENTH), 1);
		TestTrue(TEXT("A slot is freed"), Pool->MoveToStorage(ItemSlotsType(0), 1));
		TestTrue(TEXT("Now it moves back"), Pool->MoveToCarried(ITEM_SLOTS_ELEVENTH, 1));
		TestEqual(TEXT("Carried"), Pool->GetCarriedCount(ITEM_SLOTS_ELEVENTH), 1);
		TestEqual(TEXT("Nothing left in storage"), Pool->GetStoredCount(ITEM_SLOTS_ELEVENTH), 0);
	});

	It("stores what doesn't fit a full stack", [this]()
	{
		if (!Pool) return;
		const FCrystalId Garnet = ItemSlotsType(0);
		TestEqual(TEXT("Two over the stack go to storage"), Pool->CarryOrStore(Garnet, InventoryConstants::ITEM_STACK_MAX + 2), 2);
		TestEqual(TEXT("One full stack carried"), Pool->GetCarriedCount(Garnet), InventoryConstants::ITEM_STACK_MAX);
		TestFalse(TEXT("A full stack takes no more"), Pool->MoveToCarried(Garnet, 1));
	});

	It("lets every party member see and use the same carried items", [this]()
	{
		if (!Pool) return;
		UCrystalInventoryComponent *First = SpawnMember(0);
		UCrystalInventoryComponent *Second = SpawnMember(1);
		if (!First || !Second) return;
		const FCrystalId Garnet = ItemSlotsType(0);
		TestTrue(TEXT("Both share the carried items"), First->IsSharingItems() && Second->IsSharingItems());

		TestTrue(TEXT("The first member finds two"), First->AddCount(Garnet, 2));
		TestEqual(TEXT("The second member sees them"), Second->GetCount(Garnet), 2);
		TestEqual(TEXT("The second uses one"), Second->RemoveCount(Garnet, 1), 1);
		TestEqual(TEXT("The first sees one left"), First->GetCount(Garnet), 1);
		TestEqual(TEXT("The party carries one"), Pool->GetCarriedCount(Garnet), 1);
	});

	It("never refuses a member's add for space: the rest goes to storage", [this]()
	{
		if (!Pool) return;
		UCrystalInventoryComponent *Member = SpawnMember(0);
		if (!Member) return;
		const FCrystalId Garnet = ItemSlotsType(0);
		const int32 Many = InventoryConstants::CRYSTAL_PER_TIER_CAP + 1;
		TestTrue(TEXT("Room is never the reason to refuse"), Member->CanAddCount(Garnet, Many));
		TestTrue(TEXT("Added"), Member->AddCount(Garnet, Many));
		TestEqual(TEXT("A full stack carried"), Member->GetCount(Garnet), InventoryConstants::ITEM_STACK_MAX);
		TestEqual(TEXT("The rest stored"), Pool->GetStoredCount(Garnet), Many - InventoryConstants::ITEM_STACK_MAX);
	});

	It("banks a member's starting items once, and a refill doesn't reach the party's", [this]()
	{
		if (!Pool) return;
		const FCrystalId Garnet = ItemSlotsType(0);
		UCrystalInventoryComponent *First = SpawnMember(0, Garnet, ITEM_SLOTS_STARTING_COUNT);
		if (!First) return;
		TestEqual(TEXT("Starting items carried"), Pool->GetCarriedCount(Garnet), ITEM_SLOTS_STARTING_COUNT);
		TestTrue(TEXT("Banked"), Pool->HasBankedItems(UPoolSubsystem::GetCharacterKey(Members[0])));

		// The next level spawns the same character with its authored items again.
		UCrystalInventoryComponent *Again = SpawnMember(0, Garnet, ITEM_SLOTS_STARTING_COUNT);
		if (!Again) return;
		TestEqual(TEXT("Not banked twice"), Pool->GetCarriedCount(Garnet), ITEM_SLOTS_STARTING_COUNT);
		TestEqual(TEXT("It sees the party's items"), Again->GetCount(Garnet), ITEM_SLOTS_STARTING_COUNT);

		Again->ClearAll();
		TestFalse(TEXT("A re-init unbinds"), Again->IsSharingItems());
		Again->AddCount(Garnet, 1);
		TestEqual(TEXT("The party's items are untouched"), Pool->GetCarriedCount(Garnet), ITEM_SLOTS_STARTING_COUNT);
	});

	It("gives back what a fight's auto-equip took and didn't use, and makes none up", [this]()
	{
		if (!Pool) return;
		UCrystalInventoryComponent *Crystals = SpawnMember(0);
		if (!Crystals) return;
		AActor *Owner = Crystals->GetOwner();
		UInventoryComponent *Inventory = Owner->FindComponentByClass<UInventoryComponent>();
		ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>();
		const FCrystalId Garnet = ItemSlotsType(0);
		const FCrystalId Sapphire = ItemSlotsType(1);
		Pool->CarryOrStore(Garnet, InventoryConstants::ITEM_STACK_MAX);

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

		const int32 Equipped = InventoryConstants::MAX_QUANTITY_PER_ITEM_SLOT;
		TestEqual(TEXT("Auto-equip took from the party's items"), Pool->GetCarriedCount(Garnet), InventoryConstants::ITEM_STACK_MAX - Equipped);
		TestEqual(TEXT("The dropped duplicate made no Sapphire"), Pool->GetCarriedCount(Sapphire) + Pool->GetStoredCount(Sapphire), 0);

		Inventory->SavedLoadouts[0].ItemSlots[0].Quantity -= 1; // one thrown in the fight
		Loadout->ResetBattleState();
		TestEqual(TEXT("The unused ones are back"), Pool->GetCarriedCount(Garnet), InventoryConstants::ITEM_STACK_MAX - 1);
		TestEqual(TEXT("They left the slot"), Inventory->SavedLoadouts[0].ItemSlots[0].Quantity, 0);
		TestTrue(TEXT("Nothing still owed"), Loadout->GetBattleItemsTaken().IsEmpty());
	});

	It("keeps an enemy's crystals its own", [this]()
	{
		if (!Pool) return;
		AActor *Enemy = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("Enemy"), Enemy)) return;
		UCrystalInventoryComponent *Crystals = FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Enemy);
		TestTrue(TEXT("Added"), Crystals->AddCount(ItemSlotsType(0), 2));
		TestFalse(TEXT("Not shared"), Crystals->IsSharingItems());
		TestEqual(TEXT("Nothing in the party's slots"), Pool->GetUsedItemSlots(), 0);
		TestFalse(TEXT("Its own tier cap still applies"), Crystals->CanAddCount(ItemSlotsType(1), InventoryConstants::CRYSTAL_PER_TIER_CAP));
	});

	It("keeps the carried items, the slot count and who is banked through a save", [this]()
	{
		if (!Pool) return;
		Pool->BankStartingItems(UPoolSubsystem::GetCharacterKey(Members[0]), {{ItemSlotsType(0), ITEM_SLOTS_STARTING_COUNT}});
		Pool->CarryOrStore(ItemSlotsType(1), InventoryConstants::ITEM_STACK_MAX + 1);
		const FSavedPool Saved = USaveSubsystem::CapturePool(*Pool);

		FWoRTestWorld After;
		UPoolSubsystem *Restored = After.Create() ? After.GetSubsystem<UPoolSubsystem>() : nullptr;
		if (!TestNotNull(TEXT("Pool after"), Restored)) return;
		USaveSubsystem::RestorePool(Saved, *Restored);
		TestEqual(TEXT("Same carried stack"), Restored->GetCarriedCount(ItemSlotsType(0)), ITEM_SLOTS_STARTING_COUNT);
		TestEqual(TEXT("Same full stack"), Restored->GetCarriedCount(ItemSlotsType(1)), InventoryConstants::ITEM_STACK_MAX);
		TestEqual(TEXT("Same stored overflow"), Restored->GetStoredCount(ItemSlotsType(1)), 1);
		TestEqual(TEXT("Same slot count"), Restored->GetItemSlotCount(), InventoryConstants::ITEM_SLOTS_START);
		TestTrue(TEXT("Still banked"), Restored->HasBankedItems(UPoolSubsystem::GetCharacterKey(Members[0])));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
