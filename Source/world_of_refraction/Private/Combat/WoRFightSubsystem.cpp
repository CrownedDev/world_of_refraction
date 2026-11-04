// WoRFightSubsystem.cpp

#include "Combat/WoRFightSubsystem.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

bool UWoRFightSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UGameInstance *UWoRFightSubsystem::GetGameInstance() const
{
	const UWorld *World = GetWorld();
	return World ? World->GetGameInstance() : nullptr;
}

namespace WoRFight
{
	namespace
	{
		/** A game instance's world. Its own world context is only set by the engine's play paths; a
		 *  headless test world registers the game instance as the owner of a context instead. */
		UWorld *WorldOfGameInstance(const UGameInstance *GameInstance)
		{
			if (!GameInstance)
			{
				return nullptr;
			}
			if (UWorld *World = GameInstance->GetWorld())
			{
				return World;
			}
			if (GEngine)
			{
				for (const FWorldContext &WorldContext : GEngine->GetWorldContexts())
				{
					if (WorldContext.OwningGameInstance == GameInstance)
					{
						return WorldContext.World();
					}
				}
			}
			return nullptr;
		}
	}

	UWorld *FindWorld(const UObject *Context)
	{
		if (!Context)
		{
			return nullptr;
		}
		if (const UWorld *World = Cast<UWorld>(Context))
		{
			return const_cast<UWorld *>(World);
		}
		if (const UGameInstance *GameInstance = Cast<UGameInstance>(Context))
		{
			return WorldOfGameInstance(GameInstance);
		}
		if (const UGameInstanceSubsystem *GameSubsystem = Cast<UGameInstanceSubsystem>(Context))
		{
			return WorldOfGameInstance(GameSubsystem->GetGameInstance());
		}
		return Context->GetWorld();
	}

	UGameInstance *FindGameInstance(const UObject *Context)
	{
		if (const UGameInstance *GameInstance = Cast<UGameInstance>(Context))
		{
			return const_cast<UGameInstance *>(GameInstance);
		}
		if (const UGameInstanceSubsystem *GameSubsystem = Cast<UGameInstanceSubsystem>(Context))
		{
			return GameSubsystem->GetGameInstance();
		}
		const UWorld *World = FindWorld(Context);
		return World ? World->GetGameInstance() : nullptr;
	}

	FString GetFightSubsystemsString(const UWorld *World)
	{
		if (!World)
		{
			return TEXT("[FightSubsystems] No world");
		}
		const TArray<UWoRFightSubsystem *> Subsystems = World->GetSubsystemArrayCopy<UWoRFightSubsystem>();
		FString Out = FString::Printf(TEXT("[FightSubsystems] %s: %d"), *World->GetName(), Subsystems.Num());
		for (const UWoRFightSubsystem *Subsystem : Subsystems)
		{
			Out += FString::Printf(TEXT("\n  %s"), *GetNameSafe(Subsystem));
		}
		return Out;
	}

	static FAutoConsoleCommandWithWorld PrintFightSubsystemsCommand(
		TEXT("wor.Fight.Subsystems"),
		TEXT("Lists the per-fight world subsystems on this world (B-336)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			const FString Text = GetFightSubsystemsString(World);
			UE_LOG(LogWoR, Log, TEXT("%s"), *Text);
			if (GEngine)
			{
				constexpr float MessageSeconds = 8.f;
				GEngine->AddOnScreenDebugMessage(-1, MessageSeconds, FColor::Cyan, Text);
			}
		}));
}
