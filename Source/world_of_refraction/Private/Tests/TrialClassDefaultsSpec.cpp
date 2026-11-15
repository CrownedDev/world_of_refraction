// TrialClassDefaultsSpec.cpp
// B-512 (D-221 stage 1c): a trial starts on the class kit whatever the hub wore; the kit is reset each start (tier,
// sockets with their crystals back to the inventory, skills); the character's own evolution and innate spells stay; home
// gear stays owned at home. No kit, or a resumed run: the worn gear stays.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Equipment/Crystals/StatStone.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/ClassDefaultLoadoutData.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	constexpr EItemTier KIT_TEST_RAISED_TIER = EItemTier::C_Tier;
	constexpr int32 KIT_TEST_GEM_DURABILITY = 5;
	constexpr int32 KIT_TEST_PICKED_ATTACK = 2;
	constexpr int32 KIT_TEST_RUN_SEED = 512;
	const FCrystalId KIT_TEST_GEM(ECrystalType::Garnet, EItemTier::F_Tier);

	FRuntimeAttachedItem KitTestGem()
	{
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::Crystal;
		Attached.Crystal = FCrystalAttachment(KIT_TEST_GEM, KIT_TEST_GEM_DURABILITY);
		return Attached;
	}
}

BEGIN_DEFINE_SPEC(FTrialClassDefaultsSpec, "WoR.Trial.ClassDefaults", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UTrialRunSubsystem *TrialRun = nullptr;
	UCharacterData *Member = nullptr;
	UWeaponData *HomeAsset = nullptr;
	UWeaponData *KitAsset = nullptr;
	UAbilityData *KitSkill = nullptr;
	UClassDefaultLoadoutData *Kits = nullptr;
	FGuid HomeSwordID;

	FName Key() const { return UPoolSubsystem::GetCharacterKey(Member); }

	/** The hub: the member owns and wears an S-tier home sword, with a wheel pick, its own evolution and innate spell. */
	void WearHomeSword()
	{
		FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(HomeAsset);
		Entry.PersistentID = FGuid::NewGuid();
		Entry.Tier = EItemTier::S_Tier;
		Pool->SetGearOwner(Entry.PersistentID, Key());
		Pool->SetWearer(Entry.PersistentID, Key());
		Pool->UpdateWeapon(Entry);
		HomeSwordID = Entry.PersistentID;
		FWornGear Worn;
		Worn.Equipment.SetNum(WornGearLayout::FIRST_ADAPTER_INDEX);
		Worn.Equipment[WornGearLayout::PRIMARY_INDEX].GearID = HomeSwordID;
		Worn.ActiveGearID = HomeSwordID;
		Worn.EvolutionID = FGuid::NewGuid();
		Worn.InnateSpells.Add(NewObject<USpellData>(GetTransientPackage()));
		Pool->SetWornGear(Key(), Worn);
		FWheelPick Pick;
		Pick.FieldAttackIndex = KIT_TEST_PICKED_ATTACK;
		Pick.bPicked = true;
		Pool->SetWheelPick(Key(), Pick);
	}

	FGuid KitSwordID() const
	{
		const FClassDefaultKitIDs *IDs = Pool->GetClassDefaultKit(Key());
		return IDs && !IDs->Pieces.IsEmpty() ? IDs->Pieces[WornGearLayout::PRIMARY_INDEX] : FGuid();
	}
END_DEFINE_SPEC(FTrialClassDefaultsSpec)

void FTrialClassDefaultsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		Member = FWoRTestWorld::NewCharacterData();
		Member->Name = TEXT("Kit tester");
		Member->CharacterClass = ECharacterClass::Generic;
		HomeAsset = NewObject<UWeaponData>(GetTransientPackage());
		KitAsset = NewObject<UWeaponData>(GetTransientPackage());
		KitSkill = NewObject<UAbilityData>(GetTransientPackage());
		Kits = NewObject<UClassDefaultLoadoutData>(GetTransientPackage());
		FClassDefaultLoadout &Kit = Kits->Defaults.Add(ECharacterClass::Generic);
		FClassDefaultPiece &Sword = Kit.Pieces.AddDefaulted_GetRef();
		Sword.Piece = KitAsset;
		Sword.SkillPicks.Add(KitSkill);
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Pool"), Pool) || !TestNotNull(TEXT("Trial run"), TrialRun)) return;
		Party->Members.Empty();
		FPartyMember &PartyMember = Party->Members.AddDefaulted_GetRef();
		PartyMember.PawnClass = ACombatCharacter::StaticClass();
		PartyMember.CharacterData = Member;
		TrialRun->SetClassDefaultLoadouts(TSoftObjectPtr<UClassDefaultLoadoutData>(Kits));
	});

	AfterEach([this]()
	{
		Pool = nullptr;
		TrialRun = nullptr;
		TestWorld.Reset();
	});

	It("starts the trial on the class kit whatever the hub wore; home gear stays owned at home", [this]()
	{
		if (!Pool || !TrialRun) return;
		WearHomeSword();
		TrialRun->StartRunGearAndGold();
		const FWornGear *Worn = Pool->GetWornGear(Key());
		const FGuid KitID = KitSwordID();
		if (!TestNotNull(TEXT("a record"), Worn) || !TestTrue(TEXT("a kit sword"), KitID.IsValid())) return;
		TestEqual(TEXT("the kit sword is worn"), Worn->Equipment[WornGearLayout::PRIMARY_INDEX].GearID, KitID);
		TestEqual(TEXT("and in hand"), Worn->ActiveGearID, KitID);
		TestTrue(TEXT("the wheel pick is cleared"), Worn->WheelPick == FWheelPick());
		TestTrue(TEXT("the kit goes on the run"), Pool->IsOnRun(KitID));
		TestFalse(TEXT("the home sword stays home"), Pool->IsOnRun(HomeSwordID));
		TestNotNull(TEXT("still owned"), Pool->FindWeaponInPool(HomeSwordID));
		TestEqual(TEXT("still the member's"), Pool->GetGearOwner(HomeSwordID), Key());
		const FWornGear *Start = Pool->GetRunStartWornGear().Find(Key());
		TestTrue(TEXT("the run-start copy is the hub's (a quit puts it back)"), Start && Start->ActiveGearID == HomeSwordID);
		Pool->EndRunLayer();
	});

	It("resets the kit each start: tier, sockets (the crystal back to the inventory, not a find) and skills", [this]()
	{
		if (!Pool || !TrialRun) return;
		TrialRun->StartRunGearAndGold();
		Pool->EndRunLayer();
		const FGuid KitID = KitSwordID();
		const FWeaponInventoryEntry *Kept = Pool->FindWeaponInPool(KitID);
		if (!TestNotNull(TEXT("the kit sword"), Kept)) return;
		FWeaponInventoryEntry Changed = *Kept; // what a kept run left on it
		Changed.Tier = KIT_TEST_RAISED_TIER;
		Changed.AttachedItem = KitTestGem();
		Changed.SkillPicks.Reset();
		Pool->UpdateWeapon(Changed);
		const int32 HeldBefore = Pool->GetCarriedCount(Key(), KIT_TEST_GEM) + Pool->GetStoredCount(KIT_TEST_GEM);

		TrialRun->StartRunGearAndGold();
		const FWeaponInventoryEntry *Reset = Pool->FindWeaponInPool(KitID);
		if (!TestNotNull(TEXT("the same kit sword"), Reset)) return;
		TestEqual(TEXT("its authored tier"), Reset->Tier, FWeaponInventoryEntry::CreateFromWeapon(KitAsset).Tier);
		TestTrue(TEXT("its socket empty"), Reset->AttachedItem.IsEmpty());
		TestTrue(TEXT("its authored skills"), Reset->bSkillSlotsActive && Reset->SkillPicks.Num() == 1 && Reset->SkillPicks[0] == KitSkill);
		TestEqual(TEXT("the crystal is back in the inventory"),
				  Pool->GetCarriedCount(Key(), KIT_TEST_GEM) + Pool->GetStoredCount(KIT_TEST_GEM), HeldBefore + 1);
		const int32 *AtStart = Pool->GetRunStartItems().Find(StatStone::Normalize(KIT_TEST_GEM));
		TestTrue(TEXT("held at the run's start, so not a find"), AtStart && *AtStart == HeldBefore + 1);
		Pool->EndRunLayer();
	});

	It("keeps the character's own evolution and innate spells", [this]()
	{
		if (!Pool || !TrialRun) return;
		WearHomeSword();
		const FWornGear Hub = *Pool->GetWornGear(Key());
		TrialRun->StartRunGearAndGold();
		const FWornGear *Worn = Pool->GetWornGear(Key());
		if (!TestNotNull(TEXT("a record"), Worn)) return;
		TestEqual(TEXT("its evolution"), Worn->EvolutionID, Hub.EvolutionID);
		TestTrue(TEXT("its innate spells"), Worn->InnateSpells == Hub.InnateSpells);
		Pool->EndRunLayer();
	});

	It("keeps the worn gear with no kit, and on a resumed run", [this]()
	{
		if (!Pool || !TrialRun) return;
		WearHomeSword();
		TrialRun->SetClassDefaultLoadouts(TSoftObjectPtr<UClassDefaultLoadoutData>());
		TrialRun->StartRunGearAndGold();
		TestEqual(TEXT("no kit: the home sword is still worn"), Pool->GetWornGear(Key())->ActiveGearID, HomeSwordID);
		TestTrue(TEXT("and goes on the run"), Pool->IsOnRun(HomeSwordID));
		TestNull(TEXT("nothing minted"), Pool->GetClassDefaultKit(Key()));
		Pool->EndRunLayer();

		TrialRun->SetClassDefaultLoadouts(TSoftObjectPtr<UClassDefaultLoadoutData>(Kits));
		TrialRun->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), KIT_TEST_RUN_SEED, 0, ETrialDifficulty::Medium, {}, {}, {});
		TestEqual(TEXT("a resumed run wears what was saved"), Pool->GetWornGear(Key())->ActiveGearID, HomeSwordID);
		TestNull(TEXT("and mints nothing"), Pool->GetClassDefaultKit(Key()));
		Pool->EndRunLayer();
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
