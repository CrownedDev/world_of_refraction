// PartyIntoBattleSpec.cpp
// B-071: party members in battle. The Player/AI setting per member (D-004, the leader
// always Player), the per-fight player flag routing turns, the switch on a member's turn
// (D-026), the fixed ally AI level (D-044), taking over when your fighters fall (D-045)
// and the gear gate for entering a fight (D-016).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AI/AIDecisionConstants.h"
#include "AI/AIDecisionManager.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Combat/CombatControl.h"
#include "Combat/CombatOrchestrator.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Trial/BattleGameMode.h"

namespace
{
	constexpr int32 PARTY_TEST_MEMBERS = 3;
	constexpr int32 PARTY_TEST_ALLY_SLOT = 1;
}

BEGIN_DEFINE_SPEC(FPartyIntoBattleSpec, "WoR.Party.IntoBattle",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	UPartySessionSubsystem *PartySession = nullptr;

	/** A local party of PARTY_TEST_MEMBERS transient members (no leader controller). */
	UParty *MakeParty()
	{
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!Party)
		{
			return nullptr;
		}
		for (int32 Slot = 0; Slot < PARTY_TEST_MEMBERS; ++Slot)
		{
			FPartyMember Member;
			Member.PawnClass = ACombatCharacter::StaticClass();
			Member.CharacterData = FWoRTestWorld::NewCharacterData();
			Party->Members.Add(Member);
		}
		return Party;
	}

	static bool IsAlive(const AActor *Actor)
	{
		const UCharacterDataComponent *Comp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		return Comp && Comp->bIsAlive;
	}
END_DEFINE_SPEC(FPartyIntoBattleSpec)

void FPartyIntoBattleSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		TestNotNull(TEXT("Orchestrator"), Orchestrator);
		TestNotNull(TEXT("PartySession"), PartySession);
	});

	AfterEach([this]()
	{
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		Orchestrator = nullptr;
		PartySession = nullptr;
		TestWorld.Reset();
	});

	It("keeps the leader on Player and lets other members go to the AI", [this]()
	{
		if (!TestNotNull(TEXT("Party"), MakeParty())) return;
		TestTrue(TEXT("Members default to Player"), PartySession->IsMemberPlayerControlled(PARTY_TEST_ALLY_SLOT));
		TestFalse(TEXT("The leader can't go to the AI"), PartySession->SetMemberPlayerControlled(PartyConstants::LEADER_SLOT, false));
		TestTrue(TEXT("The leader stays Player"), PartySession->IsMemberPlayerControlled(PartyConstants::LEADER_SLOT));
		TestTrue(TEXT("A member can go to the AI"), PartySession->SetMemberPlayerControlled(PARTY_TEST_ALLY_SLOT, false));
		TestFalse(TEXT("That member is AI"), PartySession->IsMemberPlayerControlled(PARTY_TEST_ALLY_SLOT));

		TestFalse(TEXT("Fall setting defaults to Take over"), PartySession->GetLetAIFinish());
		PartySession->SetLetAIFinish(true);
		TestTrue(TEXT("Let AI finish set"), PartySession->GetLetAIFinish());
	});

	It("routes an unpossessed member with the Player flag to the player", [this]()
	{
		AActor *Member = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		if (!TestNotNull(TEXT("Member"), Member) || !Orchestrator) return;
		TestTrue(TEXT("Unpossessed pawn is AI"), Orchestrator->IsActorAIControlled(Member));
		CombatControl::SetPlayerDriven(Member, true);
		TestTrue(TEXT("Flagged pawn is player-driven"), CombatControl::IsPlayerDriven(Member));
		TestFalse(TEXT("Flagged pawn is routed to the player"), Orchestrator->IsActorAIControlled(Member));
	});

	It("switches a member between the AI and you on its own turn", [this]()
	{
		AActor *Ally = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		if (!TestTrue(TEXT("Spawned"), Ally && Foe) || !Orchestrator) return;
		Orchestrator->StartCombat({Ally}, {Foe});
		if (!TestTrue(TEXT("Turn 1 is the ally's"), Orchestrator->GetCurrentActor() == Ally)) return;

		TestFalse(TEXT("An enemy can't be switched"), Orchestrator->GetControlSwitchRefusedReason(Foe).IsEmpty());
		TestTrue(TEXT("Take the AI ally over"), Orchestrator->SwitchMemberControl(Ally));
		TestFalse(TEXT("You play it now"), Orchestrator->IsActorAIControlled(Ally));
		TestTrue(TEXT("Hand it back to the AI"), Orchestrator->SwitchMemberControl(Ally));
		TestTrue(TEXT("The AI plays it again"), Orchestrator->IsActorAIControlled(Ally));
	});

	It("plays AI allies at the fixed ally level and enemies at the encounter's", [this]()
	{
		UAIDecisionManager *AI = TestWorld->GetSubsystem<UAIDecisionManager>();
		AActor *Ally = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		if (!TestNotNull(TEXT("AIDecisionManager"), AI) || !TestTrue(TEXT("Spawned"), Ally && Foe) || !Orchestrator) return;
		Orchestrator->StartCombat({Ally}, {Foe}, EAIDifficulty::Easy);
		TestTrue(TEXT("Ally plays at the ally level"), AI->GetDifficultyFor(Ally) == AIConstants::ALLY_AI_DIFFICULTY);
		TestTrue(TEXT("Enemy plays at the encounter's"), AI->GetDifficultyFor(Foe) == EAIDifficulty::Easy);
	});

	It("hands you the AI members when your fighters fall, unless Let AI finish", [this]()
	{
		if (!TestNotNull(TEXT("Party"), MakeParty()) || !Orchestrator) return;
		AActor *Yours = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		AActor *AIAlly = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		AActor *SecondAIAlly = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		if (!TestTrue(TEXT("Spawned"), Yours && AIAlly && SecondAIAlly && Foe)) return;
		CombatControl::SetPlayerDriven(Yours, true);
		Orchestrator->StartCombat({Yours, AIAlly, SecondAIAlly}, {Foe});

		// Let AI finish: the AI keeps the first ally after your fighter falls.
		PartySession->SetLetAIFinish(true);
		UCharacterDataComponent *YoursComp = Yours->FindComponentByClass<UCharacterDataComponent>();
		YoursComp->ServerTakeDamage(YoursComp->MaxHP);
		TestFalse(TEXT("Your fighter is down"), IsAlive(Yours));
		TestTrue(TEXT("Let AI finish: still AI"), Orchestrator->IsActorAIControlled(AIAlly));

		// Take over: the next local death with no Player member standing hands you the rest.
		PartySession->SetLetAIFinish(false);
		UCharacterDataComponent *AIAllyComp = AIAlly->FindComponentByClass<UCharacterDataComponent>();
		AIAllyComp->ServerTakeDamage(AIAllyComp->MaxHP);
		TestTrue(TEXT("Combat still on"), Orchestrator->GetCombatState() == ECombatState::InProgress);
		TestFalse(TEXT("Take over: you play the last ally"), Orchestrator->IsActorAIControlled(SecondAIAlly));
	});

	It("keeps a gearless Generic out of a fight but lets a Refractor in", [this]()
	{
		UCharacterData *GenericData = FWoRTestWorld::NewCharacterData();
		GenericData->CharacterClass = ECharacterClass::Generic;
		UCharacterData *RefractorData = FWoRTestWorld::NewCharacterData();
		RefractorData->CharacterClass = ECharacterClass::Caster;
		AActor *Generic = TestWorld->SpawnCombatant(GenericData);
		AActor *Refractor = TestWorld->SpawnCombatant(RefractorData);
		if (!TestTrue(TEXT("Spawned"), Generic && Refractor)) return;
		TestFalse(TEXT("Gearless Generic sits out"), ABattleGameMode::CanEnterCombat(Generic));
		TestTrue(TEXT("Refractor fights on its innate element"), ABattleGameMode::CanEnterCombat(Refractor));
	});
}

#endif
