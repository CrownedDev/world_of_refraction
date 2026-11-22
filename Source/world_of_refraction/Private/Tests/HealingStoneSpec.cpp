// HealingStoneSpec.cpp
// B-577 (D-252, D-253): the Healing Stone gives no HP. Used on a living target it applies the Healing Boost
// (HealingReceivedBuff) at the tier's percent for the shared consumable duration, so a later heal and a HOT tick are
// raised by it; the boost ends on time; a dead target is refused before anything is spent; the AI's low-HP heal does
// not pick it.
// Unproven here: the fight menu, the card's buff chip and the description on screen (docs/PIE/B-577.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AI/AIDecisionManager.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/TickMath.h"

namespace
{
	constexpr int32 HEALING_STONE_TEST_DAMAGE = 60;
	constexpr int32 HEALING_STONE_TEST_HEAL = 20;
	constexpr int32 HEALING_STONE_TEST_CARRIED = 2;
	/** Low enough for the AI's survival branch at any difficulty. */
	constexpr int32 HEALING_STONE_TEST_LOW_HP = 1;
	const FCrystalId HEALING_STONE_TEST_F(ECrystalType::HealingStone, EItemTier::F_Tier);

	struct FHealingStoneTierRow
	{
		EItemTier Tier;
		float BoostPercent;
		int32 Turns;
	};
	/** D-252: the old heal numbers as percent more healing, on the shared consumable duration curve. */
	const FHealingStoneTierRow HEALING_STONE_TEST_TIERS[] = {
		{EItemTier::F_Tier, 15.0f, 4}, {EItemTier::E_Tier, 20.0f, 4}, {EItemTier::D_Tier, 25.0f, 3}, {EItemTier::C_Tier, 30.0f, 3},
		{EItemTier::B_Tier, 35.0f, 3}, {EItemTier::A_Tier, 45.0f, 2}, {EItemTier::S_Tier, 60.0f, 2}};
}

BEGIN_DEFINE_SPEC(FHealingStoneSpec, "WoR.Items.HealingStone", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *Effects = nullptr;
	UActionExecutor *Executor = nullptr;
	AActor *Hero = nullptr;
	UCharacterDataComponent *Char = nullptr;

	float GetBoost(AActor *Actor) const { return Effects->GetTotalStatModifier(Actor, ESkillEffectType::HealingReceivedBuff); }

	/** A party member with a pouch, spawned as each level does (OneInventorySpec's shape). */
	AActor *SpawnPouchMember(UCharacterData *&OutData) const
	{
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!Party)
		{
			return nullptr;
		}
		OutData = FWoRTestWorld::NewCharacterData();
		Party->Members.Empty();
		FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
		Member.PawnClass = ACombatCharacter::StaticClass();
		Member.CharacterData = OutData;
		AActor *Actor = TestWorld->SpawnCombatant(OutData);
		if (!Actor)
		{
			return nullptr;
		}
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Actor);
		UInventoryComponent *Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Actor);
		Inventory->SavedLoadouts = {FCombatLoadout()};
		Inventory->DrawSharedGear();
		return Actor;
	}
END_DEFINE_SPEC(FHealingStoneSpec)

void FHealingStoneSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		Executor = TestWorld->GetSubsystem<UActionExecutor>();
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Char = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("Effects"), Effects) || !TestNotNull(TEXT("Executor"), Executor) || !TestNotNull(TEXT("Char"), Char))
		{
			Effects = nullptr; // each case returns early
			return;
		}
		Char->ServerTakeDamage(HEALING_STONE_TEST_DAMAGE);
	});

	AfterEach([this]() { TestWorld.Reset(); });

	It("has the old heal numbers as percent more healing, on the shared duration curve", [this]()
	{
		for (const FHealingStoneTierRow &Row : HEALING_STONE_TEST_TIERS)
		{
			const FCrystalId Stone(ECrystalType::HealingStone, Row.Tier);
			TestEqual(TEXT("boost percent"), CrystalEffectTable::GetHealingBoostPercent(Stone), Row.BoostPercent);
			TestEqual(TEXT("turns"), CrystalEffectTable::GetHealingBoostDuration(Stone), Row.Turns);
		}
		TestEqual(TEXT("no other kind has a boost"), CrystalEffectTable::GetHealingBoostPercent(FCrystalId(ECrystalType::Sapphire, EItemTier::F_Tier)), 0.0f);
		TestTrue(TEXT("the stone is a boost, not an item that gives HP"),
				 ItemIdentity::GetItemEffectType(HEALING_STONE_TEST_F) == EItemEffectType::HealingBoost);
	});

	It("gives no HP on use, and the target receives the tier's percent more healing", [this]()
	{
		if (!Effects) return;
		const int32 HPBefore = Char->CurrentHP;
		const FActionResult Used = Executor->ExecuteItem(Hero, HEALING_STONE_TEST_F, {Hero});
		TestTrue(TEXT("the use lands"), Used.bSuccess);
		TestEqual(TEXT("no HP on use"), Char->CurrentHP, HPBefore);
		TestEqual(TEXT("and none reported"), Used.TotalHealingDone, 0);
		TestEqual(TEXT("the Healing Boost is on at F's percent"), GetBoost(Hero), CrystalEffectTable::GetHealingBoostPercent(HEALING_STONE_TEST_F));
	});

	It("raises a later heal and a HOT tick by that percent", [this]()
	{
		if (!Effects) return;
		TestTrue(TEXT("the use lands"), Executor->ExecuteItem(Hero, HEALING_STONE_TEST_F, {Hero}).bSuccess);
		const float BoostPercent = CrystalEffectTable::GetHealingBoostPercent(HEALING_STONE_TEST_F);
		FTickMathInputs Tick;
		Tick.Value = HEALING_STONE_TEST_HEAL;
		Tick.HealingReceivedPercent = GetBoost(Hero);
		const int32 Expected = WoRTickMath::AmplifyHeal(HEALING_STONE_TEST_HEAL, BoostPercent);
		TestTrue(TEXT("the boost raises the number"), Expected > HEALING_STONE_TEST_HEAL);
		TestEqual(TEXT("a HOT tick is raised by it"), WoRTickMath::GetHotTickHeal(Tick), Expected);
		const int32 HPBefore = Char->CurrentHP;
		TestEqual(TEXT("a later heal is raised by it"), Effects->HealActor(Hero, HEALING_STONE_TEST_HEAL, Hero), Expected);
		TestEqual(TEXT("HP went up by the raised heal"), Char->CurrentHP, HPBefore + Expected);
	});

	It("ends on time: it lasts the tier's turns of the holder, then is gone", [this]()
	{
		if (!Effects) return;
		TestTrue(TEXT("the use lands"), Executor->ExecuteItem(Hero, HEALING_STONE_TEST_F, {Hero}).bSuccess);
		const int32 Turns = CrystalEffectTable::GetHealingBoostDuration(HEALING_STONE_TEST_F);
		for (int32 Turn = 0; Turn < Turns; ++Turn)
		{
			Effects->ProcessStartOfTurnEffects(Hero);
			TestTrue(TEXT("on during each of its turns"), GetBoost(Hero) > 0.0f);
			Effects->ProcessEndOfTurnEffects(Hero);
		}
		Effects->ProcessStartOfTurnEffects(Hero);
		TestEqual(TEXT("gone when the next turn starts"), GetBoost(Hero), 0.0f);
		TestEqual(TEXT("a heal is plain again"), Effects->HealActor(Hero, HEALING_STONE_TEST_HEAL, Hero), HEALING_STONE_TEST_HEAL);
	});

	It("is refused on a dead target, with no boost given (D-253)", [this]()
	{
		if (!Effects) return;
		AActor *Fallen = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		UCharacterDataComponent *FallenChar = Fallen ? Fallen->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("Fallen"), FallenChar)) return;
		FallenChar->ServerTakeDamage(FallenChar->MaxHP);
		if (!TestFalse(TEXT("the target is dead"), FallenChar->bIsAlive)) return;

		FAction Item;
		Item.ActionType = EActionType::Item;
		Item.ItemData = HEALING_STONE_TEST_F;
		Item.Targets.Add(Fallen);
		TestFalse(TEXT("the action is not valid"), Executor->ValidateAction(Hero, Item).bIsValid);
		TestFalse(TEXT("the use is refused"), Executor->ExecuteItem(Hero, HEALING_STONE_TEST_F, {Fallen}).bSuccess);
		TestEqual(TEXT("no boost on the dead"), GetBoost(Fallen), 0.0f);
		TestFalse(TEXT("and it does not revive"), FallenChar->bIsAlive);
	});

	It("spends nothing when the target is dead, and one when it is alive", [this]()
	{
		if (!Effects) return;
		UPoolSubsystem *Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		UCharacterData *Data = nullptr;
		AActor *Member = SpawnPouchMember(Data);
		ULoadoutComponent *Loadout = Member ? Member->FindComponentByClass<ULoadoutComponent>() : nullptr;
		UInventoryComponent *Inventory = Member ? Member->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!TestNotNull(TEXT("Pool"), Pool) || !TestNotNull(TEXT("Loadout"), Loadout) || !TestNotNull(TEXT("Inventory"), Inventory)) return;
		const FName Key = UPoolSubsystem::GetCharacterKey(Data);
		Pool->CarryOrStore(Key, HEALING_STONE_TEST_F, HEALING_STONE_TEST_CARRIED);
		Inventory->ActiveLoadoutIndex = 0;
		if (!TestTrue(TEXT("Ready"), Loadout->PrepareForBattle(Inventory))) return;

		UCharacterDataComponent *HeroChar = Char;
		HeroChar->ServerTakeDamage(HeroChar->MaxHP);
		if (!TestFalse(TEXT("the target is dead"), HeroChar->bIsAlive)) return;
		TestFalse(TEXT("refused on the dead"), Executor->ExecuteItem(Member, HEALING_STONE_TEST_F, {Hero}).bSuccess);
		TestEqual(TEXT("nothing spent"), Pool->GetCarriedCount(Key, HEALING_STONE_TEST_F), HEALING_STONE_TEST_CARRIED);
		TestEqual(TEXT("none used up this fight"), Loadout->GetFightItemsLeft(HEALING_STONE_TEST_F), HEALING_STONE_TEST_CARRIED);

		TestTrue(TEXT("lands on the living"), Executor->ExecuteItem(Member, HEALING_STONE_TEST_F, {Member}).bSuccess);
		TestEqual(TEXT("one spent"), Pool->GetCarriedCount(Key, HEALING_STONE_TEST_F), HEALING_STONE_TEST_CARRIED - 1);
	});

	It("is not the AI's low-HP heal: the AI does not spend it", [this]()
	{
		if (!Effects) return;
		UAIDecisionManager *AI = TestWorld->GetSubsystem<UAIDecisionManager>();
		UPoolSubsystem *Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		UCharacterData *Data = nullptr;
		AActor *Member = SpawnPouchMember(Data);
		ULoadoutComponent *Loadout = Member ? Member->FindComponentByClass<ULoadoutComponent>() : nullptr;
		UInventoryComponent *Inventory = Member ? Member->FindComponentByClass<UInventoryComponent>() : nullptr;
		UCharacterDataComponent *MemberChar = Member ? Member->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("AI"), AI) || !TestNotNull(TEXT("Pool"), Pool) || !TestNotNull(TEXT("Loadout"), Loadout) || !TestNotNull(TEXT("MemberChar"), MemberChar)) return;
		const FName Key = UPoolSubsystem::GetCharacterKey(Data);
		Pool->CarryOrStore(Key, HEALING_STONE_TEST_F, HEALING_STONE_TEST_CARRIED);
		Inventory->ActiveLoadoutIndex = 0;
		if (!TestTrue(TEXT("Ready"), Loadout->PrepareForBattle(Inventory))) return;
		TestTrue(TEXT("the stone is in the fight list"), Loadout->GetUsableItems().ContainsByPredicate([](const FItemLoadoutSlot &Slot) { return Slot.CrystalId == HEALING_STONE_TEST_F; }));

		// The survival branch's only item heal is what this search returns (UAIDecisionManager::TrySurvivalBranch).
		MemberChar->ServerTakeDamage(MemberChar->CurrentHP - HEALING_STONE_TEST_LOW_HP);
		bool bFound = true;
		AI->FindHealingItem(Loadout, bFound);
		TestFalse(TEXT("at low HP the heal search does not find it"), bFound);
		TestEqual(TEXT("none spent"), Pool->GetCarriedCount(Key, HEALING_STONE_TEST_F), HEALING_STONE_TEST_CARRIED);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
