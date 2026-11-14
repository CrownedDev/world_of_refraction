// SaveSubsystem.h
// B-090: the save keystone. One slot per local player (UWoRSaveGame), loaded when the first
// local player joins (before any level's BeginPlay builds a party) and written:
//   - async at a fight's exit and a trial's exit (UTrialRunSubsystem),
//   - sync when the local player leaves (the game shutting down), before any subsystem deinitialises.
// What it saves (D-059, explicit structs): the party (members, formation, control; carried HP/EP,
// crystal wear and absorption only during a run), the trial run itself (B-208, Q-208: resumed
// when the save is loaded in the hub),
// the player's banked wallet (AWoRPlayerState; Gold is never banked) and the shared pool
// (UPoolSubsystem). The wallet also rides here across OpenLevel: the PlayerState banks it at
// EndPlay and takes it back at BeginPlay. Gold rides along too (B-178), in memory only: it is a
// run currency, never written to the slot, and cleared when a run starts and ends.
// B-398: the player's settings (FSavedSettings: the turn view; B-170e: End Turn button hidden), set in memory at once
// and written with the next save.
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
class UTrialRunSubsystem;
struct FActorsInitializedParams;
struct FPartyMember;
enum class ETurnViewStyle : uint8;
enum class EScreenShakeLevel : uint8;
enum class EPlayerBaseForm : uint8;

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

	/** B-234b (Q-234b (2)): this player takes over as host. Loads the host's checkpoint copy (WoRCheckpoint::SLOT_NAME)
	 *  and plays from it: the old host's party replaces the local one, its armoury joins the pool until the run ends
	 *  (EndHandedOverRun), and its run waits for the next hub world to resume (the last checkpoint). This player's own
	 *  wallet and forms stay. From now on saves go to the copy's slot (and to the others), never this player's own
	 *  slot: a restart comes back to its own save. False without a copy holding a run. */
	bool AdoptCheckpoint(const FString &OldHostKey = FString(), const FString &NewHostKey = FString());

	/** AdoptCheckpoint from SlotName (specs use their own slot, never the real copy). Its saves then go to SlotName.
	 *  B-428 (a): with both keys (WoRDefeat::PlayerDropKey's form, as the join order holds them), the run's per-player
	 *  records are re-keyed for the new host (RekeyRunForNewHost). */
	bool AdoptCheckpointFrom(const FString &SlotName, const FString &OldHostKey = FString(), const FString &NewHostKey = FString());

	/** B-428 (a, Q-428): a handed-over run's opened-chest records (the only per-player features; fragments, spots and
	 *  lore keep their shared keys): the old host's plain keys become "<id>@<OldHostKey>" (so the new host doesn't inherit them, and nobody loots its chests twice) and the
	 *  new host's "<id>@<NewHostKey>" become plain (its own, as a host's are). Other players' keys stay. Nothing when
	 *  either key is empty or they are the same. Pure, spec-tested. */
	static void RekeyRunForNewHost(FSavedRun &Run, const FString &OldHostKey, const FString &NewHostKey);

	/** B-234b: playing a run handed over from the old host's copy (AdoptCheckpoint). */
	bool IsPlayingHostCopy() const { return bPlayingHostCopy; }

	/** B-234d debug: the slot the handed-over copy is saved back to (empty when none). */
	const FString &GetAdoptedSlotName() const { return AdoptedSlotName; }

	/** B-234d debug: the handed-over run ended and its return to this player's own save waits for the hub. */
	bool IsOwnSaveReturnWaiting() const { return OwnSaveReturnHandle.IsValid(); }

	/** B-234d (Q-234d): the handed-over run ended (UTrialRunSubsystem::ExitTrial, after its exit save). Writes this
	 *  player's own slot at once (BuildOwnSaveFrom; memory is not touched), so a quit or crash before the hub keeps the
	 *  run's gains. When the hub's world is up (the run's members have pushed their last loadouts into the pool by then,
	 *  and the hub's party is not built yet), ReturnToOwnSave runs. No-op unless a handed-over run is being played. */
	void EndHandedOverRun();

	/** B-234d review: the travel to the hub was refused; the waiting return is dropped (the own slot is already
	 *  written, so a restart brings it back). No-op when nothing waits. */
	void CancelHandedOverRunEnd();

	/** B-234d (Q-234d): the party and the pool go back to this player's own save (SLOT_NAME), as it was before the
	 *  hand-over, with the upgrade memory the run raised; everything else stays as played (wallet, forms, journal,
	 *  quests, curses, counters were this player's own all along); the run is over. Saves go to its own slot again
	 *  (written now). False unless a handed-over run is being played. */
	bool ReturnToOwnSave();

	/** ReturnToOwnSave from SlotName (specs use their own slot, never the player's real save). */
	bool ReturnToOwnSaveFrom(const FString &SlotName);

	/** B-234d: the save this player's own slot gets when the handed-over run ends: the data as played, with the party
	 *  and pool from the own save in SlotName, the upgrade memory merged (MergeRunUpgradeMemory) and no run. False
	 *  unless a handed-over run is being played (or the own save can't be read). */
	bool BuildOwnSaveFrom(const FString &SlotName, FWoRSaveData &OutData) const;

	/** B-234d review (D-097): Own's upgrade memory, raised by what the handed-over run raised: each entry of Played
	 *  above what the copy held when it was adopted (AtHandOver). The old host's earlier memory is not taken. */
	static TMap<FName, EItemTier> MergeRunUpgradeMemory(const TMap<FName, EItemTier> &Own, const TMap<FName, EItemTier> &AtHandOver,
														const TMap<FName, EItemTier> &Played);

	/** Remove the slot from disk and start an empty one in memory. Nothing is written again this
	 *  session (so the next start is fresh) until wor.Save.Now. */
	void DeleteSave();

	/** Undo DeleteSave's stop (wor.Save.Now). */
	void ResumeWrites() { bWritesStopped = false; }

	/** B-469: a new game starts in place. Everything the save holds goes back to a new game's in memory (party, pool,
	 *  forms, journal, quests, curses, counters, field unlocks, a run waiting to resume, carried Gold, the local wallet),
	 *  Leader becomes the party's only member with BaseForm as the body, writes resume and the slot is written with
	 *  just that. The level change after it (UTrialRunSubsystem::ExitTrial) rebuilds the world. False, and nothing
	 *  changed, while a handed-over run is played or for a member with no CharacterData. */
	bool StartFreshGame(const FPartyMember &Leader, EPlayerBaseForm BaseForm);

	/** PlayerState EndPlay: bank the wallet's persistent balances and carry its run Gold. */
	void BankWallet(const UCurrencyComponent *Wallet);

	/** PlayerState BeginPlay: give a fresh wallet the banked balances and the carried Gold. */
	void ApplyWalletTo(UCurrencyComponent *Wallet) const;

	/** A run starts or ends: the carried Gold and the local wallet's Gold go to 0 (B-178). */
	void ClearRunGold(UWorld *World);

	int32 GetCarriedGold() const { return CarriedGold; }

	const FWoRSaveData &GetData() const;
	bool IsBoundToPlayer() const;

	// ==================== TITLE (B-467) ====================

	/** A save was loaded or written (a fresh, wiped or deleted slot has none). */
	bool HasSave() const;
	/** The last load threw an out-of-date save away (D-060), and the version it had. */
	bool WasWiped() const;
	int32 GetWipedFromVersion() const;
	/** Time played: the saved total and this session's time not saved yet. */
	double GetPlaySeconds() const;

	// ==================== SETTINGS (B-398) ====================

	/** The player's turn-view setting (default Centred Wide, D-074); an unknown saved value reads as the default. */
	ETurnViewStyle GetTurnViewStyle() const;
	/** Kept in memory at once (it survives a fight restart and level loads); written to disk with the next save. */
	void SetTurnViewStyle(ETurnViewStyle Style);

	/** B-170e (Crown, 2026-11-12): the player hides the End Turn button (default shown). Kept in memory at once; written
	 *  with the next save. */
	bool IsEndTurnButtonHidden() const;
	void SetEndTurnButtonHidden(bool bHidden);

	/** B-474 (D-207): "Switch to the weapon of the skill you use" (default on). Kept in memory at once; written with the
	 *  next save. Debug: wor.Settings.SwitchToSkillWeapon. */
	bool IsSwitchToSkillWeaponOn() const;
	void SetSwitchToSkillWeapon(bool bOn);
	/** B-453a (D-190): how much the screen shakes on heavy hits (default Full); an unknown saved value reads as Full.
	 *  Kept in memory at once; written with the next save. */
	EScreenShakeLevel GetScreenShakeLevel() const;
	void SetScreenShakeLevel(EScreenShakeLevel Level);

	// ==================== CAPTURE / RESTORE (pure, spec-tested) ====================

	/** bKeepVitals false saves no carried HP/EP, crystal wear or absorption: outside a run, a reload
	 *  starts fresh (Q-090). */
	static FSavedParty CaptureParty(const UParty *Party, bool bKeepVitals);
	static FSavedWallet CaptureWallet(const UCurrencyComponent *Wallet);
	static FSavedPool CapturePool(const UPoolSubsystem &Pool);

	/** Rebuilds the local party when none exists yet. Members whose character can't be found are dropped. */
	static bool RestoreParty(const FSavedParty &Saved, UPartySessionSubsystem &PartySession);
	static void ApplyWallet(const FSavedWallet &Saved, UCurrencyComponent *Wallet);
	/** Adds the saved items to the pool and marks it populated; returns how many entries came back. */
	static int32 RestorePool(const FSavedPool &Saved, UPoolSubsystem &Pool);

	/** B-208: the active run (bHasRun false when none), with the run's Gold. */
	static FSavedRun CaptureRun(const UTrialRunSubsystem &TrialRun, int32 RunGold);
	/** Puts the saved run back into the trial subsystem (opens nothing). False when there is no
	 *  run or its trial can't be found. */
	static bool RestoreRun(const FSavedRun &Saved, UTrialRunSubsystem &TrialRun);

private:
	void HandleLocalPlayerAdded(ULocalPlayer *LocalPlayer);
	void HandleLocalPlayerRemoved(ULocalPlayer *LocalPlayer);
	/** Write the slot as it stands (sync or async). False without a player or after DeleteSave. */
	bool WriteSlot(bool bAsync);
	void CaptureInto(UWorld *World);
	/** B-467: the time since the last call goes to the current slot's PlaySeconds (before a capture, and before the
	 *  slot changes). */
	void AddPlayTime();

	/** B-208: the first game world is up; resume the loaded run if it is the hub (Q-208). */
	void HandleWorldActorsInitialized(const FActorsInitializedParams &Params);
	void TryResumeRun(UWorld *World);
	/** A run not resumed: the restored party forgets its carried state. */
	void DropRunVitals();

	/** B-234d: the world after a handed-over run's end is up: ReturnToOwnSave. */
	void HandleOwnSaveReturnWorld(const FActorsInitializedParams &Params);

	/** The run the slot was loaded with, waiting for the first game world. */
	FSavedRun PendingRun;
	FDelegateHandle WorldActorsInitializedHandle;
	/** B-234d: EndHandedOverRun waiting for the hub's world. */
	FDelegateHandle OwnSaveReturnHandle;

	/** Always set: an unbound in-memory slot until a local player loads the real one. */
	UPROPERTY()
	TObjectPtr<UWoRSaveGame> SaveGame;

	FDelegateHandle LocalPlayerAddedHandle;
	FDelegateHandle LocalPlayerRemovedHandle;

	/** The run's Gold between levels. Not part of FWoRSaveData: Gold is never banked. */
	int32 CarriedGold = 0;

	/** Set by DeleteSave: automatic saves stop for the rest of the session. */
	bool bWritesStopped = false;
	/** B-467: FPlatformTime::Seconds when play time was last added to the save data. */
	double PlayClock = 0.0;

	/** B-234b: set by AdoptCheckpoint; WriteSlot writes the copy's slot instead of this player's own. Stays set while the
	 *  handed-over run lasts (Q-234b (2)): the game in memory holds the old host's party and armoury, so this player's
	 *  own slot is not written; cleared by ReturnToOwnSave when the run ends (B-234d), or by a restart. */
	bool bPlayingHostCopy = false;

	/** B-234b: the slot the adopted copy came from and is saved back to. */
	FString AdoptedSlotName;

	/** B-234d review: the copy's upgrade memory when it was adopted, so the run's own raises can be told apart. */
	TMap<FName, EItemTier> AdoptedUpgradeMemory;

	/** B-234d: this player's own slot SlotName, loaded (version-checked and bound to the local player, as
	 *  AdoptCheckpointFrom does) or, with none on disk, a fresh one. Null when it can't be read. */
	UWoRSaveGame *LoadOwnSaveGame(const FString &SlotName) const;
};
