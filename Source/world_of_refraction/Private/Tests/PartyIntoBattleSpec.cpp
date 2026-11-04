// PartyIntoBattleSpec.cpp
// B-071: party members in battle. B-346 (Q-21-audit): you command every member but the AI seats
// the leader set before the fight (never the leader); a seat is fixed during the fight and an AI
// seat plays at its own difficulty (default Medium); nothing hands members over when your fighters
// fall (PA-12 retired). Also: the per-fight player flag routing turns, the debug switch on a
// member's turn (D-026) and the gear gate for entering a fight (D-016).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
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

	It("commands every member by default; only the leader's AI seats go to the AI (B-346)", [this]()
	{
		UParty *Party = MakeParty();
		if (!TestNotNull(TEXT("Party"), Party)) return;
		for (int32 Slot = 0; Slot < PARTY_TEST_MEMBERS; ++Slot)
		{
			TestFalse(FString::Printf(TEXT("slot %d is yours by default"), Slot), PartySession->IsMemberAISeat(Slot));
		}
		TestFalse(TEXT("The leader is never an AI seat"), PartySession->SetMemberAISeat(PartyConstants::LEADER_SLOT, true));
		TestTrue(TEXT("A member can be an AI seat"), PartySession->SetMemberAISeat(PARTY_TEST_ALLY_SLOT, true));
		TestTrue(TEXT("That member is an AI seat"), PartySession->IsMemberAISeat(PARTY_TEST_ALLY_SLOT));
		TestTrue(TEXT("The old flag mirrors it (OBSOLETE)"), !Party->Members[PARTY_TEST_ALLY_SLOT].bPlayerControlled);
		TestTrue(TEXT("An AI seat defaults to Medium"), Party->Members[PARTY_TEST_ALLY_SLOT].AISeatDifficulty == EAIDifficulty::Medium);
	});

	It("keeps a seat fixed while a fight runs (B-346)", [this]()
	{
		if (!TestNotNull(TEXT("Party"), MakeParty()) || !Orchestrator) return;
		TestTrue(TEXT("AI seat set before the fight"), PartySession->SetMemberAISeat(PARTY_TEST_ALLY_SLOT, true));
		AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		if (!TestTrue(TEXT("Spawned"), Hero && Foe)) return;
		Orchestrator->StartCombat({Hero}, {Foe});
		TestFalse(TEXT("Can't hand the seat back mid-fight"), PartySession->SetMemberAISeat(PARTY_TEST_ALLY_SLOT, false));
		TestFalse(TEXT("Can't change its difficulty mid-fight"),
				  PartySession->SetMemberAISeatDifficulty(PARTY_TEST_ALLY_SLOT, EAIDifficulty::Hard));
		TestTrue(TEXT("Still an AI seat"), PartySession->IsMemberAISeat(PARTY_TEST_ALLY_SLOT));
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

	It("plays an AI seat at its own difficulty and enemies at the encounter's (B-346)", [this]()
	{
		UAIDecisionManager *AI = TestWorld->GetSubsystem<UAIDecisionManager>();
		AActor *Ally = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		if (!TestNotNull(TEXT("AIDecisionManager"), AI) || !TestTrue(TEXT("Spawned"), Ally && Foe) || !Orchestrator) return;
		Orchestrator->StartCombat({Ally}, {Foe}, EAIDifficulty::Easy);
		TestTrue(TEXT("An AI seat defaults to Medium"), AI->GetDifficultyFor(Ally) == EAIDifficulty::Medium);
		CombatControl::SetAISeatDifficulty(Ally, EAIDifficulty::Hard);
		TestTrue(TEXT("It plays at its seat's setting"), AI->GetDifficultyFor(Ally) == EAIDifficulty::Hard);
		TestTrue(TEXT("Enemy plays at the encounter's"), AI->GetDifficultyFor(Foe) == EAIDifficulty::Easy);
	});

	It("keeps an AI seat AI when your fighters fall (B-346: no takeover)", [this]()
	{
		if (!TestNotNull(TEXT("Party"), MakeParty()) || !Orchestrator) return;
		AActor *Yours = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		AActor *AIAlly = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		AActor *SecondAIAlly = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		if (!TestTrue(TEXT("Spawned"), Yours && AIAlly && SecondAIAlly && Foe)) return;
		CombatControl::SetPlayerDriven(Yours, true);
		Orchestrator->StartCombat({Yours, AIAlly, SecondAIAlly}, {Foe});

		UCharacterDataComponent *YoursComp = Yours->FindComponentByClass<UCharacterDataComponent>();
		YoursComp->ServerTakeDamage(YoursComp->MaxHP);
		TestFalse(TEXT("Your fighter is down"), IsAlive(Yours));
		TestTrue(TEXT("Combat still on"), Orchestrator->GetCombatState() == ECombatState::InProgress);
		TestTrue(TEXT("The AI seat stays AI"), Orchestrator->IsActorAIControlled(AIAlly));
		TestTrue(TEXT("So does the other"), Orchestrator->IsActorAIControlled(SecondAIAlly));
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
