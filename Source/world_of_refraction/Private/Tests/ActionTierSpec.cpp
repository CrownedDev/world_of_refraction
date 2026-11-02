// ActionTierSpec.cpp
// B-245 (DRY audit A3, Q-245): one action tier for tier-gap damage and crystal wear. An ability reads its own
// tier, not the weapon's; a spell reads the spell's. The wear paths (ApplyCommitCosts, ApplyCrystalWearOn)
// pass ResolveActionTier, so pinning it pins both.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

BEGIN_DEFINE_SPEC(FActionTierSpec, "WoR.Combat.ActionTier", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FActionTierSpec)

void FActionTierSpec::Define()
{
	AfterEach([this]() { TestWorld.Reset(); });

	It("gives an ability its own tier, not the weapon's, and a spell the spell's", [this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		if (!TestTrue(TEXT("Test world created"), TestWorld->Create())) return;
		UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->Tier = EItemTier::F_Tier;
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(FWoRTestWorld::NewCharacterData(), Weapon);
		if (!TestNotNull(TEXT("Executor"), Executor) || !TestNotNull(TEXT("Loadout"), Loadout)) return;
		AActor *User = Loadout->GetOwner();

		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->Tier = EItemTier::C_Tier;
		FAction AbilityAction;
		AbilityAction.ActionType = EActionType::Ability;
		AbilityAction.SkillData = Ability;
		TestEqual(TEXT("ability: its own tier"), Executor->ResolveActionTier(User, AbilityAction),
				  UInventoryComponent::ResolveAbilityTier(User, Ability));
		TestNotEqual(TEXT("ability: not the weapon's"), Executor->ResolveActionTier(User, AbilityAction), Weapon->Tier);

		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Tier = EItemTier::B_Tier;
		FAction SpellAction;
		SpellAction.ActionType = EActionType::Spell;
		SpellAction.SpellData = Spell;
		TestEqual(TEXT("spell: the spell's tier"), Executor->ResolveActionTier(User, SpellAction),
				  UInventoryComponent::ResolveSpellTier(User, Spell));
	});
}

#endif
