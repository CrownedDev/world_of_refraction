// CombatAnimInitSpec.cpp
// UCombatAnimInstance's init can run more than once on one instance (a mesh swap re-inits it: WoRSpiritLook::Dress).
// The montage notify must stay bound once: a second AddDynamic of the same function trips an engine ensure.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/CombatAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/AutomationTest.h"

BEGIN_DEFINE_SPEC(FCombatAnimInitSpec, "WoR.Combat.AnimInit", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FCombatAnimInitSpec)

void FCombatAnimInitSpec::Define()
{
	It("keeps the montage notify bound once when the init runs twice on one instance", [this]()
	{
		USkeletalMeshComponent *Mesh = NewObject<USkeletalMeshComponent>(GetTransientPackage());
		UAnimInstance *Anim = NewObject<UCombatAnimInstance>(Mesh);
		Anim->NativeInitializeAnimation();
		Anim->NativeInitializeAnimation();
		TestEqual(TEXT("bound once"), Anim->OnPlayMontageNotifyBegin.GetAllObjects().Num(), 1);
	});
}

#endif
