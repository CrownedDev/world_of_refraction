// FieldSkillSpec.cpp
// B-214a (D-099, Design/FieldAbilities.md): the field skill core. Pick rules (Dash for everyone; Enhanced Sprint needs
// an elemental source; the other types wait for their slice), Enhanced Sprint's EP drain stopping at 0 and its speed
// by element (Lightning fastest, Wind second), the sprint speed it multiplies, and the dash cooldown.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/ECharacterClass.h"
#include "Field/FieldMovementComponent.h"
#include "Field/FieldSkillTypes.h"
#include "GameFramework/Character.h"
#include "Hub/HubMovementLibrary.h"

namespace
{
	constexpr int32 FIELD_SPEC_START_EP = 3;
	constexpr float FIELD_SPEC_SPEED_TOLERANCE = 0.01f;
}

BEGIN_DEFINE_SPEC(FFieldSkillSpec, "WoR.Field.Skills",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACharacter *Pawn = nullptr;
	UCharacterDataComponent *CharComp = nullptr;
	UFieldMovementComponent *Field = nullptr;

	/** A field character of Class (a Caster's innate element is Element) with FIELD_SPEC_START_EP EP. */
	bool SpawnFieldCharacter(ECharacterClass Class, ESpellElement Element)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = Class;
		Data->InnateElement = Element;
		Pawn = TestWorld->GetWorld()->SpawnActor<ACharacter>();
		if (!Pawn)
		{
			return false;
		}
		CharComp = NewObject<UCharacterDataComponent>(Pawn);
		CharComp->CharacterData = Data;
		Pawn->AddInstanceComponent(CharComp);
		CharComp->RegisterComponent();
		CharComp->MaxEP = FIELD_SPEC_START_EP;
		CharComp->CurrentEP = FIELD_SPEC_START_EP;
		Field = FWoRTestWorld::AddComponent<UFieldMovementComponent>(Pawn);
		return Field != nullptr;
	}
END_DEFINE_SPEC(FFieldSkillSpec)

void FFieldSkillSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		Pawn = nullptr;
		CharComp = nullptr;
		Field = nullptr;
		TestWorld.Reset();
	});

	Describe("the rules", [this]()
	{
		It("let anyone pick the dash, Enhanced Sprint only with an elemental source, and nothing unbuilt", [this]()
		{
			TestTrue(TEXT("dash with no element"), FieldSkillRules::CheckPick(EFieldMovementType::Dash, ESpellElement::None) == EFieldSkillRefusal::None);
			TestTrue(TEXT("Enhanced Sprint, Generic"), FieldSkillRules::CheckPick(EFieldMovementType::EnhancedSprint, ESpellElement::Generic) == EFieldSkillRefusal::NeedsElementalSource);
			TestTrue(TEXT("Enhanced Sprint, None"), FieldSkillRules::CheckPick(EFieldMovementType::EnhancedSprint, ESpellElement::None) == EFieldSkillRefusal::NeedsElementalSource);
			TestTrue(TEXT("Enhanced Sprint, Fire"), FieldSkillRules::CheckPick(EFieldMovementType::EnhancedSprint, ESpellElement::Fire) == EFieldSkillRefusal::None);
			TestTrue(TEXT("Glide waits for its slice"), FieldSkillRules::CheckPick(EFieldMovementType::Glide, ESpellElement::Wind) == EFieldSkillRefusal::NotBuiltYet);
			TestTrue(TEXT("no EP, no start"), FieldSkillRules::CheckEnhancedSprintStart(ESpellElement::Fire, 0) == EFieldSkillRefusal::NotEnoughEP);
		});

		It("make Lightning's Enhanced Sprint the fastest, Wind's second, and give a non-element no boost", [this]()
		{
			const float Lightning = FieldSkillRules::GetEnhancedSprintMultiplier(ESpellElement::Lightning);
			const float Wind = FieldSkillRules::GetEnhancedSprintMultiplier(ESpellElement::Wind);
			const float Fire = FieldSkillRules::GetEnhancedSprintMultiplier(ESpellElement::Fire);
			TestTrue(TEXT("Lightning > Wind > Fire > 1"), Lightning > Wind && Wind > Fire && Fire > FieldSkillRules::NO_SPRINT_BOOST);
			TestEqual(TEXT("Generic"), FieldSkillRules::GetEnhancedSprintMultiplier(ESpellElement::Generic), FieldSkillRules::NO_SPRINT_BOOST);
		});
	});

	Describe("the component", [this]()
	{
		It("refuses Enhanced Sprint to a Generic with no elemental source and keeps the dash", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnFieldCharacter(ECharacterClass::Generic, ESpellElement::Fire)))
			{
				return;
			}
			TestTrue(TEXT("Generic's innate source has no element"), Field->GetSourceElement(EFieldSkillSource::Innate) == ESpellElement::Generic);
			TestTrue(TEXT("refused"), Field->SetActiveFieldSkill(EFieldSkillSource::Innate, EFieldMovementType::EnhancedSprint) == EFieldSkillRefusal::NeedsElementalSource);
			TestTrue(TEXT("weapon source with no weapon refused"), Field->SetActiveFieldSkill(EFieldSkillSource::Weapon, EFieldMovementType::EnhancedSprint) == EFieldSkillRefusal::NeedsElementalSource);
			TestTrue(TEXT("still the dash"), Field->GetActiveFieldSkill() == FFieldSkillChoice());
		});

		It("drains EP each second of Enhanced Sprint and stops it at 0", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnFieldCharacter(ECharacterClass::Caster, ESpellElement::Lightning)))
			{
				return;
			}
			TestTrue(TEXT("picked"), Field->SetActiveFieldSkill(EFieldSkillSource::Innate, EFieldMovementType::EnhancedSprint) == EFieldSkillRefusal::None);
			TestTrue(TEXT("started"), Field->ActivateFieldSkill() == EFieldSkillRefusal::None && Field->IsEnhancedSprintActive());
			TestEqual(TEXT("the first second is paid up front"), CharComp->CurrentEP, FIELD_SPEC_START_EP - FieldSkillRules::ENHANCED_SPRINT_EP_PER_SECOND);
			TestEqual(TEXT("Lightning's speed"), Field->GetSprintSpeedMultiplier(), FieldSkillRules::ENHANCED_SPRINT_LIGHTNING_MULTIPLIER);
			Field->DrainEnhancedSprint();
			TestTrue(TEXT("still on with EP left"), Field->IsEnhancedSprintActive());
			Field->DrainEnhancedSprint();
			TestEqual(TEXT("EP at 0"), CharComp->CurrentEP, 0);
			TestFalse(TEXT("off at 0 EP"), Field->IsEnhancedSprintActive());
			TestEqual(TEXT("normal sprint again"), Field->GetSprintSpeedMultiplier(), FieldSkillRules::NO_SPRINT_BOOST);
			TestTrue(TEXT("no EP, no restart"), Field->ActivateFieldSkill() == EFieldSkillRefusal::NotEnoughEP);
		});

		It("toggles Enhanced Sprint off on a second use and speeds up the sprint while on", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnFieldCharacter(ECharacterClass::Caster, ESpellElement::Wind)))
			{
				return;
			}
			const float NormalSprint = UHubMovementLibrary::ComputeHubSprintSpeed(Pawn);
			Field->SetActiveFieldSkill(EFieldSkillSource::Innate, EFieldMovementType::EnhancedSprint);
			Field->ActivateFieldSkill();
			TestEqual(TEXT("Wind's sprint"), UHubMovementLibrary::ComputeHubSprintSpeed(Pawn),
					  NormalSprint * FieldSkillRules::ENHANCED_SPRINT_WIND_MULTIPLIER, FIELD_SPEC_SPEED_TOLERANCE);
			Field->ActivateFieldSkill();
			TestFalse(TEXT("off"), Field->IsEnhancedSprintActive());
			TestEqual(TEXT("normal sprint"), UHubMovementLibrary::ComputeHubSprintSpeed(Pawn), NormalSprint, FIELD_SPEC_SPEED_TOLERANCE);
		});

		It("stops Enhanced Sprint when another skill is picked", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnFieldCharacter(ECharacterClass::Caster, ESpellElement::Fire)))
			{
				return;
			}
			Field->SetActiveFieldSkill(EFieldSkillSource::Innate, EFieldMovementType::EnhancedSprint);
			Field->ActivateFieldSkill();
			Field->SetActiveFieldSkill(EFieldSkillSource::Innate, EFieldMovementType::Dash);
			TestFalse(TEXT("off"), Field->IsEnhancedSprintActive());
		});

		It("dashes once, then waits out the cooldown", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnFieldCharacter(ECharacterClass::Generic, ESpellElement::Generic)))
			{
				return;
			}
			TestTrue(TEXT("the default skill is the dash"), Field->GetActiveFieldSkill().Type == EFieldMovementType::Dash);
			TestTrue(TEXT("dash"), Field->ActivateFieldSkill() == EFieldSkillRefusal::None);
			TestFalse(TEXT("cooling"), Field->IsDashReady());
			TestTrue(TEXT("on cooldown"), Field->Dash() == EFieldSkillRefusal::OnCooldown);
			TestEqual(TEXT("the dash is free"), CharComp->CurrentEP, FIELD_SPEC_START_EP);
		});

		It("refuses a dash on a pawn with no character movement", [this]()
		{
			AActor *Plain = TestWorld->GetWorld()->SpawnActor<AActor>();
			UFieldMovementComponent *PlainField = FWoRTestWorld::AddComponent<UFieldMovementComponent>(Plain);
			TestTrue(TEXT("no movement"), PlainField->Dash() == EFieldSkillRefusal::NoMovement);
		});
	});
}

#endif
