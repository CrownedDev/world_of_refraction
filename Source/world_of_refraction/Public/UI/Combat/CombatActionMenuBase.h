#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CombatActionMenuBase.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UCombatActionMenuBase : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
};