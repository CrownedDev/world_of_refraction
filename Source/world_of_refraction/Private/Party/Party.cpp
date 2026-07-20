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
