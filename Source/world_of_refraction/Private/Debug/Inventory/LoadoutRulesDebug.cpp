// LoadoutRulesDebug.cpp

#include "Debug/Inventory/LoadoutRulesDebug.h"
#include "Loadout/LoadoutComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

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

	FAutoConsoleCommandWithWorldAndArgs GTryAbilityPickCmd(
		TEXT("wor.Loadout.TryAbility"),
		TEXT("DEBUG: wor.Loadout.TryAbility <ability name> - try a learned ability as a pick on each character's active loadout (B-159)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				if (!World || Args.Num() == 0)
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[LoadoutRules] usage: wor.Loadout.TryAbility <ability name>"));
					return;
				}
				const FString AbilityName = FString::Join(Args, TEXT(" "));
				for (TActorIterator<AActor> It(World); It; ++It)
				{
					if (ULoadoutComponent *Loadout = It->FindComponentByClass<ULoadoutComponent>())
					{
						const FString Line = ULoadoutRulesDebug::TryAbilityPick(Loadout, AbilityName);
						UE_LOG(LogWoRLoadout, Display, TEXT("[LoadoutRules] %s"), *Line);
						if (GEngine)
						{
							GEngine->AddOnScreenDebugMessage(INDEX_NONE, LOADOUT_RULES_SCREEN_SECONDS, FColor::Cyan, Line);
						}
					}
				}
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
			UE_LOG(LogWoRLoadout, Display, TEXT("[LoadoutRules] %s"), *Line);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(INDEX_NONE, LOADOUT_RULES_SCREEN_SECONDS, FColor::Cyan, Line);
			}
			++Count;
		}
	}
	UE_LOG(LogWoRLoadout, Display, TEXT("[LoadoutRules] Compared %d characters"), Count);
}

FString ULoadoutRulesDebug::TryAbilityPick(ULoadoutComponent *Loadout, const FString &AbilityName)
{
	AActor *Owner = Loadout ? Loadout->GetOwner() : nullptr;
	UInventoryComponent *Inventory = Owner ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Inventory)
	{
		return FString::Printf(TEXT("%s: no inventory"), *GetNameSafe(Owner));
	}
	const FAbilityInstance *Learned = Inventory->Abilities.LearnedAbilities.FindByPredicate(
		[&AbilityName](const FAbilityInstance &Instance) { return Instance.Ability && Instance.Ability->Name == AbilityName; });
	if (!Learned)
	{
		return FString::Printf(TEXT("%s: hasn't learned '%s'"), *GetNameSafe(Owner), *AbilityName);
	}
	FText Refusal;
	const bool bAssigned = Loadout->TryAssignWeaponAbility(Inventory->ActiveLoadoutIndex, Learned->Ability, Refusal);
	return bAssigned
		? FString::Printf(TEXT("%s: '%s' assigned"), *GetNameSafe(Owner), *AbilityName)
		: FString::Printf(TEXT("%s: '%s' refused - %s"), *GetNameSafe(Owner), *AbilityName, *Refusal.ToString());
}
