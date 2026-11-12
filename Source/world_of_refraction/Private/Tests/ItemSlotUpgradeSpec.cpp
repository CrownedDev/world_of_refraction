// ItemSlotUpgradeSpec.cpp
// B-239 (D-097, Q-239): the hub vendor sells one more item slot for a character's pouch: 10 to start, +1 per
// purchase, a rising price in hub Gold, a cap; refused during a run, when too poor or at the cap (nothing spent).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Inventory/InventoryConstants.h"
#include "Inventory/ItemSlotUpgrade.h"
#include "Player/WoRGameState.h"
#include "Player/WoRPlayerState.h"
#include "Pool/PoolSubsystem.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRTestWorld.h"

namespace
{
	const FName ITEM_SLOT_TEST_KEY(TEXT("ItemSlotTestCharacter"));
	constexpr int32 ITEM_SLOT_TEST_RICH = 100000;
	constexpr int32 ITEM_SLOT_TEST_SEED = 13;
	constexpr int32 ITEM_SLOT_TEST_STACK = 1;
}

BEGIN_DEFINE_SPEC(FItemSlotUpgradeSpec, "WoR.Inventory.ItemSlotUpgrade", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UCurrencyComponent *Wallet = nullptr;
END_DEFINE_SPEC(FItemSlotUpgradeSpec)

void FItemSlotUpgradeSpec::Define()
{
	It("prices rise with every slot bought, up to the cap", [this]()
	{
		const int32 First = ItemSlotUpgrade::GetPrice(InventoryConstants::ITEM_SLOTS_START);
		TestEqual(TEXT("the first costs the base"), First, ItemSlotUpgrade::BASE_PRICE);
		TestTrue(TEXT("the second costs more"), ItemSlotUpgrade::GetPrice(InventoryConstants::ITEM_SLOTS_START + 1) > First);
		TestTrue(TEXT("below the cap it grows"), ItemSlotUpgrade::CanGrow(InventoryConstants::ITEM_SLOTS_START));
		TestFalse(TEXT("at the cap it doesn't"), ItemSlotUpgrade::CanGrow(InventoryConstants::ITEM_SLOTS_MAX));
	});

	Describe("buying", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("world"), TestWorld->Create());
			UWorld *World = TestWorld->GetWorld();
			AWoRGameState *GameState = World ? World->SpawnActor<AWoRGameState>() : nullptr;
			AWoRPlayerState *Player = World ? World->SpawnActor<AWoRPlayerState>() : nullptr;
			if (GameState && Player)
			{
				World->SetGameState(GameState);
				GameState->AddPlayerState(Player);
				Wallet = Player->GetWallet();
			}
			Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		});
		AfterEach([this]()
		{
			Pool = nullptr;
			Wallet = nullptr;
			TestWorld.Reset();
		});

		It("adds a slot and spends the price; the pouch then holds 11 item types", [this]()
		{
			if (!TestTrue(TEXT("setup"), Pool && Wallet))
			{
				return;
			}
			const int32 Price = ItemSlotUpgrade::GetPrice(InventoryConstants::ITEM_SLOTS_START);
			Wallet->Add(ItemSlotUpgrade::GetCurrency(), Price);
			FText Refusal;
			TestTrue(TEXT("bought"), ItemSlotUpgrade::TryBuy(TestWorld->GetWorld(), *Pool, ITEM_SLOT_TEST_KEY, Wallet, Refusal));
			TestEqual(TEXT("11 slots"), Pool->GetItemSlotCount(ITEM_SLOT_TEST_KEY), InventoryConstants::ITEM_SLOTS_START + 1);
			TestEqual(TEXT("paid"), Wallet->GetBalance(ItemSlotUpgrade::GetCurrency()), 0);

			// Eleven item types (crystal type + tier) fit the pouch, none goes to storage.
			int32 Stored = 0;
			int32 Types = 0;
			for (uint8 TypeIndex = static_cast<uint8>(ECrystalType::None) + 1; Types < InventoryConstants::ITEM_SLOTS_START + 1; ++TypeIndex)
			{
				for (uint8 TierIndex = 0; TierIndex <= static_cast<uint8>(EItemTier::S_Tier) && Types < InventoryConstants::ITEM_SLOTS_START + 1; ++TierIndex)
				{
					FCrystalId Id;
					Id.Type = static_cast<ECrystalType>(TypeIndex);
					Id.Tier = static_cast<EItemTier>(TierIndex);
					Stored += Pool->CarryOrStore(ITEM_SLOT_TEST_KEY, Id, ITEM_SLOT_TEST_STACK);
					++Types;
				}
			}
			TestEqual(TEXT("11 types carried"), Pool->GetUsedItemSlots(ITEM_SLOT_TEST_KEY), InventoryConstants::ITEM_SLOTS_START + 1);
			TestEqual(TEXT("none stored"), Stored, 0);
		});

		It("refuses when too poor, at the cap, or during a run, spending nothing", [this]()
		{
			if (!TestTrue(TEXT("setup"), Pool && Wallet))
			{
				return;
			}
			FText Refusal;
			TestFalse(TEXT("too poor"), ItemSlotUpgrade::TryBuy(TestWorld->GetWorld(), *Pool, ITEM_SLOT_TEST_KEY, Wallet, Refusal));
			TestFalse(TEXT("a reason"), Refusal.IsEmpty());
			TestEqual(TEXT("no slot"), Pool->GetItemSlotCount(ITEM_SLOT_TEST_KEY), InventoryConstants::ITEM_SLOTS_START);

			Wallet->Add(ItemSlotUpgrade::GetCurrency(), ITEM_SLOT_TEST_RICH);
			while (Pool->GetItemSlotCount(ITEM_SLOT_TEST_KEY) < InventoryConstants::ITEM_SLOTS_MAX)
			{
				if (!TestTrue(TEXT("buys up to the cap"), ItemSlotUpgrade::TryBuy(TestWorld->GetWorld(), *Pool, ITEM_SLOT_TEST_KEY, Wallet, Refusal)))
				{
					return;
				}
			}
			const int32 Balance = Wallet->GetBalance(ItemSlotUpgrade::GetCurrency());
			TestFalse(TEXT("at the cap"), ItemSlotUpgrade::TryBuy(TestWorld->GetWorld(), *Pool, ITEM_SLOT_TEST_KEY, Wallet, Refusal));
			TestEqual(TEXT("nothing spent at the cap"), Wallet->GetBalance(ItemSlotUpgrade::GetCurrency()), Balance);

			const FName Other(TEXT("ItemSlotTestOther"));
			UTrialRunSubsystem *Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
			if (TestNotNull(TEXT("run"), Run))
			{
				Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), ITEM_SLOT_TEST_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
				TestFalse(TEXT("not during a run"), ItemSlotUpgrade::TryBuy(TestWorld->GetWorld(), *Pool, Other, Wallet, Refusal));
				TestEqual(TEXT("its pouch unchanged"), Pool->GetItemSlotCount(Other), InventoryConstants::ITEM_SLOTS_START);
			}
		});
	});
}

#endif
