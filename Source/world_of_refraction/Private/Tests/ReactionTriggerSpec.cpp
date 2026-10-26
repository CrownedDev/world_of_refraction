// ReactionTriggerSpec.cpp
// B-191 (RunStructure §5, D-032, Q-191): a reaction fires when one party member wears every listed
// item while facing the listed character, not when one item is missing; its effects land on the
// wearer or the faced character; a run memory weakens the boss it names.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Memory/MemoryData.h"
#include "Reaction/ReactionTrigger.h"

namespace
{
	constexpr int32 REACTION_TEST_DEBUFF_PERCENT = 20;

	template <class TComponent>
	TComponent *ReactionAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}

	FSkillEffect ReactionMaxHPDebuff(ETargetType Target)
	{
		FSkillEffectPayload Payload;
		Payload.EffectType = ESkillEffectType::MaxHPDebuff;
		Payload.Value = REACTION_TEST_DEBUFF_PERCENT;
		Payload.Target = Target;
		Payload.bPermanent = true;
		FSkillEffect Effect;
		Effect.Payloads.Add(Payload);
		return Effect;
	}
}

BEGIN_DEFINE_SPEC(FReactionTriggerSpec, "WoR.Reaction.Trigger",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UWeaponData *Sword = nullptr;
	UWeaponData *Dagger = nullptr;
	UCharacterData *Lord = nullptr;

	/** A Generic wearing Primary (and Secondary, when set). */
	AActor *SpawnWearer(UWeaponData *Primary, UWeaponData *Secondary)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Generic;
		AActor *Actor = TestWorld->SpawnCombatant(Data);
		UInventoryComponent *Inventory = ReactionAddComponent<UInventoryComponent>(Actor);
		ULoadoutComponent *Loadout = ReactionAddComponent<ULoadoutComponent>(Actor);
		Loadout->InitializeFromCharacterData(Data, Inventory);
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.PrimarySlotType = EPrimarySlotType::Weapon;
		Active.PrimaryWeapon.WeaponEntry.Weapon = Primary;
		if (Secondary)
		{
			Active.SecondarySlotType = ESecondarySlotType::Weapon;
			Active.SecondaryWeapon.WeaponEntry.Weapon = Secondary;
		}
		return Actor;
	}

	FReactionTrigger SwordAndDaggerFacingLord() const
	{
		FReactionTrigger Trigger;
		Trigger.TriggerId = TEXT("SpecReaction");
		Trigger.RequiredItems = {Sword, Dagger};
		Trigger.FacingCharacter = Lord;
		return Trigger;
	}
END_DEFINE_SPEC(FReactionTriggerSpec)

void FReactionTriggerSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Dagger = NewObject<UWeaponData>(GetTransientPackage());
		Lord = FWoRTestWorld::NewCharacterData();
	});

	AfterEach([this]()
	{
		Sword = nullptr;
		Dagger = nullptr;
		Lord = nullptr;
		TestWorld.Reset();
	});

	It("fires when every item is worn facing the character, not with one missing", [this]()
	{
		AActor *Both = SpawnWearer(Sword, Dagger);
		AActor *SwordOnly = SpawnWearer(Sword, nullptr);
		AActor *Boss = TestWorld->SpawnCombatant(Lord, true);
		AActor *Stranger = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		const FReactionTrigger Trigger = SwordAndDaggerFacingLord();
		TestEqual(TEXT("Worn: both weapons"), ReactionTriggers::GetWornItems(Both).Num(), 2);
		TestTrue(TEXT("All worn, facing the lord: fires"), ReactionTriggers::Matches(Trigger, Both, Boss));
		TestFalse(TEXT("One missing: doesn't"), ReactionTriggers::Matches(Trigger, SwordOnly, Boss));
		TestFalse(TEXT("Facing someone else: doesn't"), ReactionTriggers::Matches(Trigger, Both, Stranger));

		const TArray<FReactionTrigger> Triggers = {Trigger};
		TestEqual(TEXT("Fires once for the fight"), ReactionTriggers::FindFired(Triggers, {SwordOnly, Both}, {Stranger, Boss}).Num(), 1);
		TestEqual(TEXT("Nobody wears both: nothing"), ReactionTriggers::FindFired(Triggers, {SwordOnly}, {Boss}).Num(), 0);
	});

	It("puts an Enemy effect on the faced character and a Self effect on the wearer", [this]()
	{
		AActor *Wearer = SpawnWearer(Sword, Dagger);
		AActor *Boss = TestWorld->SpawnCombatant(Lord, true);
		UCharacterDataComponent *WearerStats = Wearer->FindComponentByClass<UCharacterDataComponent>();
		UCharacterDataComponent *BossStats = Boss->FindComponentByClass<UCharacterDataComponent>();
		const int32 WearerMaxHP = WearerStats->MaxHP;
		const int32 BossMaxHP = BossStats->MaxHP;

		FReactionTrigger OnEnemy = SwordAndDaggerFacingLord();
		OnEnemy.Effects = {ReactionMaxHPDebuff(ETargetType::Enemy)};
		const TArray<FReactionTrigger> Triggers = {OnEnemy};
		const int32 Applied = ReactionTriggers::ApplyFired(ReactionTriggers::FindFired(Triggers, {Wearer}, {Boss}));
		TestEqual(TEXT("One effect applied"), Applied, 1);
		TestTrue(TEXT("The faced character lost max HP"), BossStats->MaxHP < BossMaxHP);
		TestEqual(TEXT("The wearer did not"), WearerStats->MaxHP, WearerMaxHP);

		FReactionTrigger OnSelf = SwordAndDaggerFacingLord();
		OnSelf.Effects = {ReactionMaxHPDebuff(ETargetType::Self)};
		const TArray<FReactionTrigger> SelfTriggers = {OnSelf};
		ReactionTriggers::ApplyFired(ReactionTriggers::FindFired(SelfTriggers, {Wearer}, {Boss}));
		TestTrue(TEXT("A Self effect lands on the wearer"), WearerStats->MaxHP < WearerMaxHP);
	});

	It("weakens the boss a run memory names, and leaves the others", [this]()
	{
		AActor *Boss = TestWorld->SpawnCombatant(Lord, true);
		AActor *Other = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		UCharacterDataComponent *BossStats = Boss->FindComponentByClass<UCharacterDataComponent>();
		UCharacterDataComponent *OtherStats = Other->FindComponentByClass<UCharacterDataComponent>();
		const int32 BossMaxHP = BossStats->MaxHP;
		const int32 OtherMaxHP = OtherStats->MaxHP;

		UMemoryData *Memory = NewObject<UMemoryData>(GetTransientPackage());
		Memory->BossModifiers.Add({Lord, EMemoryBossModifierKind::Weaken});
		Memory->BossModifiers.Add({Lord, EMemoryBossModifierKind::SkipPhase});
		const TArray<TObjectPtr<UMemoryData>> RunMemories = {Memory};
		TestEqual(TEXT("One change applied (the phase waits for B-193)"),
				  ReactionTriggers::ApplyMemoryBossModifiers(RunMemories, {Boss, Other}), 1);
		TestTrue(TEXT("The boss is weaker"), BossStats->MaxHP < BossMaxHP);
		TestEqual(TEXT("It starts at full HP"), BossStats->CurrentHP, BossStats->MaxHP);
		TestEqual(TEXT("The other is untouched"), OtherStats->MaxHP, OtherMaxHP);
	});

	It("adds up memories on one boss: weaken and strengthen cancel, two weakens go further", [this]()
	{
		AActor *Boss = TestWorld->SpawnCombatant(Lord, true);
		UCharacterDataComponent *BossStats = Boss->FindComponentByClass<UCharacterDataComponent>();
		const int32 BossMaxHP = BossStats->MaxHP;

		UMemoryData *Weaken = NewObject<UMemoryData>(GetTransientPackage());
		Weaken->BossModifiers.Add({Lord, EMemoryBossModifierKind::Weaken});
		UMemoryData *Strengthen = NewObject<UMemoryData>(GetTransientPackage());
		Strengthen->BossModifiers.Add({Lord, EMemoryBossModifierKind::Strengthen});
		const TArray<TObjectPtr<UMemoryData>> Cancelling = {Weaken, Strengthen};
		TestEqual(TEXT("Weaken + strengthen: no change"), ReactionTriggers::ApplyMemoryBossModifiers(Cancelling, {Boss}), 0);
		TestEqual(TEXT("Max HP as it was"), BossStats->MaxHP, BossMaxHP);

		AActor *OneStepBoss = TestWorld->SpawnCombatant(Lord, true);
		UCharacterDataComponent *OneStepStats = OneStepBoss->FindComponentByClass<UCharacterDataComponent>();
		const TArray<TObjectPtr<UMemoryData>> OneWeaken = {Weaken};
		const TArray<TObjectPtr<UMemoryData>> TwoWeakens = {Weaken, NewObject<UMemoryData>(GetTransientPackage())};
		TwoWeakens[1]->BossModifiers.Add({Lord, EMemoryBossModifierKind::Weaken});
		ReactionTriggers::ApplyMemoryBossModifiers(OneWeaken, {OneStepBoss});
		ReactionTriggers::ApplyMemoryBossModifiers(TwoWeakens, {Boss});
		TestTrue(TEXT("Two weakens go further than one"), BossStats->MaxHP < OneStepStats->MaxHP);
	});
}

#endif
