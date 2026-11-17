// EnemyKit.h
// B-567 (D-225, TrialPoolDesign §3-4 and §9 stage 3, the enemy's parts): what an enemy template sets for its kit.
//   - The always-has list: gear (each at the tier it is given) and skills, the same on every enemy of the type.
//   - Limits on what it takes from the pool (Crown 2026-11-17, "the limits merged"): most gear, most skills per
//     piece, most innate spells, which kinds, which elements, a tier range. Each is optional and unset means any:
//     skills fill every slot a piece has, innate places follow D-240 (GetMostInnate).
// Enemies follow the player's rules: the class decides which kinds fit and caps them (LoadoutClassTable), and slots
// come by tier. B-567b: most weapons, most adapters and evolution allowed became most gear and the kinds list.
// Stage 4 rolls the open part from the trial's pool inside these. Debug: UCharacterData's Print Enemy Kit button
// (GetEnemyKitString); wor.Trial.PrintZones warns when an always-has piece's tier is outside its zone's range.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Weapons/EWeaponType.h"
#include "Inventory/ItemTier.h"
#include "Skills/Definitions/ESpellElement.h"
#include "EnemyKit.generated.h"

class UCharacterData;
class USkillDataBase;
struct FEncounterZoneInfo;
struct FTrialTierRange;
enum class ECharacterClass : uint8;

/** A kind of gear an enemy may take from the pool. */
UENUM(BlueprintType)
enum class EEnemyKitKind : uint8
{
	Weapon,
	Adapter,
	Evolution,
	Apparel
};

/** One piece an enemy always has, at the tier it is given (it wins over the zone's range). */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FEnemyKitPiece
{
	GENERATED_BODY()

	/** A weapon, an adapter, a prebuilt find (arrives built), an evolution or apparel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit",
			  meta = (AllowedClasses = "/Script/world_of_refraction.EquipmentDataBase,/Script/world_of_refraction.PrebuiltFindData,/Script/world_of_refraction.EvolutionItemData"))
	TObjectPtr<UObject> Item = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit")
	EItemTier Tier = EItemTier::F_Tier;
};

/** An enemy's kit, on its template. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FEnemyKit
{
	GENERATED_BODY()

	// ==================== ALWAYS HAS ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Always Has")
	TArray<FEnemyKitPiece> Pieces;

	/** Always slotted first; the rest are rolled from what its gear can use (D-224). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Always Has")
	TArray<TObjectPtr<USkillDataBase>> Skills;

	// ==================== LIMITS (unset = any) ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits", meta = (InlineEditConditionToggle))
	bool bLimitGear = false;
	/** Most gear: weapons, adapters and an evolution together, the always-has ones included (apparel has its own
	 *  place). Its class decides which kinds fit and caps it. Unset: as many as its class carries. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits", meta = (EditCondition = "bLimitGear", ClampMin = 0))
	int32 MostGear = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits", meta = (InlineEditConditionToggle))
	bool bLimitSkillsPerPiece = false;
	/** B-567c (Crown): how many skills each piece may hold, never more than its slots. Unset: every slot it has. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits", meta = (EditCondition = "bLimitSkillsPerPiece", ClampMin = 0))
	int32 MostSkillsPerPiece = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits", meta = (InlineEditConditionToggle))
	bool bLimitInnate = false;
	/** B-567c (Crown): most innate spells it keeps. Unset: its innate places (D-240: 3, plus 1 per pillar point). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits", meta = (EditCondition = "bLimitInnate", ClampMin = 0))
	int32 MostInnate = 0;

	/** Which kinds of gear it may take (an evolution only when Evolution is listed). Empty: any. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits")
	TArray<EEnemyKitKind> Kinds;

	/** Which weapon types it may take. Empty: any. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits")
	TArray<EWeaponType> WeaponTypes;

	/** Which elements its crystals and spells may be. Empty: any. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits")
	TArray<ESpellElement> Elements;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits", meta = (InlineEditConditionToggle))
	bool bLimitTiers = false;
	/** The tiers it may take, inside its zone's range. Unset: any tier its zone holds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits", meta = (EditCondition = "bLimitTiers"))
	EItemTier MinTier = EItemTier::F_Tier;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits", meta = (EditCondition = "bLimitTiers"))
	EItemTier MaxTier = EItemTier::S_Tier;
};

namespace EnemyKitRules
{
	/** D-240: innate places start at 3 and grow by 1 per pillar point, up to today's full pool (6 in each of 4 schools). */
	inline constexpr int32 INNATE_BASE_PLACES = 3;
	inline constexpr int32 INNATE_PLACES_PER_PILLAR_POINT = 1;
	inline constexpr int32 MAX_INNATE_PLACES = 24;

	/** The kind of an always-has or pool item (a prebuilt find by its piece); false when it is no gear. */
	WORLD_OF_REFRACTION_API bool GetKind(const UObject *Item, EEnemyKitKind &OutKind);
	/** The class's item count (a Resonator's: its weapons and adapters), lowered by the kit's most gear; NO_LIMIT when
	 *  neither caps it. */
	WORLD_OF_REFRACTION_API int32 GetMostGear(const FEnemyKit &Kit, ECharacterClass Class);
	/** How many of Kind the class carries (NO_LIMIT: no own cap); 0 when the kit leaves the kind out. Apparel and
	 *  evolutions: NO_LIMIT here (an evolution still counts toward most gear). */
	WORLD_OF_REFRACTION_API int32 GetMostOfKind(const FEnemyKit &Kit, ECharacterClass Class, EEnemyKitKind Kind);
	WORLD_OF_REFRACTION_API bool AllowsKind(const FEnemyKit &Kit, EEnemyKitKind Kind);
	/** How many skills one piece with PieceSlots slots may hold: its slots, lowered by the kit's per-piece limit. */
	WORLD_OF_REFRACTION_API int32 GetMostSkillsOnPiece(const FEnemyKit &Kit, int32 PieceSlots);
	/** D-240's innate places at these pillar levels (Mind + Body + Spirit). */
	WORLD_OF_REFRACTION_API int32 GetInnatePlaces(int32 PillarPoints);
	/** The kit's innate limit, else Enemy's innate places by its own levels. */
	WORLD_OF_REFRACTION_API int32 GetMostInnate(const UCharacterData &Enemy);
	WORLD_OF_REFRACTION_API bool AllowsWeaponType(const FEnemyKit &Kit, EWeaponType Type);
	WORLD_OF_REFRACTION_API bool AllowsElement(const FEnemyKit &Kit, ESpellElement Element);
	/** The zone's range, narrowed by the kit's own; Min above Max when the two don't meet (nothing to take). */
	WORLD_OF_REFRACTION_API FTrialTierRange GetTierRange(const FEnemyKit &Kit, const FTrialTierRange &ZoneRange);

	/** Problems with the always-has list against the class and the kit's own limits (too many weapons, adapters or
	 *  gear, a kind or weapon type left out, an upside-down tier range, a piece of an unknown kind); empty when it fits. */
	WORLD_OF_REFRACTION_API TArray<FString> CheckAlwaysHas(const FEnemyKit &Kit, ECharacterClass Class);
	/** One line per always-has piece whose tier is outside Zone's range (the given tier wins; TrialPoolDesign §10 note
	 *  5), else empty. */
	WORLD_OF_REFRACTION_API FString DescribeOutsideZoneRange(const FString &Label, const FEncounterZoneInfo &Zone, TConstArrayView<UCharacterData *> Roster);

	WORLD_OF_REFRACTION_API FString GetEnemyKitString(const FEnemyKit &Kit, ECharacterClass Class);
}
