// StatusBarStateSpec.cpp
// B-230 part 1: a combatant's status bar is copied from the server's buildup manager into its replicated
// UStatusBarStateComponent, for its owner only. (The client side, OnRep, needs a second machine: docs/PIE/B-230.md.)

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/StatusBarStateComponent.h"
#include "Skills/Effects/StatusBuildupManager.h"

namespace
{
	constexpr float BAR_TEST_CURRENT = 30.0f;
	constexpr float BAR_TEST_MAX = 100.0f;
	constexpr float BAR_TEST_TOLERANCE = 0.01f;
}

BEGIN_DEFINE_SPEC(FStatusBarStateSpec, "WoR.Combat.StatusBarState", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FStatusBarStateSpec)

void FStatusBarStateSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("stores what the server's buildup manager reports for its owner only", [this]()
	{
		AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		UStatusBuildupManager *Manager = TestWorld->GetSubsystem<UStatusBuildupManager>();
		if (!TestTrue(TEXT("setup"), Hero && Foe && Manager)) return;
		UStatusBarStateComponent *Bar = Hero->FindComponentByClass<UStatusBarStateComponent>();
		if (!Bar)
		{
			Bar = FWoRTestWorld::AddComponent<UStatusBarStateComponent>(Hero); // registered after BeginPlay: binds now
		}
		if (!TestNotNull(TEXT("bar component"), Bar)) return;

		Manager->OnStatusBuildupChanged.Broadcast(Hero, BAR_TEST_CURRENT, BAR_TEST_MAX, ESpellElement::Fire, Foe);
		TestEqual(TEXT("current"), Bar->GetBar().Current, BAR_TEST_CURRENT, BAR_TEST_TOLERANCE);
		TestEqual(TEXT("max"), Bar->GetBar().Max, BAR_TEST_MAX, BAR_TEST_TOLERANCE);
		TestTrue(TEXT("element and source"), Bar->GetBar().PendingElement == ESpellElement::Fire && Bar->GetBar().Source == Foe);

		Manager->OnStatusBuildupChanged.Broadcast(Foe, 0.0f, BAR_TEST_MAX, ESpellElement::Water, Hero);
		TestEqual(TEXT("another combatant's bar leaves it alone"), Bar->GetBar().Current, BAR_TEST_CURRENT, BAR_TEST_TOLERANCE);
		TestTrue(TEXT("the debug line names it"), Bar->GetDebugString().Contains(TEXT("30")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
