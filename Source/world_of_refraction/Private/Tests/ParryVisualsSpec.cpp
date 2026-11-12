// ParryVisualsSpec.cpp
// B-393 (D-170 corrected 2026-11-09, Q-393): a landed parry's visuals. The weapon guard always sparks; the guard's
// parry-effect cosmetic plays tinted by the element behind it (the weapon's crystal, weapon guard only; a Refractor's
// element; an adapter's crystal); the loadout priority picks when two apply; with no element there is no effect, so a
// weapon sparks only and the unarmed guard shows nothing. A Perfect Guard's flash takes the spark's place.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/Presentation/ParryVisuals.h"
#include "Character/CharacterData.h"
#include "Character/CosmeticsData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Infusion/ElementColors.h"
#include "Loadout/LoadoutComponent.h"
#include "NiagaraSystem.h"

namespace
{
	TSoftObjectPtr<UNiagaraSystem> ParryTestVFX(const TCHAR *Name)
	{
		return TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(FString::Printf(TEXT("/Game/Test/%s.%s"), Name, Name)));
	}

	/** A weapon-guard parry with every cosmetic slot filled and no element behind it. */
	FParryVisualInputs WeaponGuardInputs()
	{
		FParryVisualInputs Inputs;
		Inputs.Guard = EGuardStyle::Weapon;
		Inputs.Spark = ParryTestVFX(TEXT("Spark"));
		Inputs.WeaponGuardEffect = ParryTestVFX(TEXT("WeaponCircle"));
		Inputs.UnarmedGuardEffect = ParryTestVFX(TEXT("BodyCircle"));
		return Inputs;
	}

	constexpr bool PARRY_TEST_NORMAL_GUARD = false;
}

BEGIN_DEFINE_SPEC(FParryVisualsSpec, "WoR.Presentation.ParryVisuals",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	/** A combatant of Class (a Caster keyed to Innate) whose cosmetics fill every parry slot; with bArmed, holding a
	 *  weapon with no crystal. */
	ULoadoutComponent *SpawnDefender(ECharacterClass Class, ESpellElement Innate, bool bArmed)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = Class;
		Data->InnateElement = Innate;
		Data->Cosmetics = NewObject<UCosmeticsData>(GetTransientPackage());
		Data->Cosmetics->GuardSparkVFX = ParryTestVFX(TEXT("Spark"));
		Data->Cosmetics->WeaponGuardParryEffectVFX = ParryTestVFX(TEXT("WeaponCircle"));
		Data->Cosmetics->UnarmedGuardParryEffectVFX = ParryTestVFX(TEXT("BodyCircle"));
		return TestWorld->SpawnCombatantWithLoadout(Data, bArmed ? NewObject<UWeaponData>(GetTransientPackage()) : nullptr);
	}
END_DEFINE_SPEC(FParryVisualsSpec)

void FParryVisualsSpec::Define()
{
	Describe("the rule", [this]()
	{
		It("a weapon with no element sparks only", [this]()
		{
			const FParryVisualChoice Choice = ParryVisuals::Resolve(WeaponGuardInputs());
			TestTrue(TEXT("sparks"), Choice.Spark == ParryTestVFX(TEXT("Spark")));
			TestFalse(TEXT("no parry effect"), Choice.ShouldSpawnEffect());
			TestTrue(TEXT("no element source"), Choice.ElementSource == EParryElementSource::None);
		});

		It("unarmed with no element shows nothing", [this]()
		{
			FParryVisualInputs Inputs = WeaponGuardInputs();
			Inputs.Guard = EGuardStyle::Unarmed;
			const FParryVisualChoice Choice = ParryVisuals::Resolve(Inputs);
			TestFalse(TEXT("no spark"), Choice.ShouldSpawnSpark());
			TestFalse(TEXT("no parry effect"), Choice.ShouldSpawnEffect());
		});

		It("a weapon's crystal tints the weapon guard's effect, on top of the spark", [this]()
		{
			FParryVisualInputs Inputs = WeaponGuardInputs();
			Inputs.WeaponElement = ESpellElement::Fire;
			const FParryVisualChoice Choice = ParryVisuals::Resolve(Inputs);
			TestTrue(TEXT("sparks"), Choice.ShouldSpawnSpark());
			TestTrue(TEXT("the weapon guard's effect"), Choice.Effect == ParryTestVFX(TEXT("WeaponCircle")));
			TestTrue(TEXT("Fire, from the weapon"), Choice.Element == ESpellElement::Fire && Choice.ElementSource == EParryElementSource::Weapon);
			TestTrue(TEXT("Fire's colour"), Choice.Tint.Equals(ElementColors::GetColorForElement(ESpellElement::Fire)));
		});

		It("the loadout priority picks between adapter and weapon", [this]()
		{
			FParryVisualInputs Inputs = WeaponGuardInputs();
			Inputs.WeaponElement = ESpellElement::Fire;
			Inputs.MagicElement = ESpellElement::Water;
			Inputs.Priority = EParryElementPriority::Weapon;
			TestTrue(TEXT("Weapon first: Fire"), ParryVisuals::Resolve(Inputs).Element == ESpellElement::Fire);
			Inputs.Priority = EParryElementPriority::Magic;
			const FParryVisualChoice Magic = ParryVisuals::Resolve(Inputs);
			TestTrue(TEXT("Magic first: Water, from the adapter"), Magic.Element == ESpellElement::Water && Magic.ElementSource == EParryElementSource::Magic);
			Inputs.MagicElement = ESpellElement::None;
			TestTrue(TEXT("Magic first with none: the weapon's Fire"), ParryVisuals::Resolve(Inputs).Element == ESpellElement::Fire);
		});

		It("an unarmed guard has put the weapon away: its crystal doesn't count", [this]()
		{
			FParryVisualInputs Inputs = WeaponGuardInputs();
			Inputs.Guard = EGuardStyle::Unarmed;
			Inputs.WeaponElement = ESpellElement::Fire;
			TestFalse(TEXT("nothing"), ParryVisuals::Resolve(Inputs).ShouldSpawnEffect());
			Inputs.MagicElement = ESpellElement::Wind;
			const FParryVisualChoice Choice = ParryVisuals::Resolve(Inputs);
			TestTrue(TEXT("the unarmed guard's effect"), Choice.Effect == ParryTestVFX(TEXT("BodyCircle")));
			TestTrue(TEXT("Wind"), Choice.Element == ESpellElement::Wind);
		});

		It("Generic is no colour; a Perfect Guard's flash replaces the spark but the effect plays", [this]()
		{
			FParryVisualInputs Inputs = WeaponGuardInputs();
			Inputs.WeaponElement = ESpellElement::Generic;
			TestFalse(TEXT("Generic: no effect"), ParryVisuals::Resolve(Inputs).ShouldSpawnEffect());
			Inputs.WeaponElement = ESpellElement::Light;
			Inputs.bPerfect = true;
			const FParryVisualChoice Perfect = ParryVisuals::Resolve(Inputs);
			TestFalse(TEXT("no spark (the flash plays)"), Perfect.ShouldSpawnSpark());
			TestTrue(TEXT("the effect, Light"), Perfect.ShouldSpawnEffect() && Perfect.Element == ESpellElement::Light);
		});
	});

	Describe("the defender", [this]()
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

		It("a Refractor's circle takes its source element", [this]()
		{
			const ULoadoutComponent *Loadout = SpawnDefender(ECharacterClass::Caster, ESpellElement::Fire, false);
			if (!TestNotNull(TEXT("spawned"), Loadout)) return;
			const FParryVisualChoice Choice = ParryVisuals::ResolveForDefender(Loadout->GetOwner(), PARRY_TEST_NORMAL_GUARD);
			TestFalse(TEXT("unarmed: no spark"), Choice.ShouldSpawnSpark());
			TestTrue(TEXT("the unarmed guard's circle"), Choice.Effect == ParryTestVFX(TEXT("BodyCircle")));
			TestTrue(TEXT("Fire, its own"), Choice.Element == ESpellElement::Fire && Choice.ElementSource == EParryElementSource::Magic);
		});

		It("a weapon with no element sparks only", [this]()
		{
			const ULoadoutComponent *Loadout = SpawnDefender(ECharacterClass::Generic, ESpellElement::Generic, true);
			if (!TestNotNull(TEXT("spawned"), Loadout)) return;
			const FParryVisualChoice Choice = ParryVisuals::ResolveForDefender(Loadout->GetOwner(), PARRY_TEST_NORMAL_GUARD);
			TestTrue(TEXT("sparks"), Choice.Spark == ParryTestVFX(TEXT("Spark")));
			TestFalse(TEXT("no parry effect"), Choice.ShouldSpawnEffect());
		});

		It("the priority setting is the loadout's, default Weapon", [this]()
		{
			ULoadoutComponent *Loadout = SpawnDefender(ECharacterClass::Generic, ESpellElement::Generic, true);
			if (!TestNotNull(TEXT("spawned"), Loadout)) return;
			TestTrue(TEXT("default Weapon"), Loadout->GetParryElementPriority() == EParryElementPriority::Weapon);
			Loadout->SetParryElementPriority(EParryElementPriority::Magic);
			TestTrue(TEXT("Magic kept"), Loadout->GetParryElementPriority() == EParryElementPriority::Magic);
			TestTrue(TEXT("the resolver reads it"),
					 ParryVisuals::GatherInputs(Loadout->GetOwner(), PARRY_TEST_NORMAL_GUARD).Priority == EParryElementPriority::Magic);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
