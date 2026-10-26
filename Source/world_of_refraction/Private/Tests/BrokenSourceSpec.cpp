// BrokenSourceSpec.cpp
// D-083 (B-163): a spell whose source's crystal breaks mid-fight stays in the menu, greyed out with
// the reason, and can't be cast; a broken ring keeps its F-tier spells.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
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

		It("blocks nothing while the crystal is intact", [this]()
		{
			SpawnResonatorWithRing(BROKEN_SOURCE_INTACT_DURABILITY);
			TestTrue(TEXT("C-tier castable"), Loadout->GetBrokenSourceReason(CTierSpell).IsEmpty());
			TestEqual(TEXT("nothing greyed"), Capabilities(ECharacterClass::Resonator).BlockedSpellReasons.Num(), 0);
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
}

#endif // WITH_DEV_AUTOMATION_TESTS
