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

	UPROPERTY()
	TArray<FSavedAssetRef> AssignedSpells;

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
	TArray<FSavedAssetRef> PrimaryStoneAbilities;

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

	UPROPERTY()
	TArray<FSavedAssetRef> SecondaryStoneAbilities;

	/** A Resonator's adapter loadout, in order; AdapterInstances matches it index for index. */
	UPROPERTY()
	TArray<FSavedAssetRef> Adapters;

	UPROPERTY()
	TArray<FGuid> AdapterInstances;

	UPROPERTY()
	TArray<FSavedAssetRef> InnateSpells;

	UPROPERTY()
	TArray<FSavedCustomBDPool> BDSpellPools;
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
};
