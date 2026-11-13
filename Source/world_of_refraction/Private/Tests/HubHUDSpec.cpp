// HubHUDSpec.cpp
// B-443 (D-181): the hub HUD's model. When the money chip shows; the interact key's label per device; every prompt
// verb has a word; a rest spot prompts "Rest" and a memory spot prompts nothing (it stays unannounced); nothing in
// reach means no prompt. Where the prompt floats and how it looks is the PIE sheet (docs/PIE/B-443.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Hub/InteractPrompt.h"
#include "Memory/MemorySpot.h"
#include "Trial/TrialRestSpot.h"
#include "UI/Hub/HubHUDModel.h"
#include "UI/Hub/HubHUDWidget.h"
#include "WoRGameNames.h"
#include "WoRTestWorld.h"

namespace
{
	constexpr float HUB_HUD_TEST_JUST_CHANGED = 1.0f;
	constexpr float HUB_HUD_TEST_NEVER_CHANGED = -1.0f;
	const EInteractVerb HUB_HUD_TEST_VERBS[] = {EInteractVerb::Talk, EInteractVerb::Enter, EInteractVerb::Return,
												EInteractVerb::Invite, EInteractVerb::Dismiss, EInteractVerb::Open,
												EInteractVerb::Read, EInteractVerb::Challenge, EInteractVerb::PickUp,
												EInteractVerb::Rest};
}

BEGIN_DEFINE_SPEC(FHubHUDSpec, "WoR.UI.HubHUD", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FHubHUDSpec)

void FHubHUDSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
	});
	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("shows the money near a vendor, or for a few seconds after it changed", [this]()
	{
		TestTrue(TEXT("near a vendor"), WoRHubHUD::ShouldShowMoney(true, HUB_HUD_TEST_NEVER_CHANGED));
		TestFalse(TEXT("away, and it never changed"), WoRHubHUD::ShouldShowMoney(false, HUB_HUD_TEST_NEVER_CHANGED));
		TestTrue(TEXT("away, just changed"), WoRHubHUD::ShouldShowMoney(false, HUB_HUD_TEST_JUST_CHANGED));
		TestFalse(TEXT("away, changed a while ago"), WoRHubHUD::ShouldShowMoney(false, WoRHubHUD::MONEY_SHOWN_SECONDS));
	});

	It("labels the interact key for the device in use", [this]()
	{
		TestFalse(TEXT("gamepad"), WoRHubHUD::GetInteractKeyLabel(true).IsEmpty());
		TestFalse(TEXT("keyboard"), WoRHubHUD::GetInteractKeyLabel(false).IsEmpty());
		TestFalse(TEXT("not the same key"), WoRHubHUD::GetInteractKeyLabel(true).EqualTo(WoRHubHUD::GetInteractKeyLabel(false)));
	});

	It("has a word for every prompt verb, and none for no prompt", [this]()
	{
		for (const EInteractVerb Verb : HUB_HUD_TEST_VERBS)
		{
			TestFalse(TEXT("a verb is named"), WoRGameNames::GetInteractVerbName(Verb).IsEmpty());
		}
		TestTrue(TEXT("None has no word"), WoRGameNames::GetInteractVerbName(EInteractVerb::None).IsEmpty());
		TestTrue(TEXT("a default prompt is empty"), FInteractPrompt().IsEmpty());
	});

	It("prompts Rest at a rest spot and nothing at a memory spot", [this]()
	{
		const ATrialRestSpot *RestSpot = TestWorld->GetWorld()->SpawnActor<ATrialRestSpot>();
		const AMemorySpot *MemorySpot = TestWorld->GetWorld()->SpawnActor<AMemorySpot>();
		if (!TestNotNull(TEXT("rest spot"), RestSpot) || !TestNotNull(TEXT("memory spot"), MemorySpot))
		{
			return;
		}
		TestTrue(TEXT("Rest"), RestSpot->GetPrompt().Verb == EInteractVerb::Rest);
		TestTrue(TEXT("a memory spot stays unannounced"), MemorySpot->GetPrompt().IsEmpty());
	});

	It("shows no prompt and no money with nothing in reach (the HUD, headless)", [this]()
	{
		TestFalse(TEXT("no pawn: no prompt"), WoRHubHUD::BuildPrompt(nullptr).bShown);
		UHubHUDWidget *HUD = CreateWidget<UHubHUDWidget>(TestWorld->GetWorld(), UHubHUDWidget::StaticClass());
		if (!TestNotNull(TEXT("HUD created"), HUD))
		{
			return;
		}
		HUD->Refresh();
		TestFalse(TEXT("no prompt"), HUD->GetPrompt().bShown);
		TestFalse(TEXT("no money chip"), HUD->IsMoneyShown());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
