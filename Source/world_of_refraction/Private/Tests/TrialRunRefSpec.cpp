// TrialRunRefSpec.cpp
// B-070: UTrialRunSubsystem holds the active trial as a reflected (UPROPERTY) strong reference, so
// garbage collection can't drop a trial asset mid-run in a packaged build (a weak pointer let it go
// once nothing else referenced the asset).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Trial/TrialRunSubsystem.h"
#include "Trial/TrialData.h"
#include "UObject/UnrealType.h"

BEGIN_DEFINE_SPEC(FTrialRunRefSpec, "WoR.Trial.ActiveTrialRef",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTrialRunRefSpec)

void FTrialRunRefSpec::Define()
{
	It("is a reflected strong reference to the trial", [this]()
	{
		const FObjectPropertyBase *Property = FindFProperty<FObjectPropertyBase>(UTrialRunSubsystem::StaticClass(), TEXT("ActiveTrial"));
		if (!TestNotNull(TEXT("ActiveTrial is a UPROPERTY object reference"), Property)) return;
		TestFalse(TEXT("Not a weak reference"), Property->IsA<FWeakObjectProperty>());
		TestTrue(TEXT("Points at a trial"), Property->PropertyClass && Property->PropertyClass->IsChildOf(UTrialData::StaticClass()));
	});
}

#endif
