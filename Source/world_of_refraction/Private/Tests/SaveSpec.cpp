// SaveSpec.cpp
// B-090: the save keystone. The party (members, formation, control, carried HP/EP), the banked
// wallet and the pool's owned items survive capture -> save bytes -> load -> restore; Gold is
// never banked; a save below the minimum version is wiped (D-060). Headless: the slot is written
// to memory, never to disk (a test world has no local player).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Kismet/GameplayStatics.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveGame.h"

namespace
{
	constexpr int32 SAVED_HP = 37;
	constexpr int32 SAVED_EP = 4;
	constexpr int32 SAVED_COLUMN = 2;
	constexpr int32 PRISMS = 5;
	constexpr int32 FIRE_ESSENCE = 3;
	constexpr int32 GOLD = 7;
	constexpr int32 CRYSTAL_COUNT = 2;

	/** Save bytes and load them back, as the slot on disk would. */
	UWoRSaveGame *RoundTrip(UWoRSaveGame *Save)
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

BEGIN_DEFINE_SPEC(FSaveSpec, "WoR.Save",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	bool CreateWorlds(FWoRTestWorld &Before, FWoRTestWorld &After)
	{
		return TestTrue(TEXT("Worlds"), Before.Create() && After.Create());
	}
END_DEFINE_SPEC(FSaveSpec)

void FSaveSpec::Define()
{
	It("brings the party back with its formation, control and carried HP/EP", [this]()
	{
		FWoRTestWorld Before;
		FWoRTestWorld After;
		if (!CreateWorlds(Before, After))
		{
			return;
		}
		UParty *Party = NewObject<UParty>(GetTransientPackage());
		Party->DisplayName = FText::FromString(TEXT("Saved Party"));
		Party->bLetAIFinish = true;
		UCharacterData *Hero = FWoRTestWorld::NewCharacterData();
		UCharacterData *Ally = FWoRTestWorld::NewCharacterData();
		for (UCharacterData *Data : {Hero, Ally})
		{
			FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
			Member.PawnClass = ACombatCharacter::StaticClass();
			Member.CharacterData = Data;
		}
		Party->Members[1].CurrentHP = SAVED_HP;
		Party->Members[1].CurrentEP = SAVED_EP;
		Party->Members[1].PreferredRow = ECombatRow::Back;
		Party->Members[1].PreferredColumn = SAVED_COLUMN;
		Party->Members[1].bPlayerControlled = false;

		UWoRSaveGame *Save = NewObject<UWoRSaveGame>();
		Save->HandlePreSave();
		Save->Data.Party = USaveSubsystem::CaptureParty(Party, true);
		TestEqual(TEXT("Without a saved run, no carried HP"),
				  USaveSubsystem::CaptureParty(Party, false).Members[1].CurrentHP, PartyConstants::NO_CARRIED_VITALS);
		UWoRSaveGame *Loaded = RoundTrip(Save);
		UPartySessionSubsystem *PartySession = After.GetSubsystem<UPartySessionSubsystem>();
		if (!TestNotNull(TEXT("Loaded"), Loaded) || !TestNotNull(TEXT("Party session"), PartySession))
		{
			return;
		}
		TestTrue(TEXT("Restored"), USaveSubsystem::RestoreParty(Loaded->Data.Party, *PartySession));

		const UParty *Restored = PartySession->GetLocalParty();
		if (!TestNotNull(TEXT("Local party"), Restored) || !TestEqual(TEXT("Members"), Restored->Members.Num(), 2))
		{
			return;
		}
		const FPartyMember &Back = Restored->Members[1];
		TestEqual(TEXT("Name"), Restored->DisplayName.ToString(), FString(TEXT("Saved Party")));
		TestTrue(TEXT("Let AI finish"), Restored->bLetAIFinish);
		TestTrue(TEXT("Same characters"), Restored->Members[0].CharacterData == Hero && Back.CharacterData == Ally);
		TestEqual(TEXT("HP"), Back.CurrentHP, SAVED_HP);
		TestEqual(TEXT("EP"), Back.CurrentEP, SAVED_EP);
		TestTrue(TEXT("Cell"), Back.PreferredRow == ECombatRow::Back && Back.PreferredColumn == SAVED_COLUMN);
		TestFalse(TEXT("AI-controlled"), Back.bPlayerControlled);
		TestFalse(TEXT("A party that already exists is left alone"),
				  USaveSubsystem::RestoreParty(Loaded->Data.Party, *PartySession));
	});

	It("banks every persistent balance but never Gold", [this]()
	{
		FWoRTestWorld Before;
		FWoRTestWorld After;
		if (!CreateWorlds(Before, After))
		{
			return;
		}
		AActor *Holder = Before.GetWorld()->SpawnActor<AActor>();
		UCurrencyComponent *Wallet = FWoRTestWorld::AddComponent<UCurrencyComponent>(Holder);
		Wallet->Add(ECurrencyType::Prisms, PRISMS);
		Wallet->AddEssenceType(EEssenceType::Fire, FIRE_ESSENCE);
		Wallet->Add(ECurrencyType::Gold, GOLD);

		UWoRSaveGame *Save = NewObject<UWoRSaveGame>();
		Save->HandlePreSave();
		Save->Data.Wallet = USaveSubsystem::CaptureWallet(Wallet);
		TestEqual(TEXT("Two balances banked"), Save->Data.Wallet.Balances.Num(), 2);
		UWoRSaveGame *Loaded = RoundTrip(Save);
		if (!TestNotNull(TEXT("Loaded"), Loaded))
		{
			return;
		}
		UCurrencyComponent *Fresh = FWoRTestWorld::AddComponent<UCurrencyComponent>(After.GetWorld()->SpawnActor<AActor>());
		USaveSubsystem::ApplyWallet(Loaded->Data.Wallet, Fresh);
		USaveSubsystem::ApplyWallet(Loaded->Data.Wallet, Fresh);
		TestEqual(TEXT("Prisms (applying twice adds nothing)"), Fresh->GetBalance(ECurrencyType::Prisms), PRISMS);
		TestEqual(TEXT("Fire Essence"), Fresh->GetEssenceType(EEssenceType::Fire), FIRE_ESSENCE);
		TestEqual(TEXT("Gold"), Fresh->GetBalance(ECurrencyType::Gold), 0);
	});

	It("carries the run's Gold across a level change but clears it when a run starts or ends", [this]()
	{
		FWoRTestWorld World;
		if (!TestTrue(TEXT("World"), World.Create()))
		{
			return;
		}
		USaveSubsystem *Save = World.GetSubsystem<USaveSubsystem>();
		UCurrencyComponent *Wallet = FWoRTestWorld::AddComponent<UCurrencyComponent>(World.GetWorld()->SpawnActor<AActor>());
		if (!TestNotNull(TEXT("Save subsystem"), Save))
		{
			return;
		}
		Wallet->Add(ECurrencyType::Gold, GOLD);
		Save->BankWallet(Wallet);
		UCurrencyComponent *NextLevel = FWoRTestWorld::AddComponent<UCurrencyComponent>(World.GetWorld()->SpawnActor<AActor>());
		Save->ApplyWalletTo(NextLevel);
		TestEqual(TEXT("Gold in the next level"), NextLevel->GetBalance(ECurrencyType::Gold), GOLD);
		TestFalse(TEXT("Gold is not written to the slot"),
				  Save->GetData().Wallet.Balances.ContainsByPredicate([](const FSavedBalance &B)
				  { return B.Currency == static_cast<uint8>(ECurrencyType::Gold); }));

		Save->ClearRunGold(World.GetWorld());
		UCurrencyComponent *AfterRun = FWoRTestWorld::AddComponent<UCurrencyComponent>(World.GetWorld()->SpawnActor<AActor>());
		Save->ApplyWalletTo(AfterRun);
		TestEqual(TEXT("No Gold after the run"), AfterRun->GetBalance(ECurrencyType::Gold), 0);
	});

	It("brings the pool's items back with the same ids and tiers", [this]()
	{
		FWoRTestWorld Before;
		FWoRTestWorld After;
		if (!CreateWorlds(Before, After))
		{
			return;
		}
		UPoolSubsystem *Pool = Before.GetSubsystem<UPoolSubsystem>();
		UPoolSubsystem *NewPool = After.GetSubsystem<UPoolSubsystem>();
		if (!TestNotNull(TEXT("Pools"), Pool) || !TestNotNull(TEXT("New pool"), NewPool))
		{
			return;
		}
		UWeaponData *Sword = NewObject<UWeaponData>(GetTransientPackage());
		FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Entry.PersistentID = FGuid::NewGuid();
		Entry.Tier = EItemTier::B_Tier;
		Pool->AddWeaponToPool(Entry);
		FCrystalId Crystal;
		Crystal.Type = ECrystalType::Garnet;
		Crystal.Tier = EItemTier::C_Tier;
		Pool->AddCrystalToPool(Crystal, CRYSTAL_COUNT);

		UWoRSaveGame *Save = NewObject<UWoRSaveGame>();
		Save->HandlePreSave();
		Save->Data.Pool = USaveSubsystem::CapturePool(*Pool);
		UWoRSaveGame *Loaded = RoundTrip(Save);
		if (!TestNotNull(TEXT("Loaded"), Loaded))
		{
			return;
		}
		TestEqual(TEXT("Entries restored"), USaveSubsystem::RestorePool(Loaded->Data.Pool, *NewPool), 2);
		const FWeaponInventoryEntry *Back = NewPool->FindWeaponInPool(Entry.PersistentID);
		if (TestNotNull(TEXT("Same weapon id"), Back))
		{
			TestTrue(TEXT("Same asset"), Back->Weapon == Sword);
			TestTrue(TEXT("Same tier"), Back->Tier == EItemTier::B_Tier);
		}
		const int32 *Count = NewPool->GetCrystals().Find(Crystal);
		const int32 *StoneCount = NewPool->GetStones().Find(Crystal);
		TestEqual(TEXT("Crystal stack"), Count ? *Count : StoneCount ? *StoneCount : 0, CRYSTAL_COUNT);
	});

	It("wipes a save below the minimum version (D-060)", [this]()
	{
		UWoRSaveGame *Old = NewObject<UWoRSaveGame>();
		Old->Data.Party.bHasParty = true; // never stamped: version 0
		UWoRSaveGame *Loaded = RoundTrip(Old);
		if (!TestNotNull(TEXT("Loaded"), Loaded))
		{
			return;
		}
		TestTrue(TEXT("Wiped"), Loaded->WasWiped());
		TestFalse(TEXT("Counts as fresh"), Loaded->WasLoaded());
		TestFalse(TEXT("Data gone"), Loaded->Data.Party.bHasParty);

		UWoRSaveGame *Current = NewObject<UWoRSaveGame>();
		Current->HandlePreSave();
		Current->Data.Party.bHasParty = true;
		UWoRSaveGame *Kept = RoundTrip(Current);
		TestTrue(TEXT("A current save is kept"), Kept && !Kept->WasWiped() && Kept->Data.Party.bHasParty);
	});
}

#endif
