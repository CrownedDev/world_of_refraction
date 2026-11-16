// ClassDefaultLoadoutData.h
// D-221 stage 1c (B-511): the kit every trial starts on, per class, so every run has the same starting point. One asset,
// DA_Loadout_ClassDefaults, found by UTrialRunSubsystem's config path (DefaultGame.ini). Each character's kit is minted
// into the pool once as its own (UPoolSubsystem::MintClassDefaultKit). Plan: docs/Design/LoadoutRestructurePlan.md §3.
// B-549 (D-228, one-player stage 2): also the class table: what each class, and a Refractor's element choice, gives
// a character (world levels, action speed, portrait, innate spells). Stage 3 builds the player from it and the picks.
// Filled once from today's Lords (FillFromSources, an editor button). Resistances stay in ClassInnateResistanceTable.

#pragma once

#include "CoreMinimal.h"
#include "Character/ECharacterClass.h"
#include "Engine/DataAsset.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Loadout/FCombatLoadout.h"
#include "Skills/Definitions/ESpellElement.h"
#include "ClassDefaultLoadoutData.generated.h"

class UAbilityData;
class UApparelData;
class UCharacterData;
class USpellData;
class UTexture2D;
class UEquipmentDataBase;
class USkillDataBase;
class UWeaponData;

/** One kit piece: a weapon or an adapter, at its authored tier (the asset's). */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FClassDefaultPiece
{
	GENERATED_BODY()

	/** A weapon (UWeaponData) or an adapter (URingData). Empty: this place stays empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit")
	TObjectPtr<UEquipmentDataBase> Piece = nullptr;

	/** Optional: the second weapon of a same-type pair (D-209). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit")
	TObjectPtr<UWeaponData> OffHand = nullptr;

	/** A weapon's pick per skill slot (empty: the weapon's built-ins). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit")
	TArray<TObjectPtr<UAbilityData>> SkillPicks;

	/** B-522 (D-223, asset overhaul §1): the gem minted into the piece's socket, at full durability. Read for a
	 *  character's StartingGear (minted once), not for the trial kit (a kit reset would mint a crystal from nowhere each
	 *  run). Type None: an empty socket. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit")
	FCrystalId Crystal;

	/** D-231 (B-532; on by default, Crown): a piece handed out with its Crystal counts as prebuilt; on, that crystal may
	 *  be taken out (destroyed). Off: it stays as built. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit")
	bool bCrystalRemovable = true;
};

/** One class's kit. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FClassDefaultLoadout
{
	GENERATED_BODY()

	// shortcut: in the WornGearLayout places (0 the primary, 1 the Generic's second weapon, 2+ adapters) until stage 2
	// drops the fixed places; leave a place's Piece empty to skip it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit")
	TArray<FClassDefaultPiece> Pieces;

	/** The place of the piece in hand at the start (the ACTIVE entry). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit", meta = (ClampMin = 0))
	int32 ActiveIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit")
	TArray<TObjectPtr<UApparelData>> Apparel;

	/** B-522 (D-224 (2), §5.3): the skills a run starts with, in the run's list, not in a slot (Q9). Field only: stage
	 *  S3 reads it. TODO: tune the starting skills (D-224). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit")
	TArray<TObjectPtr<USkillDataBase>> StartingSkills;
};

/** A character's minted kit: the owned GUIDs, place for place with FClassDefaultLoadout (invalid for an empty place). */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FClassDefaultKitIDs
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Kit")
	TArray<FGuid> Pieces;

	UPROPERTY(BlueprintReadOnly, Category = "Kit")
	TArray<FGuid> OffHands;

	UPROPERTY(BlueprintReadOnly, Category = "Kit")
	TArray<FGuid> Apparel;

	bool Contains(const FGuid &GearID) const
	{
		return GearID.IsValid() && (Pieces.Contains(GearID) || OffHands.Contains(GearID) || Apparel.Contains(GearID));
	}
};

/** B-549: what a class, or one of a Refractor's element choices, gives a character. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FClassElementNumbers
{
	GENERATED_BODY()

	/** A Refractor's element; Broken Darkness is Darkness with the flag. Unused on a class row. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class table")
	ESpellElement Element = ESpellElement::Darkness;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class table")
	bool bBrokenDarkness = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class table", meta = (ClampMin = 0, ClampMax = 7))
	int32 WorldMindLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class table", meta = (ClampMin = 0, ClampMax = 7))
	int32 WorldBodyLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class table", meta = (ClampMin = 0, ClampMax = 7))
	int32 WorldSpiritLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class table")
	TSoftObjectPtr<UTexture2D> Portrait; // D-057: a new visual field is soft

	/** A Refractor's innate spells (for Broken Darkness: the Darkness pool). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class table")
	TArray<TObjectPtr<USpellData>> InnateSpells;

	/** Broken Darkness only: the element pools, locked until absorbed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class table")
	TArray<FBDElementSpellPool> BDSpellPools;
};

/** B-549: one class's row; a Refractor's numbers come from its element rows. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FClassNumbers
{
	GENERATED_BODY()

	/** The class's own world levels and portrait (Generic, Resonator); a Refractor reads its element row instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class table")
	FClassElementNumbers Base;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class table", meta = (ClampMin = 0))
	int32 ActionSpeed = 0;

	/** A Refractor's element choices (the nine elements and Broken Darkness). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class table", meta = (TitleProperty = "Element"))
	TArray<FClassElementNumbers> Elements;
};

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UClassDefaultLoadoutData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** TODO: an F-tier kit per class (Generic, Refractor, Resonator), picked from the existing F-tier gear. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit")
	TMap<ECharacterClass, FClassDefaultLoadout> Defaults;

	/** B-549: the class table (D-228). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class table")
	TMap<ECharacterClass, FClassNumbers> Classes;

	/** B-549: the characters FillFromSources reads (today's Lords). Transient, so the table keeps no hold on them. */
	UPROPERTY(EditAnywhere, Transient, Category = "Class table|Fill")
	TArray<TObjectPtr<UCharacterData>> FillSources;

	/** B-549: what Class (and a Refractor's Element, Broken Darkness) starts with. False: no row for it. */
	bool FindNumbers(ECharacterClass Class, ESpellElement Element, bool bBrokenDarkness, FClassElementNumbers &OutNumbers,
					 int32 &OutActionSpeed) const;

	/** B-549: one row per source: a Generic or Resonator its class row, a Caster its element row (world levels, action
	 *  speed, portrait, its inventory's default loadout's innate spells and BD pools). Returns the rows written. */
	int32 FillFrom(const TArray<UCharacterData *> &Sources);

	/** The editor step: FillFrom(FillSources) and mark the asset dirty; saving it is the user's step. */
	UFUNCTION(CallInEditor, Category = "Class table|Fill")
	void FillFromSources();

	/** Each class's kit, one line per piece. */
	FString GetDefaultsString() const;
	/** B-549: the class table, a line per class and element row. */
	FString GetClassTableString() const;

	UFUNCTION(CallInEditor, Category = "Debug")
	void PrintDefaults() const;

	UFUNCTION(CallInEditor, Category = "Debug")
	void PrintClassTable() const;
};
