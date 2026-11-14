// RecruitCard.cpp

#include "Party/RecruitCard.h"
#include "Party/Party.h"
#include "Character/CharacterData.h"
#include "WoRGameNames.h"

#define LOCTEXT_NAMESPACE "WoRRecruitCard"

namespace
{
	constexpr int32 RECRUIT_LEADER_SLOT = 0;
}

FText WoRRecruitCard::GetPlayedByText(ERecruitPlayedBy PlayedBy)
{
	return PlayedBy == ERecruitPlayedBy::Player ? LOCTEXT("PlayedByPlayer", "Player") : LOCTEXT("PlayedByNPC", "NPC");
}

FRecruitCard WoRRecruitCard::Build(const UCharacterData *Recruit, const TArray<FPartyMember> &Members, TFunctionRef<bool(int32 SlotIndex)> IsPlayerSlot)
{
	FRecruitCard Card;
	Card.RuleLine = LOCTEXT("RuleLine", "Joins for this run only and uses what your party carries.");
	Card.PlayedByText = GetPlayedByText(Card.PlayedBy);
	if (!Recruit)
	{
		Card.Refusal = LOCTEXT("RefuseNoRecruit", "No one to recruit.");
		return Card;
	}
	Card.Name = FText::FromString(Recruit->Name);
	Card.ClassName = WoRGameNames::GetCharacterClassName(Recruit->CharacterClass);
	Card.bPartyFull = Members.Num() >= PartyConstants::MAX_PARTY_MEMBERS;
	if (!Card.bPartyFull)
	{
		return Card;
	}
	// D-179: only an NPC steps out; a real player (and the leader's slot, yours) never does.
	for (int32 Slot = 0; Slot < Members.Num(); ++Slot)
	{
		if (Slot == RECRUIT_LEADER_SLOT || IsPlayerSlot(Slot))
		{
			continue;
		}
		FRecruitStepOutChoice &Choice = Card.StepOut.AddDefaulted_GetRef();
		Choice.SlotIndex = Slot;
		Choice.Name = FText::FromString(Members[Slot].CharacterData ? Members[Slot].CharacterData->Name : FString());
	}
	if (Card.StepOut.Num() == 0)
	{
		Card.Refusal = LOCTEXT("RefuseAllPlayers", "The party is full of players: no one can step out.");
	}
	return Card;
}

FString WoRRecruitCard::GetCardString(const FRecruitCard &Card)
{
	FString Out = FString::Printf(TEXT("%s (%s), played by %s. %s"), *Card.Name.ToString(), *Card.ClassName.ToString(), *Card.PlayedByText.ToString(),
								  *Card.RuleLine.ToString());
	if (Card.bPartyFull)
	{
		TArray<FString> Names;
		for (const FRecruitStepOutChoice &Choice : Card.StepOut)
		{
			Names.Add(FString::Printf(TEXT("%s (slot %d)"), *Choice.Name.ToString(), Choice.SlotIndex));
		}
		Out += FString::Printf(TEXT(" Party full; can step out: %s"), Names.Num() > 0 ? *FString::Join(Names, TEXT(", ")) : TEXT("no one"));
	}
	if (!Card.Refusal.IsEmpty())
	{
		Out += FString::Printf(TEXT(" REFUSED: %s"), *Card.Refusal.ToString());
	}
	return Out;
}

#undef LOCTEXT_NAMESPACE
