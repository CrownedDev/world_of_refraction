// WoRTestWorld.h
// Headless game world for automation specs: a real UGameInstance (so every WoR
// GameInstanceSubsystem exists) and a manually ticked world with BeginPlay run.

#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "WoRLog.h"

class FWoRTestWorld
{
public:
	/** Creates the world and begins play. False (with the wrapper's errors logged) on failure. */
	bool Create()
	{
		if (Wrapper.CreateTestWorld(EWorldType::Game) && Wrapper.BeginPlayInTestWorld())
		{
			return true;
		}
		TArray<FString> Errors;
		Wrapper.AppendErrorMessages(Errors);
		for (const FString &Error : Errors)
		{
			UE_LOG(LogWoR, Error, TEXT("[WoRTestWorld] %s"), *Error);
		}
		return false;
	}

	UWorld *GetWorld() const { return Wrapper.GetTestWorld(); }

	template <class TSubsystem>
	TSubsystem *GetSubsystem() const
	{
		UWorld *World = GetWorld();
		UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<TSubsystem>() : nullptr;
	}

	/** Runs world ticks — timers (including SetTimerForNextTick) fire here. */
	void Tick(int32 Frames = 1)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Wrapper.TickTestWorld();
		}
	}

	/** Transient character asset with every stat at its default (0 points, world level 0). */
	static UCharacterData *NewCharacterData()
	{
		return NewObject<UCharacterData>(GetTransientPackage());
	}

	/** Spawns a bare combatant carrying only a UCharacterDataComponent seeded from Data.
	 *  bAIControlled spawns an unpossessed pawn (the orchestrator routes it to AI);
	 *  otherwise a plain actor (routed to the player path). */
	AActor *SpawnCombatant(UCharacterData *Data, bool bAIControlled = false)
	{
		UWorld *World = GetWorld();
		if (!World || !Data)
		{
			return nullptr;
		}

		AActor *Actor = bAIControlled ? World->SpawnActor<APawn>() : World->SpawnActor<AActor>();
		if (!Actor)
		{
			return nullptr;
		}

		UCharacterDataComponent *CharComp = NewObject<UCharacterDataComponent>(Actor);
		CharComp->CharacterData = Data;
		Actor->AddInstanceComponent(CharComp);
		// The world has begun play, so registering runs the component's BeginPlay (HP/EP seed).
		CharComp->RegisterComponent();
		return Actor;
	}

private:
	FTestWorldWrapper Wrapper;
};

#endif // WITH_DEV_AUTOMATION_TESTS
