// TrialGateSpec.cpp
// B-183 (Q-183): the trial door's difficulty scales enemy strength and the payout through one
// multiplier per level, the run holds the gate choice, and an enemy given the level's strength
// starts the fight with the scaled max HP, full.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialRunSubsystem.h"

BEGIN_DEFINE_SPEC(FTrialGateSpec, "WoR.Trial.Gate",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	UCharacterDataComponent *SpawnEnemy()
	{
		AActor *Enemy = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		return Enemy ? Enemy->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	}
END_DEFINE_SPEC(FTrialGateSpec)

void FTrialGateSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("pays more and makes enemies stronger at each harder level, with Medium unchanged", [this]()
	{
		using namespace TrialGate;
		TestEqual(TEXT("Medium strength"), GetEnemyStrengthMultiplier(ETrialDifficulty::Medium), 1.0f);
		TestEqual(TEXT("Medium payout"), GetPayoutMultiplier(ETrialDifficulty::Medium), 1.0f);
		TestTrue(TEXT("Easy enemies are weaker"), GetEnemyStrengthMultiplier(ETrialDifficulty::Easy) < 1.0f);
		TestTrue(TEXT("Hard enemies are stronger"), GetEnemyStrengthMultiplier(ETrialDifficulty::Hard) > 1.0f);
		TestTrue(TEXT("Easy pays less"), GetPayoutMultiplier(ETrialDifficulty::Easy) < 1.0f);
		TestTrue(TEXT("Hard pays more"), GetPayoutMultiplier(ETrialDifficulty::Hard) > 1.0f);
		TestEqual(TEXT("Cycling wraps Hard to Easy"), static_cast<int32>(NextDifficulty(ETrialDifficulty::Hard)), static_cast<int32>(ETrialDifficulty::Easy));
		TestEqual(TEXT("Cycling steps Easy to Medium"), static_cast<int32>(NextDifficulty(ETrialDifficulty::Easy)), static_cast<int32>(ETrialDifficulty::Medium));
	});

	It("holds the gate choice for the run", [this]()
	{
		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("TrialRun"), TrialRun)) return;
		TestEqual(TEXT("Defaults to Medium"), static_cast<int32>(TrialRun->GetTrialDifficulty()), static_cast<int32>(TrialGate::DEFAULT_DIFFICULTY));
		TestEqual(TEXT("No variation chosen"), TrialRun->GetVariationIndex(), static_cast<int32>(INDEX_NONE));

		TrialRun->SetGateChoice(1, ETrialDifficulty::Hard);
		TestEqual(TEXT("Difficulty stored"), static_cast<int32>(TrialRun->GetTrialDifficulty()), static_cast<int32>(ETrialDifficulty::Hard));
		TestEqual(TEXT("Variation stored"), TrialRun->GetVariationIndex(), 1);
		TestEqual(TEXT("Payout follows the level"), TrialRun->GetPayoutMultiplier(),
				  TrialGate::GetPayoutMultiplier(ETrialDifficulty::Hard));
		TestEqual(TEXT("Strength follows the level"), TrialRun->GetEnemyStrengthMultiplier(),
				  TrialGate::GetEnemyStrengthMultiplier(ETrialDifficulty::Hard));
		TestNull(TEXT("No variation without an active trial"), TrialRun->GetChosenVariation());
		TestTrue(TEXT("The debug line shows the level"), TrialRun->GetTrialRunString().Contains(TEXT("difficulty=Hard")));
	});

	It("gives an enemy the level's max HP, full, and leaves Medium untouched", [this]()
	{
		UCharacterDataComponent *Hard = SpawnEnemy();
		UCharacterDataComponent *Medium = SpawnEnemy();
		UCharacterDataComponent *Easy = SpawnEnemy();
		if (!TestTrue(TEXT("Enemies spawned"), Hard && Medium && Easy)) return;
		const int32 BaseMaxHP = Medium->MaxHP;
		if (!TestTrue(TEXT("Enemy has HP"), BaseMaxHP > 0)) return;

		TestTrue(TEXT("Hard applied"), TrialGate::ApplyEnemyStrength(Hard->GetOwner(),
			TrialGate::GetEnemyStrengthMultiplier(ETrialDifficulty::Hard)));
		TestTrue(TEXT("Medium applied"), TrialGate::ApplyEnemyStrength(Medium->GetOwner(),
			TrialGate::GetEnemyStrengthMultiplier(ETrialDifficulty::Medium)));
		TestTrue(TEXT("Easy applied"), TrialGate::ApplyEnemyStrength(Easy->GetOwner(),
			TrialGate::GetEnemyStrengthMultiplier(ETrialDifficulty::Easy)));

		TestTrue(TEXT("Hard raises max HP"), Hard->MaxHP > BaseMaxHP);
		TestEqual(TEXT("Hard starts full"), Hard->CurrentHP, Hard->MaxHP);
		TestEqual(TEXT("Medium unchanged"), Medium->MaxHP, BaseMaxHP);
		TestTrue(TEXT("Easy lowers max HP"), Easy->MaxHP < BaseMaxHP);
		TestEqual(TEXT("Easy starts full"), Easy->CurrentHP, Easy->MaxHP);
		TestFalse(TEXT("No enemy, nothing applied"), TrialGate::ApplyEnemyStrength(nullptr, 2.0f));
	});
}

#endif
