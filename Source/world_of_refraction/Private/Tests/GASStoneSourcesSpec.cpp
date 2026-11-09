// GASStoneSourcesSpec.cpp
// B-042f (GAS stage 3 step 5, Q-042f): stones reach GAS through the spec builder on each stat's sources GE, and
// wor.Effects.Compare's stone check (UWoRAttributeSyncComponent::GetStoneMismatchString) sees them: none without a
// stone, none once a stone is synced, a difference while an attached stone hasn't reached GAS yet.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterDataComponent.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "Loadout/LoadoutComponent.h"

BEGIN_DEFINE_SPEC(FGASStoneSourcesSpec, "WoR.GAS.StoneSources", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AActor *Hero = nullptr;
	UWoRAttributeSyncComponent *Sync = nullptr;

	/** A Spell Damage stone in the primary weapon's socket (a stat stone the SpellDamage attribute composes). */
	bool AttachStone()
	{
		UInventoryComponent *Inventory = Hero ? Hero->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!Inventory || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
		{
			AddError(TEXT("no active loadout"));
			return false;
		}
		FRuntimeAttachedItem Stone;
		Stone.Kind = EAttachedItemKind::AugmentStone;
		Stone.Crystal = FCrystalAttachment(FCrystalId(ECrystalType::SpellDamageStone, EItemTier::F_Tier), 0);
		Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].PrimaryWeapon.WeaponEntry.AttachedItem = Stone;
		return true;
	}

	float StoneFactor() const
	{
		FWoRStatLayers Layers;
		const UCharacterDataComponent *Char = Hero->FindComponentByClass<UCharacterDataComponent>();
		return Char && Char->GetStatLayers(ESubStat::SpellDamage, Layers) ? Layers.StoneFactor : 0.0f;
	}
END_DEFINE_SPEC(FGASStoneSourcesSpec)

void FGASStoneSourcesSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		const ULoadoutComponent *Loadout =
			TestWorld->SpawnCombatantWithLoadout(FWoRTestWorld::NewCharacterData(), NewObject<UWeaponData>(GetTransientPackage()));
		Hero = Loadout ? Loadout->GetOwner() : nullptr;
		if (Hero)
		{
			FWoRTestWorld::AddComponent<UAbilitySystemComponent>(Hero);
			Sync = FWoRTestWorld::AddComponent<UWoRAttributeSyncComponent>(Hero);
		}
		if (TestNotNull(TEXT("Sync"), Sync))
		{
			Sync->SyncAttributes();
		}
	});

	AfterEach([this]()
	{
		Hero = nullptr;
		Sync = nullptr;
		TestWorld.Reset();
	});

	It("sees no stone difference without a stone", [this]()
	{
		if (Sync)
		{
			TestEqual(TEXT("no stone line"), Sync->GetStoneMismatchString(), FString());
		}
	});

	It("carries an attached stone once synced, and the compare agrees", [this]()
	{
		if (!Sync || !AttachStone())
		{
			return;
		}
		TestTrue(TEXT("the stone is in the game-side layer"), StoneFactor() > 1.0f);
		Sync->SyncAttributes();
		TestEqual(TEXT("synced: no stone line"), Sync->GetStoneMismatchString(), FString());
	});

	It("names a stone that hasn't reached GAS yet", [this]()
	{
		if (!Sync || !AttachStone())
		{
			return;
		}
		TestTrue(TEXT("not synced: the compare names the stat"), Sync->GetStoneMismatchString().Contains(TEXT("stones")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
