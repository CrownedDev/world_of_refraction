// BrokenSourceSpec.cpp
// D-083 (B-163): a spell whose source's crystal breaks mid-fight stays in the menu, greyed out with
// the reason, and can't be cast; a broken ring keeps its F-tier spells.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AI/AIDecisionManager.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/SpellData.h"
#include "UI/Combat/FCombatCapabilities.h"

namespace
{
	constexpr int32 BROKEN_SOURCE_INTACT_DURABILITY = 10;
	constexpr int32 BROKEN_SOURCE_BROKEN_DURABILITY = 0;

	template <class TComponent>
	TComponent *BrokenSourceAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}

	USpellData *BrokenSourceSpell(EItemTier Tier)
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Name = TEXT("Test Spell");
		Spell->Tier = Tier;
		return Spell;
	}

	FRuntimeAttachedItem GarnetCrystal(int32 Durability)
	{
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::Crystal;
		Attached.Crystal = FCrystalAttachment(FCrystalId(ECrystalType::Garnet, EItemTier::F_Tier), Durability);
		return Attached;
	}
}

BEGIN_DEFINE_SPEC(FBrokenSourceSpec, "WoR.Combat.BrokenSources",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UInventoryComponent *Inventory = nullptr;
	ULoadoutComponent *Loadout = nullptr;
	USpellData *FTierSpell = nullptr;
	USpellData *CTierSpell = nullptr;

	FCombatLoadout &Spawn(ECharacterClass CharacterClass)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = CharacterClass;
		AActor *Actor = TestWorld->SpawnCombatant(Data);
		Inventory = BrokenSourceAddComponent<UInventoryComponent>(Actor);
		Loadout = BrokenSourceAddComponent<ULoadoutComponent>(Actor);
		Loadout->InitializeFromCharacterData(Data, Inventory);
		return Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	}

	/** A Resonator whose active ring holds an F-tier and a C-tier spell. */
	void SpawnResonatorWithRing(int32 Durability)
	{
		FCombatLoadout &Active = Spawn(ECharacterClass::Resonator);
		FRingLoadoutEntry Ring;
		Ring.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
		Ring.RingEntry.AttachedItem = GarnetCrystal(Durability);
		Ring.RingEntry.AssignedSpells = {FTierSpell, CTierSpell};
		Active.RingLoadout = {Ring};
		Active.ActiveRingIndex = 0;
	}

	/** A Generic whose primary slot is a ring holding an F-tier spell. */
	void SpawnGenericWithPrimaryRing(int32 Durability, ECharacterClass CharacterClass = ECharacterClass::Generic)
	{
		FCombatLoadout &Active = Spawn(CharacterClass);
		Active.PrimarySlotType = EPrimarySlotType::Ring;
		Active.PrimaryRing.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
		Active.PrimaryRing.RingEntry.AttachedItem = GarnetCrystal(Durability);
		Active.PrimaryRing.RingEntry.AssignedSpells = {FTierSpell};
	}

	/** A Generic whose primary weapon's crystal holds an F-tier spell. */
	void SpawnGenericWithWeapon(int32 Durability)
	{
		FCombatLoadout &Active = Spawn(ECharacterClass::Generic);
		Active.PrimarySlotType = EPrimarySlotType::Weapon;
		Active.PrimaryWeapon.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Active.PrimaryWeapon.WeaponEntry.AttachedItem = GarnetCrystal(Durability);
		Active.PrimaryWeapon.WeaponEntry.AssignedSpells = {FTierSpell};
	}

	FCombatCapabilities Capabilities(ECharacterClass CharacterClass) const
	{
		return FCombatCapabilities::BuildFrom(Loadout, CharacterClass, FLinearColor::White, [](int32) { return FLinearColor::White; });
	}
END_DEFINE_SPEC(FBrokenSourceSpec)

void FBrokenSourceSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		FTierSpell = BrokenSourceSpell(EItemTier::F_Tier);
		CTierSpell = BrokenSourceSpell(EItemTier::C_Tier);
	});

	AfterEach([this]()
	{
		Inventory = nullptr;
		Loadout = nullptr;
		FTierSpell = nullptr;
		CTierSpell = nullptr;
		TestWorld.Reset();
	});

	Describe("a broken ring crystal", [this]()
	{
		It("keeps its F-tier spells and greys out the rest", [this]()
		{
			SpawnResonatorWithRing(BROKEN_SOURCE_BROKEN_DURABILITY);
			TestTrue(TEXT("F-tier still castable"), Loadout->GetBrokenSourceReason(FTierSpell).IsEmpty());
			TestFalse(TEXT("C-tier refused with a reason"), Loadout->GetBrokenSourceReason(CTierSpell).IsEmpty());

			const FCombatCapabilities Caps = Capabilities(ECharacterClass::Resonator);
			TestEqual(TEXT("both still listed"), Caps.RingSpells.Num(), 2);
			TestTrue(TEXT("F-tier not greyed"), Caps.GetBlockedReason(FTierSpell).IsEmpty());
			TestFalse(TEXT("C-tier greyed with the reason"), Caps.GetBlockedReason(CTierSpell).IsEmpty());
		});

		It("greys out its higher-tier preset spells too", [this]()
		{
			SpawnResonatorWithRing(BROKEN_SOURCE_BROKEN_DURABILITY);
			USpellData *Preset = BrokenSourceSpell(EItemTier::B_Tier);
			Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].RingLoadout[0].RingEntry.Ring->PresetSpells.Add(Preset);
			TestFalse(TEXT("preset refused"), Loadout->GetBrokenSourceReason(Preset).IsEmpty());
		});

		It("leaves a spell that is also innate alone", [this]()
		{
			SpawnResonatorWithRing(BROKEN_SOURCE_BROKEN_DURABILITY);
			Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].InnateSpells.Add(CTierSpell);
			TestTrue(TEXT("cast as innate"), Loadout->GetBrokenSourceReason(CTierSpell).IsEmpty());
		});

		It("is no infusion source (Q-163 = B)", [this]()
		{
			SpawnResonatorWithRing(BROKEN_SOURCE_BROKEN_DURABILITY);
			UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
			if (!TestNotNull(TEXT("executor"), Executor))
			{
				return;
			}
			TestFalse(TEXT("no active ring infusion"),
					  Executor->GetAvailableInfusionSources(Loadout->GetOwner()).Contains(EInfusionSourceOption::ActiveRing));
		});

		It("refuses its F-tier spell infused through it, but casts it plain", [this]()
		{
			SpawnResonatorWithRing(BROKEN_SOURCE_BROKEN_DURABILITY);
			UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
			if (!TestNotNull(TEXT("executor"), Executor))
			{
				return;
			}
			FAction Cast;
			Cast.ActionType = EActionType::Spell;
			Cast.SpellData = FTierSpell;
			Cast.SpellSource = ESpellSource::RingCrystal;
			Cast.Targets.Add(Loadout->GetOwner());
			TestTrue(TEXT("plain cast allowed"), Executor->ValidateAction(Loadout->GetOwner(), Cast).bIsValid);
			Cast.SelectedSource = EInfusionSourceOption::ActiveRing;
			Cast.SpellInfusionLevel = 1;
			const FActionValidationResult Result = Executor->ValidateAction(Loadout->GetOwner(), Cast);
			// A broken ring offers no spells, so the spell no longer resolves to the ring and the
			// ring source is refused by the origin check.
			TestFalse(TEXT("infused cast refused"), Result.bIsValid);
		});

		It("is no infusion source as a broken primary ring either", [this]()
		{
			SpawnGenericWithPrimaryRing(BROKEN_SOURCE_BROKEN_DURABILITY);
			UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
			if (!TestNotNull(TEXT("executor"), Executor))
			{
				return;
			}
			TestFalse(TEXT("no primary ring infusion"),
					  Executor->GetAvailableInfusionSources(Loadout->GetOwner()).Contains(EInfusionSourceOption::PrimaryRing));
		});

		It("leaves a Caster's F-tier ring spell plain: no Innate infusion, a Generic spell non-elemental (Q-163b = B)", [this]()
		{
			SpawnGenericWithPrimaryRing(BROKEN_SOURCE_BROKEN_DURABILITY, ECharacterClass::Caster);
			UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
			if (!TestNotNull(TEXT("executor"), Executor))
			{
				return;
			}
			FTierSpell->Element = ESpellElement::Generic;
			TestEqual(TEXT("no infusion source at all"), Executor->GetAllowedInfusionSourcesForSpell(Loadout->GetOwner(), FTierSpell).Num(), 0);
			TestEqual(TEXT("non-elemental"), Executor->ResolveSpellCastElement(Loadout->GetOwner(), FTierSpell), ESpellElement::None);
			TestTrue(TEXT("still castable"), Loadout->GetBrokenSourceReason(FTierSpell).IsEmpty());
		});

		It("is an infusion source while intact", [this]()
		{
			SpawnResonatorWithRing(BROKEN_SOURCE_INTACT_DURABILITY);
			UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
			if (!TestNotNull(TEXT("executor"), Executor))
			{
				return;
			}
			TestTrue(TEXT("active ring infusion"),
					 Executor->GetAvailableInfusionSources(Loadout->GetOwner()).Contains(EInfusionSourceOption::ActiveRing));
		});

		It("blocks nothing while the crystal is intact", [this]()
		{
			SpawnResonatorWithRing(BROKEN_SOURCE_INTACT_DURABILITY);
			TestTrue(TEXT("C-tier castable"), Loadout->GetBrokenSourceReason(CTierSpell).IsEmpty());
			TestEqual(TEXT("nothing greyed"), Capabilities(ECharacterClass::Resonator).BlockedSpellReasons.Num(), 0);
		});
	});

	Describe("the AI (B-181)", [this]()
	{
		It("never picks a broken primary evolution's Breakthrough spells, which the loadout still lists", [this]()
		{
			FCombatLoadout &Active = Spawn(ECharacterClass::Generic);
			Active.PrimarySlotType = EPrimarySlotType::Evolution;
			Active.PrimaryEvolution.Item = NewObject<UEvolutionItemData>(GetTransientPackage());
			Active.PrimaryEvolution.CurrentDurability = BROKEN_SOURCE_BROKEN_DURABILITY;
			Active.EvolutionSpells = {FTierSpell};
			TestTrue(TEXT("the loadout still lists it"), Loadout->GetAvailableSpells().Contains(FTierSpell));
			TestFalse(TEXT("the AI does not pick it"), UAIDecisionManager::GetUsableSpells(Loadout).Contains(FTierSpell));
			Active.PrimaryEvolution.CurrentDurability = BROKEN_SOURCE_INTACT_DURABILITY;
			TestTrue(TEXT("the AI picks it while intact"), UAIDecisionManager::GetUsableSpells(Loadout).Contains(FTierSpell));
		});

		It("never picks a broken ring's higher-tier preset, which the loadout still lists", [this]()
		{
			SpawnResonatorWithRing(BROKEN_SOURCE_BROKEN_DURABILITY);
			USpellData *HighPreset = BrokenSourceSpell(EItemTier::B_Tier);
			Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].RingLoadout[0].RingEntry.Ring->PresetSpells = {HighPreset};
			TestTrue(TEXT("the loadout still lists the B-tier preset"), Loadout->GetAvailableSpells().Contains(HighPreset));
			TestFalse(TEXT("the AI does not pick it"), UAIDecisionManager::GetUsableSpells(Loadout).Contains(HighPreset));
		});

		It("never picks a spell from a broken weapon crystal", [this]()
		{
			SpawnGenericWithWeapon(BROKEN_SOURCE_BROKEN_DURABILITY);
			TestFalse(TEXT("not offered"), UAIDecisionManager::GetUsableSpells(Loadout).Contains(FTierSpell));
		});

		It("picks from every spell while the source is intact", [this]()
		{
			SpawnResonatorWithRing(BROKEN_SOURCE_INTACT_DURABILITY);
			TestEqual(TEXT("same as the loadout"), UAIDecisionManager::GetUsableSpells(Loadout).Num(), Loadout->GetAvailableSpells().Num());
			TestEqual(TEXT("no loadout: none"), UAIDecisionManager::GetUsableSpells(nullptr).Num(), 0);
		});
	});

	Describe("a broken weapon crystal", [this]()
	{
		It("greys out every spell it gave, F-tier included", [this]()
		{
			SpawnGenericWithWeapon(BROKEN_SOURCE_BROKEN_DURABILITY);
			TestFalse(TEXT("refused with a reason"), Loadout->GetBrokenSourceReason(FTierSpell).IsEmpty());

			const FCombatCapabilities Caps = Capabilities(ECharacterClass::Generic);
			TestTrue(TEXT("Resonate Weapon still shown"), Caps.bHasWeaponCrystal);
			TestEqual(TEXT("its spell still listed"), Caps.WeaponCrystalSpells.Num(), 1);
			TestFalse(TEXT("greyed"), Caps.GetBlockedReason(FTierSpell).IsEmpty());
			TestTrue(TEXT("plain attacks still work"), Caps.bCanAttack);
		});

		It("can't be infused through", [this]()
		{
			SpawnGenericWithWeapon(BROKEN_SOURCE_BROKEN_DURABILITY);
			UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
			if (!TestNotNull(TEXT("executor"), Executor))
			{
				return;
			}
			TestFalse(TEXT("no weapon crystal infusion source"),
					  Executor->GetAvailableInfusionSources(Loadout->GetOwner()).Contains(EInfusionSourceOption::WeaponCrystal));
		});

		It("refuses an ability infused through it", [this]()
		{
			SpawnGenericWithWeapon(BROKEN_SOURCE_BROKEN_DURABILITY);
			UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
			if (!TestNotNull(TEXT("executor"), Executor))
			{
				return;
			}
			FAction Swing;
			Swing.ActionType = EActionType::Ability;
			Swing.SkillData = NewObject<UAbilityData>(GetTransientPackage());
			Swing.SelectedSource = EInfusionSourceOption::WeaponCrystal;
			Swing.AbilityInfusionLevel = 1;
			Swing.Targets.Add(Loadout->GetOwner());
			const FActionValidationResult Result = Executor->ValidateAction(Loadout->GetOwner(), Swing);
			TestFalse(TEXT("refused"), Result.bIsValid);
			TestEqual(TEXT("for the source"), Result.ErrorMessage, FString(TEXT("Infusion source not available")));
		});

		It("is refused by the executor", [this]()
		{
			SpawnGenericWithWeapon(BROKEN_SOURCE_BROKEN_DURABILITY);
			UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
			if (!TestNotNull(TEXT("executor"), Executor))
			{
				return;
			}
			FAction Cast;
			Cast.ActionType = EActionType::Spell;
			Cast.SpellData = FTierSpell;
			Cast.SpellSource = ESpellSource::WeaponCrystal;
			Cast.Targets.Add(Loadout->GetOwner());
			const FActionValidationResult Result = Executor->ValidateAction(Loadout->GetOwner(), Cast);
			TestFalse(TEXT("refused"), Result.bIsValid);
			TestEqual(TEXT("with the broken-source reason"), Result.ErrorMessage, Loadout->GetBrokenSourceReason(FTierSpell).ToString());
		});
	});

	Describe("a broken primary evolution (kept since Q-159)", [this]()
	{
		It("greys out its Breakthrough spells", [this]()
		{
			FCombatLoadout &Active = Spawn(ECharacterClass::Generic);
			Active.PrimarySlotType = EPrimarySlotType::Evolution;
			Active.PrimaryEvolution.Item = NewObject<UEvolutionItemData>(GetTransientPackage());
			Active.PrimaryEvolution.CurrentDurability = BROKEN_SOURCE_BROKEN_DURABILITY;
			Active.EvolutionSpells = {FTierSpell};
			TestFalse(TEXT("greyed while broken"), Loadout->GetBrokenSourceReason(FTierSpell).IsEmpty());
			const FCombatCapabilities Caps = Capabilities(ECharacterClass::Generic);
			TestTrue(TEXT("listed in the menu"), Caps.BreakthroughSpells.Contains(FTierSpell));
			TestFalse(TEXT("greyed in the menu"), Caps.GetBlockedReason(FTierSpell).IsEmpty());
			Active.PrimaryEvolution.CurrentDurability = BROKEN_SOURCE_INTACT_DURABILITY;
			TestTrue(TEXT("usable while intact"), Loadout->GetBrokenSourceReason(FTierSpell).IsEmpty());
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
