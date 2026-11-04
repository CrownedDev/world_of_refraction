// RunLayerSaveSpec.cpp
// B-314b (Q-314, D-097, save v15): a run saved mid-way resumes with its run layer: gear found before the quit is still
// run-only (gone at run end), a home piece still goes back to its home state, and the upgrade memory survives save and
// load. Headless: the slot is written to memory, then restored into a second world.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Kismet/GameplayStatics.h"
#include "Memory/MemoryData.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveGame.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	constexpr int32 RUN_SAVE_SEED = 4321;
	constexpr int32 RUN_SAVE_VARIATION = 0;
	constexpr int32 RUN_SAVE_HOME_DURABILITY = 10;
	constexpr int32 RUN_SAVE_WORN_DURABILITY = 3;
	constexpr EItemTier RUN_SAVE_FOUND_TIER = EItemTier::C_Tier;
	constexpr EItemTier RUN_SAVE_REMEMBERED_TIER = EItemTier::B_Tier;
	const FCrystalId RUN_SAVE_GEM(ECrystalType::Garnet, EItemTier::F_Tier);

	FWeaponInventoryEntry RunSaveEntry(UWeaponData *Weapon, int32 GemDurability)
	{
		FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Weapon);
		Entry.PersistentID = FGuid::NewGuid();
		if (GemDurability > 0)
		{
			Entry.AttachedItem.Kind = EAttachedItemKind::Crystal;
			Entry.AttachedItem.Crystal = FCrystalAttachment(RUN_SAVE_GEM, GemDurability);
		}
		return Entry;
	}

	UWoRSaveGame *RunSaveRoundTrip(UWoRSaveGame *Save)
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

BEGIN_DEFINE_SPEC(FRunLayerSaveSpec, "WoR.Save.RunLayer",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FRunLayerSaveSpec)

void FRunLayerSaveSpec::Define()
{
	It("resumes a saved run with its found gear still run-only, its home gear's home state and the upgrade memory", [this]()
	{
		FWoRTestWorld Before;
		FWoRTestWorld After;
		if (!TestTrue(TEXT("Worlds"), Before.Create() && After.Create())) return;
		UPoolSubsystem *Pool = Before.GetSubsystem<UPoolSubsystem>();
		UPoolSubsystem *PoolAfter = After.GetSubsystem<UPoolSubsystem>();
		UTrialRunSubsystem *Running = Before.GetSubsystem<UTrialRunSubsystem>();
		UTrialRunSubsystem *Resumed = After.GetSubsystem<UTrialRunSubsystem>();
		if (!TestTrue(TEXT("Subsystems"), Pool && PoolAfter && Running && Resumed)) return;

		UWeaponData *Sword = NewObject<UWeaponData>(GetTransientPackage());
		UWeaponData *Dagger = NewObject<UWeaponData>(GetTransientPackage());
		UWeaponData *Axe = NewObject<UWeaponData>(GetTransientPackage());
		UTrialData *Trial = NewObject<UTrialData>(GetTransientPackage());

		// An earlier run left the axe remembered; this run takes the home sword and finds a dagger.
		TMap<FName, EItemTier> Memory;
		Memory.Add(UPoolSubsystem::GetDefinitionKey(Axe), RUN_SAVE_REMEMBERED_TIER);
		Pool->RestoreUpgradeMemory(Memory);
		FWeaponInventoryEntry Home = RunSaveEntry(Sword, RUN_SAVE_HOME_DURABILITY);
		Pool->UpdateWeapon(Home);
		Running->RestoreRun(Trial, RUN_SAVE_SEED, RUN_SAVE_VARIATION, ETrialDifficulty::Medium, {}, {}, {});
		if (!TestTrue(TEXT("The run layer is open"), Pool->IsRunLayerActive())) return;
		TestTrue(TEXT("The home sword goes on the run"), Pool->TakeOnRun(Home.PersistentID));
		Home.AttachedItem.Crystal.CurrentDurability = RUN_SAVE_WORN_DURABILITY; // worn in the run
		Pool->UpdateWeapon(Home);
		FWeaponInventoryEntry Found = RunSaveEntry(Dagger, 0);
		Found.Tier = RUN_SAVE_FOUND_TIER;
		Pool->UpdateWeapon(Found);
		TestTrue(TEXT("The dagger is a run find"), Pool->MarkRunOnly(Found.PersistentID));

		UWoRSaveGame *Save = NewObject<UWoRSaveGame>();
		Save->HandlePreSave();
		Save->Data.Pool = USaveSubsystem::CapturePool(*Pool);
		Save->Data.Run = USaveSubsystem::CaptureRun(*Running, 0);
		TestTrue(TEXT("The layer is saved with the run"), Save->Data.Run.bHasRunLayer && Save->Data.Run.RunMarks.Num() == 2);
		UWoRSaveGame *Loaded = RunSaveRoundTrip(Save);
		if (!TestNotNull(TEXT("Loaded"), Loaded)) return;

		USaveSubsystem::RestorePool(Loaded->Data.Pool, *PoolAfter);
		TestTrue(TEXT("Run restored"), USaveSubsystem::RestoreRun(Loaded->Data.Run, *Resumed));
		TestTrue(TEXT("The layer is open again"), PoolAfter->IsRunLayerActive());
		TestTrue(TEXT("The dagger is still a run find"), PoolAfter->IsRunOnly(Found.PersistentID));
		TestTrue(TEXT("The sword is still on the run"), PoolAfter->IsOnRun(Home.PersistentID));
		EItemTier Remembered = EItemTier::F_Tier;
		TestTrue(TEXT("The memory survived"), PoolAfter->FindRememberedTier(Axe, Remembered) && Remembered == RUN_SAVE_REMEMBERED_TIER);

		PoolAfter->EndRunLayer();
		TestNull(TEXT("The dagger is gone at run end"), PoolAfter->FindWeaponInPool(Found.PersistentID));
		const FWeaponInventoryEntry *Back = PoolAfter->FindWeaponInPool(Home.PersistentID);
		TestTrue(TEXT("The sword is home with its gem as it left"),
				 Back && Back->AttachedItem.GetCurrentDurability() == RUN_SAVE_HOME_DURABILITY);
		TestTrue(TEXT("The dagger's tier is remembered"), PoolAfter->FindRememberedTier(Dagger, Remembered) && Remembered == RUN_SAVE_FOUND_TIER);
	});

	It("saves no run layer when none is open", [this]()
	{
		FWoRTestWorld World;
		if (!TestTrue(TEXT("World"), World.Create())) return;
		UTrialRunSubsystem *TrialRun = World.GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("Trial run"), TrialRun)) return;
		const FSavedRun Saved = USaveSubsystem::CaptureRun(*TrialRun, 0);
		TestFalse(TEXT("No layer"), Saved.bHasRunLayer);
		TestEqual(TEXT("No marks"), Saved.RunMarks.Num(), 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
