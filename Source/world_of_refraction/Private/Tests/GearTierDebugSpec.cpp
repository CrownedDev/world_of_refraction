// GearTierDebugSpec.cpp
// B-609a: wor.Debug.SetGearTier sets the owned weapon's or adapter's tier where the truth lives (the owned entry) and
// every loadout slot holding it follows; an S weapon then takes an S crystal (one S crystal per loadout, D-143).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Inventory/InventoryData.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/WoRGameModeBase.h"
#include "Pool/PoolSubsystem.h"
#include "Debug/Inventory/GearTierDebug.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/LoadoutConstants.h"
#include "Loadout/LoadoutRules.h"

namespace
{
	const TCHAR *GEAR_TIER_SWORD = TEXT("Training Sword");
	const TCHAR *GEAR_TIER_AXE = TEXT("Training Axe");
	const TCHAR *GEAR_TIER_HAND = TEXT("Open Hand");
	const TCHAR *GEAR_TIER_ARMING_SWORD = TEXT("Arming Sword");
	const TCHAR *GEAR_TIER_HAND_AXE = TEXT("Hand Axe");
	const FCrystalId GEAR_TIER_S_EMERALD(ECrystalType::Emerald, EItemTier::S_Tier);
	constexpr int32 GEAR_TIER_LOADOUTS = 2;
}

BEGIN_DEFINE_SPEC(FGearTierDebugSpec, "WoR.Debug.SetGearTier",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UInventoryComponent *Inventory = nullptr;
	FGuid SwordId;

	UWeaponData *MakeWeapon(const TCHAR *Name, EWeaponType Type = EWeaponType::Sword)
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->Name = Name;
		Weapon->WeaponType = Type;
		Weapon->Tier = EItemTier::F_Tier;
		return Weapon;
	}

	const FWeaponInventoryEntry &SlotIn(int32 LoadoutIndex) const
	{
		return Inventory->SavedLoadouts[LoadoutIndex].PrimaryWeapon.WeaponEntry;
	}
END_DEFINE_SPEC(FGearTierDebugSpec)

void FGearTierDebugSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		AActor *Owner = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!Owner)
		{
			return;
		}
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Owner);
		Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Owner);

		UWeaponData *Sword = MakeWeapon(GEAR_TIER_SWORD);
		URingData *Hand = NewObject<URingData>(GetTransientPackage());
		Hand->Name = GEAR_TIER_HAND;
		Hand->Tier = EItemTier::F_Tier;
		TestTrue(TEXT("Sword owned"), Inventory->AddWeapon(Sword));
		TestTrue(TEXT("Axe owned"), Inventory->AddWeapon(MakeWeapon(GEAR_TIER_AXE, EWeaponType::Axe)));
		TestTrue(TEXT("Hand owned"), Inventory->AddRing(Hand));
		SwordId = Inventory->Weapons.Num() > 0 ? Inventory->Weapons[0].PersistentID : FGuid();

		Inventory->SavedLoadouts.Empty();
		for (int32 Index = 0; Index < GEAR_TIER_LOADOUTS; ++Index)
		{
			FCombatLoadout &Preset = Inventory->SavedLoadouts.AddDefaulted_GetRef();
			Preset.PrimarySlotType = EPrimarySlotType::Weapon;
			Preset.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		}
		Inventory->ActiveLoadoutIndex = 0;
		Inventory->LinkLoadoutsToOwned();
	});

	AfterEach([this]()
	{
		Inventory = nullptr;
		TestWorld.Reset();
	});

	It("sets the named weapon to S, owned and in every loadout, and leaves the rest alone", [this]()
	{
		if (!Inventory || !SwordId.IsValid()) return;
		const TArray<FString> Lines = WoRGearTier::SetOwnedGearTier(*Inventory, TEXT("sword"), EItemTier::S_Tier);
		TestEqual(TEXT("one piece changed"), Lines.Num(), 1);
		TestTrue(TEXT("the line names it and both tiers"), Lines.Num() == 1 && Lines[0] == FString::Printf(TEXT("%s: F -> S"), GEAR_TIER_SWORD));
		TestEqual(TEXT("the owned sword is S"), Inventory->FindOwnedWeapon(SwordId)->Tier, EItemTier::S_Tier);
		TestEqual(TEXT("loadout 0 shows S"), SlotIn(0).Tier, EItemTier::S_Tier);
		TestEqual(TEXT("loadout 1 shows S"), SlotIn(1).Tier, EItemTier::S_Tier);
		TestEqual(TEXT("the axe is untouched"), Inventory->Weapons[1].Tier, EItemTier::F_Tier);
		TestEqual(TEXT("the hand is untouched"), Inventory->Rings[0].Tier, EItemTier::F_Tier);
	});

	It("sets an adapter by its name too", [this]()
	{
		if (!Inventory || Inventory->Rings.Num() == 0) return;
		TestEqual(TEXT("one piece changed"), WoRGearTier::SetOwnedGearTier(*Inventory, TEXT("Hand"), EItemTier::S_Tier).Num(), 1);
		TestEqual(TEXT("the owned hand is S"), Inventory->Rings[0].Tier, EItemTier::S_Tier);
	});

	It("changes nothing for a piece already at the tier, an unknown name or an empty one", [this]()
	{
		if (!Inventory) return;
		WoRGearTier::SetOwnedGearTier(*Inventory, TEXT("Sword"), EItemTier::A_Tier);
		TestEqual(TEXT("already A"), WoRGearTier::SetOwnedGearTier(*Inventory, TEXT("Sword"), EItemTier::A_Tier).Num(), 0);
		TestEqual(TEXT("no such piece"), WoRGearTier::SetOwnedGearTier(*Inventory, TEXT("Halberd"), EItemTier::S_Tier).Num(), 0);
		TestEqual(TEXT("an empty name matches nothing"), WoRGearTier::SetOwnedGearTier(*Inventory, FString(), EItemTier::S_Tier).Num(), 0);
		TestEqual(TEXT("the axe never moved"), Inventory->Weapons[1].Tier, EItemTier::F_Tier);
	});

	It("lets the S sword take an S Emerald, the one S crystal a loadout may hold", [this]()
	{
		if (!Inventory || !SwordId.IsValid()) return;
		WoRGearTier::SetOwnedGearTier(*Inventory, TEXT("Sword"), EItemTier::S_Tier);
		TestTrue(TEXT("an S Emerald owned"), Inventory->AddCrystal(GEAR_TIER_S_EMERALD));
		TestTrue(TEXT("socketed in the S sword"), Inventory->AttachCrystalToWeapon(SwordId, GEAR_TIER_S_EMERALD));
		TestTrue(TEXT("the loadout shows the S Emerald"), SlotIn(0).AttachedItem.IsCrystal() && SlotIn(0).AttachedItem.Crystal.Id == GEAR_TIER_S_EMERALD);
		TestEqual(TEXT("one S crystal equipped, the cap"), FLoadoutRules::CountEquippedSTierCrystals(Inventory->SavedLoadouts[0], ECharacterClass::Generic),
				  LoadoutConstants::MAX_EQUIPPED_S_TIER_CRYSTALS);
	});

	It("B-612: matches a weapon by its type word and mends a loadout copy that drifted from the owned piece", [this]()
	{
		if (!Inventory || !SwordId.IsValid()) return;
		Inventory->Weapons[0].Weapon->Name = TEXT("Edgeblade"); // a shown name with no "Sword" in it
		TestEqual(TEXT("found by its type, Sword"), WoRGearTier::SetOwnedGearTier(*Inventory, TEXT("Sword"), EItemTier::B_Tier).Num(), 1);
		TestEqual(TEXT("the slot follows"), SlotIn(0).Tier, EItemTier::B_Tier);
		Inventory->SavedLoadouts[1].PrimaryWeapon.WeaponEntry.Tier = EItemTier::F_Tier; // a copy left behind
		TestEqual(TEXT("already B: no change listed"), WoRGearTier::SetOwnedGearTier(*Inventory, TEXT("Sword"), EItemTier::B_Tier).Num(), 0);
		TestEqual(TEXT("but the drifted copy is mended"), SlotIn(1).Tier, EItemTier::B_Tier);
	});

	It("B-612 (Crown's path): a new game's Generic's starting Arming Sword turns S in every entry, the pool and the next draw", [this]()
	{
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		UPoolSubsystem *Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		if (!TestNotNull(TEXT("party"), Party) || !TestNotNull(TEXT("pool"), Pool)) return;
		UWeaponData *Sword = MakeWeapon(GEAR_TIER_ARMING_SWORD);
		UWeaponData *Axe = MakeWeapon(GEAR_TIER_HAND_AXE, EWeaponType::Axe);
		UCharacterData *Leader = FWoRTestWorld::NewCharacterData();
		Leader->CharacterClass = ECharacterClass::Generic;
		Leader->Inventory = NewObject<UInventoryData>(GetTransientPackage());
		Leader->Inventory->StartingGear.Pieces.AddDefaulted_GetRef().Piece = Sword; // the new-game pick: Sword + Axe
		Leader->Inventory->StartingGear.Pieces.AddDefaulted_GetRef().Piece = Axe;
		Party->Members.Empty();
		FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
		Member.PawnClass = ACombatCharacter::StaticClass();
		Member.CharacterData = Leader;
		Member.MemberId = FGuid::NewGuid();
		APawn *Pawn = TestWorld->GetWorld()->SpawnActor<APawn>();
		UCharacterDataComponent *CharComp = Pawn ? NewObject<UCharacterDataComponent>(Pawn) : nullptr;
		if (!TestNotNull(TEXT("pawn"), CharComp)) return;
		CharComp->CharacterData = FWoRTestWorld::NewCharacterData();
		Pawn->AddInstanceComponent(CharComp);
		if (!TestTrue(TEXT("stood as the leader"), AWoRGameModeBase::StandAsPartyLeader(Pawn, Party))) return;
		CharComp->RegisterComponent();
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Pawn);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Pawn);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Pawn);
		UInventoryComponent *Hub = FWoRTestWorld::AddComponent<UInventoryComponent>(Pawn);
		ULoadoutComponent *Loadout = FWoRTestWorld::AddComponent<ULoadoutComponent>(Pawn);
		Hub->DrawSharedGear();

		const auto SwordTiers = [Hub, Sword]()
		{
			TArray<EItemTier> Tiers;
			for (FCombatLoadout Copy : Hub->SavedLoadouts)
			{
				Copy.EditGearEntries(ECharacterClass::Generic, [&Tiers, Sword](int32, FWeaponLoadoutEntry *Set, FRingLoadoutEntry *)
				{
					for (const FWeaponInventoryEntry *Piece : {Set ? &Set->WeaponEntry : nullptr, Set ? &Set->OffHandEntry : nullptr})
					{
						if (Piece && Piece->Weapon == Sword) Tiers.Add(Piece->Tier);
					}
				});
			}
			return Tiers;
		};
		const TArray<EItemTier> Before = SwordTiers();
		if (!TestTrue(TEXT("the starting sword is worn at F"), Before.Num() > 0 && !Before.Contains(EItemTier::S_Tier))) return;
		const FWeaponInventoryEntry *Owned = Hub->Weapons.FindByPredicate([Sword](const FWeaponInventoryEntry &E) { return E.Weapon == Sword; });
		if (!TestNotNull(TEXT("the starting sword is owned"), Owned)) return;
		const FGuid SwordID = Owned->PersistentID;

		const TArray<FString> Lines = WoRGearTier::SetOwnedGearTier(*Hub, TEXT("Sword"), EItemTier::S_Tier);
		TestEqual(TEXT("the sword alone changed"), Lines.Num(), 1);
		TestTrue(TEXT("every entry wearing the sword shows S"), SwordTiers().Num() == Before.Num() && !SwordTiers().ContainsByPredicate([](EItemTier T) { return T != EItemTier::S_Tier; }));
		const FWeaponInventoryEntry *Stored = Pool->FindWeaponInPool(SwordID);
		TestTrue(TEXT("the pool's copy is S"), Stored && Stored->Tier == EItemTier::S_Tier);
		const FWeaponLoadoutEntry *Active = Loadout->GetActiveWeaponLoadout();
		TestTrue(TEXT("the fight's active weapon is the S sword"), Active && Active->WeaponEntry.Weapon == Sword && Active->WeaponEntry.Tier == EItemTier::S_Tier);
		TestEqual(TEXT("with an S sword's skill slots"), Active ? Active->GetSkillSlotCount() : 0, Active ? Active->GetTierSkillSlotCount() : -1);
		Hub->DrawSharedGear(); // the hub visited again
		TestTrue(TEXT("the next draw keeps it S"), SwordTiers().Num() > 0 && !SwordTiers().ContainsByPredicate([](EItemTier T) { return T != EItemTier::S_Tier; }));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
