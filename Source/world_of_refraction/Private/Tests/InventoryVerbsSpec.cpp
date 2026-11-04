// InventoryVerbsSpec.cpp
// B-339 (D-131): the one inventory screen's verbs route to the existing rules. Each row lists the verbs that apply
// (own gear: Give, Store, Dismantle, and Convert for a duplicate's weaker copy; own stacks: Give, Store, Dismantle;
// chest gear and stored stacks: Take), Run refuses a verb that doesn't apply, and each verb does what its old screen did.
// Review cases: worn gear isn't stored, a primary isn't given (D-116), no gear goes to a run-only member (Q-321), and a
// home piece isn't dismantled during a run (Q-314b), each with its reason.

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
#include "Inventory/InventoryVerbs.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"

namespace
{
	constexpr int32 VERBS_PARTY_SIZE = 2;
	constexpr int32 VERBS_MEMBER = 0;
	constexpr int32 VERBS_RECEIVER = 1;
	constexpr int32 VERBS_GEM_COUNT = 3;
	constexpr int32 VERBS_ONE = 1;
	const FCrystalId VERBS_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
}

BEGIN_DEFINE_SPEC(FInventoryVerbsSpec, "WoR.Inventory.Verbs", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UWeaponData *Sword = nullptr;
	UWeaponData *Dagger = nullptr;
	TArray<UCharacterData *> Members;
	TArray<UInventoryComponent *> Inventories;

	FName KeyOf(int32 Index) const { return UPoolSubsystem::GetCharacterKey(Members[Index]); }
	UWorld *World() const { return Inventories[VERBS_MEMBER]->GetWorld(); }

	/** Party member Index as each level spawns it: the sword as its primary, then the draw. */
	UInventoryComponent *SpawnMember(int32 Index)
	{
		AActor *Actor = TestWorld->SpawnCombatant(Members[Index]);
		if (!Actor)
		{
			return nullptr;
		}
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Actor);
		UInventoryComponent *Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Actor);
		Inventory->AddWeapon(Sword);
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Inventory->SavedLoadouts = {Loadout};
		Inventory->ActiveLoadoutIndex = 0;
		Inventory->LinkLoadoutsToOwned();
		Inventory->DrawSharedGear();
		return Inventory;
	}

	/** The member's row for GearID, or for the stack Item on the given side; null when not listed. */
	TOptional<WoRInventoryVerbs::FInventoryRow> FindRow(const FGuid &GearID, const FCrystalId &Item, bool bInChest) const
	{
		for (const WoRInventoryVerbs::FInventoryRow &Row : WoRInventoryVerbs::GatherRows(World(), KeyOf(VERBS_MEMBER)))
		{
			const bool bMatch = GearID.IsValid() ? Row.GearID == GearID : (!Row.IsGear() && Row.Item == Item);
			if (bMatch && Row.bInChest == bInChest)
			{
				return Row;
			}
		}
		return {};
	}

	TOptional<WoRInventoryVerbs::FInventoryRow> GearRow(const FGuid &GearID, bool bInChest = false) const
	{
		return FindRow(GearID, FCrystalId(), bInChest);
	}

	FGuid AddDagger()
	{
		Inventories[VERBS_MEMBER]->AddWeapon(Dagger);
		const FWeaponInventoryEntry *Entry =
			Inventories[VERBS_MEMBER]->Weapons.FindByPredicate([this](const FWeaponInventoryEntry &E) { return E.Weapon == Dagger; });
		return Entry ? Entry->PersistentID : FGuid();
	}

	TArray<EInventoryVerb> VerbsOf(const WoRInventoryVerbs::FInventoryRow &Row) const
	{
		return WoRInventoryVerbs::GetVerbsFor(World(), KeyOf(VERBS_MEMBER), KeyOf(VERBS_RECEIVER), Row);
	}

	FGuid SwordID() const
	{
		const FWeaponInventoryEntry *Entry =
			Inventories[VERBS_MEMBER]->Weapons.FindByPredicate([this](const FWeaponInventoryEntry &E) { return E.Weapon == Sword; });
		return Entry ? Entry->PersistentID : FGuid();
	}

	FText RunOn(EInventoryVerb Verb, const WoRInventoryVerbs::FInventoryRow &Row, int32 Count = VERBS_ONE)
	{
		return WoRInventoryVerbs::Run(World(), Verb, KeyOf(VERBS_MEMBER), KeyOf(VERBS_RECEIVER), Row, Count);
	}
END_DEFINE_SPEC(FInventoryVerbsSpec)

void FInventoryVerbsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Verbs sword");
		Dagger = NewObject<UWeaponData>(GetTransientPackage());
		Dagger->Name = TEXT("Verbs dagger");
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Pool"), Pool)) return;
		Party->Members.Empty();
		for (int32 Index = 0; Index < VERBS_PARTY_SIZE; ++Index)
		{
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->Name = FString::Printf(TEXT("Verbs member %d"), Index);
			FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
			Member.PawnClass = ACombatCharacter::StaticClass();
			Member.CharacterData = Data;
			Members.Add(Data);
		}
		for (int32 Index = 0; Index < VERBS_PARTY_SIZE; ++Index)
		{
			Inventories.Add(SpawnMember(Index));
		}
	});

	AfterEach([this]()
	{
		Pool = nullptr;
		Sword = nullptr;
		Dagger = nullptr;
		Members.Empty();
		Inventories.Empty();
		TestWorld.Reset();
	});

	It("lists the verbs each row allows", [this]()
	{
		if (!Pool || Inventories.Num() < VERBS_PARTY_SIZE || Inventories.Contains(nullptr)) return;
		const FGuid DaggerID = AddDagger();
		const TOptional<WoRInventoryVerbs::FInventoryRow> Own = GearRow(DaggerID);
		if (!TestTrue(TEXT("the dagger is listed as the member's"), Own.IsSet())) return;
		const TArray<EInventoryVerb> Verbs = VerbsOf(Own.GetValue());
		TestTrue(TEXT("own gear: Give, Store, Dismantle"), Verbs.Contains(EInventoryVerb::Give) && Verbs.Contains(EInventoryVerb::Store) &&
															   Verbs.Contains(EInventoryVerb::Dismantle));
		TestFalse(TEXT("own gear: no Take"), Verbs.Contains(EInventoryVerb::Take));
		TestFalse(TEXT("not a duplicate: no Convert"), Verbs.Contains(EInventoryVerb::Convert));
		TestFalse(TEXT("a verb that doesn't apply is refused"), RunOn(EInventoryVerb::Take, Own.GetValue()).IsEmpty());
		TestTrue(TEXT("and changes nothing"), Pool->GetGearOwner(DaggerID) == KeyOf(VERBS_MEMBER));
	});

	It("Give moves the piece to the receiver", [this]()
	{
		if (!Pool || Inventories.Num() < VERBS_PARTY_SIZE || Inventories.Contains(nullptr)) return;
		const FGuid DaggerID = AddDagger();
		const TOptional<WoRInventoryVerbs::FInventoryRow> Own = GearRow(DaggerID);
		if (!TestTrue(TEXT("listed"), Own.IsSet())) return;
		TestTrue(TEXT("given"), RunOn(EInventoryVerb::Give, Own.GetValue()).IsEmpty());
		TestTrue(TEXT("the receiver owns it"), Pool->GetGearOwner(DaggerID) == KeyOf(VERBS_RECEIVER));
		TestFalse(TEXT("no longer listed as the member's"), GearRow(DaggerID).IsSet());
	});

	It("Store puts gear in the chest and Take brings it back", [this]()
	{
		if (!Pool || Inventories.Num() < VERBS_PARTY_SIZE || Inventories.Contains(nullptr)) return;
		const FGuid DaggerID = AddDagger();
		const TOptional<WoRInventoryVerbs::FInventoryRow> Own = GearRow(DaggerID);
		if (!TestTrue(TEXT("listed"), Own.IsSet())) return;
		TestTrue(TEXT("stored"), RunOn(EInventoryVerb::Store, Own.GetValue()).IsEmpty());
		TestTrue(TEXT("in the chest"), Pool->IsInChest(DaggerID));
		const TOptional<WoRInventoryVerbs::FInventoryRow> InChest = GearRow(DaggerID, /*bInChest=*/true);
		if (!TestTrue(TEXT("listed under the chest"), InChest.IsSet())) return;
		const TArray<EInventoryVerb> TakeOnly = {EInventoryVerb::Take};
		TestTrue(TEXT("chest gear: Take only"), VerbsOf(InChest.GetValue()) == TakeOnly);
		TestTrue(TEXT("taken"), RunOn(EInventoryVerb::Take, InChest.GetValue()).IsEmpty());
		TestTrue(TEXT("the member's again"), Pool->GetGearOwner(DaggerID) == KeyOf(VERBS_MEMBER));
	});

	It("Store and Take move a pouch stack to and from storage", [this]()
	{
		if (!Pool || Inventories.Num() < VERBS_PARTY_SIZE || Inventories.Contains(nullptr)) return;
		Pool->CarryOrStore(KeyOf(VERBS_MEMBER), VERBS_GEM, VERBS_GEM_COUNT);
		const int32 StoredBefore = Pool->GetStoredCount(VERBS_GEM);
		const TOptional<WoRInventoryVerbs::FInventoryRow> Stack = FindRow(FGuid(), VERBS_GEM, false);
		if (!TestTrue(TEXT("the pouch stack is listed"), Stack.IsSet())) return;
		TestTrue(TEXT("whole stack stored"), RunOn(EInventoryVerb::Store, Stack.GetValue(), VERBS_GEM_COUNT).IsEmpty());
		TestEqual(TEXT("none carried"), Pool->GetCarriedCount(KeyOf(VERBS_MEMBER), VERBS_GEM), 0);
		TestEqual(TEXT("all stored"), Pool->GetStoredCount(VERBS_GEM), StoredBefore + VERBS_GEM_COUNT);
		const TOptional<WoRInventoryVerbs::FInventoryRow> Stored = FindRow(FGuid(), VERBS_GEM, true);
		if (!TestTrue(TEXT("the stored stack is listed"), Stored.IsSet())) return;
		TestTrue(TEXT("one taken"), RunOn(EInventoryVerb::Take, Stored.GetValue()).IsEmpty());
		TestEqual(TEXT("one carried"), Pool->GetCarriedCount(KeyOf(VERBS_MEMBER), VERBS_GEM), VERBS_ONE);
	});

	It("Dismantle takes a weapon apart", [this]()
	{
		if (!Pool || Inventories.Num() < VERBS_PARTY_SIZE || Inventories.Contains(nullptr)) return;
		const FGuid DaggerID = AddDagger();
		const TOptional<WoRInventoryVerbs::FInventoryRow> Own = GearRow(DaggerID);
		if (!TestTrue(TEXT("listed"), Own.IsSet())) return;
		TestTrue(TEXT("dismantled"), RunOn(EInventoryVerb::Dismantle, Own.GetValue()).IsEmpty());
		TestNull(TEXT("gone from the store"), Pool->FindWeaponInPool(DaggerID));
	});

	It("Convert turns a duplicate's weaker copy into Gold", [this]()
	{
		if (!Pool || Inventories.Num() < VERBS_PARTY_SIZE || Inventories.Contains(nullptr)) return;
		UInventoryComponent *Inventory = Inventories[VERBS_MEMBER];
		Inventory->AddWeapon(Sword); // a second sword: the newer copy is the weaker on a tie (D-115)
		const TArray<FDuplicateGearPair> Pairs = Inventory->FindDuplicateGear();
		if (!TestEqual(TEXT("one duplicate"), Pairs.Num(), 1)) return;
		const TOptional<WoRInventoryVerbs::FInventoryRow> Weaker = GearRow(Pairs[0].WeakerID);
		if (!TestTrue(TEXT("the weaker copy is listed"), Weaker.IsSet())) return;
		TestTrue(TEXT("Convert applies to it"),
				 VerbsOf(Weaker.GetValue()).Contains(EInventoryVerb::Convert));
		TestTrue(TEXT("converted"), RunOn(EInventoryVerb::Convert, Weaker.GetValue()).IsEmpty());
		TestNull(TEXT("the weaker copy is gone"), Pool->FindWeaponInPool(Pairs[0].WeakerID));
		TestNotNull(TEXT("the kept copy stays"), Pool->FindWeaponInPool(Pairs[0].KeptID));
	});

	It("refuses Store and Give on the worn primary, with the reason", [this]()
	{
		if (!Pool || Inventories.Num() < VERBS_PARTY_SIZE || Inventories.Contains(nullptr)) return;
		const TOptional<WoRInventoryVerbs::FInventoryRow> Primary = GearRow(SwordID());
		if (!TestTrue(TEXT("the worn sword is listed"), Primary.IsSet())) return;
		const TArray<EInventoryVerb> Verbs = VerbsOf(Primary.GetValue());
		TestFalse(TEXT("worn gear: no Store"), Verbs.Contains(EInventoryVerb::Store));
		TestFalse(TEXT("a primary: no Give (D-116)"), Verbs.Contains(EInventoryVerb::Give));
		TestFalse(TEXT("a primary: no Dismantle"), Verbs.Contains(EInventoryVerb::Dismantle));
		TestFalse(TEXT("Store refused with a reason"), RunOn(EInventoryVerb::Store, Primary.GetValue()).IsEmpty());
		TestFalse(TEXT("Give refused with a reason"), RunOn(EInventoryVerb::Give, Primary.GetValue()).IsEmpty());
		TestFalse(TEXT("not in the chest"), Pool->IsInChest(SwordID()));
		TestTrue(TEXT("still the member's"), Pool->GetGearOwner(SwordID()) == KeyOf(VERBS_MEMBER));
	});

	It("refuses to give gear to a run-only member", [this]()
	{
		if (!Pool || Inventories.Num() < VERBS_PARTY_SIZE || Inventories.Contains(nullptr)) return;
		UParty *Party = TestWorld->GetSubsystem<UPartySessionSubsystem>()->GetLocalParty();
		if (!TestNotNull(TEXT("Party"), Party)) return;
		Party->Members[VERBS_RECEIVER].bRunOnly = true;
		const FGuid DaggerID = AddDagger();
		const TOptional<WoRInventoryVerbs::FInventoryRow> Own = GearRow(DaggerID);
		if (!TestTrue(TEXT("listed"), Own.IsSet())) return;
		TestFalse(TEXT("no Give to a recruit"), VerbsOf(Own.GetValue()).Contains(EInventoryVerb::Give));
		TestFalse(TEXT("refused with a reason"), RunOn(EInventoryVerb::Give, Own.GetValue()).IsEmpty());
		TestTrue(TEXT("still the member's"), Pool->GetGearOwner(DaggerID) == KeyOf(VERBS_MEMBER));
	});

	It("hides and refuses Dismantle on a home piece during a run", [this]()
	{
		if (!Pool || Inventories.Num() < VERBS_PARTY_SIZE || Inventories.Contains(nullptr)) return;
		const FGuid DaggerID = AddDagger();
		Pool->BeginRunLayer();
		const TOptional<WoRInventoryVerbs::FInventoryRow> Own = GearRow(DaggerID);
		if (!TestTrue(TEXT("listed"), Own.IsSet())) return;
		TestFalse(TEXT("no Dismantle during a run"), VerbsOf(Own.GetValue()).Contains(EInventoryVerb::Dismantle));
		TestFalse(TEXT("refused with a reason"), RunOn(EInventoryVerb::Dismantle, Own.GetValue()).IsEmpty());
		TestNotNull(TEXT("still in the store"), Pool->FindWeaponInPool(DaggerID));
		Pool->EndRunLayer();
		TestTrue(TEXT("after the run it may go"), VerbsOf(Own.GetValue()).Contains(EInventoryVerb::Dismantle));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
