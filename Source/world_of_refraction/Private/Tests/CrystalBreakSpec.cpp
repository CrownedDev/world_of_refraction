// CrystalBreakSpec.cpp
// B-035: between-combat crystal destruction, split out of ACombatOrchestrator. A broken weapon or
// ring crystal is cleared and dismantled into its typed essence; an intact one stays.
// Q-159c: RepairOn, the between-combat repair, also brings a broken evolution back.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyYield.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Durability/DurabilityConstants.h"
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
	constexpr int32 CRYSTAL_BREAK_WORN_DURABILITY = 1;
	constexpr int32 CRYSTAL_BREAK_EVOLUTION_MAX_DURABILITY = 50;
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

	/** A breakable evolution asset whose max durability is above one repair. */
	UEvolutionItemData *NewEvolution() const
	{
		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		Evolution->Breakability = EBreakability::Breakable;
		Evolution->MaxDurability = CRYSTAL_BREAK_EVOLUTION_MAX_DURABILITY;
		return Evolution;
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

	It("repair: a worn gem gains the repair amount, clamped to its max", [this]()
	{
		Spawn(CRYSTAL_BREAK_WORN_DURABILITY, CRYSTAL_BREAK_WORN_DURABILITY);
		const int32 Expected = FMath::Min(ItemIdentity::GetMaxDurability(CRYSTAL_BREAK_GEM),
										  CRYSTAL_BREAK_WORN_DURABILITY + DurabilityConstants::REPAIR_PER_BATTLE);
		TestTrue(TEXT("the gem can gain durability"), Expected > CRYSTAL_BREAK_WORN_DURABILITY);
		TestEqual(TEXT("two repaired"), CrystalBreak::RepairOn(Actor, DurabilityConstants::REPAIR_PER_BATTLE), 2);
		TestEqual(TEXT("weapon gem repaired"), Loadout->GetCrystalEntryByHolder(Weapon).GetCurrentDurability(), Expected);
	});

	It("repair: a broken gem is not repaired", [this]()
	{
		Spawn(CRYSTAL_BREAK_BROKEN_DURABILITY, CRYSTAL_BREAK_WORN_DURABILITY);
		CrystalBreak::RepairOn(Actor, DurabilityConstants::REPAIR_PER_BATTLE);
		TestEqual(TEXT("broken gem stays at 0"), Loadout->GetCrystalEntryByHolder(Weapon).GetCurrentDurability(),
				  CRYSTAL_BREAK_BROKEN_DURABILITY);
	});

	It("repair: a broken evolution on a weapon comes back (Q-159c)", [this]()
	{
		Spawn(CRYSTAL_BREAK_INTACT_DURABILITY, CRYSTAL_BREAK_INTACT_DURABILITY);
		FRuntimeAttachedItem Broken;
		Broken.Kind = EAttachedItemKind::Evolution;
		Broken.Evolution.Item = NewEvolution();
		Broken.Evolution.CurrentDurability = CRYSTAL_BREAK_BROKEN_DURABILITY;
		Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].PrimaryWeapon.WeaponEntry.AttachedItem = Broken;

		CrystalBreak::RepairOn(Actor, DurabilityConstants::REPAIR_PER_BATTLE);
		const FRuntimeAttachedItem After = Loadout->GetCrystalEntryByHolder(Weapon);
		TestEqual(TEXT("repaired by one repair"), After.GetCurrentDurability(), DurabilityConstants::REPAIR_PER_BATTLE);
		TestFalse(TEXT("no longer broken"), After.IsBroken());
	});

	It("repair: a broken primary-slot evolution comes back (Q-159c)", [this]()
	{
		Spawn(CRYSTAL_BREAK_INTACT_DURABILITY, CRYSTAL_BREAK_INTACT_DURABILITY);
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.PrimarySlotType = EPrimarySlotType::Evolution;
		Active.PrimaryEvolution.Item = NewEvolution();
		Active.PrimaryEvolution.CurrentDurability = CRYSTAL_BREAK_BROKEN_DURABILITY;

		CrystalBreak::RepairOn(Actor, DurabilityConstants::REPAIR_PER_BATTLE);
		const FCombatLoadout &After = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		TestEqual(TEXT("repaired by one repair"), After.PrimaryEvolution.CurrentDurability, DurabilityConstants::REPAIR_PER_BATTLE);
		TestFalse(TEXT("no longer broken"), After.PrimaryEvolution.IsBroken());
	});

	It("repair: an evolution is clamped to its max durability", [this]()
	{
		Spawn(CRYSTAL_BREAK_INTACT_DURABILITY, CRYSTAL_BREAK_INTACT_DURABILITY);
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.PrimarySlotType = EPrimarySlotType::Evolution;
		Active.PrimaryEvolution.Item = NewEvolution();
		Active.PrimaryEvolution.CurrentDurability = CRYSTAL_BREAK_EVOLUTION_MAX_DURABILITY - CRYSTAL_BREAK_WORN_DURABILITY;

		CrystalBreak::RepairOn(Actor, DurabilityConstants::REPAIR_PER_BATTLE);
		TestEqual(TEXT("at max"), Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].PrimaryEvolution.CurrentDurability,
				  CRYSTAL_BREAK_EVOLUTION_MAX_DURABILITY);
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
		TestEqual(TEXT("repair: no actor"), CrystalBreak::RepairOn(nullptr, DurabilityConstants::REPAIR_PER_BATTLE), 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
