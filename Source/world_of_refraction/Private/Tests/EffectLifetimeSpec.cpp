// EffectLifetimeSpec.cpp
// GAS stage 3 (B-042h, Q-042h): with `wor.Effects.UseGAS 1` the turn tracker owns a stored effect's
// lifetime. Its GE is turn-counted from the stored span, a refresh restarts the tracked span, the
// stored copy takes the tracker's count and goes when the tracker ends its GE, and removing the
// stored effect ends the GE. Permanent effects and UseGAS 0 keep untracked GEs.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "Debug/GAS/WoREffectDebug.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "GAS/WoREffectSpecBuilder.h"
#include "GAS/WoRTurnEffectTracker.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 LIFETIME_BUFF_ID = 4501;
	constexpr int32 LIFETIME_GEAR_ID = 4502;
	constexpr int32 LIFETIME_TURNS = 2;
	constexpr float LIFETIME_BUFF = 20.0f;
}

BEGIN_DEFINE_SPEC(FEffectLifetimeSpec, "WoR.GAS.EffectLifetime",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;
	UWoREffectMirrorComponent *Mirror = nullptr;
	UWoRTurnEffectTracker *Tracker = nullptr;
	UAbilitySystemComponent *ASC = nullptr;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;

	static FActiveSkillEffect Buff()
	{
		return FActiveSkillEffect::CreateBuff(TEXT("Lifetime buff"), LIFETIME_BUFF_ID, ESkillEffectType::DefenseBuff, LIFETIME_BUFF,
											  LIFETIME_TURNS);
	}

	/** The stored buff, or null. */
	const FActiveSkillEffect *StoredBuff()
	{
		Stored = EffectManager->GetActiveEffects(Hero);
		return Stored.FindByPredicate([](const FActiveSkillEffect &Effect) { return Effect.EffectID == LIFETIME_BUFF_ID; });
	}

	/** The buff's GE, found by the identity the stored copy was given (ApplyEffect sets its source key);
	 *  remembered so it can still be looked up once the copy is gone. */
	FActiveGameplayEffectHandle BuffGE()
	{
		if (const FActiveSkillEffect *Copy = StoredBuff())
		{
			BuffIdentity = FWoREffectSpecBuilder::MakeIdentity(*Copy);
		}
		return FWoREffectSpecBuilder::FindByIdentity(*ASC, BuffIdentity);
	}

	void TestMatches(const TCHAR *When)
	{
		TestEqual(FString::Printf(TEXT("GAS matches the stored effects (%s)"), When), UWoREffectDebug::GetEffectMismatchString(Hero),
				  FString());
	}

	TArray<FActiveSkillEffect> Stored;
	FWoREffectIdentity BuffIdentity;
END_DEFINE_SPEC(FEffectLifetimeSpec)

void FEffectLifetimeSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		Mirror = FWoRTestWorld::AddEffectMirror(Hero);
		Tracker = Hero ? Hero->FindComponentByClass<UWoRTurnEffectTracker>() : nullptr;
		ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Hero);
		UWoREffectMirrorComponent::SetReadsFromGAS(true);
		if (!TestNotNull(TEXT("SkillEffectManager"), EffectManager) || !TestNotNull(TEXT("Mirror"), Mirror) ||
			!TestNotNull(TEXT("Tracker"), Tracker) || !TestNotNull(TEXT("AbilitySystemComponent"), ASC))
		{
			EffectManager = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		UWoREffectMirrorComponent::SetReadsFromGAS(false);
		EffectManager = nullptr;
		Mirror = nullptr;
		Tracker = nullptr;
		ASC = nullptr;
		Hero = Foe = nullptr;
		Stored.Reset();
		BuffIdentity = FWoREffectIdentity();
		TestWorld.Reset();
	});

	It("turn-counts a timed effect's GE on the tracker", [this]()
	{
		if (!EffectManager) return;
		EffectManager->ApplyEffect(Hero, Buff(), Foe);
		TestTrue(TEXT("GAS owns the lifetime"), UWoREffectMirrorComponent::OwnsLifetime(Hero));
		TestEqual(TEXT("One tracked GE"), Tracker->NumTracked(), 1);
		TestEqual(TEXT("Tracked for the stored span"), Tracker->GetTurnsLeft(BuffGE()), LIFETIME_TURNS);
		TestMatches(TEXT("applied"));
	});

	It("keeps the stored copy on the tracker's count and drops it when the tracker ends the GE", [this]()
	{
		if (!EffectManager) return;
		EffectManager->ApplyEffect(Hero, Buff(), Foe);
		for (int32 Turn = 1; Turn <= LIFETIME_TURNS; ++Turn)
		{
			EffectManager->ProcessEndOfTurnEffects(Hero);
			const FActiveSkillEffect *Copy = StoredBuff();
			if (!TestNotNull(FString::Printf(TEXT("Stored after turn %d"), Turn), Copy))
			{
				return;
			}
			TestEqual(FString::Printf(TEXT("Copy follows the tracker after turn %d"), Turn), Copy->RemainingTurns,
					  Tracker->GetTurnsLeft(BuffGE()));
			TestEqual(FString::Printf(TEXT("Tracker counted turn %d"), Turn), Tracker->GetTurnsLeft(BuffGE()), LIFETIME_TURNS - Turn);
		}
		EffectManager->ProcessStartOfTurnEffects(Hero);
		TestEqual(TEXT("Tracker ended the GE"), Tracker->NumTracked(), 0);
		TestFalse(TEXT("GE gone"), BuffGE().IsValid());
		TestNull(TEXT("Stored copy gone with it"), StoredBuff());
		TestMatches(TEXT("ended"));
	});

	It("restarts the tracked span on a refresh", [this]()
	{
		if (!EffectManager) return;
		EffectManager->ApplyEffect(Hero, Buff(), Foe);
		EffectManager->ProcessEndOfTurnEffects(Hero);
		TestEqual(TEXT("One turn counted"), Tracker->GetTurnsLeft(BuffGE()), LIFETIME_TURNS - 1);
		EffectManager->ApplyEffect(Hero, Buff(), Foe);
		TestEqual(TEXT("Refreshed on the tracker"), Tracker->GetTurnsLeft(BuffGE()), LIFETIME_TURNS);
		const FActiveSkillEffect *Copy = StoredBuff();
		TestTrue(TEXT("Refreshed in the copy"), Copy && Copy->RemainingTurns == LIFETIME_TURNS);
		TestEqual(TEXT("Still one tracked GE"), Tracker->NumTracked(), 1);
	});

	It("drops the stored copy at the next turn start when GAS ends the GE early", [this]()
	{
		if (!EffectManager) return;
		EffectManager->ApplyEffect(Hero, Buff(), Foe);
		TestTrue(TEXT("Tracker removed the GE"), Tracker->RemoveTracked(BuffGE()));
		TestEqual(TEXT("Reads already see it gone"), EffectManager->GetTotalStatModifier(Hero, ESkillEffectType::DefenseBuff), 0.0f);
		TestFalse(TEXT("Not re-applied by a sync"), BuffGE().IsValid());
		EffectManager->ProcessStartOfTurnEffects(Hero);
		TestNull(TEXT("Stored copy gone"), StoredBuff());
	});

	It("ends the tracked GE when the stored effect is removed", [this]()
	{
		if (!EffectManager) return;
		EffectManager->ApplyEffect(Hero, Buff(), Foe);
		TestTrue(TEXT("Removed"), EffectManager->RemoveEffectByID(Hero, LIFETIME_BUFF_ID));
		TestEqual(TEXT("Not tracked"), Tracker->NumTracked(), 0);
		TestFalse(TEXT("GE gone"), BuffGE().IsValid());
	});

	It("leaves a permanent effect's GE untracked", [this]()
	{
		if (!EffectManager) return;
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreatePersistent(TEXT("Lifetime gear"), LIFETIME_GEAR_ID,
																		   ESkillEffectType::DefenseBuff, LIFETIME_BUFF), Hero);
		TestEqual(TEXT("Mirrored"), Mirror->NumMirrored(), 1);
		TestEqual(TEXT("Not tracked"), Tracker->NumTracked(), 0);
		TestMatches(TEXT("permanent"));
	});

	It("keeps the lifetime on the enum path at UseGAS 0, and moves it to the tracker when switched on", [this]()
	{
		if (!EffectManager) return;
		UWoREffectMirrorComponent::SetReadsFromGAS(false);
		EffectManager->ApplyEffect(Hero, Buff(), Foe);
		TestFalse(TEXT("Enum path owns the lifetime"), UWoREffectMirrorComponent::OwnsLifetime(Hero));
		TestEqual(TEXT("Mirrored"), Mirror->NumMirrored(), 1);
		TestEqual(TEXT("Not tracked"), Tracker->NumTracked(), 0);

		UWoREffectMirrorComponent::SetReadsFromGAS(true);
		Mirror->SyncMirror();
		TestEqual(TEXT("Tracked once switched on"), Tracker->NumTracked(), 1);
		TestEqual(TEXT("Still one GE"), Mirror->NumMirrored(), 1);
		TestEqual(TEXT("With the stored span"), Tracker->GetTurnsLeft(BuffGE()), LIFETIME_TURNS);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
