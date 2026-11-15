// HubHUDSpec.cpp
// B-443 (D-181): the hub HUD's model. When the money chip shows; the interact key's label per device; every prompt
// verb has a word; a rest spot prompts "Rest" and a memory spot prompts nothing (it stays unannounced); nothing in
// reach means no prompt. Where the prompt floats and how it looks is the PIE sheet (docs/PIE/B-443.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Character/CharacterData.h"
#include "Field/FieldSkillTypes.h"
#include "Infusion/ElementColors.h"
#include "Party/Party.h"
#include "Hub/InteractPrompt.h"
#include "Memory/MemorySpot.h"
#include "Trial/TrialRestSpot.h"
#include "UI/Hub/HubHUDModel.h"
#include "UI/Hub/HubHUDWidget.h"
#include "UI/Menus/MenuScreenStyle.h"
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

	It("shows a party card per member: the element dot, the name, the carried HP (B-503g)", [this]()
	{
		UParty *Party = NewObject<UParty>(GetTransientPackage());
		const auto AddMember = [Party](const TCHAR *Name, int32 CurrentHP) -> UCharacterData *
		{
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->Name = Name;
			FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
			Member.CharacterData = Data;
			Member.CurrentHP = CurrentHP;
			return Data;
		};
		UCharacterData *Hub = AddMember(TEXT("Sera"), PartyConstants::NO_CARRIED_VITALS);
		UCharacterData *Hurt = AddMember(TEXT("Bram"), 0);
		Hurt->CharacterClass = ECharacterClass::Caster;
		Hurt->InnateElement = ESpellElement::Fire;
		Party->Members.Last().CurrentHP = Hurt->CalculateMaxHealth() / 2;
		Party->Members.AddDefaulted(); // a member with no data shows no card

		const TArray<FHubHUDPartyCard> Cards = WoRHubHUD::BuildPartyStrip(Party);
		if (!TestEqual(TEXT("one card per member with data"), Cards.Num(), 2))
		{
			return;
		}
		TestEqual(TEXT("party order, the character's name"), Cards[0].Name.ToString(), Hub->Name);
		TestEqual(TEXT("no carried vitals reads full"), Cards[0].HPShare, 1.0f);
		TestEqual(TEXT("a non-Caster's dot is the generic colour"), Cards[0].DotColor, ElementColors::GetColorForElement(ESpellElement::Generic));
		TestEqual(TEXT("a Caster's dot is its element's colour"), Cards[1].DotColor, ElementColors::GetColorForElement(ESpellElement::Fire));
		TestEqual(TEXT("carried HP over max"), Cards[1].HPShare,
				  static_cast<float>(Hurt->CalculateMaxHealth() / 2) / static_cast<float>(Hurt->CalculateMaxHealth()), UE_KINDA_SMALL_NUMBER);
		TestEqual(TEXT("no party, no cards"), WoRHubHUD::BuildPartyStrip(nullptr).Num(), 0);
	});

	It("fits a full party and the field attack chip on one row, every card between its widths (B-503i)", [this]()
	{
		TestTrue(TEXT("a card can grow past the mockup's width"), WoRHubHUD::PARTY_CARD_MAX_WIDTH > WoRHubHUD::PARTY_CARD_MIN_WIDTH);
		TestTrue(TEXT("the chip can grow past its minimum"), WoRHubHUD::FIELD_ATTACK_CHIP_MAX_WIDTH > WoRHubHUD::FIELD_ATTACK_CHIP_MIN_WIDTH);
		// The strip starts at the screen's padding on the left and keeps the same padding clear on the right.
		const float Room = WoRHubHUD::HUD_LAYOUT_WIDTH - 2.0f * WoRMenuStyle::ScreenPadding;
		TestTrue(TEXT("four widest cards and the widest chip fit"), WoRHubHUD::GetPartyStripMaxWidth(PartyConstants::MAX_PARTY_MEMBERS) <= Room);
	});

	It("shows the field attack chip's name, table icon and element dot (B-503i)", [this]()
	{
		FFieldAttackEntry Sword;
		Sword.Kind = EFieldAttackKind::Weapon;
		Sword.Name = FText::FromString(TEXT("Generic Primary Weapon"));
		Sword.WeaponType = EWeaponType::Sword;
		Sword.Element = ESpellElement::Fire;
		const FHubHUDFieldAttackChip SwordChip = WoRHubHUD::BuildFieldAttackChip(Sword);
		TestEqual(TEXT("the item's name"), SwordChip.Name.ToString(), Sword.Name.ToString());
		TestTrue(TEXT("a weapon's table icon is its type's"), SwordChip.Icon.Kind == EWoRIconKind::WeaponType && SwordChip.Icon.WeaponType == EWeaponType::Sword);
		TestEqual(TEXT("its type in words with no texture"), SwordChip.IconText.ToString(), WoRGameNames::GetWeaponTypeName(EWeaponType::Sword).ToString());
		TestTrue(TEXT("elemental"), SwordChip.bElemental);
		TestEqual(TEXT("the dot is the element's colour"), SwordChip.ElementColor, ElementColors::GetColorForElement(ESpellElement::Fire));

		FFieldAttackEntry Ring;
		Ring.Kind = EFieldAttackKind::Ring;
		Ring.Name = FText::FromString(TEXT("Ring"));
		Ring.Element = ESpellElement::Water;
		const FHubHUDFieldAttackChip RingChip = WoRHubHUD::BuildFieldAttackChip(Ring);
		TestTrue(TEXT("a ring's table icon is its element's badge"), RingChip.Icon.Kind == EWoRIconKind::Element && RingChip.Icon.Element == ESpellElement::Water);

		const FHubHUDFieldAttackChip Plain = WoRHubHUD::BuildFieldAttackChip(FFieldAttackEntry());
		TestFalse(TEXT("the plain swing has a name"), Plain.Name.IsEmpty());
		TestTrue(TEXT("no icon"), Plain.Icon.Kind == EWoRIconKind::None && Plain.IconText.IsEmpty());
		TestFalse(TEXT("no element: no dot"), Plain.bElemental);
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

	It("writes a prompt as one line: the key, the verb, the thing (B-491, D-213)", [this]()
	{
		const FString Key = WoRHubHUD::GetInteractKeyLabel(false).ToString();
		const FString Chest = WoRHubHUD::FormatPromptLine(FInteractPrompt(EInteractVerb::Open, FText::FromString(TEXT("Chest")))).ToString();
		TestTrue(TEXT("key, verb, a dot, the thing"), Chest.StartsWith(Key) && Chest.Contains(TEXT("Open · Chest")));
		TestFalse(TEXT("no sentence"), Chest.Contains(TEXT("Press")));
		const FString Rest = WoRHubHUD::FormatPromptLine(FInteractPrompt(EInteractVerb::Rest)).ToString();
		TestTrue(TEXT("no thing: the key and the verb"), Rest.StartsWith(Key) && Rest.EndsWith(TEXT("Rest")) && !Rest.Contains(TEXT("·")));
		TestTrue(TEXT("no prompt: nothing"), WoRHubHUD::FormatPromptLine(FInteractPrompt()).IsEmpty());
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
