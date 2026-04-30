// CombatCommandMenuWidget.cpp
// World of Refraction - Combat UI

#include "UI/Combat/CommandMenu/CombatCommandMenuWidget.h"
#include "UI/Combat/CommandMenu/CombatCommandButtonWidget.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "UI/Combat/PieMenuButtonData.h"
#include "Components/PanelWidget.h"
#include "Engine/GameInstance.h"

void UCombatCommandMenuWidget::NativeConstruct()
{
    SetVisibility(ESlateVisibility::Hidden);
    Super::NativeConstruct();

    ClearFlags(RF_Transactional);

    SpawnButtonPool();
}

void UCombatCommandMenuWidget::SpawnButtonPool()
{
    if (!ButtonContainer)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCommandMenu] SpawnButtonPool: ButtonContainer is null. Check BindWidget in BP child."));
        return;
    }

    if (!ButtonWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCommandMenu] SpawnButtonPool: ButtonWidgetClass not set. Check Class Defaults in BP child."));
        return;
    }

    ButtonPool.Reserve(PoolSize);

    for (int32 i = 0; i < PoolSize; ++i)
    {
        UCombatCommandButtonWidget *Button = CreateWidget<UCombatCommandButtonWidget>(this, ButtonWidgetClass);
        if (!Button)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CombatCommandMenu] SpawnButtonPool: CreateWidget failed at index %d"), i);
            continue;
        }

        ButtonContainer->AddChild(Button);
        Button->Hide();
        ButtonPool.Add(Button);
    }

    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] Pool spawned with %d buttons"), ButtonPool.Num());
}

void UCombatCommandMenuWidget::InitialiseForCombat()
{
    if (bIsInitialised)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[CombatCommandMenu] InitialiseForCombat called twice — ignoring"));
        return;
    }

    UGameInstance *GI = GetGameInstance();
    if (!GI)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCommandMenu] InitialiseForCombat: no GameInstance"));
        return;
    }

    UCombatCommandMenuSubsystem *Subsystem = GI->GetSubsystem<UCombatCommandMenuSubsystem>();
    if (!Subsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCommandMenu] InitialiseForCombat: subsystem not found"));
        return;
    }

    Subsystem->OnCommandMenuReady.AddDynamic(this, &UCombatCommandMenuWidget::HandleCommandMenuReady);
    Subsystem->OnCommandMenuClosed.AddDynamic(this, &UCombatCommandMenuWidget::HandleCommandMenuClosed);

    CachedSubsystem = Subsystem;
    bIsInitialised = true;

    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] Initialised — bound to subsystem"));

    // Replay current state — handles the case where the subsystem broadcast
    // before this widget was constructed (e.g., menu created mid-turn).
    if (Subsystem->IsOpen())
    {
        Subsystem->RefreshMenu();
    }
}

void UCombatCommandMenuWidget::ShutdownForCombat()
{
    UnbindFromSubsystem();
}

void UCombatCommandMenuWidget::HandleCommandMenuReady(const TArray<FPieMenuButtonData> &Buttons)
{
    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] HandleCommandMenuReady — received %d buttons"), Buttons.Num());
    SetVisibility(ESlateVisibility::Hidden);
    const int32 ActiveCount = FMath::Min(Buttons.Num(), ButtonPool.Num());

    for (int32 i = 0; i < ButtonPool.Num(); ++i)
    {
        UCombatCommandButtonWidget *Button = ButtonPool[i];
        if (!Button)
        {
            continue;
        }

        if (i < ActiveCount)
        {
            Button->SetData(Buttons[i]);
            Button->Show();
        }
        else
        {
            Button->Hide();
        }
    }

    if (Buttons.Num() > ButtonPool.Num())
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCommandMenu] %d buttons requested but pool size is %d — extras ignored"),
               Buttons.Num(), ButtonPool.Num());
    }

    OnButtonsRefreshed(ActiveCount);
}

void UCombatCommandMenuWidget::HandleCommandMenuClosed()
{

    for (UCombatCommandButtonWidget *Button : ButtonPool)
    {
        if (Button)
        {
            Button->Hide();
        }
    }

    OnButtonsRefreshed(0);
    SetVisibility(ESlateVisibility::Hidden);
}

void UCombatCommandMenuWidget::UnbindFromSubsystem()
{
    if (!bIsInitialised)
    {
        return;
    }

    if (UCombatCommandMenuSubsystem *Subsystem = CachedSubsystem.Get())
    {
        Subsystem->OnCommandMenuReady.RemoveDynamic(this, &UCombatCommandMenuWidget::HandleCommandMenuReady);
        Subsystem->OnCommandMenuClosed.RemoveDynamic(this, &UCombatCommandMenuWidget::HandleCommandMenuClosed);
    }

    CachedSubsystem.Reset();
    bIsInitialised = false;
}

void UCombatCommandMenuWidget::BeginDestroy()
{
    UnbindFromSubsystem();
    Super::BeginDestroy();
}

void UCombatCommandMenuWidget::DebugLogPoolState() const
{
    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] === Pool State ==="));
    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] PoolSize: %d, Spawned: %d, Initialised: %d"),
           PoolSize, ButtonPool.Num(), bIsInitialised);
    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] Subsystem cached: %s"),
           CachedSubsystem.IsValid() ? TEXT("yes") : TEXT("no"));
    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] ButtonContainer bound: %s"),
           ButtonContainer ? TEXT("yes") : TEXT("no"));

    for (int32 i = 0; i < ButtonPool.Num(); ++i)
    {
        UCombatCommandButtonWidget *Btn = ButtonPool[i];
        UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu]   [%d] %s"),
               i,
               Btn ? TEXT("valid") : TEXT("null"));
    }
}