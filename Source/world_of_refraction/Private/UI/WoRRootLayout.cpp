// WoRRootLayout.cpp

#include "UI/WoRRootLayout.h"

#include "CommonActivatableWidget.h"
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
		UE_LOG(LogTemp, Warning, TEXT("[WoRRootLayout] Push to %s with no stack - check the WBP's BindWidget names"),
			   *UEnum::GetValueAsString(Layer));
		return nullptr;
	}
	if (Layer == EWoRUILayer::Modal && !ModalLayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WoRRootLayout] No ModalLayer in the WBP - modal push goes to MenuLayer"));
	}

	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WoRRootLayout] PushToMenu with a null widget class"));
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
