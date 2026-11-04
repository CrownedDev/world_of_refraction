// WoRFightSubsystem.h
// Base for the per-fight subsystems (D-129, D-064, B-336): they live on the battle's world, so each
// world (each PIE client, each test world) has its own and nothing is shared through the game instance.
// WoRFight::Get<T>(Context) finds any of them, or any game-instance subsystem, from a game instance,
// a subsystem, an actor, a component or a widget.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "WoRFightSubsystem.generated.h"

UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UWoRFightSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Game and PIE worlds only: no copies in editor or preview worlds. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** The world's game instance. Null inside Initialize when the world is created before its game
	 *  instance is set (headless test worlds): look other subsystems up lazily, not in Initialize. */
	UGameInstance *GetGameInstance() const;
};

namespace WoRFight
{
	/** The world Context belongs to: itself, a game instance's (or a game-instance subsystem's) current
	 *  world, or Context->GetWorld(). Null when there is none. */
	WORLD_OF_REFRACTION_API UWorld *FindWorld(const UObject *Context);

	/** The game instance Context belongs to (itself, a game-instance subsystem's, or its world's). */
	WORLD_OF_REFRACTION_API UGameInstance *FindGameInstance(const UObject *Context);

	/** A world subsystem from Context's world, or a game-instance subsystem from its game instance. */
	template <class TSubsystem>
	TSubsystem *Get(const UObject *Context)
	{
		if constexpr (TIsDerivedFrom<TSubsystem, UWorldSubsystem>::Value)
		{
			UWorld *World = FindWorld(Context);
			return World ? World->GetSubsystem<TSubsystem>() : nullptr;
		}
		else
		{
			UGameInstance *GameInstance = FindGameInstance(Context);
			return GameInstance ? GameInstance->GetSubsystem<TSubsystem>() : nullptr;
		}
	}

	/** One line per fight subsystem on World (wor.Fight.Subsystems). */
	WORLD_OF_REFRACTION_API FString GetFightSubsystemsString(const UWorld *World);
}
