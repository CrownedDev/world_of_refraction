// GASEffectReadsSpec.cpp
// GAS stage 3 (B-042ye, Q-042x): the per-effect reads go through GAS: the held list (the character panel's effect
// list, the buff / debuff counts) comes from the mirrored GEs with the tracker's turns left, and the AI sizes a DOT's
// tick from its GE's tick spec. They give the stored effects' list, counts, ticks and AI choices. B-042g2: the
// UseGAS 0 pass went with the switch.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AI/AIDecisionManager.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "GAS/WoRTurnEffectTracker.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 READS_DOT_ID = 4621;
	constexpr int32 READS_DEBUFF_ID = 4622;
	constexpr int32 READS_HOT_ID = 4623;
	constexpr int32 READS_TURNS = 3;
	constexpr float READS_DOT_VALUE = 6.0f;
	constexpr float READS_DEBUFF_VALUE = 10.0f;
	constexpr float READS_HOT_VALUE = 4.0f;
	constexpr int32 READS_EXPECTED_DEBUFFS = 2; // the burn and the Defense debuff
	constexpr int32 READS_EXPECTED_BUFFS = 1;	// the heal-over-time
}

BEGIN_DEFINE_SPEC(FGASEffectReadsSpec, "WoR.GAS.EffectReads",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;
	UAIDecisionManager *AI = nullptr;
	UWoREffectMirrorComponent *Mirror = nullptr;
	UCharacterDataComponent *HeroStats = nullptr;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;

	FActiveSkillEffect Burn() const
	{
		return FActiveSkillEffect::CreateDOT(TEXT("Reads burn"), READS_DOT_ID, READS_DOT_VALUE, READS_TURNS, ESpellElement::Fire);
	}

	/** A burn, a Defense debuff and a heal-over-time from the foe, then one holder turn. */
	void ApplyMix()
	{
		EffectManager->ApplyEffect(Hero, Burn(), Foe);
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Reads debuff"), READS_DEBUFF_ID, ESkillEffectType::DefenseDebuff,
																	   READS_DEBUFF_VALUE, READS_TURNS),
								   Foe);
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Reads heal"), READS_HOT_ID, ESkillEffectType::HealthRestore,
																	   READS_HOT_VALUE, READS_TURNS),
								   Foe);
		EffectManager->ProcessStartOfTurnEffects(Hero);
		EffectManager->ProcessEndOfTurnEffects(Hero);
	}

	const FActiveSkillEffect *FindHeld(const TArray<FActiveSkillEffect> &Held, int32 EffectID) const
	{
		return Held.FindByPredicate([EffectID](const FActiveSkillEffect &Effect) { return Effect.EffectID == EffectID; });
	}
END_DEFINE_SPEC(FGASEffectReadsSpec)

void FGASEffectReadsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		AI = TestWorld->GetSubsystem<UAIDecisionManager>();
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		Mirror = FWoRTestWorld::AddEffectMirror(Hero);
		HeroStats = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("SkillEffectManager"), EffectManager) || !TestNotNull(TEXT("AI"), AI) ||
			!TestNotNull(TEXT("Mirror"), Mirror) || !TestNotNull(TEXT("Hero stats"), HeroStats))
		{
			EffectManager = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		EffectManager = nullptr;
		AI = nullptr;
		Mirror = nullptr;
		HeroStats = nullptr;
		Hero = nullptr;
		Foe = nullptr;
		TestWorld.Reset();
	});

	It("lists the held effects with the stored turns left", [this]()
	{
		if (!EffectManager) return;
		ApplyMix();
		const TArray<FActiveSkillEffect> Stored = EffectManager->GetActiveEffects(Hero);
		const TArray<FActiveSkillEffect> Held = EffectManager->GetHeldEffects(Hero);
		TestEqual(TEXT("Same number of effects"), Held.Num(), Stored.Num());
		for (const FActiveSkillEffect &Effect : Stored)
		{
			const FActiveSkillEffect *Read = FindHeld(Held, Effect.EffectID);
			if (TestNotNull(*FString::Printf(TEXT("%s is held"), *Effect.EffectName), Read))
			{
				TestEqual(*FString::Printf(TEXT("%s turns left"), *Effect.EffectName), Read->RemainingTurns, Effect.RemainingTurns);
			}
		}
	});

	It("leaves out a burn GAS ended before its stored copy goes", [this]()
	{
		if (!EffectManager) return;
		EffectManager->ApplyEffect(Hero, Burn(), Foe);
		UWoRTurnEffectTracker *Tracker = Hero->FindComponentByClass<UWoRTurnEffectTracker>();
		if (!TestNotNull(TEXT("Tracker"), Tracker)) return;
		// The tracker's turns alone (it counts at the end, ends at the start): GAS ends the burn's GE, the
		// stored copy waits for the effect manager's release.
		for (int32 Turn = 0; Turn <= READS_TURNS; ++Turn)
		{
			Tracker->OnHolderTurnStart();
			Tracker->OnHolderTurnEnd();
		}
		TestNotNull(TEXT("The stored copy is still there"), FindHeld(EffectManager->GetActiveEffects(Hero), READS_DOT_ID));
		const FActiveSkillEffect *Held = FindHeld(EffectManager->GetHeldEffects(Hero), READS_DOT_ID);
		TestNull(TEXT("GAS ended it: not held"), Held);
	});

	It("counts the same buffs and debuffs", [this]()
	{
		if (!EffectManager) return;
		ApplyMix();
		TestEqual(TEXT("Debuffs"), EffectManager->GetDebuffCount(Hero), READS_EXPECTED_DEBUFFS);
		TestEqual(TEXT("Buffs"), EffectManager->GetBuffCount(Hero), READS_EXPECTED_BUFFS);
	});

	It("previews a burn's tick as the enum path's", [this]()
	{
		if (!EffectManager) return;
		EffectManager->ApplyEffect(Hero, Burn(), Foe);
		const TArray<FActiveSkillEffect> Held = EffectManager->GetHeldEffects(Hero);
		const FActiveSkillEffect *Held_Burn = FindHeld(Held, READS_DOT_ID);
		if (!TestNotNull(TEXT("The burn is held"), Held_Burn)) return;
		const int32 EnumTick = USkillEffectManager::GetDotTickDamage(Hero, *Held_Burn);
		TestTrue(TEXT("The burn hits"), EnumTick > 0);
		TestEqual(TEXT("Same tick"), EffectManager->PreviewDotTickDamage(Hero, *Held_Burn), EnumTick);
		TestEqual(TEXT("From the GE's tick spec"), Mirror->PreviewDotTick(*Held_Burn), EnumTick);
	});

	It("re-syncs the mirror for a stat read only when the effects changed (B-261)", [this]()
	{
		if (!EffectManager) return;
		ApplyMix();
		const float Before = EffectManager->GetTotalStatModifier(Hero, ESkillEffectType::DefenseDebuff);
		const int32 Syncs = Mirror->GetSyncCount();
		TestEqual(TEXT("Same read"), EffectManager->GetTotalStatModifier(Hero, ESkillEffectType::DefenseDebuff), Before);
		TestEqual(TEXT("Nothing changed: no sync"), Mirror->GetSyncCount(), Syncs);
		// ForgetEnded changes what the sync reads with no stored change and no event: the next read must sync.
		Mirror->ForgetEnded(Burn());
		TestEqual(TEXT("Same read after"), EffectManager->GetTotalStatModifier(Hero, ESkillEffectType::DefenseDebuff), Before);
		TestTrue(TEXT("The mirror synced"), Mirror->GetSyncCount() > Syncs);
	});

	It("makes the same AI choices on a lethal burn", [this]()
	{
		if (!EffectManager) return;
		EffectManager->ApplyEffect(Hero, Burn(), Foe);
		const int32 Tick = USkillEffectManager::GetDotTickDamage(Hero, Burn());
		HeroStats->ServerSetHP(Tick);
		TestTrue(TEXT("Dangerous: the burn kills over its turns"), AI->HasDangerousDebuff(Hero));
		TestEqual(TEXT("One tick is lethal"), AI->GetLethalDoTPerTick(Hero), static_cast<float>(Tick));
	});
}

#endif
