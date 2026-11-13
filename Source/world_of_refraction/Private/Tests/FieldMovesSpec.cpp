// FieldMovesSpec.cpp
// B-214c (Design/FieldAbilities.md): the ground and air moves. Glide (in the air only, paid once, slow fall capped, off
// on landing), high jump and the one double jump per airtime, air dash (in the air, once per airtime; Lightning blinks),
// and the wall-run rule (Enhanced Sprint, airborne, sprint speed, a wall, time left; once per airtime).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/ECharacterClass.h"
#include "Field/FieldMovementComponent.h"
#include "Field/FieldSkillTypes.h"
#include "Field/FieldSkillUnlockSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace FieldMovesSpecValues
{
	constexpr int32 START_EP = 10;
	constexpr float SPRINT_SPEED = 1000.0f;
	constexpr float FALL_SPEED = 1500.0f;
	constexpr float TOLERANCE = 0.01f;
	constexpr float WALL_TIME_LEFT = 0.0f;
}

BEGIN_DEFINE_SPEC(FFieldMovesSpec, "WoR.Field.Moves",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACharacter *Pawn = nullptr;
	UCharacterDataComponent *CharComp = nullptr;
	UFieldMovementComponent *Field = nullptr;
	UFieldSkillUnlockSubsystem *Unlocks = nullptr;

	/** A Caster of Element on the field with START_EP, Type unlocked for Element and picked. */
	bool SpawnWithSkill(ESpellElement Element, EFieldMovementType Type)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Caster;
		Data->InnateElement = Element;
		Pawn = TestWorld->GetWorld()->SpawnActor<ACharacter>();
		if (!Pawn || !Unlocks)
		{
			return false;
		}
		CharComp = NewObject<UCharacterDataComponent>(Pawn);
		CharComp->CharacterData = Data;
		Pawn->AddInstanceComponent(CharComp);
		CharComp->RegisterComponent();
		CharComp->MaxEP = FieldMovesSpecValues::START_EP;
		CharComp->CurrentEP = FieldMovesSpecValues::START_EP;
		Field = FWoRTestWorld::AddComponent<UFieldMovementComponent>(Pawn);
		Unlocks->UnlockForDebug(Type, Element);
		return Field && Field->SetActiveFieldSkill(EFieldSkillSource::Innate, Type) == EFieldSkillRefusal::None;
	}

	UCharacterMovementComponent *Movement() const { return Pawn->GetCharacterMovement(); }

	void SetInAir(bool bInAir) const
	{
		Movement()->SetMovementMode(bInAir ? MOVE_Falling : MOVE_Walking);
	}
END_DEFINE_SPEC(FFieldMovesSpec)

void FFieldMovesSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Unlocks = TestWorld->GetSubsystem<UFieldSkillUnlockSubsystem>();
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
		It("start a glide and an air dash only in the air, and allow one double jump per airtime", [this]()
		{
			TestTrue(TEXT("glide on the ground"), FieldSkillRules::CheckGlideStart(false, 5) == EFieldSkillRefusal::NeedsAir);
			TestTrue(TEXT("glide with no EP"), FieldSkillRules::CheckGlideStart(true, 0) == EFieldSkillRefusal::NotEnoughEP);
			TestTrue(TEXT("jump from the ground, double already used"), FieldSkillRules::CheckJump(true, true, 5) == EFieldSkillRefusal::None);
			TestTrue(TEXT("second double jump"), FieldSkillRules::CheckJump(false, true, 5) == EFieldSkillRefusal::AlreadyUsedInAir);
			TestTrue(TEXT("air dash on the ground"), FieldSkillRules::CheckAirDash(false, false, 5) == EFieldSkillRefusal::NeedsAir);
			TestTrue(TEXT("second air dash"), FieldSkillRules::CheckAirDash(true, true, 5) == EFieldSkillRefusal::AlreadyUsedInAir);
			TestTrue(TEXT("Lightning blinks"), FieldSkillRules::IsBlinkAirDash(ESpellElement::Lightning));
			TestFalse(TEXT("Wind doesn't"), FieldSkillRules::IsBlinkAirDash(ESpellElement::Wind));
		});

		It("wall-run only with Enhanced Sprint, in the air, at sprint speed, by a wall, within its time", [this]()
		{
			const float Fast = FieldMovesSpecValues::SPRINT_SPEED;
			TestTrue(TEXT("all met"), FieldSkillRules::CanWallRun(true, true, Fast, true, FieldMovesSpecValues::WALL_TIME_LEFT));
			TestFalse(TEXT("no Enhanced Sprint"), FieldSkillRules::CanWallRun(false, true, Fast, true, 0.0f));
			TestFalse(TEXT("on the ground"), FieldSkillRules::CanWallRun(true, false, Fast, true, 0.0f));
			TestFalse(TEXT("too slow"), FieldSkillRules::CanWallRun(true, true, FieldSkillRules::ENHANCED_SPRINT_MIN_MOVING_SPEED, true, 0.0f));
			TestFalse(TEXT("no wall"), FieldSkillRules::CanWallRun(true, true, Fast, false, 0.0f));
			TestFalse(TEXT("out of time"), FieldSkillRules::CanWallRun(true, true, Fast, true, FieldSkillRules::WALL_RUN_MAX_SECONDS));
		});
	});

	Describe("the component", [this]()
	{
		It("glides in the air only, pays once, caps the fall, and stops on landing", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnWithSkill(ESpellElement::Wind, EFieldMovementType::Glide)))
			{
				return;
			}
			const float BaseGravity = Movement()->GravityScale;
			SetInAir(false);
			TestTrue(TEXT("not on the ground"), Field->ActivateFieldSkill() == EFieldSkillRefusal::NeedsAir);
			SetInAir(true);
			Movement()->Velocity = FVector(0.0f, 0.0f, -FieldMovesSpecValues::FALL_SPEED);
			TestTrue(TEXT("glide"), Field->ActivateFieldSkill() == EFieldSkillRefusal::None && Field->IsGliding());
			TestEqual(TEXT("paid once"), CharComp->CurrentEP, FieldMovesSpecValues::START_EP - FieldSkillRules::GLIDE_EP_COST);
			TestEqual(TEXT("slow gravity"), Movement()->GravityScale, FieldSkillRules::GLIDE_GRAVITY_SCALE, FieldMovesSpecValues::TOLERANCE);
			TestEqual(TEXT("fall capped"), Movement()->Velocity.Z, -FieldSkillRules::GLIDE_MAX_FALL_SPEED, FieldMovesSpecValues::TOLERANCE);
			Field->NotifyLanded();
			TestFalse(TEXT("off on landing"), Field->IsGliding());
			TestEqual(TEXT("gravity back"), Movement()->GravityScale, BaseGravity, FieldMovesSpecValues::TOLERANCE);
		});

		It("jumps high from the ground and once more in the air until landing", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnWithSkill(ESpellElement::Fire, EFieldMovementType::HighJump)))
			{
				return;
			}
			SetInAir(false);
			TestTrue(TEXT("high jump"), Field->ActivateFieldSkill() == EFieldSkillRefusal::None);
			TestEqual(TEXT("launched up"), Movement()->PendingLaunchVelocity.Z, FieldSkillRules::HIGH_JUMP_SPEED, FieldMovesSpecValues::TOLERANCE);
			SetInAir(true);
			TestTrue(TEXT("double jump"), Field->ActivateFieldSkill() == EFieldSkillRefusal::None);
			TestEqual(TEXT("double jump speed"), Movement()->PendingLaunchVelocity.Z, FieldSkillRules::DOUBLE_JUMP_SPEED, FieldMovesSpecValues::TOLERANCE);
			TestTrue(TEXT("no third"), Field->ActivateFieldSkill() == EFieldSkillRefusal::AlreadyUsedInAir);
			TestEqual(TEXT("two jumps paid"), CharComp->CurrentEP, FieldMovesSpecValues::START_EP - 2 * FieldSkillRules::HIGH_JUMP_EP_COST);
			Field->NotifyLanded();
			TestTrue(TEXT("the double jump is back"), Field->ActivateFieldSkill() == EFieldSkillRefusal::None);
		});

		It("air dashes once per airtime along the aim", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnWithSkill(ESpellElement::Wind, EFieldMovementType::AirDash)))
			{
				return;
			}
			SetInAir(false);
			TestTrue(TEXT("not on the ground"), Field->ActivateFieldSkill() == EFieldSkillRefusal::NeedsAir);
			SetInAir(true);
			TestTrue(TEXT("air dash"), Field->ActivateFieldSkill() == EFieldSkillRefusal::None);
			TestEqual(TEXT("dash speed"), Movement()->PendingLaunchVelocity.Size(), FieldSkillRules::AIR_DASH_SPEED, FieldMovesSpecValues::TOLERANCE);
			TestTrue(TEXT("once per airtime"), Field->ActivateFieldSkill() == EFieldSkillRefusal::AlreadyUsedInAir);
			TestEqual(TEXT("paid"), CharComp->CurrentEP, FieldMovesSpecValues::START_EP - FieldSkillRules::AIR_DASH_EP_COST);
		});

		It("air dashes Lightning as a chain of blinks", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnWithSkill(ESpellElement::Lightning, EFieldMovementType::AirDash)))
			{
				return;
			}
			SetInAir(true);
			const FVector Start = Pawn->GetActorLocation();
			TestTrue(TEXT("air dash"), Field->ActivateFieldSkill() == EFieldSkillRefusal::None);
			const float Expected = FieldSkillRules::AIR_DASH_BLINK_COUNT * FieldSkillRules::AIR_DASH_BLINK_DISTANCE;
			TestEqual(TEXT("moved the blinks' distance (empty world)"), FVector::Dist(Start, Pawn->GetActorLocation()), Expected, 1.0f);
		});

		It("holds a wall-run during Enhanced Sprint and ends it off the wall, once per airtime", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnWithSkill(ESpellElement::Lightning, EFieldMovementType::EnhancedSprint)))
			{
				return;
			}
			SetInAir(true);
			Movement()->Velocity = FVector(FieldMovesSpecValues::SPRINT_SPEED, 0.0f, -FieldMovesSpecValues::FALL_SPEED);
			TestTrue(TEXT("Enhanced Sprint on"), Field->ActivateFieldSkill() == EFieldSkillRefusal::None);
			Movement()->Velocity = FVector(FieldMovesSpecValues::SPRINT_SPEED, 0.0f, -FieldMovesSpecValues::FALL_SPEED);
			Field->ApplyWallRunStep(true, FVector(0.0f, 1.0f, 0.0f));
			TestTrue(TEXT("wall-running"), Field->IsWallRunning());
			TestEqual(TEXT("height held"), Movement()->Velocity.Z, 0.0f, FieldMovesSpecValues::TOLERANCE);
			TestEqual(TEXT("no gravity"), Movement()->GravityScale, FieldSkillRules::WALL_RUN_GRAVITY_SCALE, FieldMovesSpecValues::TOLERANCE);
			Field->ApplyWallRunStep(false, FVector::ZeroVector);
			TestFalse(TEXT("off the wall"), Field->IsWallRunning());
			Field->ApplyWallRunStep(true, FVector(0.0f, 1.0f, 0.0f));
			TestFalse(TEXT("not again before landing"), Field->IsWallRunning());
			Field->NotifyLanded();
			SetInAir(true);
			Field->ApplyWallRunStep(true, FVector(0.0f, 1.0f, 0.0f));
			TestTrue(TEXT("again after landing"), Field->IsWallRunning());
		});
	});
}

#endif
