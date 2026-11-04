// SkillConditionParitySpec.cpp
// B-337d (D-130, Q-337c): a skill effect's authored conditions (FSkillCondition) read across into the one
// vocabulary (SkillConditions) and check through WoRConditions. Parity grid: every trigger x side x AND / OR,
// singly and in pairs, over fighter HP / EP states, with and without a target, for the three call sites (an
// actor-state check, the impact path's defense outcomes, an action's result), against a copy of the old
// checks (USkillEffectManager::IsSingleTriggerMet, FireDefenseTriggerEffects, UActionExecutor's EventMet).
// The old float percent could misread an exact boundary; the grid stays off exact boundaries, and the boundary
// cases below pin the exact answer.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Conditions/WoRConditions.h"
#include "Skills/Effects/SkillConditions.h"
#include "Skills/Effects/SkillTriggerUtils.h"

namespace
{
	constexpr int32 PARITY_MAX_POOL = 1000;
	constexpr float PARITY_LOW_THRESHOLD = 30.0f;
	constexpr float PARITY_HIGH_THRESHOLD = 75.0f;
	constexpr int32 PARITY_MAX_REPORTED = 5;

	enum EParityFighter : int32 { PF_Self, PF_Ally, PF_Target, PF_Foe, PF_Count };

	struct FParityScene
	{
		FWoRFighterState Fighters[PF_Count];
		bool bHasTarget = true;
		int32 Defender = INDEX_NONE;
		TArray<ESkillTrigger> Fired;
	};

	FWoRFighterState ParityFighter(int32 HP, int32 EP, int32 Max = PARITY_MAX_POOL)
	{
		FWoRFighterState Out;
		Out.CurrentHP = HP;
		Out.MaxHP = Max;
		Out.CurrentEP = EP;
		Out.MaxEP = Max;
		return Out;
	}

	// ---- The old checks, copied (parity reference) ----

	float OldPercent(int32 Current, int32 Max)
	{
		return Max > 0 ? (static_cast<float>(Current) / static_cast<float>(Max)) * 100.0f : 0.0f;
	}

	bool OldSingle(const FWoRFighterState &F, ESkillTrigger Trigger, float Threshold)
	{
		switch (Trigger)
		{
		case ESkillTrigger::OnHPBelowThreshold:     return OldPercent(F.CurrentHP, F.MaxHP) < Threshold;
		case ESkillTrigger::OnHPAboveThreshold:     return OldPercent(F.CurrentHP, F.MaxHP) > Threshold;
		case ESkillTrigger::OnEnergyBelowThreshold: return OldPercent(F.CurrentEP, F.MaxEP) < Threshold;
		case ESkillTrigger::OnEnergyAboveThreshold: return OldPercent(F.CurrentEP, F.MaxEP) > Threshold;
		case ESkillTrigger::Always:
		case ESkillTrigger::OnCrit:
		case ESkillTrigger::OnHit:
		case ESkillTrigger::OnTakeDamage:
		case ESkillTrigger::OnKill:
		case ESkillTrigger::OnDodge:
		case ESkillTrigger::OnBlock:
		case ESkillTrigger::OnGuard:
		case ESkillTrigger::OnTurnStart:
		case ESkillTrigger::OnTurnEnd:
		case ESkillTrigger::OnSpellCast:
		case ESkillTrigger::OnAbilityUsed:
		case ESkillTrigger::OnBattleStart:
		case ESkillTrigger::OnStatusApplied:
		case ESkillTrigger::OnStatusReceived:
			return true;
		default:
			return false;
		}
	}

	TArray<int32> OldSubjects(ECondSubject Subject, const FParityScene &Scene)
	{
		switch (Subject)
		{
		case ECondSubject::Self:       return {PF_Self};
		case ECondSubject::SelfTeam:   return {PF_Self, PF_Ally};
		case ECondSubject::Target:     return Scene.bHasTarget ? TArray<int32>{PF_Target} : TArray<int32>{};
		case ECondSubject::TargetTeam: return Scene.bHasTarget ? TArray<int32>{PF_Target, PF_Foe} : TArray<int32>{};
		}
		return {};
	}

	bool OldCheck(const TArray<FSkillCondition> &Conditions, ESkillCheck Check, const FParityScene &Scene)
	{
		if (Check == ESkillCheck::ActionResult)
		{
			return EvaluateConditionGroup(
				Conditions, [](const FSkillCondition &C) { return IsOwnerSide(C.Subject); },
				[&](const FSkillCondition &C)
				{
					return C.Trigger == ESkillTrigger::Always ||
						   ((C.Trigger == ESkillTrigger::OnHit || C.Trigger == ESkillTrigger::OnCrit || C.Trigger == ESkillTrigger::OnKill) &&
							Scene.Fired.Contains(C.Trigger));
				});
		}
		return EvaluateConditionGroup(
			Conditions, [&](const FSkillCondition &C) { return IsOwnerSide(C.Subject) || Scene.bHasTarget; },
			[&](const FSkillCondition &C)
			{
				const TArray<int32> Subjects = OldSubjects(C.Subject, Scene);
				if (Check == ESkillCheck::Defense && SkillTriggerUtils::IsDefenseOutcomeTrigger(C.Trigger))
				{
					return Scene.Fired.Contains(C.Trigger) && Subjects.Contains(Scene.Defender);
				}
				return Subjects.ContainsByPredicate([&](int32 I) { return OldSingle(Scene.Fighters[I], C.Trigger, C.Threshold); });
			});
	}

	// ---- The new check: the context a call site builds ----

	bool NewCheck(const TArray<FSkillCondition> &Conditions, ESkillCheck Check, const FParityScene &Scene)
	{
		FWoRFighterState States[PF_Count];
		for (int32 I = 0; I < PF_Count; ++I)
		{
			States[I] = Scene.Fighters[I];
			States[I].bDefender = I == Scene.Defender;
		}
		FWoRConditionContext Context;
		SkillConditions::PrepareContext(Context, Check, Scene.Fired);
		if (Check != ESkillCheck::ActionResult) // an action's gate reads no fighter
		{
			Context.Self = &States[PF_Self];
			Context.SelfTeam = {States[PF_Self], States[PF_Ally]};
			if (Scene.bHasTarget)
			{
				Context.Target = &States[PF_Target];
				Context.TargetTeam = {States[PF_Target], States[PF_Foe]};
			}
		}
		else if (Scene.bHasTarget)
		{
			Context.Target = &States[PF_Target]; // only says there is one; nothing target-side takes part
		}
		return SkillConditions::IsGroupMet(Conditions, Check, Context);
	}

	FSkillCondition ParityCondition(ESkillTrigger Trigger, ECondSubject Subject, ECondCombine Combine, float Threshold)
	{
		FSkillCondition Out;
		Out.Trigger = Trigger;
		Out.Subject = Subject;
		Out.Combine = Combine;
		Out.Threshold = Threshold;
		return Out;
	}

	TArray<FSkillCondition> ParityAllConditions()
	{
		TArray<FSkillCondition> Out;
		const int32 Last = static_cast<int32>(ESkillTrigger::OnFlawlessParry);
		for (int32 T = 0; T <= Last; ++T)
		{
			const ESkillTrigger Trigger = static_cast<ESkillTrigger>(T);
			const TArray<float> Thresholds = SkillTriggerUtils::IsThresholdTrigger(Trigger)
												 ? TArray<float>{PARITY_LOW_THRESHOLD, PARITY_HIGH_THRESHOLD}
												 : TArray<float>{PARITY_LOW_THRESHOLD};
			for (const ECondSubject Subject : {ECondSubject::Self, ECondSubject::SelfTeam, ECondSubject::Target, ECondSubject::TargetTeam})
			{
				for (const ECondCombine Combine : {ECondCombine::And, ECondCombine::Or})
				{
					for (const float Threshold : Thresholds)
					{
						Out.Add(ParityCondition(Trigger, Subject, Combine, Threshold));
					}
				}
			}
		}
		return Out;
	}

	/** Fighter HP / EP off every exact boundary of 30% / 75% (and a fighter with no max). */
	TArray<FParityScene> ParityStates()
	{
		const int32 Values[] = {0, 123, 299, 301, 749, 751, PARITY_MAX_POOL};
		TArray<FParityScene> Out;
		for (int32 I = 0; I < UE_ARRAY_COUNT(Values); ++I)
		{
			FParityScene Scene;
			Scene.Fighters[PF_Self] = ParityFighter(Values[I], Values[(I + 3) % UE_ARRAY_COUNT(Values)]);
			Scene.Fighters[PF_Ally] = ParityFighter(Values[(I + 1) % UE_ARRAY_COUNT(Values)], Values[(I + 5) % UE_ARRAY_COUNT(Values)]);
			Scene.Fighters[PF_Target] = ParityFighter(Values[(I + 2) % UE_ARRAY_COUNT(Values)], Values[(I + 4) % UE_ARRAY_COUNT(Values)]);
			Scene.Fighters[PF_Foe] = ParityFighter(Values[(I + 6) % UE_ARRAY_COUNT(Values)], Values[I]);
			Out.Add(Scene);
		}
		FParityScene NoMax;
		for (FWoRFighterState &F : NoMax.Fighters)
		{
			F = ParityFighter(0, 0, 0);
		}
		Out.Add(NoMax);
		return Out;
	}

	/** Every scene a check is asked in: each state with / without a target, and the check's events. */
	TArray<FParityScene> ParityScenes(ESkillCheck Check)
	{
		TArray<TArray<ESkillTrigger>> EventSets;
		TArray<int32> Defenders = {INDEX_NONE};
		if (Check == ESkillCheck::Defense)
		{
			EventSets.Reset();
			for (const EDefenseType Type : {EDefenseType::None, EDefenseType::Block, EDefenseType::Dodge})
			{
				for (const bool bPerfect : {false, true})
				{
					TArray<ESkillTrigger> &Events = EventSets.AddDefaulted_GetRef();
					SkillTriggerUtils::DefenseOutcomeToTriggers(Type, bPerfect, Events);
				}
			}
			EventSets.Add({ESkillTrigger::OnFlawlessParry});
			Defenders = {PF_Self, PF_Ally, PF_Target};
		}
		else if (Check == ESkillCheck::ActionResult)
		{
			for (int32 Mask = 0; Mask < 8; ++Mask)
			{
				TArray<ESkillTrigger> &Events = EventSets.AddDefaulted_GetRef();
				if (Mask & 1) { Events.Add(ESkillTrigger::OnHit); }
				if (Mask & 2) { Events.Add(ESkillTrigger::OnCrit); }
				if (Mask & 4) { Events.Add(ESkillTrigger::OnKill); }
			}
		}
		else
		{
			EventSets.AddDefaulted();
		}
		TArray<FParityScene> Out;
		for (const FParityScene &State : ParityStates())
		{
			for (const bool bHasTarget : {true, false})
			{
				for (const TArray<ESkillTrigger> &Events : EventSets)
				{
					for (const int32 Defender : Defenders)
					{
						FParityScene &Scene = Out.Add_GetRef(State);
						Scene.bHasTarget = bHasTarget;
						Scene.Fired = Events;
						Scene.Defender = Defender;
					}
				}
			}
		}
		return Out;
	}
}

BEGIN_DEFINE_SPEC(FSkillConditionParitySpec, "WoR.Conditions.SkillParity", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	int32 CountMismatches(const TArray<TArray<FSkillCondition>> &Groups, ESkillCheck Check);
END_DEFINE_SPEC(FSkillConditionParitySpec)

int32 FSkillConditionParitySpec::CountMismatches(const TArray<TArray<FSkillCondition>> &Groups, ESkillCheck Check)
{
	const TArray<FParityScene> Scenes = ParityScenes(Check);
	int32 Mismatches = 0;
	for (const TArray<FSkillCondition> &Group : Groups)
	{
		for (const FParityScene &Scene : Scenes)
		{
			const bool bOld = OldCheck(Group, Check, Scene);
			if (bOld != NewCheck(Group, Check, Scene) && ++Mismatches <= PARITY_MAX_REPORTED)
			{
				AddError(FString::Printf(TEXT("check %d: %s (target %d, defender %d, %d events) old %d"), static_cast<int32>(Check),
										 *SkillConditions::GetGroupString(Group, Check, Scene.bHasTarget), Scene.bHasTarget, Scene.Defender,
										 Scene.Fired.Num(), bOld));
			}
		}
	}
	return Mismatches;
}

void FSkillConditionParitySpec::Define()
{
	Describe("Every authored condition alone", [this]()
	{
		for (const ESkillCheck Check : {ESkillCheck::State, ESkillCheck::Defense, ESkillCheck::ActionResult})
		{
			It(FString::Printf(TEXT("gives the old answer (check %d)"), static_cast<int32>(Check)), [this, Check]()
			{
				TArray<TArray<FSkillCondition>> Groups;
				for (const FSkillCondition &C : ParityAllConditions())
				{
					Groups.Add({C});
				}
				TestEqual(TEXT("mismatches"), CountMismatches(Groups, Check), 0);
			});
		}
	});

	Describe("Pairs and mixed AND / OR groups", [this]()
	{
		for (const ESkillCheck Check : {ESkillCheck::State, ESkillCheck::Defense, ESkillCheck::ActionResult})
		{
			It(FString::Printf(TEXT("give the old answer (check %d)"), static_cast<int32>(Check)), [this, Check]()
			{
				// Every pair over a spread of the conditions (each trigger in a few shapes).
				const TArray<FSkillCondition> All = ParityAllConditions();
				constexpr int32 PARITY_PAIR_STRIDE = 7;
				TArray<FSkillCondition> Spread;
				for (int32 I = 0; I < All.Num(); I += PARITY_PAIR_STRIDE)
				{
					Spread.Add(All[I]);
				}
				TArray<TArray<FSkillCondition>> Groups;
				for (const FSkillCondition &A : Spread)
				{
					for (const FSkillCondition &B : Spread)
					{
						Groups.Add({A, B});
					}
				}
				// Two ANDs and two ORs on both sides.
				Groups.Add({ParityCondition(ESkillTrigger::OnHit, ECondSubject::Self, ECondCombine::And, PARITY_LOW_THRESHOLD),
							ParityCondition(ESkillTrigger::OnHPBelowThreshold, ECondSubject::Target, ECondCombine::And, PARITY_HIGH_THRESHOLD),
							ParityCondition(ESkillTrigger::OnPerfectGuard, ECondSubject::Self, ECondCombine::Or, PARITY_LOW_THRESHOLD),
							ParityCondition(ESkillTrigger::OnEnergyAboveThreshold, ECondSubject::SelfTeam, ECondCombine::Or, PARITY_LOW_THRESHOLD)});
				TestEqual(TEXT("mismatches"), CountMismatches(Groups, Check), 0);
			});
		}
	});

	Describe("The legacy trigger fields", [this]()
	{
		It("read across as a Self group with the same answer", [this]()
		{
			TArray<TArray<FSkillCondition>> Groups;
			for (const bool bBoth : {false, true})
			{
				Groups.Add(SkillConditions::FromLegacy(ESkillTrigger::OnHPBelowThreshold, PARITY_HIGH_THRESHOLD, ESkillTrigger::None, 0.0f, bBoth));
				Groups.Add(SkillConditions::FromLegacy(ESkillTrigger::OnHPBelowThreshold, PARITY_HIGH_THRESHOLD,
													   ESkillTrigger::OnEnergyAboveThreshold, PARITY_LOW_THRESHOLD, bBoth));
				Groups.Add(SkillConditions::FromLegacy(ESkillTrigger::OnTurnStart, 0.0f, ESkillTrigger::OnHPAboveThreshold, PARITY_LOW_THRESHOLD, bBoth));
			}
			TestEqual(TEXT("mismatches"), CountMismatches(Groups, ESkillCheck::State), 0);
		});
	});

	Describe("A percent threshold", [this]()
	{
		It("compares exactly at the boundary", [this]()
		{
			constexpr int32 SEVEN = 7;
			constexpr int32 TEN = 10;
			constexpr int32 SEVENTY = 70;
			TestFalse(TEXT("7 of 10 is not below 70%"), WoRConditions::ComparePercent(SEVEN, TEN, EWoRConditionOperator::Below, SEVENTY));
			TestFalse(TEXT("7 of 10 is not above 70%"), WoRConditions::ComparePercent(SEVEN, TEN, EWoRConditionOperator::Above, SEVENTY));
			TestTrue(TEXT("7 of 10 is at most 70%"), WoRConditions::ComparePercent(SEVEN, TEN, EWoRConditionOperator::AtMost, SEVENTY));
			TestTrue(TEXT("no max reads 0%: below 70%"), WoRConditions::ComparePercent(SEVEN, 0, EWoRConditionOperator::Below, SEVENTY));
		});

		It("reads a skill threshold as HpPercent on its side", [this]()
		{
			const FWoRCondition C = SkillConditions::ToCondition(
				ParityCondition(ESkillTrigger::OnHPBelowThreshold, ECondSubject::TargetTeam, ECondCombine::And, PARITY_LOW_THRESHOLD), ESkillCheck::State);
			TestEqual(TEXT("subject"), C.Subject, EWoRConditionSubject::HpPercent);
			TestEqual(TEXT("operator"), C.Operator, EWoRConditionOperator::Below);
			TestEqual(TEXT("who"), C.Who, EWoRConditionWho::TargetTeam);
			TestEqual(TEXT("value"), C.Value, FMath::RoundToInt(PARITY_LOW_THRESHOLD));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
