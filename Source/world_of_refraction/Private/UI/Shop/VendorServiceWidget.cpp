// VendorServiceWidget.cpp

#include "UI/Shop/VendorServiceWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Character/CharacterData.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "VendorServiceWidget"

namespace VendorServiceStyle
{
	// TODO: style pass (placeholder until the vendor screen, B-456).
	const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.7f);
	constexpr int32 FontSize = 22;
	constexpr float RowPadding = 6.0f;
	constexpr int32 ZOrder = 50;

	UTextBlock *MakeText(UWidgetTree *Tree)
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetAutoWrapText(true);
		return Text;
	}

	UTextBlock *AddText(UWidgetTree *Tree, UVerticalBox *Box)
	{
		UTextBlock *Text = MakeText(Tree);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Text))
		{
			BoxSlot->SetPadding(FMargin(RowPadding));
		}
		return Text;
	}

	UButton *AddButton(UWidgetTree *Tree, UVerticalBox *Box, const FText &Label, UTextBlock **OutLabel = nullptr)
	{
		UButton *Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock *Text = MakeText(Tree);
		Text->SetText(Label);
		Button->AddChild(Text);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Button))
		{
			BoxSlot->SetPadding(FMargin(RowPadding));
		}
		if (OutLabel)
		{
			*OutLabel = Text;
		}
		return Button;
	}
}

UVendorServiceWidget *UVendorServiceWidget::Open(APlayerController *PC, EMerchantType InVendor)
{
	if (!PC)
	{
		return nullptr;
	}
	UVendorServiceWidget *Widget = CreateWidget<UVendorServiceWidget>(PC, UVendorServiceWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->Vendor = InVendor;
	Widget->AddToViewport(VendorServiceStyle::ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	return Widget;
}

TSharedRef<SWidget> UVendorServiceWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(VendorServiceStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;
		UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Overlay->SetContent(Box);

		TitleText = VendorServiceStyle::AddText(WidgetTree, Box);
		VendorServiceStyle::AddButton(WidgetTree, Box, LOCTEXT("NextService", "Next service"))
			->OnClicked.AddDynamic(this, &UVendorServiceWidget::HandleNextServiceClicked);
		MemberText = VendorServiceStyle::AddText(WidgetTree, Box);
		VendorServiceStyle::AddButton(WidgetTree, Box, LOCTEXT("NextMember", "Next character"))
			->OnClicked.AddDynamic(this, &UVendorServiceWidget::HandleNextMemberClicked);
		ItemText = VendorServiceStyle::AddText(WidgetTree, Box);
		VendorServiceStyle::AddButton(WidgetTree, Box, LOCTEXT("NextItem", "Next item"))
			->OnClicked.AddDynamic(this, &UVendorServiceWidget::HandleNextItemClicked);
		UTextBlock *Label = nullptr;
		ActButton = VendorServiceStyle::AddButton(WidgetTree, Box, FText::GetEmpty(), &Label);
		ActLabel = Label;
		ActButton->OnClicked.AddDynamic(this, &UVendorServiceWidget::HandleActClicked);
		StatusLine = VendorServiceStyle::AddText(WidgetTree, Box);
		VendorServiceStyle::AddButton(WidgetTree, Box, LOCTEXT("Close", "Close"))->OnClicked.AddDynamic(this, &UVendorServiceWidget::HandleCloseClicked);
	}
	return Super::RebuildWidget();
}

void UVendorServiceWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}

TArray<EVendorService> UVendorServiceWidget::GatherServices() const
{
	TArray<EVendorService> Services;
	for (const EVendorService Service : {EVendorService::Sell, EVendorService::Dismantle})
	{
		if (WoRVendorServices::Offers(Vendor, Service))
		{
			Services.Add(Service);
		}
	}
	return Services;
}

TArray<FName> UVendorServiceWidget::GatherMembers(TArray<FText> &OutNames) const
{
	TArray<FName> Keys;
	const UGameInstance *GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
	if (Party)
	{
		for (const FPartyMember &Member : Party->Members)
		{
			if (Member.CharacterData)
			{
				Keys.Add(UPoolSubsystem::GetCharacterKey(Member));
				OutNames.Add(FText::FromString(Member.CharacterData->Name));
			}
		}
	}
	return Keys;
}

void UVendorServiceWidget::Refresh()
{
	const TArray<EVendorService> Services = GatherServices();
	TArray<FText> Names;
	const TArray<FName> Members = GatherMembers(Names);
	ServiceIndex = Services.IsEmpty() ? 0 : ServiceIndex % Services.Num();
	MemberIndex = Members.IsEmpty() ? 0 : MemberIndex % Members.Num();
	const bool bReady = !Services.IsEmpty() && !Members.IsEmpty();
	const EVendorService Service = bReady ? Services[ServiceIndex] : EVendorService::Sell;
	const TArray<WoRVendorServices::FVendorRow> Rows =
		bReady ? WoRVendorServices::GatherRows(GetWorld(), Vendor, Service, Members[MemberIndex]) : TArray<WoRVendorServices::FVendorRow>();
	ItemIndex = Rows.IsEmpty() ? 0 : ItemIndex % Rows.Num();

	if (!TitleText || !MemberText || !ItemText || !ActLabel || !ActButton || !StatusLine)
	{
		return;
	}
	const FText VendorName = WoRGameNames::GetMerchantTypeName(Vendor);
	TitleText->SetText(Services.IsEmpty() ? FText::Format(LOCTEXT("NoServices", "The {0} neither buys nor dismantles."), VendorName)
										  : FText::Format(LOCTEXT("Title", "{0}: {1}"), VendorName, WoRGameNames::GetVendorServiceName(Service)));
	MemberText->SetText(Members.IsEmpty() ? LOCTEXT("NoParty", "There is no party.") : FText::Format(LOCTEXT("Member", "For: {0}"), Names[MemberIndex]));
	if (Rows.IsEmpty())
	{
		ItemText->SetText(LOCTEXT("Nothing", "Nothing of theirs for this."));
	}
	else
	{
		const WoRVendorServices::FVendorRow &Row = Rows[ItemIndex];
		const FText Refusal = WoRVendorServices::GetRefusal(GetWorld(), Vendor, Service, Members[MemberIndex], Row);
		ItemText->SetText(FText::Format(LOCTEXT("Item", "{0} of {1}: {2} ({3})"), FText::AsNumber(ItemIndex + 1), FText::AsNumber(Rows.Num()), Row.Label,
										Refusal.IsEmpty() ? WoRVendorServices::GetPayText(GetWorld(), Service, Row) : Refusal));
	}
	ActLabel->SetText(bConfirming ? LOCTEXT("Confirm", "Confirm") : WoRGameNames::GetVendorServiceName(Service));
	ActButton->SetIsEnabled(!Rows.IsEmpty());
	StatusLine->SetText(Status);
}

FString UVendorServiceWidget::GetVendorServiceString() const
{
	return FString::Printf(TEXT("[VendorService] %s | %s | %s | %s"), TitleText ? *TitleText->GetText().ToString() : TEXT("-"),
						   MemberText ? *MemberText->GetText().ToString() : TEXT("-"), ItemText ? *ItemText->GetText().ToString() : TEXT("-"),
						   StatusLine ? *StatusLine->GetText().ToString() : TEXT("-"));
}

void UVendorServiceWidget::HandleNextServiceClicked()
{
	++ServiceIndex;
	ItemIndex = 0;
	bConfirming = false;
	Status = FText::GetEmpty();
	Refresh();
}

void UVendorServiceWidget::HandleNextMemberClicked()
{
	++MemberIndex;
	ItemIndex = 0;
	bConfirming = false;
	Status = FText::GetEmpty();
	Refresh();
}

void UVendorServiceWidget::HandleNextItemClicked()
{
	++ItemIndex;
	bConfirming = false;
	Status = FText::GetEmpty();
	Refresh();
}

void UVendorServiceWidget::HandleActClicked()
{
	const TArray<EVendorService> Services = GatherServices();
	TArray<FText> Names;
	const TArray<FName> Members = GatherMembers(Names);
	if (!Services.IsValidIndex(ServiceIndex) || !Members.IsValidIndex(MemberIndex))
	{
		return;
	}
	const EVendorService Service = Services[ServiceIndex];
	const FName Member = Members[MemberIndex];
	const TArray<WoRVendorServices::FVendorRow> Rows = WoRVendorServices::GatherRows(GetWorld(), Vendor, Service, Member);
	if (!Rows.IsValidIndex(ItemIndex))
	{
		return;
	}
	const WoRVendorServices::FVendorRow Row = Rows[ItemIndex];
	const FText Refusal = WoRVendorServices::GetRefusal(GetWorld(), Vendor, Service, Member, Row);
	if (!Refusal.IsEmpty())
	{
		bConfirming = false;
		Status = Refusal;
	}
	else if (!bConfirming)
	{
		// D-016: ask first; the second press does it.
		bConfirming = true;
		Status = WoRVendorServices::GetConfirmText(GetWorld(), Service, Member, Row);
	}
	else
	{
		bConfirming = false;
		const FText Pay = WoRVendorServices::GetPayText(GetWorld(), Service, Row);
		const FText Failure = WoRVendorServices::Run(GetWorld(), Vendor, Service, Member, Row, /*Count=*/1);
		Status = Failure.IsEmpty() ? FText::Format(LOCTEXT("Done", "Done: {0} for {1}."), Row.Label, Pay) : Failure;
	}
	Refresh();
	UE_LOG(LogWoREconomy, Log, TEXT("%s"), *GetVendorServiceString());
}

void UVendorServiceWidget::HandleCloseClicked()
{
	if (APlayerController *PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs GVendorServiceUICommand(
	TEXT("wor.UI.Vendor"),
	TEXT("PLACEHOLDER UI (B-445): a vendor's Sell and Dismantle. Usage: wor.UI.Vendor [Blacksmith|Jeweler|Spiritualist] (default Blacksmith)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
	{
		EMerchantType Vendor = EMerchantType::Blacksmith;
		if (Args.Num() > 0)
		{
			const int64 Value = StaticEnum<EMerchantType>()->GetValueByNameString(Args[0]);
			if (Value == INDEX_NONE)
			{
				UE_LOG(LogWoREconomy, Warning, TEXT("[VendorService] Unknown vendor '%s' - use Blacksmith, Jeweler or Spiritualist"), *Args[0]);
				return;
			}
			Vendor = static_cast<EMerchantType>(Value);
		}
		if (!UVendorServiceWidget::Open(World ? World->GetFirstPlayerController() : nullptr, Vendor))
		{
			UE_LOG(LogWoREconomy, Warning, TEXT("[VendorService] wor.UI.Vendor: no player controller"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
