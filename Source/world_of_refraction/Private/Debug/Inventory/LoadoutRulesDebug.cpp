// LoadoutRulesDebug.cpp

#include "Debug/Inventory/LoadoutRulesDebug.h"
#include "Loadout/LoadoutComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace
{
	constexpr float LOADOUT_RULES_SCREEN_SECONDS = 10.0f;

	FAutoConsoleCommandWithWorld GCompareLoadoutRulesCmd(
		TEXT("wor.Loadout.CompareRules"),
		TEXT("DEBUG: per character, what the merged loadout check (B-028) reports that the old one didn't, and the reverse."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				ULoadoutRulesDebug::CompareLoadoutRules(World);
			}));

	FString JoinOrNone(const TArray<FString> &Lines)
	{
		return Lines.Num() > 0 ? FString::Join(Lines, TEXT("; ")) : FString(TEXT("none"));
	}
}

FString ULoadoutRulesDebug::GetCompareRulesString(const ULoadoutComponent *Loadout)
{
	const AActor *Owner = Loadout ? Loadout->GetOwner() : nullptr;
	UInventoryComponent *Inventory = Owner ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Inventory)
	{
		return FString::Printf(TEXT("%s: no inventory"), *GetNameSafe(Owner));
	}

	const int32 Index = Inventory->ActiveLoadoutIndex;
	const TArray<FString> Merged = Loadout->GetValidationErrors(Index, Inventory);
	const TArray<FString> Old = Loadout->GetValidationErrorsLegacy(Index, Inventory);
	TArray<FString> Added;
	TArray<FString> Gone;
	for (const FString &Reason : Merged)
	{
		if (!Old.Contains(Reason))
		{
			Added.Add(Reason);
		}
	}
	for (const FString &Reason : Old)
	{
		if (!Merged.Contains(Reason))
		{
			Gone.Add(Reason);
		}
	}
	return FString::Printf(TEXT("%s loadout %d: %d findings (old %d) | new: %s | gone: %s"),
						   *GetNameSafe(Owner), Index, Merged.Num(), Old.Num(), *JoinOrNone(Added), *JoinOrNone(Gone));
}

void ULoadoutRulesDebug::CompareLoadoutRules(const UObject *WorldContextObject)
{
	UWorld *World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return;
	}
	int32 Count = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (const ULoadoutComponent *Loadout = It->FindComponentByClass<ULoadoutComponent>())
		{
			const FString Line = GetCompareRulesString(Loadout);
			UE_LOG(LogTemp, Display, TEXT("[LoadoutRules] %s"), *Line);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(INDEX_NONE, LOADOUT_RULES_SCREEN_SECONDS, FColor::Cyan, Line);
			}
			++Count;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("[LoadoutRules] Compared %d characters"), Count);
}
