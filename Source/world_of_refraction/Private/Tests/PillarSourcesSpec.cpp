// PillarSourcesSpec.cpp
// GAS stage 3 (B-042t): the pillars (UWoRAttributeSet_Stats::GetComposedPillars) on GAS. The attribute's base
// is the world base, a UWoRGE_StatSources_<Pillar> effect multiplies in the equipment, crystal and pillar buff
// layers, and the current value equals UCharacterDataComponent::GetEvolutionModified*, whose modifier is capped
// alone (so the clamp is that cap times the base).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"
#include "Debug/GAS/WoRAttributeDebug.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "GAS/WoRGameplayEffects.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 PILLAR_SOURCES_BUFF_ID = 4811;
	constexpr int32 PILLAR_SOURCES_DEBUFF_ID = 4812;
	constexpr int32 PILLAR_SOURCES_TURNS = 3;
	/** Percents, as buff values are. */
	constexpr float PILLAR_SOURCES_BUFF_PERCENT = 50.0f;
	/** Far past the modifier cap, so the clamp decides. */
	constexpr float PILLAR_SOURCES_HUGE_PERCENT = 500.0f;
	constexpr float PILLAR_SOURCES_TOLERANCE = 1.0e-3f;
}

BEGIN_DEFINE_SPEC(FPillarSourcesSpec, "WoR.GAS.PillarSources",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AActor *Hero = nullptr;
	UAbilitySystemComponent *ASC = nullptr;
	UCharacterDataComponent *Char = nullptr;
	UWoRAttributeSyncComponent *Sync = nullptr;

	/** Each pillar with its attribute and getter. */
	TArray<TTuple<EWorldPillar, FGameplayAttribute, float>> Pillars() const
	{
		return {
			MakeTuple(EWorldPillar::Mind, UWoRAttributeSet_Stats::GetMindAttribute(), Char->GetEvolutionModifiedMind()),
			MakeTuple(EWorldPillar::Body, UWoRAttributeSet_Stats::GetBodyAttribute(), Char->GetEvolutionModifiedBody()),
			MakeTuple(EWorldPillar::Spirit, UWoRAttributeSet_Stats::GetSpiritAttribute(), Char->GetEvolutionModifiedSpirit()),
		};
	}

	int32 CountPillarSources(EWorldPillar Pillar) const
	{
		FGameplayEffectQuery Query;
		Query.EffectDefinition = UWoRGE_StatSources::GetClassFor(Pillar);
		return ASC->GetActiveEffects(Query).Num();
	}

	void Buff(int32 EffectID, ESkillEffectType Type, float Percent) const
	{
		if (USkillEffectManager *SEM = TestWorld->GetSubsystem<USkillEffectManager>())
		{
			SEM->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Pillar sources test"), EffectID, Type, Percent, PILLAR_SOURCES_TURNS), Hero);
		}
		Sync->SyncAttributes();
	}
END_DEFINE_SPEC(FPillarSourcesSpec)

void FPillarSourcesSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		// The pillar layers sit behind the loadout (ApplyEvolutionPillarModifier's early return).
		const ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(FWoRTestWorld::NewCharacterData());
		Hero = Loadout ? Loadout->GetOwner() : nullptr;
		if (Hero)
		{
			ASC = FWoRTestWorld::AddComponent<UAbilitySystemComponent>(Hero);
			Sync = FWoRTestWorld::AddComponent<UWoRAttributeSyncComponent>(Hero);
			Char = Hero->FindComponentByClass<UCharacterDataComponent>();
		}
		if (!TestNotNull(TEXT("ASC"), ASC) || !TestNotNull(TEXT("Sync"), Sync) || !TestNotNull(TEXT("Character data"), Char) ||
			!TestNotNull(TEXT("Skill effect manager"), TestWorld->GetSubsystem<USkillEffectManager>()))
		{
			ASC = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		Hero = nullptr;
		ASC = nullptr;
		Char = nullptr;
		Sync = nullptr;
		TestWorld.Reset();
	});

	It("holds the world base as the base and the getter as the current value", [this]()
	{
		if (!ASC) return;
		for (const TTuple<EWorldPillar, FGameplayAttribute, float> &Pillar : Pillars())
		{
			const FString Name = UEnum::GetValueAsString(Pillar.Get<0>());
			FWoRStatLayers Layers;
			TestTrue(*(Name + TEXT(": has layers")), Char->GetPillarLayers(Pillar.Get<0>(), Layers));
			TestTrue(*(Name + TEXT(": a world base to scale")), Layers.StatBase > 0.0f);
			TestEqual(*(Name + TEXT(": base = world base")), ASC->GetNumericAttributeBase(Pillar.Get<1>()), Layers.StatBase,
					  PILLAR_SOURCES_TOLERANCE);
			TestEqual(*(Name + TEXT(": current = getter")), ASC->GetNumericAttribute(Pillar.Get<1>()), Pillar.Get<2>(),
					  PILLAR_SOURCES_TOLERANCE);
			TestEqual(*(Name + TEXT(": one sources effect")), CountPillarSources(Pillar.Get<0>()), 1);
		}
		TestTrue(TEXT("The compare sees no mismatch"), UWoRAttributeDebug::GetAttributeMismatchString(Hero).IsEmpty());
	});

	It("multiplies a pillar buff in by updating its sources effect in place", [this]()
	{
		if (!ASC) return;
		const FGameplayAttribute Mind = UWoRAttributeSet_Stats::GetMindAttribute();
		const float Before = Char->GetEvolutionModifiedMind();
		const float BaseBefore = ASC->GetNumericAttributeBase(Mind);
		Buff(PILLAR_SOURCES_BUFF_ID, ESkillEffectType::MindBuff, PILLAR_SOURCES_BUFF_PERCENT);
		TestTrue(TEXT("The buff raised the getter"), Char->GetEvolutionModifiedMind() > Before);
		TestEqual(TEXT("Current = getter"), ASC->GetNumericAttribute(Mind), Char->GetEvolutionModifiedMind(), PILLAR_SOURCES_TOLERANCE);
		TestEqual(TEXT("Base unchanged"), ASC->GetNumericAttributeBase(Mind), BaseBefore, PILLAR_SOURCES_TOLERANCE);
		TestEqual(TEXT("Still one Mind sources effect"), CountPillarSources(EWorldPillar::Mind), 1);
	});

	It("caps the modifier at the ceiling times the base, as the getter does", [this]()
	{
		if (!ASC) return;
		const FGameplayAttribute Body = UWoRAttributeSet_Stats::GetBodyAttribute();
		Buff(PILLAR_SOURCES_BUFF_ID, ESkillEffectType::BodyBuff, PILLAR_SOURCES_HUGE_PERCENT);
		const float Ceiling = ASC->GetNumericAttributeBase(Body) * CombatConstants::STAT_MODIFIER_MAX;
		TestEqual(TEXT("Getter at the ceiling"), Char->GetEvolutionModifiedBody(), Ceiling, PILLAR_SOURCES_TOLERANCE);
		TestEqual(TEXT("Attribute at the ceiling"), ASC->GetNumericAttribute(Body), Ceiling, PILLAR_SOURCES_TOLERANCE);
	});

	It("floors a pillar at 0 under a debuff past -100%", [this]()
	{
		if (!ASC) return;
		const FGameplayAttribute Spirit = UWoRAttributeSet_Stats::GetSpiritAttribute();
		Buff(PILLAR_SOURCES_DEBUFF_ID, ESkillEffectType::SpiritDebuff, PILLAR_SOURCES_HUGE_PERCENT);
		TestEqual(TEXT("Getter floored"), Char->GetEvolutionModifiedSpirit(), 0.0f, PILLAR_SOURCES_TOLERANCE);
		TestEqual(TEXT("Attribute floored"), ASC->GetNumericAttribute(Spirit), 0.0f, PILLAR_SOURCES_TOLERANCE);
	});
}

#endif
