// AdapterTierSpec.cpp
// B-519 (D-224, AssetOverhaulPlan §4 and §4.1): a piece's built tier is its own tier and its socket's averaged, a
// half rounded down; an empty socket keeps the piece's own. A ring's spell slots and its spells' channel read it; its
// stat lines don't. A weapon's elemental side reads it; its physical side stays on the weapon's own tier.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Damage/TierGapConstants.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/Entries/FRingLoadoutEntry.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	constexpr int32 ADAPTER_TIER_TEST_DURABILITY = 10;

	FRuntimeAttachedItem AdapterTierGem(ECrystalType Type, EItemTier Tier)
	{
		FRuntimeAttachedItem Gem;
		Gem.Kind = EAttachedItemKind::Crystal;
		Gem.Crystal = FCrystalAttachment(FCrystalId(Type, Tier), ADAPTER_TIER_TEST_DURABILITY);
		return Gem;
	}
}

BEGIN_DEFINE_SPEC(FAdapterTierSpec, "WoR.Loadout.AdapterTier",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FAdapterTierSpec)

void FAdapterTierSpec::Define()
{
	using namespace CrystalEffectTable;

	It("averages the piece and its socket, a half rounded down; empty keeps the piece's own", [this]()
	{
		TestEqual(TEXT("F ring, S gem: C"), GetBuiltTier(EItemTier::F_Tier, AdapterTierGem(ECrystalType::Garnet, EItemTier::S_Tier)),
				  EItemTier::C_Tier);
		TestEqual(TEXT("D ring, B gem: C"), GetBuiltTier(EItemTier::D_Tier, AdapterTierGem(ECrystalType::Garnet, EItemTier::B_Tier)),
				  EItemTier::C_Tier);
		TestEqual(TEXT("F ring, B gem: D"), GetBuiltTier(EItemTier::F_Tier, AdapterTierGem(ECrystalType::Citrine, EItemTier::B_Tier)),
				  EItemTier::D_Tier);
		TestEqual(TEXT("S ring, F gem: C"), GetBuiltTier(EItemTier::S_Tier, AdapterTierGem(ECrystalType::Garnet, EItemTier::F_Tier)),
				  EItemTier::C_Tier);
		TestEqual(TEXT("empty: own tier"), GetBuiltTier(EItemTier::B_Tier, FRuntimeAttachedItem()), EItemTier::B_Tier);
	});

	It("gives an F ring with a B Citrine the slots of D, and the ring's own once unsocketed", [this]()
	{
		FRingLoadoutEntry Ring;
		Ring.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
		Ring.RingEntry.Tier = EItemTier::F_Tier;
		Ring.RingEntry.AttachedItem = AdapterTierGem(ECrystalType::Citrine, EItemTier::B_Tier);
		TestEqual(TEXT("socketed: D's slots"), Ring.GetSpellSlotCap(), SlotsForContainerTier(EItemTier::D_Tier));
		Ring.RingEntry.AttachedItem = FRuntimeAttachedItem();
		TestEqual(TEXT("empty: F's slots"), Ring.GetSpellSlotCap(), SlotsForContainerTier(EItemTier::F_Tier));
	});

	Describe("on a combatant", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("Test world created"), TestWorld->Create());
		});

		AfterEach([this]() { TestWorld.Reset(); });

		It("splits a weapon: the elemental side reads the built tier, the physical side the weapon's own", [this]()
		{
			UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
			UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
			Weapon->Tier = EItemTier::F_Tier;
			ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(FWoRTestWorld::NewCharacterData(), Weapon);
			UInventoryComponent *Inventory = Loadout ? Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
			if (!TestNotNull(TEXT("Executor"), Executor) || !TestNotNull(TEXT("inventory"), Inventory))
			{
				return;
			}
			AActor *User = Loadout->GetOwner();
			FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
			Active.PrimaryWeapon.WeaponEntry.Tier = EItemTier::F_Tier;
			Active.PrimaryWeapon.WeaponEntry.AttachedItem = AdapterTierGem(ECrystalType::Garnet, EItemTier::B_Tier);

			UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
			Ability->Tier = EItemTier::D_Tier;
			FAction Action;
			Action.ActionType = EActionType::Ability;
			Action.SkillData = Ability;

			TestEqual(TEXT("elemental: D through D, matched"), Executor->GetElementalTierGapDamageMultiplier(User, Action),
					  TierGapDamage::MATCHED_TIER);
			TestEqual(TEXT("physical: D through F"), Executor->GetTierGapDamageMultiplier(User, Action),
					  TierGapDamage::GetTierGapDamageMultiplier(EItemTier::D_Tier, EItemTier::F_Tier));
			TestTrue(TEXT("the gem raises the elemental side"),
					 Executor->GetElementalTierGapDamageMultiplier(User, Action) > Executor->GetTierGapDamageMultiplier(User, Action));

			Active.PrimaryWeapon.WeaponEntry.AttachedItem = FRuntimeAttachedItem();
			TestEqual(TEXT("no crystal: both sides on the weapon's own tier"), Executor->GetElementalTierGapDamageMultiplier(User, Action),
					  Executor->GetTierGapDamageMultiplier(User, Action));
		});

		It("channels a ring's spell through the ring's built tier", [this]()
		{
			UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->CharacterClass = ECharacterClass::Generic;
			ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data);
			UInventoryComponent *Inventory = Loadout ? Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
			if (!TestNotNull(TEXT("Executor"), Executor) || !TestNotNull(TEXT("inventory"), Inventory))
			{
				return;
			}
			USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
			Spell->Name = TEXT("Adapter Tier Bolt");
			Spell->Tier = EItemTier::D_Tier;
			FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
			Active.PrimarySlotType = EPrimarySlotType::Ring;
			Active.PrimaryRing.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
			Active.PrimaryRing.RingEntry.Tier = EItemTier::F_Tier;
			Active.PrimaryRing.RingEntry.AttachedItem = AdapterTierGem(ECrystalType::Garnet, EItemTier::B_Tier);
			Active.PrimaryRing.RingEntry.SpellPicks = {Spell};

			FAction Cast;
			Cast.ActionType = EActionType::Spell;
			Cast.SpellData = Spell;
			Cast.SpellSource = ESpellSource::RingCrystal;
			TestEqual(TEXT("D spell through the D built tier: matched (the B gem alone would boost it)"),
					  Executor->GetTierGapDamageMultiplier(Loadout->GetOwner(), Cast), TierGapDamage::MATCHED_TIER);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
