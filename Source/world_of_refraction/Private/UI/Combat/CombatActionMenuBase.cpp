#include "UI/Combat/CombatActionMenuBase.h"

void UCombatActionMenuBase::NativeConstruct()
{
    Super::NativeConstruct();
    ClearFlags(RF_Transactional);
    if (WidgetTree)
    {
        WidgetTree->ClearFlags(RF_Transactional);
        WidgetTree->ForEachWidget([](UWidget *Widget)
                                  {
            if (Widget) Widget->ClearFlags(RF_Transactional); });
    }
}

void UCombatActionMenuBase::NativeDestruct()
{
    UE_LOG(LogTemp, Warning, TEXT("[CombatActionMenuBase] NativeDestruct fired"));
    ClearFlags(RF_Transactional);
    if (WidgetTree)
    {
        WidgetTree->ClearFlags(RF_Transactional);
        WidgetTree->ForEachWidget([](UWidget *Widget)
                                  {
            if (Widget) Widget->ClearFlags(RF_Transactional); });
    }
    Super::NativeDestruct();
}