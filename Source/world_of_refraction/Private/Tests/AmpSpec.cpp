// AmpSpec.cpp
// B-382 (Q-164b A, Crown 2026-11-08): Amp lines in one sum. A Fire Amp raises a Fire action only; an All Amp
// raises any; Physical Amp raises an unelemental action; worn gear's lines add into one total that joins the
// action's base before the multipliers; a 0-damage action takes none. A DamageBuff keyed to Fire no longer raises a
// Water hit, while an unkeyed one still raises every hit.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Debug/Combat/AmpDebug.h"
#include "Equipment/FAmpBonus.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 AMP_TEST_BASE_DAMAGE = 100;
	constexpr int32 AMP_TEST_ALL = 2;
	constexpr int32 AMP_TEST_FIRE = 3;
	constexpr int32 AMP_TEST_PHYSICAL = 1;
	constexpr int32 AMP_TEST_BUFF_EFFECT_ID = 9382;
	constexpr float AMP_TEST_DOUBLING_PERCENT = 100.0f;
	constexpr int32 AMP_TEST_BUFF_TURNS = 2;
}

BEGIN_DEFINE_SPEC(FAmpSpec, "WoR.Damage.Amp", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	static FCombatLoadout &ActiveOf(ULoadoutComponent *Loadout)
	{
		UInventoryComponent *Inventory = Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>();
		return Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	}

	/** A Generic holding a weapon whose instance carries Amp. */
	ULoadoutComponent *SpawnWithWeaponAmp(const FAmpBonus &Amp)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Generic;
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->Tier = EItemTier::F_Tier;
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, Weapon);
		if (Loadout)
		{
			ActiveOf(Loadout).PrimaryWeapon.WeaponEntry.StatBonus.Amp = Amp;
		}
		return Loadout;
	}

	static FAction SpellAction(ESpellElement Element, int32 BaseDamage)
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->BaseDamage = BaseDamage;
		Spell->Element = Element;
		FAction Action;
		Action.ActionType = EActionType::Spell;
		Action.SpellData = Spell;
		return Action;
	}

	static FAmpBonus Lines(int32 All, int32 Fire, int32 Physical = 0)
	{
		FAmpBonus Amp;
		Amp.All = All;
		Amp.Fire = Fire;
		Amp.Physical = Physical;
		return Amp;
	}
END_DEFINE_SPEC(FAmpSpec)

void FAmpSpec::Define()
{
	Describe("the lines", [this]()
	{
		It("a Fire line counts for Fire only; All for every element; Physical for unelemental", [this]()
		{
			const FAmpBonus Amp = Lines(AMP_TEST_ALL, AMP_TEST_FIRE, AMP_TEST_PHYSICAL);
			TestEqual(TEXT("Fire action"), Amp.GetTotalFor(ESpellElement::Fire), AMP_TEST_ALL + AMP_TEST_FIRE);
			TestEqual(TEXT("Water action"), Amp.GetTotalFor(ESpellElement::Water), AMP_TEST_ALL);
			TestEqual(TEXT("physical action"), Amp.GetTotalFor(ESpellElement::None), AMP_TEST_ALL + AMP_TEST_PHYSICAL);
		});

		It("add up across items, line by line", [this]()
		{
			FAmpBonus Sum = Lines(AMP_TEST_ALL, 0);
			Sum.Accumulate(Lines(0, AMP_TEST_FIRE));
			TestEqual(TEXT("one total"), Sum.GetTotalFor(ESpellElement::Fire), AMP_TEST_ALL + AMP_TEST_FIRE);
			TestFalse(TEXT("not empty"), Sum.IsEmpty());
			TestTrue(TEXT("a fresh one is empty"), FAmpBonus().IsEmpty());
		});
	});

	Describe("in the damage preview", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("test world created"), TestWorld->Create());
		});

		AfterEach([this]() { TestWorld.Reset(); });

		It("worn gear's Amp joins a Fire spell's base, and not a Water spell's", [this]()
		{
			UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
			ULoadoutComponent *Loadout = SpawnWithWeaponAmp(Lines(0, AMP_TEST_FIRE));
			if (!TestNotNull(TEXT("executor"), Executor) || !TestNotNull(TEXT("loadout"), Loadout))
			{
				return;
			}
			AActor *User = Loadout->GetOwner();
			TestEqual(TEXT("the gear sum"), Loadout->GetActiveStatBonus(User).Amp.Fire, AMP_TEST_FIRE);

			const FActionDamagePreview Fire = Executor->PreviewActionDamage(User, SpellAction(ESpellElement::Fire, AMP_TEST_BASE_DAMAGE), FActionStatModifiers());
			const FActionDamagePreview Water = Executor->PreviewActionDamage(User, SpellAction(ESpellElement::Water, AMP_TEST_BASE_DAMAGE), FActionStatModifiers());
			TestEqual(TEXT("Fire takes the Fire Amp"), Fire.AmpBonus, AMP_TEST_FIRE);
			TestEqual(TEXT("Water takes none"), Water.AmpBonus, 0);
			TestTrue(TEXT("so the Fire spell hits harder"), Fire.FinalDamage > Water.FinalDamage);
		});

		It("an All Amp raises any element", [this]()
		{
			UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
			ULoadoutComponent *Loadout = SpawnWithWeaponAmp(Lines(AMP_TEST_ALL, 0));
			if (!TestNotNull(TEXT("executor"), Executor) || !TestNotNull(TEXT("loadout"), Loadout))
			{
				return;
			}
			AActor *User = Loadout->GetOwner();
			TestEqual(TEXT("Water"), Executor->PreviewActionDamage(User, SpellAction(ESpellElement::Water, AMP_TEST_BASE_DAMAGE), FActionStatModifiers()).AmpBonus,
					  AMP_TEST_ALL);
			TestEqual(TEXT("Void"), Executor->PreviewActionDamage(User, SpellAction(ESpellElement::Void, AMP_TEST_BASE_DAMAGE), FActionStatModifiers()).AmpBonus,
					  AMP_TEST_ALL);
		});

		It("a 0-damage spell takes no Amp; no loadout has no Amp", [this]()
		{
			UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
			ULoadoutComponent *Loadout = SpawnWithWeaponAmp(Lines(AMP_TEST_ALL, AMP_TEST_FIRE));
			if (!TestNotNull(TEXT("executor"), Executor) || !TestNotNull(TEXT("loadout"), Loadout))
			{
				return;
			}
			const FActionDamagePreview Support = Executor->PreviewActionDamage(Loadout->GetOwner(), SpellAction(ESpellElement::Fire, 0), FActionStatModifiers());
			TestEqual(TEXT("support spell: no Amp"), Support.AmpBonus, 0);
			TestEqual(TEXT("support spell: still 0 damage"), Support.FinalDamage, 0);
			AActor *Bare = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			TestEqual(TEXT("no loadout"), Executor->GetActionAmp(Bare, ESpellElement::Fire), 0);
			TestTrue(TEXT("debug string names the lines"), WoRAmpDebug::GetAmpString(Loadout->GetOwner()).Contains(TEXT("Fire")));
		});
	});

	Describe("element-keyed damage buffs", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("test world created"), TestWorld->Create());
		});

		AfterEach([this]() { TestWorld.Reset(); });

		auto HitWithBuff = [this](ESpellElement BuffElement, ESpellElement HitElement) -> int32
		{
			UDamageCalculator *Calculator = TestWorld->GetSubsystem<UDamageCalculator>();
			USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
			if (!Calculator || !Effects)
			{
				AddError(TEXT("no calculator or effect manager"));
				return INDEX_NONE;
			}
			AActor *Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			AActor *Defender = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			FActiveSkillEffect Buff = FActiveSkillEffect::CreateBuff(TEXT("Damage Up"), AMP_TEST_BUFF_EFFECT_ID, ESkillEffectType::DamageBuff,
																	 AMP_TEST_DOUBLING_PERCENT, AMP_TEST_BUFF_TURNS);
			Buff.Element = BuffElement;
			Effects->ApplyEffect(Attacker, Buff);
			FDamageCalculationInput Input;
			Input.BaseDamage = AMP_TEST_BASE_DAMAGE;
			Input.ActionType = EActionType::Spell;
			Input.Element = HitElement;
			Input.bIgnoreResistance = true;
			Input.OverrideCritChance = 0.0f;
			return Calculator->CalculateDamage(Attacker, Defender, Input).FinalDamage;
		};

		It("a Fire damage buff raises a Fire hit, not a Water hit", [this, HitWithBuff]()
		{
			const int32 FireHit = HitWithBuff(ESpellElement::Fire, ESpellElement::Fire);
			const int32 WaterHit = HitWithBuff(ESpellElement::Fire, ESpellElement::Water);
			const int32 Unbuffed = HitWithBuff(ESpellElement::Fire, ESpellElement::None);
			TestTrue(TEXT("Fire hit is raised"), FireHit > WaterHit);
			TestEqual(TEXT("Water hit is not (same as a hit the buff does not key)"), WaterHit, Unbuffed);
		});

		It("an unkeyed damage buff still raises every hit", [this, HitWithBuff]()
		{
			TestEqual(TEXT("Water and Fire alike"), HitWithBuff(ESpellElement::None, ESpellElement::Water),
					  HitWithBuff(ESpellElement::None, ESpellElement::Fire));
			TestTrue(TEXT("and above a Fire-keyed buff's Water hit"),
					 HitWithBuff(ESpellElement::None, ESpellElement::Water) > HitWithBuff(ESpellElement::Fire, ESpellElement::Water));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
