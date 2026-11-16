// FullAfterRestSpec.cpp
// CS-6 (docs/Audits/CharacterStateAudit.md, clash 6): after a rest a member starts the next fight at its real full HP,
// gear, stones and run levels included. The rest used to bank the asset formula's HP (CalculateMaxHealth), which the
// pawn's own larger max then kept as the starting HP.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	constexpr int32 FULL_REST_SEED = 6;
	constexpr int32 FULL_REST_BODY_LEVELS = 5;
	constexpr int32 FULL_REST_WOUNDED_HP = 1;
	constexpr int32 FULL_REST_MAX_HEALTH_POINTS = 10; // so Body scales HP
}

BEGIN_DEFINE_SPEC(FFullAfterRestSpec, "WoR.Party.FullAfterRest", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPartySessionSubsystem *PartySession = nullptr;
	UCharacterData *Data = nullptr;
END_DEFINE_SPEC(FFullAfterRestSpec)

void FFullAfterRestSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UTrialRunSubsystem *Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestTrue(TEXT("Party and run"), Party && Run)) return;
		Data = FWoRTestWorld::NewCharacterData();
		Data->MaxHealth = FULL_REST_MAX_HEALTH_POINTS;
		FPartyMember Member;
		Member.PawnClass = ACombatCharacter::StaticClass();
		Member.CharacterData = Data;
		Party->Members.Add(Member);
		// Body levels earned this run raise the pawn's max above the asset formula.
		Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), FULL_REST_SEED, 0, ETrialDifficulty::Medium, {}, {}, {});
		Run->AddRunEarnedLevels(UPoolSubsystem::GetCharacterKey(Party->Members.Last()), EWorldPillar::Body, FULL_REST_BODY_LEVELS);
	});

	AfterEach([this]()
	{
		PartySession = nullptr;
		Data = nullptr;
		TestWorld.Reset();
	});

	It("starts the fight after a rest at the pawn's own full HP, run levels included", [this]()
	{
		const int32 Slot = PartySession ? PartySession->GetMemberSlotByData(Data) : INDEX_NONE;
		if (!TestTrue(TEXT("Member in the party"), Slot != INDEX_NONE)) return;
		PartySession->SetMemberVitals(Slot, FULL_REST_WOUNDED_HP, 0); // the last fight left it wounded
		PartySession->RestMembersWhere([](const FPartyMember &) { return true; });

		AActor *Pawn = TestWorld->SpawnCombatant(Data); // the next fight's pawn
		const UCharacterDataComponent *CharComp = Pawn ? Pawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("Pawn"), CharComp)) return;
		if (!TestTrue(TEXT("Run levels raise the max above the asset formula"), CharComp->MaxHP > Data->CalculateMaxHealth())) return;
		TestEqual(TEXT("Full HP"), CharComp->CurrentHP, CharComp->MaxHP);
		TestEqual(TEXT("Full EP"), CharComp->CurrentEP, CharComp->MaxEP);
	});

	It("still starts a wounded member wounded when there was no rest", [this]()
	{
		const int32 Slot = PartySession ? PartySession->GetMemberSlotByData(Data) : INDEX_NONE;
		if (!TestTrue(TEXT("Member in the party"), Slot != INDEX_NONE)) return;
		PartySession->SetMemberVitals(Slot, FULL_REST_WOUNDED_HP, 0);
		AActor *Pawn = TestWorld->SpawnCombatant(Data);
		const UCharacterDataComponent *CharComp = Pawn ? Pawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("Pawn"), CharComp)) return;
		TestEqual(TEXT("The carried HP"), CharComp->CurrentHP, FULL_REST_WOUNDED_HP);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
