// PouchItemUseSpec.cpp
// B-313 (D-111, D-117, Q-313): a party member's fight items are its own pouch, one instance per item. Using an
// item in a fight spends it from the user's pouch and nothing else (not another member's pouch, not the shared
// storage, not the preset's item slots). No tier cap on what a member carries or uses (B-354, D-143 retired the
// D-072 bands); stones are slot-only and never listed.
// B-313b: a member's crystal inventory reads its pouch; the item-slot rules skip a pouch member's preset picks,
// which hold no supply. B-317: the bound crystal inventory keeps no copy of the pouch.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryConstants.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/LoadoutConstants.h"
#include "Loadout/LoadoutRules.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"

namespace
{
	const FCrystalId POUCH_USE_GARNET(ECrystalType::Garnet, EItemTier::F_Tier);
	const FCrystalId POUCH_USE_SAPPHIRE(ECrystalType::Sapphire, EItemTier::F_Tier);
	const FCrystalId POUCH_USE_S_GARNET(ECrystalType::Garnet, EItemTier::S_Tier);
	const FCrystalId POUCH_USE_STONE(ECrystalType::DamageStone, EItemTier::F_Tier);

	constexpr int32 POUCH_USE_A_STACK = 5;
	constexpr int32 POUCH_USE_B_STACK = 3;
	constexpr int32 POUCH_USE_STORED = 2;
	constexpr int32 POUCH_USE_S_STACK = 3;
}

BEGIN_DEFINE_SPEC(FPouchItemUseSpec, "WoR.Pool.PouchItemUse",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UActionExecutor *Actions = nullptr;
	TArray<UCharacterData *> Members;
	AActor *User = nullptr;
	AActor *Target = nullptr;
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;

	FName KeyOf(int32 Index) const { return UPoolSubsystem::GetCharacterKey(Members[Index]); }

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
		UInventoryComponent *MemberInventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Actor);
		MemberInventory->SavedLoadouts = {FCombatLoadout()};
		MemberInventory->DrawSharedGear();
		return Actor;
	}
END_DEFINE_SPEC(FPouchItemUseSpec)

void FPouchItemUseSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		Actions = TestWorld->GetSubsystem<UActionExecutor>();
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Pool"), Pool) || !TestNotNull(TEXT("ActionExecutor"), Actions))
		{
			Pool = nullptr;
			return;
		}
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
		User = SpawnMember(0);
		if (!TestNotNull(TEXT("User"), User))
		{
			Pool = nullptr;
			return;
		}
		Loadout = User->FindComponentByClass<ULoadoutComponent>();
		Inventory = User->FindComponentByClass<UInventoryComponent>();
		Target = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Pool->CarryOrStore(KeyOf(0), POUCH_USE_GARNET, POUCH_USE_A_STACK);
		Pool->CarryOrStore(KeyOf(1), POUCH_USE_GARNET, POUCH_USE_B_STACK);
		Pool->AddCrystalToPool(POUCH_USE_GARNET, POUCH_USE_STORED);
		if (!TestTrue(TEXT("Ready for battle"), Loadout->PrepareForBattle(Inventory)))
		{
			Pool = nullptr;
		}
	});

	AfterEach([this]()
	{
		Pool = nullptr;
		Actions = nullptr;
		User = nullptr;
		Target = nullptr;
		Loadout = nullptr;
		Inventory = nullptr;
		Members.Empty();
		TestWorld.Reset();
	});

	It("spends one item from the user's pouch and nothing else", [this]()
	{
		if (!Pool) return;
		TestTrue(TEXT("The member fights with its pouch"), Loadout->UsesPouchItems());
		const int32 PresetUsesBefore = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].GetTotalItemUses();

		TestTrue(TEXT("The Garnet lands"), Actions->ExecuteItem(User, POUCH_USE_GARNET, {Target}).bSuccess);

		TestEqual(TEXT("One left the user's pouch"), Pool->GetCarriedCount(KeyOf(0), POUCH_USE_GARNET), POUCH_USE_A_STACK - 1);
		TestEqual(TEXT("The other member's pouch is untouched"), Pool->GetCarriedCount(KeyOf(1), POUCH_USE_GARNET), POUCH_USE_B_STACK);
		TestEqual(TEXT("Storage is untouched"), Pool->GetStoredCount(POUCH_USE_GARNET), POUCH_USE_STORED);
		TestEqual(TEXT("The preset's item slots are untouched"),
				  Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].GetTotalItemUses(), PresetUsesBefore);
		const UCrystalInventoryComponent *CrystalInv = User->FindComponentByClass<UCrystalInventoryComponent>();
		TestEqual(TEXT("The member's crystal inventory reads the pouch"), CrystalInv ? CrystalInv->GetCount(POUCH_USE_GARNET) : INDEX_NONE,
				  POUCH_USE_A_STACK - 1);
	});

	It("refuses an item the user's pouch doesn't carry, and spends nothing", [this]()
	{
		if (!Pool) return;
		TestFalse(TEXT("No Sapphire carried"), Actions->ExecuteItem(User, POUCH_USE_SAPPHIRE, {Target}).bSuccess);
		TestEqual(TEXT("The pouch keeps its Garnets"), Pool->GetCarriedCount(KeyOf(0), POUCH_USE_GARNET), POUCH_USE_A_STACK);
		TestEqual(TEXT("Storage keeps its Garnets"), Pool->GetStoredCount(POUCH_USE_GARNET), POUCH_USE_STORED);
	});

	It("lists the pouch's gems for the fight, but no stones", [this]()
	{
		if (!Pool) return;
		Pool->CarryOrStore(KeyOf(0), POUCH_USE_STONE, 1);
		const TArray<FItemLoadoutSlot> Usable = Loadout->GetUsableItems();
		if (!TestEqual(TEXT("One usable stack"), Usable.Num(), 1)) return;
		TestTrue(TEXT("The Garnet"), Usable[0].CrystalId == POUCH_USE_GARNET);
		TestEqual(TEXT("All of it: no tier cap"), Usable[0].Quantity, POUCH_USE_A_STACK);
	});

	It("has no tier cap on owned S crystals: all are carried and each can be used in one fight (B-354)", [this]()
	{
		if (!Pool) return;
		Pool->CarryOrStore(KeyOf(0), POUCH_USE_S_GARNET, POUCH_USE_S_STACK);
		TestEqual(TEXT("Every S crystal kept"), Pool->GetCarriedCount(KeyOf(0), POUCH_USE_S_GARNET) + Pool->GetStoredCount(POUCH_USE_S_GARNET),
				  POUCH_USE_S_STACK);
		const int32 Carried = Pool->GetCarriedCount(KeyOf(0), POUCH_USE_S_GARNET);
		for (int32 Use = 0; Use < Carried; ++Use)
		{
			TestTrue(TEXT("Each carried S crystal is usable"), Loadout->UseCarriedItem(POUCH_USE_S_GARNET));
		}
		TestEqual(TEXT("All the carried ones used in one fight"), Pool->GetCarriedCount(KeyOf(0), POUCH_USE_S_GARNET), 0);
	});

	It("reads a member's crystal counts from its pouch and keeps no copy of it (B-313b, B-317)", [this]()
	{
		if (!Pool) return;
		UCrystalInventoryComponent *CrystalInv = User->FindComponentByClass<UCrystalInventoryComponent>();
		if (!TestNotNull(TEXT("Crystal inventory"), CrystalInv)) return;
		Pool->CarryOrStore(KeyOf(0), POUCH_USE_STONE, 1);
		const TMap<FCrystalId, int32> Pouch = Pool->GetCarriedItems(KeyOf(0));
		int32 PouchTotal = 0;
		for (const TPair<FCrystalId, int32> &Pair : Pouch)
		{
			PouchTotal += Pair.Value;
		}
		TestTrue(TEXT("No copy of the pouch's gems (B-317)"), CrystalInv->Crystals.IsEmpty());
		TestTrue(TEXT("No copy of the pouch's stones (B-317)"), CrystalInv->Stones.IsEmpty());
		TestEqual(TEXT("The Garnets, from the pouch"), CrystalInv->GetCount(POUCH_USE_GARNET), POUCH_USE_A_STACK);
		TestEqual(TEXT("The stone, from the pouch"), CrystalInv->GetCount(POUCH_USE_STONE), Pool->GetCarriedCount(KeyOf(0), POUCH_USE_STONE));
		TestEqual(TEXT("The total"), CrystalInv->GetTotalCount(), PouchTotal);
		TestEqual(TEXT("The F-tier total"), CrystalInv->GetCountForTier(EItemTier::F_Tier), PouchTotal);
		TestEqual(TEXT("The stacks"), CrystalInv->GetStackCount(), Pouch.Num());
		TestEqual(TEXT("The held stacks are the pouch's"), CrystalInv->GetHeldStacks().Num(), Pouch.Num());
	});

	It("skips the item-slot rules for a pouch member's preset picks, which hold no supply (B-313b)", [this]()
	{
		if (!Pool) return;
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		FItemLoadoutSlot Pick;
		Pick.CrystalId = POUCH_USE_S_GARNET;
		Pick.Quantity = POUCH_USE_S_STACK;
		Active.ItemSlots = {Pick, Pick}; // over the one-S rule, and a duplicate type
		auto HasItemFinding = [](const TArray<FInvalidSlotFinding> &Findings)
		{
			return Findings.ContainsByPredicate([](const FInvalidSlotFinding &Finding)
			{
				return Finding.SlotType == ELoadoutSlotType::ItemSlot || Finding.SlotType == ELoadoutSlotType::DuplicateItems ||
					   Finding.SlotType == ELoadoutSlotType::EquippedSTierCrystal;
			});
		};
		TestFalse(TEXT("No item-slot finding for a pouch member"), HasItemFinding(Loadout->CollectInvalidSlotFindings()));
		FLoadoutRuleContext NoPouch;
		TestTrue(TEXT("The same picks are findings for an actor with no pouch"), HasItemFinding(FLoadoutRules::CollectFindings(Active, NoPouch)));
		Loadout->ClearInvalidSlots();
		TestEqual(TEXT("The picks are left as they are"), Active.ItemSlots.Num() == 2 ? Active.ItemSlots[1].Quantity : INDEX_NONE, POUCH_USE_S_STACK);
		TestEqual(TEXT("The pouch is untouched"), Pool->GetCarriedCount(KeyOf(0), POUCH_USE_GARNET), POUCH_USE_A_STACK);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
