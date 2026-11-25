// WeaponBuffTintSpec.cpp
// B-622 (D-275, Crown 2026-11-25: "Yes, tint only"): a weapon buff or debuff takes no infusion, but its VFX takes the
// colour of the caster's infusion that is on: the action's LookElement, through the same EffectTint path. No EP for
// it, and the action's element stays its own. With no infusion on, no tint. The menu's half is in
// WoR.UI.CommandMenu.Keys. Unproven here: the colour on screen (no montage VFX headless; docs/PIE/B-622.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Presentation/EffectTint.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SupportSkillRules.h"
#include "Skills/Effects/EffectDefinition.h"

namespace
{
	constexpr float WEAPON_BUFF_TINT_HEAL = 10.0f;
	constexpr int32 WEAPON_BUFF_TINT_EP = 40;
	/** Spans the executor's 10 s action timeout at 0.01 s a frame; a run stops once the action completes. */
	constexpr int32 WEAPON_BUFF_TINT_SETTLE_FRAMES = 1100;
}

BEGIN_DEFINE_SPEC(FWeaponBuffTintSpec, "WoR.Presentation.WeaponBuffTint",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UActionExecutor *Executor = nullptr;
	AActor *Hero = nullptr;
	UAbilityData *Rally = nullptr;

	/** Hero uses Rally on itself with Source riding on the action (at no level, as the menu sends it). */
	TOptional<FActionResult> UseRally(EInfusionSourceOption Source)
	{
		FAction Action;
		Action.ActionType = EActionType::Ability;
		Action.SkillData = Rally;
		Action.SelectedSource = Source;
		Action.Targets.Add(Hero);
		TOptional<FActionResult> Result;
		Executor->ExecuteActionAsync(Hero, Action, FOnActionComplete::CreateLambda([&Result](const FActionResult &Done) { Result = Done; }));
		for (int32 Frame = 0; Frame < WEAPON_BUFF_TINT_SETTLE_FRAMES && !Result.IsSet(); ++Frame)
		{
			TestWorld->Tick();
		}
		return Result;
	}
END_DEFINE_SPEC(FWeaponBuffTintSpec)

void FWeaponBuffTintSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Executor = TestWorld->GetSubsystem<UActionExecutor>();
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Caster;
		Data->InnateElement = ESpellElement::Fire;
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, NewObject<UWeaponData>(GetTransientPackage()));
		Hero = Loadout ? Loadout->GetOwner() : nullptr;
		if (UCharacterDataComponent *Stats = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr)
		{
			Stats->MaxEP = Stats->CurrentEP = WEAPON_BUFF_TINT_EP;
		}

		FSkillEffectPayload Payload;
		Payload.EffectType = ESkillEffectType::HealthRestore;
		Payload.Magnitude = WEAPON_BUFF_TINT_HEAL;
		FSkillEffect Effect;
		Effect.Payloads.Add(Payload);
		UEffectDefinition *Definition = NewObject<UEffectDefinition>(GetTransientPackage());
		Definition->Effects.Add(Effect);
		Rally = NewObject<UAbilityData>(GetTransientPackage());
		Rally->Name = TEXT("Montage-less Rally");
		Rally->BaseDamage = 0;
		Rally->TargetType = ETargetType::Self;
		Rally->ExecutionType = EAbilityExecutionType::Melee;
		Rally->ReferencedEffects.Add(Definition);
		Rally->AddToRoot();
	});

	AfterEach([this]()
	{
		if (Rally)
		{
			Rally->RemoveFromRoot();
			Rally = nullptr;
		}
		Executor = nullptr;
		Hero = nullptr;
		TestWorld.Reset();
	});

	It("tints with the look element when it is an element, else the action's", [this]()
	{
		TestTrue(TEXT("an infusion on: its element"), EffectTint::ChooseTintElement(ESpellElement::None, ESpellElement::Fire) == ESpellElement::Fire);
		TestTrue(TEXT("no infusion on: the action's own"), EffectTint::ChooseTintElement(ESpellElement::Water, ESpellElement::None) == ESpellElement::Water);
		TestTrue(TEXT("an inherit look doesn't tint"), EffectTint::ChooseTintElement(ESpellElement::None, ESpellElement::Generic) == ESpellElement::None);
	});

	It("gives a weapon buff the infusion's element as its look, not as its element", [this]()
	{
		if (!TestNotNull(TEXT("Executor"), Executor) || !TestNotNull(TEXT("Hero"), Hero)) return;
		TestTrue(TEXT("Rally is a weapon buff"), SupportSkillRules::IsWeaponBuffOrDebuff(Rally));
		const TOptional<FActionResult> Result = UseRally(EInfusionSourceOption::Innate);
		if (!TestTrue(TEXT("Rally completed"), Result.IsSet())) return;
		TestTrue(FString::Printf(TEXT("it ran (%s)"), *Result->ErrorMessage), Result->bSuccess);
		TestTrue(TEXT("the look is the infusion's Fire"), Result->LookElement == ESpellElement::Fire);
		TestTrue(TEXT("the action itself isn't Fire"), Result->AttackElement != ESpellElement::Fire);
		TestTrue(TEXT("its VFX tint is Fire"), EffectTint::ChooseTintElement(Result->AttackElement, Result->LookElement) == ESpellElement::Fire);
	});

	It("leaves it clear with no infusion on", [this]()
	{
		if (!TestNotNull(TEXT("Executor"), Executor) || !TestNotNull(TEXT("Hero"), Hero)) return;
		const TOptional<FActionResult> Result = UseRally(EInfusionSourceOption::None);
		if (!TestTrue(TEXT("Rally completed"), Result.IsSet())) return;
		TestTrue(TEXT("it ran"), Result->bSuccess);
		TestTrue(TEXT("no look element"), Result->LookElement == ESpellElement::None);
		TestFalse(TEXT("no tint"), EffectTint::ShouldSetSoftTofuColour(EffectTint::ChooseTintElement(Result->AttackElement, Result->LookElement)));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
