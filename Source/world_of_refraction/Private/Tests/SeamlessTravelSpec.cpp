// SeamlessTravelSpec.cpp
// B-227 (Design/CoopStateOwnership.md #1): levels change by seamless travel on a listen server. The game
// modes allow it and use AWoRGameState; a PlayerState hands its wallet to the next level's one. The travel
// itself needs two players in PIE (docs/PIE/B-227.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyTypes.h"
#include "Player/WoRGameState.h"
#include "Player/WoRPlayerState.h"
#include "Trial/BattleGameMode.h"
#include "Trial/TrialGameMode.h"

namespace
{
	constexpr int32 TRAVEL_TEST_GOLD = 37;
	constexpr int32 TRAVEL_TEST_PRISMS = 120;
	constexpr int32 TRAVEL_TEST_ESSENCE = 9;
	constexpr EEssenceType TRAVEL_TEST_ESSENCE_TYPE = EEssenceType(0);
}

BEGIN_DEFINE_SPEC(FSeamlessTravelSpec, "WoR.Coop.SeamlessTravel", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FSeamlessTravelSpec)

void FSeamlessTravelSpec::Define()
{
	It("has the trial and battle game modes travel seamlessly with AWoRGameState", [this]()
	{
		for (const AGameModeBase *Mode : {static_cast<const AGameModeBase *>(GetDefault<ATrialGameMode>()),
										  static_cast<const AGameModeBase *>(GetDefault<ABattleGameMode>())})
		{
			TestTrue(FString::Printf(TEXT("%s: seamless travel"), *Mode->GetClass()->GetName()), Mode->bUseSeamlessTravel);
			TestTrue(FString::Printf(TEXT("%s: AWoRGameState"), *Mode->GetClass()->GetName()),
					 Mode->GameStateClass == AWoRGameState::StaticClass());
			TestTrue(FString::Printf(TEXT("%s: AWoRPlayerState"), *Mode->GetClass()->GetName()),
					 Mode->PlayerStateClass == AWoRPlayerState::StaticClass());
		}
	});

	It("hands the wallet to the next level's PlayerState", [this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		if (!TestTrue(TEXT("Test world created"), TestWorld->Create())) return;
		UWorld *World = TestWorld->GetWorld();
		AWoRPlayerState *Old = World->SpawnActor<AWoRPlayerState>();
		AWoRPlayerState *Next = World->SpawnActor<AWoRPlayerState>();
		if (!TestNotNull(TEXT("Old"), Old) || !TestNotNull(TEXT("Next"), Next)) return;
		UCurrencyComponent *Wallet = Old->GetWallet();
		Wallet->Add(ECurrencyType::Gold, TRAVEL_TEST_GOLD);
		Wallet->Add(ECurrencyType::Prisms, TRAVEL_TEST_PRISMS);
		Wallet->AddEssenceType(TRAVEL_TEST_ESSENCE_TYPE, TRAVEL_TEST_ESSENCE);

		Old->CopyProperties(Next);
		const UCurrencyComponent *Carried = Next->GetWallet();
		TestEqual(TEXT("Gold"), Carried->GetBalance(ECurrencyType::Gold), TRAVEL_TEST_GOLD);
		TestEqual(TEXT("Prisms"), Carried->GetBalance(ECurrencyType::Prisms), TRAVEL_TEST_PRISMS);
		TestEqual(TEXT("Essence"), Carried->GetEssenceType(TRAVEL_TEST_ESSENCE_TYPE), TRAVEL_TEST_ESSENCE);
		TestWorld.Reset();
	});
}

#endif
