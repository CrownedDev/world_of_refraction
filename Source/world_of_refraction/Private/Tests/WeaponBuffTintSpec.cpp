// WeaponBuffTintSpec.cpp
// B-622b (D-275, Crown 2026-11-25: "wait rally doesnt get a tint as buffs cannot be infused"; replaces B-622's tint):
// a weapon buff or debuff takes no infusion and no tint, even with an infusion riding on its action. Its VFX tint is
// the action's own element (AttackElement), which isn't the infusion's. Unproven here: the look on screen (no montage
// VFX headless; docs/PIE/B-622b.md).

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

	It("leaves a weapon buff untinted with a Fire infusion riding on it", [this]()
	{
		if (!TestNotNull(TEXT("Executor"), Executor) || !TestNotNull(TEXT("Hero"), Hero)) return;
		TestTrue(TEXT("Rally is a weapon buff"), SupportSkillRules::IsWeaponBuffOrDebuff(Rally));
		const TOptional<FActionResult> Result = UseRally(EInfusionSourceOption::Innate);
		if (!TestTrue(TEXT("Rally completed"), Result.IsSet())) return;
		TestTrue(FString::Printf(TEXT("it ran (%s)"), *Result->ErrorMessage), Result->bSuccess);
		TestTrue(TEXT("the action isn't Fire"), Result->AttackElement != ESpellElement::Fire);
		TestFalse(TEXT("its VFX isn't tinted"), EffectTint::ShouldSetSoftTofuColour(Result->AttackElement));
	});

	It("leaves it untinted with no infusion on", [this]()
	{
		if (!TestNotNull(TEXT("Executor"), Executor) || !TestNotNull(TEXT("Hero"), Hero)) return;
		const TOptional<FActionResult> Result = UseRally(EInfusionSourceOption::None);
		if (!TestTrue(TEXT("Rally completed"), Result.IsSet())) return;
		TestTrue(TEXT("it ran"), Result->bSuccess);
		TestFalse(TEXT("its VFX isn't tinted"), EffectTint::ShouldSetSoftTofuColour(Result->AttackElement));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
