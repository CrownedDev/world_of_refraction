// GiveWidget.cpp

#include "UI/Hub/GiveWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Character/CharacterData.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/GearGive.h"
#include "Inventory/ItemTier.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

#define LOCTEXT_NAMESPACE "Give"

namespace GiveStyle
{
	// TODO: style pass (placeholder until the CommonUI screens).
	const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.7f);
	constexpr int32 FontSize = 22;
	constexpr float RowPadding = 6.0f;
	constexpr int32 ZOrder = 50;
}

namespace GiveBuild
{
	UTextBlock *AddText(UWidgetTree *Tree, UVerticalBox *Box)
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = GiveStyle::FontSize;
		Text->SetFont(Font);
		Text->SetAutoWrapText(true);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Text))
		{
			BoxSlot->SetPadding(FMargin(GiveStyle::RowPadding));
		}
		return Text;
	}

	UButton *AddButton(UWidgetTree *Tree, UVerticalBox *Box, const FText &Label)
	{
		UButton *Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = GiveStyle::FontSize;
		Text->SetFont(Font);
		Text->SetText(Label);
		Button->AddChild(Text);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Button))
		{
			BoxSlot->SetPadding(FMargin(GiveStyle::RowPadding));
		}
		return Button;
	}

	// TODO: gear and tier names are FStrings; FText with B-127 (D-061).
	FText GearLabel(const FString &Name, EItemTier Tier)
	{
		return FText::Format(LOCTEXT("GearLabel", "{0} ({1})"), FText::FromString(Name), FText::FromString(TierHelpers::GetTierName(Tier)));
	}
}

UGiveWidget *UGiveWidget::Open(APlayerController *PC)
{
	if (!PC)
	{
		return nullptr;
	}
	UGiveWidget *Widget = CreateWidget<UGiveWidget>(PC, UGiveWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->AddToViewport(GiveStyle::ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	return Widget;
}

TSharedRef<SWidget> UGiveWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(GiveStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;

		UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Overlay->SetContent(Box);

		GiverText = GiveBuild::AddText(WidgetTree, Box);
		GiveBuild::AddButton(WidgetTree, Box, LOCTEXT("NextGiver", "Next giver"))->OnClicked.AddDynamic(this, &UGiveWidget::HandleNextGiverClicked);
		ItemText = GiveBuild::AddText(WidgetTree, Box);
		GiveBuild::AddButton(WidgetTree, Box, LOCTEXT("NextItem", "Next item"))->OnClicked.AddDynamic(this, &UGiveWidget::HandleNextItemClicked);
		ReceiverText = GiveBuild::AddText(WidgetTree, Box);
		GiveBuild::AddButton(WidgetTree, Box, LOCTEXT("NextReceiver", "Next receiver"))
			->OnClicked.AddDynamic(this, &UGiveWidget::HandleNextReceiverClicked);
		GiveOneButton = GiveBuild::AddButton(WidgetTree, Box, LOCTEXT("GiveOne", "Give"));
		GiveOneButton->OnClicked.AddDynamic(this, &UGiveWidget::HandleGiveOneClicked);
		GiveStackButton = GiveBuild::AddButton(WidgetTree, Box, LOCTEXT("GiveStack", "Give the stack"));
		GiveStackButton->OnClicked.AddDynamic(this, &UGiveWidget::HandleGiveStackClicked);
		RequestButton = GiveBuild::AddButton(WidgetTree, Box, LOCTEXT("Request", "Ask for it (the receiver asks the giver)"));
		RequestButton->OnClicked.AddDynamic(this, &UGiveWidget::HandleRequestClicked);
		TeachButton = GiveBuild::AddButton(WidgetTree, Box, LOCTEXT("Teach", "Teach (costs Mentor Tokens; you keep it)"));
		TeachButton->OnClicked.AddDynamic(this, &UGiveWidget::HandleTeachClicked);
		StatusLine = GiveBuild::AddText(WidgetTree, Box);
		GiveBuild::AddButton(WidgetTree, Box, LOCTEXT("Close", "Close"))->OnClicked.AddDynamic(this, &UGiveWidget::HandleCloseClicked);
	}
	return Super::RebuildWidget();
}

void UGiveWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}

UPoolSubsystem *UGiveWidget::GetPool() const
{
	const UGameInstance *GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
}

TArray<UGiveWidget::FMember> UGiveWidget::GatherMembers() const
{
	TArray<FMember> Members;
	const UGameInstance *GI = GetGameInstance();
	const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
	if (!Party)
	{
		return Members;
	}
	for (const FPartyMember &Member : Party->Members)
	{
		if (Member.CharacterData)
		{
			Members.Add({UPoolSubsystem::GetCharacterKey(Member), FText::FromString(Member.CharacterData->Name)});
		}
	}
	return Members;
}

FName UGiveWidget::GiverKey() const
{
	const TArray<FMember> Members = GatherMembers();
	return Members.IsValidIndex(GiverIndex) ? Members[GiverIndex].Key : NAME_None;
}

FName UGiveWidget::ReceiverKey() const
{
	const TArray<FMember> Members = GatherMembers();
	return Members.IsValidIndex(ReceiverIndex) ? Members[ReceiverIndex].Key : NAME_None;
}

TArray<UGiveWidget::FGiveChoice> UGiveWidget::GatherChoices() const
{
	TArray<FGiveChoice> Choices;
	const UPoolSubsystem *Pool = GetPool();
	const FName Giver = GiverKey();
	if (!Pool || Giver.IsNone())
	{
		return Choices;
	}
	for (const FWeaponInventoryEntry &Weapon : Pool->GetOwnedWeapons())
	{
		if (Pool->GetGearOwner(Weapon.PersistentID) == Giver && Weapon.Weapon)
		{
			FGiveChoice Choice;
			Choice.GearID = Weapon.PersistentID;
			Choice.Label = GiveBuild::GearLabel(Weapon.Weapon->Name, Weapon.Tier);
			Choices.Add(Choice);
		}
	}
	for (const FRingInventoryEntry &Ring : Pool->GetOwnedRings())
	{
		if (Pool->GetGearOwner(Ring.PersistentID) == Giver && Ring.Ring)
		{
			FGiveChoice Choice;
			Choice.GearID = Ring.PersistentID;
			Choice.Label = GiveBuild::GearLabel(Ring.Ring->Name, Ring.Tier);
			Choices.Add(Choice);
		}
	}
	for (const TPair<FCrystalId, int32> &Stack : Pool->GetCarriedItems(WoRGive::GetPouchKey(GetWorld(), Giver)))
	{
		FGiveChoice Choice;
		Choice.Item = Stack.Key;
		Choice.Count = Stack.Value;
		Choice.Label = FText::Format(LOCTEXT("StackLabel", "{0} x{1}"), FText::FromString(ItemIdentity::GetDisplayName(Stack.Key)), Stack.Value);
		Choices.Add(Choice);
	}
	for (const UObject *Skill : WoRGive::GetGiveableSkills(GetWorld(), Giver)) // B-316b
	{
		FGiveChoice Choice;
		Choice.Skill = Skill;
		// TODO: FText skill names with B-127 (D-061).
		const USpellData *Spell = Cast<USpellData>(Skill);
		const UAbilityData *Ability = Cast<UAbilityData>(Skill);
		Choice.Label = FText::FromString(Spell ? Spell->Name : Ability ? Ability->Name : FString());
		Choices.Add(Choice);
	}
	return Choices;
}

void UGiveWidget::Refresh()
{
	const TArray<FMember> Members = GatherMembers();
	GiverIndex = Members.IsEmpty() ? 0 : FMath::Clamp(GiverIndex, 0, Members.Num() - 1);
	ReceiverIndex = Members.IsEmpty() ? 0 : FMath::Clamp(ReceiverIndex, 0, Members.Num() - 1);
	const TArray<FGiveChoice> Choices = GatherChoices();
	SelectedIndex = Choices.IsEmpty() ? 0 : FMath::Clamp(SelectedIndex, 0, Choices.Num() - 1);

	if (Members.Num() < 2)
	{
		GiverText->SetText(LOCTEXT("NeedTwo", "Giving needs two characters in the party"));
		ReceiverText->SetText(FText::GetEmpty());
	}
	else
	{
		GiverText->SetText(FText::Format(LOCTEXT("GiverRow", "From: {0}"), Members[GiverIndex].Name));
		ReceiverText->SetText(FText::Format(LOCTEXT("ReceiverRow", "To: {0}"), Members[ReceiverIndex].Name));
	}
	if (Choices.IsEmpty())
	{
		ItemText->SetText(LOCTEXT("NothingToGive", "Nothing to give"));
	}
	else
	{
		const FGiveChoice &Choice = Choices[SelectedIndex];
		ItemText->SetText(FText::Format(LOCTEXT("ItemRow", "{0}: {1} ({2} of {3})"),
										Choice.IsGear() ? LOCTEXT("Gear", "Gear") : Choice.IsSkill() ? LOCTEXT("Skill", "Skill") : LOCTEXT("Pouch", "Pouch"),
										Choice.Label, SelectedIndex + 1,
										Choices.Num()));
	}
	const bool bCanGive = Members.Num() >= 2 && !Choices.IsEmpty() && GiverIndex != ReceiverIndex;
	GiveOneButton->SetIsEnabled(bCanGive);
	GiveStackButton->SetIsEnabled(bCanGive && !Choices[SelectedIndex].IsGear() && !Choices[SelectedIndex].IsSkill());
	RequestButton->SetIsEnabled(bCanGive && Choices[SelectedIndex].IsSkill());
	TeachButton->SetIsEnabled(bCanGive && Choices[SelectedIndex].IsSkill());
	StatusLine->SetText(StatusText);
}

FString UGiveWidget::GetGiveString() const
{
	FString Out = TEXT("[Give] ") + GiverText->GetText().ToString() + TEXT(" | ") + ItemText->GetText().ToString() + TEXT(" | ") +
				  ReceiverText->GetText().ToString();
	if (!StatusText.IsEmpty())
	{
		Out += TEXT("\n  ") + StatusText.ToString();
	}
	return Out;
}

void UGiveWidget::GiveSelected(int32 Count)
{
	const TArray<FGiveChoice> Choices = GatherChoices();
	if (!Choices.IsValidIndex(SelectedIndex))
	{
		return;
	}
	const FGiveChoice Choice = Choices[SelectedIndex];
	const FText Refusal = Choice.IsGear()	 ? WoRGive::TryGiveGear(GetWorld(), GiverKey(), ReceiverKey(), Choice.GearID)
						  : Choice.IsSkill() ? WoRGive::TryGiveSkill(GetWorld(), GiverKey(), ReceiverKey(), Choice.Skill.Get())
											 : WoRGive::TryGiveCarried(GetWorld(), GiverKey(), ReceiverKey(), Choice.Item, FMath::Min(Count, Choice.Count));
	StatusText = Refusal.IsEmpty() ? FText::Format(LOCTEXT("Given", "Gave {0}"), Choice.Label) : Refusal;
	Refresh();
	UE_LOG(LogTemp, Log, TEXT("%s"), *GetGiveString());
}

void UGiveWidget::HandleRequestClicked()
{
	const TArray<FGiveChoice> Choices = GatherChoices();
	if (!Choices.IsValidIndex(SelectedIndex) || !Choices[SelectedIndex].IsSkill())
	{
		return;
	}
	const FGiveChoice Choice = Choices[SelectedIndex];
	const FText Refusal = WoRGive::TryRequestSkill(GetWorld(), ReceiverKey(), GiverKey(), Choice.Skill.Get());
	StatusText = Refusal.IsEmpty() ? FText::Format(LOCTEXT("Requested", "Asked for {0}: given"), Choice.Label) : Refusal;
	Refresh();
	UE_LOG(LogTemp, Log, TEXT("%s"), *GetGiveString());
}

void UGiveWidget::HandleTeachClicked()
{
	const TArray<FGiveChoice> Choices = GatherChoices();
	if (!Choices.IsValidIndex(SelectedIndex) || !Choices[SelectedIndex].IsSkill())
	{
		return;
	}
	const FGiveChoice Choice = Choices[SelectedIndex];
	const FText Refusal = WoRGive::TryTeachSkill(GetWorld(), GiverKey(), ReceiverKey(), Choice.Skill.Get());
	StatusText = Refusal.IsEmpty() ? FText::Format(LOCTEXT("Taught", "Taught {0}"), Choice.Label) : Refusal;
	Refresh();
	UE_LOG(LogTemp, Log, TEXT("%s"), *GetGiveString());
}

void UGiveWidget::HandleNextGiverClicked()
{
	const int32 Count = GatherMembers().Num();
	GiverIndex = Count > 0 ? (GiverIndex + 1) % Count : 0;
	SelectedIndex = 0;
	StatusText = FText::GetEmpty();
	Refresh();
}

void UGiveWidget::HandleNextItemClicked()
{
	const int32 Count = GatherChoices().Num();
	SelectedIndex = Count > 0 ? (SelectedIndex + 1) % Count : 0;
	StatusText = FText::GetEmpty();
	Refresh();
}

void UGiveWidget::HandleNextReceiverClicked()
{
	const int32 Count = GatherMembers().Num();
	ReceiverIndex = Count > 0 ? (ReceiverIndex + 1) % Count : 0;
	StatusText = FText::GetEmpty();
	Refresh();
}

void UGiveWidget::HandleGiveOneClicked() { GiveSelected(1); }

void UGiveWidget::HandleGiveStackClicked() { GiveSelected(MAX_int32); }

void UGiveWidget::HandleCloseClicked()
{
	if (APlayerController *PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

static FAutoConsoleCommandWithWorld GGiveUICommand(
	TEXT("wor.UI.Give"),
	TEXT("PLACEHOLDER UI: give a weapon, adapter or pouch stack from one of your characters to another (B-323)."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld *World)
		{
			if (!UGiveWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
			{
				UE_LOG(LogTemp, Warning, TEXT("[Give] wor.UI.Give: no player controller"));
			}
		}));

#undef LOCTEXT_NAMESPACE
