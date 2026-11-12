// HostSuccessionSpec.cpp
// B-234b part 1 (Q-234b): who hosts after the host drops (the next player in join order), which failures start a
// hand-over, and a new host adopting the checkpoint copy (its own slot is never written). The drop itself, the take-over
// and the rejoin are the 3-player PIE sheet. B-234d (Q-234d): after the handed-over run the pool and party are the
// player's own again; the join order survives a seamless travel (simulated: new GameState, PlayerStates copied).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Kismet/GameplayStatics.h"
#include "Online/HostSuccession.h"
#include "OnlineSubsystemTypes.h" // FUniqueNetIdString (B-234d: test players' ids)
#include "Equipment/Crystals/CrystalType.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Player/WoRGameState.h"
#include "Player/WoRPlayerState.h"
#include "Pool/PoolSubsystem.h"
#include "Save/CoopCheckpoint.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveGame.h"

namespace
{
	const FString SUCCESSION_TEST_HOST(TEXT("host-id"));
	const FString SUCCESSION_TEST_SECOND(TEXT("second-id"));
	const FString SUCCESSION_TEST_THIRD(TEXT("third-id"));
	const FString SUCCESSION_TEST_FOURTH(TEXT("fourth-id"));
	constexpr int32 SUCCESSION_TEST_USER = 0;
	/** Review: the spec's own slot, never the real checkpoint copy (WoRCheckpoint::SLOT_NAME) a player keeps. */
	const FString SUCCESSION_TEST_SLOT(TEXT("WoRTestHandOverCopy"));
	/** B-234d: the spec's stand-in for the player's own save (never USaveSubsystem::SLOT_NAME). */
	const FString SUCCESSION_TEST_OWN_SLOT(TEXT("WoRTestHandOverOwn"));
	/** The online subsystem type the test ids claim (the Null subsystem's name). */
	const FName SUCCESSION_TEST_NET_TYPE(TEXT("NULL"));
	constexpr int32 SUCCESSION_TEST_OWN_GARNETS = 3;
	constexpr int32 SUCCESSION_TEST_COPY_SAPPHIRES = 5;
	/** B-234d review: upgrade-memory keys (definition keys; any name does for the merge). */
	const FName SUCCESSION_TEST_DEF_OWN(TEXT("TestDef_Own"));
	const FName SUCCESSION_TEST_DEF_OLD(TEXT("TestDef_OldHost"));
	const FName SUCCESSION_TEST_DEF_RAISED(TEXT("TestDef_Raised"));
	const FName SUCCESSION_TEST_DEF_NEW(TEXT("TestDef_New"));

	const FCrystalId OwnCrystal() { return FCrystalId(ECrystalType::Garnet, EItemTier::F_Tier); }
	const FCrystalId CopyCrystal() { return FCrystalId(ECrystalType::Sapphire, EItemTier::F_Tier); }

	int32 CountInPool(const UPoolSubsystem &Pool, const FCrystalId &Id)
	{
		return Pool.GetCrystals().FindRef(Id) + Pool.GetStones().FindRef(Id);
	}

	FSavedCrystalStack SavedStack(const FCrystalId &Id, int32 Count)
	{
		FSavedCrystalStack Stack;
		Stack.Type = static_cast<uint8>(Id.Type);
		Stack.Tier = Id.Tier;
		Stack.Count = Count;
		return Stack;
	}

	/** A save stamped with the current version (else the load check wipes it, D-060) holding Stack in its pool. */
	bool WriteHandOverTestSave(const FString &Slot, const FSavedCrystalStack &Stack, bool bHasRun)
	{
		UWoRSaveGame *Save = Cast<UWoRSaveGame>(UGameplayStatics::CreateSaveGameObject(UWoRSaveGame::StaticClass()));
		if (!Save)
		{
			return false;
		}
		Save->HandlePreSave();
		Save->Data.Pool.Crystals.Add(Stack);
		Save->Data.Run.bHasRun = bHasRun;
		return UGameplayStatics::SaveGameToSlot(Save, Slot, SUCCESSION_TEST_USER);
	}

	/** Logs Player in with Id, as the server's login does (its OnSetUniqueId updates the join order). */
	void LogInTestPlayer(APlayerState *Player, const FString &Id)
	{
		const FUniqueNetIdRef NetId = FUniqueNetIdString::Create(Id, SUCCESSION_TEST_NET_TYPE);
		Player->SetUniqueId(FUniqueNetIdRepl(NetId));
	}

	FString JoinOrderString(const TArray<FString> &Order)
	{
		return FString::Join(Order, TEXT(","));
	}
}

BEGIN_DEFINE_SPEC(FHostSuccessionSpec, "WoR.Coop.HostSuccession", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FHostSuccessionSpec)

void FHostSuccessionSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		UGameplayStatics::DeleteGameInSlot(SUCCESSION_TEST_SLOT, SUCCESSION_TEST_USER);
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
		UGameplayStatics::DeleteGameInSlot(SUCCESSION_TEST_SLOT, SUCCESSION_TEST_USER);
	});

	It("picks the next player in join order after the old host", [this]()
	{
		const TArray<FString> Order = {SUCCESSION_TEST_HOST, SUCCESSION_TEST_SECOND, SUCCESSION_TEST_THIRD};
		TestEqual(TEXT("the second joiner"), WoRHostSuccession::PickNextHost(Order, SUCCESSION_TEST_HOST), SUCCESSION_TEST_SECOND);
		TestEqual(TEXT("the old host is skipped wherever it is"), WoRHostSuccession::PickNextHost({SUCCESSION_TEST_SECOND, SUCCESSION_TEST_HOST}, SUCCESSION_TEST_SECOND), SUCCESSION_TEST_HOST);
		TestTrue(TEXT("nobody left"), WoRHostSuccession::PickNextHost({SUCCESSION_TEST_HOST}, SUCCESSION_TEST_HOST).IsEmpty());
		TestEqual(TEXT("an empty id is skipped"), WoRHostSuccession::PickNextHost({FString(), SUCCESSION_TEST_THIRD}, SUCCESSION_TEST_HOST), SUCCESSION_TEST_THIRD);
	});

	It("only a lost connection or a timeout starts a hand-over", [this]()
	{
		TestTrue(TEXT("lost"), WoRHostSuccession::IsHostLoss(ENetworkFailure::ConnectionLost));
		TestTrue(TEXT("timeout"), WoRHostSuccession::IsHostLoss(ENetworkFailure::ConnectionTimeout));
		TestFalse(TEXT("a refused join"), WoRHostSuccession::IsHostLoss(ENetworkFailure::PendingConnectionFailure));
		TestFalse(TEXT("a version mismatch"), WoRHostSuccession::IsHostLoss(ENetworkFailure::OutdatedClient));
	});

	It("B-234c: a planned hand-over needs a listen server and another player", [this]()
	{
		TestTrue(TEXT("host with a friend"), WoRHostSuccession::CanHandOverFrom(NM_ListenServer, 2));
		TestFalse(TEXT("host alone"), WoRHostSuccession::CanHandOverFrom(NM_ListenServer, 1));
		TestFalse(TEXT("a client"), WoRHostSuccession::CanHandOverFrom(NM_Client, 3));
		TestFalse(TEXT("solo"), WoRHostSuccession::CanHandOverFrom(NM_Standalone, 2));
		UWoRHostHandOverSubsystem *HandOver = TestWorld->GetSubsystem<UWoRHostHandOverSubsystem>();
		if (!TestNotNull(TEXT("hand-over subsystem"), HandOver)) return;
		HandOver->NoteHostOrder({SUCCESSION_TEST_HOST, SUCCESSION_TEST_SECOND});
		TestFalse(TEXT("the headless world is no listen server: refused"), HandOver->BeginPlannedHandOver(TestWorld->GetWorld()));
		TestEqual(TEXT("nothing started"), HandOver->GetStep(), EHostHandOverStep::None);
	});

	It("a client that isn't next rejoins the next host; with nobody else known nothing starts", [this]()
	{
		UWoRHostHandOverSubsystem *HandOver = TestWorld->GetSubsystem<UWoRHostHandOverSubsystem>();
		if (!TestNotNull(TEXT("hand-over subsystem"), HandOver))
		{
			return;
		}
		HandOver->NoteHostOrder({SUCCESSION_TEST_HOST});
		TestFalse(TEXT("alone with the host: nothing to hand over"), HandOver->BeginHandOver());
		// The headless world's player id is none of these, so it rejoins the second joiner.
		HandOver->NoteHostOrder({SUCCESSION_TEST_HOST, SUCCESSION_TEST_SECOND, SUCCESSION_TEST_THIRD});
		TestTrue(TEXT("started"), HandOver->BeginHandOver());
		TestEqual(TEXT("next host"), HandOver->GetNextHost(), SUCCESSION_TEST_SECOND);
		TestEqual(TEXT("rejoining"), HandOver->GetStep(), EHostHandOverStep::Rejoining);
		TestTrue(TEXT("debug lists the order"), HandOver->GetSuccessionString().Contains(SUCCESSION_TEST_THIRD));
	});

	It("a new host adopts a copy holding a run, and refuses without one", [this]()
	{
		USaveSubsystem *Save = TestWorld->GetSubsystem<USaveSubsystem>();
		if (!TestNotNull(TEXT("save subsystem"), Save))
		{
			return;
		}
		TestFalse(TEXT("no copy kept"), Save->AdoptCheckpointFrom(SUCCESSION_TEST_SLOT));
		UWoRSaveGame *Copy = Cast<UWoRSaveGame>(UGameplayStatics::CreateSaveGameObject(UWoRSaveGame::StaticClass()));
		if (!TestNotNull(TEXT("copy"), Copy))
		{
			return;
		}
		Copy->HandlePreSave(); // stamps the save version, as a real save does (else the load check wipes it, D-060)
		TestTrue(TEXT("a copy without a run written"), UGameplayStatics::SaveGameToSlot(Copy, SUCCESSION_TEST_SLOT, SUCCESSION_TEST_USER));
		TestFalse(TEXT("a copy without a run: refused"), Save->AdoptCheckpointFrom(SUCCESSION_TEST_SLOT));
		Copy->Data.Run.bHasRun = true;
		TestTrue(TEXT("a copy with a run written"), UGameplayStatics::SaveGameToSlot(Copy, SUCCESSION_TEST_SLOT, SUCCESSION_TEST_USER));
		TestTrue(TEXT("adopted"), Save->AdoptCheckpointFrom(SUCCESSION_TEST_SLOT));
		TestTrue(TEXT("playing the host's copy"), Save->IsPlayingHostCopy());
		TestTrue(TEXT("its run waits to resume"), Save->GetData().Run.bHasRun);
	});

	Describe("B-234d", [this]()
	{
		BeforeEach([this]() { UGameplayStatics::DeleteGameInSlot(SUCCESSION_TEST_OWN_SLOT, SUCCESSION_TEST_USER); });
		AfterEach([this]() { UGameplayStatics::DeleteGameInSlot(SUCCESSION_TEST_OWN_SLOT, SUCCESSION_TEST_USER); });

		It("after a handed-over run the pool holds only the player's own gear", [this]()
		{
			USaveSubsystem *Save = TestWorld->GetSubsystem<USaveSubsystem>();
			UPoolSubsystem *Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
			if (!TestNotNull(TEXT("save subsystem"), Save) || !TestNotNull(TEXT("pool subsystem"), Pool))
			{
				return;
			}
			TestFalse(TEXT("no handed-over run: nothing to return from"), Save->ReturnToOwnSaveFrom(SUCCESSION_TEST_OWN_SLOT));
			// The player's own save and pool (as loaded at start), and the old host's copy with its armoury and run.
			TestTrue(TEXT("own save written"), WriteHandOverTestSave(SUCCESSION_TEST_OWN_SLOT, SavedStack(OwnCrystal(), SUCCESSION_TEST_OWN_GARNETS), false));
			TestTrue(TEXT("copy written"), WriteHandOverTestSave(SUCCESSION_TEST_SLOT, SavedStack(CopyCrystal(), SUCCESSION_TEST_COPY_SAPPHIRES), true));
			Pool->AddCrystalToPool(OwnCrystal(), SUCCESSION_TEST_OWN_GARNETS);
			if (!TestTrue(TEXT("adopted"), Save->AdoptCheckpointFrom(SUCCESSION_TEST_SLOT)))
			{
				return;
			}
			TestEqual(TEXT("during the run the old host's armoury is in the pool"), CountInPool(*Pool, CopyCrystal()), SUCCESSION_TEST_COPY_SAPPHIRES);

			TestTrue(TEXT("returned"), Save->ReturnToOwnSaveFrom(SUCCESSION_TEST_OWN_SLOT));
			TestEqual(TEXT("the player's own items are back"), CountInPool(*Pool, OwnCrystal()), SUCCESSION_TEST_OWN_GARNETS);
			TestEqual(TEXT("the old host's are gone"), CountInPool(*Pool, CopyCrystal()), 0);
			TestFalse(TEXT("no longer playing the copy"), Save->IsPlayingHostCopy());
			TestFalse(TEXT("the run is over"), Save->GetData().Run.bHasRun);
			TestFalse(TEXT("only once"), Save->ReturnToOwnSaveFrom(SUCCESSION_TEST_OWN_SLOT));
		});

		It("review: the run's upgrade memory raises the own memory; the old host's earlier memory isn't taken", [this]()
		{
			const TMap<FName, EItemTier> Own = {{SUCCESSION_TEST_DEF_OWN, EItemTier::E_Tier}, {SUCCESSION_TEST_DEF_RAISED, EItemTier::B_Tier}};
			const TMap<FName, EItemTier> AtHandOver = {{SUCCESSION_TEST_DEF_OLD, EItemTier::D_Tier}, {SUCCESSION_TEST_DEF_RAISED, EItemTier::F_Tier}};
			const TMap<FName, EItemTier> Played = {{SUCCESSION_TEST_DEF_OLD, EItemTier::D_Tier}, {SUCCESSION_TEST_DEF_RAISED, EItemTier::C_Tier},
												   {SUCCESSION_TEST_DEF_NEW, EItemTier::A_Tier}};
			const TMap<FName, EItemTier> Merged = USaveSubsystem::MergeRunUpgradeMemory(Own, AtHandOver, Played);
			TestEqual(TEXT("own memory kept"), Merged.FindRef(SUCCESSION_TEST_DEF_OWN), EItemTier::E_Tier);
			TestEqual(TEXT("a run raise lower than the own memory: the own one stays"), Merged.FindRef(SUCCESSION_TEST_DEF_RAISED), EItemTier::B_Tier);
			TestEqual(TEXT("reached first in this run"), Merged.FindRef(SUCCESSION_TEST_DEF_NEW), EItemTier::A_Tier);
			TestFalse(TEXT("the old host's earlier memory isn't taken"), Merged.Contains(SUCCESSION_TEST_DEF_OLD));
		});

		It("review: the run's end writes the own save at once: played data, own pool, the run's memory, no run", [this]()
		{
			USaveSubsystem *Save = TestWorld->GetSubsystem<USaveSubsystem>();
			UPoolSubsystem *Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
			if (!TestNotNull(TEXT("save subsystem"), Save) || !TestNotNull(TEXT("pool subsystem"), Pool))
			{
				return;
			}
			FWoRSaveData Built;
			TestFalse(TEXT("no handed-over run: nothing to build"), Save->BuildOwnSaveFrom(SUCCESSION_TEST_OWN_SLOT, Built));
			TestTrue(TEXT("own save written"), WriteHandOverTestSave(SUCCESSION_TEST_OWN_SLOT, SavedStack(OwnCrystal(), SUCCESSION_TEST_OWN_GARNETS), false));
			TestTrue(TEXT("copy written"), WriteHandOverTestSave(SUCCESSION_TEST_SLOT, SavedStack(CopyCrystal(), SUCCESSION_TEST_COPY_SAPPHIRES), true));
			if (!TestTrue(TEXT("adopted"), Save->AdoptCheckpointFrom(SUCCESSION_TEST_SLOT)))
			{
				return;
			}
			// The run raises a definition's memory; the exit save captures it (into the copy's slot).
			Pool->RestoreUpgradeMemory({{SUCCESSION_TEST_DEF_NEW, EItemTier::A_Tier}});
			TestTrue(TEXT("exit save"), Save->SaveNow(TestWorld->GetWorld(), /*bAsync=*/false));

			if (!TestTrue(TEXT("built"), Save->BuildOwnSaveFrom(SUCCESSION_TEST_OWN_SLOT, Built)))
			{
				return;
			}
			int32 OwnGarnets = 0;
			int32 CopySapphires = 0;
			for (const FSavedCrystalStack &Stack : Built.Pool.Crystals)
			{
				OwnGarnets += Stack.Type == static_cast<uint8>(OwnCrystal().Type) ? Stack.Count : 0;
				CopySapphires += Stack.Type == static_cast<uint8>(CopyCrystal().Type) ? Stack.Count : 0;
			}
			TestEqual(TEXT("the own pool"), OwnGarnets, SUCCESSION_TEST_OWN_GARNETS);
			TestEqual(TEXT("none of the copy's"), CopySapphires, 0);
			TestEqual(TEXT("the run's memory kept"), Built.Pool.UpgradeMemory.FindRef(SUCCESSION_TEST_DEF_NEW), EItemTier::A_Tier);
			TestFalse(TEXT("no run"), Built.Run.bHasRun);
			TestTrue(TEXT("building the own save leaves the game on the copy"), Save->IsPlayingHostCopy());
			TestEqual(TEXT("the copy's items still in the pool"), CountInPool(*Pool, CopyCrystal()), SUCCESSION_TEST_COPY_SAPPHIRES);
		});

		It("join places: the next place, and the order by place", [this]()
		{
			TestEqual(TEXT("first player"), WoRHostSuccession::NextJoinSequence({}), 0);
			TestEqual(TEXT("after the highest, ignoring no place"), WoRHostSuccession::NextJoinSequence({0, 2, WoRHostSuccession::NO_JOIN_SEQUENCE}), 3);
			TArray<TPair<FString, int32>> Players;
			Players.Emplace(SUCCESSION_TEST_FOURTH, WoRHostSuccession::NO_JOIN_SEQUENCE);
			Players.Emplace(SUCCESSION_TEST_THIRD, 2);
			Players.Emplace(SUCCESSION_TEST_HOST, 0);
			Players.Emplace(SUCCESSION_TEST_SECOND, 1);
			Players.Emplace(SUCCESSION_TEST_HOST, 0); // a travelling player's old and new PlayerState
			TestEqual(TEXT("by place, no place last, each id once"), JoinOrderString(WoRHostSuccession::OrderByJoinSequence(Players)),
					  JoinOrderString({SUCCESSION_TEST_HOST, SUCCESSION_TEST_SECOND, SUCCESSION_TEST_THIRD, SUCCESSION_TEST_FOURTH}));
		});

		It("the join order survives a seamless travel", [this]()
		{
			// Review: no explicit SetGameState / AddPlayerState here. A spawned GameState's PostInitializeComponents sets
			// itself as the world's GameState and adds every PlayerState already in the world (actor order), and a spawned
			// PlayerState's PostInitializeComponents adds itself to the world's GameState (its id comes later, at login).
			UWorld *World = TestWorld->GetWorld();
			AWoRGameState *Before = World ? World->SpawnActor<AWoRGameState>() : nullptr;
			if (!TestNotNull(TEXT("first level's GameState"), Before))
			{
				return;
			}
			const TArray<FString> Joined = {SUCCESSION_TEST_HOST, SUCCESSION_TEST_SECOND, SUCCESSION_TEST_THIRD};
			TArray<AWoRPlayerState *> Old;
			for (const FString &Id : Joined)
			{
				AWoRPlayerState *Player = World->SpawnActor<AWoRPlayerState>();
				if (!TestNotNull(TEXT("player"), Player))
				{
					return;
				}
				LogInTestPlayer(Player, Id);
				Old.Add(Player);
			}
			TestEqual(TEXT("joined in order"), JoinOrderString(Before->GetHostOrder()), JoinOrderString(Joined));
			TestEqual(TEXT("the host has the first place"), Old[0]->GetJoinSequence(), 0);

			// The travel, as AGameModeBase::HandleSeamlessTravelPlayer does it, the players taken last-joined first: the
			// next level's GameState holds the travelled PlayerStates (added on spawn), each player gets a new one (place
			// and id copied) and the old one leaves.
			AWoRGameState *After = World->SpawnActor<AWoRGameState>();
			if (!TestNotNull(TEXT("next level's GameState"), After))
			{
				return;
			}
			for (int32 Index = Old.Num() - 1; Index >= 0; --Index)
			{
				AWoRPlayerState *Next = World->SpawnActor<AWoRPlayerState>();
				if (!TestNotNull(TEXT("next PlayerState"), Next))
				{
					return;
				}
				Old[Index]->CopyProperties(Next);
				After->RemovePlayerState(Old[Index]);
				Old[Index]->Destroy();
			}
			TestEqual(TEXT("after the travel the order is as they joined"), JoinOrderString(After->GetHostOrder()), JoinOrderString(Joined));

			AWoRPlayerState *Late = World->SpawnActor<AWoRPlayerState>();
			if (!TestNotNull(TEXT("late joiner"), Late))
			{
				return;
			}
			LogInTestPlayer(Late, SUCCESSION_TEST_FOURTH);
			TestEqual(TEXT("a player joining after the travel comes last"), JoinOrderString(After->GetHostOrder()),
					  JoinOrderString({SUCCESSION_TEST_HOST, SUCCESSION_TEST_SECOND, SUCCESSION_TEST_THIRD, SUCCESSION_TEST_FOURTH}));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
