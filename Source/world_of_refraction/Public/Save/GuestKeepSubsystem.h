// GuestKeepSubsystem.h
// B-563 (Q-561, Crown 2026-11-17: a co-op guest keeps what it earns, as it would playing its own game). On the guest's
// machine: the character it joined a friend's game with (its own save slot and picks) and what goes back into that
// slot.
//   - Currencies: the run already pays every player's own wallet in full (D-046, D-103), and the guest's wallet starts
//     on the host from the guest's report (B-425). What it earned is the wallet now minus the wallet at connect, added
//     to the joined character's save (never another slot's balance copied in). Gold stays run-only (B-178).
//   - Kept run levels (D-237): the host settles them for every member at run end; its hub save reaches the guest as
//     the B-234 checkpoint copy. A settled copy (no run open) gives the guest's member's kept levels to its character.
//     A copy with a run open gives nothing but the wallet: leaving or the host quitting mid-run is a quit.
//   - Gear (D-219) and skills (D-224): B-564.
// A new character made to join gets a free slot of its own right away.
// Unproven without a second client: the hooks on a real client (checkpoint arrival, the slot writes). Debug:
// wor.Coop.PrintGuestKeep logs GetGuestKeepString.

#pragma once

#include "CoreMinimal.h"
#include "Character/PlayerPicks.h"
#include "Save/WoRSaveTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GuestKeepSubsystem.generated.h"

class UCurrencyComponent;

UCLASS()
class WORLD_OF_REFRACTION_API UGuestKeepSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The guest asked to join as Slot's character (INDEX_NONE: a new one, given the first free slot now). A wallet
	 *  delta still owed to an earlier character goes to it first. False: no slot (full) or the slot could not be
	 *  written; nothing is kept then. */
	bool BeginJoin(int32 Slot, const FPlayerPicks &Picks, const UCurrencyComponent *Wallet);
	/** The wallet this player reported to the host (B-425): the start of what it earns. Connection: the client's
	 *  server connection. A new one is a new visit (the last one's character is let go); the same one again is a
	 *  seamless travel inside the visit and changes nothing. */
	void NoteReportedWallet(const FSavedWallet &Reported, const UObject *Connection);
	/** The host's checkpoint copy arrived: the wallet delta, and with no run open the guest member's kept levels. */
	void FoldCheckpoint(const FWoRSaveData &HostCopy, const UCurrencyComponent *Wallet);
	/** The wallet delta alone (the guest's player state ending). bLeaving: the visit is over; nothing more is kept. */
	void FoldWallet(const UCurrencyComponent *Wallet, bool bLeaving);

	bool IsJoined() const { return JoinedSlot != INDEX_NONE; }
	int32 GetJoinedSlot() const { return JoinedSlot; }
	FString GetGuestKeepString() const;

	// The folds, with no world in them (the specs call these).
	/** False when Now is empty while Counted is not: the wallet has not replicated yet (folding it would take the
	 *  guest's balances away). */
	static bool CanFoldWallet(const FSavedWallet &Counted, const FSavedWallet &Now);
	/** Own plus (Now minus Before), per balance, never below zero. */
	static FSavedWallet AddWalletDelta(const FSavedWallet &Own, const FSavedWallet &Before, const FSavedWallet &Now);
	/** The host's member built from these picks as a guest (B-561: same name, class, element, Broken Darkness). */
	static const FSavedPartyMember *FindGuestMember(const FWoRSaveData &HostCopy, const FPlayerPicks &Picks);
	/** Gives Member's kept levels to Own's character (its first member with picks). False: nothing to give. */
	static bool FoldKeptLevels(FWoRSaveData &Own, const FSavedPartyMember &Member);
	/** A fresh save holding only the character made from Picks (the load builds it, B-554). */
	static FWoRSaveData MakeNewCharacterSave(const FPlayerPicks &Picks);

private:
	/** Reads JoinedSlot, applies Fold, writes it back. */
	bool FoldIntoSlot(TFunctionRef<bool(FWoRSaveData &)> Fold);

	int32 JoinedSlot = INDEX_NONE;
	FPlayerPicks JoinedPicks;
	/** The wallet already counted: the report at connect, then each fold's. */
	FSavedWallet CountedWallet;
	bool bHasCountedWallet = false;
	/** The visit's server connection (NoteReportedWallet). */
	TWeakObjectPtr<const UObject> VisitConnection;
};
