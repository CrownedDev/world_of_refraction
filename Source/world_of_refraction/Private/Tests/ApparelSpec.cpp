// ApparelSpec.cpp
// B-380 (D-164, Q-380): an apparel piece adds its HP and Defence lines (nothing else) to the wearer's gear stats, takes
// no loadout budget, holds at most two pieces, and its socket takes a refined crystal only.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/FCrystalAttachment.h"
#include "Skills/Effects/EffectDefinition.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/Entries/FApparelEntry.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/LoadoutRules.h"

namespace
{
	constexpr int32 APPAREL_TEST_HP = 5;
	constexpr int32 APPAREL_TEST_DEFENSE = 3;
	constexpr int32 APPAREL_TEST_LUCK = 4;
	const FCrystalId APPAREL_TEST_FIRE_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	constexpr int32 APPAREL_TEST_DURABILITY = 5;

	UApparelData *ApparelTestPiece()
	{
		UApparelData *Piece = NewObject<UApparelData>(GetTransientPackage());
		Piece->Tier = EItemTier::F_Tier;
		Piece->BaseStatBonus.BonusMaxHP = APPAREL_TEST_HP;
		Piece->BaseStatBonus.BonusDefense = APPAREL_TEST_DEFENSE;
		Piece->BaseStatBonus.BonusLuck = APPAREL_TEST_LUCK; // not an apparel line: ignored
		return Piece;
	}
}

BEGIN_DEFINE_SPEC(FApparelSpec, "WoR.Loadout.Apparel", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	static FCombatLoadout &ActiveOf(ULoadoutComponent *Loadout)
	{
		UInventoryComponent *Inventory = Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>();
		return Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	}
END_DEFINE_SPEC(FApparelSpec)

void FApparelSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]() { TestWorld.Reset(); });

	It("adds only its HP and Defence lines to the wearer's gear stats", [this]()
	{
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("Loadout"), Loadout))
		{
			return;
		}
		const FEquipmentStatBonus Before = Loadout->GetActiveStatBonus(Loadout->GetOwner());
		TestTrue(TEXT("worn"), ApparelRules::AddApparel(ActiveOf(Loadout), FApparelEntry::CreateFromApparel(ApparelTestPiece())));
		const FEquipmentStatBonus After = Loadout->GetActiveStatBonus(Loadout->GetOwner());
		TestEqual(TEXT("HP"), After.BonusMaxHP - Before.BonusMaxHP, APPAREL_TEST_HP);
		TestEqual(TEXT("Defence (F tier: x1)"), After.BonusDefense - Before.BonusDefense, APPAREL_TEST_DEFENSE);
		TestEqual(TEXT("Luck is not an apparel line"), After.BonusLuck, Before.BonusLuck);
		TestTrue(TEXT("the asset flags the unused line"), ApparelTestPiece()->HasUnusedStatLines());
	});

	It("takes no budget, holds at most two pieces, and not the same piece twice", [this]()
	{
		FCombatLoadout Loadout;
		FLoadoutRuleContext Context;
		const int32 FindingsBefore = FLoadoutRules::CollectFindings(Loadout, Context).Num();
		FApparelEntry Owned = FApparelEntry::CreateFromApparel(ApparelTestPiece());
		Owned.PersistentID = FGuid::NewGuid();
		TestTrue(TEXT("first"), ApparelRules::AddApparel(Loadout, Owned));
		TestFalse(TEXT("the same owned piece again"), ApparelRules::AddApparel(Loadout, Owned));
		TestTrue(TEXT("second"), ApparelRules::AddApparel(Loadout, FApparelEntry::CreateFromApparel(ApparelTestPiece())));
		TestFalse(TEXT("a third (Q-380: two slots)"), ApparelRules::AddApparel(Loadout, FApparelEntry::CreateFromApparel(ApparelTestPiece())));
		TestFalse(TEXT("no asset"), ApparelRules::AddApparel(Loadout, FApparelEntry()));
		TestEqual(TEXT("no new budget findings"), FLoadoutRules::CollectFindings(Loadout, Context).Num(), FindingsBefore);
		TestTrue(TEXT("the print names it"), ApparelRules::GetApparelString(Loadout).Contains(TEXT("2 of 2")));
	});

	It("keys to a refined crystal only, which gives its element", [this]()
	{
		FRuntimeAttachedItem Evolution;
		Evolution.Kind = EAttachedItemKind::Evolution;
		FRuntimeAttachedItem Stone;
		Stone.Kind = EAttachedItemKind::AugmentStone;
		FRuntimeAttachedItem Crystal;
		Crystal.Kind = EAttachedItemKind::Crystal;
		TestTrue(TEXT("a crystal"), ApparelRules::CanSocket(Crystal));
		TestFalse(TEXT("no evolution (Q-380)"), ApparelRules::CanSocket(Evolution));
		TestFalse(TEXT("no augment stone"), ApparelRules::CanSocket(Stone));
		TestEqual(TEXT("no crystal: no element"), FApparelEntry::CreateFromApparel(ApparelTestPiece()).GetElement(), ESpellElement::None);
		FCombatLoadout Loadout;
		FApparelEntry WithEvolution = FApparelEntry::CreateFromApparel(ApparelTestPiece());
		WithEvolution.AttachedItem = Evolution;
		TestFalse(TEXT("a piece holding an evolution is not worn"), ApparelRules::AddApparel(Loadout, WithEvolution));
	});

	It("adds resistance to its crystal's element, and its effects count (D-164, Crown)", [this]()
	{
		FApparelEntry Piece = FApparelEntry::CreateFromApparel(ApparelTestPiece());
		Piece.AttachedItem.Kind = EAttachedItemKind::Crystal;
		Piece.AttachedItem.Crystal = FCrystalAttachment(APPAREL_TEST_FIRE_GEM, APPAREL_TEST_DURABILITY);
		TestEqual(TEXT("keyed to Fire"), Piece.GetElement(), ESpellElement::Fire);
		TestEqual(TEXT("Fire resistance"), Piece.GetResistance().Fire, ApparelRules::CRYSTAL_ELEMENT_RESISTANCE);
		TestEqual(TEXT("not Water"), Piece.GetResistance().Water, 0.0f);

		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("Loadout"), Loadout))
		{
			return;
		}
		UEffectDefinition *Bundle = NewObject<UEffectDefinition>(GetTransientPackage());
		FSkillEffect Effect;
		FSkillEffectPayload Payload;
		Payload.EffectType = ESkillEffectType::DefenseBuff;
		Effect.Payloads.Add(Payload);
		Bundle->Effects.Add(Effect);
		Piece.Apparel->ReferencedEffects.Add(Bundle);
		const int32 EffectsBefore = Loadout->GetActiveEffects(Loadout->GetOwner()).Num();
		const float FireBefore = Loadout->GetActiveResistanceBonus(Loadout->GetOwner()).Fire;
		TestTrue(TEXT("worn"), ApparelRules::AddApparel(ActiveOf(Loadout), Piece));
		TestEqual(TEXT("its effect counts"), Loadout->GetActiveEffects(Loadout->GetOwner()).Num(), EffectsBefore + 1);
		TestEqual(TEXT("its Fire resistance counts"), Loadout->GetActiveResistanceBonus(Loadout->GetOwner()).Fire - FireBefore,
				  ApparelRules::CRYSTAL_ELEMENT_RESISTANCE);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
