// CarriedStateSpec.cpp
// B-115 (D-013, D-093, Q-115): what fight 1 of a run leaves on a member's crystals and Broken
// Darkness pool is what fight 2 starts with: wear, a destroyed gem, the absorption stacks and
// overload EP. Trial entry and hub return wipe it.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"

namespace
{
	constexpr int32 CARRY_TEST_FULL_DURABILITY = 10;
	constexpr int32 CARRY_TEST_WORN_DURABILITY = 4;
	constexpr int32 CARRY_TEST_STACKS = 2;
	constexpr int32 CARRY_TEST_OVERLOAD_EP = 1;
	const FCrystalId CARRY_TEST_GEM(ECrystalType::Garnet, EItemTier::F_Tier);

	template <class TComponent>
	TComponent *CarryTestAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}

	FRuntimeAttachedItem CarryTestGem(int32 Durability)
	{
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::Crystal;
		Attached.Crystal = FCrystalAttachment(CARRY_TEST_GEM, Durability);
		return Attached;
	}
}

BEGIN_DEFINE_SPEC(FCarriedStateSpec, "WoR.Party.CarriedState",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPartySessionSubsystem *PartySession = nullptr;
	UCharacterData *Data = nullptr;
	UWeaponData *Weapon = nullptr;
	URingData *Ring = nullptr;
	int32 Slot = INDEX_NONE;

	/** A fresh spawn of the member, as each fight builds it: both gems at full durability. */
	ULoadoutComponent *SpawnFresh()
	{
		AActor *Actor = TestWorld->SpawnCombatant(Data);
		UInventoryComponent *Inventory = CarryTestAddComponent<UInventoryComponent>(Actor);
		ULoadoutComponent *Loadout = CarryTestAddComponent<ULoadoutComponent>(Actor);
		Loadout->InitializeFromCharacterData(Data, Inventory);
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.PrimarySlotType = EPrimarySlotType::Weapon;
		Active.PrimaryWeapon.WeaponEntry.Weapon = Weapon;
		Active.PrimaryWeapon.WeaponEntry.AttachedItem = CarryTestGem(CARRY_TEST_FULL_DURABILITY);
		FRingLoadoutEntry RingEntry;
		RingEntry.RingEntry.Ring = Ring;
		RingEntry.RingEntry.AttachedItem = CarryTestGem(CARRY_TEST_FULL_DURABILITY);
		Active.RingLoadout = {RingEntry};
		Active.ActiveRingIndex = 0;
		return Loadout;
	}
END_DEFINE_SPEC(FCarriedStateSpec)

void FCarriedStateSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party)) return;
		Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Resonator;
		FPartyMember Member;
		Member.PawnClass = ACombatCharacter::StaticClass();
		Member.CharacterData = Data;
		Party->Members.Add(Member);
		Slot = PartySession->GetMemberSlotByData(Data);
		Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Ring = NewObject<URingData>(GetTransientPackage());
	});

	AfterEach([this]()
	{
		PartySession = nullptr;
		Data = nullptr;
		Weapon = nullptr;
		Ring = nullptr;
		TestWorld.Reset();
	});

	It("starts the next fight with the wear and the destroyed gem the last one left", [this]()
	{
		if (!TestTrue(TEXT("Member in the party"), PartySession && Slot != INDEX_NONE)) return;
		ULoadoutComponent *FightOne = SpawnFresh();
		FightOne->FindAttachedItemByHolder(Weapon)->Crystal.CurrentDurability = CARRY_TEST_WORN_DURABILITY;
		FightOne->ResetCrystalEntryByHolder(Ring);
		PartySession->SetMemberCarriedState(Slot, FightOne->SnapshotCarriedGear(), FCarriedAbsorptionState());

		ULoadoutComponent *FightTwo = SpawnFresh();
		TestEqual(TEXT("Both attachments restored"), FightTwo->ApplyCarriedGear(PartySession->GetMemberCarriedState(Slot)->CarriedGear), 2);
		TestEqual(TEXT("Worn gem stays worn"), FightTwo->GetCrystalEntryByHolder(Weapon).GetCurrentDurability(), CARRY_TEST_WORN_DURABILITY);
		TestTrue(TEXT("Destroyed gem stays gone"), FightTwo->GetCrystalEntryByHolder(Ring).IsEmpty());
	});

	It("starts fresh after the run ends", [this]()
	{
		if (!TestTrue(TEXT("Member in the party"), PartySession && Slot != INDEX_NONE)) return;
		ULoadoutComponent *FightOne = SpawnFresh();
		FightOne->FindAttachedItemByHolder(Weapon)->Crystal.CurrentDurability = CARRY_TEST_WORN_DURABILITY;
		PartySession->SetMemberCarriedState(Slot, FightOne->SnapshotCarriedGear(), FCarriedAbsorptionState());
		PartySession->ClearAllMemberVitals();

		ULoadoutComponent *NextRun = SpawnFresh();
		TestEqual(TEXT("Nothing carried"), NextRun->ApplyCarriedGear(PartySession->GetMemberCarriedState(Slot)->CarriedGear), 0);
		TestEqual(TEXT("Gem at full"), NextRun->GetCrystalEntryByHolder(Weapon).GetCurrentDurability(), CARRY_TEST_FULL_DURABILITY);
	});

	It("resumes a born Broken Darkness member's absorption pool and overload", [this]()
	{
		Data->bBrokenDarknessInnate = true;
		AActor *Actor = TestWorld->SpawnCombatant(Data);
		UCharacterDataComponent *CharComp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestTrue(TEXT("Born Broken Darkness"), CharComp && CharComp->IsBrokenDarkness() && CharComp->MaxEP > 0)) return;
		UBrokenDarknessManager *BD = CarryTestAddComponent<UBrokenDarknessManager>(Actor);
		BD->InitializeBornBrokenDarkness();
		CharComp->CurrentEP = CharComp->MaxEP;

		FCarriedAbsorptionState Carried;
		Carried.bCarried = true;
		Carried.AbsorbedElements = {ESpellElement::Darkness, ESpellElement::Fire};
		Carried.AlignmentElement = ESpellElement::Fire;
		Carried.AbsorptionStacks = CARRY_TEST_STACKS;
		Carried.OverloadEP = CARRY_TEST_OVERLOAD_EP;
		BD->ApplyCarriedAbsorption(Carried);

		const FCarriedAbsorptionState Resumed = BD->SnapshotCarriedAbsorption();
		TestTrue(TEXT("Carried"), Resumed.bCarried);
		TestEqual(TEXT("Stacks"), Resumed.AbsorptionStacks, CARRY_TEST_STACKS);
		TestTrue(TEXT("Alignment"), Resumed.AlignmentElement == ESpellElement::Fire);
		TestEqual(TEXT("Absorbed elements"), Resumed.AbsorbedElements, Carried.AbsorbedElements);
		TestEqual(TEXT("Overload EP above max"), CharComp->CurrentEP, CharComp->MaxEP + CARRY_TEST_OVERLOAD_EP);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
