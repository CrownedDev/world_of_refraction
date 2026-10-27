// Party.cpp

#include "Party/Party.h"

#include "Character/CharacterData.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

void UParty::SetLeader(APlayerController *NewLeader)
{
	Leader = NewLeader;
	LeaderPlayer = NewLeader ? NewLeader->GetLocalPlayer() : nullptr;
}

APlayerController *UParty::ResolveLeader(const UWorld *World) const
{
	APlayerController *Current = Leader.Get();
	if (Current && Current->GetWorld() == World)
	{
		return Current;
	}
	const ULocalPlayer *Player = LeaderPlayer.Get();
	return Player && World ? Player->GetPlayerController(World) : nullptr;
}

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
