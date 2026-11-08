// CombatPresentationSpec.cpp
// B-231 part 1: who sends the presentation multicasts and who plays a received montage; standalone, a hit still
// fires the local event and a montage sends nothing. The two-machine path is the PIE sheet (docs/PIE/B-231.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/CombatPresentationComponent.h"
#include "Animation/AnimMontage.h"
#include "WoRTestWorld.h"

BEGIN_DEFINE_SPEC(FCombatPresentationSpec, "WoR.Net.Presentation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UCombatPresentationComponent *Presentation = nullptr;
END_DEFINE_SPEC(FCombatPresentationSpec)

void FCombatPresentationSpec::Define()
{
	It("sends only from a server, and plays a received montage only off the server", [this]()
	{
		TestTrue(TEXT("listen server sends"), CombatPresentation::SendsFromHere(NM_ListenServer));
		TestTrue(TEXT("dedicated server sends"), CombatPresentation::SendsFromHere(NM_DedicatedServer));
		TestFalse(TEXT("standalone: no one to send to"), CombatPresentation::SendsFromHere(NM_Standalone));
		TestFalse(TEXT("a client never sends"), CombatPresentation::SendsFromHere(NM_Client));
		TestTrue(TEXT("a client plays it"), CombatPresentation::PlaysReceivedMontage(false));
		TestFalse(TEXT("the server already played it"), CombatPresentation::PlaysReceivedMontage(true));
	});

	Describe("standalone", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("world"), TestWorld->Create());
			AActor *Owner = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<AActor>() : nullptr;
			Presentation = Owner ? NewObject<UCombatPresentationComponent>(Owner) : nullptr;
			if (Presentation)
			{
				Presentation->RegisterComponent();
			}
		});
		AfterEach([this]()
		{
			Presentation = nullptr;
			TestWorld.Reset();
		});

		It("still shows a hit on this machine", [this]()
		{
			if (!TestNotNull(TEXT("setup"), Presentation))
			{
				return;
			}
			Presentation->SendHit(42, true);
			TestTrue(TEXT("one hit shown, a crit"), Presentation->GetDebugString().Contains(TEXT("1 hit(s) shown, last 42 (crit)")));
		});

		It("sends no montage (no clients)", [this]()
		{
			if (!TestNotNull(TEXT("setup"), Presentation))
			{
				return;
			}
			Presentation->SendMontage(NewObject<UAnimMontage>(GetTransientPackage()), 1.0f);
			TestTrue(TEXT("nothing sent"), Presentation->GetDebugString().Contains(TEXT("(0 sent, 0 received)")));
		});
	});
}

#endif
