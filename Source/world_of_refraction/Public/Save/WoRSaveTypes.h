// WoRSaveTypes.h
// B-090: the explicit save structs (D-059: copied whole, no SaveGame tags). Asset references are
// saved as an FPrimaryAssetId plus the asset's path, never a pointer: the id survives an asset
// move, the path resolves a type the Asset Manager doesn't scan.

#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/SoftObjectPath.h"
#include "Combat/Grid/ECombatRow.h"
#include "Inventory/ItemTier.h"
#include "Inventory/ItemQuality.h"
#include "Party/Party.h"
#include "Loadout/CarriedGearState.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/FResistanceBonus.h"
#include "Combat/Actions/ActionStatModifiers.h" // ESubStat::None (B-353)
#include "Equipment/FRuntimeAttachedItem.h"
#include "WoRSaveTypes.generated.h"

/** A data asset by id and path. Resolve loads it (synchronously) or returns null. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedAssetRef
{
	GENERATED_BODY()

	UPROPERTY()
	FPrimaryAssetId Id;

	UPROPERTY()
	FSoftObjectPath Path;

	static FSavedAssetRef Make(const UObject *Asset);
	bool IsSet() const { return Id.IsValid() || Path.IsValid(); }
	UObject *Resolve() const;

	template <typename T>
	T *ResolveAs() const { return Cast<T>(Resolve()); }
};

/** B-208: one weapon's or ring's carried attachment. The holder and an evolution's item are
 *  saved as refs; the saved Attached has its evolution pointer cleared. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedCarriedAttachment
{
	GENERATED_BODY()

	UPROPERTY()
	FSavedAssetRef Holder;

	/** B-310: the owned item the attachment sits on; invalid in saves made before it (then matched by Holder). */
	UPROPERTY()
	FGuid GearID;

	UPROPERTY()
	FRuntimeAttachedItem Attached;

	/** B-351b (save v22): the item's stone socket as the fight left it. */
	UPROPERTY()
	FRuntimeAttachedItem StoneSocket;

	UPROPERTY()
	FSavedAssetRef EvolutionItem;
};

/** B-208: FCarriedGearState without pointers (D-059). */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedCarriedGear
{
	GENERATED_BODY()

	UPROPERTY()
	bool bCarried = false;

	UPROPERTY()
	TArray<FSavedCarriedAttachment> Attachments;

	UPROPERTY()
	bool bHasPrimaryEvolution = false;

	/** Item cleared; the ref below carries it. */
	UPROPERTY()
	FEvolutionAttachment PrimaryEvolution;

	UPROPERTY()
	FSavedAssetRef PrimaryEvolutionItem;

	static FSavedCarriedGear Make(const FCarriedGearState &Carried);
	/** The state back; an attachment whose holder can't be found is dropped (logged). */
	FCarriedGearState Restore() const;
};

/** Carried HP/EP, crystal wear and absorption are written only with a saved run (B-208, Q-090). */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedPartyMember
{
	GENERATED_BODY()

	UPROPERTY()
	FSavedAssetRef CharacterData;

	UPROPERTY()
	FSoftClassPath PawnClass;

	UPROPERTY()
	ECombatRow PreferredRow = ECombatRow::Middle;

	UPROPERTY()
	int32 PreferredColumn = PartyConstants::UNSET_FORMATION;

	UPROPERTY()
	bool bPlayerControlled = true;

	/** B-346 / B-348: the AI seat's difficulty (restored only when the member is an AI seat). */
	UPROPERTY()
	EAIDifficulty AISeatDifficulty = EAIDifficulty::Medium;

	/** B-348b (save v21): the head-start pillar allocation (FPartyMember::HeadStart*). */
	UPROPERTY()
	int32 HeadStartMind = 0;

	UPROPERTY()
	int32 HeadStartBody = 0;

	UPROPERTY()
	int32 HeadStartSpirit = 0;

	/** B-348c (save v21, shared with B-348b: both landed in dev together, before any v21 save existed): the member is in Broken Darkness (persistent until reverted, Q-348b), and the
	 *  run's strain (written only with a saved run, like the carried vitals). */
	UPROPERTY()
	bool bBrokenDarkness = false;

	UPROPERTY()
	float RunStrain = 0.0f;

	/** B-076: a character found in the trial; written only with a saved run. */
	UPROPERTY()
	bool bRunOnly = false;

	UPROPERTY()
	FGuid RunWeapon;

	UPROPERTY()
	FGuid RunRing;

	/** B-360b (save v14): the member apart from other copies of its character, and whether the pool keys it by that id. */
	UPROPERTY()
	FGuid MemberId;

	UPROPERTY()
	bool bKeyedByMember = false;

	/** B-392 (D-170, save v32): the member's chosen guard and the weapon it put away (FPartyMember::GuardStyle /
	 *  GuardWeaponId). */
	UPROPERTY()
	EGuardStyle GuardStyle = EGuardStyle::Weapon;

	UPROPERTY()
	FGuid GuardWeaponId;

	/** B-393 (D-170, save v33): which element colours the member's landed-parry effect
	 *  (FPartyMember::ParryElementPriority). */
	UPROPERTY()
	EParryElementPriority ParryElementPriority = EParryElementPriority::Weapon;

	/** B-214a (D-099, save v36): the member's active field skill (FPartyMember::FieldSkill). */
	UPROPERTY()
	EFieldSkillSource FieldSkillSource = EFieldSkillSource::Innate;

	UPROPERTY()
	EFieldMovementType FieldSkillType = EFieldMovementType::Dash;

	UPROPERTY()
	int32 CurrentHP = PartyConstants::NO_CARRIED_VITALS;

	UPROPERTY()
	int32 CurrentEP = PartyConstants::NO_CARRIED_VITALS;

	UPROPERTY()
	FSavedCarriedGear CarriedGear;

	/** Plain values (no pointers), saved as the fight left it. */
	UPROPERTY()
	FCarriedAbsorptionState CarriedAbsorption;
};

/** B-314b (Q-314, save v15): one piece's run mark (UPoolSubsystem's FPoolRunMark) and its home state, pointers as refs. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedRunMark
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid GearID;

	UPROPERTY()
	bool bRunOnly = false;

	UPROPERTY()
	bool bOnRun = false;

	/** The home attachment with its evolution pointer cleared (HomeEvolution holds it), as FSavedGearDetail does. */
	UPROPERTY()
	FRuntimeAttachedItem HomeAttached;

	UPROPERTY()
	FSavedAssetRef HomeEvolution;

	/** One ref per spell slot, an empty slot as an unset ref. */
	UPROPERTY()
	TArray<FSavedAssetRef> HomeSpells;

	UPROPERTY()
	int32 HomeDurability = 0;
};

/** B-208 (Q-090, Q-208): the trial run as the last fight's exit left it; reloading resumes it. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedRun
{
	GENERATED_BODY()

	UPROPERTY()
	bool bHasRun = false;

	UPROPERTY()
	FSavedAssetRef Trial;

	UPROPERTY()
	int32 RunSeed = 0;

	UPROPERTY()
	int32 VariationIndex = INDEX_NONE;

	/** ETrialDifficulty. */
	UPROPERTY()
	uint8 Difficulty = 0;

	UPROPERTY()
	TArray<FName> BeatenEncounters;

	UPROPERTY()
	TMap<FName, FText> FoundFeatures;

	UPROPERTY()
	TArray<FSavedAssetRef> RunMemories;

	/** Q-208: the run's Gold rides with the run (never banked into the wallet). */
	UPROPERTY()
	int32 RunGold = 0;

	/** B-362 (D-155, save v18): the run's party wipes and losses to the main boss, and the last rest spot used. */
	UPROPERTY()
	int32 Wipes = 0;

	UPROPERTY()
	int32 MainBossLosses = 0;

	UPROPERTY()
	FName LastRestSpot;

	/** B-362b (D-160, save v19): the run's Prisms dropped at a fight, and that fight's map teams. */
	UPROPERTY()
	int32 DroppedPrisms = 0;

	/** OBSOLETE (B-362c): no longer written; delete with the next save bump after PIE. */
	UPROPERTY()
	TArray<FName> DropEncounters;

	/** B-362c (D-160 changed, save v20): where the drop's pickup lies on the trial map; false = the trial's start. */
	UPROPERTY()
	bool bDropHasSpot = false;

	UPROPERTY()
	FVector DropSpot = FVector::ZeroVector;

	/** B-073 (save v23; 21 / 22 are B-348b / B-351b: renumber if dev moved): the run's reward tally, paid at its end. */
	UPROPERTY()
	int32 TallyHubGold = 0;

	UPROPERTY()
	int32 TallyGearEssence = 0;

	UPROPERTY()
	int32 TallySkillEssence = 0;

	/** B-073 (save v23, desktop review): the run's boss was beaten, so a resumed run's end still gets the boss bonus. */
	UPROPERTY()
	bool bTallyBossBeaten = false;

	/** B-187c (save v24, on B-073's v23: renumber if dev moved): the beaten mini-bosses (each a rest spot) and the
	 *  revivals per mini-boss encounter (Q-187b). */
	UPROPERTY()
	TArray<FName> BeatenMiniBosses;

	UPROPERTY()
	TMap<FName, int32> EncounterRevivals;

	/** B-314b (save v15): the run layer was open; its marks follow, so a resumed run keeps its found gear run-only and
	 *  puts home gear back as it left home. */
	UPROPERTY()
	bool bHasRunLayer = false;

	UPROPERTY()
	TArray<FSavedRunMark> RunMarks;
};

USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedParty
{
	GENERATED_BODY()

	UPROPERTY()
	bool bHasParty = false;

	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	bool bLetAIFinish = false;

	UPROPERTY()
	TArray<FSavedPartyMember> Members;
};

/** One banked balance: an ECurrencyType, its essence sub-key, the amount. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedBalance
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 Currency = 0;

	UPROPERTY()
	uint8 SubKey = 0;

	UPROPERTY()
	int32 Amount = 0;
};

/** The player's banked currencies. Gold is never banked (CurrencySystem.md), so it is not here. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedWallet
{
	GENERATED_BODY()

	UPROPERTY()
	bool bHasWallet = false;

	UPROPERTY()
	TArray<FSavedBalance> Balances;
};

/** B-124 (D-059): the rest of an owned weapon or ring: its attached crystal or evolution, the
 *  spells on it and its rolls. Pointers are saved as refs; the saved Attached has its evolution
 *  pointer cleared. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedGearDetail
{
	GENERATED_BODY()

	UPROPERTY()
	FRuntimeAttachedItem Attached;

	/** B-351 (D-141): the stone socket (an augment stone carries only its FCrystalId, no asset pointer). */
	UPROPERTY()
	FRuntimeAttachedItem StoneSocket;

	/** B-351 (D-145): a weapon's skill-slot picks, one ref per slot (an unset ref = the built-in), and whether its
	 *  one list is active. Empty for an adapter. */
	UPROPERTY()
	TArray<FSavedAssetRef> SkillPicks;

	UPROPERTY()
	bool bSkillSlotsActive = false;

	UPROPERTY()
	FSavedAssetRef EvolutionItem;

	/** B-098 (Q-098, save v37): this and the two spell fields below are written for an adapter only; a weapon gives
	 *  no spells (D-027), so its spells are not saved. */
	UPROPERTY()
	TArray<FSavedAssetRef> AssignedSpells;

	/** B-351d (D-145): the crystal's one spell list, one ref per slot (unset = the built-in), and whether it is active.
	 *  New tagged fields: an older save reads them as empty / false (the old merge), so no version bump. */
	UPROPERTY()
	TArray<FSavedAssetRef> SpellPicks;

	UPROPERTY()
	bool bSpellSlotsActive = false;

	UPROPERTY()
	FEquipmentStatBonus StatBonus;

	UPROPERTY()
	FResistanceBonus ResistanceBonus;

	UPROPERTY()
	int32 StatPool = 0;

	UPROPERTY()
	int32 StatMaxPool = 0;

	UPROPERTY()
	int32 ResistancePool = 0;

	UPROPERTY()
	int32 ResistanceMaxPool = 0;
};

/** An owned gear or skill instance. InstanceID is the weapon/ring PersistentID or the
 *  spell/ability/evolution InstanceID. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedOwnedItem
{
	GENERATED_BODY()

	UPROPERTY()
	FSavedAssetRef Def;

	UPROPERTY()
	FGuid InstanceID;

	UPROPERTY()
	EItemTier Tier = EItemTier::F_Tier;

	UPROPERTY()
	EItemQuality Quality = EItemQuality::F_Quality;

	UPROPERTY()
	int32 Durability = 0;

	/** B-124: weapons and rings only. */
	UPROPERTY()
	FSavedGearDetail Gear;
};

/** A counted crystal stack. Gem or stone follows from the type (UPoolSubsystem::AddCrystalToPool). */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedCrystalStack
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 Type = 0;

	UPROPERTY()
	EItemTier Tier = EItemTier::F_Tier;

	UPROPERTY()
	int32 Count = 0;

	/** B-353 (D-142): a StatStone's sub-stat (ESubStat); ESubStat::None otherwise (and for a stack saved before the
	 *  field existed, so a gem never reads back with a sub-stat). */
	UPROPERTY()
	uint8 StoneStat = static_cast<uint8>(ESubStat::None);
};

/** B-091: a piece of shared gear and the character wearing it (UPoolSubsystem::GetCharacterKey). */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedWearer
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid GearID;

	UPROPERTY()
	FName Character;
};

/** B-311: a Broken Darkness element's spell pool in a custom loadout. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedCustomBDPool
{
	GENERATED_BODY()

	/** ESpellElement. */
	UPROPERTY()
	uint8 Element = 0;

	UPROPERTY()
	TArray<FSavedAssetRef> Spells;
};

/** B-311 (D-111): one custom loadout; each piece by its owned GUID and asset, the rest by asset. An adapter's spells
 *  ride on the adapter itself (FSavedGearDetail). */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedCustomLoadout
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	/** ECharacterClass. */
	UPROPERTY()
	uint8 RequiredClass = 0;

	UPROPERTY()
	bool bShowPrimary = true;

	/** EPrimarySlotType; PrimaryAsset is the weapon, adapter or evolution it names. */
	UPROPERTY()
	uint8 PrimarySlotType = 0;

	UPROPERTY()
	FSavedAssetRef PrimaryAsset;

	UPROPERTY()
	FGuid PrimaryInstance;

	UPROPERTY()
	TArray<FSavedAssetRef> PrimaryAbilities;

	UPROPERTY()
	TArray<FSavedAssetRef> EvolutionSpells;

	/** ESecondarySlotType. */
	UPROPERTY()
	uint8 SecondarySlotType = 0;

	UPROPERTY()
	FSavedAssetRef SecondaryWeapon;

	UPROPERTY()
	FGuid SecondaryInstance;

	UPROPERTY()
	TArray<FSavedAssetRef> SecondaryAbilities;

	/** A Resonator's adapter loadout, in order; AdapterInstances matches it index for index. */
	UPROPERTY()
	TArray<FSavedAssetRef> Adapters;

	UPROPERTY()
	TArray<FGuid> AdapterInstances;

	UPROPERTY()
	TArray<FSavedAssetRef> InnateSpells;

	UPROPERTY()
	TArray<FSavedCustomBDPool> BDSpellPools;

	/** B-380c (D-164, save v29): apparel worn with this loadout, and each piece's owned instance. */
	UPROPERTY()
	TArray<FSavedAssetRef> Apparel;

	UPROPERTY()
	TArray<FGuid> ApparelInstances;
};

/** B-311: one character's custom loadouts (UPoolSubsystem::GetCharacterKey). */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedCharacterLoadouts
{
	GENERATED_BODY()

	UPROPERTY()
	FName Character;

	UPROPERTY()
	TArray<FSavedCustomLoadout> Loadouts;
};

/** B-316b (Q-316b, save v16): one character's learned skills (present once banked, even when empty). */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedCharacterSkills
{
	GENERATED_BODY()

	UPROPERTY()
	FName Character;

	UPROPERTY()
	TArray<FSavedOwnedItem> Spells;

	UPROPERTY()
	TArray<FSavedOwnedItem> Abilities;
};

/** B-322 (D-117, save v9): one character's pouch. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedPouch
{
	GENERATED_BODY()

	UPROPERTY()
	FName Character;

	/** One stack per slot. */
	UPROPERTY()
	TArray<FSavedCrystalStack> Stacks;

	UPROPERTY()
	int32 SlotCount = 0;
};

/** UPoolSubsystem's owned items. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedPool
{
	GENERATED_BODY()

	/** B-091: who wears each piece of the shared armoury. */
	UPROPERTY()
	TArray<FSavedWearer> Wearers;

	/** B-321 (D-117, save v7): who owns each piece (GearID -> character). Empty in an older save: each piece goes
	 *  to its wearer, else the party leader, at the next draw. */
	UPROPERTY()
	TArray<FSavedWearer> Owners;

	/** B-091: characters whose starting gear is already in the pool (it isn't banked twice). */
	UPROPERTY()
	TArray<FName> BankedCharacters;

	/** B-308 (D-110, save v5): owned weapons and adapters in one list; the kind follows from the asset. */
	UPROPERTY()
	TArray<FSavedOwnedItem> Gear;

	UPROPERTY()
	TArray<FSavedOwnedItem> Evolutions;

	UPROPERTY()
	TArray<FSavedOwnedItem> Spells;

	UPROPERTY()
	TArray<FSavedOwnedItem> Abilities;

	/** Storage (D-097): crystals and stones not carried. */
	UPROPERTY()
	TArray<FSavedCrystalStack> Crystals;

	/** B-322 (D-117, save v9): each character's pouch. */
	UPROPERTY()
	TArray<FSavedPouch> Pouches;

	/** B-203: characters whose starting items are already carried or stored. */
	UPROPERTY()
	TArray<FName> BankedItemCharacters;

	/** B-311 (save v6): each character's custom loadouts. */
	UPROPERTY()
	TArray<FSavedCharacterLoadouts> CustomLoadouts;

	/** B-314b (D-097, save v15): gear definition key -> the highest tier a run-only copy of it reached. */
	UPROPERTY()
	TMap<FName, EItemTier> UpgradeMemory;

	/** B-316b (save v16): each character's learned skills. */
	UPROPERTY()
	TArray<FSavedCharacterSkills> CharacterSkills;

	int32 Num() const
	{
		int32 Carried = 0;
		for (const FSavedPouch &Pouch : Pouches)
		{
			Carried += Pouch.Stacks.Num();
		}
		return Gear.Num() + Evolutions.Num() + Spells.Num() + Abilities.Num() + Crystals.Num() + Carried;
	}
};

/** B-279: each character form's loadout-match meter, by form asset name (0..1; 1 = unlocked). */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedForms
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FName, float> Progress;

	/** B-279b (save v8): the worn skin's form asset name (None: no skin). */
	UPROPERTY()
	FName WornForm;

	/** B-279b: EPlayerBaseForm as a byte (0 Manny, 1 Quinn). */
	UPROPERTY()
	uint8 BaseForm = 0;

	/** B-279c (save v10): the current outfit, one soft path per EOutfitSlot (empty in an older save: no outfit). */
	UPROPERTY()
	TArray<FSoftObjectPath> CurrentOutfit;
};

/** B-377: one hidden quest's progress (MemoryCatalogue §3.6): steps done are kept across runs. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedQuestProgress
{
	GENERATED_BODY()

	UPROPERTY()
	FName QuestId;

	UPROPERTY()
	int32 StepsDone = 0;

	UPROPERTY()
	bool bFinished = false;

	/** Q-397 (save v35): when this player found it (higher = newer), for the journal's newest-first order. */
	UPROPERTY()
	int32 FoundOrder = 0;
};

/** B-377: this player's hidden quests (save v25). */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedHiddenQuests
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FSavedQuestProgress> Quests;
};

/** B-378: the curses this player holds (save v26); they last across runs until removed. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedCurses
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FName> Held;

	/** Lifted before (Q-378 changed: a lifted curse can be regained; this list feeds logging and WasLifted). */
	UPROPERTY()
	TArray<FName> Lifted;
};

/** B-379: one hidden counter's count for a character (None: all characters). */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedCounter
{
	GENERATED_BODY()

	UPROPERTY()
	FName Counter;

	UPROPERTY()
	FName Character;

	UPROPERTY()
	int32 Count = 0;
};

/** B-379: this player's hidden counters (save v27); they keep counting across runs. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedCounters
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FSavedCounter> Counts;
};

/** B-377b: a lore entry the journal kept. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedLore
{
	GENERATED_BODY()

	UPROPERTY()
	FName LoreId;

	UPROPERTY()
	FText Title;

	UPROPERTY()
	FText Body;
};

/** B-377b: this player's memory journal (save v28; it was transient before): memories in order with where each came
 *  from (EMemorySource as a byte), lore, V lines heard, conversations played. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedJournal
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FSavedAssetRef> Memories;

	UPROPERTY()
	TArray<uint8> Sources;

	UPROPERTY()
	TArray<FSavedLore> Lore;

	UPROPERTY()
	TArray<FName> HeardVLines;

	UPROPERTY()
	TArray<FName> PlayedConversations;
};

/** B-398: the player's own settings (the settings save, D-024's screen later). They stay this player's on a host
 *  hand-over (USaveSubsystem::AdoptCheckpoint). Enums are kept as their byte; readers fall back to the default on an
 *  unknown value. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FSavedSettings
{
	GENERATED_BODY()

	/** ETurnViewStyle; 0 = Centred Wide, the default (D-074). */
	UPROPERTY()
	uint8 TurnViewStyle = 0;

	/** B-170e (Crown, 2026-11-12): hide the End Turn button; false = shown, the default. An older save has no such field
	 *  and reads false, which is the default: no save version bump. */
	UPROPERTY()
	bool bHideEndTurnButton = false;
};

/** Everything one save slot holds. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FWoRSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	FSavedParty Party;

	UPROPERTY()
	FSavedWallet Wallet;

	UPROPERTY()
	FSavedPool Pool;

	UPROPERTY()
	FSavedRun Run;

	UPROPERTY()
	FSavedForms Forms;

	UPROPERTY()
	FSavedHiddenQuests HiddenQuests; // B-377

	UPROPERTY()
	FSavedCurses Curses; // B-378

	UPROPERTY()
	FSavedCounters Counters; // B-379

	UPROPERTY()
	FSavedJournal Journal; // B-377b

	UPROPERTY()
	FSavedSettings Settings; // B-398

	/** B-467: time played, in seconds, for the title's Continue line. Added with no version bump: an older 38 loads 0. */
	UPROPERTY()
	double PlaySeconds = 0.0;
};
