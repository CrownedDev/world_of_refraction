// CombatEventsSpec.cpp
// GAS stage 3 (B-042d, Q-044j): the effect manager sends Event.Combat.* GameplayEvents to the
// actor they happen to (hit, defense resolved, turn start, turn end), with the right instigator
// and magnitude, and an actor with no ability system component gets nothing.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Defense/DefenseSystem.h"
#include "GAS/WoRCombatEvents.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Tags/WoRGameplayTags.h"

namespace
{
	constexpr int32 EVENTS_HIT_DAMAGE = 12;
	constexpr float EVENTS_ATTACK_COST = 4.0f;
}

BEGIN_DEFINE_SPEC(FCombatEventsSpec, "WoR.GAS.CombatEvents",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;
	UAbilitySystemComponent *ASC = nullptr;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;
	TArray<FGameplayEventData> Received;

	void Listen(const FGameplayTag &Event)
	{
		ASC->GenericGameplayEventCallbacks.FindOrAdd(Event).AddLambda(
			[this](const FGameplayEventData *Payload) { Received.Add(*Payload); });
	}

	/** The one event received, checked against what was expected. */
	void TestReceived(const FGameplayTag &Event, const AActor *Instigator, float Magnitude)
	{
		if (!TestEqual(FString::Printf(TEXT("One %s"), *Event.ToString()), Received.Num(), 1))
		{
			return;
		}
		TestEqual(TEXT("Tag"), Received[0].EventTag, Event);
		TestTrue(TEXT("Target is the hero"), Received[0].Target.Get() == Hero);
		TestTrue(TEXT("Instigator"), Received[0].Instigator.Get() == Instigator);
		TestEqual(TEXT("Magnitude"), Received[0].EventMagnitude, Magnitude);
	}
END_DEFINE_SPEC(FCombatEventsSpec)

void FCombatEventsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		if (Hero)
		{
			ASC = FWoRTestWorld::AddComponent<UAbilitySystemComponent>(Hero);
			ASC->InitAbilityActorInfo(Hero, Hero);
		}
		Received.Reset();
		if (!TestNotNull(TEXT("SkillEffectManager"), EffectManager) || !TestNotNull(TEXT("AbilitySystemComponent"), ASC))
		{
			EffectManager = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		Received.Reset();
		EffectManager = nullptr;
		ASC = nullptr;
		Hero = nullptr;
		Foe = nullptr;
		TestWorld.Reset();
	});

	It("sends Event.Combat.Hit to the damaged actor", [this]()
	{
		if (!EffectManager) return;
		UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
		if (!TestNotNull(TEXT("ActionExecutor"), Executor)) return;
		Listen(WoRTags::Event_Combat_Hit);
		Executor->OnDamageDealt.Broadcast(Foe, Hero, EVENTS_HIT_DAMAGE, false);
		TestReceived(WoRTags::Event_Combat_Hit, Foe, EVENTS_HIT_DAMAGE);
	});

	It("sends Event.Combat.DefenseResolved to the defender", [this]()
	{
		if (!EffectManager) return;
		UDefenseSystem *Defense = TestWorld->GetSubsystem<UDefenseSystem>();
		if (!TestNotNull(TEXT("DefenseSystem"), Defense)) return;
		Listen(WoRTags::Event_Combat_DefenseResolved);
		Defense->OnDefenseResolved.Broadcast(Hero, Foe, EDefenseType::Block, false, 0, EVENTS_ATTACK_COST);
		TestReceived(WoRTags::Event_Combat_DefenseResolved, Foe, EVENTS_ATTACK_COST);
	});

	It("sends Event.Combat.TurnStart and TurnEnd to the actor whose turn it is", [this]()
	{
		if (!EffectManager) return;
		Listen(WoRTags::Event_Combat_TurnStart);
		EffectManager->ProcessStartOfTurnEffects(Hero);
		TestReceived(WoRTags::Event_Combat_TurnStart, Hero, 0.0f);

		Received.Reset();
		Listen(WoRTags::Event_Combat_TurnEnd);
		EffectManager->ProcessEndOfTurnEffects(Hero);
		TestReceived(WoRTags::Event_Combat_TurnEnd, Hero, 0.0f);
	});

	It("sends nothing to an actor with no ability system component", [this]()
	{
		if (!EffectManager) return;
		TestFalse(TEXT("Not sent"), WoRCombatEvents::Send(Foe, WoRTags::Event_Combat_Hit, Hero, EVENTS_HIT_DAMAGE));
		EffectManager->ProcessStartOfTurnEffects(Foe); // no error from an ASC-less actor
		TestTrue(TEXT("Sent to the hero"), WoRCombatEvents::Send(Hero, WoRTags::Event_Combat_Hit, Foe, EVENTS_HIT_DAMAGE));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
