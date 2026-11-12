// BossFramingSpec.cpp
// B-405 (study R13, R14; Q-405): the camera height a boss gets in the turn view, and the kill shot chosen by
// EEncounterRank. Pure rules: no world.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Camera/BossFraming.h"
#include "Combat/Camera/CombatCameraTypes.h"

namespace
{
	/** Viewer's character at the origin, enemies 1000 cm ahead on +X, everyone on one floor. */
	FCombatCameraFrame BossFramingTestFrame()
	{
		FCombatCameraFrame Frame;
		Frame.Focus = FVector::ZeroVector;
		Frame.TowardEnemies = FVector::ForwardVector;
		Frame.EnemyCentre = FVector(1000.0f, 0.0f, 0.0f);
		Frame.ArenaCentre = FVector(500.0f, 0.0f, 0.0f);
		return Frame;
	}

	/** The turn view for Style against an enemy of Rank, as the director builds it. */
	FTransform BossFramingTurnView(ETurnViewStyle Style, EEncounterRank Rank)
	{
		const FCombatCameraFrame Frame = BossFramingTestFrame();
		const EEnemyCameraHeight Height = FBossFramingRules::ResolveHeight(EEnemyCameraHeight::ByRank, Rank);
		return FBossFramingRules::ApplyHeight(FCombatCameraRules::ComputeView(ECombatCameraMode::Turn, Style, Frame), Height, Style, Frame);
	}
}

BEGIN_DEFINE_SPEC(FBossFramingSpec, "WoR.Combat.Camera.BossFraming",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FBossFramingSpec)

void FBossFramingSpec::Define()
{
	Describe("Camera height (R13)", [this]()
	{
		It("puts bosses and mini-bosses low and normal enemies at eye level", [this]()
		{
			TestEqual(TEXT("boss"), FBossFramingRules::ResolveHeight(EEnemyCameraHeight::ByRank, EEncounterRank::Boss), EEnemyCameraHeight::Low);
			TestEqual(TEXT("mini-boss"), FBossFramingRules::ResolveHeight(EEnemyCameraHeight::ByRank, EEncounterRank::MiniBoss), EEnemyCameraHeight::Low);
			TestEqual(TEXT("normal"), FBossFramingRules::ResolveHeight(EEnemyCameraHeight::ByRank, EEncounterRank::Normal), EEnemyCameraHeight::EyeLevel);
			TestEqual(TEXT("elite"), FBossFramingRules::ResolveHeight(EEnemyCameraHeight::ByRank, EEncounterRank::Elite), EEnemyCameraHeight::EyeLevel);
		});

		It("lets an enemy's own data override its rank", [this]()
		{
			TestEqual(TEXT("tall normal enemy"), FBossFramingRules::ResolveHeight(EEnemyCameraHeight::Low, EEncounterRank::Normal), EEnemyCameraHeight::Low);
			TestEqual(TEXT("small boss"), FBossFramingRules::ResolveHeight(EEnemyCameraHeight::EyeLevel, EEncounterRank::Boss), EEnemyCameraHeight::EyeLevel);
		});

		It("frames the fight low while any living enemy is low", [this]()
		{
			TestEqual(TEXT("boss among minions"), FBossFramingRules::FightHeight({EEnemyCameraHeight::EyeLevel, EEnemyCameraHeight::Low}), EEnemyCameraHeight::Low);
			TestEqual(TEXT("minions only"), FBossFramingRules::FightHeight({EEnemyCameraHeight::EyeLevel}), EEnemyCameraHeight::EyeLevel);
			TestEqual(TEXT("nobody left"), FBossFramingRules::FightHeight({}), EEnemyCameraHeight::EyeLevel);
		});

		It("gives a boss a lower, upward-looking turn view than a normal enemy", [this]()
		{
			for (const ETurnViewStyle Style : {ETurnViewStyle::CentredWide, ETurnViewStyle::OverTheShoulder})
			{
				const FString Name = UEnum::GetValueAsString(Style);
				const FTransform Normal = BossFramingTurnView(Style, EEncounterRank::Normal);
				const FTransform Boss = BossFramingTurnView(Style, EEncounterRank::Boss);
				TestTrue(*(Name + TEXT(": boss camera lower")), Boss.GetLocation().Z < Normal.GetLocation().Z);
				TestTrue(*(Name + TEXT(": boss camera looks up")), Boss.GetRotation().Rotator().Pitch > 0.0f);
				TestTrue(*(Name + TEXT(": normal camera doesn't look up")), Normal.GetRotation().Rotator().Pitch <= 0.0f);
				TestTrue(*(Name + TEXT(": still faces the enemies")), Boss.GetRotation().GetForwardVector().X > 0.0f);
			}
		});

		It("leaves a normal fight's turn view and the tactical overview unchanged", [this]()
		{
			const FCombatCameraFrame Frame = BossFramingTestFrame();
			const FTransform Wide = FCombatCameraRules::ComputeView(ECombatCameraMode::Turn, ETurnViewStyle::CentredWide, Frame);
			TestTrue(TEXT("eye level: unchanged"), BossFramingTurnView(ETurnViewStyle::CentredWide, EEncounterRank::Normal).Equals(Wide));
			const FTransform Tactical = FCombatCameraRules::ComputeView(ECombatCameraMode::Turn, ETurnViewStyle::Tactical, Frame);
			TestTrue(TEXT("tactical: unchanged"), BossFramingTurnView(ETurnViewStyle::Tactical, EEncounterRank::Boss).Equals(Tactical));
		});
	});

	Describe("Kill shots (R14)", [this]()
	{
		It("is chosen by EEncounterRank", [this]()
		{
			TestEqual(TEXT("normal"), FBossFramingRules::KillShotFor(EEncounterRank::Normal), EKillShotKind::Normal);
			TestEqual(TEXT("elite"), FBossFramingRules::KillShotFor(EEncounterRank::Elite), EKillShotKind::Normal);
			TestEqual(TEXT("mini-boss"), FBossFramingRules::KillShotFor(EEncounterRank::MiniBoss), EKillShotKind::Boss);
			TestEqual(TEXT("boss"), FBossFramingRules::KillShotFor(EEncounterRank::Boss), EKillShotKind::Boss);
		});

		It("holds a normal kill briefly and a boss kill longer, breaking apart inside its hold", [this]()
		{
			const float Normal = FBossFramingRules::KillShotSeconds(EKillShotKind::Normal);
			const float Boss = FBossFramingRules::KillShotSeconds(EKillShotKind::Boss);
			TestTrue(TEXT("normal holds"), Normal > 0.0f);
			TestTrue(TEXT("boss holds longer"), Boss > Normal);
			TestTrue(TEXT("break-apart inside the boss hold"), FBossFramingRules::BreakApartDelaySeconds < Boss);
			TestTrue(TEXT("only the boss breaks apart"), FBossFramingRules::BreaksApart(EKillShotKind::Boss) && !FBossFramingRules::BreaksApart(EKillShotKind::Normal));
			TestTrue(TEXT("only the normal kill darkens"), FBossFramingRules::DarkensView(EKillShotKind::Normal) && !FBossFramingRules::DarkensView(EKillShotKind::Boss));
		});

		It("frames a normal kill wide and a boss kill close and low, both on the fallen enemy", [this]()
		{
			const FCombatCameraFrame Frame = BossFramingTestFrame();
			const FVector Victim = Frame.EnemyCentre;
			const FTransform Wide = FBossFramingRules::ComputeKillShot(EKillShotKind::Normal, Frame, Victim);
			const FTransform Close = FBossFramingRules::ComputeKillShot(EKillShotKind::Boss, Frame, Victim);
			TestTrue(TEXT("boss shot closer"), FVector::Dist(Close.GetLocation(), Victim) < FVector::Dist(Wide.GetLocation(), Victim));
			TestTrue(TEXT("boss shot looks up"), Close.GetRotation().Rotator().Pitch > 0.0f);
			TestTrue(TEXT("wide shot looks down"), Wide.GetRotation().Rotator().Pitch < 0.0f);
			TestTrue(TEXT("wide shot from the viewer's side"), Wide.GetLocation().X < Victim.X);
			TestTrue(TEXT("both face the enemy"), Wide.GetRotation().GetForwardVector().X > 0.0f && Close.GetRotation().GetForwardVector().X > 0.0f);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
