// SaveRunSpec.cpp
// B-208 (Q-090, Q-208): a trial run captured at a fight's exit survives save bytes and comes back
// into the trial subsystem as it was (trial, seed, gate choice, beaten encounters, found features,
// memories, Gold), and the party's carried crystal wear and absorption come back with it.
// Headless: the slot is written to memory (no local player, so no level is opened).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Kismet/GameplayStatics.h"
#include "Memory/MemoryData.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveGame.h"
#include "Trial/TrialData.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	constexpr int32 SAVE_RUN_SEED = 1234;
	constexpr int32 SAVE_RUN_VARIATION = 1;
	constexpr int32 SAVE_RUN_GOLD = 25;
	constexpr int32 SAVE_RUN_WORN_DURABILITY = 6;
	constexpr int32 SAVE_RUN_EVO_DURABILITY = 40;
	constexpr int32 SAVE_RUN_STACKS = 3;
	constexpr int32 SAVE_RUN_HP = 21;
	const FName SAVE_RUN_ENCOUNTER(TEXT("TrialMap.Encounter_A"));
	const FName SAVE_RUN_CHEST(TEXT("TrialMap.Chest_1"));

	UWoRSaveGame *SaveRunRoundTrip(UWoRSaveGame *Save)
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

BEGIN_DEFINE_SPEC(FSaveRunSpec, "WoR.Save.Run",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FSaveRunSpec)

void FSaveRunSpec::Define()
{
	It("brings a mid-trial run back as the last fight's exit left it", [this]()
	{
		FWoRTestWorld Before;
		FWoRTestWorld After;
		if (!TestTrue(TEXT("Worlds"), Before.Create() && After.Create())) return;
		UTrialRunSubsystem *Running = Before.GetSubsystem<UTrialRunSubsystem>();
		UTrialRunSubsystem *Resumed = After.GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("Trial run"), Running) || !TestNotNull(TEXT("Trial run after"), Resumed)) return;

		UTrialData *Trial = NewObject<UTrialData>(GetTransientPackage());
		UMemoryData *Memory = NewObject<UMemoryData>(GetTransientPackage());
		TMap<FName, FText> Found;
		Found.Add(SAVE_RUN_CHEST, FText::FromString(TEXT("12 Gold")));
		Running->RestoreRun(Trial, SAVE_RUN_SEED, SAVE_RUN_VARIATION, ETrialDifficulty::Hard, {SAVE_RUN_ENCOUNTER}, Found, {Memory});

		UWoRSaveGame *Save = NewObject<UWoRSaveGame>();
		Save->HandlePreSave();
		Save->Data.Run = USaveSubsystem::CaptureRun(*Running, SAVE_RUN_GOLD);
		UWoRSaveGame *Loaded = SaveRunRoundTrip(Save);
		if (!TestNotNull(TEXT("Loaded"), Loaded)) return;
		TestEqual(TEXT("The run's Gold is saved with it"), Loaded->Data.Run.RunGold, SAVE_RUN_GOLD);
		TestFalse(TEXT("Nothing running before the restore"), Resumed->IsRunActive());

		TestTrue(TEXT("Restored"), USaveSubsystem::RestoreRun(Loaded->Data.Run, *Resumed));
		TestTrue(TEXT("The run is active"), Resumed->IsRunActive());
		TestTrue(TEXT("Same trial"), Resumed->GetActiveTrial() == Trial);
		TestEqual(TEXT("Same seed"), Resumed->GetRunSeed(), SAVE_RUN_SEED);
		TestEqual(TEXT("Same variation"), Resumed->GetVariationIndex(), SAVE_RUN_VARIATION);
		TestTrue(TEXT("Same difficulty"), Resumed->GetTrialDifficulty() == ETrialDifficulty::Hard);
		TestTrue(TEXT("The beaten fight stays beaten"), Resumed->IsEncounterBeaten(SAVE_RUN_ENCOUNTER));
		TestTrue(TEXT("The opened chest stays open"), Resumed->HasFound(SAVE_RUN_CHEST));
		TestTrue(TEXT("The memory is still collected"),
				 Resumed->GetRunMemories().Num() == 1 && Resumed->GetRunMemories()[0] == Memory);
	});

	It("saves no run outside a trial", [this]()
	{
		FWoRTestWorld World;
		if (!TestTrue(TEXT("World"), World.Create())) return;
		UTrialRunSubsystem *TrialRun = World.GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("Trial run"), TrialRun)) return;
		const FSavedRun Saved = USaveSubsystem::CaptureRun(*TrialRun, SAVE_RUN_GOLD);
		TestFalse(TEXT("No run"), Saved.bHasRun);
		TestFalse(TEXT("Nothing to restore"), USaveSubsystem::RestoreRun(Saved, *TrialRun));
		TestFalse(TEXT("Still no run"), TrialRun->IsRunActive());
	});

	It("brings back the party's carried HP, crystal wear and absorption during a run", [this]()
	{
		FWoRTestWorld After;
		if (!TestTrue(TEXT("World"), After.Create())) return;
		UPartySessionSubsystem *PartySession = After.GetSubsystem<UPartySessionSubsystem>();
		if (!TestNotNull(TEXT("Party session"), PartySession)) return;

		UWeaponData *Sword = NewObject<UWeaponData>(GetTransientPackage());
		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		UParty *Party = NewObject<UParty>(GetTransientPackage());
		FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
		Member.PawnClass = ACombatCharacter::StaticClass();
		Member.CharacterData = FWoRTestWorld::NewCharacterData();
		Member.CurrentHP = SAVE_RUN_HP;
		Member.CarriedGear.bCarried = true;
		FCarriedAttachment &Worn = Member.CarriedGear.Attachments.AddDefaulted_GetRef();
		Worn.Holder = Sword;
		Worn.Attached.Kind = EAttachedItemKind::Crystal;
		Worn.Attached.Crystal.Id = FCrystalId(ECrystalType::Garnet, EItemTier::F_Tier);
		Worn.Attached.Crystal.CurrentDurability = SAVE_RUN_WORN_DURABILITY;
		Member.CarriedGear.bHasPrimaryEvolution = true;
		Member.CarriedGear.PrimaryEvolution.Item = Evolution;
		Member.CarriedGear.PrimaryEvolution.CurrentDurability = SAVE_RUN_EVO_DURABILITY;
		Member.CarriedAbsorption.bCarried = true;
		Member.CarriedAbsorption.AbsorptionStacks = SAVE_RUN_STACKS;

		TestFalse(TEXT("Outside a run, no crystal wear is saved"),
				  USaveSubsystem::CaptureParty(Party, false).Members[0].CarriedGear.bCarried);
		UWoRSaveGame *Save = NewObject<UWoRSaveGame>();
		Save->HandlePreSave();
		Save->Data.Party = USaveSubsystem::CaptureParty(Party, true);
		TestNull(TEXT("No pointer in the saved evolution"), Save->Data.Party.Members[0].CarriedGear.PrimaryEvolution.Item);
		UWoRSaveGame *Loaded = SaveRunRoundTrip(Save);
		if (!TestNotNull(TEXT("Loaded"), Loaded)) return;
		TestTrue(TEXT("Restored"), USaveSubsystem::RestoreParty(Loaded->Data.Party, *PartySession));

		const UParty *Restored = PartySession->GetLocalParty();
		if (!TestNotNull(TEXT("Local party"), Restored) || !TestEqual(TEXT("Members"), Restored->Members.Num(), 1)) return;
		const FPartyMember &Back = Restored->Members[0];
		TestEqual(TEXT("HP"), Back.CurrentHP, SAVE_RUN_HP);
		TestTrue(TEXT("Gear carried"), Back.CarriedGear.bCarried);
		if (!TestEqual(TEXT("One attachment"), Back.CarriedGear.Attachments.Num(), 1)) return;
		TestTrue(TEXT("On the same sword"), Back.CarriedGear.Attachments[0].Holder == Sword);
		TestEqual(TEXT("As worn"), Back.CarriedGear.Attachments[0].Attached.GetCurrentDurability(), SAVE_RUN_WORN_DURABILITY);
		TestTrue(TEXT("The same primary evolution"), Back.CarriedGear.PrimaryEvolution.Item == Evolution);
		TestEqual(TEXT("Its durability"), Back.CarriedGear.PrimaryEvolution.CurrentDurability, SAVE_RUN_EVO_DURABILITY);
		TestEqual(TEXT("Absorption stacks"), Back.CarriedAbsorption.AbsorptionStacks, SAVE_RUN_STACKS);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
