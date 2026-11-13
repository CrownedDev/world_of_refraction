// WoRRootLayout.cpp

#include "UI/WoRRootLayout.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "CommonActivatableWidget.h"
#include "Input/CommonUIActionRouterBase.h"
#include "UI/Hub/HubHUDWidget.h"
#include "UI/Party/PartyDisplayWidget.h"
#include "WoRLog.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

UCommonActivatableWidget *UWoRRootLayout::PushToMenu(TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	return PushToLayer(EWoRUILayer::Menu, WidgetClass);
}

void UWoRRootLayout::PopMenu()
{
	PopLayer(EWoRUILayer::Menu);
}

UCommonActivatableWidgetStack *UWoRRootLayout::GetLayerStack(EWoRUILayer Layer) const
{
	switch (Layer)
	{
	case EWoRUILayer::Game:
		return GameLayer;
	case EWoRUILayer::Modal:
		return ModalLayer ? ModalLayer : MenuLayer;
	case EWoRUILayer::Menu:
	default:
		return MenuLayer;
	}
}

UCommonActivatableWidget *UWoRRootLayout::PushToLayer(EWoRUILayer Layer, TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	UCommonActivatableWidgetStack *Stack = GetLayerStack(Layer);
	if (!Stack)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[WoRRootLayout] Push to %s with no stack - check the WBP's BindWidget names"),
			   *UEnum::GetValueAsString(Layer));
		return nullptr;
	}
	if (Layer == EWoRUILayer::Modal && !ModalLayer)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[WoRRootLayout] No ModalLayer in the WBP - modal push goes to MenuLayer"));
	}

	if (!WidgetClass)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[WoRRootLayout] Push to %s with a null widget class"), *UEnum::GetValueAsString(Layer));
		return nullptr;
	}

	// AddWidget is a template whose default parameter is UCommonActivatableWidget;
	// spelled out here so the return type is obvious at the call site. The stack
	// activates the pushed widget and deactivates whatever was beneath it.
	return Stack->AddWidget<UCommonActivatableWidget>(WidgetClass);
}

void UWoRRootLayout::PopLayer(EWoRUILayer Layer)
{
	UCommonActivatableWidgetStack *Stack = GetLayerStack(Layer);
	if (!Stack)
	{
		return;
	}

	UCommonActivatableWidget *Active = Stack->GetActiveWidget();
	if (!Active)
	{
		return;
	}

	// ⚠️ RemoveWidget takes a REFERENCE, not a pointer. The pointer overload
	// exists but is PRIVATE (BlueprintCallable only) — passing Active directly
	// resolves to it and fails to compile with an accessibility error that reads
	// nothing like the actual problem. Dereference deliberately.
	Stack->RemoveWidget(*Active);
}

void UWoRRootLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// Menu and modal only: the game layer is the HUD's (it will state its own Game config when it moves there).
	for (UCommonActivatableWidgetStack *Stack : {MenuLayer, ModalLayer})
	{
		if (Stack)
		{
			Stack->OnDisplayedWidgetChanged().AddUObject(this, &UWoRRootLayout::HandleDisplayedWidgetChanged);
		}
	}
}

bool UWoRRootLayout::HasActiveScreen() const
{
	for (const UCommonActivatableWidgetStack *Stack : {MenuLayer, ModalLayer})
	{
		if (Stack && Stack->GetActiveWidget())
		{
			return true;
		}
	}
	return false;
}

void UWoRRootLayout::SetPartyStripHidden(bool bHidden)
{
	if (!bHidden)
	{
		for (const TPair<TWeakObjectPtr<UWidget>, ESlateVisibility> &Hidden : HiddenStrips)
		{
			if (UWidget *Strip = Hidden.Key.Get())
			{
				Strip->SetVisibility(Hidden.Value);
			}
		}
		HiddenStrips.Reset();
		return;
	}
	if (HiddenStrips.Num() > 0 || !GetWorld())
	{
		return;
	}
	// The viewport HUD that sits above this layout: the old party strip and the hub HUD (B-443).
	for (const TSubclassOf<UUserWidget> HudClass : {TSubclassOf<UUserWidget>(UPartyDisplayWidget::StaticClass()), TSubclassOf<UUserWidget>(UHubHUDWidget::StaticClass())})
	{
		TArray<UUserWidget *> Strips;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Strips, HudClass, /*TopLevelOnly=*/false);
		for (UUserWidget *Strip : Strips)
		{
			HiddenStrips.Emplace(Strip, Strip->GetVisibility());
			Strip->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UWoRRootLayout::HandleDisplayedWidgetChanged(UCommonActivatableWidget *Displayed)
{
	const bool bAnyScreen = Displayed || HasActiveScreen();
	SetPartyStripHidden(bAnyScreen);
	if (bAnyScreen)
	{
		return;
	}
	if (UCommonUIActionRouterBase *Router = UCommonUIActionRouterBase::Get(*this))
	{
		Router->SetActiveUIInputConfig(FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently), this);
		UE_LOG(LogWoRUI, Log, TEXT("[WoRRootLayout] Every screen closed: input back to the game"));
	}
	if (WhenAllClosed)
	{
		const TFunction<void()> Action = MoveTemp(WhenAllClosed);
		WhenAllClosed = nullptr;
		Action();
	}
}

void UWoRRootLayout::RunWhenAllClosed(TFunction<void()> Action)
{
	if (!HasActiveScreen())
	{
		Action();
		return;
	}
	if (WhenAllClosed)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[WoRRootLayout] RunWhenAllClosed: an action was already waiting and is dropped"));
	}
	WhenAllClosed = MoveTemp(Action);
}

void UWoRRootLayout::NativeDestruct()
{
	WhenAllClosed = nullptr; // a waiting action dies with the layout
	Super::NativeDestruct();
}

FString UWoRRootLayout::GetRootLayoutString() const
{
	FString Out = TEXT("[WoRRootLayout]");
	for (const EWoRUILayer Layer : {EWoRUILayer::Game, EWoRUILayer::Menu, EWoRUILayer::Modal})
	{
		const UCommonActivatableWidgetStack *Stack = GetLayerStack(Layer);
		const UCommonActivatableWidget *Active = Stack ? Stack->GetActiveWidget() : nullptr;
		Out += FString::Printf(TEXT("\n  %s: %s, %d widget(s), active %s"), *UEnum::GetValueAsString(Layer),
							   Stack ? *Stack->GetName() : TEXT("unbound"), Stack ? Stack->GetNumWidgets() : 0,
							   *GetNameSafe(Active));
	}
	return Out;
}
