// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Combat/TurnOrderSlotWidget.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Components/TextBlock.h"
#include "GameFramework/Actor.h"

namespace TurnSlotStyle
{
	// TODO: style pass.
	constexpr float ActiveScale = 1.15f;
	constexpr float InactiveScale = 1.0f;
	constexpr float ActiveOpacity = 1.0f;
	constexpr float InactiveOpacity = 0.75f;
	constexpr const TCHAR *BonusBadge = TEXT("+");
}

void UTurnOrderSlotWidget::InitialiseSlot_Implementation(AActor *InActor, int32 InTurnNumber, bool bIsActive, bool bIsBonus)
{
	if (!InActor)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);

	FString DisplayName = InActor->GetName();
	if (const UCharacterDataComponent *CharComp = InActor->FindComponentByClass<UCharacterDataComponent>())
	{
		if (CharComp->CharacterData)
		{
			DisplayName = CharComp->CharacterData->Name;
		}
	}

	if (NameText)
	{
		NameText->SetText(FText::FromString(DisplayName));
	}
	if (TurnNumberText)
	{
		TurnNumberText->SetText(FText::FromString(
			FString::Printf(TEXT("%s%d"), bIsBonus ? TurnSlotStyle::BonusBadge : TEXT(""), InTurnNumber)));
	}

	const float Scale = bIsActive ? TurnSlotStyle::ActiveScale : TurnSlotStyle::InactiveScale;
	SetRenderScale(FVector2D(Scale, Scale));
	SetRenderOpacity(bIsActive ? TurnSlotStyle::ActiveOpacity : TurnSlotStyle::InactiveOpacity);
}
