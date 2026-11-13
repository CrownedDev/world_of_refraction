// BuyForSpec.cpp
// B-316 (D-111 buy-for, D-117, Q-316): one character's wallet buys for another party member. The bought weapon becomes
// the receiver's (one instance, no copy), the bought gems go to the receiver's pouch, and a cart the receiver can't
// take yet (an evolution, Q-323; a spell is bought for another member since B-316b) or a receiver outside the party is
// refused with nothing spent. Review fixes: a receiver with no
// room is refused before paying; a payer who owns a copy gets no duplicate prompt; gear bought in a run stays run-only.
// B-432: a payer whose own armoury is full still buys for a receiver with room (the gear only passes through).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyWallet.h"
#include "Currency/EconomyService.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Merchant/MerchantData.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "InventoryChangeRecorder.h"

namespace
{
	constexpr int32 BUY_FOR_PARTY_SIZE = 2;
	constexpr int32 BUY_FOR_PAYER = 0;
	constexpr int32 BUY_FOR_RECEIVER = 1;
	constexpr int32 BUY_FOR_WALLET_PRISMS = 100000;
	constexpr int32 BUY_FOR_WALLET_ESSENCE = 100000; // B-333: a gem costs Elemental essence on top of Prisms
	constexpr int32 BUY_FOR_GEM_COUNT = 3;
	const FCrystalId BUY_FOR_GEM(ECrystalType::Garnet, EItemTier::F_Tier);

	int32 CountOfAsset(const UPoolSubsystem &Pool, const UWeaponData *Asset)
	{
		return Pool.GetOwnedWeapons().FilterByPredicate([Asset](const FWeaponInventoryEntry &E) { return E.Weapon == Asset; }).Num();
	}
}

BEGIN_DEFINE_SPEC(FBuyForSpec, "WoR.Economy.BuyFor", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UEconomyService *Economy = nullptr;
	UWeaponData *Sword = nullptr;
	UWeaponData *Bought = nullptr;
	TArray<UCharacterData *> Members;
	TArray<UInventoryComponent *> Inventories;

	FName KeyOf(int32 Index) const { return UPoolSubsystem::GetCharacterKey(Members[Index]); }
	AActor *Payer() const { return Inventories[BUY_FOR_PAYER]->GetOwner(); }
	UCurrencyComponent *Wallet() const { return CurrencyWallet::FindFor(Payer()); }

	/** Party member Index as each level spawns it: the sword as its primary, then the draw. */
	UInventoryComponent *SpawnMember(int32 Index)
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
		Inventory->AddWeapon(Sword);
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Inventory->SavedLoadouts = {Loadout};
		Inventory->ActiveLoadoutIndex = 0;
		Inventory->LinkLoadoutsToOwned();
		Inventory->DrawSharedGear();
		return Inventory;
	}

	TArray<FMerchantStockEntry> WeaponOnly() const
	{
		FMerchantStockEntry Weapon;
		Weapon.Asset = Bought;
		return {Weapon};
	}

	/** The receiver's weapon of the bought asset, or null. */
	const FWeaponInventoryEntry *ReceiversBought() const
	{
		const FPoolGearView View = Pool->GetOwnedGearOf(KeyOf(BUY_FOR_RECEIVER));
		const FWeaponInventoryEntry *const *Found =
			View.Weapons.FindByPredicate([this](const FWeaponInventoryEntry *E) { return E->Weapon == Bought; });
		return Found ? *Found : nullptr;
	}

	TArray<FMerchantStockEntry> WeaponAndGems() const
	{
		FMerchantStockEntry Weapon;
		Weapon.Asset = Bought;
		FMerchantStockEntry Gems;
		Gems.Crystal = BUY_FOR_GEM;
		Gems.Count = BUY_FOR_GEM_COUNT;
		return {Weapon, Gems};
	}
END_DEFINE_SPEC(FBuyForSpec)

void FBuyForSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		Economy = TestWorld->GetSubsystem<UEconomyService>();
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Buy-for sword");
		Bought = NewObject<UWeaponData>(GetTransientPackage());
		Bought->Name = TEXT("Buy-for spear");
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Pool"), Pool) || !TestNotNull(TEXT("Economy"), Economy)) return;
		Party->Members.Empty();
		for (int32 Index = 0; Index < BUY_FOR_PARTY_SIZE; ++Index)
		{
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->Name = FString::Printf(TEXT("Buy-for member %d"), Index);
			FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
			Member.PawnClass = ACombatCharacter::StaticClass();
			Member.CharacterData = Data;
			Member.bPlayerControlled = Index == BUY_FOR_PAYER; // the receiver is an AI member
			Members.Add(Data);
		}
		for (int32 Index = 0; Index < BUY_FOR_PARTY_SIZE; ++Index)
		{
			Inventories.Add(SpawnMember(Index));
		}
		if (UCurrencyComponent *PayerWallet = Inventories[BUY_FOR_PAYER] ? Wallet() : nullptr)
		{
			PayerWallet->Add(ECurrencyType::Prisms, BUY_FOR_WALLET_PRISMS);
			PayerWallet->Add(ECurrencyType::ElementalEssence, BUY_FOR_WALLET_ESSENCE);
		}
	});

	AfterEach([this]()
	{
		Pool = nullptr;
		Economy = nullptr;
		Sword = nullptr;
		Bought = nullptr;
		Members.Empty();
		Inventories.Empty();
		TestWorld.Reset();
	});

	It("buys a weapon and gems for an AI member: one instance, the receiver's own, the gems in its pouch", [this]()
	{
		if (!Pool || !Economy || Inventories.Contains(nullptr) || !TestNotNull(TEXT("Wallet"), Wallet())) return;
		TestTrue(TEXT("bought for the receiver"), Economy->PurchaseFor(Payer(), KeyOf(BUY_FOR_RECEIVER), WeaponAndGems()));
		TestTrue(TEXT("Prisms spent from the payer's wallet"), Wallet()->GetBalance(ECurrencyType::Prisms) < BUY_FOR_WALLET_PRISMS);
		TestEqual(TEXT("one instance in the store"), CountOfAsset(*Pool, Bought), 1);
		const FPoolGearView ReceiverGear = Pool->GetOwnedGearOf(KeyOf(BUY_FOR_RECEIVER));
		TestTrue(TEXT("the receiver owns it"),
				 ReceiverGear.Weapons.ContainsByPredicate([this](const FWeaponInventoryEntry *E) { return E->Weapon == Bought; }));
		const FPoolGearView PayerGear = Pool->GetOwnedGearOf(KeyOf(BUY_FOR_PAYER));
		TestFalse(TEXT("the payer doesn't"),
				  PayerGear.Weapons.ContainsByPredicate([this](const FWeaponInventoryEntry *E) { return E->Weapon == Bought; }));
		TestTrue(TEXT("it is in the receiver's armoury"),
				 Inventories[BUY_FOR_RECEIVER]->Weapons.ContainsByPredicate([this](const FWeaponInventoryEntry &E) { return E.Weapon == Bought; }));
		TestFalse(TEXT("and not in the payer's"),
				  Inventories[BUY_FOR_PAYER]->Weapons.ContainsByPredicate([this](const FWeaponInventoryEntry &E) { return E.Weapon == Bought; }));
		TestEqual(TEXT("the gems are in the receiver's pouch"), Pool->GetCarriedCount(KeyOf(BUY_FOR_RECEIVER), BUY_FOR_GEM), BUY_FOR_GEM_COUNT);
		TestEqual(TEXT("not the payer's"), Pool->GetCarriedCount(KeyOf(BUY_FOR_PAYER), BUY_FOR_GEM), 0);
	});

	It("refuses an evolution for another member, and a receiver outside the party, with nothing spent", [this]()
	{
		if (!Pool || !Economy || Inventories.Contains(nullptr) || !TestNotNull(TEXT("Wallet"), Wallet())) return;
		FMerchantStockEntry Evolution;
		Evolution.Asset = NewObject<UEvolutionItemData>(GetTransientPackage());
		TestFalse(TEXT("an evolution for another member waits for an evolution give (Q-323)"),
				  Economy->PurchaseFor(Payer(), KeyOf(BUY_FOR_RECEIVER), {Evolution}));
		TestFalse(TEXT("not a party member"), Economy->PurchaseFor(Payer(), FName(TEXT("NotInTheParty")), WeaponAndGems()));
		TestEqual(TEXT("nothing spent"), Wallet()->GetBalance(ECurrencyType::Prisms), BUY_FOR_WALLET_PRISMS);
		TestEqual(TEXT("nothing bought"), CountOfAsset(*Pool, Bought), 0);
	});

	It("buys for yourself as a plain purchase", [this]()
	{
		if (!Pool || !Economy || Inventories.Contains(nullptr) || !TestNotNull(TEXT("Wallet"), Wallet())) return;
		TestTrue(TEXT("bought for yourself"), Economy->PurchaseFor(Payer(), KeyOf(BUY_FOR_PAYER), WeaponAndGems()));
		TestTrue(TEXT("the payer owns it"), Pool->GetOwnedGearOf(KeyOf(BUY_FOR_PAYER))
												.Weapons.ContainsByPredicate([this](const FWeaponInventoryEntry *E) { return E->Weapon == Bought; }));
		TestEqual(TEXT("the gems are in the payer's pouch"), Pool->GetCarriedCount(KeyOf(BUY_FOR_PAYER), BUY_FOR_GEM), BUY_FOR_GEM_COUNT);
	});

	It("refuses before paying when the receiver has no room for the gear", [this]()
	{
		if (!Pool || !Economy || Inventories.Contains(nullptr) || !TestNotNull(TEXT("Wallet"), Wallet())) return;
		UInventoryComponent *Receiver = Inventories[BUY_FOR_RECEIVER];
		while (Receiver->CanAddWeapon(Sword))
		{
			Receiver->AddWeapon(NewObject<UWeaponData>(GetTransientPackage()));
		}
		TestFalse(TEXT("a full receiver is refused"), Economy->PurchaseFor(Payer(), KeyOf(BUY_FOR_RECEIVER), WeaponOnly()));
		TestEqual(TEXT("nothing spent"), Wallet()->GetBalance(ECurrencyType::Prisms), BUY_FOR_WALLET_PRISMS);
		TestEqual(TEXT("nothing bought"), CountOfAsset(*Pool, Bought), 0);
	});

	It("buys for a receiver with room although the payer's own armoury is full (B-432)", [this]()
	{
		if (!Pool || !Economy || Inventories.Contains(nullptr) || !TestNotNull(TEXT("Wallet"), Wallet())) return;
		UInventoryComponent *PayerInventory = Inventories[BUY_FOR_PAYER];
		while (PayerInventory->CanAddWeapon(Sword))
		{
			PayerInventory->AddWeapon(NewObject<UWeaponData>(GetTransientPackage()));
		}
		TestTrue(TEXT("bought for the receiver"), Economy->PurchaseFor(Payer(), KeyOf(BUY_FOR_RECEIVER), WeaponOnly()));
		TestNotNull(TEXT("the receiver owns it"), ReceiversBought());
		TestFalse(TEXT("the payer is still full"), PayerInventory->CanAddWeapon(Sword));
		TestFalse(TEXT("and still refuses gear for itself"), Economy->PurchaseFor(Payer(), KeyOf(BUY_FOR_PAYER), WeaponOnly()));
	});

	It("gives a payer who owns a copy no duplicate prompt; the receiver gets the piece", [this]()
	{
		if (!Pool || !Economy || Inventories.Contains(nullptr) || !TestNotNull(TEXT("Wallet"), Wallet())) return;
		TestTrue(TEXT("the payer owns a copy"), Inventories[BUY_FOR_PAYER]->AddWeapon(Bought));
		UDuplicateGearRecorder *PayerRecorder = NewObject<UDuplicateGearRecorder>(GetTransientPackage());
		PayerRecorder->Listen(Inventories[BUY_FOR_PAYER]);
		TestTrue(TEXT("bought for the receiver"), Economy->PurchaseFor(Payer(), KeyOf(BUY_FOR_RECEIVER), WeaponOnly()));
		TestEqual(TEXT("no prompt for the payer"), PayerRecorder->NewIDs.Num(), 0);
		TestNotNull(TEXT("the receiver owns the new one"), ReceiversBought());
		TestEqual(TEXT("two instances in all: the payer's and the receiver's"), CountOfAsset(*Pool, Bought), 2);
	});

	It("keeps gear bought for another member during a run run-only", [this]()
	{
		if (!Pool || !Economy || Inventories.Contains(nullptr) || !TestNotNull(TEXT("Wallet"), Wallet())) return;
		Pool->BeginRunLayer();
		TestTrue(TEXT("bought for the receiver in a run"), Economy->PurchaseFor(Payer(), KeyOf(BUY_FOR_RECEIVER), WeaponOnly()));
		const FWeaponInventoryEntry *Piece = ReceiversBought();
		if (!TestNotNull(TEXT("the receiver owns it"), Piece)) return;
		TestTrue(TEXT("still run-only after the give"), Pool->IsRunOnly(Piece->PersistentID));
		Pool->EndRunLayer();
		TestNull(TEXT("gone when the run ends"), ReceiversBought());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
