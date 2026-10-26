// PartyDisplayWidget.cpp

#include "UI/Party/PartyDisplayWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Character/CharacterData.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "UI/Combat/CharacterPanelWidget.h"

namespace PartyDisplayConstants
{
	/** Gap between adjacent cards in the strip (slot padding, runtime-set —
	 *  designer cannot author runtime-added slots). */
	constexpr float CARD_GAP = 2.0f;
}

void UPartyDisplayWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// TransBuffer: keep the editor's undo buffer from holding this widget across
	// OpenLevel ("Old World not cleaned up by GC") — the
	// UCharacterPanelWidget::InitialiseForActor treatment.
	ClearFlags(RF_Transactional);

	// Build once; refresh forever after (TransBuffer: never recreate widgets
	// inside a designer-placed widget).
	EnsurePanels();

	if (UPartySessionSubsystem *PartySession = GetPartySession())
	{
		PartySession->OnPartyChanged.AddUniqueDynamic(this, &UPartyDisplayWidget::HandlePartyChanged);
	}

	// Initial paint — REQUIRED: CreateSoloParty's early-return on an existing
	// party does not broadcast, so hub re-entry would otherwise show ghosts.
	RefreshFromParty();
}

void UPartyDisplayWidget::NativeDestruct()
{
	// Safe to run twice: RemoveDynamic on an unbound delegate is a no-op.
	if (UPartySessionSubsystem *PartySession = GetPartySession())
	{
		PartySession->OnPartyChanged.RemoveDynamic(this, &UPartyDisplayWidget::HandlePartyChanged);
	}

	Super::NativeDestruct();
}

void UPartyDisplayWidget::EnsurePanels()
{
	if (Panels.Num() > 0)
	{
		return;
	}

	if (!PanelClass)
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("[PartyDisplay] PanelClass unset on '%s' - no panels built (configure it on the WBP)"),
			   *GetName());
		return;
	}
	if (!CardContainer)
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("[PartyDisplay] CardContainer missing on '%s' - no panels built (add a panel named CardContainer)"),
			   *GetName());
		return;
	}

	for (int32 i = 0; i < PartyConstants::MAX_PARTY_MEMBERS; ++i)
	{
		UCharacterPanelWidget *Panel = CreateWidget<UCharacterPanelWidget>(this, PanelClass);
		if (!Panel)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PartyDisplay] CreateWidget failed for panel %d"), i);
			continue;
		}
		// TransBuffer treatment for the runtime-created panel as well.
		Panel->ClearFlags(RF_Transactional);

		UPanelSlot *PanelSlot = CardContainer->AddChild(Panel);
		// Runtime-added slots cannot be authored in the designer: force Auto
		// sizing (Fill stacks the panels on top of each other) and the
		// between-panel gap here.
		if (UVerticalBoxSlot *VSlot = Cast<UVerticalBoxSlot>(PanelSlot))
		{
			VSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, PartyDisplayConstants::CARD_GAP));
			// Left-align: a fill-width slot overrides the panel's own SizeBox
			// 280 width override and stretches it across the container.
			VSlot->SetHorizontalAlignment(HAlign_Left);
		}
		else if (UHorizontalBoxSlot *HSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
		{
			HSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			HSlot->SetPadding(FMargin(0.0f, 0.0f, PartyDisplayConstants::CARD_GAP, 0.0f));
		}
		Panels.Add(Panel);
	}
	LastPanelNames.SetNum(Panels.Num());
}

void UPartyDisplayWidget::RefreshFromParty()
{
	const UPartySessionSubsystem *PartySession = GetPartySession();
	const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;

	for (int32 i = 0; i < Panels.Num(); ++i)
	{
		UCharacterPanelWidget *Panel = Panels[i];
		if (!Panel)
		{
			continue;
		}

		// Members is dense: i < count = populated, otherwise vacant (derived).
		// Vacant slots COLLAPSE rather than ghosting; the panel itself is never
		// destroyed — build-once is the TransBuffer mitigation and must hold.
		if (Party && i < Party->GetMemberCount())
		{
			const FPartyMember &Member = Party->Members[i];
			UCharacterData *Data = Member.CharacterData;
			Panel->SetVisibility(ESlateVisibility::Visible);
			// Carried trial vitals, or NO_CARRIED_VITALS — the panel falls back to
			// the asset's computed maxima on the sentinel, so a hub member reads full.
			Panel->InitialiseForData(Data, /*bIsLeader=*/i == 0, Member.CurrentHP, Member.CurrentEP);
			LastPanelNames[i] = Data ? Data->Name : FString();
		}
		else
		{
			// SetVacant first — clears stale state so a later reveal is clean.
			Panel->SetVacant();
			Panel->SetVisibility(ESlateVisibility::Collapsed);
			LastPanelNames[i] = FString();
		}
	}
}

UCharacterPanelWidget *UPartyDisplayWidget::GetPanel(int32 Index) const
{
	return Panels.IsValidIndex(Index) ? Panels[Index].Get() : nullptr;
}

FString UPartyDisplayWidget::GetPanelName(int32 Index) const
{
	return LastPanelNames.IsValidIndex(Index) ? LastPanelNames[Index] : FString();
}

void UPartyDisplayWidget::HandlePartyChanged(UParty * /*Party*/)
{
	// Contract: re-read state on fire; the payload is convenience only.
	RefreshFromParty();
}

UPartySessionSubsystem *UPartyDisplayWidget::GetPartySession() const
{
	const UGameInstance *GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
}
