// FieldAttackSpec.cpp
// B-214f (Design/FieldAbilities.md): the field attack. The strike it carries (a real element only; Water held = Ice; the
// weapon's physical type; the buildup), the source switch, a swing that strikes nothing, and its cooldown. The engagement
// itself (first strike and the buildup in the fight) needs a party and a level change: PIE (docs/PIE/B-214f.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/ECharacterClass.h"
#include "Field/FieldMovementComponent.h"
#include "Field/FieldSkillTypes.h"
#include "GameFramework/Character.h"

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
		});
	});

	Describe("the component", [this]()
	{
		It("swings from the weapon by default and switches to the innate element", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnCaster(ESpellElement::Water)))
			{
				return;
			}
			TestTrue(TEXT("the weapon by default"), Field->GetFieldAttackSource() == EFieldSkillSource::Weapon);
			TestTrue(TEXT("no weapon: no element, no physical type"),
					 Field->GetFieldStrike(false).Element == ESpellElement::None && Field->GetFieldAttackPhysicalType() == EPhysicalDamageType::None);
			Field->SwitchFieldAttackSource(); // -> Ring
			Field->SwitchFieldAttackSource(); // -> Innate
			TestTrue(TEXT("innate now"), Field->GetFieldAttackSource() == EFieldSkillSource::Innate);
			const FFieldStrike Held = Field->GetFieldStrike(true);
			TestTrue(TEXT("held innate Water is Ice"), Held.Element == ESpellElement::Water && Held.bIce);
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
