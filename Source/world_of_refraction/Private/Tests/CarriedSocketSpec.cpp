// CarriedSocketSpec.cpp
// CS-4 (docs/Audits/CharacterStateAudit.md, clash 4): a crystal swapped on the trial map between fights stays swapped
// at the next fight. The last fight's carried copy (FPartyMember::CarriedGear) used to put the old crystal back over
// the owned piece. A removed crystal is destroyed, not returned to the pouch (CL-28b).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"

namespace
{
	const FCrystalId CARRIED_SOCKET_OLD_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	const FCrystalId CARRIED_SOCKET_NEW_GEM(ECrystalType::Sapphire, EItemTier::F_Tier);
	constexpr int32 CARRIED_SOCKET_WORN_DURABILITY = 4;
}

BEGIN_DEFINE_SPEC(FCarriedSocketSpec, "WoR.Party.CarriedSocket", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPartySessionSubsystem *PartySession = nullptr;
	UCrystalInventoryComponent *Crystals = nullptr;
	UInventoryComponent *Inventory = nullptr;
	ULoadoutComponent *Loadout = nullptr;
	int32 Slot = INDEX_NONE;
	FGuid SwordID;

	const FWeaponInventoryEntry *OwnedSword() const { return Inventory->FindOwnedWeapon(SwordID); }
END_DEFINE_SPEC(FCarriedSocketSpec)

void FCarriedSocketSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party)) return;
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		FPartyMember Member;
		Member.PawnClass = ACombatCharacter::StaticClass();
		Member.CharacterData = Data;
		Party->Members.Add(Member);
		Slot = PartySession->GetMemberSlotByData(Data);

		// The member's pawn wears an owned sword with the old gem, as the last fight left it.
		AActor *Pawn = TestWorld->SpawnCombatant(Data);
		Crystals = FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Pawn);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Pawn);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Pawn);
		Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Pawn);
		Loadout = FWoRTestWorld::AddComponent<ULoadoutComponent>(Pawn);
		Loadout->InitializeFromCharacterData(Data, Inventory);
		if (!TestTrue(TEXT("Sword owned"), Inventory->AddWeapon(NewObject<UWeaponData>(GetTransientPackage())))) return;
		SwordID = Inventory->Weapons.Last().PersistentID;
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.PrimarySlotType = EPrimarySlotType::Weapon;
		Active.PrimaryWeapon.WeaponEntry = Inventory->Weapons.Last();
		Inventory->AddCrystal(CARRIED_SOCKET_OLD_GEM);
		TestTrue(TEXT("Old gem socketed"), Inventory->AttachCrystalToWeapon(SwordID, CARRIED_SOCKET_OLD_GEM));
	});

	AfterEach([this]()
	{
		PartySession = nullptr;
		TestWorld.Reset();
	});

	It("keeps a gem swapped between fights at the next fight, and the removed gem stays destroyed", [this]()
	{
		if (!TestTrue(TEXT("Set up"), Slot != INDEX_NONE && OwnedSword())) return;
		PartySession->SetMemberCarriedState(Slot, Loadout->SnapshotCarriedGear(), FCarriedAbsorptionState()); // fight one ends

		// On the map: the old gem comes out (destroyed) and the new one goes in.
		const int32 SwordIndex = Inventory->Weapons.IndexOfByPredicate([this](const FWeaponInventoryEntry &E) { return E.PersistentID == SwordID; });
		TestTrue(TEXT("Old gem removed"), Inventory->RemoveCrystalFromWeapon(SwordIndex));
		Inventory->AddCrystal(CARRIED_SOCKET_NEW_GEM);
		TestTrue(TEXT("New gem socketed"), Inventory->AttachCrystalToWeapon(SwordID, CARRIED_SOCKET_NEW_GEM));

		// Fight two: the pawn's BeginPlay applies the carry (CharacterDataComponent.cpp, B-115).
		Loadout->ApplyCarriedGear(PartySession->GetMemberCarriedState(Slot)->CarriedGear);
		TestTrue(TEXT("The owned sword holds the new gem"), OwnedSword()->AttachedItem.Crystal.Id == CARRIED_SOCKET_NEW_GEM);
		TestTrue(TEXT("The worn sword holds the new gem"),
				 Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].PrimaryWeapon.WeaponEntry.AttachedItem.Crystal.Id == CARRIED_SOCKET_NEW_GEM);
		TestEqual(TEXT("The old gem is not back in the pouch"), Crystals->GetCount(CARRIED_SOCKET_OLD_GEM), 0);
		TestEqual(TEXT("The new gem is not in the pouch too"), Crystals->GetCount(CARRIED_SOCKET_NEW_GEM), 0);
	});

	It("still carries the last fight's wear when nothing was swapped", [this]()
	{
		if (!TestTrue(TEXT("Set up"), Slot != INDEX_NONE && OwnedSword())) return;
		Loadout->FindAttachedItemByHolder(OwnedSword()->Weapon)->Crystal.CurrentDurability = CARRIED_SOCKET_WORN_DURABILITY; // the fight's wear
		PartySession->SetMemberCarriedState(Slot, Loadout->SnapshotCarriedGear(), FCarriedAbsorptionState());
		Loadout->ApplyCarriedGear(PartySession->GetMemberCarriedState(Slot)->CarriedGear);
		TestEqual(TEXT("The owned gem keeps its wear"), OwnedSword()->AttachedItem.GetCurrentDurability(), CARRIED_SOCKET_WORN_DURABILITY);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
