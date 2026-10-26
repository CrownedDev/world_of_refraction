// ItemUseSpec.cpp
// B-060: item use reports whether it landed. A use that fails (here a Garnet thrown at an actor
// with no character data) reports failure, so the caller keeps the charge; an area use counts as
// landed when at least one target took it.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Inventory/ItemExecutor.h"

namespace
{
	const FCrystalId ITEM_USE_GARNET(ECrystalType::Garnet, EItemTier::F_Tier);
}

BEGIN_DEFINE_SPEC(FItemUseSpec, "WoR.Items.Use",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UItemExecutor *Executor = nullptr;
	AActor *User = nullptr;
	AActor *Combatant = nullptr;
	/** No UCharacterDataComponent: Garnet's damage handler refuses it. */
	AActor *Bare = nullptr;
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
}

#endif // WITH_DEV_AUTOMATION_TESTS
