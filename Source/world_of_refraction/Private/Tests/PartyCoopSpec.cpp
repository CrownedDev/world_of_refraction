// PartyCoopSpec.cpp
// B-360 (D-150): the co-op party. At most three; a joining player takes an AI seat the host picks, and that
// member's gear and pouch go to the host; no free AI seat means the party is full and the join is refused.
// B-360b (D-150 point 2): a character already in the party joins again as a separate copy: its own member id,
// its own pool key, its own claim; a battle pawn finds its slot by the id stamped on it; the save keeps both.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/Entries/FWeaponInventoryEntry.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/RunStateComponent.h"
#include "Player/WoRGameState.h"
#include "Player/WoRPlayerState.h"
#include "Pool/PoolSubsystem.h"
#include "Character/CombatCharacter.h"
#include "Character/CharacterDataComponent.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveTypes.h"

namespace
{
	constexpr int32 COOP_TEST_HOST_ID = 1;
	constexpr int32 COOP_TEST_JOINER_ID = 2;
	constexpr int32 COOP_TEST_STACK = 3;
	const FCrystalId COOP_TEST_GEM(ECrystalType::Garnet, EItemTier::F_Tier);

	FPartyMember CoopMember(UCharacterData *Character, bool bAISeat)
	{
		FPartyMember Member;
		Member.PawnClass = ACombatCharacter::StaticClass(); // a save round trip keeps only a member with a pawn class
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
		FWeaponInventoryEntry Sword;
		Sword.Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Sword.PersistentID = FGuid::NewGuid();
		Pool->AddWeaponToPool(Sword);
		Pool->SetGearOwner(Sword.PersistentID, AIKey);
		Pool->SetWearer(Sword.PersistentID, AIKey);
		TestEqual(TEXT("the AI seat owns the sword"), Pool->GetGearOwner(Sword.PersistentID), AIKey);

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
		TestEqual(TEXT("the host owns the sword now"), Pool->GetGearOwner(Sword.PersistentID), HostKey);
		TestTrue(TEXT("the joiner claims the guest's member (B-360b)"), Party && Joiner->ClaimsMember(Party->Members[1].MemberId));
		TestFalse(TEXT("the host does not"), Party && Host->ClaimsMember(Party->Members[1].MemberId));
		TestTrue(TEXT("the roster followed"), Run->GetRoster().Num() == 2 && Run->GetRoster()[1].Character == Guest && !Run->GetRoster()[1].bAISeat);
	});

	It("refuses a join with no AI seat free (the party is full) or from a player with a seat; the host can refuse", [this]()
	{
		if (!Run || !PartySession || !Host || !Joiner) return;
		Host->ClaimCharacter(Leader);
		TestFalse(TEXT("a player with a seat already (the host) is refused"), Run->RequestJoin(Host, Guest, TSoftClassPtr<ACombatCharacter>()));
		TestTrue(TEXT("the guest asks"), Run->RequestJoin(Joiner, Guest, TSoftClassPtr<ACombatCharacter>()));
		TestTrue(TEXT("the host refuses"), Run->RefuseJoin(COOP_TEST_JOINER_ID));
		TestEqual(TEXT("nothing waits after the refusal"), Run->GetJoinRequests().Num(), 0);
		PartySession->SetMemberAISeat(1, false);
		TestTrue(TEXT("no AI seat left"), Run->GetFreeAISeats().IsEmpty());
		TestFalse(TEXT("full: refused"), Run->RequestJoin(Joiner, Guest, TSoftClassPtr<ACombatCharacter>()));
		TestEqual(TEXT("nothing waits"), Run->GetJoinRequests().Num(), 0);
	});

	It("lets a joiner bring a copy of a character already in the party: its own slot, pool key and claim (B-360b, D-150 point 2)", [this]()
	{
		if (!Run || !PartySession || !Pool || !Host || !Joiner) return;
		const UParty *Party = PartySession->GetLocalParty();
		if (!Party) return;
		const FGuid LeaderId = Party->Members[0].MemberId;
		TestTrue(TEXT("every member has an id"), LeaderId.IsValid() && Party->Members[1].MemberId.IsValid() && LeaderId != Party->Members[1].MemberId);
		TestEqual(TEXT("the first copy keeps the asset-path key"), UPoolSubsystem::GetCharacterKey(Party->Members[0]), UPoolSubsystem::GetCharacterKey(Leader));

		TestTrue(TEXT("the leader's own character is a fine request now"), Run->RequestJoin(Joiner, Leader, TSoftClassPtr<ACombatCharacter>()));
		TestTrue(TEXT("the host gives slot 1"), Run->AcceptJoin(COOP_TEST_JOINER_ID, 1));
		const FPartyMember &Copy = Party->Members[1];
		TestTrue(TEXT("slot 1 is the same character"), Copy.CharacterData == Leader);
		TestTrue(TEXT("as its own member"), Copy.MemberId.IsValid() && Copy.MemberId != LeaderId);
		TestTrue(TEXT("keyed by its member id"), Copy.bKeyedByMember);
		const FName CopyKey = UPoolSubsystem::GetCharacterKey(Copy);
		TestTrue(TEXT("a key of its own"), CopyKey != UPoolSubsystem::GetCharacterKey(Leader) && CopyKey.ToString().Contains(Leader->GetPathName()));
		TestEqual(TEXT("slot by id: the copy"), PartySession->GetMemberSlotById(Copy.MemberId), 1);
		TestEqual(TEXT("slot by data: the first copy"), PartySession->GetMemberSlotByData(Leader), 0);
		TestEqual(TEXT("the second occurrence: the copy"), PartySession->GetNthMemberSlotByData(Leader, 1), 1);

		// The copy's pouch is its own.
		Pool->AddCrystalToPool(COOP_TEST_GEM, COOP_TEST_STACK);
		Pool->CarryOrStore(CopyKey, COOP_TEST_GEM, COOP_TEST_STACK);
		TestEqual(TEXT("the copy carries the gems"), Pool->GetCarriedCount(CopyKey, COOP_TEST_GEM), COOP_TEST_STACK);
		TestEqual(TEXT("the first copy does not"), Pool->GetCarriedCount(UPoolSubsystem::GetCharacterKey(Leader), COOP_TEST_GEM), 0);

		// Claims: the joiner commands its copy, not the host's.
		TestTrue(TEXT("the joiner claims the copy"), Joiner->ClaimsMember(Copy.MemberId));
		TestFalse(TEXT("not the leader's member"), Joiner->ClaimsMember(LeaderId));
		Host->ClaimMember(LeaderId);
		TestTrue(TEXT("the host claims its own"), Host->ClaimsMember(LeaderId));
		Host->ClaimMember(Copy.MemberId);
		TestFalse(TEXT("and cannot take the joiner's copy"), Host->ClaimsMember(Copy.MemberId));

		// A battle pawn stamped with the copy's id stands for the copy; one with no stamp for the first copy.
		AActor *CopyPawn = TestWorld->SpawnCombatant(Leader);
		AActor *HostPawn = TestWorld->SpawnCombatant(Leader);
		UCharacterDataComponent *CopyComp = CopyPawn ? CopyPawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		const UCharacterDataComponent *HostComp = HostPawn ? HostPawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("copy pawn"), CopyComp) || !TestNotNull(TEXT("host pawn"), HostComp)) return;
		CopyComp->PartyMemberId = Copy.MemberId;
		TestEqual(TEXT("the stamped pawn resolves to the copy's slot"), PartySession->GetMemberSlotOf(CopyComp), 1);
		TestEqual(TEXT("and to its key"), UPoolSubsystem::GetCharacterKey(CopyComp), CopyKey);
		TestEqual(TEXT("the unstamped pawn: the first copy's slot"), PartySession->GetMemberSlotOf(HostComp), 0);
		TestTrue(TEXT("the roster tells them apart"), Run->GetRoster().Num() == 2 && Run->GetRoster()[1].MemberId == Copy.MemberId);
		TestEqual(TEXT("the joiner commands the copy's pawn"), Run->GetCommander(CopyPawn), COOP_TEST_JOINER_ID);
		TestEqual(TEXT("the host commands its own"), Run->GetCommander(HostPawn), COOP_TEST_HOST_ID);

		// The save keeps both the id and the key form.
		const FSavedParty Saved = USaveSubsystem::CaptureParty(Party, false);
		if (!TestEqual(TEXT("two saved members"), Saved.Members.Num(), 2)) return;
		TestTrue(TEXT("the copy's id and key form are saved"), Saved.Members[1].MemberId == Copy.MemberId && Saved.Members[1].bKeyedByMember);
		TestTrue(TEXT("the first copy's too"), Saved.Members[0].MemberId == LeaderId && !Saved.Members[0].bKeyedByMember);
		FWoRTestWorld After;
		if (!TestTrue(TEXT("a second world"), After.Create())) return;
		UPartySessionSubsystem *AfterSession = After.GetSubsystem<UPartySessionSubsystem>();
		if (!TestNotNull(TEXT("its party session"), AfterSession) || !TestTrue(TEXT("restored"), USaveSubsystem::RestoreParty(Saved, *AfterSession))) return;
		const UParty *Restored = AfterSession->GetLocalParty();
		if (!Restored || !TestEqual(TEXT("two members back"), Restored->Members.Num(), 2)) return;
		TestTrue(TEXT("the copy is the same member with the same key"), Restored->Members[1].MemberId == Copy.MemberId && UPoolSubsystem::GetCharacterKey(Restored->Members[1]) == CopyKey);
		TestEqual(TEXT("the first copy keeps the plain key"), UPoolSubsystem::GetCharacterKey(Restored->Members[0]), UPoolSubsystem::GetCharacterKey(Leader));
	});

	It("gives two plain copies in a restored party their own keys (a party from before B-360b)", [this]()
	{
		FWoRTestWorld Fresh;
		if (!TestTrue(TEXT("a world"), Fresh.Create())) return;
		UPartySessionSubsystem *Session = Fresh.GetSubsystem<UPartySessionSubsystem>();
		if (!Session) return;
		UCharacterData *Lord = FWoRTestWorld::NewCharacterData();
		Session->RestoreLocalParty(FText::FromString(TEXT("Old")), false, {CoopMember(Lord, false), CoopMember(Lord, true)});
		const UParty *Party = Session->GetLocalParty();
		if (!Party || !TestEqual(TEXT("two members"), Party->Members.Num(), 2)) return;
		TestTrue(TEXT("ids minted"), Party->Members[0].MemberId.IsValid() && Party->Members[1].MemberId.IsValid() && Party->Members[0].MemberId != Party->Members[1].MemberId);
		TestFalse(TEXT("the first keeps the asset-path key"), Party->Members[0].bKeyedByMember);
		TestTrue(TEXT("the second takes its own"), Party->Members[1].bKeyedByMember);
		TestTrue(TEXT("the keys differ"), UPoolSubsystem::GetCharacterKey(Party->Members[0]) != UPoolSubsystem::GetCharacterKey(Party->Members[1]));
		TestTrue(TEXT("an invite of the same character again is a third copy"), Session->InviteMemberWithData(ACombatCharacter::StaticClass(), Lord));
		TestTrue(TEXT("keyed by member"), Party->Members.Num() == 3 && Party->Members[2].bKeyedByMember && Party->Members[2].MemberId.IsValid());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
