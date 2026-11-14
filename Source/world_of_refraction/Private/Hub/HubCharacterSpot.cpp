// HubCharacterSpot.cpp

#include "Hub/HubCharacterSpot.h"

#include "Character/CharacterData.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UI/Hub/NpcTalkWidget.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "HubCharacterSpot"

void AHubCharacterSpot::Interact(APawn *Pawn)
{
	APlayerController *PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!Character)
	{
		UE_LOG(LogWoR, Warning, TEXT("[HubCharacterSpot] %s has no Character set"), *GetName());
		return;
	}
	if (!UNpcTalkWidget::OpenForCharacter(PC, Character))
	{
		UE_LOG(LogWoR, Warning, TEXT("[HubCharacterSpot] %s: the talk box didn't open (no root layout)"), *GetName());
	}
}

FInteractPrompt AHubCharacterSpot::GetPrompt() const
{
	// TODO: B-127: the character's Name becomes FText on the asset.
	return Character ? FInteractPrompt(EInteractVerb::Talk, FText::FromString(Character->Name)) : FInteractPrompt();
}

FString AHubCharacterSpot::GetHintText() const
{
	return Character ? FText::Format(LOCTEXT("CharacterHint", "Press E: talk to {0}"), FText::FromString(Character->Name)).ToString() : FString();
}

#undef LOCTEXT_NAMESPACE
