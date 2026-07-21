// PartyMemberCardWidget.cpp

#include "UI/Party/PartyMemberCardWidget.h"

#include "Character/CharacterData.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Infusion/ElementColors.h"

namespace PartyCardConstants
{
	/** Ghost-state ElementBar opacity — dim enough to read as vacant, visible
	 *  enough to hold the card's silhouette. */
	constexpr float DEFAULT_GHOST_ALPHA = 0.25f;
}

UPartyMemberCardWidget::UPartyMemberCardWidget(const FObjectInitializer &ObjectInitializer)
	: Super(ObjectInitializer)
{
	GhostAlpha = PartyCardConstants::DEFAULT_GHOST_ALPHA;
}

void UPartyMemberCardWidget::SetMemberData(UCharacterData *Data, bool bIsLeader)
{
	// TransBuffer: keep the editor's undo buffer from holding this widget across
	// OpenLevel ("Old World not cleaned up by GC"). Same treatment as
	// UCharacterPanelWidget::InitialiseForActor — cleared at the init entry point.
	ClearFlags(RF_Transactional);

	if (!Data)
	{
		SetEmpty();
		return;
	}

	bIsEmpty = false;

	if (PortraitImage)
	{
		CachePortraitBrushOnce();
		if (Data->Portrait)
		{
			PortraitImage->SetBrushFromTexture(Data->Portrait);
		}
		else
		{
			// Null Portrait is the NORMAL case today (unset on every lord) — show
			// the WBP's own placeholder, keep the layout space, no warning.
			PortraitImage->SetBrush(DefaultPortraitBrush);
		}
		PortraitImage->SetColorAndOpacity(FLinearColor::White);
	}

	if (NameText)
	{
		// Slot 0 is the player per the design lock — the leader card says so.
		const FString DisplayName = bIsLeader
										? FString::Printf(TEXT("%s (You)"), *Data->Name)
										: Data->Name;
		NameText->SetText(FText::FromString(DisplayName));
	}

	if (ClassText)
	{
		// No class icon exists in the project; enum display text is the shop's
		// own fallback (merchant type does the same).
		ClassText->SetText(UEnum::GetDisplayValueAsText(Data->CharacterClass));
	}

	if (ElementBar)
	{
		// GetElement(), NOT InnateElement: Caster → innate, others → Generic.
		ElementBar->SetColorAndOpacity(ElementColors::GetColorForElement(Data->GetElement()));
	}

	if (HealthText)
	{
		// Computed hit points — the MaxHealth UPROPERTY is a raw sub-stat point
		// count, not HP.
		HealthText->SetText(FText::AsNumber(Data->CalculateMaxHealth()));
	}

	if (EnergyText)
	{
		EnergyText->SetText(FText::AsNumber(Data->CalculateMaxEnergy()));
	}
}

void UPartyMemberCardWidget::SetEmpty()
{
	// TransBuffer — see SetMemberData; either entry point may be hit first.
	ClearFlags(RF_Transactional);

	bIsEmpty = true;

	if (PortraitImage)
	{
		CachePortraitBrushOnce();
		// Placeholder brush back, dimmed like the other ghost fields.
		PortraitImage->SetBrush(DefaultPortraitBrush);
		PortraitImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, GhostAlpha));
	}

	if (NameText)
	{
		NameText->SetText(NSLOCTEXT("Party", "EmptySlot", "Empty"));
	}

	if (ClassText)
	{
		ClassText->SetText(FText::GetEmpty());
	}

	if (ElementBar)
	{
		// Neutral tint (Generic brown) at ghost alpha — a vacant slot carries no
		// element identity.
		FLinearColor Dimmed = ElementColors::GetColorForElement(ESpellElement::Generic);
		Dimmed.A = GhostAlpha;
		ElementBar->SetColorAndOpacity(Dimmed);
	}

	if (HealthText)
	{
		HealthText->SetText(FText::GetEmpty());
	}

	if (EnergyText)
	{
		EnergyText->SetText(FText::GetEmpty());
	}
}

void UPartyMemberCardWidget::CachePortraitBrushOnce()
{
	if (!bPortraitBrushCached && PortraitImage)
	{
		DefaultPortraitBrush = PortraitImage->GetBrush();
		bPortraitBrushCached = true;
	}
}
