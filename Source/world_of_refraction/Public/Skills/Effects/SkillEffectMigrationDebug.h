// SkillEffectMigrationDebug.h
// F2 migration tooling. Re-saves every Effects-carrying data asset so the new-shape
// Conditions[]/Payloads[] (populated on load by FSkillEffect::MigrateLegacyToNew) are
// written to disk, and verifies the on-disk result so F3 can delete the migration.
// EDITOR-ONLY: asset-registry enumeration + re-save are editor operations (bodies are
// WITH_EDITOR-guarded). Run order: ResaveAllEffectAssets -> VerifyAllEffectAssets.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SkillEffectMigrationDebug.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogWoRMigration, Log, All);

/**
 * Mirrors the project *Debug library pattern (BlueprintCallable statics). Run from an
 * Editor Utility Widget/Blueprint. NOTE: CallInEditor buttons need a UObject instance,
 * which a static library lacks — convert to an AActor if one-click buttons are wanted.
 */
UCLASS()
class WORLD_OF_REFRACTION_API USkillEffectMigrationDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** F2 step 1 — re-save every USkillDataBase / UEquipmentDataBase / UEvolutionItemData
	 *  asset so migrated Conditions[]/Payloads[] persist to disk. Does NOT verify. Run FIRST. */
	UFUNCTION(BlueprintCallable, Category = "WoR|Migration")
	static void ResaveAllEffectAssets();

	/** F2 step 2 — verify every Effects-carrying asset has on-disk Payloads[] AND passes
	 *  CompareEffect parity. ZERO failures gates F3. Read-only and repeatable. Run SECOND. */
	UFUNCTION(BlueprintCallable, Category = "WoR|Migration")
	static void VerifyAllEffectAssets();

private:
	/** Load every Effects-carrying carrier via the AssetRegistry (recursive classes,
	 *  try-guarded — a null/failed load is logged and skipped, never crashes the batch). */
	static TArray<UObject *> GetAllEffectCarriers();
};
