// FieldFlightSpec.cpp
// B-214d (FieldAbilities choice 4): flight. Refused in a no-fly zone and without a second's EP (each liftable), paid up
// front then each second, landing at 0 EP, the flying mode while on, the ceiling above the take-off ground, and each
// limit lifted by FFieldFlightLimits.

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

namespace FieldFlightSpecValues
{
	constexpr int32 START_EP = 5; // two seconds and a bit
	constexpr float ABOVE_CEILING = 500.0f;
	constexpr float TOLERANCE = 1.0f;
	constexpr int32 PLENTY_OF_EP = 10;
	constexpr float EXTRA_CEILING = 300.0f;
	constexpr double TAKEOFF_GROUND_Z = 100.0;
	constexpr double NO_CEILING_ABOVE = 1.0e30;
}

BEGIN_DEFINE_SPEC(FFieldFlightSpec, "WoR.Field.Flight",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACharacter *Pawn = nullptr;
	UCharacterDataComponent *CharComp = nullptr;
	UFieldMovementComponent *Field = nullptr;

	/** A Wind Caster with START_EP and Flight unlocked and picked. */
	bool SpawnFlyer()
	{
		UFieldSkillUnlockSubsystem *Unlocks = TestWorld->GetSubsystem<UFieldSkillUnlockSubsystem>();
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Caster;
		Data->InnateElement = ESpellElement::Wind;
		Pawn = TestWorld->GetWorld()->SpawnActor<ACharacter>();
		if (!Pawn || !Unlocks)
		{
			return false;
		}
		CharComp = NewObject<UCharacterDataComponent>(Pawn);
		CharComp->CharacterData = Data;
		Pawn->AddInstanceComponent(CharComp);
		CharComp->RegisterComponent();
		CharComp->MaxEP = FieldFlightSpecValues::START_EP;
		CharComp->CurrentEP = FieldFlightSpecValues::START_EP;
		Field = FWoRTestWorld::AddComponent<UFieldMovementComponent>(Pawn);
		Unlocks->UnlockForDebug(EFieldMovementType::Flight, ESpellElement::Wind);
		return Field && Field->SetActiveFieldSkill(EFieldSkillSource::Innate, EFieldMovementType::Flight) == EFieldSkillRefusal::None;
	}
END_DEFINE_SPEC(FFieldFlightSpec)

void FFieldFlightSpec::Define()
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
		It("refuse a no-fly zone and an empty EP bar unless lifted, and lift the ceiling", [this]()
		{
			const FFieldFlightLimits None;
			FFieldFlightLimits Lifted;
			Lifted.bIgnoreNoFly = true;
			Lifted.bUnlimitedEP = true;
			Lifted.bNoCeiling = true;
			TestTrue(TEXT("no-fly"), FieldSkillRules::CheckFlightStart(true, FieldFlightSpecValues::PLENTY_OF_EP, None) == EFieldSkillRefusal::NoFlyZone);
			TestTrue(TEXT("no EP"), FieldSkillRules::CheckFlightStart(false, 0, None) == EFieldSkillRefusal::NotEnoughEP);
			TestTrue(TEXT("lifted"), FieldSkillRules::CheckFlightStart(true, 0, Lifted) == EFieldSkillRefusal::None);
			TestEqual(TEXT("ceiling above the ground"), FieldSkillRules::GetFlightCeilingZ(FieldFlightSpecValues::TAKEOFF_GROUND_Z, None), FieldFlightSpecValues::TAKEOFF_GROUND_Z + FieldSkillRules::FLIGHT_MAX_HEIGHT);
			FFieldFlightLimits Raised;
			Raised.ExtraCeiling = FieldFlightSpecValues::EXTRA_CEILING;
			TestEqual(TEXT("raised"), FieldSkillRules::GetFlightCeilingZ(0.0, Raised), static_cast<double>(FieldSkillRules::FLIGHT_MAX_HEIGHT + FieldFlightSpecValues::EXTRA_CEILING));
			TestTrue(TEXT("no ceiling"), FieldSkillRules::GetFlightCeilingZ(0.0, Lifted) > FieldFlightSpecValues::NO_CEILING_ABOVE);
		});
	});

	Describe("the component", [this]()
	{
		It("flies, pays each second, and lands at 0 EP", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnFlyer()))
			{
				return;
			}
			TestTrue(TEXT("flying"), Field->ActivateFieldSkill() == EFieldSkillRefusal::None && Field->IsFlying());
			TestTrue(TEXT("flying mode"), Pawn->GetCharacterMovement()->MovementMode == MOVE_Flying);
			TestEqual(TEXT("lifted off"), static_cast<float>(Pawn->GetCharacterMovement()->PendingLaunchVelocity.Z), FieldSkillRules::FLIGHT_TAKEOFF_LIFT_SPEED, FieldFlightSpecValues::TOLERANCE);
			TestEqual(TEXT("first second paid"), CharComp->CurrentEP, FieldFlightSpecValues::START_EP - FieldSkillRules::FLIGHT_EP_PER_SECOND);
			Field->DrainFlight();
			TestTrue(TEXT("still flying"), Field->IsFlying());
			Field->DrainFlight();
			TestFalse(TEXT("landed: not enough EP for another second"), Field->IsFlying());
			TestTrue(TEXT("falling"), Pawn->GetCharacterMovement()->MovementMode == MOVE_Falling);
		});

		It("costs nothing with unlimited flight", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnFlyer()))
			{
				return;
			}
			FFieldFlightLimits Unlimited;
			Unlimited.bUnlimitedEP = true;
			Field->SetFlightLimits(Unlimited);
			Field->ActivateFieldSkill();
			Field->DrainFlight();
			TestEqual(TEXT("no EP spent"), CharComp->CurrentEP, FieldFlightSpecValues::START_EP);
			TestTrue(TEXT("still flying"), Field->IsFlying());
		});

		It("holds the flyer under the ceiling unless it is lifted", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnFlyer()))
			{
				return;
			}
			const FVector Start = Pawn->GetActorLocation(); // an empty world: the ceiling counts from here
			Field->ActivateFieldSkill();
			const double Ceiling = Start.Z + FieldSkillRules::FLIGHT_MAX_HEIGHT;
			Pawn->SetActorLocation(FVector(Start.X, Start.Y, Ceiling + FieldFlightSpecValues::ABOVE_CEILING));
			Field->ApplyFlightStep();
			TestEqual(TEXT("brought to the ceiling"), static_cast<float>(Pawn->GetActorLocation().Z), static_cast<float>(Ceiling), FieldFlightSpecValues::TOLERANCE);
			FFieldFlightLimits NoCeiling;
			NoCeiling.bNoCeiling = true;
			NoCeiling.bUnlimitedEP = true;
			Field->SetFlightLimits(NoCeiling);
			Pawn->SetActorLocation(FVector(Start.X, Start.Y, Ceiling + FieldFlightSpecValues::ABOVE_CEILING));
			Field->ApplyFlightStep();
			TestTrue(TEXT("above it when lifted"), Pawn->GetActorLocation().Z > Ceiling + FieldFlightSpecValues::TOLERANCE);
		});

		It("toggles off on a second use and when another skill is picked", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnFlyer()))
			{
				return;
			}
			Field->ActivateFieldSkill();
			Field->ActivateFieldSkill();
			TestFalse(TEXT("off"), Field->IsFlying());
			Field->ActivateFieldSkill();
			Field->SetActiveFieldSkill(EFieldSkillSource::Innate, EFieldMovementType::Dash);
			TestFalse(TEXT("off on a new pick"), Field->IsFlying());
		});

		It("ends when something else takes the movement out of flying", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnFlyer()))
			{
				return;
			}
			Field->ActivateFieldSkill();
			Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			Field->ApplyFlightStep();
			TestFalse(TEXT("off"), Field->IsFlying());
		});
	});
}

#endif
