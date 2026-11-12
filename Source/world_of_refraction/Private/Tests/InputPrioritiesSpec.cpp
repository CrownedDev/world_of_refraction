// InputPrioritiesSpec.cpp
// B-129: mapping-context priorities are named (WoRInputPriority), not literals; the combat menu keys sit above the
// combat context, and the exploration controller's default comes from the same table.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Hub/ExplorationPlayerController.h"
#include "Player/InputPriorities.h"
#include "UObject/UnrealType.h"

BEGIN_DEFINE_SPEC(FInputPrioritiesSpec, "WoR.Input.Priorities", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FInputPrioritiesSpec)

void FInputPrioritiesSpec::Define()
{
	It("puts the combat menu keys above the combat context", [this]()
	{
		TestTrue(TEXT("menu keys win a shared key"), WoRInputPriority::COMBAT_MENU_KEYS > WoRInputPriority::COMBAT);
	});

	It("gives the exploration controller its named default", [this]()
	{
		// The field is protected (designer-tunable): read the class default through its UPROPERTY.
		const AExplorationPlayerController *Defaults = GetDefault<AExplorationPlayerController>();
		const FIntProperty *Priority = FindFProperty<FIntProperty>(AExplorationPlayerController::StaticClass(), TEXT("ExplorationMappingPriority"));
		if (TestNotNull(TEXT("class default"), Defaults) && TestNotNull(TEXT("the priority property"), Priority))
		{
			TestEqual(TEXT("exploration priority"), Priority->GetPropertyValue_InContainer(Defaults), WoRInputPriority::EXPLORATION);
		}
	});
}

#endif
