// CharacterDataReplicationSpec.cpp
// B-131 (co-op, D-103): UCharacterDataComponent replicates, with HP, EP, the max pools, alive and Broken
// Darkness among its replicated properties. Seeing HP on a listen client needs two players in PIE
// (docs/PIE/B-131.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Character/CharacterDataComponent.h"
#include "UObject/UnrealType.h"

BEGIN_DEFINE_SPEC(FCharacterDataReplicationSpec, "WoR.Coop.CharacterDataReplication",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FCharacterDataReplicationSpec)

void FCharacterDataReplicationSpec::Define()
{
	It("replicates by default", [this]()
	{
		TestTrue(TEXT("Replicated"), GetDefault<UCharacterDataComponent>()->GetIsReplicated());
	});

	It("replicates HP, EP, the max pools, alive, Broken Darkness and the template", [this]()
	{
		for (const TCHAR *Name : {TEXT("CurrentHP"), TEXT("CurrentEP"), TEXT("MaxHP"), TEXT("MaxEP"), TEXT("bIsAlive"), TEXT("bIsBrokenDarkness"), TEXT("CharacterData")})
		{
			const FProperty *Property = FindFProperty<FProperty>(UCharacterDataComponent::StaticClass(), Name);
			TestTrue(FString::Printf(TEXT("%s replicates"), Name), Property && Property->HasAnyPropertyFlags(CPF_Net));
		}
	});
}

#endif
