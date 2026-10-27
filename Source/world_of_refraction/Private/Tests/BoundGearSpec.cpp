// BoundGearSpec.cpp
// B-192 (D-033, Q-192): an item can belong to a lord; anyone can wear it, only the lord gets its
// bound bonus, and the bonus line is hidden from everyone else.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Effects/EffectDefinition.h"

namespace
{
	constexpr int32 BOUND_TEST_BONUS_PERCENT = 10;

	UEffectDefinition *BoundGearBonus()
	{
		FSkillEffectPayload Payload;
		Payload.EffectType = ESkillEffectType::DamageBuff;
		Payload.Value = BOUND_TEST_BONUS_PERCENT;
		Payload.Target = ETargetType::Self;
		FSkillEffect Effect;
		Effect.Payloads.Add(Payload);
		UEffectDefinition *Definition = NewObject<UEffectDefinition>(GetTransientPackage());
		Definition->Effects.Add(Effect);
		return Definition;
	}
}

BEGIN_DEFINE_SPEC(FBoundGearSpec, "WoR.Equipment.BoundGear",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UWeaponData *Sword = nullptr;
	UCharacterData *Lord = nullptr;

	/** Data's character, a Generic wearing Weapon as its primary. */
	ULoadoutComponent *SpawnWearing(UCharacterData *Data, UWeaponData *Weapon)
	{
		Data->CharacterClass = ECharacterClass::Generic;
		return TestWorld->SpawnCombatantWithLoadout(Data, Weapon);
	}
END_DEFINE_SPEC(FBoundGearSpec)

void FBoundGearSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Lord = FWoRTestWorld::NewCharacterData();
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->BoundOwner = Lord;
		Sword->BoundEffects.Add(BoundGearBonus());
	});

	AfterEach([this]()
	{
		Sword = nullptr;
		Lord = nullptr;
		TestWorld.Reset();
	});

	It("gives the bound bonus to its owner, not to another wearer", [this]()
	{
		ULoadoutComponent *OnLord = SpawnWearing(Lord, Sword);
		ULoadoutComponent *OnOther = SpawnWearing(FWoRTestWorld::NewCharacterData(), Sword);
		TestEqual(TEXT("The owner gets the bonus"), OnLord->GetActiveEffectsGathered(OnLord->GetOwner()).Num(), 1);
		TestEqual(TEXT("Another wearer doesn't"), OnOther->GetActiveEffectsGathered(OnOther->GetOwner()).Num(), 0);
	});

	It("hides the bonus line unless the owner wears it", [this]()
	{
		TestFalse(TEXT("The owner sees it"), Sword->GetBoundBonusText(Lord).IsEmpty());
		TestTrue(TEXT("Another wearer doesn't"), Sword->GetBoundBonusText(FWoRTestWorld::NewCharacterData()).IsEmpty());
		TestTrue(TEXT("Nobody wearing it doesn't"), Sword->GetBoundBonusText(nullptr).IsEmpty());
	});

	It("gives an unbound item's bonus field to nobody", [this]()
	{
		UWeaponData *Unbound = NewObject<UWeaponData>(GetTransientPackage());
		Unbound->BoundEffects.Add(BoundGearBonus());
		TestFalse(TEXT("No owner: never active"), Unbound->IsBoundBonusActive(nullptr));
		ULoadoutComponent *Wearer = SpawnWearing(FWoRTestWorld::NewCharacterData(), Unbound);
		TestEqual(TEXT("No bonus gathered"), Wearer->GetActiveEffectsGathered(Wearer->GetOwner()).Num(), 0);
	});
}

#endif
