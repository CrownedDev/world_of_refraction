// TurnEffectTrackerSpec.cpp
// GAS stage 3 (B-042): the native Stun GE grants Status.Stun, and UWoRTurnEffectTracker counts the
// holder's turns for it by the one rule (FTurnSpan, D-051): it ends as the holder's turn after the
// N counted turns starts, a landing turn counts, and a GE removed elsewhere leaves the tracker.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "GameplayEffect.h"
#include "GAS/WoRGameplayEffects.h"
#include "GAS/WoRTurnEffectTracker.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Tags/WoRGameplayTags.h"

namespace
{
	constexpr int32 TRACKER_ONE_TURN = 1;
	constexpr int32 TRACKER_TWO_TURNS = 2;
}

BEGIN_DEFINE_SPEC(FTurnEffectTrackerSpec, "WoR.GAS.TurnTracker",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UAbilitySystemComponent *ASC = nullptr;
	UWoRTurnEffectTracker *Tracker = nullptr;
	AActor *Hero = nullptr;

	bool IsStunned() const { return ASC && ASC->HasMatchingGameplayTag(WoRTags::Status_Stun); }

	/** One full turn of the holder: its start, then its end. */
	void HolderTakesTurn()
	{
		Tracker->OnHolderTurnStart();
		Tracker->OnHolderTurnEnd();
	}
END_DEFINE_SPEC(FTurnEffectTrackerSpec)

void FTurnEffectTrackerSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (Hero)
		{
			ASC = FWoRTestWorld::AddComponent<UAbilitySystemComponent>(Hero);
			ASC->InitAbilityActorInfo(Hero, Hero);
			Tracker = FWoRTestWorld::AddComponent<UWoRTurnEffectTracker>(Hero);
		}
		TestNotNull(TEXT("AbilitySystemComponent"), ASC);
		TestNotNull(TEXT("Tracker"), Tracker);
	});

	AfterEach([this]()
	{
		ASC = nullptr;
		Tracker = nullptr;
		Hero = nullptr;
		TestWorld.Reset();
	});

	It("grants Status.Stun while the Stun GE is on, and drops it when removed", [this]()
	{
		if (!Tracker)
		{
			return;
		}
		const FActiveGameplayEffectHandle Handle = Tracker->ApplyForHolderTurns(UWoRGE_Stun::StaticClass(), TRACKER_ONE_TURN, Hero);
		TestTrue(TEXT("Applied"), Handle.IsValid());
		TestTrue(TEXT("Stunned"), IsStunned());
		TestTrue(TEXT("Removed"), Tracker->RemoveTracked(Handle));
		TestFalse(TEXT("No longer stunned"), IsStunned());
		TestEqual(TEXT("Nothing tracked"), Tracker->NumTracked(), 0);
	});

	It("ends a 1-turn effect landed off-turn as the holder's turn after the next one starts", [this]()
	{
		if (!Tracker)
		{
			return;
		}
		Tracker->ApplyForHolderTurns(UWoRGE_Stun::StaticClass(), TRACKER_ONE_TURN, Hero);
		HolderTakesTurn();
		TestTrue(TEXT("Held through the holder's next turn"), IsStunned());
		Tracker->OnHolderTurnStart();
		TestFalse(TEXT("Gone as the following turn starts"), IsStunned());
	});

	It("counts the landing turn when the effect lands during the holder's own turn", [this]()
	{
		if (!Tracker)
		{
			return;
		}
		Tracker->OnHolderTurnStart();
		const FActiveGameplayEffectHandle Handle = Tracker->ApplyForHolderTurns(UWoRGE_Stun::StaticClass(), TRACKER_TWO_TURNS, Hero);
		Tracker->OnHolderTurnEnd();
		TestEqual(TEXT("The landing turn counted"), Tracker->GetTurnsLeft(Handle), TRACKER_TWO_TURNS - 1);
		HolderTakesTurn();
		TestTrue(TEXT("Still held after the second counted turn ends"), IsStunned());
		Tracker->OnHolderTurnStart();
		TestFalse(TEXT("Gone as the next turn starts"), IsStunned());
		TestEqual(TEXT("Untracked"), Tracker->GetTurnsLeft(Handle), -1);
	});

	It("forgets a tracked effect removed some other way", [this]()
	{
		if (!Tracker || !ASC)
		{
			return;
		}
		const FActiveGameplayEffectHandle Handle = Tracker->ApplyForHolderTurns(UWoRGE_Stun::StaticClass(), TRACKER_TWO_TURNS, Hero);
		ASC->RemoveActiveGameplayEffect(Handle);
		TestEqual(TEXT("Nothing tracked"), Tracker->NumTracked(), 0);
		TestFalse(TEXT("No longer stunned"), IsStunned());
	});

	It("is driven by the effect manager's turn start and end", [this]()
	{
		USkillEffectManager *EffectManager = TestWorld ? TestWorld->GetSubsystem<USkillEffectManager>() : nullptr;
		if (!Tracker || !EffectManager)
		{
			return;
		}
		Tracker->ApplyForHolderTurns(UWoRGE_Stun::StaticClass(), TRACKER_ONE_TURN, Hero);
		EffectManager->ProcessStartOfTurnEffects(Hero);
		EffectManager->ProcessEndOfTurnEffects(Hero);
		TestTrue(TEXT("Held through the holder's next turn"), IsStunned());
		EffectManager->ProcessStartOfTurnEffects(Hero);
		TestFalse(TEXT("Gone as the following turn starts"), IsStunned());
	});

	// The real path: ACombatCharacter's own tracker and ASC, deferred spawn as ABattleGameMode does.
	It("works on a combat character's own tracker and ability system", [this]()
	{
		UWorld *World = TestWorld->GetWorld();
		if (!World)
		{
			return;
		}
		ACombatCharacter *Fighter = World->SpawnActorDeferred<ACombatCharacter>(ACombatCharacter::StaticClass(), FTransform::Identity, nullptr, nullptr,
																				 ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!TestNotNull(TEXT("Combat character spawned"), Fighter))
		{
			return;
		}
		Fighter->CharacterDataComponent->CharacterData = FWoRTestWorld::NewCharacterData();
		UGameplayStatics::FinishSpawningActor(Fighter, FTransform::Identity);

		UWoRTurnEffectTracker *OwnTracker = Fighter->TurnEffectTracker;
		UAbilitySystemComponent *OwnASC = Fighter->GetAbilitySystemComponent();
		if (!TestNotNull(TEXT("Own tracker"), OwnTracker) || !TestNotNull(TEXT("Own ASC"), OwnASC))
		{
			return;
		}
		const FActiveGameplayEffectHandle Handle = OwnTracker->ApplyForHolderTurns(UWoRGE_Stun::StaticClass(), TRACKER_ONE_TURN, Fighter);
		TestTrue(TEXT("Stunned"), OwnASC->HasMatchingGameplayTag(WoRTags::Status_Stun));
		OwnASC->RemoveActiveGameplayEffect(Handle);
		TestEqual(TEXT("The tracker heard the removal"), OwnTracker->NumTracked(), 0);
		Fighter->Destroy();
	});

	It("refuses a span under 1 turn and a GE that isn't Infinite", [this]()
	{
		if (!Tracker)
		{
			return;
		}
		TestFalse(TEXT("0 turns refused"), Tracker->ApplyForHolderTurns(UWoRGE_Stun::StaticClass(), 0, Hero).IsValid());
		TestFalse(TEXT("Instant GE refused"), Tracker->ApplyForHolderTurns(UGameplayEffect::StaticClass(), TRACKER_ONE_TURN, Hero).IsValid());
		TestEqual(TEXT("Nothing tracked"), Tracker->NumTracked(), 0);
		TestFalse(TEXT("Not stunned"), IsStunned());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
