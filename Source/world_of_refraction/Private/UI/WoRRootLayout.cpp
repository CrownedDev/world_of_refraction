// WoRRootLayout.cpp

#include "UI/WoRRootLayout.h"

#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

UCommonActivatableWidget *UWoRRootLayout::PushToMenu(TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	if (!MenuLayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WoRRootLayout] PushToMenu with no MenuLayer - check the WBP's BindWidget names"));
		return nullptr;
	}

	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WoRRootLayout] PushToMenu with a null widget class"));
		return nullptr;
	}

	// AddWidget is a template whose default parameter is UCommonActivatableWidget;
	// spelled out here so the return type is obvious at the call site. The stack
	// activates the pushed widget and deactivates whatever was beneath it.
	return MenuLayer->AddWidget<UCommonActivatableWidget>(WidgetClass);
}

void UWoRRootLayout::PopMenu()
{
	if (!MenuLayer)
	{
		return;
	}

	UCommonActivatableWidget *Active = MenuLayer->GetActiveWidget();
	if (!Active)
	{
		return;
	}

	// ⚠️ RemoveWidget takes a REFERENCE, not a pointer. The pointer overload
	// exists but is PRIVATE (BlueprintCallable only) — passing Active directly
	// resolves to it and fails to compile with an accessibility error that reads
	// nothing like the actual problem. Dereference deliberately.
	MenuLayer->RemoveWidget(*Active);
}
