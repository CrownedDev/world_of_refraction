// DifficultyAITierSpec.cpp
// B-344 (D-136, D-044): the trial difficulty sets the enemy AI tier; an encounter's own tier wins only when it
// overrides.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Trial/TrialGate.h"

BEGIN_DEFINE_SPEC(FDifficultyAITierSpec, "WoR.Trial.DifficultyAITier",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDifficultyAITierSpec)

void FDifficultyAITierSpec::Define()
{
	It("maps Easy / Medium / Hard to the same AI tier", [this]()
	{
		TestTrue(TEXT("Easy"), TrialGate::GetEnemyAIDifficulty(ETrialDifficulty::Easy) == EAIDifficulty::Easy);
		TestTrue(TEXT("Medium"), TrialGate::GetEnemyAIDifficulty(ETrialDifficulty::Medium) == EAIDifficulty::Medium);
		TestTrue(TEXT("Hard"), TrialGate::GetEnemyAIDifficulty(ETrialDifficulty::Hard) == EAIDifficulty::Hard);
	});

	It("uses the trial's tier unless the encounter overrides it", [this]()
	{
		TestTrue(TEXT("no override: the trial's"),
				 TrialGate::ResolveEncounterAIDifficulty(ETrialDifficulty::Hard, false, EAIDifficulty::Easy) == EAIDifficulty::Hard);
		TestTrue(TEXT("override: the encounter's"),
				 TrialGate::ResolveEncounterAIDifficulty(ETrialDifficulty::Hard, true, EAIDifficulty::Easy) == EAIDifficulty::Easy);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
