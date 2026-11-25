// TestSetSpec.cpp
// B-634: the current test set (Config/DefaultGame.ini). Every name finds its asset; a Generic wears the three infusion
// sources through the loadout screen's own drafts and checks (CL-03, CL-23c, the budget), each a different element; the
// home shops get their slice.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Debug/Inventory/TestSetDebug.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/FEvolutionInventoryEntry.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/Entries/FRingInventoryEntry.h"
#include "Loadout/Entries/FWeaponInventoryEntry.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutRules.h"

namespace
{
	FRuntimeAttachedItem TestSetCrystal(const FCrystalId &Id)
	{
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::Crystal;
		Attached.Crystal = FCrystalAttachment(Id, ItemIdentity::GetMaxDurability(Id));
		return Attached;
	}
}

BEGIN_DEFINE_SPEC(FTestSetSpec, "WoR.Debug.TestSet", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTestSetSpec)

void FTestSetSpec::Define()
{
	It("finds an asset for every name of the current test set", [this]()
	{
		for (const FString &Missing : WoRTestSet::FindUnresolvedNames(*GetDefault<UWoRTestSet>()))
		{
			AddError(FString::Printf(TEXT("not found (or more than one): %s"), *Missing));
		}
	});

	It("dresses a Generic in three sources of three elements: the evolution primary, sword and shield, the adapter", [this]()
	{
		const UWoRTestSet &Set = *GetDefault<UWoRTestSet>();
		UEvolutionItemData *Gale = Cast<UEvolutionItemData>(WoRTestSet::FindAsset(UEvolutionItemData::StaticClass(), Set.GenericEvolution));
		UWeaponData *Sword = Cast<UWeaponData>(WoRTestSet::FindAsset(UWeaponData::StaticClass(), Set.GenericWeapon));
		UWeaponData *Shield = Cast<UWeaponData>(WoRTestSet::FindAsset(UWeaponData::StaticClass(), Set.GenericOffHand));
		URingData *Ring = Cast<URingData>(WoRTestSet::FindAsset(URingData::StaticClass(), Set.GenericAdapter));
		if (!TestTrue(TEXT("the Generic's four pieces found"), Gale && Sword && Shield && Ring))
		{
			return;
		}
		// As wor.Debug.TestSet leaves them: every piece at the set's tier (S), the crystals and the stone from the list.
		FEvolutionInventoryEntry Evolution(Gale);
		Evolution.Tier = Set.GearTier;
		FWeaponInventoryEntry Weapon = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Weapon.Tier = Set.GearTier;
		Weapon.AttachedItem = TestSetCrystal(Set.GenericWeaponCrystal);
		Weapon.StoneSocket.Kind = EAttachedItemKind::AugmentStone;
		Weapon.StoneSocket.Crystal = FCrystalAttachment(Set.GenericWeaponStone, 0);
		FWeaponInventoryEntry OffHand = FWeaponInventoryEntry::CreateFromWeapon(Shield);
		OffHand.Tier = Set.GearTier;
		FRingInventoryEntry Adapter = FRingInventoryEntry::CreateFromRing(Ring);
		Adapter.Tier = Set.GearTier;
		Adapter.AttachedItem = TestSetCrystal(Set.GenericAdapterCrystal);
		TestEqual(TEXT("the set's gear tier is S (Crown)"), Set.GearTier, EItemTier::S_Tier);

		const TSet<ESpellElement> Elements = {Gale->GetAssociatedElement(), CrystalTypeHelpers::GetElement(Set.GenericWeaponCrystal.Type),
											  CrystalTypeHelpers::GetElement(Set.GenericAdapterCrystal.Type)};
		TestEqual(TEXT("three different elements, so the three sources can be told apart"), Elements.Num(), 3);

		// From a loadout that already holds a weapon primary (the first entry is replaced, never left empty: D-116).
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		FText Refusal;
		if (!TestTrue(TEXT("the loadout rules take the kit"), WoRTestSet::WearGenericKit(Loadout, Evolution, Weapon, OffHand, Adapter, Refusal)))
		{
			AddError(FString::Printf(TEXT("refused: %s"), *Refusal.ToString()));
			return;
		}
		TestEqual(TEXT("the evolution is the primary"), Loadout.PrimarySlotType, EPrimarySlotType::Evolution);
		TestEqual(TEXT("... the owned one"), Loadout.PrimaryEvolutionInstance, Evolution.InstanceID);
		TestTrue(TEXT("the sword is the second weapon"), Loadout.SecondarySlotType == ESecondarySlotType::Weapon &&
															 Loadout.SecondaryWeapon.WeaponEntry.PersistentID == Weapon.PersistentID);
		TestEqual(TEXT("... with the shield in the off hand"), Loadout.SecondaryWeapon.OffHandEntry.PersistentID, OffHand.PersistentID);
		TestTrue(TEXT("... and its crystal"), Loadout.SecondaryWeapon.WeaponEntry.AttachedItem.IsCrystal() &&
												  Loadout.SecondaryWeapon.WeaponEntry.AttachedItem.Crystal.Id == Set.GenericWeaponCrystal);
		TestTrue(TEXT("... and its stone (the budget's fifth cell)"), Loadout.SecondaryWeapon.WeaponEntry.StoneSocket.Crystal.Id == Set.GenericWeaponStone);
		TestEqual(TEXT("... at S"), Loadout.SecondaryWeapon.WeaponEntry.Tier, EItemTier::S_Tier);
		TestTrue(TEXT("the adapter, with its crystal, in the adapter list"),
				 Loadout.RingLoadout.ContainsByPredicate([&Adapter, &Set](const FRingLoadoutEntry &Entry)
														 { return Entry.RingEntry.PersistentID == Adapter.PersistentID && Entry.RingEntry.AttachedItem.IsCrystal() &&
																  Entry.RingEntry.AttachedItem.Crystal.Id == Set.GenericAdapterCrystal; }));
		for (const FString &Reason : FLoadoutRules::ToReasons(FLoadoutRules::CollectClassLimitFindings(Loadout, ECharacterClass::Generic)))
		{
			AddError(FString::Printf(TEXT("a class limit refuses the kit: %s"), *Reason));
		}

		FCombatLoadout Empty;
		TestTrue(TEXT("an empty loadout takes it too"), WoRTestSet::WearGenericKit(Empty, Evolution, Weapon, OffHand, Adapter, Refusal));
	});

	It("stocks each home shop with its slice", [this]()
	{
		const UWoRTestSet &Set = *GetDefault<UWoRTestSet>();
		TestEqual(TEXT("the Blacksmith: the weapons"), WoRTestSet::GetStock(Set, EMerchantType::Blacksmith).Num(), Set.Weapons.Num());
		TestEqual(TEXT("the Jeweler: the adapters"), WoRTestSet::GetStock(Set, EMerchantType::Jeweler).Num(), Set.Adapters.Num());
		TestEqual(TEXT("the Combat Master: the abilities"), WoRTestSet::GetStock(Set, EMerchantType::CombatMaster).Num(), Set.Abilities.Num());
		TestEqual(TEXT("the Spell Shop: the spells"), WoRTestSet::GetStock(Set, EMerchantType::SpellShop).Num(), Set.Spells.Num());
		const int32 Gems = Set.Items.FilterByPredicate([](const FWoRTestSetItem &Item) { return CrystalTypeHelpers::IsGemType(Item.Crystal.Type); }).Num();
		const TArray<FMerchantStockEntry> Spiritualist = WoRTestSet::GetStock(Set, EMerchantType::Spiritualist);
		TestEqual(TEXT("the Spiritualist: the evolutions and the gems"), Spiritualist.Num(), Set.Evolutions.Num() + Gems);
		TestTrue(TEXT("... a gem line with its count"), Spiritualist.ContainsByPredicate([](const FMerchantStockEntry &Line) { return Line.IsCrystalEntry() && Line.Count > 0; }));
		TestTrue(TEXT("the loadout vendor keeps its own stock"), WoRTestSet::GetStock(Set, EMerchantType::LoadoutVendor).IsEmpty());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
