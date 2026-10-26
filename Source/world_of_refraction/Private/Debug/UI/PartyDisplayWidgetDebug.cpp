// PartyDisplayWidgetDebug.cpp

#include "Debug/UI/PartyDisplayWidgetDebug.h"

#include "Engine/Engine.h"
#include "UI/Combat/CharacterPanelWidget.h"
#include "UI/Party/PartyDisplayWidget.h"

FString UPartyDisplayWidgetDebug::GetPartyDisplayStateString(const UPartyDisplayWidget *Display)
{
	if (!Display)
	{
		return TEXT("[PartyDisplay] null widget");
	}

	const int32 PanelCount = Display->GetPanelCount();
	FString Result = FString::Printf(TEXT("[PartyDisplay] %s - %d panel(s)"), *Display->GetName(), PanelCount);
	for (int32 i = 0; i < PanelCount; ++i)
	{
		const UCharacterPanelWidget *Panel = Display->GetPanel(i);
		if (!Panel)
		{
			Result += FString::Printf(TEXT("\n  [%d] <null panel>"), i);
			continue;
		}

		// The actual mode, straight from the panel. Name comes from the
		// display's cache — GetDisplayData is private on the panel.
		const FString Mode = UEnum::GetDisplayValueAsText(Panel->GetDisplayMode()).ToString();
		const FString Name = Display->GetPanelName(i);
		Result += Name.IsEmpty()
					  ? FString::Printf(TEXT("\n  [%d] %s"), i, *Mode)
					  : FString::Printf(TEXT("\n  [%d] %s: %s"), i, *Mode, *Name);
	}
	return Result;
}

void UPartyDisplayWidgetDebug::PrintPartyDisplayState(const UPartyDisplayWidget *Display, float Duration)
{
	const FString State = GetPartyDisplayStateString(Display);
	UE_LOG(LogTemp, Display, TEXT("%s"), *State);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Green, State);
	}
}
