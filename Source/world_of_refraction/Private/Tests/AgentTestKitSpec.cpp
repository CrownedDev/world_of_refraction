// AgentTestKitSpec.cpp
// wor.Debug.GiveTestKit, headless: the adapters go in in turn until the kit's count is owned (20; D-250 removed the
// room, so the kit counts for itself), a second give adds nothing, and
// no adapter to give is no give; with no played character the kit says so.
// Unproven here: apparel, evolutions, skills and the second member on a real character (the brain's live check).

#if WITH_DEV_AUTOMATION_TESTS

#include "Debug/Inventory/AgentTestKitDebug.h"
#include "Equipment/Rings/RingData.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryConstants.h"
#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"

BEGIN_DEFINE_SPEC(FAgentTestKitSpec, "WoR.Debug.GiveTestKit", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FAgentTestKitSpec)

void FAgentTestKitSpec::Define()
{
	It("gives the adapters in turn up to the kit's count, and a second give adds none", [this]()
	{
		FWoRTestWorld TestWorld;
		TestTrue(TEXT("test world created"), TestWorld.Create());
		AActor *Owner = TestWorld.SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("owner"), Owner)) return;
		UInventoryComponent *Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Owner);

		TestEqual(TEXT("no adapter to give"), WoRAgentTestKit::GiveRings(*Inventory, {}), 0);

		const TArray<URingData *> Rings = {NewObject<URingData>(GetTransientPackage()), NewObject<URingData>(GetTransientPackage())};
		const int32 Given = WoRAgentTestKit::GiveRings(*Inventory, Rings);
		TestTrue(TEXT("some given"), Given > 0);
		TestTrue(TEXT("within the kit's count"), Given <= WoRAgentTestKit::ADAPTERS);
		TestEqual(TEXT("all owned"), Inventory->Rings.Num(), Given);
		TestTrue(TEXT("both kinds given"), Inventory->OwnsRingAsset(Rings[0]) && Inventory->OwnsRingAsset(Rings[1]));
		TestEqual(TEXT("a second give adds none"), WoRAgentTestKit::GiveRings(*Inventory, Rings), 0);
	});

	It("says so when there is no played character", [this]()
	{
		FWoRTestWorld TestWorld;
		TestTrue(TEXT("test world created"), TestWorld.Create());
		TestTrue(TEXT("refused with the reason"), WoRAgentTestKit::GiveTestKit(TestWorld.GetWorld(), {}).StartsWith(TEXT("no played character")));
		TestTrue(TEXT("no world"), WoRAgentTestKit::GiveTestKit(nullptr, {}).StartsWith(TEXT("no played character")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
