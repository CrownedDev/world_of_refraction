// CookRulesSpec.cpp
// B-278: every data asset type the game finds by an asset-registry scan or a fixed path (nothing references it, so
// a packaged build would leave it out) has an AlwaysCook Asset Manager rule (Config/DefaultGame.ini).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/AssetManagerSettings.h"

namespace
{
	/** The scanned or path-loaded types (docs/Architecture/Packaging.md): a new one goes here and in DefaultGame.ini. */
	const TCHAR *const COOK_RULE_TYPES[] = {
		TEXT("StoryLinesData"),		   // StoryUnlocks::GatherStoryData
		TEXT("BossData"),			   // BossData.cpp registry scan
		TEXT("ReactionTriggerData"),   // ReactionTrigger.cpp registry scan
		TEXT("MemoryData"),			   // MemoryConditions.cpp registry scan
		TEXT("ResistanceProfileData"), // fixed path
		TEXT("CombatTuningData"),	   // fixed path
		TEXT("TrialData"),
		TEXT("CharacterForm"),		   // CharacterFormSubsystem lists them by type (GetPrimaryAssetIdList)
		TEXT("WeaponData"), TEXT("RingData"), TEXT("EvolutionItemData"), // TrialLoot::GatherPool (equipment)
		TEXT("SpellData"), TEXT("AbilityData"),							  // TrialLoot::GatherPool (skills)
	};
}

BEGIN_DEFINE_SPEC(FCookRulesSpec, "WoR.Packaging.CookRules", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FCookRulesSpec)

void FCookRulesSpec::Define()
{
	It("always cooks every data asset type found by scan or path", [this]()
	{
		const UAssetManagerSettings *Settings = GetDefault<UAssetManagerSettings>();
		if (!TestNotNull(TEXT("Asset Manager settings"), Settings))
		{
			return;
		}
		for (const TCHAR *Type : COOK_RULE_TYPES)
		{
			const FPrimaryAssetTypeInfo *Rule = Settings->PrimaryAssetTypesToScan.FindByPredicate(
				[Type](const FPrimaryAssetTypeInfo &Info) { return Info.PrimaryAssetType == FName(Type); });
			if (TestNotNull(FString::Printf(TEXT("%s has a rule"), Type), Rule))
			{
				TestEqual(FString::Printf(TEXT("%s is always cooked"), Type), Rule->Rules.CookRule, EPrimaryAssetCookRule::AlwaysCook);
				TestFalse(FString::Printf(TEXT("%s is not editor-only"), Type), Rule->bIsEditorOnly);
			}
		}
	});
}

#endif
