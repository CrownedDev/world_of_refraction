// RecruitCardSpec.cpp
// B-471 (D-179): the recruit card. With room, nobody steps out; with a full party, only an NPC can (never the leader's
// slot, never a real player's), and a party of players refuses the offer. The card reads "NPC" or "Player".

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Character/CharacterData.h"
#include "Party/Party.h"
#include "Party/RecruitCard.h"

namespace
{
	UCharacterData *RecruitCardTestCharacter(const TCHAR *Name)
	{
		UCharacterData *Data = NewObject<UCharacterData>(GetTransientPackage());
		Data->Name = Name;
		return Data;
	}

	TArray<FPartyMember> RecruitCardTestParty(int32 Count)
	{
		TArray<FPartyMember> Members;
		for (int32 Slot = 0; Slot < Count; ++Slot)
		{
			FPartyMember &Member = Members.AddDefaulted_GetRef();
			Member.CharacterData = RecruitCardTestCharacter(*FString::Printf(TEXT("Member %d"), Slot));
		}
		return Members;
	}
}

BEGIN_DEFINE_SPEC(FRecruitCardSpec, "WoR.Party.RecruitCard", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FRecruitCardSpec)

void FRecruitCardSpec::Define()
{
	It("needs nobody to step out when the party has room", [this]()
	{
		const FRecruitCard Card = WoRRecruitCard::Build(RecruitCardTestCharacter(TEXT("Sera")), RecruitCardTestParty(1), [](int32) { return false; });
		TestFalse(TEXT("not full"), Card.bPartyFull);
		TestEqual(TEXT("no step-out list"), Card.StepOut.Num(), 0);
		TestTrue(TEXT("no refusal"), Card.Refusal.IsEmpty());
		TestEqual(TEXT("played by NPC"), Card.PlayedByText.ToString(), FString(TEXT("NPC")));
		TestFalse(TEXT("the rule in one line"), Card.RuleLine.IsEmpty());
	});

	It("offers only the NPC slots of a full party, never the leader's (D-179)", [this]()
	{
		const TArray<FPartyMember> Party = RecruitCardTestParty(PartyConstants::MAX_PARTY_MEMBERS);
		const FRecruitCard AllNPCs = WoRRecruitCard::Build(RecruitCardTestCharacter(TEXT("Sera")), Party, [](int32) { return false; });
		TestTrue(TEXT("full"), AllNPCs.bPartyFull);
		if (TestEqual(TEXT("both NPCs can step out"), AllNPCs.StepOut.Num(), PartyConstants::MAX_PARTY_MEMBERS - 1))
		{
			TestFalse(TEXT("not the leader"), AllNPCs.StepOut.ContainsByPredicate([](const FRecruitStepOutChoice &C) { return C.SlotIndex == 0; }));
		}
		const FRecruitCard OnePlayer = WoRRecruitCard::Build(RecruitCardTestCharacter(TEXT("Sera")), Party, [](int32 Slot) { return Slot == 1; });
		TestTrue(TEXT("a player's slot is never offered"), OnePlayer.StepOut.Num() == 1 && OnePlayer.StepOut[0].SlotIndex == 2);
	});

	It("refuses the offer when every other slot is a player (D-179)", [this]()
	{
		const FRecruitCard Card = WoRRecruitCard::Build(RecruitCardTestCharacter(TEXT("Sera")), RecruitCardTestParty(PartyConstants::MAX_PARTY_MEMBERS),
														[](int32) { return true; });
		TestEqual(TEXT("no one to step out"), Card.StepOut.Num(), 0);
		TestFalse(TEXT("refused, with the reason"), Card.Refusal.IsEmpty());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
