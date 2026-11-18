// WeaponMeshFollowSpec.cpp
// B-138: the weapon mesh follows ULoadoutComponent::OnActiveWeaponChanged instead of polling on a
// tick. Switching loadout or toggling weapons updates the displayed weapon at once, with no world
// tick; the component never ticks; the delegate fires only when the active weapon really changes.
// The meshes themselves (sockets, dual wield) need a skeletal owner, so that half is PIE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Weapons/WeaponMeshComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"

namespace
{
	const FString MESH_TEST_SECOND_LOADOUT = TEXT("Mesh test second");
}

BEGIN_DEFINE_SPEC(FWeaponMeshFollowSpec, "WoR.Weapons.MeshFollowsLoadout",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	UWeaponMeshComponent *Mesh = nullptr;
	UWeaponData *FirstWeapon = nullptr;
	UWeaponData *SecondWeapon = nullptr;
	int32 FirstLoadout = INDEX_NONE;
	int32 SecondLoadout = INDEX_NONE;
END_DEFINE_SPEC(FWeaponMeshFollowSpec)

void FWeaponMeshFollowSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		FirstWeapon = NewObject<UWeaponData>(GetTransientPackage());
		SecondWeapon = NewObject<UWeaponData>(GetTransientPackage());
		Loadout = TestWorld->SpawnCombatantWithLoadout(FWoRTestWorld::NewCharacterData(), FirstWeapon);
		Inventory = Loadout ? Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!TestNotNull(TEXT("Loadout"), Loadout) || !TestNotNull(TEXT("Inventory"), Inventory))
		{
			Loadout = nullptr; // each case returns early
			return;
		}
		FirstLoadout = Inventory->ActiveLoadoutIndex;
		SecondLoadout = Loadout->CreateNewLoadout(MESH_TEST_SECOND_LOADOUT);
		if (!TestTrue(TEXT("Second loadout created"), Inventory->SavedLoadouts.IsValidIndex(SecondLoadout)))
		{
			Loadout = nullptr;
			return;
		}
		FCombatLoadout &Second = Inventory->SavedLoadouts[SecondLoadout];
		Second.PrimarySlotType = EPrimarySlotType::Weapon;
		Second.PrimaryWeapon.WeaponEntry.Weapon = SecondWeapon;
		Second.bShowPrimary = true;
		Mesh = FWoRTestWorld::AddComponent<UWeaponMeshComponent>(Loadout->GetOwner());
	});

	AfterEach([this]()
	{
		Loadout = nullptr;
		Inventory = nullptr;
		Mesh = nullptr;
		FirstWeapon = nullptr;
		SecondWeapon = nullptr;
		TestWorld.Reset();
	});

	It("never ticks", [this]()
	{
		if (!Loadout) return;
		TestFalse(TEXT("Cannot tick"), Mesh->PrimaryComponentTick.bCanEverTick);
	});

	It("shows the active weapon from the start", [this]()
	{
		if (!Loadout) return;
		TestEqual(TEXT("First weapon shown"), Mesh->GetDisplayedWeapon(), FirstWeapon);
	});

	It("follows a loadout switch at once, without a world tick", [this]()
	{
		if (!Loadout) return;
		TestTrue(TEXT("Switched"), Loadout->SetActiveLoadoutIndex(SecondLoadout));
		TestEqual(TEXT("Second weapon shown"), Mesh->GetDisplayedWeapon(), SecondWeapon);
		TestTrue(TEXT("Switched back"), Loadout->SetActiveLoadoutIndex(FirstLoadout));
		TestEqual(TEXT("First weapon shown again"), Mesh->GetDisplayedWeapon(), FirstWeapon);
	});

	It("follows a change made straight to the loadout once it is announced", [this]()
	{
		if (!Loadout) return;
		Inventory->SavedLoadouts[FirstLoadout].PrimaryWeapon.WeaponEntry.Weapon = SecondWeapon;
		TestEqual(TEXT("Not yet announced"), Mesh->GetDisplayedWeapon(), FirstWeapon);
		Loadout->NotifyActiveWeaponChanged();
		TestEqual(TEXT("Announced: second weapon shown"), Mesh->GetDisplayedWeapon(), SecondWeapon);
	});

	It("re-checks the weapon after any inventory change", [this]()
	{
		if (!Loadout) return;
		Inventory->SavedLoadouts[FirstLoadout].PrimaryWeapon.WeaponEntry.Weapon = SecondWeapon;
		TestTrue(TEXT("An inventory change"), Inventory->AddWeapon(NewObject<UWeaponData>(GetTransientPackage())));
		TestEqual(TEXT("Second weapon shown"), Mesh->GetDisplayedWeapon(), SecondWeapon);
	});

	It("shows no weapon when the active loadout has none", [this]()
	{
		if (!Loadout) return;
		Loadout->ClearLoadout(FirstLoadout);
		TestNull(TEXT("Nothing shown"), Mesh->GetDisplayedWeapon());
	});

	It("follows a Generic's weapon toggle", [this]()
	{
		if (!Loadout) return;
		UCharacterDataComponent *CharComp = Loadout->GetOwner()->FindComponentByClass<UCharacterDataComponent>();
		if (!TestNotNull(TEXT("Character data"), CharComp)) return;
		CharComp->CharacterData->CharacterClass = ECharacterClass::Generic;
		FCombatLoadout &First = Inventory->SavedLoadouts[FirstLoadout];
		First.SecondarySlotType = ESecondarySlotType::Weapon;
		First.SecondaryWeapon.WeaponEntry.Weapon = SecondWeapon;
		Loadout->ToggleEquipment();
		TestEqual(TEXT("Secondary weapon shown"), Mesh->GetDisplayedWeapon(), SecondWeapon);
		Loadout->ToggleEquipment();
		TestEqual(TEXT("Primary weapon shown again"), Mesh->GetDisplayedWeapon(), FirstWeapon);
	});

	It("does not announce when the weapon did not change", [this]()
	{
		if (!Loadout) return;
		Mesh->ClearWeaponMesh(); // a cleared mesh re-shows only on the next announcement
		TestTrue(TEXT("Same loadout chosen"), Loadout->SetActiveLoadoutIndex(FirstLoadout));
		TestNull(TEXT("No announcement, so still cleared"), Mesh->GetDisplayedWeapon());
	});

	It("stops listening once the mesh component ends play", [this]()
	{
		if (!Loadout) return;
		TestTrue(TEXT("Listening"), Loadout->OnActiveWeaponChanged.IsBound());
		Mesh->DestroyComponent();
		TestFalse(TEXT("Unbound"), Loadout->OnActiveWeaponChanged.IsBound());
	});

	Describe("the socket each hand uses (D-242)", [this]()
	{
		const auto Typed = [](EWeaponType Type, EWeaponWieldMode Mode = EWeaponWieldMode::Single)
		{
			UWeaponData *Piece = NewObject<UWeaponData>(GetTransientPackage());
			Piece->WeaponType = Type;
			Piece->WieldMode = Mode;
			return Piece;
		};
		const auto Every = [](FName) { return true; };  // a body with every socket
		const auto NoGrips = [](FName) { return false; }; // a body with none of the D-242 sockets
		constexpr bool RIGHT = true;
		constexpr bool LEFT = false;
		constexpr bool USE_GRIP = true;

		It("puts every right-hand weapon type on hand_r_grip and an off-hand weapon on hand_l_grip", [this, Typed, Every]()
		{
			bool bFellBack = true;
			for (const EWeaponType Type : {EWeaponType::Sword, EWeaponType::Axe, EWeaponType::Greatsword, EWeaponType::Dagger})
			{
				TestEqual(TEXT("right hand: the grip"), UWeaponMeshComponent::ChooseSocket(*Typed(Type), RIGHT, USE_GRIP, Every, bFellBack),
						  UWeaponMeshComponent::RIGHT_GRIP_SOCKET);
				TestFalse(TEXT("no fallback"), bFellBack);
				TestEqual(TEXT("left hand: the grip"), UWeaponMeshComponent::ChooseSocket(*Typed(Type, EWeaponWieldMode::Dual), LEFT, USE_GRIP, Every, bFellBack),
						  UWeaponMeshComponent::LEFT_GRIP_SOCKET);
			}
		});

		It("puts a shield on its own point in either hand", [this, Typed, Every]()
		{
			bool bFellBack = true;
			TestEqual(TEXT("a shield piece, left"), UWeaponMeshComponent::ChooseSocket(*Typed(EWeaponType::Shield), LEFT, USE_GRIP, Every, bFellBack),
					  UWeaponMeshComponent::LEFT_SHIELD_SOCKET);
			TestEqual(TEXT("a shield piece, right"), UWeaponMeshComponent::ChooseSocket(*Typed(EWeaponType::Shield), RIGHT, USE_GRIP, Every, bFellBack),
					  UWeaponMeshComponent::RIGHT_SHIELD_SOCKET);
			const UWeaponData *Baked = Typed(EWeaponType::Sword, EWeaponWieldMode::OffHandShield); // the older sword with its shield baked in
			TestEqual(TEXT("a baked shield, left"), UWeaponMeshComponent::ChooseSocket(*Baked, LEFT, USE_GRIP, Every, bFellBack),
					  UWeaponMeshComponent::LEFT_SHIELD_SOCKET);
			TestEqual(TEXT("its sword, right: the grip"), UWeaponMeshComponent::ChooseSocket(*Baked, RIGHT, USE_GRIP, Every, bFellBack),
					  UWeaponMeshComponent::RIGHT_GRIP_SOCKET);
		});

		It("falls back to the older per-type socket when the body has no D-242 socket, and says so", [this, Typed, NoGrips]()
		{
			bool bFellBack = false;
			TestEqual(TEXT("sword, right"), UWeaponMeshComponent::ChooseSocket(*Typed(EWeaponType::Sword), RIGHT, USE_GRIP, NoGrips, bFellBack), FName(TEXT("hand_r_sword")));
			TestTrue(TEXT("fell back"), bFellBack);
			TestEqual(TEXT("axe, left"), UWeaponMeshComponent::ChooseSocket(*Typed(EWeaponType::Axe, EWeaponWieldMode::Dual), LEFT, USE_GRIP, NoGrips, bFellBack),
					  FName(TEXT("hand_l_axe")));
			TestEqual(TEXT("shield, left"), UWeaponMeshComponent::ChooseSocket(*Typed(EWeaponType::Shield), LEFT, USE_GRIP, NoGrips, bFellBack), FName(TEXT("hand_l_sas")));
			TestEqual(TEXT("shield, right: the one older shield socket"), UWeaponMeshComponent::ChooseSocket(*Typed(EWeaponType::Shield), RIGHT, USE_GRIP, NoGrips, bFellBack),
					  FName(TEXT("hand_l_sas")));
			TestTrue(TEXT("fell back"), bFellBack);
		});

		It("uses the older sockets when the switch is off, with no fallback warning", [this, Typed, Every]()
		{
			bool bFellBack = true;
			TestEqual(TEXT("greatsword, right"), UWeaponMeshComponent::ChooseSocket(*Typed(EWeaponType::Greatsword), RIGHT, !USE_GRIP, Every, bFellBack),
					  FName(TEXT("hand_r_greatsword")));
			TestFalse(TEXT("the switch, not a missing socket"), bFellBack);
			TestEqual(TEXT("shield, left"), UWeaponMeshComponent::ChooseSocket(*Typed(EWeaponType::Shield), LEFT, !USE_GRIP, Every, bFellBack), FName(TEXT("hand_l_sas")));
			TestTrue(TEXT("the switch starts on"), UWeaponMeshComponent::UsesGripSockets());
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
