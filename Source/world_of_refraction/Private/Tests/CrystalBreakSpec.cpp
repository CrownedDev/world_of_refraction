// CrystalBreakSpec.cpp
// B-035: between-combat crystal destruction, split out of ACombatOrchestrator. A broken weapon or
// ring crystal is cleared and dismantled into its typed essence; an intact one stays.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyYield.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/FFusionAttachment.h"
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
	/** An elemental fusion: a stone half (never pays) and a gem half (pays half its essence). */
	const FCrystalId CRYSTAL_BREAK_FUSION_STONE(ECrystalType::DamageStone, EItemTier::C_Tier);
	const FCrystalId CRYSTAL_BREAK_FUSION_GEM(ECrystalType::Garnet, EItemTier::C_Tier);

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

	FRuntimeAttachedItem CrystalBreakBrokenFusion()
	{
		FFusionId Id;
		Id.HalfA = CRYSTAL_BREAK_FUSION_STONE;
		Id.HalfB = CRYSTAL_BREAK_FUSION_GEM;
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::Fusion;
		Attached.Fusion = FFusionAttachment(Id, CRYSTAL_BREAK_BROKEN_DURABILITY);
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
	UInventoryComponent *Inventory = nullptr;

	/** A Resonator with a gem on its primary weapon and on its active ring. */
	void Spawn(int32 WeaponDurability, int32 RingDurability)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Resonator;
		Actor = TestWorld->SpawnCombatant(Data);
		Inventory = CrystalBreakAddComponent<UInventoryComponent>(Actor);
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
		Inventory = nullptr;
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
		TestEqual(TEXT("typed essence at the crystal curve"), GemEssence(),
				  EconomyYield::GetTypedEssenceYieldForTier(CRYSTAL_BREAK_GEM.Tier));
		TestFalse(TEXT("weapon crystal kept"), Loadout->GetCrystalEntryByHolder(Weapon).IsEmpty());
	});

	It("clears both when both broke", [this]()
	{
		Spawn(CRYSTAL_BREAK_BROKEN_DURABILITY, CRYSTAL_BREAK_BROKEN_DURABILITY);
		TestEqual(TEXT("two broke"), CrystalBreak::DestroyBrokenOn(Actor), 2);
		TestEqual(TEXT("essence for both"), GemEssence(),
				  2 * EconomyYield::GetTypedEssenceYieldForTier(CRYSTAL_BREAK_GEM.Tier));
	});

	It("dismantles a broken fusion: half essence for the gem half, nothing for the stone", [this]()
	{
		Spawn(CRYSTAL_BREAK_INTACT_DURABILITY, CRYSTAL_BREAK_INTACT_DURABILITY);
		Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].PrimaryWeapon.WeaponEntry.AttachedItem = CrystalBreakBrokenFusion();
		const int32 StoneEssenceBefore = Currency->GetEssenceType(EconomyYield::ResolveEssenceType(CRYSTAL_BREAK_FUSION_STONE));
		TestEqual(TEXT("one broke"), CrystalBreak::DestroyBrokenOn(Actor), 1);
		TestTrue(TEXT("weapon socket empty"), Loadout->GetCrystalEntryByHolder(Weapon).IsEmpty());
		const int32 HalfYield = FMath::FloorToInt(EconomyYield::GetTypedEssenceYieldForTier(CRYSTAL_BREAK_FUSION_GEM.Tier)
												 * EconomyYield::Constants::FUSION_BREAK_ESSENCE_FRACTION);
		TestTrue(TEXT("the gem half is worth something"), HalfYield > 0);
		TestEqual(TEXT("gem half pays half"), Currency->GetEssenceType(EconomyYield::ResolveEssenceType(CRYSTAL_BREAK_FUSION_GEM)), HalfYield);
		TestEqual(TEXT("stone half pays nothing"), Currency->GetEssenceType(EconomyYield::ResolveEssenceType(CRYSTAL_BREAK_FUSION_STONE)), StoneEssenceBefore);
	});

	It("keeps a broken evolution: equipment is never lost to wear (Q-159)", [this]()
	{
		Spawn(CRYSTAL_BREAK_INTACT_DURABILITY, CRYSTAL_BREAK_INTACT_DURABILITY);
		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		Evolution->Breakability = EBreakability::Breakable;
		FRuntimeAttachedItem Broken;
		Broken.Kind = EAttachedItemKind::Evolution;
		Broken.Evolution.Item = Evolution;
		Broken.Evolution.CurrentDurability = CRYSTAL_BREAK_BROKEN_DURABILITY;
		Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].PrimaryWeapon.WeaponEntry.AttachedItem = Broken;
		TestTrue(TEXT("it is broken"), Loadout->GetCrystalEntryByHolder(Weapon).IsBroken());

		TestEqual(TEXT("nothing destroyed"), CrystalBreak::DestroyBrokenOn(Actor), 0);
		TestTrue(TEXT("still on the weapon"), Loadout->GetCrystalEntryByHolder(Weapon).Evolution.Item == Evolution);
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
