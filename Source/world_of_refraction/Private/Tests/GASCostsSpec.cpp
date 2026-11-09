// GASCostsSpec.cpp
// B-043a (Q-043a): with `wor.Costs.UseGAS 1` a skill's EP cost and its infusion HP
// backlash are paid by Instant GEs (WoRCosts), and the Vitals set hands the change to the game's EP / HP path.
// Otherwise WoRCosts declines and the caller pays the old way. Each case pays as UActionExecutor::SpendEnergy and
// ApplyPendingInfusionHPCost do (TryPay, else the old call), so both paths must leave the same EP and HP.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/CharacterDataComponent.h"
#include "GAS/WoRActionCosts.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "GAS/WoRGameplayEffects.h"
#include "GameplayEffectComponents/ImmunityGameplayEffectComponent.h"
#include "Tags/WoRGameplayTags.h"

namespace
{
	constexpr int32 COSTS_START_EP = 20;
	constexpr int32 COSTS_EP_COST = 7;
	constexpr int32 COSTS_BACKLASH = 6;
	constexpr int32 COSTS_LOW_HP = 3;
}

BEGIN_DEFINE_SPEC(FGASCostsSpec, "WoR.GAS.Costs",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AActor *Hero = nullptr;
	UAbilitySystemComponent *ASC = nullptr;
	UCharacterDataComponent *HeroStats = nullptr;
	int32 CostEffectsApplied = 0;

	/** As UActionExecutor::SpendEnergy after its Silence check. */
	void SpendAsExecutor(int32 Amount)
	{
		if (!WoRCosts::TryPayEnergy(Hero, Amount))
		{
			HeroStats->ServerSpendEnergy(Amount);
		}
	}

	/** As UActionExecutor::ApplyPendingInfusionHPCost. */
	void BacklashAsExecutor(int32 Amount)
	{
		if (!WoRCosts::TryPayBacklash(Hero, Amount))
		{
			HeroStats->ServerTakeDamage(Amount);
		}
	}

	/** B-042g2: wor.Costs.UseGAS alone (the effects switch is gone; B-043h deletes this one). */
	void SetCostsSwitch(bool bCosts)
	{
		WoRCosts::SetPaysThroughGAS(bCosts);
	}
END_DEFINE_SPEC(FGASCostsSpec)

void FGASCostsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		FWoRTestWorld::AddEffectMirror(Hero);
		ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Hero);
		HeroStats = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		CostEffectsApplied = 0;
		if (!TestNotNull(TEXT("AbilitySystemComponent"), ASC) || !TestNotNull(TEXT("Hero stats"), HeroStats) ||
			!TestNotNull(TEXT("Vitals set"), ASC->GetSet<UWoRAttributeSet_Vitals>()))
		{
			HeroStats = nullptr; // each case returns early
			return;
		}
		HeroStats->MaxEP = COSTS_START_EP;
		HeroStats->CurrentEP = COSTS_START_EP;
		ASC->OnGameplayEffectAppliedDelegateToSelf.AddLambda(
			[this](UAbilitySystemComponent *, const FGameplayEffectSpec &Spec, FActiveGameplayEffectHandle)
			{
				if (Spec.Def && (Spec.Def->IsA<UWoRGE_EnergyCost>() || Spec.Def->IsA<UWoRGE_HPBacklash>()))
				{
					++CostEffectsApplied;
				}
			});
	});

	AfterEach([this]()
	{
		SetCostsSwitch(false);
		Hero = nullptr;
		ASC = nullptr;
		HeroStats = nullptr;
		TestWorld.Reset();
	});

	It("builds the cost specs with minus the amount", [this]()
	{
		if (!HeroStats)
		{
			return;
		}
		const FGameplayEffectSpecHandle Energy = WoRCosts::MakeEnergyCostSpec(ASC, COSTS_EP_COST);
		const FGameplayEffectSpecHandle Backlash = WoRCosts::MakeBacklashSpec(ASC, COSTS_BACKLASH);
		if (TestTrue(TEXT("Energy spec"), Energy.IsValid()) && TestTrue(TEXT("Backlash spec"), Backlash.IsValid()))
		{
			TestEqual(TEXT("EP change"), Energy.Data->GetSetByCallerMagnitude(WoRTags::SetByCaller_Cost_Energy, false, 0.0f),
					  -static_cast<float>(COSTS_EP_COST));
			TestEqual(TEXT("HP change"), Backlash.Data->GetSetByCallerMagnitude(WoRTags::SetByCaller_Cost_Health, false, 0.0f),
					  -static_cast<float>(COSTS_BACKLASH));
		}
		TestFalse(TEXT("No spec without an ASC"), WoRCosts::MakeEnergyCostSpec(nullptr, COSTS_EP_COST).IsValid());
	});

	It("declines with the costs switch off", [this]()
	{
		if (!HeroStats)
		{
			return;
		}
		SetCostsSwitch(false);
		TestFalse(TEXT("Costs switch off"), WoRCosts::TryPayEnergy(Hero, COSTS_EP_COST));
		TestEqual(TEXT("Nothing paid"), HeroStats->CurrentEP, COSTS_START_EP);
		TestEqual(TEXT("No cost GE"), CostEffectsApplied, 0);
	});

	It("pays nothing for a free skill", [this]()
	{
		if (!HeroStats)
		{
			return;
		}
		SetCostsSwitch(true);
		TestTrue(TEXT("Zero is paid"), WoRCosts::TryPayEnergy(Hero, 0));
		TestEqual(TEXT("EP unchanged"), HeroStats->CurrentEP, COSTS_START_EP);
		TestEqual(TEXT("No cost GE"), CostEffectsApplied, 0);
	});

	It("pays once the old way when an immunity blocks the cost GE (never free, never twice)", [this]()
	{
		if (!HeroStats)
		{
			return;
		}
		UGameplayEffect *CostImmunity = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("GE_TestCostImmunity"));
		CostImmunity->DurationPolicy = EGameplayEffectDurationType::Infinite;
		CostImmunity->FindOrAddComponent<UImmunityGameplayEffectComponent>().ImmunityQueries.Add(
			FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(FGameplayTagContainer(WoRTags::Effect_Family_Cost)));
		ASC->ApplyGameplayEffectToSelf(CostImmunity, 1.0f, ASC->MakeEffectContext());

		SetCostsSwitch(true);
		TestFalse(TEXT("The blocked GE declines"), WoRCosts::TryPayEnergy(Hero, COSTS_EP_COST));
		TestEqual(TEXT("Nothing paid by the blocked GE"), HeroStats->CurrentEP, COSTS_START_EP);
		SpendAsExecutor(COSTS_EP_COST);
		TestEqual(TEXT("Paid once the old way"), HeroStats->CurrentEP, COSTS_START_EP - COSTS_EP_COST);
		TestEqual(TEXT("No cost GE applied"), CostEffectsApplied, 0);
	});

	for (const bool bThroughGAS : {false, true})
	{
		Describe(bThroughGAS ? TEXT("through GAS") : TEXT("the old way"), [this, bThroughGAS]()
		{
			BeforeEach([this, bThroughGAS]() { SetCostsSwitch(bThroughGAS); });

			It("pays the EP cost", [this, bThroughGAS]()
			{
				if (!HeroStats)
				{
					return;
				}
				SpendAsExecutor(COSTS_EP_COST);
				TestEqual(TEXT("EP after the cost"), HeroStats->CurrentEP, COSTS_START_EP - COSTS_EP_COST);
				TestEqual(TEXT("Cost GEs"), CostEffectsApplied, bThroughGAS ? 1 : 0);
				if (bThroughGAS)
				{
					TestEqual(TEXT("Energy attribute follows the game's EP"),
							  FMath::RoundToInt(ASC->GetNumericAttribute(UWoRAttributeSet_Vitals::GetEnergyAttribute())),
							  HeroStats->CurrentEP);
				}
			});

			It("pays the HP backlash", [this, bThroughGAS]()
			{
				if (!HeroStats)
				{
					return;
				}
				const int32 StartHP = HeroStats->CurrentHP;
				BacklashAsExecutor(COSTS_BACKLASH);
				TestEqual(TEXT("HP after the backlash"), HeroStats->CurrentHP, StartHP - COSTS_BACKLASH);
				TestEqual(TEXT("Cost GEs"), CostEffectsApplied, bThroughGAS ? 1 : 0);
			});

			It("kills with a lethal backlash", [this]()
			{
				if (!HeroStats)
				{
					return;
				}
				HeroStats->ServerSetHP(COSTS_LOW_HP);
				BacklashAsExecutor(COSTS_BACKLASH);
				TestEqual(TEXT("HP 0"), HeroStats->CurrentHP, 0);
				TestFalse(TEXT("Dead"), HeroStats->bIsAlive);
			});
		});
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
