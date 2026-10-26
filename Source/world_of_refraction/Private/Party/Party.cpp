// Party.cpp

#include "Party/Party.h"

#include "Character/CharacterData.h"

TArray<UCharacterData *> UParty::GetMemberCharacterData() const
{
	TArray<UCharacterData *> Result;
	Result.Reserve(Members.Num());
	for (const FPartyMember &Member : Members)
	{
		if (Member.CharacterData)
		{
			Result.Add(Member.CharacterData);
		}
	}
	return Result;
}

TArray<UCharacterData *> UParty::GetLivingMemberCharacterData() const
{
	TArray<UCharacterData *> Result;
	Result.Reserve(Members.Num());
	for (const FPartyMember &Member : Members)
	{
		if (!Member.CharacterData)
		{
			continue;
		}

		// A fallen member (D-043) stays on the roster but sits the fight out.
		if (Member.IsFallen())
		{
			continue;
		}

		Result.Add(Member.CharacterData);
	}
	return Result;
}
