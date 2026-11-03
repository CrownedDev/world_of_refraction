// GASClientReadsSpec.cpp
// B-318 (Q-042ye, Q-318): co-op guests read effects from GAS. At UseGAS 1 the server's effect mirror keeps its GAS
// read of the held effects (turns left from the tracker) in a replicated snapshot; an actor without authority (a
// simulated proxy, as a guest sees another player's character) reads that snapshot, not its stored list, so it lists
// the same effects and turns as the server. At UseGAS 0 nothing changes.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 CLIENT_READS_DOT_ID = 31801;
	constexpr int32 CLIENT_READS_DEBUFF_ID = 31802;
	constexpr int32 CLIENT_READS_TURNS = 3;
	constexpr float CLIENT_READS_DOT_VALUE = 6.0f;
	constexpr float CLIENT_READS_DEBUFF_VALUE = 10.0f;
}

BEGIN_DEFINE_SPEC(FGASClientReadsSpec, "WoR.GAS.ClientReads",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;
	UWoREffectMirrorComponent *Mirror = nullptr;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;

	/** A burn and a Defense debuff from the foe, then one holder turn (the tracker counts one down). */
	void ApplyMix()
	{
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateDOT(TEXT("Client burn"), CLIENT_READS_DOT_ID, CLIENT_READS_DOT_VALUE,
																	   CLIENT_READS_TURNS, ESpellElement::Fire),
								   Foe);
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Client debuff"), CLIENT_READS_DEBUFF_ID, ESkillEffectType::DefenseDebuff,
																	   CLIENT_READS_DEBUFF_VALUE, CLIENT_READS_TURNS),
								   Foe);
		EffectManager->ProcessStartOfTurnEffects(Hero);
		EffectManager->ProcessEndOfTurnEffects(Hero);
	}

	/** Same effects (by ID) with the same turns left. */
	void TestSameHeld(const TCHAR *What, const TArray<FActiveSkillEffect> &Expected, const TArray<FActiveSkillEffect> &Actual)
	{
		TestEqual(FString::Printf(TEXT("%s: same number of effects"), What), Actual.Num(), Expected.Num());
		for (const FActiveSkillEffect &Effect : Expected)
		{
			const FActiveSkillEffect *Match = Actual.FindByPredicate([&Effect](const FActiveSkillEffect &Other) { return Other.EffectID == Effect.EffectID; });
			if (TestNotNull(FString::Printf(TEXT("%s: %s listed"), What, *Effect.EffectName), Match))
			{
				TestEqual(FString::Printf(TEXT("%s: %s turns left"), What, *Effect.EffectName), Match->RemainingTurns, Effect.RemainingTurns);
			}
		}
	}
END_DEFINE_SPEC(FGASClientReadsSpec)

void FGASClientReadsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		Mirror = FWoRTestWorld::AddEffectMirror(Hero);
		if (!TestNotNull(TEXT("SkillEffectManager"), EffectManager) || !TestNotNull(TEXT("Mirror"), Mirror))
		{
			EffectManager = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		if (Hero)
		{
			Hero->SetRole(ROLE_Authority);
		}
		UWoREffectMirrorComponent::SetReadsFromGAS(false);
		EffectManager = nullptr;
		Mirror = nullptr;
		Hero = nullptr;
		Foe = nullptr;
		TestWorld.Reset();
	});

	It("keeps the server's GAS read in the replicated snapshot at UseGAS 1", [this]()
	{
		if (!EffectManager) return;
		UWoREffectMirrorComponent::SetReadsFromGAS(true);
		ApplyMix();
		// Read before anything could resync: the effect events' syncs alone must have filled it.
		const TArray<FActiveSkillEffect> Snapshot = Mirror->GetHeldSnapshot();
		TestEqual(TEXT("both effects in the snapshot"), Snapshot.Num(), 2);
		for (const FActiveSkillEffect &Held : Snapshot)
		{
			TestTrue(FString::Printf(TEXT("%s: a holder turn counted, not over"), *Held.EffectName),
					 Held.RemainingTurns > 0 && Held.RemainingTurns <= CLIENT_READS_TURNS);
		}
		const TArray<FActiveSkillEffect> ServerHeld = EffectManager->GetHeldEffects(Hero);
		TestSameHeld(TEXT("snapshot"), ServerHeld, Snapshot);
	});

	It("gives an actor without authority the server's list and turns, not its stored list, at UseGAS 1", [this]()
	{
		if (!EffectManager) return;
		UWoREffectMirrorComponent::SetReadsFromGAS(true);
		ApplyMix();
		const TArray<FActiveSkillEffect> ServerHeld = EffectManager->GetHeldEffects(Hero);
		Hero->SetRole(ROLE_SimulatedProxy); // as a co-op guest sees another player's character
		TestSameHeld(TEXT("client read, empty stored list"), ServerHeld, Mirror->ReadHeldEffects(TArray<FActiveSkillEffect>()));
		TestSameHeld(TEXT("client read through the effect manager"), ServerHeld, EffectManager->GetHeldEffects(Hero));
	});

	It("a client at UseGAS 0 still reads the server's snapshot when the server is at 1", [this]()
	{
		if (!EffectManager) return;
		UWoREffectMirrorComponent::SetReadsFromGAS(true);
		ApplyMix();
		const TArray<FActiveSkillEffect> ServerHeld = EffectManager->GetHeldEffects(Hero);
		Hero->SetRole(ROLE_SimulatedProxy);
		UWoREffectMirrorComponent::SetReadsFromGAS(false); // this client's own switch; the server's snapshot says 1
		TestSameHeld(TEXT("client follows the server through the effect manager"), ServerHeld, EffectManager->GetHeldEffects(Hero));
	});

	It("changes nothing at UseGAS 0: no snapshot, and a client reads its stored list", [this]()
	{
		if (!EffectManager) return;
		UWoREffectMirrorComponent::SetReadsFromGAS(false);
		ApplyMix();
		TestEqual(TEXT("no snapshot"), Mirror->GetHeldSnapshot().Num(), 0);
		const TArray<FActiveSkillEffect> Stored = EffectManager->GetActiveEffects(Hero);
		Hero->SetRole(ROLE_SimulatedProxy);
		TestSameHeld(TEXT("client read"), Stored, Mirror->ReadHeldEffects(Stored));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
