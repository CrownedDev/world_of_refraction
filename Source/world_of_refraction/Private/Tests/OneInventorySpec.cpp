// OneInventorySpec.cpp
// B-575 (D-250 part 1, Q-250): a character carries everything, with no limit on kinds, counts, weapons or
// adapters; nothing goes to storage (overflow, a kept run's finds, a recruit leaving). The only limit is the
// fight-item pick: up to the character's item slots, any kind with a use (every gem, the Healing Stone, the
// stat stones; not the attach-only Durability Stone), a new stack picked by itself while a slot is free. A fight lists the pick and brings at most ITEM_STACK_MAX of each; the rest stays carried.
// The pick is saved with the pouch (save v45). Q-250 (Crown: B): a version 44 save is wiped, no converter.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/StatStone.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryConstants.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveGame.h"
#include "Save/WoRSaveTypes.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr int32 ONE_INVENTORY_GEM_TYPES = 10;
	constexpr int32 ONE_INVENTORY_BIG_STACK = 25;
	constexpr int32 ONE_INVENTORY_FOUND = 3;

	/** The Index-th distinct gem kind: ten types at tier F (Garnet first), then the same ten at tier E. */
	FCrystalId OneInventoryGem(int32 Index)
	{
		return FCrystalId(static_cast<ECrystalType>(static_cast<int32>(ECrystalType::Garnet) + Index % ONE_INVENTORY_GEM_TYPES),
						  Index < ONE_INVENTORY_GEM_TYPES ? EItemTier::F_Tier : EItemTier::E_Tier);
	}

	/** A stat stone: its use is a buff or debuff (StoneRowTable). Carried as a StatStone (B-353). */
	const FCrystalId ONE_INVENTORY_STAT_STONE(ECrystalType::DamageStone, EItemTier::F_Tier);
	/** Consume-only: it heals, and can't be socketed usefully. */
	const FCrystalId ONE_INVENTORY_HEALING_STONE(ECrystalType::HealingStone, EItemTier::F_Tier);
	/** Attach-only: the one kind with no use in a fight. */
	const FCrystalId ONE_INVENTORY_DURABILITY_STONE(ECrystalType::DurabilityStone, EItemTier::F_Tier);

	/** The last save version from before the pick was saved (45). */
	constexpr int32 ONE_INVENTORY_LAST_VERSION_WITHOUT_PICK = 44;
	const FName ONE_INVENTORY_SAVED_VERSION(TEXT("SavedDataVersion"));
	const FName ONE_INVENTORY_LOADED_VERSION(TEXT("LoadedDataVersion"));
}

BEGIN_DEFINE_SPEC(FOneInventorySpec, "WoR.Pool.OneInventory",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	TArray<UCharacterData *> Members;

	FName KeyOf(int32 Index) const { return UPoolSubsystem::GetCharacterKey(Members[Index]); }

	/** Carries one of each of the first Kinds gem kinds. */
	void CarryKinds(FName Key, int32 Kinds)
	{
		for (int32 Index = 0; Index < Kinds; ++Index)
		{
			Pool->CarryOrStore(Key, OneInventoryGem(Index), 1);
		}
	}

	/** Spawns party member Index as each level does (ItemSlotsSpec's shape): the shared draw binds its pouch. */
	AActor *SpawnMember(int32 Index)
	{
		AActor *Actor = TestWorld->SpawnCombatant(Members[Index]);
		if (!Actor)
		{
			return nullptr;
		}
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Actor);
		UInventoryComponent *Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Actor);
		Inventory->SavedLoadouts = {FCombatLoadout()};
		Inventory->DrawSharedGear();
		return Actor;
	}

	/** Sets an int UPROPERTY by name (the save's version fields are protected on ULocalPlayerSaveGame; TwoSocketsSpec's way). */
	static bool SetIntProperty(UObject *Object, FName Name, int32 Value)
	{
		FIntProperty *Property = FindFProperty<FIntProperty>(Object->GetClass(), Name);
		if (!Property)
		{
			return false;
		}
		Property->SetPropertyValue_InContainer(Object, Value);
		return true;
	}
END_DEFINE_SPEC(FOneInventorySpec)

void FOneInventorySpec::Define()
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

	// ---- 1. No carry limit ----

	It("carries any number of kinds and any count: nothing overflows to storage", [this]()
	{
		if (!Pool) return;
		const FName A = KeyOf(0);
		const int32 Kinds = InventoryConstants::ITEM_SLOTS_MAX + 2; // more than even a fully upgraded pouch has slots
		for (int32 Index = 0; Index < Kinds; ++Index)
		{
			TestEqual(TEXT("none goes to storage"), Pool->CarryOrStore(A, OneInventoryGem(Index % (2 * ONE_INVENTORY_GEM_TYPES)), 1), 0);
		}
		TestEqual(TEXT("past the stack size too"), Pool->CarryOrStore(A, OneInventoryGem(0), ONE_INVENTORY_BIG_STACK), 0);
		TestEqual(TEXT("all of it carried"), Pool->GetCarriedCount(A, OneInventoryGem(0)), ONE_INVENTORY_BIG_STACK + 2);
		TestEqual(TEXT("every kind carried"), Pool->GetCarriedItems(A).Num(), 2 * ONE_INVENTORY_GEM_TYPES);
		TestEqual(TEXT("storage untouched"), Pool->GetStoredCount(OneInventoryGem(0)), 0);
		TestEqual(TEXT("room is never short"), Pool->GetCarryRoom(A, OneInventoryGem(0)), InventoryConstants::UNLIMITED_CARRY_ROOM);
	});

	It("has no weapon or adapter room to run out of", [this]()
	{
		if (!Pool) return;
		AActor *Actor = SpawnMember(0);
		UInventoryComponent *Inventory = Actor ? Actor->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!TestNotNull(TEXT("Inventory"), Inventory)) return;
		for (int32 Index = 0; Index <= InventoryConstants::MAX_WEAPON_INVENTORY_SLOTS; ++Index)
		{
			UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
			TestTrue(TEXT("room for another weapon"), Inventory->CanAddWeapon(Weapon));
			TestTrue(TEXT("added"), Inventory->AddWeapon(Weapon));
		}
		TestTrue(TEXT("more than the old room of 20"), Inventory->GetWeaponSlotCostTotal() > InventoryConstants::MAX_WEAPON_INVENTORY_SLOTS);
		TestEqual(TEXT("weapon room"), Inventory->GetRemainingWeaponCapacity(), InventoryConstants::UNLIMITED_CARRY_ROOM);
		TestEqual(TEXT("adapter room"), Inventory->GetRemainingRingCapacity(), InventoryConstants::UNLIMITED_CARRY_ROOM);
	});

	// ---- 2. The fight-item pick ----

	It("picks a new stack by itself while a slot is free, and no further", [this]()
	{
		if (!Pool) return;
		const FName A = KeyOf(0);
		CarryKinds(A, InventoryConstants::ITEM_SLOTS_START + 1);
		TestEqual(TEXT("ten picked"), Pool->GetUsedItemSlots(A), InventoryConstants::ITEM_SLOTS_START);
		TestTrue(TEXT("the first is a fight item"), Pool->IsFightItem(A, OneInventoryGem(0)));
		TestFalse(TEXT("the eleventh is carried, not picked"), Pool->IsFightItem(A, OneInventoryGem(InventoryConstants::ITEM_SLOTS_START)));
		TestEqual(TEXT("but carried"), Pool->GetCarriedCount(A, OneInventoryGem(InventoryConstants::ITEM_SLOTS_START)), 1);
		Pool->CarryOrStore(A, OneInventoryGem(0), 1);
		TestEqual(TEXT("more of a picked kind takes no slot"), Pool->GetUsedItemSlots(A), InventoryConstants::ITEM_SLOTS_START);
	});

	It("picks a stone with a use like a gem, and refuses the Durability Stone", [this]()
	{
		if (!Pool) return;
		const FName A = KeyOf(0);
		Pool->CarryOrStore(A, ONE_INVENTORY_HEALING_STONE, 2);
		Pool->CarryOrStore(A, ONE_INVENTORY_STAT_STONE, 1);
		Pool->CarryOrStore(A, ONE_INVENTORY_DURABILITY_STONE, 1);
		TestTrue(TEXT("the Healing Stone is picked"), Pool->IsFightItem(A, ONE_INVENTORY_HEALING_STONE));
		TestTrue(TEXT("the stat stone is picked"), Pool->IsFightItem(A, ONE_INVENTORY_STAT_STONE));
		TestEqual(TEXT("two slots used"), Pool->GetUsedItemSlots(A), 2);
		TestEqual(TEXT("the Durability Stone is carried"), Pool->GetCarriedCount(A, ONE_INVENTORY_DURABILITY_STONE), 1);
		TestFalse(TEXT("but not picked: it has no use in a fight"), Pool->IsFightItem(A, ONE_INVENTORY_DURABILITY_STONE));
		TestFalse(TEXT("nor by hand"), Pool->SetFightItem(A, ONE_INVENTORY_DURABILITY_STONE, true));
		TestTrue(TEXT("a stone can be unpicked"), Pool->SetFightItem(A, ONE_INVENTORY_STAT_STONE, false));
		TestTrue(TEXT("and picked again"), Pool->SetFightItem(A, ONE_INVENTORY_STAT_STONE, true));
	});

	It("picks and unpicks by hand, within the slots", [this]()
	{
		if (!Pool) return;
		const FName A = KeyOf(0);
		const FCrystalId Eleventh = OneInventoryGem(InventoryConstants::ITEM_SLOTS_START);
		const FCrystalId NotCarried = OneInventoryGem(InventoryConstants::ITEM_SLOTS_START + 1);
		CarryKinds(A, InventoryConstants::ITEM_SLOTS_START + 1);
		TestFalse(TEXT("no free slot for the eleventh"), Pool->SetFightItem(A, Eleventh, true));
		TestTrue(TEXT("unpick the first"), Pool->SetFightItem(A, OneInventoryGem(0), false));
		TestFalse(TEXT("unpicking it twice changes nothing"), Pool->SetFightItem(A, OneInventoryGem(0), false));
		TestEqual(TEXT("nine picked"), Pool->GetUsedItemSlots(A), InventoryConstants::ITEM_SLOTS_START - 1);
		TestEqual(TEXT("the unpicked one is still carried"), Pool->GetCarriedCount(A, OneInventoryGem(0)), 1);
		TestFalse(TEXT("what isn't carried can't be picked"), Pool->SetFightItem(A, NotCarried, true));
		TestTrue(TEXT("the eleventh takes the free slot"), Pool->SetFightItem(A, Eleventh, true));
		TestTrue(TEXT("it is a fight item"), Pool->IsFightItem(A, Eleventh));
		TestTrue(TEXT("the print marks it"), Pool->GetItemSlotsString().Contains(TEXT("(fight item)")));
	});

	It("unpicks a stack that runs out, and the freed slot goes to the next new stack", [this]()
	{
		if (!Pool) return;
		const FName A = KeyOf(0);
		CarryKinds(A, InventoryConstants::ITEM_SLOTS_START);
		TestEqual(TEXT("the last one is used"), Pool->TakeCarried(A, OneInventoryGem(0), 1), 1);
		TestEqual(TEXT("its slot is free"), Pool->GetUsedItemSlots(A), InventoryConstants::ITEM_SLOTS_START - 1);
		Pool->CarryOrStore(A, OneInventoryGem(InventoryConstants::ITEM_SLOTS_START), 1);
		TestTrue(TEXT("a new kind takes it"), Pool->IsFightItem(A, OneInventoryGem(InventoryConstants::ITEM_SLOTS_START)));
	});

	It("lets a bought slot take one more fight item", [this]()
	{
		if (!Pool) return;
		const FName A = KeyOf(0);
		const FCrystalId Eleventh = OneInventoryGem(InventoryConstants::ITEM_SLOTS_START);
		CarryKinds(A, InventoryConstants::ITEM_SLOTS_START + 1);
		TestTrue(TEXT("a slot is bought"), Pool->AddItemSlot(A));
		TestTrue(TEXT("the eleventh can be picked"), Pool->SetFightItem(A, Eleventh, true));
		TestEqual(TEXT("eleven picked"), Pool->GetUsedItemSlots(A), InventoryConstants::ITEM_SLOTS_START + 1);
	});

	It("moves the pick with a given stack: off the giver, onto the receiver's free slot", [this]()
	{
		if (!Pool) return;
		const FName A = KeyOf(0);
		const FName B = KeyOf(1);
		Pool->CarryOrStore(A, OneInventoryGem(0), 2);
		TestTrue(TEXT("given whole"), Pool->GiveCarried(A, B, OneInventoryGem(0), 2));
		TestEqual(TEXT("the giver's slot is free"), Pool->GetUsedItemSlots(A), 0);
		TestTrue(TEXT("the receiver picked it"), Pool->IsFightItem(B, OneInventoryGem(0)));
	});

	// ---- 3. Fights read the pick ----

	It("lists only the pick in a fight, at most ten of each, and the rest stays carried", [this]()
	{
		if (!Pool) return;
		AActor *Actor = SpawnMember(0);
		ULoadoutComponent *Loadout = Actor ? Actor->FindComponentByClass<ULoadoutComponent>() : nullptr;
		UInventoryComponent *Inventory = Actor ? Actor->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!TestNotNull(TEXT("Loadout"), Loadout) || !TestNotNull(TEXT("Inventory"), Inventory)) return;
		const FName A = KeyOf(0);
		const FCrystalId Big = OneInventoryGem(0);
		const FCrystalId Unpicked = OneInventoryGem(InventoryConstants::ITEM_SLOTS_START);
		Pool->CarryOrStore(A, Big, ONE_INVENTORY_BIG_STACK);
		CarryKinds(A, InventoryConstants::ITEM_SLOTS_START + 1); // one more of Big, nine other picks, one unpicked
		const int32 BigCarried = ONE_INVENTORY_BIG_STACK + 1;
		Inventory->ActiveLoadoutIndex = 0;
		if (!TestTrue(TEXT("Ready"), Loadout->PrepareForBattle(Inventory))) return;

		const TArray<FItemLoadoutSlot> Usable = Loadout->GetUsableItems();
		TestEqual(TEXT("the fight lists the ten picked"), Usable.Num(), InventoryConstants::ITEM_SLOTS_START);
		const FItemLoadoutSlot *BigSlot = Usable.FindByPredicate([&Big](const FItemLoadoutSlot &Slot) { return Slot.CrystalId == Big; });
		TestTrue(TEXT("ten of the big stack come in"), BigSlot && BigSlot->Quantity == InventoryConstants::ITEM_STACK_MAX);
		TestFalse(TEXT("the unpicked kind is not listed"), Usable.ContainsByPredicate([&Unpicked](const FItemLoadoutSlot &Slot) { return Slot.CrystalId == Unpicked; }));
		TestFalse(TEXT("nor usable"), Loadout->CanUseCarriedItem(Unpicked));
		TestFalse(TEXT("the press is refused"), Loadout->UseCarriedItem(Unpicked));

		for (int32 Use = 0; Use < InventoryConstants::ITEM_STACK_MAX; ++Use)
		{
			TestTrue(TEXT("one of the ten is used"), Loadout->UseCarriedItem(Big));
		}
		TestEqual(TEXT("none left this fight"), Loadout->GetFightItemsLeft(Big), 0);
		TestFalse(TEXT("an eleventh is refused"), Loadout->UseCarriedItem(Big));
		TestEqual(TEXT("the rest is still carried"), Pool->GetCarriedCount(A, Big), BigCarried - InventoryConstants::ITEM_STACK_MAX);
		TestTrue(TEXT("and still picked"), Pool->IsFightItem(A, Big));

		Loadout->ResetBattleState();
		if (!TestTrue(TEXT("Ready again"), Loadout->PrepareForBattle(Inventory))) return;
		TestEqual(TEXT("the next fight brings ten again"), Loadout->GetFightItemsLeft(Big), InventoryConstants::ITEM_STACK_MAX);
	});

	It("lists picked stones in a fight and uses them: a Healing Stone and a stat stone work, a Durability Stone is not there", [this]()
	{
		if (!Pool) return;
		UActionExecutor *Actions = TestWorld->GetSubsystem<UActionExecutor>();
		AActor *User = SpawnMember(0);
		AActor *Enemy = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		ULoadoutComponent *Loadout = User ? User->FindComponentByClass<ULoadoutComponent>() : nullptr;
		UInventoryComponent *Inventory = User ? User->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!TestNotNull(TEXT("ActionExecutor"), Actions) || !TestNotNull(TEXT("Loadout"), Loadout) || !TestNotNull(TEXT("Enemy"), Enemy)) return;
		const FName A = KeyOf(0);
		const FCrystalId StatStoneCarried = StatStone::Normalize(ONE_INVENTORY_STAT_STONE);
		Pool->CarryOrStore(A, ONE_INVENTORY_HEALING_STONE, 2);
		Pool->CarryOrStore(A, ONE_INVENTORY_STAT_STONE, 2);
		Pool->CarryOrStore(A, ONE_INVENTORY_DURABILITY_STONE, 2);
		Inventory->ActiveLoadoutIndex = 0;
		if (!TestTrue(TEXT("Ready"), Loadout->PrepareForBattle(Inventory))) return;

		const TArray<FItemLoadoutSlot> Usable = Loadout->GetUsableItems();
		TestEqual(TEXT("the two stones with a use are listed"), Usable.Num(), 2);
		TestTrue(TEXT("the Healing Stone"), Usable.ContainsByPredicate([](const FItemLoadoutSlot &Slot) { return Slot.CrystalId == ONE_INVENTORY_HEALING_STONE && Slot.Quantity == 2; }));
		TestTrue(TEXT("the stat stone"), Usable.ContainsByPredicate([&StatStoneCarried](const FItemLoadoutSlot &Slot) { return Slot.CrystalId == StatStoneCarried && Slot.Quantity == 2; }));
		TestFalse(TEXT("the Durability Stone can't be used"), Loadout->CanUseCarriedItem(ONE_INVENTORY_DURABILITY_STONE));

		FAction Item;
		Item.ActionType = EActionType::Item;
		Item.Targets.Add(User);
		Item.ItemData = ONE_INVENTORY_HEALING_STONE;
		TestTrue(TEXT("the Healing Stone is a valid action"), Actions->ValidateAction(User, Item).bIsValid);
		TestTrue(TEXT("the Healing Stone heals"), Actions->ExecuteItem(User, ONE_INVENTORY_HEALING_STONE, {User}).bSuccess);
		TestEqual(TEXT("one Healing Stone spent"), Pool->GetCarriedCount(A, ONE_INVENTORY_HEALING_STONE), 1);
		TestTrue(TEXT("the stat stone lands on the enemy"), Actions->ExecuteItem(User, ONE_INVENTORY_STAT_STONE, {Enemy}).bSuccess);
		TestEqual(TEXT("one stat stone spent"), Pool->GetCarriedCount(A, ONE_INVENTORY_STAT_STONE), 1);
		TestEqual(TEXT("one left this fight"), Loadout->GetFightItemsLeft(ONE_INVENTORY_STAT_STONE), 1);
		TestFalse(TEXT("the Durability Stone is refused"), Actions->ExecuteItem(User, ONE_INVENTORY_DURABILITY_STONE, {Enemy}).bSuccess);
		TestEqual(TEXT("and none is spent"), Pool->GetCarriedCount(A, ONE_INVENTORY_DURABILITY_STONE), 2);
	});

	// ---- 4. Nothing goes to storage ----

	It("keeps a kept run's finds carried", [this]()
	{
		if (!Pool) return;
		const FName A = KeyOf(0);
		Pool->BeginRunLayer();
		Pool->CarryOrStore(A, OneInventoryGem(0), ONE_INVENTORY_FOUND);
		Pool->CloseRun(EPoolRunEnding::Kept);
		TestEqual(TEXT("still carried"), Pool->GetCarriedCount(A, OneInventoryGem(0)), ONE_INVENTORY_FOUND);
		TestEqual(TEXT("none stored"), Pool->GetStoredCount(OneInventoryGem(0)), 0);
	});

	It("hands a leaving recruit's items to the host, not to storage", [this]()
	{
		if (!Pool) return;
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession->GetLocalParty();
		if (!TestNotNull(TEXT("Party"), Party)) return;
		Party->Members[1].bRunOnly = true;
		Pool->CarryOrStore(KeyOf(1), OneInventoryGem(0), 2);
		TestEqual(TEXT("One run-only member leaves"), PartySession->DismissRunMembers(), 1);
		TestEqual(TEXT("The host carries its items"), Pool->GetCarriedCount(KeyOf(0), OneInventoryGem(0)), 2);
		TestEqual(TEXT("None stored"), Pool->GetStoredCount(OneInventoryGem(0)), 0);
		TestFalse(TEXT("Its pouch is gone"), Pool->GetPouches().Contains(KeyOf(1)));
	});

	// ---- The save (v45, Q-250: older saves are wiped, no converter) ----

	It("keeps the pick through a save, hand-made changes and pick order included", [this]()
	{
		if (!Pool) return;
		const FName A = KeyOf(0);
		const FName B = KeyOf(1);
		const FCrystalId Eleventh = OneInventoryGem(InventoryConstants::ITEM_SLOTS_START);
		CarryKinds(A, InventoryConstants::ITEM_SLOTS_START + 2);
		Pool->SetFightItem(A, OneInventoryGem(0), false);
		Pool->SetFightItem(A, Eleventh, true); // now the last in pick order
		CarryKinds(B, 1);
		Pool->CarryOrStore(B, ONE_INVENTORY_HEALING_STONE, 1);
		Pool->CarryOrStore(B, ONE_INVENTORY_STAT_STONE, 1);
		Pool->SetFightItem(B, OneInventoryGem(0), false); // B's pick is its two stones only
		const TArray<FCrystalId> PickedBefore = Pool->GetFightItems(A);
		const TArray<FCrystalId> StonesPickedBefore = Pool->GetFightItems(B);
		const FSavedPool Saved = USaveSubsystem::CapturePool(*Pool);

		FWoRTestWorld After;
		UPoolSubsystem *Restored = After.Create() ? After.GetSubsystem<UPoolSubsystem>() : nullptr;
		if (!TestNotNull(TEXT("Pool after"), Restored)) return;
		USaveSubsystem::RestorePool(Saved, *Restored);
		TestEqual(TEXT("everything still carried"), Restored->GetCarriedItems(A).Num(), InventoryConstants::ITEM_SLOTS_START + 2);
		TestTrue(TEXT("the same pick, in the same order"), Restored->GetFightItems(A) == PickedBefore);
		TestFalse(TEXT("the unpicked one stays unpicked"), Restored->IsFightItem(A, OneInventoryGem(0)));
		TestTrue(TEXT("the hand-picked one stays picked"), Restored->IsFightItem(A, Eleventh));
		TestEqual(TEXT("the stone pick: two"), StonesPickedBefore.Num(), 2);
		TestTrue(TEXT("the stone pick comes back the same"), Restored->GetFightItems(B) == StonesPickedBefore);
		TestTrue(TEXT("the Healing Stone is still picked"), Restored->IsFightItem(B, ONE_INVENTORY_HEALING_STONE));
		TestTrue(TEXT("the stat stone is still picked"), Restored->IsFightItem(B, ONE_INVENTORY_STAT_STONE));
		TestFalse(TEXT("the gem unpicked beside them stays unpicked"), Restored->IsFightItem(B, OneInventoryGem(0)));
		TestEqual(TEXT("and stays carried"), Restored->GetCarriedCount(B, OneInventoryGem(0)), 1);
	});

	It("keeps an emptied pick empty through a save", [this]()
	{
		if (!Pool) return;
		const FName A = KeyOf(0);
		CarryKinds(A, 2);
		Pool->SetFightItem(A, OneInventoryGem(0), false);
		Pool->SetFightItem(A, OneInventoryGem(1), false);
		const FSavedPool Saved = USaveSubsystem::CapturePool(*Pool);

		FWoRTestWorld After;
		UPoolSubsystem *Restored = After.Create() ? After.GetSubsystem<UPoolSubsystem>() : nullptr;
		if (!TestNotNull(TEXT("Pool after"), Restored)) return;
		USaveSubsystem::RestorePool(Saved, *Restored);
		TestEqual(TEXT("nothing is picked for it"), Restored->GetUsedItemSlots(A), 0);
		TestEqual(TEXT("its items stay carried"), Restored->GetCarriedItems(A).Num(), 2);
	});

	It("drops a saved pick the pouch can't back", [this]()
	{
		if (!Pool) return;
		const FName A = KeyOf(0);
		CarryKinds(A, 2);
		Pool->CarryOrStore(A, ONE_INVENTORY_DURABILITY_STONE, 1);
		FSavedPool Saved = USaveSubsystem::CapturePool(*Pool);
		if (!TestEqual(TEXT("one pouch written"), Saved.Pouches.Num(), 1)) return;
		FSavedPouch &Pouch = Saved.Pouches[0];
		if (!TestEqual(TEXT("three stacks, two picked"), Pouch.Stacks.Num() - Pouch.FightItemStacks.Num(), 1)) return;
		int32 StoneStack = 0; // the one stack the pick doesn't name
		while (Pouch.FightItemStacks.Contains(StoneStack))
		{
			++StoneStack;
		}
		Pouch.FightItemStacks.Add(StoneStack);				 // the Durability Stone: no use in a fight
		Pouch.FightItemStacks.Add(Pouch.Stacks.Num());		 // no such stack
		Pouch.FightItemStacks.Add(Pouch.FightItemStacks[0]); // the same one twice

		FWoRTestWorld After;
		UPoolSubsystem *Restored = After.Create() ? After.GetSubsystem<UPoolSubsystem>() : nullptr;
		if (!TestNotNull(TEXT("Pool after"), Restored)) return;
		USaveSubsystem::RestorePool(Saved, *Restored);
		TestEqual(TEXT("only the two carried gems are picked"), Restored->GetUsedItemSlots(A), 2);
		TestFalse(TEXT("not the Durability Stone"), Restored->IsFightItem(A, ONE_INVENTORY_DURABILITY_STONE));
	});

	It("wipes a version 44 save: no converter (Q-250, D-138)", [this]()
	{
		UWoRSaveGame *Save = NewObject<UWoRSaveGame>(GetTransientPackage()); // in memory only: no save file is read or written
		const bool bSet = SetIntProperty(Save, ONE_INVENTORY_SAVED_VERSION, ONE_INVENTORY_LAST_VERSION_WITHOUT_PICK);
		SetIntProperty(Save, ONE_INVENTORY_LOADED_VERSION, ONE_INVENTORY_LAST_VERSION_WITHOUT_PICK);
		if (!TestTrue(TEXT("version field found"), bSet)) return;
		Save->Data.Pool.Pouches.AddDefaulted();
		Save->HandlePostLoad();
		TestTrue(TEXT("wiped"), Save->WasWiped());
		TestEqual(TEXT("its pouches are gone"), Save->Data.Pool.Pouches.Num(), 0);
		TestTrue(TEXT("the pick's version is the minimum"), WoRSaveVersion::MIN_SUPPORTED > ONE_INVENTORY_LAST_VERSION_WITHOUT_PICK);
		TestEqual(TEXT("the minimum is the latest (D-138)"), WoRSaveVersion::MIN_SUPPORTED, WoRSaveVersion::LATEST);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
