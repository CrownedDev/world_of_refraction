// GASTriggerParitySpec.cpp
// B-042i (Q-042i, Q-042i-b): a trigger's payload and a starting effect reach GAS through the spec builder in the same
// call that stores them (ApplyEffect -> OnEffectApplied -> the mirror's sync), as one GE of the effect's identity, on
// both paths (wor.Effects.UseGAS 0 and 1). A second fire replaces, never doubles. The stored copy stays until B-042g.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/CharacterData.h"
#include "Combat/Defense/EDefenseType.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "GAS/WoREffectSpecBuilder.h"
#include "GAS/WoRTurnEffectTracker.h"
#include "Skills/Effects/EffectIdentity.h"
#include "Skills/Effects/FGatheredEffect.h"
#include "Skills/Effects/FSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 TRIGGER_PARITY_DEF_ID = 95;
	constexpr int32 TRIGGER_PARITY_ITEM = 7201;
	constexpr float TRIGGER_PARITY_HEAL = 12.0f;
	constexpr int32 TRIGGER_PARITY_TURNS = 2;
	constexpr float TRIGGER_PARITY_ATTACK_COST = 20.0f;

	FSkillEffectPayload SelfHeal()
	{
		FSkillEffectPayload Payload;
		Payload.EffectType = ESkillEffectType::HealthRestore;
		Payload.Magnitude = TRIGGER_PARITY_HEAL;
		Payload.Duration = TRIGGER_PARITY_TURNS; // turn-counted: a duration-0 heal is instant and never stored
		Payload.Target = ETargetType::Self;
		Payload.TargetCount = ETargetCount::Single;
		return Payload;
	}

	/** Gear that heals its wearer when the wearer parries. */
	FGatheredEffect ParryHealGear()
	{
		FSkillEffect Effect;
		Effect.EffectName = TEXT("Trigger parity parry heal");
		FSkillCondition OnParry;
		OnParry.Trigger = ESkillTrigger::OnParry;
		OnParry.Subject = ECondSubject::Self;
		Effect.Conditions.Add(OnParry);
		Effect.Payloads.Add(SelfHeal());
		return FGatheredEffect(TRIGGER_PARITY_DEF_ID, 0, Effect, TRIGGER_PARITY_ITEM);
	}

	/** Gear whose heal is on from the start of the fight. */
	FGatheredEffect StartingHealGear()
	{
		FSkillEffect Effect;
		Effect.EffectName = TEXT("Trigger parity starting heal");
		Effect.Payloads.Add(SelfHeal());
		return FGatheredEffect(TRIGGER_PARITY_DEF_ID, 1, Effect, TRIGGER_PARITY_ITEM);
	}
}

BEGIN_DEFINE_SPEC(FGASTriggerParitySpec, "WoR.GAS.TriggerParity", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;
	AActor *Holder = nullptr;
	AActor *Foe = nullptr;
	UWoREffectMirrorComponent *Mirror = nullptr;

	void Parry()
	{
		EffectManager->OnDefenseResolvedHandler(Holder, Foe, EDefenseType::Parry, true, 0, TRIGGER_PARITY_ATTACK_COST);
	}

	/** The holder's stored effects with this packed ID. */
	TArray<FActiveSkillEffect> StoredWithID(int32 EffectID) const
	{
		return EffectManager->GetActiveEffects(Holder).FilterByPredicate([EffectID](const FActiveSkillEffect &E) { return E.EffectID == EffectID; });
	}

	/** Live GEs on the holder's ASC carrying this stored effect's identity. */
	int32 LiveGEsOf(const FActiveSkillEffect &Effect) const
	{
		const UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Holder);
		if (!ASC)
		{
			return 0;
		}
		const FWoREffectIdentity Wanted = FWoREffectSpecBuilder::MakeIdentity(Effect);
		int32 Count = 0;
		for (const FActiveGameplayEffect &Active : &ASC->GetActiveGameplayEffects())
		{
			const FWoREffectContext *Context = FWoREffectContext::Get(Active.Spec.GetContext());
			Count += Context && Context->Identity.IsSameIdentity(Wanted) ? 1 : 0;
		}
		return Count;
	}

	/** At UseGAS 1 the tracker owns the GE's turns (B-042h); at 0 the mirror holds it untracked. */
	void CheckPathOwnership(const TCHAR *What, const FActiveSkillEffect &Stored, bool bReadsFromGAS)
	{
		const int32 TrackedTurns = Mirror->GetTrackedTurnsLeft(Stored);
		if (bReadsFromGAS)
		{
			TestEqual(FString::Printf(TEXT("%s: the tracker counts its turns"), What), TrackedTurns, TRIGGER_PARITY_TURNS);
		}
		else
		{
			TestEqual(FString::Printf(TEXT("%s: untracked on the enum path"), What), TrackedTurns, INDEX_NONE);
		}
	}

	void CheckOneHealOnGAS(const TCHAR *What, int32 EffectID, bool bReadsFromGAS)
	{
		const TArray<FActiveSkillEffect> Stored = StoredWithID(EffectID);
		if (!TestEqual(FString::Printf(TEXT("%s: one stored copy"), What), Stored.Num(), 1))
		{
			return;
		}
		TestEqual(FString::Printf(TEXT("%s: a heal"), What), Stored[0].EffectType, ESkillEffectType::HealthRestore);
		TestTrue(FString::Printf(TEXT("%s: a value"), What), Stored[0].EffectValue > 0.0f);
		TestEqual(FString::Printf(TEXT("%s: exactly one GE of its identity"), What), LiveGEsOf(Stored[0]), 1);
		TestEqual(FString::Printf(TEXT("%s: the mirror holds it"), What), Mirror->NumMirrored(), 1);
		CheckPathOwnership(What, Stored[0], bReadsFromGAS);
	}
END_DEFINE_SPEC(FGASTriggerParitySpec)

void FGASTriggerParitySpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		Holder = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		Mirror = FWoRTestWorld::AddEffectMirror(Holder);
		if (!TestNotNull(TEXT("SkillEffectManager"), EffectManager) || !TestNotNull(TEXT("Mirror"), Mirror))
		{
			EffectManager = nullptr; // each case returns early
		}
		else
		{
			EffectManager->ResetForNewCombat();
		}
	});

	AfterEach([this]()
	{
		UWoREffectMirrorComponent::SetReadsFromGAS(false);
		EffectManager = nullptr;
		Holder = nullptr;
		Foe = nullptr;
		Mirror = nullptr;
		TestWorld.Reset();
	});

	for (const bool bReadsFromGAS : {false, true})
	{
		Describe(bReadsFromGAS ? TEXT("UseGAS 1") : TEXT("UseGAS 0"), [this, bReadsFromGAS]()
		{
			BeforeEach([bReadsFromGAS]() { UWoREffectMirrorComponent::SetReadsFromGAS(bReadsFromGAS); });

			It("lands a defender trigger's payload as one GE of its identity when it fires", [this, bReadsFromGAS]()
			{
				if (!EffectManager) return;
				EffectManager->ArmConditionalEffects(Holder, {ParryHealGear()});
				TestEqual(TEXT("armed, nothing on GAS yet"), Mirror->NumMirrored(), 0);
				Parry();
				CheckOneHealOnGAS(TEXT("parry heal"), EffectIdentity::PackEffectID(TRIGGER_PARITY_DEF_ID, 0, 0), bReadsFromGAS);
			});

			It("replaces, never doubles, when the trigger fires again", [this, bReadsFromGAS]()
			{
				if (!EffectManager) return;
				EffectManager->ArmConditionalEffects(Holder, {ParryHealGear()});
				Parry();
				// A holder turn passes, then the second parry refreshes the span to its full length again.
				EffectManager->ProcessStartOfTurnEffects(Holder);
				Parry();
				CheckOneHealOnGAS(TEXT("second parry"), EffectIdentity::PackEffectID(TRIGGER_PARITY_DEF_ID, 0, 0), bReadsFromGAS);
			});

			It("lands a starting effect as one GE of its identity at combat start", [this, bReadsFromGAS]()
			{
				if (!EffectManager) return;
				EffectManager->ApplyEquipmentEffects(Holder, {StartingHealGear()});
				CheckOneHealOnGAS(TEXT("starting heal"), EffectIdentity::PackEffectID(TRIGGER_PARITY_DEF_ID, 1, 0), bReadsFromGAS);
			});

			It("ends the GE with the stored copy when the effect is removed", [this]()
			{
				if (!EffectManager) return;
				EffectManager->ApplyEquipmentEffects(Holder, {StartingHealGear()});
				const TArray<FActiveSkillEffect> Stored = StoredWithID(EffectIdentity::PackEffectID(TRIGGER_PARITY_DEF_ID, 1, 0));
				if (!TestEqual(TEXT("stored"), Stored.Num(), 1)) return;
				EffectManager->RemoveAllEffects(Holder);
				TestEqual(TEXT("no GE left"), LiveGEsOf(Stored[0]), 0);
				TestEqual(TEXT("nothing mirrored"), Mirror->NumMirrored(), 0);
			});
		});
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
