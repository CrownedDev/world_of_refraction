// WoRSaveGame.h
// B-090: the one save slot per local player. Version-stamped through ULocalPlayerSaveGame;
// until the first public build a save below MIN_SUPPORTED_VERSION is wiped with a warning (D-060).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Save/WoRSaveTypes.h"
#include "WoRSaveGame.generated.h"

namespace WoRSaveVersion
{
	/** Bump when a save struct changes shape, and MIN_SUPPORTED with it (D-138: one save version while in dev). */
	constexpr int32 LATEST = 31; // 2: B-208's run and carried state; 3: B-124's gear detail; 4: B-279's form meters (older load with defaults); 5: B-308's one gear list; 6: B-311's custom loadouts; 7: B-321's gear owners; 8: B-279b's worn skin; 9: B-322's pouch per character; 10: B-279c's current outfit (older load with none); 11: B-333's seven balances; 12: B-348 drops every old-shape list; 13: B-351's two sockets per weapon / adapter (D-141) and one skill-slot list (D-145); from here MIN_SUPPORTED rises with it (D-138); 14: B-360b's member id and key form per party member (separate copies of one character, D-150); 15: B-314b's open run layer (FSavedRun) and upgrade memory (FSavedPool); 16: B-316b's learned skills per character (FSavedPool); 17: B-361b's Mentor Tokens balance (D-154); 18: B-362's run wipe and main-boss-loss counters and last rest spot (FSavedRun, D-155); 19: B-362b's dropped Prisms (FSavedRun, D-160); 20: B-362c's drop spot (FSavedRun, D-160 changed); 21: B-348b's head-start pillars per party member (FSavedPartyMember); 22: B-351b's carried stone socket (FSavedCarriedAttachment); 23: B-073's run reward tally (FSavedRun); 24: B-187c's mini-boss marks and revivals (FSavedRun); 25: B-377's hidden quest progress (FSavedHiddenQuests); 26: B-378's curses held (FSavedCurses); 27: B-379's hidden counters (FSavedCounters); 28: B-377b's memory journal (FSavedJournal); 29: B-380c's apparel in loadouts (FSavedLoadout / FSavedCustomLoadout::Apparel); 30: B-382's Amp lines on gear (FEquipmentStatBonus::Amp); 31: B-383's Spell Amp line (FAmpBonus::Spell)
	/** Saves below this are wiped with a warning (D-060). D-138: equal to LATEST while in dev, with no upgrade steps until
	 *  the first public build. */
	constexpr int32 MIN_SUPPORTED = LATEST;
}

UCLASS()
class WORLD_OF_REFRACTION_API UWoRSaveGame : public ULocalPlayerSaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FWoRSaveData Data;

	virtual int32 GetLatestDataVersion() const override { return WoRSaveVersion::LATEST; }
	virtual void HandlePostLoad() override;
	virtual void ResetToDefault() override;

	/** True when the last load threw an out-of-date save away. */
	bool WasWiped() const { return bWiped; }

private:
	UPROPERTY(Transient)
	bool bWiped = false;
};
