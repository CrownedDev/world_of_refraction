// GASPayoutsSpec.cpp
// GAS stage 3 (B-042y): with `wor.Effects.UseGAS 1` a DOT's and a HOT's tick is paid by its GE's tick spec
// (UWoRTickExecution), and the Vitals set hands the Health change to the game's HP path (damage, heal,
// death). With 0 the enum path pays. The HP each path leaves is the same.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "GAS/WoRGameplayEffects.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 PAYOUT_DOT_ID = 4601;
	constexpr int32 PAYOUT_HOT_ID = 4602;
	constexpr int32 PAYOUT_HEAL_BLOCK_ID = 4603;
	constexpr int32 PAYOUT_TURNS = 2;
	constexpr float PAYOUT_DOT_VALUE = 5.0f;
	constexpr float PAYOUT_HOT_VALUE = 4.0f;
	constexpr float PAYOUT_LETHAL_DOT_VALUE = 50.0f;
	constexpr int32 PAYOUT_WOUND = 20;
	constexpr int32 PAYOUT_LOW_HP = 1;
}

BEGIN_DEFINE_SPEC(FGASPayoutsSpec, "WoR.GAS.Payouts",
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

	int32 ExpectedTicks(bool bReadsFromGAS, int32 Ticks) const { return bReadsFromGAS ? Ticks : 0; }
END_DEFINE_SPEC(FGASPayoutsSpec)

void FGASPayoutsSpec::Define()
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
			!TestNotNull(TEXT("AbilitySystemComponent"), ASC) || !TestNotNull(TEXT("Hero stats"), HeroStats))
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
		UWoREffectMirrorComponent::SetReadsFromGAS(false);
		EffectManager = nullptr;
		ASC = nullptr;
		HeroStats = nullptr;
		Hero = nullptr;
		Foe = nullptr;
		TestWorld.Reset();
	});

	for (const bool bReadsFromGAS : {false, true})
	{
		Describe(bReadsFromGAS ? TEXT("UseGAS 1") : TEXT("UseGAS 0"), [this, bReadsFromGAS]()
		{
			BeforeEach([bReadsFromGAS]() { UWoREffectMirrorComponent::SetReadsFromGAS(bReadsFromGAS); });

			It("pays a DOT's ticks, on GAS with the switch on", [this, bReadsFromGAS]()
			{
				if (!EffectManager) return;
				const FActiveSkillEffect Burn = FActiveSkillEffect::CreateDOT(TEXT("Payout burn"), PAYOUT_DOT_ID, PAYOUT_DOT_VALUE,
																			  PAYOUT_TURNS, ESpellElement::Fire);
				const int32 StartHP = HeroStats->CurrentHP;
				const int32 Hit = USkillEffectManager::GetDotTickDamage(Hero, Burn);
				EffectManager->ApplyEffect(Hero, Burn, Foe);
				for (int32 Turn = 0; Turn < PAYOUT_TURNS; ++Turn)
				{
					HolderTurn();
				}
				HolderTurn();
				TestTrue(TEXT("The burn hits"), Hit > 0);
				TestEqual(TEXT("Exactly N hits"), HeroStats->CurrentHP, StartHP - PAYOUT_TURNS * Hit);
				TestEqual(TEXT("Ticks through GAS"), TicksThroughGAS, ExpectedTicks(bReadsFromGAS, PAYOUT_TURNS));
				if (bReadsFromGAS)
				{
					TestEqual(TEXT("Health holds the HP"),
							  FMath::RoundToInt(ASC->GetNumericAttribute(UWoRAttributeSet_Vitals::GetHealthAttribute())), HeroStats->CurrentHP);
				}
			});

			It("pays a HOT's ticks, on GAS with the switch on", [this, bReadsFromGAS]()
			{
				if (!EffectManager) return;
				HeroStats->ServerTakeDamage(PAYOUT_WOUND);
				const int32 WoundedHP = HeroStats->CurrentHP;
				EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Payout heal"), PAYOUT_HOT_ID, ESkillEffectType::HealthRestore,
																			   PAYOUT_HOT_VALUE, PAYOUT_TURNS),
										   Foe);
				for (int32 Turn = 0; Turn < PAYOUT_TURNS; ++Turn)
				{
					HolderTurn();
				}
				TestEqual(TEXT("Healed N times"), HeroStats->CurrentHP, WoundedHP + PAYOUT_TURNS * FMath::RoundToInt(PAYOUT_HOT_VALUE));
				TestEqual(TEXT("Ticks through GAS"), TicksThroughGAS, ExpectedTicks(bReadsFromGAS, PAYOUT_TURNS));
			});

			It("kills through the death path when a tick is lethal", [this, bReadsFromGAS]()
			{
				if (!EffectManager) return;
				HeroStats->ServerSetHP(PAYOUT_LOW_HP);
				EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateDOT(TEXT("Payout lethal burn"), PAYOUT_DOT_ID, PAYOUT_LETHAL_DOT_VALUE,
																			  PAYOUT_TURNS, ESpellElement::Fire),
										   Foe);
				EffectManager->ProcessStartOfTurnEffects(Hero);
				TestEqual(TEXT("HP 0"), HeroStats->CurrentHP, 0);
				TestFalse(TEXT("Dead"), HeroStats->bIsAlive);
				TestEqual(TEXT("Ticks through GAS"), TicksThroughGAS, ExpectedTicks(bReadsFromGAS, 1));
			});

			It("skips a HOT's tick while heal-blocked", [this, bReadsFromGAS]()
			{
				if (!EffectManager) return;
				HeroStats->ServerTakeDamage(PAYOUT_WOUND);
				const int32 WoundedHP = HeroStats->CurrentHP;
				EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Payout heal"), PAYOUT_HOT_ID, ESkillEffectType::HealthRestore,
																			   PAYOUT_HOT_VALUE, PAYOUT_TURNS),
										   Foe);
				EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Payout heal block"), PAYOUT_HEAL_BLOCK_ID,
																			   ESkillEffectType::HealBlock, 0.0f, PAYOUT_TURNS + 1),
										   Foe);
				EffectManager->ProcessStartOfTurnEffects(Hero);
				TestEqual(TEXT("No heal"), HeroStats->CurrentHP, WoundedHP);
				TestEqual(TEXT("No tick"), TicksThroughGAS, 0);
			});
		});
	}
}

#endif
