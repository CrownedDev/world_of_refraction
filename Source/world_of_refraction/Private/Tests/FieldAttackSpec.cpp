// FieldAttackSpec.cpp
// B-214f (Design/FieldAbilities.md): the field attack. The strike it carries (a real element only; Water held = Ice; the
// weapon's physical type; the buildup), a swing that strikes nothing, and its cooldown. The engagement itself (first
// strike and the buildup in the fight) needs a party and a level change: PIE (docs/PIE/B-214f.md).
// B-503 (D-215): the cycle through the loadout (weapons, the innate element; a weapon swings, the rest throw the pulse)
// and the strike following the entry in use. Rings and evolutions in the cycle: PIE (docs/PIE/B-503.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/ECharacterClass.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Field/FieldMovementComponent.h"
#include "Field/FieldSkillTypes.h"
#include "GameFramework/Character.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Trial/BattleGameMode.h"

BEGIN_DEFINE_SPEC(FFieldAttackSpec, "WoR.Field.Attack",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACharacter *Pawn = nullptr;
	UFieldMovementComponent *Field = nullptr;

	/** A Caster of Element (no weapon) with field movement. */
	bool SpawnCaster(ESpellElement Element)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Caster;
		Data->InnateElement = Element;
		Pawn = TestWorld->GetWorld()->SpawnActor<ACharacter>();
		if (!Pawn)
		{
			return false;
		}
		UCharacterDataComponent *CharComp = NewObject<UCharacterDataComponent>(Pawn);
		CharComp->CharacterData = Data;
		Pawn->AddInstanceComponent(CharComp);
		CharComp->RegisterComponent();
		Field = FWoRTestWorld::AddComponent<UFieldMovementComponent>(Pawn);
		return Field != nullptr;
	}

	/** B-503: a Caster of Element with field movement and two weapons: the primary a Slash weapon with a live Garnet
	 *  (Fire), the secondary with a broken one. */
	bool SpawnCasterWithWeapons(ESpellElement Element)
	{
		constexpr int32 LIVE_DURABILITY = 10;
		constexpr int32 BROKEN_DURABILITY = 0;
		const auto Garnet = [](int32 Durability)
		{
			FRuntimeAttachedItem Attached;
			Attached.Kind = EAttachedItemKind::Crystal;
			Attached.Crystal = FCrystalAttachment(FCrystalId(ECrystalType::Garnet, EItemTier::F_Tier), Durability);
			return Attached;
		};
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Caster;
		Data->InnateElement = Element;
		UAbilityData *Slash = NewObject<UAbilityData>(GetTransientPackage());
		Slash->PhysicalDamageType = EPhysicalDamageType::Slash;
		UWeaponData *Primary = NewObject<UWeaponData>(GetTransientPackage());
		Primary->WeaponAttack = Slash;
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, Primary);
		UInventoryComponent *Inventory = Loadout ? Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!Inventory || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
		{
			return false;
		}
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.PrimaryWeapon.WeaponEntry.AttachedItem = Garnet(LIVE_DURABILITY);
		Active.SecondarySlotType = ESecondarySlotType::Weapon;
		Active.SecondaryWeapon.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Active.SecondaryWeapon.WeaponEntry.AttachedItem = Garnet(BROKEN_DURABILITY);
		Field = FWoRTestWorld::AddComponent<UFieldMovementComponent>(Loadout->GetOwner());
		return Field != nullptr;
	}
END_DEFINE_SPEC(FFieldAttackSpec)

void FFieldAttackSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		Pawn = nullptr;
		Field = nullptr;
		TestWorld.Reset();
	});

	Describe("the rules", [this]()
	{
		It("carry a real element, make held Water Ice, keep the physical type, and step the sources", [this]()
		{
			const FFieldStrike Water = FieldSkillRules::MakeFieldStrike(ESpellElement::Water, EPhysicalDamageType::Slash, true);
			TestTrue(TEXT("held Water is Ice"), Water.bIce && Water.Element == ESpellElement::Water);
			TestTrue(TEXT("the weapon's type"), Water.Physical == EPhysicalDamageType::Slash);
			TestEqual(TEXT("buildup"), Water.Buildup, FieldSkillRules::FIELD_STRIKE_BUILDUP);
			TestFalse(TEXT("a tap isn't Ice"), FieldSkillRules::MakeFieldStrike(ESpellElement::Water, EPhysicalDamageType::None, false).bIce);
			TestFalse(TEXT("held Fire isn't Ice"), FieldSkillRules::MakeFieldStrike(ESpellElement::Fire, EPhysicalDamageType::None, true).bIce);
			TestTrue(TEXT("Generic carries no element"), FieldSkillRules::MakeFieldStrike(ESpellElement::Generic, EPhysicalDamageType::Impact, false).Element == ESpellElement::None);
			TestTrue(TEXT("Innate -> Weapon"), FieldSkillRules::NextSource(EFieldSkillSource::Innate) == EFieldSkillSource::Weapon);
			TestTrue(TEXT("Ring -> Innate"), FieldSkillRules::NextSource(EFieldSkillSource::Ring) == EFieldSkillSource::Innate);
			constexpr int32 THREE_ENTRIES = 3;
			TestEqual(TEXT("B-503: the next stop"), FieldSkillRules::NextFieldAttackIndex(0, THREE_ENTRIES), 1);
			TestEqual(TEXT("the last wraps to the first"), FieldSkillRules::NextFieldAttackIndex(THREE_ENTRIES - 1, THREE_ENTRIES), 0);
			TestEqual(TEXT("a kept index past the loadout counts as the first"), FieldSkillRules::NextFieldAttackIndex(THREE_ENTRIES + 2, THREE_ENTRIES), 1);
			TestEqual(TEXT("nothing to cycle"), FieldSkillRules::NextFieldAttackIndex(0, 0), 0);
			TestEqual(TEXT("a negative index is the first"), FieldSkillRules::ClampFieldAttackIndex(INDEX_NONE, THREE_ENTRIES), 0);
			TestFalse(TEXT("a tap isn't held (B-214g)"), FieldSkillRules::IsFieldAttackHeld(0.0));
			TestTrue(TEXT("held from the threshold"), FieldSkillRules::IsFieldAttackHeld(FieldSkillRules::FIELD_ATTACK_HOLD_SECONDS));
		});
	});

	Describe("the component", [this]()
	{
		It("with no gear cycles only the innate element, a pulse (B-503)", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnCaster(ESpellElement::Water)))
			{
				return;
			}
			TestEqual(TEXT("one entry"), Field->GetFieldAttackEntries().Num(), 1);
			const FFieldAttackEntry Innate = Field->GetFieldAttackEntry();
			TestTrue(TEXT("the innate element, thrown"), Innate.Kind == EFieldAttackKind::Innate && !Innate.IsSwing());
			const FFieldStrike Held = Field->GetFieldStrike(true);
			TestTrue(TEXT("held innate Water is Ice, no physical type"),
					 Held.Element == ESpellElement::Water && Held.bIce && Held.Physical == EPhysicalDamageType::None);
			TestTrue(TEXT("one entry wraps onto itself"), Field->CycleFieldAttack().Kind == EFieldAttackKind::Innate);
		});

		It("with no gear and no element is the plain swing (B-503)", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnCaster(ESpellElement::Generic)))
			{
				return;
			}
			TestEqual(TEXT("nothing to cycle"), Field->GetFieldAttackEntries().Num(), 0);
			TestTrue(TEXT("a swing"), Field->CycleFieldAttack().IsSwing());
			TestTrue(TEXT("no element"), Field->GetFieldStrike(false).Element == ESpellElement::None);
		});

		It("cycles the loadout in order: weapons, the innate element, then back (B-503)", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnCasterWithWeapons(ESpellElement::Water)))
			{
				return;
			}
			const TArray<FFieldAttackEntry> Entries = Field->GetFieldAttackEntries();
			if (!TestEqual(TEXT("two weapons and the innate element"), Entries.Num(), 3))
			{
				return;
			}
			TestTrue(TEXT("the primary weapon first, a swing in its crystal's element"),
					 Entries[0].IsSwing() && Entries[0].Element == ESpellElement::Fire && Entries[0].Physical == EPhysicalDamageType::Slash);
			TestTrue(TEXT("a broken crystal gives no element"), Entries[1].IsSwing() && Entries[1].Element == ESpellElement::None);
			TestTrue(TEXT("the innate element last"), Entries[2].Kind == EFieldAttackKind::Innate && Entries[2].Element == ESpellElement::Water);

			TestEqual(TEXT("the first entry by default"), Field->GetFieldAttackIndex(), 0);
			const FFieldStrike Swing = Field->GetFieldStrike(false);
			TestTrue(TEXT("the strike follows the entry"), Swing.Element == ESpellElement::Fire && Swing.Physical == EPhysicalDamageType::Slash);
			Field->CycleFieldAttack();
			TestTrue(TEXT("the third stop is the pulse"), !Field->CycleFieldAttack().IsSwing());
			TestEqual(TEXT("kept"), Field->GetFieldAttackIndex(), 2);
			TestTrue(TEXT("the pulse carries the innate element and no physical type"),
					 Field->GetFieldStrike(false).Element == ESpellElement::Water && Field->GetFieldStrike(false).Physical == EPhysicalDamageType::None);
			TestTrue(TEXT("wraps to the first"), Field->CycleFieldAttack().Element == ESpellElement::Fire && Field->GetFieldAttackIndex() == 0);
		});

		It("gives every enemy of the fight the strike's buildup, and nothing for an empty strike", [this]()
		{
			UStatusBuildupManager *Buildup = TestWorld->GetSubsystem<UStatusBuildupManager>();
			AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			AActor *FirstEnemy = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
			AActor *SecondEnemy = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
			if (!TestTrue(TEXT("set up"), Buildup && Hero && FirstEnemy && SecondEnemy))
			{
				return;
			}
			const TArray<AActor *> Enemies = {FirstEnemy, SecondEnemy};
			TestEqual(TEXT("an empty strike: no one"), ABattleGameMode::ApplyFieldStrikeTo(*Buildup, Hero, Enemies, FFieldStrike()), 0);
			const FFieldStrike Strike = FieldSkillRules::MakeFieldStrike(ESpellElement::Fire, EPhysicalDamageType::Slash, false);
			TestEqual(TEXT("both enemies"), ABattleGameMode::ApplyFieldStrikeTo(*Buildup, Hero, Enemies, Strike), Enemies.Num());
			TestTrue(TEXT("the first has buildup"), Buildup->GetStatusBarBuildup(FirstEnemy) > 0.0f);
			TestTrue(TEXT("the second too"), Buildup->GetStatusBarBuildup(SecondEnemy) > 0.0f);
		});

		It("swings at nothing, then waits out the cooldown", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnCaster(ESpellElement::Fire)))
			{
				return;
			}
			TestTrue(TEXT("a swing"), Field->FieldAttack(false) == EFieldSkillRefusal::None);
			TestTrue(TEXT("cooldown"), Field->FieldAttack(false) == EFieldSkillRefusal::OnCooldown);
		});
	});
}

#endif
