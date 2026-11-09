// GASOtherPayoutsSpec.cpp
// GAS stage 3 (B-042yd): the other payouts (energy restore / drain, HP drain, self-damage, the percent restores)
// are paid by their GE's tick spec (UWoRTickExecution), and the Vitals set hands the Health / Energy change to the
// game's HP / EP path; an instant pays through a tick spec of its own. B-042g2: the UseGAS 0 pass (the enum path
// paying) went with the switch.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/CombatConstants.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "GAS/WoRGameplayEffects.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 OTHER_PAYOUT_ID = 4611;
	constexpr int32 OTHER_PAYOUT_HEAL_BLOCK_ID = 4612;
	constexpr int32 OTHER_PAYOUT_TURNS = 2;
	constexpr int32 OTHER_PAYOUT_INSTANT = 0;
	/** Prepare leaves the hero at half HP and half EP. */
	constexpr int32 OTHER_PAYOUT_HALF = 2;
	constexpr float OTHER_PAYOUT_FLAT = 3.0f;
	constexpr float OTHER_PAYOUT_PERCENT = 10.0f;

	/** One payout type and what one tick should change, worked out from the enum path's rule. */
	struct FOtherPayoutCase
	{
		ESkillEffectType Type;
		float Value;
		bool bEnergy;
		/** Signed change of one tick, given the holder's max pool. */
		int32 (*ExpectedTick)(int32 MaxPool, float Value);
	};

	int32 FlatGain(int32, float Value) { return FMath::RoundToInt(Value); }
	int32 FlatLoss(int32, float Value) { return -FMath::RoundToInt(Value); }
	int32 ShareGain(int32 MaxPool, float Value) { return FMath::RoundToInt(MaxPool * Value / CombatConstants::STAT_PERCENT_DIVISOR); }

	const FOtherPayoutCase OTHER_PAYOUT_CASES[] = {
		{ESkillEffectType::EnergyRestore, OTHER_PAYOUT_FLAT, true, &FlatGain},
		{ESkillEffectType::EnergyDrain, OTHER_PAYOUT_FLAT, true, &FlatLoss},
		{ESkillEffectType::DrainEnergy, OTHER_PAYOUT_FLAT, true, &FlatLoss},
		{ESkillEffectType::RestoreEnergyPercent, OTHER_PAYOUT_PERCENT, true, &ShareGain},
		{ESkillEffectType::DrainHP, OTHER_PAYOUT_FLAT, false, &FlatLoss},
		{ESkillEffectType::SelfDamage, OTHER_PAYOUT_FLAT, false, &FlatLoss},
		{ESkillEffectType::RestoreHPPercent, OTHER_PAYOUT_PERCENT, false, &ShareGain},
	};
}

BEGIN_DEFINE_SPEC(FGASOtherPayoutsSpec, "WoR.GAS.OtherPayouts",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;
	UAbilitySystemComponent *ASC = nullptr;
	UCharacterDataComponent *HeroStats = nullptr;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;
	int32 TicksThroughGAS = 0;

	/** One holder turn: its start (the payouts) and its end. */
	void HolderTurn()
	{
		EffectManager->ProcessStartOfTurnEffects(Hero);
		EffectManager->ProcessEndOfTurnEffects(Hero);
	}

	/** Room both ways: half the HP and half the EP. */
	void Prepare()
	{
		HeroStats->ServerSetHP(HeroStats->MaxHP / OTHER_PAYOUT_HALF);
		HeroStats->ServerSetEP(HeroStats->MaxEP / OTHER_PAYOUT_HALF);
	}

	/** On the GAS path the attribute a payout changed holds the game's pool (the pools stay the truth). The
	 *  test world has no attribute sync, so only the changed one is set. */
	void TestAttributeHoldsPool(bool bEnergy)
	{
		if (bEnergy)
		{
			TestEqual(TEXT("Energy holds the EP"),
					  FMath::RoundToInt(ASC->GetNumericAttribute(UWoRAttributeSet_Vitals::GetEnergyAttribute())), HeroStats->CurrentEP);
		}
		else
		{
			TestEqual(TEXT("Health holds the HP"),
					  FMath::RoundToInt(ASC->GetNumericAttribute(UWoRAttributeSet_Vitals::GetHealthAttribute())), HeroStats->CurrentHP);
		}
	}
END_DEFINE_SPEC(FGASOtherPayoutsSpec)

void FGASOtherPayoutsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		const UWoREffectMirrorComponent *Mirror = FWoRTestWorld::AddEffectMirror(Hero);
		ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Hero);
		HeroStats = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		TicksThroughGAS = 0;
		if (!TestNotNull(TEXT("SkillEffectManager"), EffectManager) || !TestNotNull(TEXT("Mirror"), Mirror) ||
			!TestNotNull(TEXT("AbilitySystemComponent"), ASC) || !TestNotNull(TEXT("Hero stats"), HeroStats) ||
			!TestTrue(TEXT("Half the EP pool takes every flat payout"),
					  HeroStats->MaxEP / OTHER_PAYOUT_HALF >= OTHER_PAYOUT_TURNS * FMath::RoundToInt(OTHER_PAYOUT_FLAT)))
		{
			EffectManager = nullptr; // each case returns early
			return;
		}
		ASC->OnGameplayEffectAppliedDelegateToSelf.AddLambda(
			[this](UAbilitySystemComponent *, const FGameplayEffectSpec &Spec, FActiveGameplayEffectHandle)
			{
				if (Spec.Def && Spec.Def->IsA<UWoRGE_HealthTick>())
				{
					++TicksThroughGAS;
				}
			});
	});

	AfterEach([this]()
	{
		EffectManager = nullptr;
		ASC = nullptr;
		HeroStats = nullptr;
		Hero = nullptr;
		Foe = nullptr;
		TestWorld.Reset();
	});

	for (const FOtherPayoutCase &Case : OTHER_PAYOUT_CASES)
	{
		const FString TypeName = UEnum::GetValueAsString(Case.Type);
		It(FString::Printf(TEXT("pays %s each turn, on GAS"), *TypeName), [this, Case, TypeName]()
		{
			if (!EffectManager) return;
			Prepare();
			const int32 StartHP = HeroStats->CurrentHP;
			const int32 StartEP = HeroStats->CurrentEP;
			const int32 Tick = Case.ExpectedTick(Case.bEnergy ? HeroStats->MaxEP : HeroStats->MaxHP, Case.Value);
			EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TypeName, OTHER_PAYOUT_ID, Case.Type, Case.Value,
																		   OTHER_PAYOUT_TURNS),
									   Foe);
			for (int32 Turn = 0; Turn < OTHER_PAYOUT_TURNS; ++Turn)
			{
				HolderTurn();
			}
			HolderTurn(); // expired: no more payouts
			TestTrue(TEXT("The payout moves something"), Tick != 0);
			TestEqual(TEXT("HP after N payouts"), HeroStats->CurrentHP, StartHP + (Case.bEnergy ? 0 : OTHER_PAYOUT_TURNS * Tick));
			TestEqual(TEXT("EP after N payouts"), HeroStats->CurrentEP, StartEP + (Case.bEnergy ? OTHER_PAYOUT_TURNS * Tick : 0));
			TestEqual(TEXT("Ticks through GAS"), TicksThroughGAS, OTHER_PAYOUT_TURNS);
			TestAttributeHoldsPool(Case.bEnergy);
		});
	}

	It("pays an instant self-damage once, through a tick spec of its own", [this]()
	{
		if (!EffectManager) return;
		const int32 StartHP = HeroStats->CurrentHP;
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Instant self-damage"), OTHER_PAYOUT_ID,
																	   ESkillEffectType::SelfDamage, OTHER_PAYOUT_FLAT,
																	   OTHER_PAYOUT_INSTANT),
								   Foe);
		TestEqual(TEXT("Hit once"), HeroStats->CurrentHP, StartHP - FMath::RoundToInt(OTHER_PAYOUT_FLAT));
		TestEqual(TEXT("Ticks through GAS"), TicksThroughGAS, 1);
		TestAttributeHoldsPool(false);
	});

	It("skips a percent HP restore while heal-blocked", [this]()
	{
		if (!EffectManager) return;
		Prepare();
		const int32 WoundedHP = HeroStats->CurrentHP;
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Percent restore"), OTHER_PAYOUT_ID,
																	   ESkillEffectType::RestoreHPPercent, OTHER_PAYOUT_PERCENT,
																	   OTHER_PAYOUT_TURNS),
								   Foe);
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Heal block"), OTHER_PAYOUT_HEAL_BLOCK_ID,
																	   ESkillEffectType::HealBlock, 0.0f, OTHER_PAYOUT_TURNS + 1),
								   Foe);
		EffectManager->ProcessStartOfTurnEffects(Hero);
		TestEqual(TEXT("No heal"), HeroStats->CurrentHP, WoundedHP);
		TestEqual(TEXT("No tick"), TicksThroughGAS, 0);
	});

	It("kills through the death path when a drain is lethal", [this]()
	{
		if (!EffectManager) return;
		HeroStats->ServerSetHP(1);
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Lethal drain"), OTHER_PAYOUT_ID, ESkillEffectType::DrainHP,
																	   OTHER_PAYOUT_FLAT, OTHER_PAYOUT_TURNS),
								   Foe);
		EffectManager->ProcessStartOfTurnEffects(Hero);
		TestEqual(TEXT("HP 0"), HeroStats->CurrentHP, 0);
		TestFalse(TEXT("Dead"), HeroStats->bIsAlive);
		TestEqual(TEXT("Ticks through GAS"), TicksThroughGAS, 1);
	});
}

#endif
