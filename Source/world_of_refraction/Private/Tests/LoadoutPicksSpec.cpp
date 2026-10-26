// LoadoutPicksSpec.cpp
// B-159 (D-081 = A): a pick that is itself invalid is refused with a reason; a loadout's invalid
// parts (too many abilities, too many utility crystals of a tier band) are dropped and named.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/LoadoutConstants.h"
#include "Loadout/LoadoutRules.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "PartsDroppedRecorder.h"

namespace
{
	template <class TComponent>
	TComponent *PicksTestAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}
}

BEGIN_DEFINE_SPEC(FLoadoutPicksSpec, "WoR.Loadout.Picks",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UInventoryComponent *Inventory = nullptr;
	ULoadoutComponent *Loadouts = nullptr;
	UWeaponData *Sword = nullptr;

	/** An ability for WeaponType, learned by the character. */
	UAbilityData *LearnedAbility(EWeaponType WeaponType, const TCHAR *Name = TEXT("Test Ability"))
	{
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->Name = Name;
		Ability->RequiredWeaponType = WeaponType;
		Ability->RequiredWieldMode = Sword->WieldMode;
		Inventory->LearnAbility(Ability);
		return Ability;
	}

	/** Loadout 0: the owned sword as the primary weapon, nothing assigned. */
	void AddSwordLoadout()
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon.WeaponEntry = Inventory->Weapons[0];
		Inventory->SavedLoadouts.Add(Loadout);
	}
END_DEFINE_SPEC(FLoadoutPicksSpec)

void FLoadoutPicksSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		AActor *Owner = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Inventory = PicksTestAddComponent<UInventoryComponent>(Owner);
		Loadouts = PicksTestAddComponent<ULoadoutComponent>(Owner);
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->WeaponType = EWeaponType::Sword;
		TestTrue(TEXT("sword owned"), Inventory->AddWeapon(Sword));
		Inventory->SavedLoadouts.Empty();
		AddSwordLoadout();
	});

	AfterEach([this]()
	{
		Inventory = nullptr;
		Loadouts = nullptr;
		Sword = nullptr;
		TestWorld.Reset();
	});

	Describe("a pick", [this]()
	{
		It("is refused with a reason when it is itself invalid", [this]()
		{
			FText Refusal;
			TestFalse(TEXT("a Spear ability on a Sword is refused"), Loadouts->TryAssignWeaponAbility(0, LearnedAbility(EWeaponType::Spear), Refusal));
			TestFalse(TEXT("with a reason"), Refusal.IsEmpty());
			TestEqual(TEXT("and the loadout is unchanged"), Inventory->SavedLoadouts[0].PrimaryWeapon.AssignedAbilities.Num(), 0);
		});

		It("goes through when it breaks nothing", [this]()
		{
			FText Refusal;
			TestTrue(TEXT("a Sword ability on a Sword"), Loadouts->TryAssignWeaponAbility(0, LearnedAbility(EWeaponType::Sword), Refusal));
			TestTrue(TEXT("no reason"), Refusal.IsEmpty());
			TestEqual(TEXT("assigned"), Inventory->SavedLoadouts[0].PrimaryWeapon.AssignedAbilities.Num(), 1);
		});

		It("is refused when it repeats a part already assigned", [this]()
		{
			UAbilityData *Ability = LearnedAbility(EWeaponType::Sword);
			FText Refusal;
			TestTrue(TEXT("first time"), Loadouts->TryAssignWeaponAbility(0, Ability, Refusal));
			TestFalse(TEXT("second time"), Loadouts->TryAssignWeaponAbility(0, Ability, Refusal));
			TestEqual(TEXT("assigned once"), Inventory->SavedLoadouts[0].PrimaryWeapon.AssignedAbilities.Num(), 1);
		});

		It("is refused into a part the loadout doesn't use", [this]()
		{
			Inventory->SavedLoadouts[0].PrimarySlotType = EPrimarySlotType::Ring;
			FText Refusal;
			TestFalse(TEXT("ability with a ring primary"), Loadouts->TryAssignWeaponAbility(0, LearnedAbility(EWeaponType::Sword), Refusal));
			TestFalse(TEXT("with a reason"), Refusal.IsEmpty());
			TestFalse(TEXT("ring spell with no ring equipped"), Loadouts->TryAssignRingSpell(0, NewObject<USpellData>(GetTransientPackage()), Refusal));
		});

		It("fills the gap a dropped ability left, not over the cap", [this]()
		{
			TArray<UAbilityData *> &Assigned = Inventory->SavedLoadouts[0].PrimaryWeapon.AssignedAbilities;
			for (int32 i = 0; i < LoadoutConstants::MAX_WEAPON_ABILITIES; ++i)
			{
				Assigned.Add(LearnedAbility(EWeaponType::Sword, *FString::Printf(TEXT("Ability %d"), i)));
			}
			Assigned[0] = nullptr;
			FText Refusal;
			TestTrue(TEXT("a fourth real ability fits"), Loadouts->TryAssignWeaponAbility(0, LearnedAbility(EWeaponType::Sword, TEXT("New")), Refusal));
			TestEqual(TEXT("in the gap"), Inventory->SavedLoadouts[0].PrimaryWeapon.AssignedAbilities.Num(), LoadoutConstants::MAX_WEAPON_ABILITIES);
		});

		It("is refused for a missing loadout or nothing to assign", [this]()
		{
			FText Refusal;
			TestFalse(TEXT("no such loadout"), Loadouts->TryAssignWeaponAbility(1, LearnedAbility(EWeaponType::Sword), Refusal));
			TestFalse(TEXT("null ability"), Loadouts->TryAssignWeaponAbility(0, nullptr, Refusal));
		});
	});

	Describe("dropping invalid parts", [this]()
	{
		It("trims abilities over the weapon's cap and names what it dropped", [this]()
		{
			TArray<UAbilityData *> &Assigned = Inventory->SavedLoadouts[0].PrimaryWeapon.AssignedAbilities;
			for (int32 i = 0; i < LoadoutConstants::MAX_WEAPON_ABILITIES; ++i)
			{
				Assigned.Add(LearnedAbility(EWeaponType::Sword, *FString::Printf(TEXT("Ability %d"), i)));
			}
			Assigned.Add(nullptr);
			Assigned.Add(LearnedAbility(EWeaponType::Sword, TEXT("Extra")));

			const TArray<FInvalidSlotFinding> Dropped = Loadouts->DropInvalidParts(0);

			TestEqual(TEXT("one part dropped"), Dropped.Num(), 1);
			TestTrue(TEXT("names the dropped ability"), Dropped.Num() == 1 && Dropped[0].Reason.Contains(TEXT("Extra")));
			TestEqual(TEXT("down to the cap"), Inventory->SavedLoadouts[0].PrimaryWeapon.AssignedAbilities.Num(), LoadoutConstants::MAX_WEAPON_ABILITIES);
			TestFalse(TEXT("the empty entry went, not a real ability"), Inventory->SavedLoadouts[0].PrimaryWeapon.AssignedAbilities.Contains(nullptr));
		});

		It("trims utility crystals over a tier band's cap, in every loadout", [this]()
		{
			AddSwordLoadout();
			FItemLoadoutSlot Stack;
			Stack.CrystalId = FCrystalId(ECrystalType::Garnet, EItemTier::S_Tier);
			Stack.Quantity = LoadoutConstants::MAX_UTILITY_CRYSTALS_S_TIER + 1;
			Inventory->SavedLoadouts[1].ItemSlots.Add(Stack);

			UPartsDroppedRecorder *Recorder = NewObject<UPartsDroppedRecorder>();
			Loadouts->OnPartsDropped.AddDynamic(Recorder, &UPartsDroppedRecorder::Record);

			Loadouts->DropInvalidPartsFromAllLoadouts();

			TestEqual(TEXT("one announcement"), Recorder->LoadoutIndices.Num(), 1);
			TestTrue(TEXT("for the second loadout"), Recorder->LoadoutIndices.Num() == 1 && Recorder->LoadoutIndices[0] == 1);
			TestTrue(TEXT("naming the crystal"), Recorder->Messages.Num() == 1 && Recorder->Messages[0].ToString().Contains(TEXT("x1")));
			TestEqual(TEXT("second loadout trimmed to the S cap"), Inventory->SavedLoadouts[1].ItemSlots[0].Quantity, LoadoutConstants::MAX_UTILITY_CRYSTALS_S_TIER);
			TestEqual(TEXT("nothing left to drop"), Loadouts->DropInvalidParts(1).Num(), 0);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
