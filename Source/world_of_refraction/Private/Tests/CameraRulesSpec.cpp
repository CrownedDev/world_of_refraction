// CameraRulesSpec.cpp
// D-255 (B-583): the hub and trial look at most 10 up and 55 down; from contact the engaged player's look stays within 30
// of yaw and 10 of pitch of where it pointed and its move input is ignored; a re-arm gives both back. The clamps run
// through the engine's own APlayerCameraManager::ProcessViewRotation, as a mouse or stick turn does.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Combat/Encounter/EncounterComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Player/WoRCameraRules.h"

namespace
{
	constexpr float CAMERA_TEST_TOLERANCE = 0.01f;
	/** A turn far past any limit. */
	constexpr float CAMERA_TEST_BIG_TURN = 80.0f;
	/** Looking just left of north: the contact range crosses 0 / 360. */
	constexpr float CAMERA_TEST_NEAR_NORTH_YAW = 350.0f;
	/** Test-world ticks are 0.01s: well past the 2s join window. */
	constexpr int32 CAMERA_TEST_PAST_JOIN_WINDOW_FRAMES = 400;
}

BEGIN_DEFINE_SPEC(FCameraRulesSpec, "WoR.Field.CameraRules",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	APlayerController *Player = nullptr;

	/** The view rotation after turning by Delta from Start, clamped as the engine clamps a player's turn. */
	FRotator Turn(const FRotator &Start, const FRotator &Delta) const
	{
		FRotator View = Start + Delta;
		FRotator Remaining = FRotator::ZeroRotator;
		Player->PlayerCameraManager->ProcessViewRotation(0.0f, View, Remaining);
		return View;
	}

	void TestAngle(const TCHAR *What, float Actual, float Expected)
	{
		TestTrue(What, FMath::IsNearlyZero(FRotator::NormalizeAxis(Actual - Expected), CAMERA_TEST_TOLERANCE));
	}
END_DEFINE_SPEC(FCameraRulesSpec)

void FCameraRulesSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Player = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<APlayerController>() : nullptr;
		TestTrue(TEXT("a player with a camera manager"), Player && Player->PlayerCameraManager);
	});

	AfterEach([this]()
	{
		Player = nullptr;
		TestWorld.Reset();
	});

	It("limits the hub and trial look to 10 up and 55 down, yaw free (D-255 a)", [this]()
	{
		if (!Player || !Player->PlayerCameraManager)
		{
			return;
		}
		WoRCameraRules::ApplyFieldLimits(*Player->PlayerCameraManager);
		TestAngle(TEXT("up stops at 10"), Turn(FRotator::ZeroRotator, FRotator(CAMERA_TEST_BIG_TURN, 0.0f, 0.0f)).Pitch, WoRCameraRules::FIELD_PITCH_UP_MAX);
		TestAngle(TEXT("down stops at 55"), Turn(FRotator::ZeroRotator, FRotator(-CAMERA_TEST_BIG_TURN, 0.0f, 0.0f)).Pitch, -WoRCameraRules::FIELD_PITCH_DOWN_MAX);
		TestAngle(TEXT("yaw turns freely"), Turn(FRotator::ZeroRotator, FRotator(0.0f, 170.0f, 0.0f)).Yaw, 170.0f);
	});

	It("holds the look within 30 yaw and 10 pitch of contact, across 0/360 (D-255 b)", [this]()
	{
		if (!Player || !Player->PlayerCameraManager)
		{
			return;
		}
		WoRCameraRules::ApplyFieldLimits(*Player->PlayerCameraManager);
		const FRotator Look(-5.0f, CAMERA_TEST_NEAR_NORTH_YAW, 0.0f);
		WoRCameraRules::Apply(*Player->PlayerCameraManager,
							  WoRCameraRules::MakeContactLimits(Look, WoRCameraRules::Read(*Player->PlayerCameraManager)));
		TestAngle(TEXT("right past north stops at 20"), Turn(Look, FRotator(0.0f, CAMERA_TEST_BIG_TURN, 0.0f)).Yaw, 20.0f);
		TestAngle(TEXT("left stops at 320"), Turn(Look, FRotator(0.0f, -CAMERA_TEST_BIG_TURN, 0.0f)).Yaw, 320.0f);
		TestAngle(TEXT("a small turn across north is free"), Turn(Look, FRotator(0.0f, 15.0f, 0.0f)).Yaw, 5.0f);
		TestAngle(TEXT("up stops 10 above"), Turn(Look, FRotator(CAMERA_TEST_BIG_TURN, 0.0f, 0.0f)).Pitch, 5.0f);
		TestAngle(TEXT("down stops 10 below"), Turn(Look, FRotator(-CAMERA_TEST_BIG_TURN, 0.0f, 0.0f)).Pitch, -15.0f);

		const WoRCameraRules::FLookLimits NearTop =
			WoRCameraRules::MakeContactLimits(FRotator(8.0f, 0.0f, 0.0f), WoRCameraRules::FLookLimits());
		TestEqual(TEXT("inside the field's 10 up"), NearTop.PitchMax, WoRCameraRules::FIELD_PITCH_UP_MAX);
	});

	It("stops the engaged player and gives move and look back on release, the wheel's own ignore kept", [this]()
	{
		AActor *Owner = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<AActor>() : nullptr;
		UEncounterComponent *Encounter = Owner ? FWoRTestWorld::AddComponent<UEncounterComponent>(Owner) : nullptr;
		if (!TestTrue(TEXT("setup"), Player && Player->PlayerCameraManager && Encounter))
		{
			return;
		}
		WoRCameraRules::ApplyFieldLimits(*Player->PlayerCameraManager);
		const WoRCameraRules::FLookLimits Before = WoRCameraRules::Read(*Player->PlayerCameraManager);
		Player->SetIgnoreMoveInput(true); // the field wheel's own hold, still open
		Encounter->HoldEngagedPlayer(Player);
		Encounter->HoldEngagedPlayer(Player); // a second contact doesn't stack
		TestTrue(TEXT("held"), Encounter->IsHoldingEngagedPlayer());
		TestTrue(TEXT("move ignored"), Player->IsMoveInputIgnored());
		TestTrue(TEXT("yaw limited"), Player->PlayerCameraManager->ViewYawMax - Player->PlayerCameraManager->ViewYawMin < 360.0f - 1.0f);

		Encounter->ReleaseEngagedPlayer();
		TestFalse(TEXT("released"), Encounter->IsHoldingEngagedPlayer());
		TestTrue(TEXT("the wheel's hold still stands"), Player->IsMoveInputIgnored());
		Player->SetIgnoreMoveInput(false);
		TestFalse(TEXT("then move is free"), Player->IsMoveInputIgnored());
		const WoRCameraRules::FLookLimits After = WoRCameraRules::Read(*Player->PlayerCameraManager);
		TestTrue(TEXT("the limits are as before"), After.PitchMin == Before.PitchMin && After.PitchMax == Before.PitchMax &&
													   After.YawMin == Before.YawMin && After.YawMax == Before.YawMax);
	});

	It("holds from a field hit and gives both back when the fight can't start (re-arm, D-255 d)", [this]()
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->Origin = ECharacterOrigin::Player;
		// The trial party holds this character (contact only engages a party member), as the other party specs build it.
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("party"), Party))
		{
			return;
		}
		Party->Members.Empty();
		FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
		Member.PawnClass = ACombatCharacter::StaticClass();
		Member.CharacterData = Data;
		APawn *Pawn = Cast<APawn>(TestWorld->SpawnCombatant(Data, /*bAIControlled=*/true));
		AActor *Owner = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<AActor>() : nullptr;
		UEncounterComponent *Encounter = Owner ? FWoRTestWorld::AddComponent<UEncounterComponent>(Owner) : nullptr;
		if (!TestTrue(TEXT("setup"), Player && Player->PlayerCameraManager && Pawn && Encounter && TestWorld->GetWorld()->GetGameInstance()))
		{
			return;
		}
		Player->Possess(Pawn);
		WoRCameraRules::ApplyFieldLimits(*Player->PlayerCameraManager);
		const WoRCameraRules::FLookLimits Before = WoRCameraRules::Read(*Player->PlayerCameraManager);

		if (!TestTrue(TEXT("the field hit engages"), Encounter->StrikeFromField(Pawn, FFieldStrike())))
		{
			return;
		}
		TestTrue(TEXT("held from contact"), Encounter->IsHoldingEngagedPlayer() && Player->IsMoveInputIgnored());

		TestWorld->Tick(CAMERA_TEST_PAST_JOIN_WINDOW_FRAMES); // no trial runs here, so the window closes into a re-arm
		TestFalse(TEXT("released on re-arm"), Encounter->IsHoldingEngagedPlayer());
		TestFalse(TEXT("move given back"), Player->IsMoveInputIgnored());
		const WoRCameraRules::FLookLimits After = WoRCameraRules::Read(*Player->PlayerCameraManager);
		TestTrue(TEXT("look limits given back"), After.PitchMin == Before.PitchMin && After.PitchMax == Before.PitchMax &&
													 After.YawMin == Before.YawMin && After.YawMax == Before.YawMax);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
