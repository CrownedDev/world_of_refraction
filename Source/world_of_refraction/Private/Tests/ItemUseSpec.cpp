// ItemUseSpec.cpp
// B-060: item use reports whether it landed. A use that fails (here a Garnet thrown at an actor
// with no character data) reports failure; an area use counts as landed when at least one target
// took it. B-201 (Q-060 changed): the failed use still spends its charge, like a missed throw.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Inventory/ItemExecutor.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Combat/Actions/ActionExecutor.h"

namespace
{
	const FCrystalId ITEM_USE_GARNET(ECrystalType::Garnet, EItemTier::F_Tier);

	/** Garnets in the loaded slot; it sits after an empty slot so the raw slot index is tested. */
	constexpr int32 ITEM_USE_GARNET_CHARGES = 2;
	constexpr int32 ITEM_USE_GARNET_SLOT = 1;

	template <class TComponent>
	TComponent *ItemUseTestAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}
}

BEGIN_DEFINE_SPEC(FItemUseSpec, "WoR.Items.Use",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UItemExecutor *Executor = nullptr;
	AActor *User = nullptr;
	AActor *Combatant = nullptr;
	/** No UCharacterDataComponent: Garnet's damage handler refuses it. */
	AActor *Bare = nullptr;
	ULoadoutComponent *Loadout = nullptr;
	UActionExecutor *Actions = nullptr;
END_DEFINE_SPEC(FItemUseSpec)

void FItemUseSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		Executor = TestWorld->GetSubsystem<UItemExecutor>();
		TestNotNull(TEXT("ItemExecutor"), Executor);
		User = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Combatant = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Bare = TestWorld->GetWorld()->SpawnActor<AActor>();
	});

	AfterEach([this]()
	{
		Executor = nullptr;
		User = nullptr;
		Combatant = nullptr;
		Bare = nullptr;
		Loadout = nullptr;
		Actions = nullptr;
		TestWorld.Reset();
	});

	It("reports a use that landed", [this]()
	{
		if (!Executor) return;
		TestTrue(TEXT("Garnet on a combatant"), Executor->UseItem(User, ITEM_USE_GARNET, Combatant).bSuccess);
	});

	It("reports a use that failed, with its reason", [this]()
	{
		if (!Executor) return;
		const FItemUseResult Result = Executor->UseItem(User, ITEM_USE_GARNET, Bare);
		TestFalse(TEXT("Garnet on an actor with no character data"), Result.bSuccess);
		TestFalse(TEXT("a reason is given"), Result.ErrorMessage.IsEmpty());
	});

	It("counts an area use as landed when one target took it", [this]()
	{
		if (!Executor) return;
		const FItemUseResult Result = Executor->UseItemMultiTarget(User, ITEM_USE_GARNET, {Bare, Combatant});
		TestTrue(TEXT("one of two landed"), Result.bSuccess);
		TestTrue(TEXT("its damage counts"), Result.DamageDealt > 0);
		TestTrue(TEXT("no error for a partial miss"), Result.ErrorMessage.IsEmpty());
	});

	It("fails an area use when no target took it", [this]()
	{
		if (!Executor) return;
		TestFalse(TEXT("none landed"), Executor->UseItemMultiTarget(User, ITEM_USE_GARNET, {Bare}).bSuccess);
		TestFalse(TEXT("no targets"), Executor->UseItemMultiTarget(User, ITEM_USE_GARNET, {}).bSuccess);
	});

	Describe("from a battle loadout (B-201)", [this]()
	{
		BeforeEach([this]()
		{
			UInventoryComponent *Inventory = ItemUseTestAddComponent<UInventoryComponent>(User);
			Loadout = ItemUseTestAddComponent<ULoadoutComponent>(User);
			FCombatLoadout Battle;
			Battle.ItemSlots.AddDefaulted(ITEM_USE_GARNET_SLOT);
			FItemLoadoutSlot Garnets;
			Garnets.CrystalId = ITEM_USE_GARNET;
			Garnets.Quantity = ITEM_USE_GARNET_CHARGES;
			Battle.ItemSlots.Add(Garnets);
			Inventory->SavedLoadouts = {Battle};
			Inventory->ActiveLoadoutIndex = 0;
			TestTrue(TEXT("ready for battle"), Loadout->PrepareForBattle(Inventory));
			TestEqual(TEXT("charges loaded"), Loadout->GetItemRemainingUses(ITEM_USE_GARNET_SLOT), ITEM_USE_GARNET_CHARGES);
			Actions = TestWorld->GetSubsystem<UActionExecutor>();
			TestNotNull(TEXT("ActionExecutor"), Actions);
		});

		It("spends a charge on a use that landed", [this]()
		{
			if (!Actions) return;
			TestTrue(TEXT("landed"), Actions->ExecuteItem(User, ITEM_USE_GARNET, {Combatant}).bSuccess);
			TestEqual(TEXT("one charge spent"), Loadout->GetItemRemainingUses(ITEM_USE_GARNET_SLOT), ITEM_USE_GARNET_CHARGES - 1);
		});

		It("spends a charge on a use that failed, and still reports the failure", [this]()
		{
			if (!Actions) return;
			const FActionResult Result = Actions->ExecuteItem(User, ITEM_USE_GARNET, {Bare});
			TestFalse(TEXT("reported as failed"), Result.bSuccess);
			TestFalse(TEXT("with its reason"), Result.ErrorMessage.IsEmpty());
			TestEqual(TEXT("the charge is gone"), Loadout->GetItemRemainingUses(ITEM_USE_GARNET_SLOT), ITEM_USE_GARNET_CHARGES - 1);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
