// WoRTestScreen.cpp

#include "UI/WoRTestScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Player/WoRHubController.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRTestScreen"

namespace WoRTestScreenStyle
{
	// TODO: style pass (placeholder until the real screens).
	const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.7f);
	constexpr int32 FontSize = 24;
}

UWoRTestScreen::UWoRTestScreen(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true; // Back deactivates it, and its stack removes it
}

TSharedRef<SWidget> UWoRTestScreen::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(WoRTestScreenStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;

		UTextBlock *Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Hint"));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = WoRTestScreenStyle::FontSize;
		Text->SetFont(Font);
		Text->SetText(LOCTEXT("Hint", "Test screen: press Back (Esc or gamepad B) to close"));
		Overlay->SetContent(Text);
	}
	return Super::RebuildWidget();
}

void UWoRTestScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	UE_LOG(LogWoRUI, Log, TEXT("[WoRTestScreen] %s activated"), *GetName());
}

void UWoRTestScreen::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
	UE_LOG(LogWoRUI, Log, TEXT("[WoRTestScreen] %s deactivated"), *GetName());
}

static FAutoConsoleCommandWithWorldAndArgs GWoRTestScreenCommand(
	TEXT("wor.UI.TestScreen"),
	TEXT("PLACEHOLDER (B-103): push the empty test screen onto a root layout layer. Usage: wor.UI.TestScreen [Game|Menu|Modal] (default Menu); Back closes it."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString> &Args, UWorld *World)
		{
			AWoRHubController *Hub = World ? Cast<AWoRHubController>(World->GetFirstPlayerController()) : nullptr;
			UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
			if (!Root)
			{
				UE_LOG(LogWoRUI, Warning, TEXT("[WoRTestScreen] wor.UI.TestScreen: no AWoRHubController with a root layout"));
				return;
			}
			EWoRUILayer Layer = EWoRUILayer::Menu;
			if (Args.Num() > 0)
			{
				const int64 Value = StaticEnum<EWoRUILayer>()->GetValueByNameString(Args[0]);
				if (Value == INDEX_NONE)
				{
					UE_LOG(LogWoRUI, Warning, TEXT("[WoRTestScreen] Unknown layer '%s' - use Game, Menu or Modal"), *Args[0]);
					return;
				}
				Layer = static_cast<EWoRUILayer>(Value);
			}
			Root->PushToLayer(Layer, UWoRTestScreen::StaticClass());
			UE_LOG(LogWoRUI, Log, TEXT("%s"), *Root->GetRootLayoutString());
		}));

#undef LOCTEXT_NAMESPACE
