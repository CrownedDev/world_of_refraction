// HubLeaderSpec.cpp
// B-529: a new game's hub pawn stands as the party leader (AWoRGameModeBase, shared with the trial maps), so its
// StartingGear is minted once and worn; a trial then wears the class kit and a quit puts the hub record back. A trial
// entered with no worn record seeds the starting gear first, so the kit (per run, D-221) never becomes the first record.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryData.h"
#include "Loadout/ClassDefaultLoadoutData.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/WoRGameModeBase.h"
#include "Pool/PoolSubsystem.h"
#include "Trial/TrialRunSubsystem.h"

BEGIN_DEFINE_SPEC(FHubLeaderSpec, "WoR.Party.HubLeader", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UTrialRunSubsystem *TrialRun = nullptr;
	UParty *Party = nullptr;
	UCharacterData *Leader = nullptr;
	UWeaponData *StartSword = nullptr;
	UWeaponData *KitSword = nullptr;

	FName Key() const { return UPoolSubsystem::GetCharacterKey(Leader); }

	/** The asset of the weapon Key holds in hand; null with no record. */
	const UWeaponData *InHand() const
	{
		const FWornGear *Worn = Pool->GetWornGear(Key());
		const FWeaponInventoryEntry *Weapon = Worn ? Pool->FindWeaponInPool(Worn->ActiveGearID) : nullptr;
		return Weapon ? Weapon->Weapon : nullptr;
	}

	int32 CountOwned(const UWeaponData *Asset) const
	{
		return Pool->GetOwnedWeapons().FilterByPredicate([Asset](const FWeaponInventoryEntry &E) { return E.Weapon == Asset; }).Num();
	}
END_DEFINE_SPEC(FHubLeaderSpec)

void FHubLeaderSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		StartSword = NewObject<UWeaponData>(GetTransientPackage());
		KitSword = NewObject<UWeaponData>(GetTransientPackage());
		// The new game's pick: a Generic whose template authors one starting sword.
		Leader = FWoRTestWorld::NewCharacterData();
		Leader->Name = TEXT("Hub leader");
		Leader->CharacterClass = ECharacterClass::Generic;
		Leader->Inventory = NewObject<UInventoryData>(GetTransientPackage());
		Leader->Inventory->StartingGear.Pieces.AddDefaulted_GetRef().Piece = StartSword;
		UClassDefaultLoadoutData *Kits = NewObject<UClassDefaultLoadoutData>(GetTransientPackage());
		Kits->Defaults.Add(ECharacterClass::Generic).Pieces.AddDefaulted_GetRef().Piece = KitSword;
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("party"), Party) || !TestNotNull(TEXT("pool"), Pool) || !TestNotNull(TEXT("trial run"), TrialRun)) return;
		Party->Members.Empty();
		FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
		Member.PawnClass = ACombatCharacter::StaticClass();
		Member.CharacterData = Leader;
		Member.MemberId = FGuid::NewGuid();
		TrialRun->SetClassDefaultLoadouts(TSoftObjectPtr<UClassDefaultLoadoutData>(Kits));
	});

	AfterEach([this]()
	{
		Pool = nullptr;
		TrialRun = nullptr;
		TestWorld.Reset();
	});

	It("stands the hub pawn as the leader: its starting gear is minted once and worn; a trial's kit doesn't replace it", [this]()
	{
		if (!Party) return;
		TestFalse(TEXT("no party: nothing to stand as"), AWoRGameModeBase::StandAsPartyLeader(nullptr, Party));
		// The hub's pawn as its Blueprint class makes it: the class default template, before BeginPlay.
		APawn *Pawn = TestWorld->GetWorld()->SpawnActor<APawn>();
		UCharacterDataComponent *CharComp = Pawn ? NewObject<UCharacterDataComponent>(Pawn) : nullptr;
		if (!TestNotNull(TEXT("pawn"), CharComp)) return;
		CharComp->CharacterData = FWoRTestWorld::NewCharacterData(); // "Character Template"
		Pawn->AddInstanceComponent(CharComp);
		TestTrue(TEXT("stood as the leader"), AWoRGameModeBase::StandAsPartyLeader(Pawn, Party));
		TestTrue(TEXT("the picked character"), CharComp->CharacterData == Leader);
		TestTrue(TEXT("the leader's member id"), CharComp->PartyMemberId == Party->Members[0].MemberId);
		CharComp->RegisterComponent(); // BeginPlay, with the leader's data
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Pawn);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Pawn);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Pawn);
		UInventoryComponent *Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Pawn);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Pawn);
		Inventory->DrawSharedGear();
		TestTrue(TEXT("the starting sword in hand"), InHand() == StartSword);
		Inventory->DrawSharedGear(); // the hub visited again
		TestEqual(TEXT("minted once"), CountOwned(StartSword), 1);

		Pawn->Destroy(); // the travel takes the hub's pawn away; the trial map spawns its own
		TrialRun->StartRunGearAndGold(); // what EnterTrial does, without the travel
		TestTrue(TEXT("the trial wears the kit"), InHand() == KitSword);
		const FWornGear *Start = Pool->GetRunStartWornGear().Find(Key());
		const FWeaponInventoryEntry *StartWeapon = Start ? Pool->FindWeaponInPool(Start->ActiveGearID) : nullptr;
		TestTrue(TEXT("the run-start record is the hub's"), StartWeapon && StartWeapon->Weapon == StartSword);
		Pool->CloseRun(EPoolRunEnding::Quit);
		TestTrue(TEXT("a quit puts the hub record back"), InHand() == StartSword);
		TestEqual(TEXT("still minted once"), CountOwned(StartSword), 1);
	});

	It("seeds the starting gear before the kit when a trial is entered with no worn record", [this]()
	{
		if (!Party) return;
		TestNull(TEXT("no record yet (the hub never drew)"), Pool->GetWornGear(Key()));
		TrialRun->StartRunGearAndGold();
		TestTrue(TEXT("the trial wears the kit"), InHand() == KitSword);
		const FWornGear *Start = Pool->GetRunStartWornGear().Find(Key());
		const FWeaponInventoryEntry *StartWeapon = Start ? Pool->FindWeaponInPool(Start->ActiveGearID) : nullptr;
		TestTrue(TEXT("the run-start record is the starting gear, not the kit"), StartWeapon && StartWeapon->Weapon == StartSword);
		Pool->CloseRun(EPoolRunEnding::Quit);
		TestTrue(TEXT("after the run: the starting sword in hand"), InHand() == StartSword);
		TestEqual(TEXT("minted once"), CountOwned(StartSword), 1);
	});

	It("seeds nothing at a trial start for a run-only recruit or a template with no inventory", [this]()
	{
		if (!Party) return;
		UCharacterData *Recruit = FWoRTestWorld::NewCharacterData();
		Recruit->Name = TEXT("Run recruit");
		Recruit->Inventory = Leader->Inventory; // it would have starting gear, but it is run-only
		FPartyMember &RunOnly = Party->Members.AddDefaulted_GetRef();
		RunOnly.CharacterData = Recruit;
		RunOnly.bRunOnly = true;
		Leader->Inventory = nullptr; // and the leader's template authors no inventory
		TrialRun->StartRunGearAndGold();
		TestFalse(TEXT("the run-only recruit isn't seeded"), Pool->GetRunStartWornGear().Contains(UPoolSubsystem::GetCharacterKey(Recruit)));
		TestFalse(TEXT("no inventory: not seeded"), Pool->GetRunStartWornGear().Contains(Key()));
		TestEqual(TEXT("no starting sword minted"), CountOwned(StartSword), 0);
		Pool->EndRunLayer();
	});
}

#endif
