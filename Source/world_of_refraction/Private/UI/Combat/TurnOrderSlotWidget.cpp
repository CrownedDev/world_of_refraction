// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Combat/TurnOrderSlotWidget.h"
#include "UI/Combat/Battle/BattleHUDView.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Components/Border.h"
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
	// Background tints, the values WBP_TurnOrderSlot's InitialiseSlot override used.
	const FLinearColor BonusBackground(0.016130f, 0.505208f, 0.053500f, 0.7f);
	const FLinearColor ActiveBackground(0.0f, 0.0f, 0.0f, 0.7f);
	const FLinearColor InactiveBackground(0.0f, 0.0f, 0.0f, 0.3f);
}

void UTurnOrderSlotWidget::InitialiseSlot_Implementation(AActor *InActor, int32 InTurnNumber, bool bIsActive, bool bIsBonus)
{
	if (!InActor)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (NameText)
	{
		NameText->SetText(BattleHUDView::GetCombatantName(InActor)); // D-229: You or the player's tag, as the card
	}
	if (TurnNumberText)
	{
		TurnNumberText->SetText(FText::FromString(
			FString::Printf(TEXT("%s%d"), bIsBonus ? TurnSlotStyle::BonusBadge : TEXT(""), InTurnNumber)));
	}

	if (BackgroundBorder)
	{
		const FLinearColor &Background = bIsBonus    ? TurnSlotStyle::BonusBackground
										  : bIsActive ? TurnSlotStyle::ActiveBackground
													  : TurnSlotStyle::InactiveBackground;
		BackgroundBorder->SetBrushColor(Background);
	}

	const float Scale = bIsActive ? TurnSlotStyle::ActiveScale : TurnSlotStyle::InactiveScale;
	SetRenderScale(FVector2D(Scale, Scale));
	SetRenderOpacity(bIsActive ? TurnSlotStyle::ActiveOpacity : TurnSlotStyle::InactiveOpacity);
}
