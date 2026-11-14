// HostHandOverModel.h
// B-459a (D-200, D-181; UIScreens #33, mockups screen_mockups/coop/session/3_host_lost.png and
// 4_planned_handover.png): what the host-lost card and the "Hand over host?" confirm show, with no widget in it, so the
// specs read it headless. The card follows UWoRHostHandOverSubsystem's step (its OnStepChanged is the lost-host signal):
// four pips (the host left, the old session left, the new host up, rejoined), the step's line, and B "Stop waiting"
// only while this machine is waiting on someone else (Q-459a). The confirm names the next host and says why it can't
// start when it can't. UHostLostCardWidget and UHandOverConfirmWidget draw them. Debug: wor.Coop.PrintHandOverCard.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"

enum class EHostHandOverStep : uint8;
class UWorld;

namespace WoRHostHandOverScreens
{
	/** The card's pips (mockup 3_host_lost). */
	constexpr int32 CARD_PIPS = 4;
}

struct FHostLostCard
{
	/** False when no hand-over is going on (the card closes). */
	bool bShown = false;
	/** "The host left" / "Handing over host". */
	FText Title;
	/** "You are taking over as host." / "The next player is taking over as host." */
	FText HostLine;
	/** The step's line: "Reopening the trial for everyone…". */
	FText StatusLine;
	/** "Your run, gear and progress are kept." (or where the run waits, when it gave up). */
	FText KeptLine;
	/** 0..CARD_PIPS. */
	int32 LitPips = 0;
	bool bGaveUp = false;
	/** B "Stop waiting": only while this machine waits on someone else. */
	bool bCanStop = false;
};

struct FHandOverConfirm
{
	/** True when A can start it now. */
	bool bCanHandOver = false;
	/** The next host's name ("the next player" when not known). */
	FText NextHost;
	/** Why it can't start, for the player; empty when it can. */
	FText Refusal;
};

namespace WoRHostHandOverScreens
{
	/** The card for a step: bPlanned (chosen at a rest spot), bThisMachineHosts (this machine is the next host),
	 *  RejoinTries (searches for the new host so far), bLeavePending (the planned old host hasn't left yet: no B). */
	WORLD_OF_REFRACTION_API FHostLostCard BuildCard(EHostHandOverStep Step, bool bPlanned, bool bThisMachineHosts, int32 RejoinTries,
													bool bLeavePending = false);

	/** The card for this game instance's hand-over. */
	WORLD_OF_REFRACTION_API FHostLostCard BuildCard(UWorld *World);

	/** The confirm from the net mode, the players known in the join order and the next host's name (empty: unknown). */
	WORLD_OF_REFRACTION_API FHandOverConfirm BuildConfirm(ENetMode NetMode, int32 PlayersKnown, const FString &NextHostName);

	/** The confirm for World (the next host's name from its player list). */
	WORLD_OF_REFRACTION_API FHandOverConfirm BuildConfirm(UWorld *World);

	/** A on the confirm: hand over now. Empty on success; else the reason. */
	WORLD_OF_REFRACTION_API FText HandOver(UWorld *World);

	WORLD_OF_REFRACTION_API FString GetCardString(UWorld *World);
}
