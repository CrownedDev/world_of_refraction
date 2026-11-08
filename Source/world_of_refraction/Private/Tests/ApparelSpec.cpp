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
#include "Loadout/FSavedLoadout.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Character/CombatCharacter.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"

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

	It("keys to a refined crystal or an evolution, not an augment stone", [this]()
	{
		FRuntimeAttachedItem Evolution;
		Evolution.Kind = EAttachedItemKind::Evolution;
		FRuntimeAttachedItem Stone;
		Stone.Kind = EAttachedItemKind::AugmentStone;
		FRuntimeAttachedItem Crystal;
		Crystal.Kind = EAttachedItemKind::Crystal;
		TestTrue(TEXT("a crystal"), ApparelRules::CanSocket(Crystal));
		TestTrue(TEXT("an evolution (Crown, D-164)"), ApparelRules::CanSocket(Evolution));
		TestFalse(TEXT("no augment stone"), ApparelRules::CanSocket(Stone));
		TestEqual(TEXT("no crystal: no element"), FApparelEntry::CreateFromApparel(ApparelTestPiece()).GetElement(), ESpellElement::None);
		FCombatLoadout Loadout;
		FApparelEntry WithStone = FApparelEntry::CreateFromApparel(ApparelTestPiece());
		WithStone.AttachedItem = Stone;
		TestFalse(TEXT("a piece holding an augment stone is not worn"), ApparelRules::AddApparel(Loadout, WithStone));
		FApparelEntry WithEvolution = FApparelEntry::CreateFromApparel(ApparelTestPiece());
		WithEvolution.AttachedItem = Evolution;
		TestTrue(TEXT("a piece holding an evolution is worn"), ApparelRules::AddApparel(Loadout, WithEvolution));
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

	It("is owned in the store with its socket and comes back from the save (B-380b)", [this]()
	{
		UPoolSubsystem *Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		if (!TestNotNull(TEXT("Pool"), Pool))
		{
			return;
		}
		FApparelEntry Piece = FApparelEntry::CreateFromApparel(ApparelTestPiece());
		Piece.AttachedItem.Kind = EAttachedItemKind::Crystal;
		Piece.AttachedItem.Crystal = FCrystalAttachment(APPAREL_TEST_FIRE_GEM, APPAREL_TEST_DURABILITY);
		Piece.StatBonus.BonusDefense = 1;
		const FGuid Id = Pool->AddApparelToPool(Piece);
		TestTrue(TEXT("owned: an id minted"), Id.IsValid());
		TestNotNull(TEXT("found by its id"), Pool->FindApparelInPool(Id));

		const int32 OwnedBefore = Pool->GetOwnedApparel().Num();
		const FSavedPool Saved = USaveSubsystem::CapturePool(*Pool);
		USaveSubsystem::RestorePool(Saved, *Pool); // restored beside the original
		TestEqual(TEXT("restored as apparel"), Pool->GetOwnedApparel().Num(), OwnedBefore * 2);
		const FApparelEntry &Back = Pool->GetOwnedApparel().Last();
		TestTrue(TEXT("same id"), Back.PersistentID == Id);
		TestEqual(TEXT("its crystal: Fire"), Back.GetElement(), ESpellElement::Fire);
		TestEqual(TEXT("its roll"), Back.StatBonus.BonusDefense, 1);
	});

	It("keeps worn apparel through a saved loadout and follows a crystal set in play (B-380c)", [this]()
	{
		UPoolSubsystem *Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		// Review: a party member drawn from the store (DrawSharedGear binds the store's change event), as CustomLoadoutsSpec.
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Pool"), Pool) || !TestNotNull(TEXT("Party"), Party))
		{
			return;
		}
		Party->Members.Empty();
		UCharacterData *Member = FWoRTestWorld::NewCharacterData();
		FPartyMember &PartyMember = Party->Members.AddDefaulted_GetRef();
		PartyMember.PawnClass = ACombatCharacter::StaticClass();
		PartyMember.CharacterData = Member;
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Member);
		if (!TestNotNull(TEXT("Loadout"), Loadout))
		{
			return;
		}
		Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>()->DrawSharedGear();
		FApparelEntry Piece = FApparelEntry::CreateFromApparel(ApparelTestPiece());
		Piece.PersistentID = Pool->AddApparelToPool(Piece);
		FCombatLoadout &Active = ActiveOf(Loadout);
		TestTrue(TEXT("worn"), ApparelRules::AddApparel(Active, Piece));

		// A crystal set in play reaches the worn copy, and its Fire resistance counts.
		FRuntimeAttachedItem Gem;
		Gem.Kind = EAttachedItemKind::Crystal;
		Gem.Crystal = FCrystalAttachment(APPAREL_TEST_FIRE_GEM, APPAREL_TEST_DURABILITY);
		TestTrue(TEXT("attached through the store"), Pool->AttachToApparel(Piece.PersistentID, Gem));
		TestEqual(TEXT("the worn copy follows"), ActiveOf(Loadout).Apparel[0].GetElement(), ESpellElement::Fire);
		TestFalse(TEXT("an unowned piece cannot be socketed"), Pool->AttachToApparel(FGuid::NewGuid(), Gem));

		// Through a saved loadout and back: the same owned piece, with its crystal.
		const UInventoryComponent *Inventory = Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>();
		const FSavedLoadout Saved = ActiveOf(Loadout).ToSavedLoadout(ECharacterClass::Generic);
		TestEqual(TEXT("saved with its id"), Saved.ApparelInstances.Num(), 1);
		const FCombatLoadout Back = FCombatLoadout::CreateFromSavedLoadout(Saved, Inventory, nullptr);
		TestEqual(TEXT("one piece back"), Back.Apparel.Num(), 1);
		TestTrue(TEXT("the owned piece"), Back.Apparel.Num() == 1 && Back.Apparel[0].PersistentID == Piece.PersistentID);
		TestTrue(TEXT("with its crystal"), Back.Apparel.Num() == 1 && Back.Apparel[0].GetElement() == ESpellElement::Fire);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
