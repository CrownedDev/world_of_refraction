// TurnStartEnergyDebug.cpp

#include "Debug/Combat/TurnStartEnergyDebug.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"
#include "Combat/Mechanics/TurnStartEnergy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float ENERGY_DEBUG_SCREEN_SECONDS = 8.0f;
	constexpr float ENERGY_DEBUG_PERCENT = 100.0f;

	FAutoConsoleCommandWithWorld GPrintEnergyCmd(
		TEXT("wor.Energy.Print"),
		TEXT("DEBUG: each combatant's EP and what its next turn start refills (B-173, D-089)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { UTurnStartEnergyDebug::PrintEnergy(World); }));

	/** Why the next turn start would add nothing; empty when it adds EP. Mirrors ServerGainEnergy's gates. */
	FString RefusalReason(const UCharacterDataComponent &Char, int32 Offer)
	{
		if (!Char.bIsAlive)
		{
			return TEXT("dead");
		}
		if (Offer <= 0)
		{
			return TEXT("no Max EP");
		}
		if (Char.IsBrokenDarkness())
		{
			return TEXT("Broken Darkness (absorption only)");
		}
		if (Char.CharacterData && Char.CharacterData->CharacterClass == ECharacterClass::Resonator && !Char.HasUsableEPTarget())
		{
			return TEXT("Resonator with no EP spend target");
		}
		if (Char.CurrentEP >= Char.MaxEP)
		{
			return TEXT("EP full");
		}
		return FString();
	}
}

FString UTurnStartEnergyDebug::GetEnergyString(AActor *Actor)
{
	const UCharacterDataComponent *Char = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (!Char)
	{
		return FString::Printf(TEXT("%s: no character data"), *GetNameSafe(Actor));
	}
	const int32 Offer = TurnStartEnergy::GetAmount(Actor);
	const FString Refusal = RefusalReason(*Char, Offer);
	return FString::Printf(TEXT("%s: EP %d/%d; turn start offers +%d (%.0f%% of Max EP, at least %d)%s"), *Actor->GetName(),
						   Char->CurrentEP, Char->MaxEP, Offer,
						   CombatConstants::TURN_START_EP_SHARE_OF_MAX * ENERGY_DEBUG_PERCENT, CombatConstants::TURN_START_EP_MIN,
						   Refusal.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(", refused: %s"), *Refusal));
}

int32 UTurnStartEnergyDebug::PrintEnergy(const UObject *WorldContextObject)
{
	UWorld *World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return 0;
	}
	int32 Gaining = 0;
	int32 Checked = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const UCharacterDataComponent *Char = It->FindComponentByClass<UCharacterDataComponent>();
		if (!Char)
		{
			continue;
		}
		++Checked;
		Gaining += RefusalReason(*Char, TurnStartEnergy::GetAmount(*It)).IsEmpty() ? 1 : 0;
		UE_LOG(LogWoRCombat, Display, TEXT("[EnergyDebug] %s"), *GetEnergyString(*It));
	}
	const FString Summary = FString::Printf(TEXT("wor.Energy.Print: %d of %d combatant(s) refill at their next turn start"), Gaining, Checked);
	UE_LOG(LogWoRCombat, Display, TEXT("[EnergyDebug] %s"), *Summary);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, ENERGY_DEBUG_SCREEN_SECONDS, FColor::Cyan, Summary);
	}
	return Gaining;
}
