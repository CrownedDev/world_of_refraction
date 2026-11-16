// WoRFrontEndGameMode.h
// B-537 (D-232): the menu level's game mode. The game starts in a small level that holds only the title: no pawn, and
// the hub's controller for the UI root (a Blueprint child names it; editor step, docs/PIE/B-537.md). The controller
// opens the title each time this level loads; Continue / Begin travel to the hub; the game menu's Quit outside a run
// travels back here.

#pragma once

#include "CoreMinimal.h"
#include "Player/WoRGameModeBase.h"
#include "WoRFrontEndGameMode.generated.h"

class UGameInstance;
class UWorld;

UCLASS()
class WORLD_OF_REFRACTION_API AWoRFrontEndGameMode : public AWoRGameModeBase
{
    GENERATED_BODY()

public:
    AWoRFrontEndGameMode();

    /** No pawn: the title is all this level holds (UI input only). */
    virtual bool PlayerCanRestart_Implementation(APlayerController *Player) override;
};

namespace WoRFrontEnd
{
    /** Where the game menu's Quit goes. */
    enum class EQuitTo : uint8
    {
        Hub,      // in a run: the run is abandoned, back to the hub (D-176)
        FrontEnd, // outside a run, with a menu level: back to the title
        Desktop,  // outside a run, no menu level yet: the game closes (as before B-537)
    };

    /** The menu level exists once the game's default map is not the hub (the ini step made after the map exists).
     *  Long package names. */
    WORLD_OF_REFRACTION_API bool HasFrontEnd(const FString &DefaultMapPackage, const FString &HubMapPackage);

    /** Play pressed straight in the hub or a trial with no save and no party leader: the game goes to the menu level
     *  rather than standing the pawn's class default (the Character Template). A save without a leader plays as before
     *  (no loop back from Continue). Never under tests. */
    WORLD_OF_REFRACTION_API bool ShouldFallBackToFrontEnd(bool bHasSave, bool bHasLeader, bool bHasFrontEnd, bool bAutomationTesting);

    WORLD_OF_REFRACTION_API EQuitTo GetQuitTarget(bool bInRun, bool bHasFrontEnd);

    /** The game's default map (GameDefaultMap, DefaultEngine.ini) as a long package name. */
    WORLD_OF_REFRACTION_API FString GetDefaultMapPackage();

    /** HasFrontEnd for this game's config (GameDefaultMap against the trial run's HubLevel). */
    WORLD_OF_REFRACTION_API bool HasFrontEnd(const UGameInstance *GameInstance);

    /** World is the menu level (its game mode is AWoRFrontEndGameMode; the server's view). */
    WORLD_OF_REFRACTION_API bool IsFrontEndWorld(const UWorld *World);

    /** Opens the menu level. False, doing nothing, when there is none. */
    WORLD_OF_REFRACTION_API bool TravelToFrontEnd(UWorld *World);
}
