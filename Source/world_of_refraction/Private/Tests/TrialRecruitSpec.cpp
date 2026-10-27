// TrialRecruitSpec.cpp
// B-076 (D-026, Q-076): a character found during a trial joins for that run only. It arrives
// ungeared (nothing of its own goes into the shared armoury), uses the armoury pieces it's given,
// leaves when the run ends, and a saved run keeps it.

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
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"

namespace
{
	constexpr int32 TRIAL_RECRUIT_SLOT = 1;
}

BEGIN_DEFINE_SPEC(FTrialRecruitSpec, "WoR.Party.TrialRecruit",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPartySessionSubsystem *PartySession = nullptr;
	UPoolSubsystem *Pool = nullptr;
	UWeaponData *Sword = nullptr;
	UCharacterData *Leader = nullptr;
	UCharacterData *Recruit = nullptr;

	/** Spawns a party member as each level does: its authored gear and a preset naming it, then the shared draw. */
	UInventoryComponent *Spawn(UCharacterData *Data, bool bBringsSword)
	{
		AActor *Actor = TestWorld->SpawnCombatant(Data);
		if (!Actor)
		{
			return nullptr;
		}
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Actor);
		UInventoryComponent *Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Actor);
		FCombatLoadout Preset;
		if (bBringsSword)
		{
			Inventory->AddWeapon(Sword);
			Preset.PrimarySlotType = EPrimarySlotType::Weapon;
			Preset.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		}
		Inventory->SavedLoadouts = {Preset};
		Inventory->ActiveLoadoutIndex = 0;
		Inventory->LinkLoadoutsToOwned();
		Inventory->DrawSharedGear();
		return Inventory;
	}

	static FGuid ActiveWeaponID(const UInventoryComponent *Inventory)
	{
		return Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].PrimaryWeapon.WeaponEntry.PersistentID;
	}
END_DEFINE_SPEC(FTrialRecruitSpec)

void FTrialRecruitSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Pool"), Pool)) return;
		Leader = FWoRTestWorld::NewCharacterData();
		Leader->Name = TEXT("Leader");
		Recruit = FWoRTestWorld::NewCharacterData();
		Recruit->Name = TEXT("Recruit");
		Party->Members.Empty();
		FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
		Member.PawnClass = ACombatCharacter::StaticClass();
		Member.CharacterData = Leader;
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Fire Sword");
	});

	AfterEach([this]()
	{
		PartySession = nullptr;
		Pool = nullptr;
		Sword = nullptr;
		Leader = nullptr;
		Recruit = nullptr;
		TestWorld.Reset();
	});

	It("joins a recruit for this run only and lets it go when the run ends", [this]()
	{
		if (!Pool) return;
		TestTrue(TEXT("Invited"), PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit));
		const UParty *Party = PartySession->GetLocalParty();
		if (!TestEqual(TEXT("Two members"), Party->Members.Num(), 2)) return;
		TestTrue(TEXT("The recruit is run-only"), Party->Members[TRIAL_RECRUIT_SLOT].bRunOnly);
		TestFalse(TEXT("The leader isn't"), Party->Members[0].bRunOnly);
		TestFalse(TEXT("The leader's gear isn't a recruit's to set"), PartySession->SetRunMemberGear(0, FGuid::NewGuid(), FGuid()));

		TestEqual(TEXT("One leaves"), PartySession->DismissRunMembers(), 1);
		TestTrue(TEXT("Only the leader stays"), Party->Members.Num() == 1 && Party->Members[0].CharacterData == Leader);
		TestEqual(TEXT("Nobody else to dismiss"), PartySession->DismissRunMembers(), 0);
	});

	It("brings none of the recruit's own gear into the armoury", [this]()
	{
		if (!Pool) return;
		PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit);
		UInventoryComponent *Inventory = Spawn(Recruit, true);
		if (!Inventory) return;
		TestEqual(TEXT("No sword in the armoury"), Pool->GetOwnedWeapons().Num(), 0);
		TestFalse(TEXT("Nothing banked for the recruit"), Pool->HasBankedGear(UPoolSubsystem::GetCharacterKey(Recruit)));
		TestEqual(TEXT("Its inventory shows the (empty) armoury"), Inventory->Weapons.Num(), 0);
		TestFalse(TEXT("It holds no weapon"), ActiveWeaponID(Inventory).IsValid());
	});

	It("gives the recruit the armoury weapon it was handed", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *LeaderInventory = Spawn(Leader, true);
		if (!LeaderInventory) return;
		const FGuid SwordID = Pool->GetOwnedWeapons()[0].PersistentID;
		LeaderInventory->SavedLoadouts[0] = FCombatLoadout();
		LeaderInventory->ClaimActiveSharedGear();
		TestFalse(TEXT("The leader put the sword down"), Pool->GetWearer(SwordID) == UPoolSubsystem::GetCharacterKey(Leader));

		PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit);
		TestTrue(TEXT("Gear set"), PartySession->SetRunMemberGear(TRIAL_RECRUIT_SLOT, SwordID, FGuid()));
		UInventoryComponent *Inventory = Spawn(Recruit, false);
		if (!Inventory) return;
		TestEqual(TEXT("The recruit holds the sword"), ActiveWeaponID(Inventory), SwordID);
		TestEqual(TEXT("And wears it"), Pool->GetWearer(SwordID), UPoolSubsystem::GetCharacterKey(Recruit));
	});

	It("keeps a piece another member wears with that member", [this]()
	{
		if (!Pool) return;
		Spawn(Leader, true);
		const FGuid SwordID = Pool->GetOwnedWeapons()[0].PersistentID;
		PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit);
		PartySession->SetRunMemberGear(TRIAL_RECRUIT_SLOT, SwordID, FGuid());
		UInventoryComponent *Inventory = Spawn(Recruit, false);
		if (!Inventory) return;
		TestFalse(TEXT("The recruit's loadout lost the sword"), ActiveWeaponID(Inventory).IsValid());
		TestEqual(TEXT("The leader still wears it"), Pool->GetWearer(SwordID), UPoolSubsystem::GetCharacterKey(Leader));
		TestEqual(TEXT("The clash is noted"), Pool->GetClashes().Num(), 1);
	});

	It("saves a recruit only with the run, and brings it back with its gear", [this]()
	{
		if (!Pool) return;
		PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit);
		const FGuid SwordID = FGuid::NewGuid();
		PartySession->SetRunMemberGear(TRIAL_RECRUIT_SLOT, SwordID, FGuid());
		const UParty *Party = PartySession->GetLocalParty();

		TestEqual(TEXT("Outside a run, the recruit isn't saved"), USaveSubsystem::CaptureParty(Party, false).Members.Num(), 1);
		const FSavedParty Saved = USaveSubsystem::CaptureParty(Party, true);
		if (!TestEqual(TEXT("With the run, both are saved"), Saved.Members.Num(), 2)) return;
		// Restored in a fresh session, as after a restart: an existing party is never overwritten.
		FWoRTestWorld After;
		UPartySessionSubsystem *Resumed = After.Create() ? After.GetSubsystem<UPartySessionSubsystem>() : nullptr;
		if (!TestNotNull(TEXT("Party session after"), Resumed)) return;
		TestTrue(TEXT("Restored"), USaveSubsystem::RestoreParty(Saved, *Resumed));
		const UParty *Restored = Resumed->GetLocalParty();
		if (!TestNotNull(TEXT("Restored party"), Restored)) return;
		if (!TestEqual(TEXT("Two members back"), Restored->Members.Num(), 2)) return;
		const FPartyMember &Back = Restored->Members[TRIAL_RECRUIT_SLOT];
		TestTrue(TEXT("Still run-only"), Back.bRunOnly);
		TestEqual(TEXT("Same weapon"), Back.RunWeapon, SwordID);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
