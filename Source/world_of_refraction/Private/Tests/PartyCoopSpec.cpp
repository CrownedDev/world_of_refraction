// PartyCoopSpec.cpp
// B-360 (D-150): the co-op party. At most three; a joining player takes an AI seat the host picks, and that
// member's gear and pouch go to the host; no free AI seat means the party is full and the join is refused;
// a character already in the party is refused (separate copies: B-360b).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/RunStateComponent.h"
#include "Player/WoRGameState.h"
#include "Player/WoRPlayerState.h"
#include "Pool/PoolSubsystem.h"

namespace
{
	constexpr int32 COOP_TEST_HOST_ID = 1;
	constexpr int32 COOP_TEST_JOINER_ID = 2;
	constexpr int32 COOP_TEST_STACK = 3;
	const FCrystalId COOP_TEST_GEM(ECrystalType::Garnet, EItemTier::F_Tier);

	FPartyMember CoopMember(UCharacterData *Character, bool bAISeat)
	{
		FPartyMember Member;
		Member.CharacterData = Character;
		Member.bAISeat = bAISeat;
		Member.bPlayerControlled = !bAISeat;
		return Member;
	}
}

BEGIN_DEFINE_SPEC(FPartyCoopSpec, "WoR.Party.Coop", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AWoRGameState *GameState = nullptr;
	URunStateComponent *Run = nullptr;
	UPartySessionSubsystem *PartySession = nullptr;
	UPoolSubsystem *Pool = nullptr;
	AWoRPlayerState *Host = nullptr;
	AWoRPlayerState *Joiner = nullptr;
	UCharacterData *Leader = nullptr;
	UCharacterData *AIMember = nullptr;
	UCharacterData *Guest = nullptr;

	AWoRPlayerState *SpawnPlayer(int32 PlayerId)
	{
		UWorld *World = TestWorld->GetWorld();
		AWoRPlayerState *Player = World->SpawnActor<AWoRPlayerState>();
		APlayerController *PC = World->SpawnActor<APlayerController>();
		if (!Player || !PC)
		{
			return nullptr;
		}
		if (APlayerState *AutoState = PC->PlayerState; AutoState && AutoState != Player)
		{
			GameState->RemovePlayerState(AutoState);
			AutoState->Destroy();
		}
		Player->SetPlayerId(PlayerId);
		PC->PlayerState = Player;
		GameState->AddPlayerState(Player);
		return Player;
	}
END_DEFINE_SPEC(FPartyCoopSpec)

void FPartyCoopSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		UWorld *World = TestWorld->GetWorld();
		GameState = World ? World->SpawnActor<AWoRGameState>() : nullptr;
		if (!TestNotNull(TEXT("GameState"), GameState)) return;
		World->SetGameState(GameState);
		Run = GameState->GetRunState();
		PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		Host = SpawnPlayer(COOP_TEST_HOST_ID);
		Joiner = SpawnPlayer(COOP_TEST_JOINER_ID);
		Leader = FWoRTestWorld::NewCharacterData();
		AIMember = FWoRTestWorld::NewCharacterData();
		Guest = FWoRTestWorld::NewCharacterData();
		if (PartySession)
		{
			PartySession->RestoreLocalParty(FText::FromString(TEXT("Coop")), false, {CoopMember(Leader, false), CoopMember(AIMember, true)});
		}
		if (Run)
		{
			Run->RefreshRoster();
		}
	});

	AfterEach([this]()
	{
		GameState = nullptr; Run = nullptr; PartySession = nullptr; Pool = nullptr;
		Host = Joiner = nullptr;
		Leader = AIMember = Guest = nullptr;
		TestWorld.Reset();
	});

	It("holds at most three members (D-150)", [this]()
	{
		TestEqual(TEXT("the cap is three"), PartyConstants::MAX_PARTY_MEMBERS, 3);
		if (!PartySession || !PartySession->GetLocalParty()) return;
		TestFalse(TEXT("two members: not full"), PartySession->GetLocalParty()->IsFull());
		PartySession->GetLocalParty()->Members.Add(CoopMember(FWoRTestWorld::NewCharacterData(), true));
		TestTrue(TEXT("three members: full"), PartySession->GetLocalParty()->IsFull());
	});

	It("lets a joiner take the AI seat the host picks; its gear and pouch go to the host; the joiner commands it", [this]()
	{
		if (!Run || !PartySession || !Pool || !Host || !Joiner) return;
		const FName AIKey = UPoolSubsystem::GetCharacterKey(AIMember);
		const FName HostKey = UPoolSubsystem::GetCharacterKey(Leader);
		Pool->AddCrystalToPool(COOP_TEST_GEM, COOP_TEST_STACK);
		Pool->CarryOrStore(AIKey, COOP_TEST_GEM, COOP_TEST_STACK);
		const int32 HostHad = Pool->GetCarriedCount(HostKey, COOP_TEST_GEM) + Pool->GetStoredCount(COOP_TEST_GEM);
		TestEqual(TEXT("the AI seat carries the gems"), Pool->GetCarriedCount(AIKey, COOP_TEST_GEM), COOP_TEST_STACK);

		TestTrue(TEXT("the join request is taken"), Run->RequestJoin(Joiner, Guest, TSoftClassPtr<ACombatCharacter>()));
		TestEqual(TEXT("one request waits"), Run->GetJoinRequests().Num(), 1);
		TestTrue(TEXT("slot 1 is the free AI seat"), Run->GetFreeAISeats() == TArray<int32>({1}));

		TestFalse(TEXT("the leader's slot is refused"), Run->AcceptJoin(COOP_TEST_JOINER_ID, PartyConstants::LEADER_SLOT));
		TestTrue(TEXT("the host gives slot 1"), Run->AcceptJoin(COOP_TEST_JOINER_ID, 1));
		const UParty *Party = PartySession->GetLocalParty();
		TestTrue(TEXT("slot 1 is the guest now"), Party && Party->Members[1].CharacterData == Guest);
		TestFalse(TEXT("and no AI seat"), Party && Party->Members[1].bAISeat);
		TestEqual(TEXT("no request left"), Run->GetJoinRequests().Num(), 0);
		TestEqual(TEXT("the AI seat's pouch is empty"), Pool->GetCarriedCount(AIKey, COOP_TEST_GEM), 0);
		TestEqual(TEXT("the host holds the gems (pouch or storage)"), Pool->GetCarriedCount(HostKey, COOP_TEST_GEM) + Pool->GetStoredCount(COOP_TEST_GEM), HostHad + COOP_TEST_STACK);
		TestTrue(TEXT("the joiner claims the guest"), Joiner->ClaimsCharacter(Guest));
		TestFalse(TEXT("the host does not"), Host->ClaimsCharacter(Guest));
		TestTrue(TEXT("the roster followed"), Run->GetRoster().Num() == 2 && Run->GetRoster()[1].Character == Guest && !Run->GetRoster()[1].bAISeat);
	});

	It("refuses a join with no AI seat free (the party is full) and a character already in the party", [this]()
	{
		if (!Run || !PartySession) return;
		TestFalse(TEXT("the leader's own character is refused (separate copies: B-360b)"), Run->RequestJoin(Joiner, Leader, TSoftClassPtr<ACombatCharacter>()));
		PartySession->SetMemberAISeat(1, false);
		TestTrue(TEXT("no AI seat left"), Run->GetFreeAISeats().IsEmpty());
		TestFalse(TEXT("full: refused"), Run->RequestJoin(Joiner, Guest, TSoftClassPtr<ACombatCharacter>()));
		TestEqual(TEXT("nothing waits"), Run->GetJoinRequests().Num(), 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
