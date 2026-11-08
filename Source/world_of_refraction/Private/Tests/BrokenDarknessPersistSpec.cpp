// BrokenDarknessPersistSpec.cpp
// B-348c (Q-348b, Crown 2026-11-08): Broken Darkness is persistent in and out of the run. A party member whose record
// says it broke spawns in Broken Darkness; banking writes the state and strain back; the run's end clears the strain
// only. The save round trip is in SaveSpec.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"

namespace
{
	constexpr float BD_PERSIST_TEST_STRAIN = 30.0f;
}

BEGIN_DEFINE_SPEC(FBrokenDarknessPersistSpec, "WoR.Combat.BrokenDarknessPersists", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> TestWorld;
UPartySessionSubsystem *PartySession = nullptr;
UCharacterData *Member = nullptr;

/** Review: as ACombatCharacter builds it: the character component first (its BeginPlay runs first and seeds the
 *  manager), then the Broken Darkness manager's own BeginPlay, which must not undo the seeding. */
UBrokenDarknessManager *SpawnWithManager(UCharacterData *Data)
{
	UWorld *World = TestWorld->GetWorld();
	AActor *Actor = World ? World->SpawnActor<AActor>() : nullptr;
	if (!Actor)
	{
		return nullptr;
	}
	UBrokenDarknessManager *Manager = NewObject<UBrokenDarknessManager>(Actor);
	Actor->AddInstanceComponent(Manager);
	UCharacterDataComponent *CharComp = NewObject<UCharacterDataComponent>(Actor);
	CharComp->CharacterData = Data;
	Actor->AddInstanceComponent(CharComp);
	CharComp->RegisterComponent(); // BeginPlay: the init cascade (flip, absorption, strain)
	Manager->RegisterComponent();  // then the manager's BeginPlay
	return Manager;
}
END_DEFINE_SPEC(FBrokenDarknessPersistSpec)

void FBrokenDarknessPersistSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party))
		{
			return;
		}
		Party->Members.Empty();
		Member = FWoRTestWorld::NewCharacterData();
		Member->Name = TEXT("Broken");
		FPartyMember &PartyMember = Party->Members.AddDefaulted_GetRef();
		PartyMember.PawnClass = ACombatCharacter::StaticClass();
		PartyMember.CharacterData = Member;
	});

	AfterEach([this]()
	{
		PartySession = nullptr;
		Member = nullptr;
		TestWorld.Reset();
	});

	It("a member that broke spawns in Broken Darkness", [this]()
	{
		if (!TestTrue(TEXT("setup"), PartySession && Member)) return;
		PartySession->SetMemberBrokenDarkness(0, true, 0.0f);
		AActor *Pawn = TestWorld->SpawnCombatant(Member);
		const UCharacterDataComponent *Comp = Pawn ? Pawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("spawned"), Comp)) return;
		TestTrue(TEXT("still Broken Darkness"), Comp->IsBrokenDarkness());
	});

	It("the manager stays flipped after its own BeginPlay (review)", [this]()
	{
		if (!TestTrue(TEXT("setup"), PartySession && Member)) return;
		PartySession->SetMemberBrokenDarkness(0, true, 0.0f);
		const UBrokenDarknessManager *Manager = SpawnWithManager(Member);
		if (!TestNotNull(TEXT("manager"), Manager)) return;
		TestTrue(TEXT("transformed"), Manager->IsTransformed());
	});

	It("the run's strain survives the manager's BeginPlay (review)", [this]()
	{
		if (!TestTrue(TEXT("setup"), PartySession && Member)) return;
		PartySession->SetMemberBrokenDarkness(0, false, BD_PERSIST_TEST_STRAIN);
		const UBrokenDarknessManager *Manager = SpawnWithManager(Member);
		if (!TestNotNull(TEXT("manager"), Manager)) return;
		TestFalse(TEXT("not transformed"), Manager->IsTransformed());
		TestEqual(TEXT("its run strain"), Manager->GetAccruedStrain(), BD_PERSIST_TEST_STRAIN);
	});

	It("a member that never broke spawns as itself", [this]()
	{
		if (!TestNotNull(TEXT("setup"), Member)) return;
		AActor *Pawn = TestWorld->SpawnCombatant(Member);
		const UCharacterDataComponent *Comp = Pawn ? Pawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("spawned"), Comp)) return;
		TestFalse(TEXT("not Broken Darkness"), Comp->IsBrokenDarkness());
	});

	It("banks the state and strain; the run's end clears only the strain", [this]()
	{
		if (!TestNotNull(TEXT("setup"), PartySession)) return;
		TestTrue(TEXT("banked"), PartySession->SetMemberBrokenDarkness(0, true, BD_PERSIST_TEST_STRAIN));
		TestFalse(TEXT("an invalid slot"), PartySession->SetMemberBrokenDarkness(5, true, 0.0f));
		const FPartyMember &Record = PartySession->GetLocalParty()->Members[0];
		TestTrue(TEXT("in Broken Darkness"), Record.bBrokenDarkness);
		TestEqual(TEXT("its strain"), Record.RunStrain, BD_PERSIST_TEST_STRAIN);
		PartySession->ClearAllMemberVitals();
		TestEqual(TEXT("the run's end clears the strain"), PartySession->GetLocalParty()->Members[0].RunStrain, 0.0f);
		TestTrue(TEXT("but not Broken Darkness"), PartySession->GetLocalParty()->Members[0].bBrokenDarkness);
		PartySession->SetMemberBrokenDarkness(0, false, 0.0f);
		TestFalse(TEXT("a revert is banked too"), PartySession->GetLocalParty()->Members[0].bBrokenDarkness);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
