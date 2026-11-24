// HubHUDSpec.cpp
// B-443 (D-181): the hub HUD's model. When the money chip shows; the interact key's label per device; every prompt
// verb has a word; a rest spot prompts "Rest" and a memory spot prompts nothing (it stays unannounced); nothing in
// reach means no prompt. Where the prompt floats and how it looks is the PIE sheet (docs/PIE/B-443.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Character/CharacterData.h"
#include "Character/PlayerBuild.h"
#include "Field/FieldSkillTypes.h"
#include "Infusion/ElementColors.h"
#include "Party/Party.h"
#include "Hub/InteractPrompt.h"
#include "Memory/MemorySpot.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRestSpot.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Combat/Battle/BattleHUDView.h"
#include "UI/Hub/HubHUDModel.h"
#include "UI/Hub/HubHUDWidget.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "WoRGameNames.h"
#include "WoRTestWorld.h"

namespace
{
	constexpr float HUB_HUD_TEST_JUST_CHANGED = 1.0f;
	constexpr float HUB_HUD_TEST_NEVER_CHANGED = -1.0f;
	constexpr int32 HUB_HUD_TEST_RUN_SEED = 503;
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

	It("shows the battle card per member: the name, the carried HP and EP (B-503g, B-503j)", [this]()
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

		Party->Members.Last(1).CurrentEP = 0;

		const TArray<FBattleHUDCard> Cards = WoRHubHUD::BuildPartyStrip(Party);
		if (!TestEqual(TEXT("one card per member with data"), Cards.Num(), 2))
		{
			return;
		}
		TestEqual(TEXT("party order, the character's name"), Cards[0].Name.ToString(), Hub->Name);
		TestTrue(TEXT("a party card"), Cards[0].bParty);
		TestEqual(TEXT("no carried vitals reads full"), Cards[0].HPFraction, 1.0f);
		TestEqual(TEXT("full HP in numbers"), Cards[0].HPLine.ToString(),
				  BattleHUDView::FormatPool(BattleHUDView::GetHPLabel(), Hub->CalculateMaxHealth(), Hub->CalculateMaxHealth()).ToString());
		TestEqual(TEXT("full EP in numbers"), Cards[0].EPLine.ToString(),
				  BattleHUDView::FormatPool(BattleHUDView::GetEPLabel(), Hub->CalculateMaxEnergy(), Hub->CalculateMaxEnergy()).ToString());
		TestEqual(TEXT("carried HP over max"), Cards[1].HPFraction,
				  static_cast<float>(Hurt->CalculateMaxHealth() / 2) / static_cast<float>(Hurt->CalculateMaxHealth()), UE_KINDA_SMALL_NUMBER);
		TestEqual(TEXT("carried EP"), Cards[1].EPLine.ToString(),
				  BattleHUDView::FormatPool(BattleHUDView::GetEPLabel(), 0, Hurt->CalculateMaxEnergy()).ToString());
		TestEqual(TEXT("B-503m: the EP bar follows it"), Cards[1].EPFraction, 0.0f);
		TestEqual(TEXT("and a full one is full"), Cards[0].EPFraction,
				  BattleHUDView::PoolFraction(Hub->CalculateMaxEnergy(), Hub->CalculateMaxEnergy()));
		TestEqual(TEXT("the build-up bar is empty between fights (SB-18)"), Cards[1].BuildupFraction, 0.0f);
		TestEqual(TEXT("no party, no cards"), WoRHubHUD::BuildPartyStrip(nullptr).Num(), 0);
		const TArray<FBattleHUDCard> Played = WoRHubHUD::BuildPartyStrip(Party, 1);
		TestTrue(TEXT("B-503m4 (D-217): the played member first, then the rest in party order"),
				 Played.Num() == 2 && Played[0].Name.ToString() == Hurt->Name && Played[1].Name.ToString() == Hub->Name);
	});

	It("shows no Spirit or Body line under a player character's name, in the hub or a trial (D-254)", [this]()
	{
		UPackage *Package = GetTransientPackage();
		UCharacterData *Player = NewObject<UCharacterData>(
			Package, MakeUniqueObjectName(Package, UCharacterData::StaticClass(), *(FString(WoRPlayerBuild::BUILT_NAME) + TEXT("_HubCardSpec"))));
		Player->CharacterClass = ECharacterClass::Caster;
		Player->InnateElement = ESpellElement::Fire;
		UParty *Party = NewObject<UParty>();
		Party->Members.AddDefaulted_GetRef().CharacterData = Player;
		TestFalse(TEXT("the character has identity lines (the Character sheet shows them)"),
				  WoRPlayerBuild::GetDetailLine(Player, Party->Members[0].MemberId, nullptr).IsEmpty());
		// One strip builds the hub's and the trial's cards; the level only picks which parts are drawn.
		const TArray<FBattleHUDCard> Cards = WoRHubHUD::BuildPartyStrip(Party);
		if (TestEqual(TEXT("one card"), Cards.Num(), 1))
		{
			TestTrue(TEXT("nothing under the name"), Cards[0].DetailLine.IsEmpty());
			TestFalse(TEXT("the name still shows"), Cards[0].Name.IsEmpty());
		}
	});

	It("fits a full party on one row (B-503i, B-503j; the chip's room is kept until it is deleted, B-503k)", [this]()
	{
		TestTrue(TEXT("the chip can grow past its minimum"), WoRHubHUD::FIELD_ATTACK_CHIP_MAX_WIDTH > WoRHubHUD::FIELD_ATTACK_CHIP_MIN_WIDTH);
		// The strip starts at the screen's padding on the left and keeps the same padding clear on the right.
		const float Room = WoRHubHUD::HUD_LAYOUT_WIDTH - 2.0f * WoRMenuStyle::ScreenPadding;
		TestTrue(TEXT("four cards and the widest chip fit"), WoRHubHUD::GetPartyStripMaxWidth(PartyConstants::MAX_PARTY_MEMBERS) <= Room);
	});

	It("shows the trial level on every map, run or not (B-503j, B-503m; B-597)", [this]()
	{
		TestTrue(TEXT("no world: the trial's cards (B-597)"), WoRHubHUD::GetCardLevel(nullptr) == EBattleCardLevel::Trial);
		TestTrue(TEXT("not the hub map, no run (a trial map played straight from the editor): the trial's cards"),
				 WoRHubHUD::GetCardLevel(TestWorld->GetWorld()) == EBattleCardLevel::Trial);
		UTrialRunSubsystem *Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("run subsystem"), Run))
		{
			return;
		}
		Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), HUB_HUD_TEST_RUN_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
		TestTrue(TEXT("in a run: the trial's cards (HP, EP, build-up)"), WoRHubHUD::GetCardLevel(TestWorld->GetWorld()) == EBattleCardLevel::Trial);
	});

	It("puts the field attack strip above the played member's card, at the Trial level (B-503k, D-217; B-597)", [this]()
	{
		TestFalse(TEXT("none on a name-only card"), WoRHubHUD::ShowsFieldAttackStrip(EBattleCardLevel::Hub));
		TestTrue(TEXT("at the Trial level (trials and, B-597, the hub)"), WoRHubHUD::ShowsFieldAttackStrip(EBattleCardLevel::Trial));
		TestFalse(TEXT("not the battle's (its card has the gear tooltip)"), WoRHubHUD::ShowsFieldAttackStrip(EBattleCardLevel::Battle));

		UParty *Party = NewObject<UParty>(GetTransientPackage());
		Party->Members.AddDefaulted_GetRef().CharacterData = FWoRTestWorld::NewCharacterData();
		Party->Members.AddDefaulted(); // no data: no card
		Party->Members.AddDefaulted_GetRef().CharacterData = FWoRTestWorld::NewCharacterData();
		TestEqual(TEXT("the first member's card"), WoRHubHUD::GetFieldAttackCardIndex(Party, 0), 0);
		TestEqual(TEXT("B-503m4 (D-217): the played member's card is always the first"), WoRHubHUD::GetFieldAttackCardIndex(Party, 2), 0);
		TestEqual(TEXT("a member with no card"), WoRHubHUD::GetFieldAttackCardIndex(Party, 1), INDEX_NONE);
		TestEqual(TEXT("no slot"), WoRHubHUD::GetFieldAttackCardIndex(Party, INDEX_NONE), INDEX_NONE);
		TestEqual(TEXT("no party"), WoRHubHUD::GetFieldAttackCardIndex(nullptr, 0), INDEX_NONE);
		TestEqual(TEXT("no pawn"), WoRHubHUD::FindFieldAttackCard(nullptr), INDEX_NONE);
	});

	It("opens the quick strip in the hub too, with the trial's cards and card strip (B-528, D-218; B-597)", [this]()
	{
		UWorld *World = TestWorld->GetWorld();
		UTrialRunSubsystem *Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		FSoftObjectProperty *HubLevel = FindFProperty<FSoftObjectProperty>(UTrialRunSubsystem::StaticClass(), TEXT("HubLevel"));
		if (!TestNotNull(TEXT("run subsystem"), Run) || !TestNotNull(TEXT("HubLevel property"), HubLevel))
		{
			return;
		}
		FSoftObjectPtr *HubLevelValue = HubLevel->ContainerPtrToValuePtr<FSoftObjectPtr>(Run);
		const FSoftObjectPtr AuthoredHub = *HubLevelValue;
		*HubLevelValue = FSoftObjectPtr(World); // this test world plays the hub
		TestTrue(TEXT("B-597: the hub shows the trial's cards (HP, EP)"), WoRHubHUD::GetCardLevel(World) == EBattleCardLevel::Trial);
		TestTrue(TEXT("B-597: and the weapon strip above the played card"), WoRHubHUD::ShowsFieldAttackStrip(WoRHubHUD::GetCardLevel(World)));
		UHubHUDWidget *HUD = CreateWidget<UHubHUDWidget>(World, UHubHUDWidget::StaticClass());
		if (TestNotNull(TEXT("HUD created"), HUD))
		{
			HUD->TakeWidget(); // builds the tree
			TestTrue(TEXT("the quick strip is built"), HUD->GetFieldWheelCapacity() > 0);
			FFieldAttackEntry Sword;
			Sword.Kind = EFieldAttackKind::Weapon;
			Sword.SliceName = FText::FromString(TEXT("Arming Sword"));
			TestTrue(TEXT("and opens"), HUD->ShowFieldAttackWheel({Sword}, 0, 0));
			HUD->HideFieldAttackWheel();
		}
		*HubLevelValue = AuthoredHub;
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
