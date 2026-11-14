// RecruitCardDebug.cpp

#include "Debug/Party/RecruitCardDebug.h"
#include "Party/PartyInteractionComponent.h"
#include "Party/RecruitCard.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float RECRUIT_CARD_SCREEN_SECONDS = 10.0f;

	UPartyInteractionComponent *FindNearestRecruit(UWorld *World)
	{
		APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
		return UPartyInteractionComponent::FindNearestInRange(PC ? PC->GetPawn() : nullptr);
	}

	void ShowLine(const FString &Line)
	{
		UE_LOG(LogWoR, Display, TEXT("[RecruitCard] %s"), *Line);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, RECRUIT_CARD_SCREEN_SECONDS, FColor::Cyan, Line);
		}
	}

	FAutoConsoleCommandWithWorld GPrintRecruitCardCmd(
		TEXT("wor.Party.PrintRecruitCard"),
		TEXT("DEBUG (B-471, D-179): the recruit card of the character the player faces: who they are, played by, and who can step out."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { RecruitCardDebug::PrintNearestRecruitCard(World); }));

	FAutoConsoleCommandWithWorldAndArgs GRecruitInPlaceOfCmd(
		TEXT("wor.Party.RecruitInPlaceOf"),
		TEXT("PLACEHOLDER (B-471, D-179; until the recruit card widget, B-471b): the faced recruit joins for this run in place of ")
			TEXT("the NPC at <slot> (never a player's slot)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
		{
			if (Args.Num() == 0 || !Args[0].IsNumeric())
			{
				UE_LOG(LogWoR, Warning, TEXT("[RecruitCard] Usage: wor.Party.RecruitInPlaceOf <slot>"));
				return;
			}
			RecruitCardDebug::RecruitNearestInPlaceOf(World, FCString::Atoi(*Args[0]));
		}));
}

void RecruitCardDebug::PrintNearestRecruitCard(UWorld *World)
{
	const UPartyInteractionComponent *Recruit = FindNearestRecruit(World);
	ShowLine(Recruit ? WoRRecruitCard::GetCardString(Recruit->BuildRecruitCard()) : FString(TEXT("no recruit in range")));
}

bool RecruitCardDebug::RecruitNearestInPlaceOf(UWorld *World, int32 StepOutSlot)
{
	UPartyInteractionComponent *Recruit = FindNearestRecruit(World);
	const bool bJoined = Recruit && Recruit->InviteInPlaceOf(StepOutSlot);
	ShowLine(bJoined ? FString::Printf(TEXT("joined; slot %d stepped out"), StepOutSlot)
					 : FString::Printf(TEXT("refused (no recruit in range, or slot %d is a player's / the leader's / empty)"), StepOutSlot));
	return bJoined;
}
