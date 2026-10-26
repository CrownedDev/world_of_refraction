// CrystalBreakSpec.cpp
// B-035: between-combat crystal destruction, split out of ACombatOrchestrator. A broken weapon or
// ring crystal is cleared and dismantled into its typed essence; an intact one stays.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyYield.h"
#include "Equipment/Durability/CrystalBreak.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"

namespace
{
	constexpr int32 CRYSTAL_BREAK_INTACT_DURABILITY = 10;
	constexpr int32 CRYSTAL_BREAK_BROKEN_DURABILITY = 0;
	const FCrystalId CRYSTAL_BREAK_GEM(ECrystalType::Garnet, EItemTier::F_Tier);

	template <class TComponent>
	TComponent *CrystalBreakAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}

	FRuntimeAttachedItem CrystalBreakGem(int32 Durability)
	{
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::Crystal;
		Attached.Crystal = FCrystalAttachment(CRYSTAL_BREAK_GEM, Durability);
		return Attached;
	}
}

BEGIN_DEFINE_SPEC(FCrystalBreakSpec, "WoR.Durability.CrystalBreak",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AActor *Actor = nullptr;
	ULoadoutComponent *Loadout = nullptr;
	UCurrencyComponent *Currency = nullptr;
	UWeaponData *Weapon = nullptr;
	URingData *Ring = nullptr;

	/** A Resonator with a gem on its primary weapon and on its active ring. */
	void Spawn(int32 WeaponDurability, int32 RingDurability)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Resonator;
		Actor = TestWorld->SpawnCombatant(Data);
		UInventoryComponent *Inventory = CrystalBreakAddComponent<UInventoryComponent>(Actor);
		Loadout = CrystalBreakAddComponent<ULoadoutComponent>(Actor);
		Currency = CrystalBreakAddComponent<UCurrencyComponent>(Actor);
		Loadout->InitializeFromCharacterData(Data, Inventory);

		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Active.PrimarySlotType = EPrimarySlotType::Weapon;
		Active.PrimaryWeapon.WeaponEntry.Weapon = Weapon;
		Active.PrimaryWeapon.WeaponEntry.AttachedItem = CrystalBreakGem(WeaponDurability);

		Ring = NewObject<URingData>(GetTransientPackage());
		FRingLoadoutEntry RingEntry;
		RingEntry.RingEntry.Ring = Ring;
		RingEntry.RingEntry.AttachedItem = CrystalBreakGem(RingDurability);
		Active.RingLoadout = {RingEntry};
		Active.ActiveRingIndex = 0;
	}

	int32 GemEssence() const
	{
		return Currency->GetEssenceType(EconomyYield::ResolveEssenceType(CRYSTAL_BREAK_GEM));
	}
END_DEFINE_SPEC(FCrystalBreakSpec)

void FCrystalBreakSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		Actor = nullptr;
		Loadout = nullptr;
		Currency = nullptr;
		Weapon = nullptr;
		Ring = nullptr;
		TestWorld.Reset();
	});

	It("clears a broken weapon crystal and pays its essence", [this]()
	{
		Spawn(CRYSTAL_BREAK_BROKEN_DURABILITY, CRYSTAL_BREAK_INTACT_DURABILITY);
		TestEqual(TEXT("one broke"), CrystalBreak::DestroyBrokenOn(Actor), 1);
		TestTrue(TEXT("weapon socket empty"), Loadout->GetCrystalEntryByHolder(Weapon).IsEmpty());
		TestFalse(TEXT("ring crystal kept"), Loadout->GetCrystalEntryByHolder(Ring).IsEmpty());
		TestEqual(TEXT("typed essence at the crystal curve"), GemEssence(),
				  EconomyYield::GetTypedEssenceYieldForTier(CRYSTAL_BREAK_GEM.Tier));
	});

	It("clears a broken ring crystal", [this]()
	{
		Spawn(CRYSTAL_BREAK_INTACT_DURABILITY, CRYSTAL_BREAK_BROKEN_DURABILITY);
		TestEqual(TEXT("one broke"), CrystalBreak::DestroyBrokenOn(Actor), 1);
		TestTrue(TEXT("ring socket empty"), Loadout->GetCrystalEntryByHolder(Ring).IsEmpty());
		TestFalse(TEXT("weapon crystal kept"), Loadout->GetCrystalEntryByHolder(Weapon).IsEmpty());
	});

	It("clears both when both broke", [this]()
	{
		Spawn(CRYSTAL_BREAK_BROKEN_DURABILITY, CRYSTAL_BREAK_BROKEN_DURABILITY);
		TestEqual(TEXT("two broke"), CrystalBreak::DestroyBrokenOn(Actor), 2);
		TestEqual(TEXT("essence for both"), GemEssence(),
				  2 * EconomyYield::GetTypedEssenceYieldForTier(CRYSTAL_BREAK_GEM.Tier));
	});

	It("leaves intact crystals alone", [this]()
	{
		Spawn(CRYSTAL_BREAK_INTACT_DURABILITY, CRYSTAL_BREAK_INTACT_DURABILITY);
		TestEqual(TEXT("none broke"), CrystalBreak::DestroyBrokenOn(Actor), 0);
		TestFalse(TEXT("weapon crystal kept"), Loadout->GetCrystalEntryByHolder(Weapon).IsEmpty());
		TestEqual(TEXT("no essence"), GemEssence(), 0);
	});

	It("does nothing without a loadout", [this]()
	{
		TestEqual(TEXT("no actor"), CrystalBreak::DestroyBrokenOn(nullptr), 0);
		TestEqual(TEXT("no loadout"), CrystalBreak::DestroyBrokenOn(TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData())), 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
