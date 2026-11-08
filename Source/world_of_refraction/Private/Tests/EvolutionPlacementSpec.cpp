// EvolutionPlacementSpec.cpp
// B-357b part 1 (D-147, Q-357b): the one resolve path for "an evolution attached to X" and what each place gives:
// the character's own slot always gives its passives; a weapon's or adapter's evolution only to an action charged at
// level 2, so none at fight start.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/EvolutionPlacement.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Effects/EffectDefinition.h"
#include "Character/CharacterDataComponent.h"

namespace
{
	constexpr float EVOLUTION_PLACEMENT_TEST_RESIST = 5.0f;
	constexpr float EVOLUTION_PLACEMENT_TEST_MIND_PERCENT = 20.0f;
	constexpr float EVOLUTION_PLACEMENT_TEST_TOLERANCE = 0.001f;

	UEvolutionItemData *EvolutionPlacementTestEvolution()
	{
		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		UEffectDefinition *Bundle = NewObject<UEffectDefinition>(GetTransientPackage());
		FSkillEffect Effect;
		FSkillEffectPayload Payload;
		Payload.EffectType = ESkillEffectType::DefenseBuff;
		Effect.Payloads.Add(Payload);
		Bundle->Effects.Add(Effect);
		Evolution->ReferencedEffects.Add(Bundle);
		return Evolution;
	}

	FRuntimeAttachedItem EvolutionPlacementTestSocket(UEvolutionItemData *Evolution)
	{
		FRuntimeAttachedItem Socket;
		Socket.Kind = EAttachedItemKind::Evolution;
		Socket.Evolution.Item = Evolution;
		return Socket;
	}
}

BEGIN_DEFINE_SPEC(FEvolutionPlacementSpec, "WoR.Loadout.EvolutionPlacement", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	static FCombatLoadout &ActiveOf(ULoadoutComponent *Loadout)
	{
		UInventoryComponent *Inventory = Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>();
		return Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	}

	ULoadoutComponent *SpawnGeneric(UWeaponData *Primary)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Generic;
		return TestWorld->SpawnCombatantWithLoadout(Data, Primary);
	}
END_DEFINE_SPEC(FEvolutionPlacementSpec)

void FEvolutionPlacementSpec::Define()
{
	It("the character's slot always gives its passives; gear only at charge level 2", [this]()
	{
		TestTrue(TEXT("character, not infused"), WoREvolutionPlacement::GivesPassives(EEvolutionPlace::Character, 0));
		TestFalse(TEXT("weapon, L0"), WoREvolutionPlacement::GivesPassives(EEvolutionPlace::Weapon, 0));
		TestFalse(TEXT("weapon, L1"), WoREvolutionPlacement::GivesPassives(EEvolutionPlace::Weapon, 1));
		TestTrue(TEXT("weapon, L2"), WoREvolutionPlacement::GivesPassives(EEvolutionPlace::Weapon, WoREvolutionPlacement::GEAR_PASSIVES_CHARGE_LEVEL));
		TestFalse(TEXT("adapter, L1"), WoREvolutionPlacement::GivesPassives(EEvolutionPlace::Adapter, 1));
		TestTrue(TEXT("adapter, L2"), WoREvolutionPlacement::GivesPassives(EEvolutionPlace::Adapter, 2));
	});

	It("a socket adds its evolution with its place, and nothing for a crystal or an empty one", [this]()
	{
		TArray<FAttachedEvolution> Attached;
		TestTrue(TEXT("evolution"), WoREvolutionPlacement::AppendFrom(EvolutionPlacementTestSocket(EvolutionPlacementTestEvolution()), EEvolutionPlace::Adapter, Attached));
		TestFalse(TEXT("empty socket"), WoREvolutionPlacement::AppendFrom(FRuntimeAttachedItem(), EEvolutionPlace::Weapon, Attached));
		FRuntimeAttachedItem Crystal;
		Crystal.Kind = EAttachedItemKind::Crystal;
		TestFalse(TEXT("a crystal"), WoREvolutionPlacement::AppendFrom(Crystal, EEvolutionPlace::Weapon, Attached));
		if (TestEqual(TEXT("one added"), Attached.Num(), 1))
		{
			TestEqual(TEXT("its place"), Attached[0].Place, EEvolutionPlace::Adapter);
		}
		TestEqual(TEXT("not at L1"), WoREvolutionPlacement::FilterPassives(Attached, 1).Num(), 0);
		TestEqual(TEXT("at L2"), WoREvolutionPlacement::FilterPassives(Attached, 2).Num(), 1);
	});

	It("B-357c: only the character's slot gives stats and resistance", [this]()
	{
		TestTrue(TEXT("character"), WoREvolutionPlacement::GivesStats(EEvolutionPlace::Character));
		TestFalse(TEXT("weapon"), WoREvolutionPlacement::GivesStats(EEvolutionPlace::Weapon));
		TestFalse(TEXT("adapter"), WoREvolutionPlacement::GivesStats(EEvolutionPlace::Adapter));
	});

	Describe("on a loadout", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("Test world created"), TestWorld->Create());
		});
		AfterEach([this]() { TestWorld.Reset(); });

		It("the character's own evolution is resolved there and gives its effects at fight start", [this]()
		{
			ULoadoutComponent *Loadout = SpawnGeneric(nullptr);
			if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
			FCombatLoadout &L = ActiveOf(Loadout);
			L.PrimarySlotType = EPrimarySlotType::Evolution;
			L.PrimaryEvolution.Item = EvolutionPlacementTestEvolution();
			const TArray<FAttachedEvolution> Attached = Loadout->GatherAttachedEvolutions();
			if (TestEqual(TEXT("one attached"), Attached.Num(), 1))
			{
				TestEqual(TEXT("on the character"), Attached[0].Place, EEvolutionPlace::Character);
			}
			TestEqual(TEXT("its effect"), Loadout->GetActiveEffects(Loadout->GetOwner()).Num(), 1);
			TestEqual(TEXT("gathered too"), Loadout->GetActiveEffectsGathered(Loadout->GetOwner()).Num(), 1);
		});

		It("B-357c: the character's evolution gives its resistance and pillar percent; a weapon's gives neither", [this]()
		{
			UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
			ULoadoutComponent *Loadout = SpawnGeneric(Weapon);
			if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
			UCharacterDataComponent *Char = Loadout->GetOwner()->FindComponentByClass<UCharacterDataComponent>();
			if (!TestNotNull(TEXT("character"), Char)) return;
			UEvolutionItemData *Evolution = EvolutionPlacementTestEvolution();
			Evolution->BaseResistance.Fire = EVOLUTION_PLACEMENT_TEST_RESIST;
			Evolution->BaseStatBonus.BonusMindModifierPercent = EVOLUTION_PLACEMENT_TEST_MIND_PERCENT;
			FCombatLoadout &L = ActiveOf(Loadout);
			L.PrimarySlotType = EPrimarySlotType::Weapon;
			L.PrimaryWeapon.WeaponEntry.Weapon = Weapon;
			const float MindBare = Char->GetEvolutionModifiedMind();
			const float FireBare = Loadout->GetActiveResistanceBonus(Loadout->GetOwner()).Fire;
			L.PrimaryWeapon.WeaponEntry.AttachedItem = EvolutionPlacementTestSocket(Evolution);
			const TArray<FAttachedEvolution> OnWeapon = Loadout->GatherAttachedEvolutions(); // review: the weapon's is really seen
			TestTrue(TEXT("weapon: its evolution is attached there"), OnWeapon.Num() == 1 && OnWeapon[0].Place == EEvolutionPlace::Weapon);
			TestEqual(TEXT("weapon: no resistance"), Loadout->GetActiveResistanceBonus(Loadout->GetOwner()).Fire, FireBare, EVOLUTION_PLACEMENT_TEST_TOLERANCE);
			TestEqual(TEXT("weapon: no pillar percent"), Char->GetEvolutionModifiedMind(), MindBare, EVOLUTION_PLACEMENT_TEST_TOLERANCE);
			L.PrimarySlotType = EPrimarySlotType::Evolution;
			L.PrimaryEvolution.Item = Evolution;
			L.PrimaryWeapon.WeaponEntry.AttachedItem = FRuntimeAttachedItem();
			TestEqual(TEXT("character slot: its resistance"), Loadout->GetActiveResistanceBonus(Loadout->GetOwner()).Fire, FireBare + EVOLUTION_PLACEMENT_TEST_RESIST,
					  EVOLUTION_PLACEMENT_TEST_TOLERANCE);
			if (MindBare > 0.0f)
			{
				TestTrue(TEXT("character slot: its pillar percent"), Char->GetEvolutionModifiedMind() > MindBare);
			}
			else
			{
				AddWarning(TEXT("bare Mind is 0: the pillar percent can't be seen here")); // review: say so rather than pass silently
			}
		});

		It("a weapon's evolution is resolved on the weapon and gives nothing at fight start", [this]()
		{
			UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
			ULoadoutComponent *Loadout = SpawnGeneric(Weapon);
			if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
			FCombatLoadout &L = ActiveOf(Loadout);
			L.PrimarySlotType = EPrimarySlotType::Weapon;
			L.PrimaryWeapon.WeaponEntry.Weapon = Weapon;
			L.PrimaryWeapon.WeaponEntry.AttachedItem = EvolutionPlacementTestSocket(EvolutionPlacementTestEvolution());
			const TArray<FAttachedEvolution> Attached = Loadout->GatherAttachedEvolutions();
			if (TestEqual(TEXT("one attached"), Attached.Num(), 1))
			{
				TestEqual(TEXT("on the weapon"), Attached[0].Place, EEvolutionPlace::Weapon);
			}
			TestEqual(TEXT("no effect at fight start (the weapon has none of its own)"), Loadout->GetActiveEffects(Loadout->GetOwner()).Num(), 0);
			TestTrue(TEXT("debug names the place"), Loadout->GetAttachedEvolutionsString().Contains(TEXT("on the weapon")));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
