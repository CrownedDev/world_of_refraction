// CombatCommandButtonWidget.cpp
// World of Refraction - Combat UI

#include "UI/Combat/CommandMenu/CombatCommandButtonWidget.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

void UCombatCommandButtonWidget::NativeConstruct()
{
    Super::NativeConstruct();

    ClearFlags(RF_Transactional);

    if (CommandButton)
    {
        CommandButton->OnClicked.AddDynamic(this, &UCombatCommandButtonWidget::HandleClicked);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCommandButton] NativeConstruct: CommandButton is null. Check BindWidget in BP child."));
    }
}

void UCombatCommandButtonWidget::NativeDestruct()
{
    if (CommandButton)
    {
        CommandButton->OnClicked.RemoveDynamic(this, &UCombatCommandButtonWidget::HandleClicked);
    }

    Super::NativeDestruct();
}

void UCombatCommandButtonWidget::SetData(const FPieMenuButtonData &InData)
{
    CurrentData = InData;
    bHasData = true;

    if (LabelText)
    {
        LabelText->SetText(InData.DisplayName);
    }

    if (CommandButton)
    {
        CommandButton->SetIsEnabled(InData.bEnabled);
    }

    OnDataChanged(InData);
}

void UCombatCommandButtonWidget::Show()
{
    SetVisibility(ESlateVisibility::Visible);
}

void UCombatCommandButtonWidget::Hide()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

void UCombatCommandButtonWidget::HandleClicked()
{
    if (!bHasData)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCommandButton] HandleClicked: no data set — ignoring"));
        return;
    }

    UGameInstance *GI = GetGameInstance();
    if (!GI)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCommandButton] HandleClicked: no GameInstance"));
        return;
    }

    UCombatCommandMenuSubsystem *Subsystem = GI->GetSubsystem<UCombatCommandMenuSubsystem>();
    if (!Subsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCommandButton] HandleClicked: subsystem not found"));
        return;
    }

    Subsystem->HandleSelection(CurrentData);
}

void UCombatCommandButtonWidget::DebugLogData() const
{
    UE_LOG(LogTemp, Log, TEXT("[CombatCommandButton] === Data ==="));
    UE_LOG(LogTemp, Log, TEXT("[CombatCommandButton] HasData: %s"), bHasData ? TEXT("yes") : TEXT("no"));
    UE_LOG(LogTemp, Log, TEXT("[CombatCommandButton] DisplayName: %s"), *CurrentData.DisplayName.ToString());
    UE_LOG(LogTemp, Log, TEXT("[CombatCommandButton] ButtonID: %s"), *CurrentData.ButtonID);
    UE_LOG(LogTemp, Log, TEXT("[CombatCommandButton] Enabled: %s"), CurrentData.bEnabled ? TEXT("yes") : TEXT("no"));
}