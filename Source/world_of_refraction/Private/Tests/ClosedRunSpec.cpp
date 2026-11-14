// ClosedRunSpec.cpp
// B-408a (D-172, D-176, Q-408): a game closed mid-run counts as a quit. A run in the player's own save is restored and
// settled: the gear found in it is gone, the home piece it took is back in its home state, the items found are taken
// back and the run is over. The title's Continue then says the hub and that the last run was lost.
// Headless: the slot is written to memory, then restored into a second world (as RunLayerSaveSpec). The hand-over
// copy that still resumes is a PIE check (docs/PIE/B-408a.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Kismet/GameplayStatics.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveGame.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Menus/TitleScreenModel.h"

namespace
{
	constexpr int32 CLOSED_RUN_SEED = 8642;
	constexpr int32 CLOSED_RUN_VARIATION = 0;
	constexpr int32 CLOSED_RUN_HOME_DURABILITY = 10;
	constexpr int32 CLOSED_RUN_WORN_DURABILITY = 3;
	constexpr int32 CLOSED_RUN_FOUND_ITEMS = 2;
	constexpr double CLOSED_RUN_PLAY_SECONDS = 600.0;
	const FCrystalId CLOSED_RUN_GEM(ECrystalType::Garnet, EItemTier::F_Tier);

	FWeaponInventoryEntry ClosedRunEntry(UWeaponData *Weapon, int32 GemDurability)
	{
		FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Weapon);
		Entry.PersistentID = FGuid::NewGuid();
		if (GemDurability > 0)
		{
			Entry.AttachedItem.Kind = EAttachedItemKind::Crystal;
			Entry.AttachedItem.Crystal = FCrystalAttachment(CLOSED_RUN_GEM, GemDurability);
		}
		return Entry;
	}

	UWoRSaveGame *ClosedRunRoundTrip(UWoRSaveGame *Save)
	{
		TArray<uint8> Bytes;
		if (!UGameplayStatics::SaveGameToMemory(Save, Bytes))
		{
			return nullptr;
		}
		UWoRSaveGame *Loaded = Cast<UWoRSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
		if (Loaded)
		{
			Loaded->InitializeSaveGame(nullptr, USaveSubsystem::SLOT_NAME, true);
		}
		return Loaded;
	}
}

BEGIN_DEFINE_SPEC(FClosedRunSpec, "WoR.Save.ClosedRun", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FClosedRunSpec)

void FClosedRunSpec::Define()
{
	It("settles a run from the save as a quit: its finds gone, the home piece back, the run over", [this]()
	{
		FWoRTestWorld Before;
		FWoRTestWorld After;
		if (!TestTrue(TEXT("Worlds"), Before.Create() && After.Create())) return;
		UPoolSubsystem *Pool = Before.GetSubsystem<UPoolSubsystem>();
		UPoolSubsystem *PoolAfter = After.GetSubsystem<UPoolSubsystem>();
		UTrialRunSubsystem *Running = Before.GetSubsystem<UTrialRunSubsystem>();
		UTrialRunSubsystem *Closed = After.GetSubsystem<UTrialRunSubsystem>();
		if (!TestTrue(TEXT("Subsystems"), Pool && PoolAfter && Running && Closed)) return;

		UWeaponData *Sword = NewObject<UWeaponData>(GetTransientPackage());
		UWeaponData *Dagger = NewObject<UWeaponData>(GetTransientPackage());
		UTrialData *Trial = NewObject<UTrialData>(GetTransientPackage());

		// The run takes the home sword (worn in the run), finds a dagger and two crystals; then the game closes.
		FWeaponInventoryEntry Home = ClosedRunEntry(Sword, CLOSED_RUN_HOME_DURABILITY);
		Pool->UpdateWeapon(Home);
		Running->RestoreRun(Trial, CLOSED_RUN_SEED, CLOSED_RUN_VARIATION, ETrialDifficulty::Medium, {}, {}, {});
		if (!TestTrue(TEXT("The run layer is open"), Pool->IsRunLayerActive())) return;
		TestTrue(TEXT("The home sword goes on the run"), Pool->TakeOnRun(Home.PersistentID));
		Home.AttachedItem.Crystal.CurrentDurability = CLOSED_RUN_WORN_DURABILITY;
		Pool->UpdateWeapon(Home);
		FWeaponInventoryEntry Found = ClosedRunEntry(Dagger, 0);
		Pool->UpdateWeapon(Found);
		TestTrue(TEXT("The dagger is a run find"), Pool->MarkRunOnly(Found.PersistentID));
		const FCrystalId FoundItem(ECrystalType::Emerald, EItemTier::D_Tier);
		Pool->AddCrystalToPool(FoundItem, CLOSED_RUN_FOUND_ITEMS);

		UWoRSaveGame *Save = NewObject<UWoRSaveGame>();
		Save->HandlePreSave();
		Save->Data.Pool = USaveSubsystem::CapturePool(*Pool);
		Save->Data.Run = USaveSubsystem::CaptureRun(*Running, 0);
		UWoRSaveGame *Loaded = ClosedRunRoundTrip(Save);
		if (!TestNotNull(TEXT("Loaded"), Loaded)) return;

		// The next start: the save's run is restored only to be settled (USaveSubsystem::SettleClosedRun).
		USaveSubsystem::RestorePool(Loaded->Data.Pool, *PoolAfter);
		if (!TestTrue(TEXT("Run restored"), USaveSubsystem::RestoreRun(Loaded->Data.Run, *Closed))) return;
		Closed->SettleRunAsQuit(/*bTravelIfAway=*/false);

		TestFalse(TEXT("The run is over"), Closed->IsRunActive());
		TestFalse(TEXT("The layer is closed"), PoolAfter->IsRunLayerActive());
		TestNull(TEXT("The dagger found in the run is gone"), PoolAfter->FindWeaponInPool(Found.PersistentID));
		const FWeaponInventoryEntry *Back = PoolAfter->FindWeaponInPool(Home.PersistentID);
		TestTrue(TEXT("The sword is home as it left"), Back && Back->AttachedItem.GetCurrentDurability() == CLOSED_RUN_HOME_DURABILITY);
		TestEqual(TEXT("The crystals found in the run are taken back"), PoolAfter->GetStoredCount(FoundItem), 0);
		TestFalse(TEXT("The quit mark is cleared with the run"), Closed->IsAbandoned());
	});

	It("does nothing without an active run", [this]()
	{
		FWoRTestWorld World;
		if (!TestTrue(TEXT("World"), World.Create())) return;
		UTrialRunSubsystem *TrialRun = World.GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("Trial run"), TrialRun)) return;
		TrialRun->SettleRunAsQuit(/*bTravelIfAway=*/false);
		TestFalse(TEXT("Still no run"), TrialRun->IsRunActive());
	});

	It("gives Continue the hub, and says the last run was lost when the save held one", [this]()
	{
		FWoRSaveData WithRun;
		WithRun.Run.bHasRun = true;
		const FTitleSaveSummary Lost = WoRTitleScreen::Summarize(WithRun, true, CLOSED_RUN_PLAY_SECONDS);
		TestTrue(TEXT("the run was lost"), Lost.bLastRunLost);
		const FString Line = WoRTitleScreen::BuildRows(Lost)[0].Line.ToString();
		TestTrue(TEXT("Continue starts in the hub"), Line.Contains(TEXT("hub")));
		TestTrue(TEXT("and says the run was lost"), Line.Contains(TEXT("lost")));
		TestTrue(TEXT("settled already this session: still said"), WoRTitleScreen::Summarize(FWoRSaveData(), true, CLOSED_RUN_PLAY_SECONDS, /*bRunSettled=*/true).bLastRunLost);
		TestFalse(TEXT("no run: nothing lost"), WoRTitleScreen::Summarize(FWoRSaveData(), true, CLOSED_RUN_PLAY_SECONDS).bLastRunLost);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
