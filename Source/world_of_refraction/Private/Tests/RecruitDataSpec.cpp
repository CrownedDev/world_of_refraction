// RecruitDataSpec.cpp
// B-075: a recruit joins as the CharacterData its placed pawn carries, not its class default,
// so the same lookup that dismisses it finds it; a second invite of the same character is refused.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"

BEGIN_DEFINE_SPEC(FRecruitDataSpec, "WoR.Party.RecruitData",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPartySessionSubsystem *PartySession = nullptr;
END_DEFINE_SPEC(FRecruitDataSpec)

void FRecruitDataSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		TestNotNull(TEXT("Party"), PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr);
	});

	AfterEach([this]()
	{
		PartySession = nullptr;
		TestWorld.Reset();
	});

	It("joins as the pawn's own CharacterData, once", [this]()
	{
		if (!PartySession) return;
		const TSoftClassPtr<ACombatCharacter> PawnClass(ACombatCharacter::StaticClass());
		UCharacterData *Placed = FWoRTestWorld::NewCharacterData();

		TestTrue(TEXT("Invited"), PartySession->InviteMemberWithData(PawnClass, Placed));
		TestNotEqual(TEXT("Found by the pawn's own data"), PartySession->GetMemberSlotByData(Placed), static_cast<int32>(INDEX_NONE));
		TestFalse(TEXT("A second invite is refused"), PartySession->InviteMemberWithData(PawnClass, Placed));
		TestFalse(TEXT("No data, no invite"), PartySession->InviteMemberWithData(PawnClass, nullptr));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
