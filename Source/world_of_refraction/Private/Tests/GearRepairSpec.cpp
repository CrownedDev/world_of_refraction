// GearRepairSpec.cpp
// B-178 (D-093): a rest-spot repair pays Gold by each worn item's tier and brings the party's
// carried gear back to full. The run-end reset of wear is WoR.Party.CarriedState ("starts fresh
// after the run ends").

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Durability/GearRepair.h"
#include "Party/Party.h"

namespace
{
	constexpr int32 REPAIR_TEST_EVOLUTION_MAX = 20;
	constexpr int32 REPAIR_TEST_EVOLUTION_WORN = 5;
	const FCrystalId REPAIR_TEST_F_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	const FCrystalId REPAIR_TEST_C_GEM(ECrystalType::Garnet, EItemTier::C_Tier);

	FCarriedAttachment RepairTestGem(const FCrystalId &Id, int32 Durability)
	{
		FCarriedAttachment Carried;
		Carried.Attached.Kind = EAttachedItemKind::Crystal;
		Carried.Attached.Crystal = FCrystalAttachment(Id, Durability);
		return Carried;
	}
}

BEGIN_DEFINE_SPEC(FGearRepairSpec, "WoR.Equipment.GearRepair",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	UParty *Party = nullptr;
	UEvolutionItemData *Evolution = nullptr;
	int32 ExpectedCost = 0;
END_DEFINE_SPEC(FGearRepairSpec)

void FGearRepairSpec::Define()
{
	BeforeEach([this]()
	{
		Party = NewObject<UParty>();
		Evolution = NewObject<UEvolutionItemData>();
		Evolution->MaxDurability = REPAIR_TEST_EVOLUTION_MAX;

		FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
		Member.CarriedGear.bCarried = true;
		Member.CarriedGear.Attachments.Add(RepairTestGem(REPAIR_TEST_F_GEM, 1));
		Member.CarriedGear.Attachments.Add(RepairTestGem(REPAIR_TEST_C_GEM, 0));
		Member.CarriedGear.Attachments.Add(RepairTestGem(REPAIR_TEST_F_GEM, ItemIdentity::GetMaxDurability(REPAIR_TEST_F_GEM)));
		Member.CarriedGear.bHasPrimaryEvolution = true;
		Member.CarriedGear.PrimaryEvolution.Item = Evolution;
		Member.CarriedGear.PrimaryEvolution.Tier = EItemTier::B_Tier;
		Member.CarriedGear.PrimaryEvolution.CurrentDurability = REPAIR_TEST_EVOLUTION_WORN;

		// The worn F gem, the broken C gem and the worn B evolution; the full gem is free.
		ExpectedCost = GearRepair::GetGoldForTier(EItemTier::F_Tier) + GearRepair::GetGoldForTier(EItemTier::C_Tier) +
					   GearRepair::GetGoldForTier(EItemTier::B_Tier);
	});

	It("prices a repair by each worn item's tier", [this]()
	{
		TestTrue(TEXT("A higher tier costs more"),
				 GearRepair::GetGoldForTier(EItemTier::S_Tier) > GearRepair::GetGoldForTier(EItemTier::F_Tier));
		TestEqual(TEXT("Party repair cost"), GearRepair::GetPartyRepairCost(*Party), ExpectedCost);
	});

	It("pays the Gold and repairs everything to full", [this]()
	{
		FWoRTestWorld World;
		if (!TestTrue(TEXT("World"), World.Create()))
		{
			return;
		}
		UCurrencyComponent *Wallet = FWoRTestWorld::AddComponent<UCurrencyComponent>(World.GetWorld()->SpawnActor<AActor>());
		Wallet->Add(ECurrencyType::Gold, ExpectedCost + 1);

		int32 Spent = 0;
		TestTrue(TEXT("Repaired"), GearRepair::RepairParty(*Party, *Wallet, Spent));
		TestEqual(TEXT("Gold spent"), Spent, ExpectedCost);
		TestEqual(TEXT("Gold left"), Wallet->GetBalance(ECurrencyType::Gold), 1);
		const FCarriedGearState &Gear = Party->Members[0].CarriedGear;
		TestEqual(TEXT("Broken C gem at full"), Gear.Attachments[1].Attached.GetCurrentDurability(),
				  ItemIdentity::GetMaxDurability(REPAIR_TEST_C_GEM));
		TestEqual(TEXT("Evolution at full"), Gear.PrimaryEvolution.CurrentDurability, REPAIR_TEST_EVOLUTION_MAX);
		TestEqual(TEXT("Nothing left to repair"), GearRepair::GetPartyRepairCost(*Party), 0);
	});

	It("pays and repairs nothing when the wallet can't afford it", [this]()
	{
		FWoRTestWorld World;
		if (!TestTrue(TEXT("World"), World.Create()))
		{
			return;
		}
		UCurrencyComponent *Wallet = FWoRTestWorld::AddComponent<UCurrencyComponent>(World.GetWorld()->SpawnActor<AActor>());
		Wallet->Add(ECurrencyType::Gold, ExpectedCost - 1);

		int32 Spent = 0;
		TestFalse(TEXT("Not repaired"), GearRepair::RepairParty(*Party, *Wallet, Spent));
		TestEqual(TEXT("No Gold spent"), Wallet->GetBalance(ECurrencyType::Gold), ExpectedCost - 1);
		TestEqual(TEXT("Still worn"), GearRepair::GetPartyRepairCost(*Party), ExpectedCost);
	});

	It("charges nothing for an unbreakable evolution", [this]()
	{
		Evolution->Breakability = EBreakability::Unbreakable;
		TestEqual(TEXT("Cost without the evolution"), GearRepair::GetPartyRepairCost(*Party),
				  ExpectedCost - GearRepair::GetGoldForTier(EItemTier::B_Tier));
	});
}

#endif
