// FieldSkillSpec.cpp
// B-214a (D-099, Design/FieldAbilities.md): the field skill core. Pick rules (Dash for everyone; Enhanced Sprint needs
// an elemental source; the other types wait for their slice), Enhanced Sprint's EP drain stopping at 0 and its speed
// by element (Lightning fastest, Wind second), the sprint speed it multiplies, and the dash cooldown.
// B-214b: unlocks (rank 3 / 7 by type, every listed aura, Teleport Void only, account-wide, ranks never fall, saved).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/ECharacterClass.h"
#include "Field/FieldMovementComponent.h"
#include "Field/FieldSkillTypes.h"
#include "Field/FieldSkillUnlockSubsystem.h"
#include "Skills/Definitions/SpellData.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Hub/HubMovementLibrary.h"

namespace
{
	constexpr int32 FIELD_SPEC_START_EP = 3;
	constexpr float FIELD_SPEC_SPEED_TOLERANCE = 0.01f;
	constexpr float FIELD_SPEC_SPRINT_SPEED = 1000.0f;
	constexpr float FIELD_SPEC_WALK_SPEED = 500.0f;
}

BEGIN_DEFINE_SPEC(FFieldSkillSpec, "WoR.Field.Skills",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACharacter *Pawn = nullptr;
	UCharacterDataComponent *CharComp = nullptr;
	UFieldMovementComponent *Field = nullptr;
	UFieldSkillUnlockSubsystem *Unlocks = nullptr;

	/** A transient aura spell of Element. */
	static USpellData *NewAuraSpell(EFieldAura Aura, ESpellElement Element)
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->FieldAura = Aura;
		Spell->Element = Element;
		return Spell;
	}

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

	/** The pawn's ground speed: a sprint, or standing still. The spec never ticks, so it holds. */
	void SetSprinting(bool bSprinting) const
	{
		Pawn->GetCharacterMovement()->Velocity = bSprinting ? FVector(FIELD_SPEC_SPRINT_SPEED, 0.0f, 0.0f) : FVector::ZeroVector;
	}
END_DEFINE_SPEC(FFieldSkillSpec)

void FFieldSkillSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Unlocks = TestWorld->GetSubsystem<UFieldSkillUnlockSubsystem>();
		TestNotNull(TEXT("unlock subsystem"), Unlocks);
	});

	AfterEach([this]()
	{
		Pawn = nullptr;
		CharComp = nullptr;
		Field = nullptr;
		Unlocks = nullptr;
		TestWorld.Reset();
	});

	Describe("the rules", [this]()
	{
		It("let anyone pick the dash, Enhanced Sprint only with an elemental source, and nothing unbuilt", [this]()
		{
			TestTrue(TEXT("dash with no element, never unlocked"), FieldSkillRules::CheckPick(EFieldMovementType::Dash, ESpellElement::None, false) == EFieldSkillRefusal::None);
			TestTrue(TEXT("Enhanced Sprint, Generic"), FieldSkillRules::CheckPick(EFieldMovementType::EnhancedSprint, ESpellElement::Generic, true) == EFieldSkillRefusal::NeedsElementalSource);
			TestTrue(TEXT("Enhanced Sprint, None"), FieldSkillRules::CheckPick(EFieldMovementType::EnhancedSprint, ESpellElement::None, true) == EFieldSkillRefusal::NeedsElementalSource);
			TestTrue(TEXT("Enhanced Sprint, Fire, unlocked"), FieldSkillRules::CheckPick(EFieldMovementType::EnhancedSprint, ESpellElement::Fire, true) == EFieldSkillRefusal::None);
			TestTrue(TEXT("Enhanced Sprint, Fire, locked"), FieldSkillRules::CheckPick(EFieldMovementType::EnhancedSprint, ESpellElement::Fire, false) == EFieldSkillRefusal::NotUnlocked);
			TestTrue(TEXT("Flight waits for its slice"), FieldSkillRules::CheckPick(EFieldMovementType::Flight, ESpellElement::Wind, true) == EFieldSkillRefusal::NotBuiltYet);
			TestTrue(TEXT("no EP, no start"), FieldSkillRules::CheckEnhancedSprintStart(ESpellElement::Fire, 0, true) == EFieldSkillRefusal::NotEnoughEP);
			TestTrue(TEXT("locked, no start"), FieldSkillRules::CheckEnhancedSprintStart(ESpellElement::Fire, FIELD_SPEC_START_EP, false) == EFieldSkillRefusal::NotUnlocked);
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

	Describe("the unlocks (B-214b)", [this]()
	{
		It("need rank 3 for the simple types and rank 7 for the advanced, with every listed aura", [this]()
		{
			TestEqual(TEXT("D tier is rank 3"), FieldSkillRules::TierToRank(EItemTier::D_Tier), FieldSkillRules::SIMPLE_UNLOCK_RANK);
			TestEqual(TEXT("S tier is rank 7"), FieldSkillRules::TierToRank(EItemTier::S_Tier), FieldSkillRules::ADVANCED_UNLOCK_RANK);
			TestEqual(TEXT("Enhanced Sprint is simple"), FieldSkillRules::GetUnlockRank(EFieldMovementType::EnhancedSprint), FieldSkillRules::SIMPLE_UNLOCK_RANK);
			TestEqual(TEXT("Glide is simple"), FieldSkillRules::GetUnlockRank(EFieldMovementType::Glide), FieldSkillRules::SIMPLE_UNLOCK_RANK);
			TestEqual(TEXT("Flight is advanced"), FieldSkillRules::GetUnlockRank(EFieldMovementType::Flight), FieldSkillRules::ADVANCED_UNLOCK_RANK);
			TestEqual(TEXT("Regroup needs all three auras"), FieldSkillRules::GetUnlockAuras(EFieldMovementType::Regroup).Num(), 3);

			Unlocks->RecordSpellTier(NewAuraSpell(EFieldAura::Enhancement, ESpellElement::Fire), EItemTier::E_Tier);
			TestFalse(TEXT("rank 2 isn't enough"), Unlocks->IsUnlocked(EFieldMovementType::EnhancedSprint, ESpellElement::Fire));
			TestTrue(TEXT("rank 3 rises"), Unlocks->RecordSpellTier(NewAuraSpell(EFieldAura::Enhancement, ESpellElement::Fire), EItemTier::D_Tier));
			TestTrue(TEXT("rank 3 unlocks Enhanced Sprint for Fire"), Unlocks->IsUnlocked(EFieldMovementType::EnhancedSprint, ESpellElement::Fire));
			TestTrue(TEXT("and High jump (same aura and rank)"), Unlocks->IsUnlocked(EFieldMovementType::HighJump, ESpellElement::Fire));
			TestFalse(TEXT("not for Water"), Unlocks->IsUnlocked(EFieldMovementType::EnhancedSprint, ESpellElement::Water));

			Unlocks->RecordSpellTier(NewAuraSpell(EFieldAura::Manipulation, ESpellElement::Fire), EItemTier::A_Tier);
			TestTrue(TEXT("rank 6 Manipulation gives Glide"), Unlocks->IsUnlocked(EFieldMovementType::Glide, ESpellElement::Fire));
			TestFalse(TEXT("but not Flight"), Unlocks->IsUnlocked(EFieldMovementType::Flight, ESpellElement::Fire));
			Unlocks->RecordSpellTier(NewAuraSpell(EFieldAura::Manipulation, ESpellElement::Fire), EItemTier::S_Tier);
			TestTrue(TEXT("rank 7 gives Flight"), Unlocks->IsUnlocked(EFieldMovementType::Flight, ESpellElement::Fire));
			TestFalse(TEXT("Phase step also needs Elemental at 7"), Unlocks->IsUnlocked(EFieldMovementType::PhaseStep, ESpellElement::Fire));
			Unlocks->RecordSpellTier(NewAuraSpell(EFieldAura::Elemental, ESpellElement::Fire), EItemTier::S_Tier);
			TestTrue(TEXT("both at 7: Phase step"), Unlocks->IsUnlocked(EFieldMovementType::PhaseStep, ESpellElement::Fire));
			TestFalse(TEXT("Regroup still needs Enhancement at 7"), Unlocks->IsUnlocked(EFieldMovementType::Regroup, ESpellElement::Fire));
		});

		It("let only Void unlock Teleport, and never a Generic or a non-aura spell", [this]()
		{
			for (const ESpellElement Element : {ESpellElement::Fire, ESpellElement::Void})
			{
				Unlocks->RecordSpellTier(NewAuraSpell(EFieldAura::Elemental, Element), EItemTier::S_Tier);
				Unlocks->RecordSpellTier(NewAuraSpell(EFieldAura::Manipulation, Element), EItemTier::S_Tier);
			}
			TestTrue(TEXT("Void teleports"), Unlocks->IsUnlocked(EFieldMovementType::Teleport, ESpellElement::Void));
			TestFalse(TEXT("Fire never teleports"), Unlocks->IsUnlocked(EFieldMovementType::Teleport, ESpellElement::Fire));
			TestFalse(TEXT("debug unlock refused too"), Unlocks->UnlockForDebug(EFieldMovementType::Teleport, ESpellElement::Fire));
			TestFalse(TEXT("a Generic aura spell records nothing"), Unlocks->RecordSpellTier(NewAuraSpell(EFieldAura::Enhancement, ESpellElement::Generic), EItemTier::S_Tier));
			TestFalse(TEXT("a non-aura spell records nothing"), Unlocks->RecordSpellTier(NewAuraSpell(EFieldAura::None, ESpellElement::Wind), EItemTier::S_Tier));
			TestTrue(TEXT("the dash needs nothing"), Unlocks->IsUnlocked(EFieldMovementType::Dash, ESpellElement::None));
		});

		It("keep a rank once reached and round-trip through the save", [this]()
		{
			USpellData *Spell = NewAuraSpell(EFieldAura::Enhancement, ESpellElement::Lightning);
			Unlocks->RecordSpellTier(Spell, EItemTier::C_Tier);
			TestFalse(TEXT("a lower tier later doesn't lower it"), Unlocks->RecordSpellTier(Spell, EItemTier::F_Tier));
			TestTrue(TEXT("still unlocked"), Unlocks->IsUnlocked(EFieldMovementType::EnhancedSprint, ESpellElement::Lightning));
			const FSavedFieldUnlocks Saved = Unlocks->Capture();
			Unlocks->Restore(FSavedFieldUnlocks());
			TestFalse(TEXT("cleared"), Unlocks->IsUnlocked(EFieldMovementType::EnhancedSprint, ESpellElement::Lightning));
			Unlocks->Restore(Saved);
			TestEqual(TEXT("rank back"), Unlocks->GetBestRank(EFieldAura::Enhancement, ESpellElement::Lightning), FieldSkillRules::TierToRank(EItemTier::C_Tier));
			TestTrue(TEXT("unlocked again"), Unlocks->IsUnlocked(EFieldMovementType::EnhancedSprint, ESpellElement::Lightning));
		});

		It("let any character with a source of the element pick an unlocked type (account-wide)", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnFieldCharacter(ECharacterClass::Caster, ESpellElement::Water)))
			{
				return;
			}
			TestTrue(TEXT("locked first"), Field->SetActiveFieldSkill(EFieldSkillSource::Innate, EFieldMovementType::EnhancedSprint) == EFieldSkillRefusal::NotUnlocked);
			// Another character ranked a Water Enhancement Aura spell: this character never held it.
			Unlocks->RecordSpellTier(NewAuraSpell(EFieldAura::Enhancement, ESpellElement::Water), EItemTier::D_Tier);
			TestTrue(TEXT("now picked"), Field->SetActiveFieldSkill(EFieldSkillSource::Innate, EFieldMovementType::EnhancedSprint) == EFieldSkillRefusal::None);
			TestTrue(TEXT("and it starts"), Field->ActivateFieldSkill() == EFieldSkillRefusal::None && Field->IsEnhancedSprintActive());
		});
	});

	Describe("the component", [this]()
	{
		BeforeEach([this]()
		{
			for (const ESpellElement Element : {ESpellElement::Lightning, ESpellElement::Fire, ESpellElement::Wind})
			{
				Unlocks->UnlockForDebug(EFieldMovementType::EnhancedSprint, Element);
			}
		});

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
			SetSprinting(true);
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

		It("charges nothing while the character stands or walks, and stays on (Q-214a, Crown)", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnFieldCharacter(ECharacterClass::Caster, ESpellElement::Fire)))
			{
				return;
			}
			TestFalse(TEXT("walking is not a sprint"), FieldSkillRules::IsMovingAtSprintSpeed(FIELD_SPEC_WALK_SPEED));
			TestTrue(TEXT("the slowest sprint is"), FieldSkillRules::IsMovingAtSprintSpeed(UHubMovementLibrary::GetSprintSpeedMin()));
			TestTrue(TEXT("the threshold sits between the walk and the slowest sprint"),
					 FieldSkillRules::ENHANCED_SPRINT_MIN_MOVING_SPEED > FIELD_SPEC_WALK_SPEED &&
						 FieldSkillRules::ENHANCED_SPRINT_MIN_MOVING_SPEED < UHubMovementLibrary::GetSprintSpeedMin());
			SetSprinting(false);
			Field->SetActiveFieldSkill(EFieldSkillSource::Innate, EFieldMovementType::EnhancedSprint);
			TestTrue(TEXT("started standing"), Field->ActivateFieldSkill() == EFieldSkillRefusal::None && Field->IsEnhancedSprintActive());
			Field->DrainEnhancedSprint();
			TestEqual(TEXT("nothing paid standing still"), CharComp->CurrentEP, FIELD_SPEC_START_EP);
			TestTrue(TEXT("still on"), Field->IsEnhancedSprintActive());
			SetSprinting(true);
			Field->DrainEnhancedSprint();
			TestEqual(TEXT("a sprinting second is paid"), CharComp->CurrentEP, FIELD_SPEC_START_EP - FieldSkillRules::ENHANCED_SPRINT_EP_PER_SECOND);
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
