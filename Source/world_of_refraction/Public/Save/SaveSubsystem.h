// SaveSubsystem.h
// B-090: the save keystone. One slot per local player (UWoRSaveGame), loaded when the first
// local player joins (before any level's BeginPlay builds a party) and written:
//   - async at a fight's exit and a trial's exit (UTrialRunSubsystem),
//   - sync when the local player leaves (the game shutting down), before any subsystem deinitialises.
// What it saves (D-059, explicit structs): the party (members, formation, control; carried HP/EP
// only with a saved run, which doesn't exist yet: Q-090),
// the player's banked wallet (AWoRPlayerState; Gold is never banked) and the shared pool
// (UPoolSubsystem). The wallet also rides here across OpenLevel: the PlayerState banks it at
// EndPlay and takes it back at BeginPlay.
// Debug: wor.Save.Print, wor.Save.Now, wor.Save.Delete (SaveDebug.cpp).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/WoRSaveTypes.h"
#include "SaveSubsystem.generated.h"

class UWoRSaveGame;
class ULocalPlayer;
class UParty;
class UPartySessionSubsystem;
class UPoolSubsystem;
class UCurrencyComponent;

UCLASS()
class WORLD_OF_REFRACTION_API USaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static const TCHAR *SLOT_NAME;

	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	// ==================== FLOW ====================

	/** Load (or create) the local player's slot and restore the party and pool from it.
	 *  Called when the first local player is added; later players are ignored (no split screen). */
	void LoadForPlayer(ULocalPlayer *LocalPlayer);

	/** Capture the party, pool and (if World has a local player) its wallet, then write the slot.
	 *  False when no local player owns the slot yet (headless tests, before the first player). */
	bool SaveNow(UWorld *World, bool bAsync);

	/** Remove the slot from disk and start an empty one in memory. Nothing is written again this
	 *  session (so the next start is fresh) until wor.Save.Now. */
	void DeleteSave();

	/** Undo DeleteSave's stop (wor.Save.Now). */
	void ResumeWrites() { bWritesStopped = false; }

	/** PlayerState EndPlay: bank the wallet's persistent balances. */
	void BankWallet(const UCurrencyComponent *Wallet);

	/** PlayerState BeginPlay: give a fresh wallet the banked balances. */
	void ApplyWalletTo(UCurrencyComponent *Wallet) const;

	const FWoRSaveData &GetData() const;
	bool IsBoundToPlayer() const;

	// ==================== CAPTURE / RESTORE (pure, spec-tested) ====================

	/** bKeepVitals false saves no carried HP/EP: without a saved run, a reload starts outside it (Q-090). */
	static FSavedParty CaptureParty(const UParty *Party, bool bKeepVitals);
	static FSavedWallet CaptureWallet(const UCurrencyComponent *Wallet);
	static FSavedPool CapturePool(const UPoolSubsystem &Pool);

	/** Rebuilds the local party when none exists yet. Members whose character can't be found are dropped. */
	static bool RestoreParty(const FSavedParty &Saved, UPartySessionSubsystem &PartySession);
	static void ApplyWallet(const FSavedWallet &Saved, UCurrencyComponent *Wallet);
	/** Adds the saved items to the pool and marks it populated; returns how many entries came back. */
	static int32 RestorePool(const FSavedPool &Saved, UPoolSubsystem &Pool);

private:
	void HandleLocalPlayerAdded(ULocalPlayer *LocalPlayer);
	void HandleLocalPlayerRemoved(ULocalPlayer *LocalPlayer);
	/** Write the slot as it stands (sync or async). False without a player or after DeleteSave. */
	bool WriteSlot(bool bAsync);
	void CaptureInto(UWorld *World);

	/** Always set: an unbound in-memory slot until a local player loads the real one. */
	UPROPERTY()
	TObjectPtr<UWoRSaveGame> SaveGame;

	FDelegateHandle LocalPlayerAddedHandle;
	FDelegateHandle LocalPlayerRemovedHandle;

	/** Set by DeleteSave: automatic saves stop for the rest of the session. */
	bool bWritesStopped = false;
};
