// GearSwapEffectsSpec.cpp
// B-081 (D-008): gear effects follow a mid-fight swap. A ring switch takes the old ring's
// starting effects off and puts the new ring's on; swapping back gives them back once.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/Rings/RingData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Effects/EffectDefinition.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 GEAR_SWAP_TEST_BUFF_PERCENT = 10;
	constexpr int32 GEAR_SWAP_EFFECT_RING = 0;
	constexpr int32 GEAR_SWAP_PLAIN_RING = 1;

	template <class TComponent>
	TComponent *GearSwapAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}

	UEffectDefinition *GearSwapBuff()
	{
		FSkillEffectPayload Payload;
		Payload.EffectType = ESkillEffectType::DamageBuff;
		Payload.Value = GEAR_SWAP_TEST_BUFF_PERCENT;
		Payload.Target = ETargetType::Self;
		Payload.bPermanent = true; // a gear passive, like an authored ring effect
		FSkillEffect Effect;
		Effect.EffectName = TEXT("Ring buff");
		Effect.Payloads.Add(Payload);
		UEffectDefinition *Definition = NewObject<UEffectDefinition>(GetTransientPackage());
		Definition->Effects.Add(Effect);
		return Definition;
	}
}

BEGIN_DEFINE_SPEC(FGearSwapEffectsSpec, "WoR.Effects.GearSwap",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ULoadoutComponent *Loadout = nullptr;
	USkillEffectManager *Effects = nullptr;
	AActor *Wearer = nullptr;

	/** Battle prep for gear (ACombatOrchestrator::PrepareAllLoadoutsForBattle). */
	void ArmForFight()
	{
		Effects->ArmConditionalEffects(Wearer, Loadout->GetActiveConditionalEffectsGathered(Wearer));
		Effects->ApplyEquipmentEffects(Wearer, Loadout->GetActiveEffectsGathered(Wearer));
	}
END_DEFINE_SPEC(FGearSwapEffectsSpec)

void FGearSwapEffectsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		TestNotNull(TEXT("effect manager"), Effects);

		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Resonator;
		Wearer = TestWorld->SpawnCombatant(Data);
		UInventoryComponent *Inventory = GearSwapAddComponent<UInventoryComponent>(Wearer);
		Loadout = GearSwapAddComponent<ULoadoutComponent>(Wearer);
		Loadout->InitializeFromCharacterData(Data, Inventory);

		FRingLoadoutEntry EffectRing;
		EffectRing.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
		EffectRing.RingEntry.Ring->ReferencedEffects.Add(GearSwapBuff());
		FRingLoadoutEntry PlainRing;
		PlainRing.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.RingLoadout = {EffectRing, PlainRing};
		Active.ActiveRingIndex = GEAR_SWAP_EFFECT_RING;
	});

	AfterEach([this]()
	{
		if (Effects)
		{
			Effects->ClearAllEffects();
		}
		Loadout = nullptr;
		Effects = nullptr;
		Wearer = nullptr;
		TestWorld.Reset();
	});

	It("takes the old ring's effect off and gives it back once on the swap back", [this]()
	{
		if (!Effects)
		{
			return;
		}
		ArmForFight();
		const int32 WithRing = Effects->GetEffectCount(Wearer);
		TestTrue(TEXT("the ring's buff is on at the start"), WithRing > 0);

		Loadout->SetActiveRingIndex(GEAR_SWAP_PLAIN_RING);
		TestEqual(TEXT("switched away: the buff is off"), Effects->GetEffectCount(Wearer), 0);

		Loadout->SetActiveRingIndex(GEAR_SWAP_EFFECT_RING);
		TestEqual(TEXT("switched back: on once, no stacking"), Effects->GetEffectCount(Wearer), WithRing);
		Loadout->SetActiveRingIndex(GEAR_SWAP_EFFECT_RING);
		TestEqual(TEXT("the same ring again changes nothing"), Effects->GetEffectCount(Wearer), WithRing);
	});

	It("leaves effects alone out of a fight", [this]()
	{
		if (!Effects)
		{
			return;
		}
		Loadout->SetActiveRingIndex(GEAR_SWAP_PLAIN_RING);
		Loadout->SetActiveRingIndex(GEAR_SWAP_EFFECT_RING);
		TestEqual(TEXT("no gear armed: nothing applied"), Effects->GetEffectCount(Wearer), 0);
		TestFalse(TEXT("not armed"), Effects->IsGearArmed(Wearer));
	});

	It("stops following swaps once the fight's effects are cleared", [this]()
	{
		if (!Effects)
		{
			return;
		}
		ArmForFight();
		Effects->ClearAllEffects();
		TestFalse(TEXT("combat end disarms the gear"), Effects->IsGearArmed(Wearer));
		Loadout->SetActiveRingIndex(GEAR_SWAP_PLAIN_RING);
		Loadout->SetActiveRingIndex(GEAR_SWAP_EFFECT_RING);
		TestEqual(TEXT("nothing re-applied after the fight"), Effects->GetEffectCount(Wearer), 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
