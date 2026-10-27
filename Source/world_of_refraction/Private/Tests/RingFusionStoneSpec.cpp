// RingFusionStoneSpec.cpp
// B-082 (D-009): on a ring, the stat-stone half of an elemental fusion counts like on a weapon,
// while the crystal half still gives the ring its spells.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Rings/RingData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	constexpr int32 RING_FUSION_DURABILITY = 10;
	constexpr int32 RING_FUSION_RING = 0;
	constexpr int32 RING_FUSION_PLAIN_RING = 1;

	template <class TComponent>
	TComponent *RingFusionAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}

	/** DefenseStone + Garnet (Fire): the elemental fusion rings accept. */
	FRuntimeAttachedItem RingFusionDefenseFire()
	{
		FFusionId Id;
		Id.HalfA = FCrystalId(ECrystalType::DefenseStone, EItemTier::F_Tier);
		Id.HalfB = FCrystalId(ECrystalType::Garnet, EItemTier::F_Tier);
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::Fusion;
		Attached.Fusion = FFusionAttachment(Id, RING_FUSION_DURABILITY);
		return Attached;
	}
}

BEGIN_DEFINE_SPEC(FRingFusionStoneSpec, "WoR.Loadout.RingFusionStone",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ULoadoutComponent *Loadout = nullptr;
	USpellData *FireSpell = nullptr;
END_DEFINE_SPEC(FRingFusionStoneSpec)

void FRingFusionStoneSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());

		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Resonator;
		AActor *Actor = TestWorld->SpawnCombatant(Data);
		UInventoryComponent *Inventory = RingFusionAddComponent<UInventoryComponent>(Actor);
		Loadout = RingFusionAddComponent<ULoadoutComponent>(Actor);
		Loadout->InitializeFromCharacterData(Data, Inventory);

		FireSpell = NewObject<USpellData>(GetTransientPackage());
		FireSpell->Name = TEXT("Test Fire Spell");
		FRingLoadoutEntry FusionRing;
		FusionRing.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
		FusionRing.RingEntry.AttachedItem = RingFusionDefenseFire();
		FusionRing.RingEntry.AssignedSpells = {FireSpell};
		FRingLoadoutEntry PlainRing;
		PlainRing.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.RingLoadout = {FusionRing, PlainRing};
		Active.ActiveRingIndex = RING_FUSION_RING;
	});

	AfterEach([this]()
	{
		Loadout = nullptr;
		FireSpell = nullptr;
		TestWorld.Reset();
	});

	It("raises Defense from the ring's stone half and keeps its Fire spells (B-082)", [this]()
	{
		const float Expected = CrystalEffectTable::GetAttachedStonePercent(RingFusionDefenseFire(), ESubStat::Defense);
		TestTrue(TEXT("the fusion's stone half gives Defense"), Expected > 0.0f);
		TestEqual(TEXT("the ring's stone counts, with no weapon"), Loadout->GetAttachedStonePercent(ESubStat::Defense), Expected);
		TestEqual(TEXT("no other stat from it"), Loadout->GetAttachedStonePercent(ESubStat::Reflex), 0.0f);
		TestTrue(TEXT("still offers the Fire spell"), Loadout->GetAvailableSpells().Contains(FireSpell));
	});

	It("stops counting when another ring is active", [this]()
	{
		Loadout->SetActiveRingIndex(RING_FUSION_PLAIN_RING);
		TestEqual(TEXT("plain ring: no stone"), Loadout->GetAttachedStonePercent(ESubStat::Defense), 0.0f);
	});

	It("raises the defender's Defense in the damage calculator", [this]()
	{
		const UDamageCalculator *Damage = TestWorld->GetSubsystem<UDamageCalculator>();
		if (!TestNotNull(TEXT("damage calculator"), Damage))
		{
			return;
		}
		AActor *Defender = Loadout->GetOwner();
		const float WithFusion = Damage->GetDefenderFlatDefense(Defender);
		Loadout->SetActiveRingIndex(RING_FUSION_PLAIN_RING);
		const float WithPlain = Damage->GetDefenderFlatDefense(Defender);
		// Zero base defense would hide a multiplier; only compare when there is some.
		if (WithPlain > 0.0f)
		{
			TestTrue(TEXT("the fusion ring defends more"), WithFusion > WithPlain);
		}
		else
		{
			TestEqual(TEXT("no base defense: both zero"), WithFusion, WithPlain);
		}
	});

	It("counts on a Caster's primary ring too", [this]()
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Caster;
		AActor *Actor = TestWorld->SpawnCombatant(Data);
		UInventoryComponent *Inventory = RingFusionAddComponent<UInventoryComponent>(Actor);
		ULoadoutComponent *Caster = RingFusionAddComponent<ULoadoutComponent>(Actor);
		Caster->InitializeFromCharacterData(Data, Inventory);
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.PrimarySlotType = EPrimarySlotType::Ring;
		Active.PrimaryRing.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
		Active.PrimaryRing.RingEntry.AttachedItem = RingFusionDefenseFire();
		TestTrue(TEXT("the primary ring's stone counts"), Caster->GetAttachedStonePercent(ESubStat::Defense) > 0.0f);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
