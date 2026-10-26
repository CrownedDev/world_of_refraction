// PlayerWalletSpec.cpp
// B-151 (D-046): one wallet per player. Every member of the player's party spends from and
// earns into the wallet on the leader's AWoRPlayerState; an actor with no player behind it
// keeps its own component.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyWallet.h"
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
	AActor *Hero = nullptr;
	AActor *Ally = nullptr;
	AActor *Stranger = nullptr;
END_DEFINE_SPEC(FPlayerWalletSpec)

void FPlayerWalletSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		UWorld *World = TestWorld->GetWorld();
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		APlayerController *PC = World ? World->SpawnActor<APlayerController>() : nullptr;
		AWoRPlayerState *PlayerState = World ? World->SpawnActor<AWoRPlayerState>() : nullptr;
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		TestTrue(TEXT("controller, player state and party"), PC && PlayerState && Party);
		if (!PC || !PlayerState || !Party)
		{
			return;
		}
		PC->PlayerState = PlayerState;
		Party->Leader = PC;
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
		Hero = TestWorld->SpawnCombatant(HeroData);
		Ally = TestWorld->SpawnCombatant(AllyData, true);

		Stranger = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		UCurrencyComponent *StrangerWallet = NewObject<UCurrencyComponent>(Stranger);
		Stranger->AddInstanceComponent(StrangerWallet);
		StrangerWallet->RegisterComponent();
	});

	AfterEach([this]()
	{
		PlayerWallet = nullptr;
		Hero = nullptr;
		Ally = nullptr;
		Stranger = nullptr;
		TestWorld.Reset();
	});

	It("gives every party member the player's one wallet", [this]()
	{
		if (!PlayerWallet) return;
		TestTrue(TEXT("the hero"), CurrencyWallet::FindFor(Hero) == PlayerWallet);
		TestTrue(TEXT("an AI-played ally"), CurrencyWallet::FindFor(Ally) == PlayerWallet);
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
