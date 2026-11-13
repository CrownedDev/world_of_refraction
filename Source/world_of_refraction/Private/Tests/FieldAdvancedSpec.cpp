// FieldAdvancedSpec.cpp
// B-214e (Design/FieldAbilities.md): the advanced moves. Costs and cooldowns, Regroup refused in a fight, Teleport for
// Void only, each move's movement in an empty world (teleport, phase step, mark then recall, super launch, regroup to
// the other party member), its EP paid once it happened and its cooldown.

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

namespace FieldAdvancedSpecValues
{
	constexpr int32 START_EP = 20;
	constexpr int32 PLENTY_OF_EP = 10;
	constexpr float TOLERANCE = 1.0f;
	constexpr float AWAY = 1000.0f;
	constexpr float OTHER_MEMBER_X = 2000.0f;
	constexpr double JUST_USED = 0.0;
	constexpr double LONG_AGO = 100.0;
}

BEGIN_DEFINE_SPEC(FFieldAdvancedSpec, "WoR.Field.Advanced",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACharacter *Pawn = nullptr;
	UCharacterDataComponent *CharComp = nullptr;
	UFieldMovementComponent *Field = nullptr;

	/** A Caster of Element at Location with START_EP and its field movement. */
	ACharacter *SpawnFieldPawn(ESpellElement Element, const FVector &Location, UCharacterDataComponent *&OutCharComp)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Caster;
		Data->InnateElement = Element;
		ACharacter *Spawned = TestWorld->GetWorld()->SpawnActor<ACharacter>(Location, FRotator::ZeroRotator);
		if (!Spawned)
		{
			return nullptr;
		}
		OutCharComp = NewObject<UCharacterDataComponent>(Spawned);
		OutCharComp->CharacterData = Data;
		Spawned->AddInstanceComponent(OutCharComp);
		OutCharComp->RegisterComponent();
		OutCharComp->MaxEP = FieldAdvancedSpecValues::START_EP;
		OutCharComp->CurrentEP = FieldAdvancedSpecValues::START_EP;
		OutCharComp->PartyMemberId = FGuid::NewGuid(); // a party member's pawn (Regroup's target)
		Spawned->GetCharacterMovement()->SetMovementMode(MOVE_Walking); // no controller: else MOVE_None ignores a launch
		FWoRTestWorld::AddComponent<UFieldMovementComponent>(Spawned);
		return Spawned;
	}

	/** The test's pawn: Element, Type unlocked for it and picked. False when the pick is refused. */
	bool SpawnWithSkill(ESpellElement Element, EFieldMovementType Type)
	{
		Pawn = SpawnFieldPawn(Element, FVector::ZeroVector, CharComp);
		Field = Pawn ? Pawn->FindComponentByClass<UFieldMovementComponent>() : nullptr;
		UFieldSkillUnlockSubsystem *Unlocks = TestWorld->GetSubsystem<UFieldSkillUnlockSubsystem>();
		if (!Field || !Unlocks)
		{
			return false;
		}
		Unlocks->UnlockForDebug(Type, Element);
		return Field->SetActiveFieldSkill(EFieldSkillSource::Innate, Type) == EFieldSkillRefusal::None;
	}

	float DistanceFrom(const FVector &Start) const
	{
		return static_cast<float>(FVector::Dist(Start, Pawn->GetActorLocation()));
	}
END_DEFINE_SPEC(FFieldAdvancedSpec)

void FFieldAdvancedSpec::Define()
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
		It("refuse Regroup in a fight, a move on cooldown, and one without its EP", [this]()
		{
			using namespace FieldAdvancedSpecValues;
			TestTrue(TEXT("Regroup in a fight"), FieldSkillRules::CheckAdvancedMove(EFieldMovementType::Regroup, PLENTY_OF_EP, LONG_AGO, true) == EFieldSkillRefusal::InFight);
			TestTrue(TEXT("Teleport ignores the fight flag"), FieldSkillRules::CheckAdvancedMove(EFieldMovementType::Teleport, PLENTY_OF_EP, LONG_AGO, true) == EFieldSkillRefusal::None);
			TestTrue(TEXT("on cooldown"), FieldSkillRules::CheckAdvancedMove(EFieldMovementType::Teleport, PLENTY_OF_EP, JUST_USED, false) == EFieldSkillRefusal::OnCooldown);
			TestTrue(TEXT("no EP"), FieldSkillRules::CheckAdvancedMove(EFieldMovementType::SuperLaunch, 0, LONG_AGO, false) == EFieldSkillRefusal::NotEnoughEP);
			TestEqual(TEXT("phase step costs"), FieldSkillRules::GetAdvancedMoveEPCost(EFieldMovementType::PhaseStep), FieldSkillRules::PHASE_STEP_EP_COST);
			TestTrue(TEXT("every type is built"), FieldSkillRules::IsBuilt(EFieldMovementType::Regroup));
		});
	});

	Describe("the component", [this]()
	{
		It("teleports only for Void, along the aim, then waits out the cooldown", [this]()
		{
			TestFalse(TEXT("Fire can't unlock Teleport"), FieldSkillRules::CanElementUnlock(EFieldMovementType::Teleport, ESpellElement::Fire));
			if (!TestTrue(TEXT("Void picks it"), SpawnWithSkill(ESpellElement::Void, EFieldMovementType::Teleport)))
			{
				return;
			}
			const FVector Start = Pawn->GetActorLocation();
			TestTrue(TEXT("teleported"), Field->ActivateFieldSkill() == EFieldSkillRefusal::None);
			TestEqual(TEXT("its full distance (empty world)"), DistanceFrom(Start), FieldSkillRules::TELEPORT_DISTANCE, FieldAdvancedSpecValues::TOLERANCE);
			TestEqual(TEXT("paid"), CharComp->CurrentEP, FieldAdvancedSpecValues::START_EP - FieldSkillRules::TELEPORT_EP_COST);
			TestTrue(TEXT("cooldown"), Field->ActivateFieldSkill() == EFieldSkillRefusal::OnCooldown);
		});

		It("phase-steps a short way ahead", [this]()
		{
			if (!TestTrue(TEXT("picked"), SpawnWithSkill(ESpellElement::Darkness, EFieldMovementType::PhaseStep)))
			{
				return;
			}
			const FVector Start = Pawn->GetActorLocation();
			TestTrue(TEXT("stepped"), Field->ActivateFieldSkill() == EFieldSkillRefusal::None);
			TestEqual(TEXT("its distance"), DistanceFrom(Start), FieldSkillRules::PHASE_STEP_DISTANCE, FieldAdvancedSpecValues::TOLERANCE);
		});

		It("marks for free, then recalls to the mark for its EP", [this]()
		{
			if (!TestTrue(TEXT("picked"), SpawnWithSkill(ESpellElement::Water, EFieldMovementType::MarkAndRecall)))
			{
				return;
			}
			const FVector Mark = Pawn->GetActorLocation();
			TestTrue(TEXT("marked"), Field->ActivateFieldSkill() == EFieldSkillRefusal::None && Field->HasRecallMark());
			TestEqual(TEXT("marking is free"), CharComp->CurrentEP, FieldAdvancedSpecValues::START_EP);
			Pawn->SetActorLocation(Mark + FVector(FieldAdvancedSpecValues::AWAY, 0.0f, 0.0f));
			TestTrue(TEXT("recalled"), Field->ActivateFieldSkill() == EFieldSkillRefusal::None);
			TestEqual(TEXT("back at the mark"), DistanceFrom(Mark), 0.0f, FieldAdvancedSpecValues::TOLERANCE);
			TestFalse(TEXT("mark used up"), Field->HasRecallMark());
			TestEqual(TEXT("recall paid"), CharComp->CurrentEP, FieldAdvancedSpecValues::START_EP - FieldSkillRules::RECALL_EP_COST);
		});

		It("launches straight up", [this]()
		{
			if (!TestTrue(TEXT("picked"), SpawnWithSkill(ESpellElement::Fire, EFieldMovementType::SuperLaunch)))
			{
				return;
			}
			TestTrue(TEXT("launched"), Field->ActivateFieldSkill() == EFieldSkillRefusal::None);
			// Rising at once (no pending launch), so the glide can only start at the top.
			TestEqual(TEXT("up"), static_cast<float>(Pawn->GetCharacterMovement()->Velocity.Z), FieldSkillRules::SUPER_LAUNCH_SPEED, FieldAdvancedSpecValues::TOLERANCE);
			TestTrue(TEXT("in the air"), Pawn->GetCharacterMovement()->MovementMode == MOVE_Falling);
			TestFalse(TEXT("not gliding yet"), Field->IsGliding());
		});

		It("regroups behind the other party member, and refuses with no one there", [this]()
		{
			if (!TestTrue(TEXT("picked"), SpawnWithSkill(ESpellElement::Light, EFieldMovementType::Regroup)))
			{
				return;
			}
			TestTrue(TEXT("no one"), Field->ActivateFieldSkill() == EFieldSkillRefusal::NoTarget);
			TestEqual(TEXT("nothing paid"), CharComp->CurrentEP, FieldAdvancedSpecValues::START_EP);
			UCharacterDataComponent *OtherComp = nullptr;
			ACharacter *Other = SpawnFieldPawn(ESpellElement::Fire, FVector(FieldAdvancedSpecValues::OTHER_MEMBER_X, 0.0f, 0.0f), OtherComp);
			if (!TestNotNull(TEXT("other member"), Other))
			{
				return;
			}
			TestTrue(TEXT("regrouped"), Field->ActivateFieldSkill() == EFieldSkillRefusal::None);
			TestTrue(TEXT("next to them"), FVector::Dist(Pawn->GetActorLocation(), Other->GetActorLocation()) < FieldSkillRules::REGROUP_OFFSET + FieldAdvancedSpecValues::AWAY);
			TestTrue(TEXT("came a long way"), Pawn->GetActorLocation().X > FieldAdvancedSpecValues::AWAY);
		});
	});
}

#endif
