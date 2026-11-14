// NoRankWordsSpec.cpp
// B-450 (D-187): no enemy rank in player-facing text. The lines this job reworded name no rank ("boss", "mini-boss",
// "elite"), and the wipe result carries the opponent that dealt the last blow, for the wipe card. The rest menu's
// revive rows are Crown's label (Q-450, open) and are not checked here.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Trial/DefeatRules.h"
#include "Trial/TrialRestSpot.h"
#include "UI/Menus/PauseMenuModel.h"

namespace
{
	bool NoRankWordsHasRank(const FText &Text)
	{
		const FString Lower = Text.ToString().ToLower();
		for (const TCHAR *Word : {TEXT("boss"), TEXT("elite"), TEXT("mini-boss")})
		{
			if (Lower.Contains(Word))
			{
				return true;
			}
		}
		return false;
	}
}

BEGIN_DEFINE_SPEC(FNoRankWordsSpec, "WoR.UI.NoRankWords", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FNoRankWordsSpec)

void FNoRankWordsSpec::Define()
{
	It("names no rank in the extract and quit lines (D-187)", [this]()
	{
		TestFalse(TEXT("the extract confirm"), NoRankWordsHasRank(WoRRestSpot::GetLeaveConfirmText()));
		TestFalse(TEXT("the game menu's extract note"), NoRankWordsHasRank(WoRPauseMenu::GetExtractNote()));
		TestFalse(TEXT("the quit warning in a run"), NoRankWordsHasRank(WoRPauseMenu::GetQuitWarning(true)));
	});

	It("carries the foe that dealt the last blow in the wipe result (D-187)", [this]()
	{
		const FText Foe = FText::FromString(TEXT("Ash Warden"));
		const WoRDefeat::FWipeResult Result = WoRDefeat::ApplyWipe(nullptr, EEncounterRank::Boss, {}, Foe);
		TestTrue(TEXT("named, even outside a run"), Result.LastBlowBy.EqualTo(Foe));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
