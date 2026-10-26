// TrialMapFeatureSpec.cpp
// B-188 (RunStructure §4, Q-188): a chest rolls one item from its zone's loot pool (tag = filter,
// seeded roll = selector) and opens once per run; lore is recorded once; a boss arena without a
// boss starts nothing.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "GameFramework/Pawn.h"
#include "Combat/Encounter/EnemyZone.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Skills/Definitions/SpellData.h"
#include "Trial/TrialLoot.h"
#include "Trial/TrialMapFeature.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	constexpr int32 MAP_FEATURE_TEST_ITEMS = 4;
	constexpr int32 MAP_FEATURE_TEST_SEED = 188;
	/** No authored item carries this pool, so a chest using it opens empty. */
	const TCHAR *MAP_FEATURE_TEST_POOL = TEXT("Source.Zone.Tier4");
	const TCHAR *MAP_FEATURE_TEST_OTHER_POOL = TEXT("Source.Zone.Tier3");
}

BEGIN_DEFINE_SPEC(FTrialMapFeatureSpec, "WoR.Trial.MapFeature",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	/** A pawn with an inventory, like the exploration player. */
	APawn *SpawnPlayerWithInventory()
	{
		APawn *Pawn = TestWorld->GetWorld()->SpawnActor<APawn>();
		if (Pawn)
		{
			UInventoryComponent *Inventory = NewObject<UInventoryComponent>(Pawn);
			Pawn->AddInstanceComponent(Inventory);
			Inventory->RegisterComponent();
		}
		return Pawn;
	}

	static UWeaponData *NewTaggedWeapon(const FGameplayTag &Pool)
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->SourceTags.AddTag(Pool);
		return Weapon;
	}
END_DEFINE_SPEC(FTrialMapFeatureSpec)

void FTrialMapFeatureSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("picks only items tagged with the pool, the same one for the same seed", [this]()
	{
		const FGameplayTag Pool = FGameplayTag::RequestGameplayTag(MAP_FEATURE_TEST_POOL);
		const FGameplayTag Other = FGameplayTag::RequestGameplayTag(MAP_FEATURE_TEST_OTHER_POOL);
		TArray<UObject *> Candidates;
		TSet<UObject *> InPool;
		for (int32 Index = 0; Index < MAP_FEATURE_TEST_ITEMS; ++Index)
		{
			UWeaponData *Tagged = NewTaggedWeapon(Pool);
			InPool.Add(Tagged);
			Candidates.Add(Tagged);
			Candidates.Add(NewTaggedWeapon(Other));
		}
		for (int32 Seed = 0; Seed < MAP_FEATURE_TEST_SEED; ++Seed)
		{
			UObject *Picked = TrialLoot::PickFromPool(Candidates, Pool, Seed);
			if (!TestTrue(FString::Printf(TEXT("Seed %d picks from the pool"), Seed), InPool.Contains(Picked))) return;
		}
		TestEqual(TEXT("Same seed, same item"), TrialLoot::PickFromPool(Candidates, Pool, MAP_FEATURE_TEST_SEED),
				  TrialLoot::PickFromPool(Candidates, Pool, MAP_FEATURE_TEST_SEED));
		TestNull(TEXT("No match: nothing"), TrialLoot::PickFromPool({NewTaggedWeapon(Other)}, Pool, MAP_FEATURE_TEST_SEED));
		TestNull(TEXT("Invalid pool: nothing"), TrialLoot::PickFromPool(Candidates, FGameplayTag(), MAP_FEATURE_TEST_SEED));
	});

	It("gives a weapon to the inventory and teaches a spell", [this]()
	{
		APawn *Player = SpawnPlayerWithInventory();
		UInventoryComponent *Inventory = Player ? Player->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!TestNotNull(TEXT("Inventory"), Inventory)) return;
		const int32 WeaponsBefore = Inventory->GetWeaponCount();
		TestTrue(TEXT("Weapon given"), TrialLoot::GrantTo(Player, NewTaggedWeapon(FGameplayTag())));
		TestEqual(TEXT("One more weapon"), Inventory->GetWeaponCount(), WeaponsBefore + 1);
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		TestTrue(TEXT("Spell learned"), TrialLoot::GrantTo(Player, Spell));
		TestTrue(TEXT("Knows the spell"), Inventory->HasSpell(Spell));
		TestFalse(TEXT("Not an item: refused"), TrialLoot::GrantTo(Player, Player));
	});

	It("rolls the zone's pool and opens once per run", [this]()
	{
		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		AEnemyZone *Zone = TestWorld->GetWorld()->SpawnActor<AEnemyZone>(FVector::ZeroVector, FRotator::ZeroRotator);
		ATrialChest *Chest = TestWorld->GetWorld()->SpawnActor<ATrialChest>(FVector::ZeroVector, FRotator::ZeroRotator);
		APawn *Player = SpawnPlayerWithInventory();
		if (!TestTrue(TEXT("Spawned"), TrialRun && Zone && Chest && Player)) return;
		Zone->Zone.LootPool = FGameplayTag::RequestGameplayTag(MAP_FEATURE_TEST_POOL);
		TestEqual(TEXT("The chest uses its zone's pool"), Chest->GetLootPool(), Zone->Zone.LootPool);

		Chest->Interact(Player);
		TestTrue(TEXT("Recorded as found"), TrialRun->HasFound(Chest->GetFeatureId()));
		Chest->Interact(Player);
		TestEqual(TEXT("Opening it again finds nothing new"), TrialRun->GetFoundCount(), 1);
	});

	It("records lore once, however often it is read", [this]()
	{
		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		ATrialLoreObject *Lore = TestWorld->GetWorld()->SpawnActor<ATrialLoreObject>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (!TestTrue(TEXT("Spawned"), TrialRun && Lore)) return;
		Lore->Interact(nullptr);
		Lore->Interact(nullptr);
		TestTrue(TEXT("Recorded"), TrialRun->HasFound(Lore->GetFeatureId()));
		TestEqual(TEXT("Once"), TrialRun->GetFoundCount(), 1);
		TestTrue(TEXT("Listed by wor.Trial.Print"), TrialRun->GetTrialRunString().Contains(TEXT("found 1")));
	});

	It("starts no fight when the arena has no boss", [this]()
	{
		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		ABossArenaTrigger *Arena = TestWorld->GetWorld()->SpawnActor<ABossArenaTrigger>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (!TestTrue(TEXT("Spawned"), TrialRun && Arena)) return;
		TestNull(TEXT("No variation boss, no fallback"), Arena->GetBoss());
		Arena->Interact(nullptr);
		TestFalse(TEXT("No fight pending"), TrialRun->HasPendingEncounter());
	});
}

#endif
