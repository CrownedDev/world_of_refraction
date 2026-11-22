// PotionsSpec.cpp
// B-579 (D-253): the Health Potion gives 25% of max HP and the Energy Potion 25% of max EP, at one strength (no tier,
// one stack), with no status build-up. The Healing Boost raises the Health Potion and not the Energy Potion. Both are
// weaker than every Sapphire and Citrine tier, are fight items for the living (a dead target is refused unspent), cost
// less than a tier F Sapphire or Citrine, are neither a gem nor a stone (no socket), and the AI's low-HP heal search
// finds the Health Potion.
// Unproven here: the fight menu, the tile and its text on screen, a merchant's shelf (no merchant asset stocks potions
// yet), and the sell / dismantle refusal at a vendor (docs/PIE/B-579.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AI/AIDecisionManager.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyService.h"
#include "Currency/EconomyYield.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/ItemConstants.h"
#include "Loadout/Entries/FApparelEntry.h"
#include "Loadout/LoadoutComponent.h"
#include "Merchant/MerchantData.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Skills/Effects/TickMath.h"

namespace
{
	constexpr int32 POTION_TEST_CARRIED = 2;
	const FCrystalId POTION_TEST_HEALTH(ECrystalType::HealthPotion, EItemTier::F_Tier);
	const FCrystalId POTION_TEST_ENERGY(ECrystalType::EnergyPotion, EItemTier::F_Tier);
	const FCrystalId POTION_TEST_HEALING_STONE(ECrystalType::HealingStone, EItemTier::F_Tier);
	const EItemTier POTION_TEST_TIERS[] = {EItemTier::F_Tier, EItemTier::E_Tier, EItemTier::D_Tier, EItemTier::C_Tier,
										   EItemTier::B_Tier, EItemTier::A_Tier, EItemTier::S_Tier};
}

BEGIN_DEFINE_SPEC(FPotionsSpec, "WoR.Items.Potions", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *Effects = nullptr;
	UActionExecutor *Executor = nullptr;
	AActor *Hero = nullptr;
	UCharacterDataComponent *Char = nullptr;

	int32 ShareOf(int32 Max, float Percent) const { return FMath::Max(1, FMath::RoundToInt(Max * Percent / 100.0f)); }

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

	static FMerchantStockEntry StockOf(const FCrystalId &Item)
	{
		FMerchantStockEntry Entry;
		Entry.Crystal = Item;
		Entry.Count = 1;
		return Entry;
	}
END_DEFINE_SPEC(FPotionsSpec)

void FPotionsSpec::Define()
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
		}
	});

	AfterEach([this]() { TestWorld.Reset(); });

	It("gives 25% of max HP, the same whatever tier the potion was made with", [this]()
	{
		if (!Effects) return;
		const int32 Share = ShareOf(Char->MaxHP, ItemConstants::HEALTH_POTION_HP_PERCENT);
		TestEqual(TEXT("the number is 25"), ItemConstants::HEALTH_POTION_HP_PERCENT, 25.0f);
		for (const EItemTier Tier : {EItemTier::F_Tier, EItemTier::S_Tier})
		{
			Char->ServerTakeDamage(Char->CurrentHP - 1);
			const int32 HPBefore = Char->CurrentHP;
			const FActionResult Used = Executor->ExecuteItem(Hero, FCrystalId(ECrystalType::HealthPotion, Tier), {Hero});
			TestTrue(TEXT("the use lands"), Used.bSuccess);
			TestEqual(TEXT("HP goes up by 25% of max HP"), Char->CurrentHP - HPBefore, Share);
			TestEqual(TEXT("and the result says so"), Used.TotalHealingDone, Share);
		}
	});

	It("is raised by the Healing Boost: a Healing Stone first makes the Health Potion heal more", [this]()
	{
		if (!Effects) return;
		Char->ServerTakeDamage(Char->CurrentHP - 1);
		TestTrue(TEXT("the stone lands"), Executor->ExecuteItem(Hero, POTION_TEST_HEALING_STONE, {Hero}).bSuccess);
		const int32 Share = ShareOf(Char->MaxHP, ItemConstants::HEALTH_POTION_HP_PERCENT);
		const int32 Boosted = WoRTickMath::AmplifyHeal(Share, CrystalEffectTable::GetHealingBoostPercent(POTION_TEST_HEALING_STONE));
		const int32 HPBefore = Char->CurrentHP;
		TestTrue(TEXT("the potion lands"), Executor->ExecuteItem(Hero, POTION_TEST_HEALTH, {Hero}).bSuccess);
		TestTrue(TEXT("the boost raises the number"), Boosted > Share);
		TestEqual(TEXT("HP goes up by the raised amount"), Char->CurrentHP - HPBefore, Boosted);
	});

	It("gives 25% of max EP with no status build-up, and the Healing Boost does not raise it", [this]()
	{
		if (!Effects) return;
		UStatusBuildupManager *Bars = TestWorld->GetSubsystem<UStatusBuildupManager>();
		if (!TestNotNull(TEXT("Bars"), Bars) || !TestTrue(TEXT("the character has energy to restore"), Char->MaxEP > 0)) return;
		const int32 Share = ShareOf(Char->MaxEP, ItemConstants::ENERGY_POTION_EP_PERCENT);
		TestEqual(TEXT("the number is 25"), CrystalEffectTable::GetEPRestorePercent(POTION_TEST_ENERGY), 25.0f);

		Char->ServerSpendEnergy(Char->CurrentEP);
		TestTrue(TEXT("the potion lands"), Executor->ExecuteItem(Hero, POTION_TEST_ENERGY, {Hero}).bSuccess);
		TestEqual(TEXT("EP goes up by 25% of max EP"), Char->CurrentEP, Share);
		TestEqual(TEXT("no status build-up"), Bars->GetStatusBarBuildup(Hero), 0.0f);

		TestTrue(TEXT("the stone lands"), Executor->ExecuteItem(Hero, POTION_TEST_HEALING_STONE, {Hero}).bSuccess);
		Char->ServerSpendEnergy(Char->CurrentEP);
		TestTrue(TEXT("the potion lands again"), Executor->ExecuteItem(Hero, POTION_TEST_ENERGY, {Hero}).bSuccess);
		TestEqual(TEXT("the Healing Boost does not raise energy"), Char->CurrentEP, Share);
	});

	It("is weaker than every Sapphire and every Citrine tier", [this]()
	{
		TestTrue(TEXT("under Sapphire's revive (30%)"), ItemConstants::HEALTH_POTION_HP_PERCENT < ItemConstants::REVIVE_HP_PERCENT * 100.0f);
		TestTrue(TEXT("under Sapphire's death-save (50%)"), ItemConstants::HEALTH_POTION_HP_PERCENT < ItemConstants::LAST_STAND_HP_PERCENT);
		for (const EItemTier Tier : POTION_TEST_TIERS)
		{
			TestTrue(TEXT("under Citrine at this tier"), CrystalEffectTable::GetEPRestorePercent(POTION_TEST_ENERGY) <
															 CrystalEffectTable::GetEPRestorePercent(FCrystalId(ECrystalType::Citrine, Tier)));
			TestEqual(TEXT("the same Energy Potion at any tier"), CrystalEffectTable::GetEPRestorePercent(FCrystalId(ECrystalType::EnergyPotion, Tier)),
					  ItemConstants::ENERGY_POTION_EP_PERCENT);
		}
	});

	It("is neither a gem nor a stone: no tier in its name, no element, no build-up, no socket", [this]()
	{
		for (const FCrystalId &Potion : {POTION_TEST_HEALTH, POTION_TEST_ENERGY})
		{
			TestTrue(TEXT("a potion"), CrystalTypeHelpers::IsPotionType(Potion.Type));
			TestFalse(TEXT("not a gem"), CrystalTypeHelpers::IsGemType(Potion.Type));
			TestFalse(TEXT("not a stone"), CrystalTypeHelpers::IsAugmentStoneType(Potion.Type));
			TestTrue(TEXT("no element"), ItemIdentity::GetElement(Potion) == ESpellElement::None);
			TestEqual(TEXT("no status build-up"), CrystalEffectTable::GetElementalBuildupPercent(Potion), 0.0f);
			TestEqual(TEXT("no Broken Darkness energy, which only a gem gives"), CrystalEffectTable::GetBrokenDarknessEnergyPercent(Potion), 0.0f);
			TestFalse(TEXT("its name shows no tier"), ItemIdentity::GetDisplayName(Potion).Contains(TEXT("(")));
			FRuntimeAttachedItem Socketed;
			Socketed.Kind = EAttachedItemKind::Crystal;
			Socketed.Crystal.Id = Potion;
			TestFalse(TEXT("an apparel socket refuses it"), ApparelRules::CanSocket(Socketed));
			TestFalse(TEXT("it can't be merged up"), UEconomyService::GetMergeRefusal(Hero, Potion.Type, EItemTier::E_Tier).IsEmpty());
		}
		TestTrue(TEXT("the Health Potion gives HP"), ItemIdentity::GetItemEffectType(POTION_TEST_HEALTH) == EItemEffectType::RestoreHealth);
		TestTrue(TEXT("the Energy Potion gives EP"), ItemIdentity::GetItemEffectType(POTION_TEST_ENERGY) == EItemEffectType::EnergyRestore);
		TestEqual(TEXT("the names"), ItemIdentity::GetDisplayName(POTION_TEST_HEALTH), FString(TEXT("Health Potion")));
		TestEqual(TEXT("the names"), ItemIdentity::GetDisplayName(POTION_TEST_ENERGY), FString(TEXT("Energy Potion")));
	});

	It("costs less than a tier F Sapphire or Citrine: Prisms only, no essence", [this]()
	{
		const UEconomyService *Economy = TestWorld->GetSubsystem<UEconomyService>();
		if (!TestNotNull(TEXT("Economy"), Economy)) return;
		for (const FCrystalId &Potion : {POTION_TEST_HEALTH, POTION_TEST_ENERGY})
		{
			const FPurchaseCost Cost = Economy->PreviewCartCost({StockOf(Potion)});
			TestEqual(TEXT("the potion price in Prisms"), Cost.Prisms, EconomyYield::GetPotionPrisms());
			TestTrue(TEXT("a price above nothing"), Cost.Prisms > 0);
			TestEqual(TEXT("no essence of any kind"), Cost.ElementalEssence + Cost.PillarEssence + Cost.SkillEssence, 0);
			for (const ECrystalType Gem : {ECrystalType::Sapphire, ECrystalType::Citrine})
			{
				const FPurchaseCost GemCost = Economy->PreviewCartCost({StockOf(FCrystalId(Gem, EItemTier::F_Tier))});
				TestTrue(TEXT("fewer Prisms than the tier F gem"), Cost.Prisms < GemCost.Prisms);
				TestTrue(TEXT("and the gem costs essence too"), GemCost.ElementalEssence > 0);
			}
		}
	});

	It("can't be dismantled, sold or socketed, where a gem beside it can be dismantled and socketed (Q-579)", [this]()
	{
		UEconomyService *Economy = TestWorld->GetSubsystem<UEconomyService>();
		// A well-formed owner, as EconomyPreambleSpec and GearAttachSpec build one.
		AActor *Owner = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		UCrystalInventoryComponent *Crystals = FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Owner);
		UInventoryComponent *Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Owner);
		if (!TestNotNull(TEXT("Economy"), Economy)) return;
		const FCrystalId Gem(ECrystalType::Garnet, EItemTier::F_Tier);
		TestTrue(TEXT("gems added"), Inventory->AddCrystal(Gem, POTION_TEST_CARRIED));
		TestTrue(TEXT("potions added"), Inventory->AddCrystal(POTION_TEST_HEALTH, POTION_TEST_CARRIED));
		TestTrue(TEXT("weapon added"), Inventory->AddWeapon(NewObject<UWeaponData>(GetTransientPackage())));
		if (!TestTrue(TEXT("a weapon to socket"), Inventory->Weapons.Num() > 0)) return;
		const FGuid WeaponId = Inventory->Weapons[0].PersistentID;

		TestFalse(TEXT("a potion is not dismantled"), Economy->DismantleCrystal(Owner, POTION_TEST_HEALTH, 1));
		TestFalse(TEXT("a potion is not sold"), Economy->SellCrystal(Owner, POTION_TEST_HEALTH, 1));
		TestFalse(TEXT("a potion is not socketed"), Inventory->AttachCrystalToWeapon(WeaponId, POTION_TEST_HEALTH));
		TestEqual(TEXT("every potion is still carried"), Crystals->GetCount(POTION_TEST_HEALTH), POTION_TEST_CARRIED);
		TestTrue(TEXT("the weapon's socket is still empty"), Inventory->Weapons[0].AttachedItem.IsEmpty());

		// The same owner can do both with a gem, so the refusals above are the potion's.
		TestTrue(TEXT("a gem is dismantled"), Economy->DismantleCrystal(Owner, Gem, 1));
		TestTrue(TEXT("a gem is socketed"), Inventory->AttachCrystalToWeapon(WeaponId, Gem));
	});

	It("is one stack and a fight item; a dead target is refused and nothing is spent", [this]()
	{
		if (!Effects) return;
		UPoolSubsystem *Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		UCharacterData *Data = nullptr;
		AActor *Member = SpawnPouchMember(Data);
		ULoadoutComponent *Loadout = Member ? Member->FindComponentByClass<ULoadoutComponent>() : nullptr;
		UInventoryComponent *Inventory = Member ? Member->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!TestNotNull(TEXT("Pool"), Pool) || !TestNotNull(TEXT("Loadout"), Loadout) || !TestNotNull(TEXT("Inventory"), Inventory)) return;
		const FName Key = UPoolSubsystem::GetCharacterKey(Data);
		Pool->CarryOrStore(Key, POTION_TEST_HEALTH, 1);
		Pool->CarryOrStore(Key, FCrystalId(ECrystalType::HealthPotion, EItemTier::S_Tier), POTION_TEST_CARRIED - 1); // made at another tier
		TestEqual(TEXT("one stack, whatever tier it was made with"), Pool->GetCarriedCount(Key, POTION_TEST_HEALTH), POTION_TEST_CARRIED);
		TestTrue(TEXT("picked as a fight item"), Pool->IsFightItem(Key, POTION_TEST_HEALTH));
		Inventory->ActiveLoadoutIndex = 0;
		if (!TestTrue(TEXT("Ready"), Loadout->PrepareForBattle(Inventory))) return;
		TestTrue(TEXT("in the fight list"), Loadout->GetUsableItems().ContainsByPredicate([](const FItemLoadoutSlot &Slot) { return Slot.CrystalId == POTION_TEST_HEALTH; }));

		Char->ServerTakeDamage(Char->MaxHP);
		if (!TestFalse(TEXT("the target is dead"), Char->bIsAlive)) return;
		const FActionResult Refused = Executor->ExecuteItem(Member, POTION_TEST_HEALTH, {Hero});
		TestFalse(TEXT("refused on the dead"), Refused.bSuccess);
		TestEqual(TEXT("because the target is dead"), Refused.ErrorMessage, FString(TEXT("Target is dead")));
		TestEqual(TEXT("nothing spent"), Pool->GetCarriedCount(Key, POTION_TEST_HEALTH), POTION_TEST_CARRIED);
		TestFalse(TEXT("and it does not revive"), Char->bIsAlive);

		TestTrue(TEXT("lands on the living"), Executor->ExecuteItem(Member, POTION_TEST_HEALTH, {Member}).bSuccess);
		TestEqual(TEXT("one spent"), Pool->GetCarriedCount(Key, POTION_TEST_HEALTH), POTION_TEST_CARRIED - 1);
	});

	It("is the AI's low-HP heal: the heal search finds the Health Potion, not the Healing Stone or the Energy Potion", [this]()
	{
		if (!Effects) return;
		UAIDecisionManager *AI = TestWorld->GetSubsystem<UAIDecisionManager>();
		UPoolSubsystem *Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		UCharacterData *Data = nullptr;
		AActor *Member = SpawnPouchMember(Data);
		ULoadoutComponent *Loadout = Member ? Member->FindComponentByClass<ULoadoutComponent>() : nullptr;
		UInventoryComponent *Inventory = Member ? Member->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!TestNotNull(TEXT("AI"), AI) || !TestNotNull(TEXT("Pool"), Pool) || !TestNotNull(TEXT("Loadout"), Loadout) || !TestNotNull(TEXT("Inventory"), Inventory)) return;
		const FName Key = UPoolSubsystem::GetCharacterKey(Data);
		Pool->CarryOrStore(Key, POTION_TEST_HEALING_STONE, 1);
		Pool->CarryOrStore(Key, POTION_TEST_ENERGY, 1);
		Inventory->ActiveLoadoutIndex = 0;
		if (!TestTrue(TEXT("Ready"), Loadout->PrepareForBattle(Inventory))) return;
		bool bFound = true;
		AI->FindHealingItem(Loadout, bFound);
		TestFalse(TEXT("with no Health Potion carried, no heal is found"), bFound);

		Pool->CarryOrStore(Key, POTION_TEST_HEALTH, 1);
		bFound = false;
		const FCrystalId Heal = AI->FindHealingItem(Loadout, bFound);
		TestTrue(TEXT("with one carried, a heal is found"), bFound);
		TestTrue(TEXT("and it is the Health Potion"), Heal == POTION_TEST_HEALTH);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
