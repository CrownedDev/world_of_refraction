// EnemyKit.h
// B-567 (D-225, TrialPoolDesign §3-4 and §9 stage 3, the enemy's parts): what an enemy template sets for its kit.
//   - The always-has list: gear (each at the tier it is given) and skills, the same on every enemy of the type.
//   - Five limits on what it takes from the pool (Crown 2026-11-17, "the limits merged"): most gear, most skills,
//     which kinds, which elements, a tier range. Each is optional and unset means any; the one exception is the
//     skills, unset 3 (DEFAULT_MOST_SKILLS).
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
	bool bLimitSkills = false;
	/** Most skills, the always-has ones included, never more than its slots. Unset: 3. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits", meta = (EditCondition = "bLimitSkills", ClampMin = 0))
	int32 MostSkills = 3;

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
	/** TrialPoolDesign §3: the one limit whose unset value is a number. TODO: tunable. */
	inline constexpr int32 DEFAULT_MOST_SKILLS = 3;

	/** The kind of an always-has or pool item (a prebuilt find by its piece); false when it is no gear. */
	WORLD_OF_REFRACTION_API bool GetKind(const UObject *Item, EEnemyKitKind &OutKind);
	/** The class's item count (a Resonator's: its weapons and adapters), lowered by the kit's most gear. */
	WORLD_OF_REFRACTION_API int32 GetMostGear(const FEnemyKit &Kit, ECharacterClass Class);
	/** How many of Kind the class carries (NO_LIMIT: no own cap); 0 when the kit leaves the kind out. Apparel and
	 *  evolutions: NO_LIMIT here (an evolution still counts toward most gear). */
	WORLD_OF_REFRACTION_API int32 GetMostOfKind(const FEnemyKit &Kit, ECharacterClass Class, EEnemyKitKind Kind);
	WORLD_OF_REFRACTION_API bool AllowsKind(const FEnemyKit &Kit, EEnemyKitKind Kind);
	/** The kit's skill limit (unset 3), never more than Slots. */
	WORLD_OF_REFRACTION_API int32 GetMostSkills(const FEnemyKit &Kit, int32 Slots);
	WORLD_OF_REFRACTION_API bool AllowsWeaponType(const FEnemyKit &Kit, EWeaponType Type);
	WORLD_OF_REFRACTION_API bool AllowsElement(const FEnemyKit &Kit, ESpellElement Element);
	/** The zone's range, narrowed by the kit's own; Min above Max when the two don't meet (nothing to take). */
	WORLD_OF_REFRACTION_API FTrialTierRange GetTierRange(const FEnemyKit &Kit, const FTrialTierRange &ZoneRange);

	/** Problems with the always-has list against the class and the kit's own limits (too many weapons, adapters,
	 *  gear or skills, a kind or weapon type left out, an upside-down tier range, a piece of an unknown kind); empty when it fits. */
	WORLD_OF_REFRACTION_API TArray<FString> CheckAlwaysHas(const FEnemyKit &Kit, ECharacterClass Class);
	/** One line per always-has piece whose tier is outside Zone's range (the given tier wins; TrialPoolDesign §10 note
	 *  5), else empty. */
	WORLD_OF_REFRACTION_API FString DescribeOutsideZoneRange(const FString &Label, const FEncounterZoneInfo &Zone, TConstArrayView<UCharacterData *> Roster);

	WORLD_OF_REFRACTION_API FString GetEnemyKitString(const FEnemyKit &Kit, ECharacterClass Class);
}
