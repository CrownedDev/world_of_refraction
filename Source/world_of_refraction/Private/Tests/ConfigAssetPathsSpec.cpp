// ConfigAssetPathsSpec.cpp
// B-205: asset paths that used to be /Game/ literals in code now come from Config/DefaultGame.ini.
// Each one must be set and point at an asset that exists.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Hub/ExplorationPlayerController.h"
#include "UI/Combat/CombatHUD.h"

namespace
{
	/** Read by reflection, so protected config properties can be checked too. */
	FSoftObjectPath GetConfiguredPath(const UClass *Class, const FName PropertyName)
	{
		const FSoftObjectProperty *Property = FindFProperty<FSoftObjectProperty>(Class, PropertyName);
		return Property ? Property->GetPropertyValue_InContainer(Class->GetDefaultObject()).ToSoftObjectPath()
						: FSoftObjectPath();
	}
}

BEGIN_DEFINE_SPEC(FConfigAssetPathsSpec, "WoR.Config.AssetPaths",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	void ExpectAssetExists(const UClass *Class, const FName PropertyName);
END_DEFINE_SPEC(FConfigAssetPathsSpec)

void FConfigAssetPathsSpec::ExpectAssetExists(const UClass *Class, const FName PropertyName)
{
	const FString Label = FString::Printf(TEXT("%s.%s"), *Class->GetName(), *PropertyName.ToString());
	const FSoftObjectPath Path = GetConfiguredPath(Class, PropertyName);
	if (!TestFalse(Label + TEXT(" is set in DefaultGame.ini"), Path.IsNull()))
	{
		return;
	}
	TestTrue(FString::Printf(TEXT("%s points at an existing asset (%s)"), *Label, *Path.ToString()),
			 FPackageName::DoesPackageExist(Path.GetLongPackageName()));
}

void FConfigAssetPathsSpec::Define()
{
	It("sets the exploration controller's interact action", [this]()
	{
		ExpectAssetExists(AExplorationPlayerController::StaticClass(), TEXT("DefaultInteractAction"));
	});

	// The optional ones (FallbackCharacterPanelClass, DefensePromptClass) may stay empty.
	It("sets every required combat HUD widget class", [this]()
	{
		for (const TCHAR *Property : {TEXT("CharacterPanelClass"), TEXT("TurnOrderStripClass"), TEXT("CommandMenuClass")})
		{
			ExpectAssetExists(UCombatHUD::StaticClass(), Property);
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
