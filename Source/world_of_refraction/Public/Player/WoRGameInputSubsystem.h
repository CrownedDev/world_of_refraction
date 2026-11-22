// WoRGameInputSubsystem.h
// B-578: game input is given back at the start of every level change.
//
// Why: a menu that takes the keys (FInputModeUIOnly) tells the game viewport to ignore game input. The viewport
// outlives a level change; the menu and its controller do not. A menu still up when the level changes (a fight's
// results screen: Continue is the level change) left the next level with no move, no Tab and no click, until
// something set game input again. Nothing in the engine does that on a level change.
//
// So the rule lives in one place: when a level change starts (OpenLevel or seamless travel) with a menu-only mode
// still on, every local player gets FInputModeGameOnly, as closing any menu does. A level change with game input on
// is left alone. A screen the next level opens on arrival (the title, a join seat, a duplicate prompt) sets its own
// mode after that, so it is not undone.
// Debug: wor.Input.PrintState (GameInputDebug.h).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WoRGameInputSubsystem.generated.h"

class UGameViewportClient;
struct FWorldContext;

UCLASS()
class WORLD_OF_REFRACTION_API UWoRGameInputSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	/** Game input back for this game's viewport and local players, when the viewport is ignoring game input. Nothing
	 *  for another game instance's context (a second PIE window), one with no viewport (a dedicated server), or a
	 *  viewport that takes game input already. True when it gave input back. */
	bool RestoreGameInput(const FWorldContext &WorldContext);

	/** What FInputModeGameOnly puts on the viewport, without needing a player or a viewport widget. */
	static void ApplyGameOnly(UGameViewportClient &Viewport);

private:
	void HandlePreLoadMap(const FWorldContext &WorldContext, const FString &MapName);
	void HandleSeamlessTravelStart(UWorld *World, const FString &MapName);
};
