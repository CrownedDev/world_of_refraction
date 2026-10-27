// PlayerWalletSpec.cpp
// B-151 (D-046): one wallet per player. Every member of the player's party spends from and
// earns into the wallet on the leader's AWoRPlayerState; an actor with no player behind it
// keeps its own component. B-132: the leader is found again after OpenLevel through its local
// player, and a leader who isn't player 0 still gets the party's money.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyWallet.h"
#include "GameFramework/Pawn.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/WoRPlayerState.h"

namespace
{
	constexpr int32 WALLET_TEST_PRISMS = 100;
	constexpr int32 WALLET_TEST_SPEND = 30;
	constexpr int32 WALLET_TEST_ESSENCE = 12;
}

BEGIN_DEFINE_SPEC(FPlayerWalletSpec, "WoR.Economy.PlayerWallet",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UCurrencyComponent *PlayerWallet = nullptr;
	UCurrencyComponent *PlayerZeroWallet = nullptr;
	AActor *Hero = nullptr;
	AActor *Ally = nullptr;
	AActor *Stranger = nullptr;
	UParty *Party = nullptr;
	APlayerController *LeaderController = nullptr;
END_DEFINE_SPEC(FPlayerWalletSpec)

void FPlayerWalletSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		UWorld *World = TestWorld->GetWorld();
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		// Player 0 is someone else (another local player), so a player-0 lookup would pay them.
		APlayerController *PlayerZero = World ? World->SpawnActor<APlayerController>() : nullptr;
		AWoRPlayerState *PlayerZeroState = World ? World->SpawnActor<AWoRPlayerState>() : nullptr;
		if (PlayerZero && PlayerZeroState)
		{
			PlayerZero->PlayerState = PlayerZeroState;
			PlayerZeroWallet = PlayerZeroState->GetWallet();
		}
		APlayerController *PC = World ? World->SpawnActor<APlayerController>() : nullptr;
		AWoRPlayerState *PlayerState = World ? World->SpawnActor<AWoRPlayerState>() : nullptr;
		Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		TestTrue(TEXT("controller, player state and party"), PC && PlayerState && Party);
		if (!PC || !PlayerState || !Party)
		{
			return;
		}
		PC->PlayerState = PlayerState;
		PC->Player = NewObject<ULocalPlayer>(GEngine); // ULocalPlayer is ClassWithin UEngine
		LeaderController = PC;
		Party->SetLeader(PC);
		PlayerWallet = PlayerState->GetWallet();

		UCharacterData *HeroData = FWoRTestWorld::NewCharacterData();
		UCharacterData *AllyData = FWoRTestWorld::NewCharacterData();
		for (UCharacterData *Data : {HeroData, AllyData})
		{
			FPartyMember Member;
			Member.PawnClass = ACombatCharacter::StaticClass();
			Member.CharacterData = Data;
			Party->Members.Add(Member);
		}
		// The hero is the player's pawn (its PlayerState path); the ally is AI-played (no PlayerState).
		APawn *HeroPawn = Cast<APawn>(TestWorld->SpawnCombatant(HeroData, true));
		if (HeroPawn)
		{
			HeroPawn->SetPlayerState(PlayerState);
		}
		Hero = HeroPawn;
		Ally = TestWorld->SpawnCombatant(AllyData, true);

		Stranger = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		UCurrencyComponent *StrangerWallet = NewObject<UCurrencyComponent>(Stranger);
		Stranger->AddInstanceComponent(StrangerWallet);
		StrangerWallet->RegisterComponent();
	});

	AfterEach([this]()
	{
		PlayerWallet = nullptr;
		PlayerZeroWallet = nullptr;
		Hero = nullptr;
		Ally = nullptr;
		Stranger = nullptr;
		Party = nullptr;
		if (LeaderController)
		{
			LeaderController->Player = nullptr; // the stand-in local player isn't the engine's
			LeaderController = nullptr;
		}
		TestWorld.Reset();
	});

	It("gives every party member the player's one wallet", [this]()
	{
		if (!PlayerWallet) return;
		TestTrue(TEXT("the hero"), CurrencyWallet::FindFor(Hero) == PlayerWallet);
		TestTrue(TEXT("an AI-played ally"), CurrencyWallet::FindFor(Ally) == PlayerWallet);
	});

	It("finds the leader's wallet again when the leader pointer is stale (after OpenLevel), not player 0's", [this]()
	{
		if (!PlayerWallet || !TestNotNull(TEXT("player 0's wallet"), PlayerZeroWallet)) return;
		Party->Leader = nullptr;
		TestTrue(TEXT("the leader's local player's controller"), CurrencyWallet::FindFor(Ally) == PlayerWallet);
	});

	It("pays a party whose leader is not player 0 into that leader's wallet (B-132)", [this]()
	{
		if (!PlayerWallet || !TestNotNull(TEXT("player 0's wallet"), PlayerZeroWallet)) return;
		UCurrencyComponent *AllyWallet = CurrencyWallet::FindFor(Ally);
		if (!TestNotNull(TEXT("the ally's wallet"), AllyWallet)) return;
		TestTrue(TEXT("earned through the ally"), AllyWallet->Add(ECurrencyType::Prisms, WALLET_TEST_PRISMS));
		TestEqual(TEXT("the leader's balance"), PlayerWallet->GetBalance(ECurrencyType::Prisms), WALLET_TEST_PRISMS);
		TestEqual(TEXT("player 0 got nothing"), PlayerZeroWallet->GetBalance(ECurrencyType::Prisms), 0);
	});

	It("lets two characters spend from one balance", [this]()
	{
		if (!PlayerWallet) return;
		TestTrue(TEXT("earned through the hero"), CurrencyWallet::FindFor(Hero)->Add(ECurrencyType::Prisms, WALLET_TEST_PRISMS));
		TestTrue(TEXT("spent through the ally"), CurrencyWallet::FindFor(Ally)->Spend(ECurrencyType::Prisms, WALLET_TEST_SPEND));
		TestEqual(TEXT("one balance"), CurrencyWallet::FindFor(Hero)->GetBalance(ECurrencyType::Prisms), WALLET_TEST_PRISMS - WALLET_TEST_SPEND);
	});

	It("shares the essences too", [this]()
	{
		if (!PlayerWallet) return;
		CurrencyWallet::FindFor(Ally)->AddEssenceType(EEssenceType::Fire, WALLET_TEST_ESSENCE);
		TestEqual(TEXT("seen by the hero"), CurrencyWallet::FindFor(Hero)->GetEssenceType(EEssenceType::Fire), WALLET_TEST_ESSENCE);
	});

	It("keeps an actor with no player behind it on its own wallet", [this]()
	{
		if (!PlayerWallet) return;
		UCurrencyComponent *Own = Stranger->FindComponentByClass<UCurrencyComponent>();
		TestTrue(TEXT("its own component"), CurrencyWallet::FindFor(Stranger) == Own);
		TestNull(TEXT("no actor, no wallet"), CurrencyWallet::FindFor(nullptr));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
