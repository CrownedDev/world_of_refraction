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

		// Death is a CARRIED zero, written back by ABattleGameMode::HandleCombatEnd.
		// The HasCarriedVitals() guard is load-bearing: without it the
		// NO_CARRIED_VITALS sentinel would still not match zero, but the intent
		// would rest on the sentinel's numeric value rather than on its meaning.
		if (Member.HasCarriedVitals() && Member.CurrentHP == 0)
		{
			continue;
		}

		Result.Add(Member.CharacterData);
	}
	return Result;
}
