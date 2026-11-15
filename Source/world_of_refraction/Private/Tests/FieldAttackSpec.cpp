// FieldAttackSpec.cpp
// B-214f (Design/FieldAbilities.md): the field attack. The strike it carries (a real element only; Water held = Ice; the
// weapon's physical type; the buildup), a swing that strikes nothing, and its cooldown. The engagement itself (first
// strike and the buildup in the fight) needs a party and a level change: PIE (docs/PIE/B-214f.md).
// B-503 (D-215): the cycle through the loadout (weapons, the innate element; a weapon swings, the rest throw the pulse)
// and the strike following the entry in use. Rings and evolutions in the cycle: PIE (docs/PIE/B-503.md).
// B-503b: the swing's hit timed to the montage's Impact notify. The swing itself on screen: PIE (docs/PIE/B-503b.md).
// B-503c: the pulse's reach and its wait for the cast's release. The pulse on screen and its 7 m hit: PIE (B-503c.md).
// B-503k (D-217): the cycle is the attack's infusions (a weapon user: plain, then each element; a Refractor: elements).
// B-503l (Q-503l): a hold steps a Reality or Broken Darkness stop through the other stops' elements; a press skips them.
// B-503m4 (D-217): one wheel slice per loadout entry, its element options inside (Ice on Water); the D-pad and mouse rules.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/ECharacterClass.h"
#include "Combat/CombatNotify.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Field/FieldMovementComponent.h"
#include "Field/FieldSkillTypes.h"
#include "GameFramework/Character.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Trial/BattleGameMode.h"
#include "WoRGameNames.h"

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

	/** B-503: a Class of Element with field movement and two weapons: the primary (in hand) a Slash sword with a live
	 *  Garnet (Fire), the secondary with a broken one. */
	bool SpawnWithWeapons(ECharacterClass Class, ESpellElement Element)
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
		Data->CharacterClass = Class;
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
			TestFalse(TEXT("B-503k: a press of R / LB changes the source"), FieldSkillRules::IsFieldSourceHeld(0.0));
			TestTrue(TEXT("a hold changes the element"), FieldSkillRules::IsFieldSourceHeld(FieldSkillRules::FIELD_SOURCE_HOLD_SECONDS));

			// B-503l: a stop with two hold steps, then a stop with none.
			TArray<FFieldAttackEntry> Stops;
			Stops.AddDefaulted(4);
			Stops[1].bHoldStep = Stops[2].bHoldStep = true;
			TestEqual(TEXT("a press skips the hold steps"), FieldSkillRules::NextFieldAttackSourceIndex(Stops, 0), 3);
			TestEqual(TEXT("a press from a hold step goes to the next stop"), FieldSkillRules::NextFieldAttackSourceIndex(Stops, 2), 3);
			TestEqual(TEXT("a press wraps"), FieldSkillRules::NextFieldAttackSourceIndex(Stops, 3), 0);
			TestEqual(TEXT("a hold steps in"), FieldSkillRules::NextFieldAttackHoldIndex(Stops, 0), 1);
			TestEqual(TEXT("a hold steps on"), FieldSkillRules::NextFieldAttackHoldIndex(Stops, 1), 2);
			TestEqual(TEXT("a hold wraps to the stop"), FieldSkillRules::NextFieldAttackHoldIndex(Stops, 2), 0);
			TestEqual(TEXT("a hold on a one-element stop stays"), FieldSkillRules::NextFieldAttackHoldIndex(Stops, 3), 3);
			TestEqual(TEXT("nothing to step"), FieldSkillRules::NextFieldAttackSourceIndex(TArray<FFieldAttackEntry>(), 0), 0);
		});

		It("B-503m3: pick the wheel's entry with the stick and step it with the mouse wheel", [this]()
		{
			constexpr int32 FOUR_ENTRIES = 4;
			TestEqual(TEXT("up is the first"), FieldSkillRules::GetFieldWheelIndexFromStick(FVector2D(0, 1), FOUR_ENTRIES), 0);
			TestEqual(TEXT("right is the second (clockwise)"), FieldSkillRules::GetFieldWheelIndexFromStick(FVector2D(1, 0), FOUR_ENTRIES), 1);
			TestEqual(TEXT("down"), FieldSkillRules::GetFieldWheelIndexFromStick(FVector2D(0, -1), FOUR_ENTRIES), 2);
			TestEqual(TEXT("left"), FieldSkillRules::GetFieldWheelIndexFromStick(FVector2D(-1, 0), FOUR_ENTRIES), 3);
			TestEqual(TEXT("just left of up rounds to the first"), FieldSkillRules::GetFieldWheelIndexFromStick(FVector2D(-0.1, 1), FOUR_ENTRIES), 0);
			TestEqual(TEXT("a centred stick picks nothing"), FieldSkillRules::GetFieldWheelIndexFromStick(FVector2D(0.1, 0.1), FOUR_ENTRIES), INDEX_NONE);
			TestEqual(TEXT("no entries"), FieldSkillRules::GetFieldWheelIndexFromStick(FVector2D(0, 1), 0), INDEX_NONE);
			TestEqual(TEXT("one entry anywhere"), FieldSkillRules::GetFieldWheelIndexFromStick(FVector2D(0, -1), 1), 0);
			TestEqual(TEXT("scroll on"), FieldSkillRules::StepFieldWheelIndex(1, 1, FOUR_ENTRIES), 2);
			TestEqual(TEXT("scroll wraps on"), FieldSkillRules::StepFieldWheelIndex(3, 1, FOUR_ENTRIES), 0);
			TestEqual(TEXT("scroll wraps back"), FieldSkillRules::StepFieldWheelIndex(0, -1, FOUR_ENTRIES), 3);
			TestEqual(TEXT("scroll with nothing"), FieldSkillRules::StepFieldWheelIndex(2, 1, 0), 0);
		});

		It("B-503m4: find the wheel's slices and step a slice's element options", [this]()
		{
			// Two slices: entry 0 with two options after it, entry 3 alone.
			TArray<FFieldAttackEntry> Entries;
			Entries.AddDefaulted(4);
			Entries[1].bHoldStep = Entries[2].bHoldStep = true;
			TestTrue(TEXT("two slices"), FieldSkillRules::GetFieldSliceStarts(Entries) == TArray<int32>({0, 3}));
			TestEqual(TEXT("an option belongs to its slice"), FieldSkillRules::GetFieldSliceOf(Entries, 2), 0);
			TestEqual(TEXT("the second slice"), FieldSkillRules::GetFieldSliceOf(Entries, 3), 1);
			TestEqual(TEXT("no entries: no slice"), FieldSkillRules::GetFieldSliceOf(TArray<FFieldAttackEntry>(), 0), INDEX_NONE);
			TestEqual(TEXT("D-pad right: the next option"), FieldSkillRules::StepFieldSliceOption(Entries, 0, 1), 1);
			TestEqual(TEXT("right from the last option wraps inside the slice"), FieldSkillRules::StepFieldSliceOption(Entries, 2, 1), 0);
			TestEqual(TEXT("D-pad left from the first wraps to the last"), FieldSkillRules::StepFieldSliceOption(Entries, 0, -1), 2);
			TestEqual(TEXT("a one-option slice stays"), FieldSkillRules::StepFieldSliceOption(Entries, 3, 1), 3);
			TestEqual(TEXT("nothing to step"), FieldSkillRules::StepFieldSliceOption(TArray<FFieldAttackEntry>(), 0, 1), 0);
			const FVector2D Moved = FieldSkillRules::MoveFieldWheelPointer(FVector2D::ZeroVector, FVector2D(1.0, 0.0));
			TestTrue(TEXT("the mouse moves the pointer by its scaled travel"), Moved.Equals(FVector2D(FieldSkillRules::FIELD_WHEEL_MOUSE_SCALE, 0.0)));
			TestTrue(TEXT("and never past the rim"), FMath::IsNearlyEqual(FieldSkillRules::MoveFieldWheelPointer(FVector2D(0.0, 1.0), FVector2D(0.0, 1000.0)).Size(), 1.0));
		});

		It("B-503m5: the mouse cursor picks the slice under it", [this]()
		{
			constexpr int32 FOUR_SLICES = 4;
			constexpr float HUB = 50.0f;
			constexpr float RIM = 150.0f;
			TestEqual(TEXT("above the middle is the first (the screen's Y is down)"), FieldSkillRules::GetFieldWheelSliceAt(FVector2D(0, -100), HUB, RIM, FOUR_SLICES), 0);
			TestEqual(TEXT("right is the second (clockwise)"), FieldSkillRules::GetFieldWheelSliceAt(FVector2D(100, 0), HUB, RIM, FOUR_SLICES), 1);
			TestEqual(TEXT("below"), FieldSkillRules::GetFieldWheelSliceAt(FVector2D(0, 100), HUB, RIM, FOUR_SLICES), 2);
			TestEqual(TEXT("left"), FieldSkillRules::GetFieldWheelSliceAt(FVector2D(-100, 0), HUB, RIM, FOUR_SLICES), 3);
			TestEqual(TEXT("the middle circle picks nothing"), FieldSkillRules::GetFieldWheelSliceAt(FVector2D(0, -20), HUB, RIM, FOUR_SLICES), INDEX_NONE);
			TestEqual(TEXT("past the rim picks nothing"), FieldSkillRules::GetFieldWheelSliceAt(FVector2D(0, -200), HUB, RIM, FOUR_SLICES), INDEX_NONE);
			TestEqual(TEXT("no slices"), FieldSkillRules::GetFieldWheelSliceAt(FVector2D(0, -100), HUB, RIM, 0), INDEX_NONE);
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
			const TArray<FFieldAttackEntry> Entries = Field->GetFieldAttackEntries();
			if (!TestEqual(TEXT("one slice: Water and its Ice option (B-503m4)"), Entries.Num(), 2))
			{
				return;
			}
			const FFieldAttackEntry Innate = Field->GetFieldAttackEntry();
			TestTrue(TEXT("the innate element, thrown"), Innate.Kind == EFieldAttackKind::Innate && !Innate.IsSwing() && !Innate.bIce);
			TestTrue(TEXT("the Ice option (D-217 Ice)"), Entries[1].bHoldStep && Entries[1].bIce && Entries[1].Element == ESpellElement::Water &&
															 Entries[1].Name.EqualTo(WoRGameNames::GetIceName()));
			TestFalse(TEXT("plain Water isn't Ice"), Field->GetFieldStrike(false).bIce);
			Field->SelectFieldAttack(1);
			const FFieldStrike Ice = Field->GetFieldStrike(false);
			TestTrue(TEXT("the Ice option strikes Ice, no physical type"),
					 Ice.Element == ESpellElement::Water && Ice.bIce && Ice.Physical == EPhysicalDamageType::None);
			TestTrue(TEXT("one slice: a tap wraps onto itself"), Field->CycleFieldAttack().Kind == EFieldAttackKind::Innate && Field->GetFieldAttackIndex() == 0);
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

		It("gives a weapon user a slice per weapon, plain then its crystal, and swings that weapon (B-503m4, D-217)", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnWithWeapons(ECharacterClass::Generic, ESpellElement::Water)))
			{
				return;
			}
			const FString Sword = WoRGameNames::GetWeaponTypeName(EWeaponType::Sword).ToString();
			const TArray<FFieldAttackEntry> Entries = Field->GetFieldAttackEntries();
			if (!TestEqual(TEXT("the primary: plain and the crystal's Fire; the secondary (broken crystal): plain; no innate"), Entries.Num(), 3))
			{
				return;
			}
			TestTrue(TEXT("plain first: a swing with no element and the weapon's type"),
					 Entries[0].IsSwing() && Entries[0].Element == ESpellElement::None && Entries[0].Physical == EPhysicalDamageType::Slash);
			TestEqual(TEXT("named by the weapon type"), Entries[0].Name.ToString(), Sword);
			TestEqual(TEXT("the slice is the weapon"), Entries[0].SliceName.ToString(), Sword);
			TestTrue(TEXT("then infused, an option of the same slice: still the swing"),
					 Entries[1].bHoldStep && Entries[1].IsSwing() && Entries[1].Element == ESpellElement::Fire && Entries[1].Physical == EPhysicalDamageType::Slash);
			TestEqual(TEXT("the element first"), Entries[1].Name.ToString(),
					  FString::Printf(TEXT("%s %s"), *WoRGameNames::GetElementName(ESpellElement::Fire).ToString(), *Sword));
			TestTrue(TEXT("the secondary's own slice, plain, its own swing (no attack: no physical type)"),
					 !Entries[2].bHoldStep && Entries[2].IsSwing() && Entries[2].Element == ESpellElement::None && Entries[2].Physical == EPhysicalDamageType::None);

			TestEqual(TEXT("plain by default"), Field->GetFieldAttackIndex(), 0);
			TestTrue(TEXT("a plain strike"), Field->GetFieldStrike(false).Element == ESpellElement::None);
			Field->SelectFieldAttack(1);
			const FFieldStrike Swing = Field->GetFieldStrike(false);
			TestTrue(TEXT("the strike follows the pick"), Swing.Element == ESpellElement::Fire && Swing.Physical == EPhysicalDamageType::Slash);
			TestEqual(TEXT("a tap steps to the next slice"), Field->CycleFieldAttack().Physical, EPhysicalDamageType::None);
			TestTrue(TEXT("and wraps to the first"), Field->CycleFieldAttack().Element == ESpellElement::None && Field->GetFieldAttackIndex() == 0);
		});

		It("swings an entry's override attack over its weapon's own (B-503o)", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnWithWeapons(ECharacterClass::Generic, ESpellElement::Water)))
			{
				return;
			}
			UAbilityData *Pierce = NewObject<UAbilityData>(GetTransientPackage());
			Pierce->PhysicalDamageType = EPhysicalDamageType::Pierce;
			UInventoryComponent *Inventory = Field->GetOwner()->FindComponentByClass<UInventoryComponent>();
			FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
			Active.PrimaryWeapon.OverrideAttack = Pierce;
			Active.SecondaryWeapon.OverrideAttack = Pierce;
			const TArray<FFieldAttackEntry> Entries = Field->GetFieldAttackEntries();
			if (!TestEqual(TEXT("the same three entries"), Entries.Num(), 3))
			{
				return;
			}
			TestEqual(TEXT("the primary swings the override, not its Slash"), Entries[0].Physical, EPhysicalDamageType::Pierce);
			TestEqual(TEXT("infused too"), Entries[1].Physical, EPhysicalDamageType::Pierce);
			TestEqual(TEXT("the secondary's slice swings its own override"), Entries[2].Physical, EPhysicalDamageType::Pierce);
		});

		It("gives a weapon user's ring its own slice, infusing the weapon in hand (B-503m4, D-217)", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnWithWeapons(ECharacterClass::Generic, ESpellElement::Water)))
			{
				return;
			}
			UInventoryComponent *Inventory = Field->GetOwner()->FindComponentByClass<UInventoryComponent>();
			URingData *Ring = NewObject<URingData>(GetTransientPackage());
			Ring->Name = TEXT("Storm Ring");
			FRingLoadoutEntry &Adapter = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].RingLoadout.AddDefaulted_GetRef();
			Adapter.RingEntry.Ring = Ring;
			Adapter.RingEntry.AttachedItem.Kind = EAttachedItemKind::Crystal;
			constexpr int32 LIVE_DURABILITY = 10;
			Adapter.RingEntry.AttachedItem.Crystal = FCrystalAttachment(FCrystalId(ECrystalType::Citrine, EItemTier::F_Tier), LIVE_DURABILITY);
			const TArray<FFieldAttackEntry> Entries = Field->GetFieldAttackEntries();
			if (!TestEqual(TEXT("the two weapons' three, then the ring"), Entries.Num(), 4))
			{
				return;
			}
			const FFieldAttackEntry &Storm = Entries[3];
			TestEqual(TEXT("the ring's slice"), Storm.SliceName.ToString(), FString(TEXT("Storm Ring")));
			TestTrue(TEXT("its own slice, the crystal's Lightning on the weapon in hand's swing"),
					 !Storm.bHoldStep && Storm.IsSwing() && Storm.Element == ESpellElement::Lightning && Storm.Physical == EPhysicalDamageType::Slash);
			TestTrue(TEXT("named as the strip reads"), Storm.Name.ToString().StartsWith(WoRGameNames::GetElementName(ESpellElement::Lightning).ToString()));
		});

		It("gives a Refractor a slice per entry, thrown and named by the element (B-503m4, D-217)", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnWithWeapons(ECharacterClass::Caster, ESpellElement::Water)))
			{
				return;
			}
			const TArray<FFieldAttackEntry> Entries = Field->GetFieldAttackEntries();
			if (!TestEqual(TEXT("the crystal's Fire; the innate Water and its Ice; no plain stop"), Entries.Num(), 3))
			{
				return;
			}
			TestTrue(TEXT("the crystal in hand first (loadout order), thrown with no physical type"),
					 !Entries[0].IsSwing() && Entries[0].Element == ESpellElement::Fire && Entries[0].Physical == EPhysicalDamageType::None);
			TestEqual(TEXT("no weapon line"), Entries[0].Name.ToString(), WoRGameNames::GetElementName(ESpellElement::Fire).ToString());
			TestTrue(TEXT("then the innate element, its own slice, a pulse"), !Entries[1].bHoldStep && Entries[1].Kind == EFieldAttackKind::Innate &&
																				  !Entries[1].IsSwing() && Entries[1].Element == ESpellElement::Water);
			TestEqual(TEXT("just the element"), Entries[1].Name.ToString(), WoRGameNames::GetElementName(ESpellElement::Water).ToString());
			TestTrue(TEXT("and its Ice"), Entries[2].bHoldStep && Entries[2].bIce);
		});

		It("steps a Reality Refractor's innate through its other gear's elements (B-503l, Q-503l)", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnWithWeapons(ECharacterClass::Caster, ESpellElement::Reality)))
			{
				return;
			}
			const TArray<FFieldAttackEntry> Entries = Field->GetFieldAttackEntries();
			if (!TestEqual(TEXT("the crystal, the innate, its option"), Entries.Num(), 3))
			{
				return;
			}
			const FString Fire = WoRGameNames::GetElementName(ESpellElement::Fire).ToString();
			TestTrue(TEXT("the crystal's own slice, no option (not innate or evolution)"), !Entries[0].bHoldStep && Entries[0].Name.ToString() == Fire);
			TestEqual(TEXT("Reality reads Unknown (ED-42)"), Entries[1].Name.ToString(), WoRGameNames::GetElementName(ESpellElement::Reality).ToString());
			TestTrue(TEXT("its option: the crystal's Fire"), Entries[2].bHoldStep && Entries[2].Element == ESpellElement::Fire && !Entries[2].IsSwing());
			TestTrue(TEXT("named (R) Fire"), Entries[2].Name.ToString().StartsWith(TEXT("(R)")) && Entries[2].Name.ToString().EndsWith(Fire));
			TestEqual(TEXT("a hold on the crystal stays"), Field->CycleFieldAttack(true).Name.ToString(), Fire);
			TestEqual(TEXT("a press: the innate"), Field->CycleFieldAttack(false).Element, ESpellElement::Reality);
			TestTrue(TEXT("a hold: (R) Fire"), Field->CycleFieldAttack(true).bHoldStep && Field->GetFieldStrike(false).Element == ESpellElement::Fire);
			TestEqual(TEXT("a hold again: back to Unknown"), Field->CycleFieldAttack(true).Element, ESpellElement::Reality);
		});

		It("steps a Broken Darkness character's Darkness through its other gear's elements (B-503l, Q-503l)", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnWithWeapons(ECharacterClass::Caster, ESpellElement::Darkness)))
			{
				return;
			}
			UCharacterDataComponent *CharComp = Field->GetOwner()->FindComponentByClass<UCharacterDataComponent>();
			if (!TestNotNull(TEXT("character"), CharComp))
			{
				return;
			}
			CharComp->bIsBrokenDarkness = true;
			const TArray<FFieldAttackEntry> Entries = Field->GetFieldAttackEntries();
			if (!TestEqual(TEXT("the crystal, Darkness, its option"), Entries.Num(), 3))
			{
				return;
			}
			TestEqual(TEXT("Darkness"), Entries[1].Element, ESpellElement::Darkness);
			TestTrue(TEXT("Dark Fire"), Entries[2].bHoldStep && Entries[2].Element == ESpellElement::Fire &&
											Entries[2].Name.ToString().EndsWith(WoRGameNames::GetElementName(ESpellElement::Fire).ToString()) &&
											!Entries[2].Name.ToString().StartsWith(TEXT("(R)")));
			CharComp->bIsBrokenDarkness = false;
			TestEqual(TEXT("not Broken Darkness: no option"), Field->GetFieldAttackEntries().Num(), 2);
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

		It("times the swing's hit to the montage's earliest Impact notify, and hits at once with none", [this]()
		{
			constexpr float VFX_TIME = 0.3f;
			constexpr float IMPACT_TIME = 0.4f;
			constexpr float LATER_IMPACT_TIME = 0.9f;
			constexpr float DOUBLE_RATE = 2.0f;
			UAnimMontage *Montage = NewObject<UAnimMontage>(GetTransientPackage());
			const auto AddNotify = [Montage](float Time, ECombatNotifyFamily Family)
			{
				UCombatNotify *Notify = NewObject<UCombatNotify>(Montage);
				Notify->Family = Family;
				FAnimNotifyEvent &Event = Montage->Notifies.AddDefaulted_GetRef();
				Event.Link(Montage, Time);
				Event.Notify = Notify;
			};
			TestEqual(TEXT("no montage"), FieldSkillRules::GetSwingImpactSeconds(nullptr), FieldSkillRules::NO_SWING_DELAY);
			AddNotify(VFX_TIME, ECombatNotifyFamily::VFX);
			TestEqual(TEXT("no Impact notify"), FieldSkillRules::GetSwingImpactSeconds(Montage), FieldSkillRules::NO_SWING_DELAY);
			AddNotify(LATER_IMPACT_TIME, ECombatNotifyFamily::Impact);
			AddNotify(IMPACT_TIME, ECombatNotifyFamily::Impact);
			TestEqual(TEXT("the earliest Impact"), FieldSkillRules::GetSwingImpactSeconds(Montage), IMPACT_TIME);
			Montage->RateScale = DOUBLE_RATE;
			TestEqual(TEXT("at the montage's rate"), FieldSkillRules::GetSwingImpactSeconds(Montage), IMPACT_TIME / DOUBLE_RATE);
		});

		It("gives a weapon entry its attack's swing, and holds the next swing until the hit has landed", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnWithWeapons(ECharacterClass::Generic, ESpellElement::Water)))
			{
				return;
			}
			constexpr float IMPACT_TIME = 5.0f; // past the cooldown: only the pending hit can refuse the second swing
			UAnimMontage *Montage = NewObject<UAnimMontage>(GetTransientPackage());
			UCombatNotify *Notify = NewObject<UCombatNotify>(Montage);
			FAnimNotifyEvent &Event = Montage->Notifies.AddDefaulted_GetRef();
			Event.Link(Montage, IMPACT_TIME);
			Event.Notify = Notify;
			const ULoadoutComponent *Loadout = Field->GetOwner()->FindComponentByClass<ULoadoutComponent>();
			Loadout->GetActiveLoadout().PrimaryWeapon.WeaponEntry.Weapon->WeaponAttack->FieldMontage = Montage;

			const TArray<FFieldAttackEntry> Entries = Field->GetFieldAttackEntries();
			TestTrue(TEXT("the primary weapon swings with it"), Entries[0].IsSwing() && Entries[0].SwingMontage.Get() == Montage);
			TestTrue(TEXT("infused, the same swing (B-503k)"), Entries.IsValidIndex(1) && Entries[1].IsSwing() && Entries[1].SwingMontage.Get() == Montage);
			TestTrue(TEXT("B-503m4: the secondary weapon's slice swings its own attack (none here)"), Entries.Last().IsSwing() && Entries.Last().SwingMontage.IsNull());
			TestTrue(TEXT("a swing"), Field->FieldAttack(false) == EFieldSkillRefusal::None);
			constexpr int32 MAX_FRAMES = 1000;
			const UWorld *World = TestWorld->GetWorld();
			const double CooldownOver = World->GetTimeSeconds() + FieldSkillRules::FIELD_ATTACK_COOLDOWN_SECONDS;
			for (int32 Frame = 0; Frame < MAX_FRAMES && World->GetTimeSeconds() <= CooldownOver; ++Frame)
			{
				TestWorld->Tick();
			}
			TestTrue(TEXT("the cooldown is over"), World->GetTimeSeconds() > CooldownOver);
			TestTrue(TEXT("the hit is still to land"), Field->FieldAttack(false) == EFieldSkillRefusal::OnCooldown);
		});

		It("throws the pulse further than the swing reaches, and waits for the cast's release (B-503c)", [this]()
		{
			TestTrue(TEXT("the pulse outreaches the swing"),
					 FieldSkillRules::GetFieldAttackRange(false) == FieldSkillRules::FIELD_PULSE_RANGE &&
						 FieldSkillRules::GetFieldAttackRange(true) == FieldSkillRules::FIELD_ATTACK_RANGE &&
						 FieldSkillRules::FIELD_PULSE_RANGE > FieldSkillRules::FIELD_ATTACK_RANGE);
			TestEqual(TEXT("a thinner sweep"), FieldSkillRules::GetFieldAttackRadius(false), FieldSkillRules::FIELD_PULSE_RADIUS);
			if (!TestTrue(TEXT("spawned"), SpawnCaster(ESpellElement::Fire)))
			{
				return;
			}
			constexpr float RELEASE_TIME = 5.0f; // past the cooldown: only the pending pulse can refuse the second one
			UAnimMontage *CastMontage = NewObject<UAnimMontage>(GetTransientPackage());
			FAnimNotifyEvent &Event = CastMontage->Notifies.AddDefaulted_GetRef();
			Event.Link(CastMontage, RELEASE_TIME);
			Event.Notify = NewObject<UCombatNotify>(CastMontage);
			Field->FieldPulseMontage = CastMontage;
			TestFalse(TEXT("the innate element is a pulse"), Field->GetFieldAttackEntry().IsSwing());
			TestTrue(TEXT("a pulse"), Field->FieldAttack(false) == EFieldSkillRefusal::None);
			constexpr int32 MAX_FRAMES = 1000;
			const UWorld *World = TestWorld->GetWorld();
			const double CooldownOver = World->GetTimeSeconds() + FieldSkillRules::FIELD_ATTACK_COOLDOWN_SECONDS;
			for (int32 Frame = 0; Frame < MAX_FRAMES && World->GetTimeSeconds() <= CooldownOver; ++Frame)
			{
				TestWorld->Tick();
			}
			TestTrue(TEXT("the pulse has not left yet"), Field->FieldAttack(false) == EFieldSkillRefusal::OnCooldown);
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
