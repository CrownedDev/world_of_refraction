// TrialDuplicatesSpec.cpp
// B-570 (D-225, TrialPoolDesign §6, stage 6): a dropped weapon the character already has one of, and its three choices,
// on a party member spawned headless. Unproven here: the prompt itself and its opening on the trial map (PIE sheet B-570).

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyYield.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Misc/AutomationTest.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Skills/Definitions/SpellData.h"
#include "Trial/TrialDuplicates.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRTestWorld.h"

BEGIN_DEFINE_SPEC(FTrialDuplicatesSpec, "WoR.Trial.Duplicates", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	static constexpr EItemTier DROP_TIER = EItemTier::D_Tier;
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UWeaponData *Sword = nullptr;
	UCharacterData *Data = nullptr;
	AActor *Actor = nullptr;
	UInventoryComponent *Inventory = nullptr;
	UCurrencyComponent *Wallet = nullptr;

	/** The second sword, as a won fight drops it; the waiting duplicate it makes. */
	FTrialDuplicate DropSecondSword()
	{
		FTrialDuplicate Duplicate;
		Duplicate.CharacterKey = UPoolSubsystem::GetCharacterKey(Data);
		if (Inventory->AddDroppedGear(Sword, DROP_TIER))
		{
			Duplicate.NewID = Inventory->GetWeaponAt(Inventory->GetWeaponCount() - 1).PersistentID;
			Duplicate.HeldID = TrialDuplicates::FindHeldWeapon(*Inventory, Sword, Duplicate.NewID);
		}
		return Duplicate;
	}

	bool Owns(const FGuid &ID) const
	{
		return Inventory->Weapons.ContainsByPredicate([&ID](const FWeaponInventoryEntry &Entry) { return Entry.PersistentID == ID; });
	}
END_DEFINE_SPEC(FTrialDuplicatesSpec)

void FTrialDuplicatesSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Duplicate sword");
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Pool"), Pool)) return;
		Data = FWoRTestWorld::NewCharacterData();
		Data->Name = TEXT("Duplicate member");
		Party->Members.Empty();
		FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
		Member.PawnClass = ACombatCharacter::StaticClass();
		Member.CharacterData = Data;
		Actor = TestWorld->SpawnCombatant(Data);
		if (!TestNotNull(TEXT("Actor"), Actor)) return;
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Actor);
		Wallet = FWoRTestWorld::AddComponent<UCurrencyComponent>(Actor);
		Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Actor);
		Inventory->AddWeapon(Sword);
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Inventory->SavedLoadouts = {Loadout};
		Inventory->ActiveLoadoutIndex = 0;
		Inventory->LinkLoadoutsToOwned();
		Inventory->DrawSharedGear();
	});

	AfterEach([this]()
	{
		Pool = nullptr;
		Sword = nullptr;
		Data = nullptr;
		Actor = nullptr;
		Inventory = nullptr;
		Wallet = nullptr;
		TestWorld.Reset();
	});

	It("finds the held copy of a dropped weapon and shows both tiers and the essence", [this]()
	{
		if (!Inventory || !Pool) return;
		TestFalse(TEXT("one sword is no duplicate"), TrialDuplicates::FindHeldWeapon(*Inventory, Sword, Inventory->GetWeaponAt(0).PersistentID).IsValid());
		const FTrialDuplicate Duplicate = DropSecondSword();
		if (!TestTrue(TEXT("the second sword is a duplicate"), Duplicate.IsSet())) return;
		FDuplicateDropView View;
		if (!TestTrue(TEXT("the prompt has both pieces"), TrialDuplicates::MakeView(*Pool, Duplicate, View))) return;
		TestEqual(TEXT("the dropped one's tier"), View.Drop.Tier, DROP_TIER);
		TestEqual(TEXT("the held one's tier"), View.HeldTier, Inventory->GetWeaponAt(0).Tier);
		TestEqual(TEXT("the essence a convert gives"), View.EssenceValue, EconomyYield::GetLevelingEssenceYieldForTier(DROP_TIER));

		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("trial run"), TrialRun)) return;
		TrialRun->AddPendingDuplicate(FTrialDuplicate()); // unset: not kept
		TrialRun->AddPendingDuplicate(Duplicate);
		TestEqual(TEXT("one waits"), TrialRun->GetPendingDuplicates().Num(), 1);
		FTrialDuplicate Taken;
		TestTrue(TEXT("taken once"), TrialRun->TakePendingDuplicate(Taken) && Taken.NewID == Duplicate.NewID && !TrialRun->TakePendingDuplicate(Taken));
	});

	It("Add keeps both; Convert dismantles the new one for Gear essence", [this]()
	{
		if (!Inventory || !Wallet) return;
		const FTrialDuplicate Duplicate = DropSecondSword();
		if (!TestTrue(TEXT("a duplicate"), Duplicate.IsSet())) return;
		TestTrue(TEXT("Add does nothing"), TrialDuplicates::Resolve(Actor->GetWorld(), Duplicate, EDuplicateChoice::Add).IsEmpty());
		TestTrue(TEXT("both kept"), Owns(Duplicate.NewID) && Owns(Duplicate.HeldID));

		const int32 Before = Wallet->GetGearEssence();
		TestTrue(TEXT("Convert runs"), TrialDuplicates::Resolve(Actor->GetWorld(), Duplicate, EDuplicateChoice::Convert).IsEmpty());
		TestTrue(TEXT("the new one is gone, the held one stays"), !Owns(Duplicate.NewID) && Owns(Duplicate.HeldID));
		TestEqual(TEXT("its tier's essence is paid"), Wallet->GetGearEssence() - Before, EconomyYield::GetLevelingEssenceYieldForTier(DROP_TIER));
	});

	It("Replace puts the new one in the held one's place; the held one stays, unworn", [this]()
	{
		if (!Inventory || !Wallet) return;
		const FTrialDuplicate Duplicate = DropSecondSword();
		if (!TestTrue(TEXT("a duplicate"), Duplicate.IsSet())) return;
		TestTrue(TEXT("the held one is the primary"), Inventory->IsPrimaryInAnyLoadout(Duplicate.HeldID));
		TestTrue(TEXT("Replace runs"), TrialDuplicates::Resolve(Actor->GetWorld(), Duplicate, EDuplicateChoice::Replace).IsEmpty());
		TestTrue(TEXT("the new one is the primary"), Inventory->IsPrimaryInAnyLoadout(Duplicate.NewID));
		TestTrue(TEXT("the held one stays, unworn"), Owns(Duplicate.HeldID) && Owns(Duplicate.NewID) && !Inventory->IsPrimaryInAnyLoadout(Duplicate.HeldID));
	});

	It("converts a dropped skill the character already knows to Skill essence", [this]()
	{
		if (!Actor || !Wallet) return;
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Tier = EItemTier::E_Tier;
		const int32 Before = Wallet->GetSkillEssence();
		const int32 Essence = TrialDuplicates::ConvertKnownSkill(Actor, Spell);
		TestEqual(TEXT("its tier's essence"), Essence, EconomyYield::GetLevelingEssenceYieldForTier(EItemTier::E_Tier));
		TestEqual(TEXT("paid to the wallet"), Wallet->GetSkillEssence() - Before, Essence);
		FTrialDropView Shown;
		Shown.Tier = EItemTier::E_Tier;
		TestTrue(TEXT("the line says so"), TrialDuplicates::GetConvertedLine(Shown, Essence).ToString().Contains(TEXT("already known")));
	});
}

#endif
