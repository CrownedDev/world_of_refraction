// AmpDebug.cpp

#include "Debug/Combat/AmpDebug.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Loadout/LoadoutComponent.h"
#include "WoRLog.h"

FString WoRAmpDebug::GetAmpString(AActor *Actor, ESpellElement SampleElement)
{
	const ULoadoutComponent *Loadout = Actor ? Actor->FindComponentByClass<ULoadoutComponent>() : nullptr;
	if (!Loadout)
	{
		return FString::Printf(TEXT("%s: no loadout (no Amp)"), *GetNameSafe(Actor));
	}
	const FAmpBonus Amp = Loadout->GetActiveStatBonus(Actor).Amp;
	return FString::Printf(TEXT("%s: %s | %s action %+d, physical %+d"), *GetNameSafe(Actor), *Amp.ToString(),
						   *UEnum::GetDisplayValueAsText(SampleElement).ToString(), Amp.GetTotalFor(SampleElement),
						   Amp.GetTotalFor(ESpellElement::None));
}

void WoRAmpDebug::PrintAmpState(UWorld *World)
{
	if (!World)
	{
		return;
	}
	int32 Count = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->FindComponentByClass<ULoadoutComponent>())
		{
			UE_LOG(LogWoR, Display, TEXT("[Amp] %s"), *GetAmpString(*It));
			++Count;
		}
	}
	UE_LOG(LogWoR, Display, TEXT("[Amp] %d actor(s) with a loadout"), Count);
}

static FAutoConsoleCommandWithWorld GDamagePrintAmpCommand(
	TEXT("wor.Damage.Amp"),
	TEXT("B-382: every loadout's Amp lines (worn gear, one sum) and its Fire / physical totals."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&WoRAmpDebug::PrintAmpState));
