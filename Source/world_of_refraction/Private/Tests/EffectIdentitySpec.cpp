// EffectIdentitySpec.cpp
// Effect identity (B-062): an effect is (holder, EffectID, source).
// - Fires-once is one use per holder and source (D-011); instants spend it too.
// - Full bars: the same attacker refreshes, another attacker adds (D-012).
// - Gear: the item is the source; two items with one definition add up (D-053),
//   unless the effect doesn't stack. A same-source re-apply replaces (Q-044 A).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/EffectIdentity.h"
#include "Skills/Effects/FGatheredEffect.h"
#include "Combat/CombatConstants.h"
#include "Skills/Effects/EffectDefinition.h"
#include "Equipment/Rings/RingData.h"

namespace
{
	constexpr int32 IDENTITY_TEST_EFFECT_ID = 9301;
	constexpr int32 IDENTITY_TEST_DEF_ID = 93;
	constexpr int32 IDENTITY_TEST_ITEM_A = 7001;
	constexpr int32 IDENTITY_TEST_ITEM_B = 7002;
	constexpr float IDENTITY_TEST_GEAR_MAGNITUDE = 0.1f;
	constexpr float IDENTITY_TEST_STRONG_VALUE = 30.0f;
	constexpr float IDENTITY_TEST_WEAK_VALUE = 10.0f;
	constexpr float IDENTITY_TEST_HEAL_VALUE = 10.0f;
	constexpr int32 IDENTITY_TEST_DURATION_TURNS = 2;
}

BEGIN_DEFINE_SPEC(FEffectIdentitySpec, "WoR.Effects.Identity",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;
	UCharacterData *Data = nullptr;

	AActor *Spawn()
	{
		return TestWorld->SpawnCombatant(Data);
	}

	TArray<FActiveSkillEffect> EffectsOfType(AActor *Actor, ESkillEffectType Type) const
	{
		return EffectManager->GetActiveEffects(Actor).FilterByPredicate(
			[Type](const FActiveSkillEffect &Effect) { return Effect.EffectType == Type; });
	}

	/** One gear effect from IDENTITY_TEST_DEF_ID, given by the item SourceKey. */
	static FGatheredEffect GearEffect(int32 ItemSourceKey, bool bFiresOnce = false, bool bDoesNotStack = false)
	{
		FSkillEffectPayload Payload;
		Payload.EffectType = ESkillEffectType::SpellDamageBuff;
		Payload.Magnitude = IDENTITY_TEST_GEAR_MAGNITUDE;
		Payload.bPermanent = true;
		Payload.ProcessTiming = ESkillEffectTiming::Persistent;

		FSkillEffect Effect;
		Effect.EffectName = TEXT("Identity test gear");
		Effect.Payloads.Add(Payload);
		Effect.bFiresOncePerMatch = bFiresOnce;
		Effect.bDoesNotStack = bDoesNotStack;
		return FGatheredEffect(IDENTITY_TEST_DEF_ID, 0, Effect, ItemSourceKey);
	}

	static FActiveSkillEffect Debuff(float Value)
	{
		return FActiveSkillEffect::CreateBuff(TEXT("Identity test debuff"), IDENTITY_TEST_EFFECT_ID,
											  ESkillEffectType::DefenseDebuff, Value, IDENTITY_TEST_DURATION_TURNS);
	}

	static FActiveSkillEffect InstantHeal()
	{
		FActiveSkillEffect Heal = FActiveSkillEffect::CreateBuff(TEXT("Identity test heal"), IDENTITY_TEST_EFFECT_ID,
																 ESkillEffectType::HealthRestore, IDENTITY_TEST_HEAL_VALUE, 0);
		Heal.bFiresOncePerMatch = true;
		return Heal;
	}
END_DEFINE_SPEC(FEffectIdentitySpec)

void FEffectIdentitySpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		TestNotNull(TEXT("SkillEffectManager"), EffectManager);
		Data = FWoRTestWorld::NewCharacterData();
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
		EffectManager = nullptr;
	});

	Describe("Once per match (D-011)", [this]()
	{
		It("gives each wearer of the same gear its own use", [this]()
		{
			if (!EffectManager) return;
			AActor *WearerA = Spawn();
			AActor *WearerB = Spawn();
			const TArray<FGatheredEffect> Gear{GearEffect(IDENTITY_TEST_ITEM_A, /*bFiresOnce*/ true)};

			EffectManager->ApplyEquipmentEffects(WearerA, Gear);
			EffectManager->ApplyEquipmentEffects(WearerB, Gear);
			TestEqual(TEXT("Wearer A has its effect"), EffectsOfType(WearerA, ESkillEffectType::SpellDamageBuff).Num(), 1);
			TestEqual(TEXT("Wearer B has its effect"), EffectsOfType(WearerB, ESkillEffectType::SpellDamageBuff).Num(), 1);

			FActiveSkillEffect Again = FActiveSkillEffect::CreateAllFromSkillEffect(TEXT("Gear"), IDENTITY_TEST_DEF_ID, Gear[0].Effect, 0)[0];
			Again.SourceKey = IDENTITY_TEST_ITEM_A;
			TestTrue(TEXT("Wearer A's second use is refused"),
					 EffectManager->ApplyEffect(WearerA, Again) == EEffectApplicationResult::Rejected);
		});

		It("spends an instant's use too", [this]()
		{
			if (!EffectManager) return;
			AActor *Holder = Spawn();
			AActor *Source = Spawn();
			const FActiveSkillEffect Heal = InstantHeal();
			TestTrue(TEXT("The heal is an instant"), Heal.IsInstant());

			TestTrue(TEXT("First use lands"),
					 EffectManager->ApplyEffect(Holder, Heal, Source, TEXT("Test")) == EEffectApplicationResult::Applied);
			TestTrue(TEXT("Second use is refused"),
					 EffectManager->ApplyEffect(Holder, Heal, Source, TEXT("Test")) == EEffectApplicationResult::Rejected);
			TestTrue(TEXT("Another holder still gets its use"),
					 EffectManager->ApplyEffect(Spawn(), Heal, Source, TEXT("Test")) == EEffectApplicationResult::Applied);
		});

		It("gives a doesn't-stack effect one use per holder, whichever item brings it", [this]()
		{
			if (!EffectManager) return;
			AActor *Wearer = Spawn();
			EffectManager->ApplyEquipmentEffects(Wearer, {GearEffect(IDENTITY_TEST_ITEM_A, true, /*bDoesNotStack*/ true)});
			TestEqual(TEXT("The first item's use lands"), EffectsOfType(Wearer, ESkillEffectType::SpellDamageBuff).Num(), 1);
			EffectManager->RemoveAllEffects(Wearer);
			EffectManager->ApplyEquipmentEffects(Wearer, {GearEffect(IDENTITY_TEST_ITEM_B, true, /*bDoesNotStack*/ true)});
			TestEqual(TEXT("The second item's use is refused"), EffectsOfType(Wearer, ESkillEffectType::SpellDamageBuff).Num(), 0);
		});
	});

	Describe("Full bars (D-012)", [this]()
	{
		It("refresh when the same attacker fills the bar again", [this]()
		{
			if (!EffectManager) return;
			AActor *Holder = Spawn();
			AActor *Attacker = Spawn();

			EffectManager->ApplyTriggeredSkillEffect(Attacker, Holder, ESkillEffectType::DOT, ESpellElement::Fire);
			EffectManager->ProcessEndOfTurnEffects(Holder);
			const TArray<FActiveSkillEffect> Ticked = EffectsOfType(Holder, ESkillEffectType::DOT);
			if (!TestEqual(TEXT("One Burn after the first fill"), Ticked.Num(), 1)) return;
			TestEqual(TEXT("A turn has ticked off"), Ticked[0].RemainingTurns, CombatConstants::STATUS_CAP_TURNS - 1);

			EffectManager->ApplyTriggeredSkillEffect(Attacker, Holder, ESkillEffectType::DOT, ESpellElement::Fire);
			const TArray<FActiveSkillEffect> Refreshed = EffectsOfType(Holder, ESkillEffectType::DOT);
			if (!TestEqual(TEXT("Still one Burn"), Refreshed.Num(), 1)) return;
			TestEqual(TEXT("Its timer is reset"), Refreshed[0].RemainingTurns, CombatConstants::STATUS_CAP_TURNS);
		});

		It("add a copy for each attacker", [this]()
		{
			if (!EffectManager) return;
			AActor *Holder = Spawn();
			AActor *AttackerA = Spawn();
			AActor *AttackerB = Spawn();

			EffectManager->ApplyTriggeredSkillEffect(AttackerA, Holder, ESkillEffectType::DOT, ESpellElement::Fire);
			EffectManager->ApplyTriggeredSkillEffect(AttackerB, Holder, ESkillEffectType::DOT, ESpellElement::Fire);
			TestEqual(TEXT("Two Burns"), EffectsOfType(Holder, ESkillEffectType::DOT).Num(), 2);
		});

		It("use one fixed ID per status and element", [this]()
		{
			if (!EffectManager) return;
			AActor *Holder = Spawn();
			EffectManager->ApplyTriggeredSkillEffect(Spawn(), Holder, ESkillEffectType::DOT, ESpellElement::Fire);
			const TArray<FActiveSkillEffect> Burns = EffectsOfType(Holder, ESkillEffectType::DOT);
			if (!TestEqual(TEXT("One Burn"), Burns.Num(), 1)) return;
			TestEqual(TEXT("Burn's ID"), Burns[0].EffectID,
					  EffectIdentity::MakeStatusCapEffectID(static_cast<uint8>(ESkillEffectType::DOT), static_cast<uint8>(ESpellElement::Fire)));
		});
	});

	Describe("Gear and skills (D-053)", [this]()
	{
		It("adds one copy per item with the same effect", [this]()
		{
			if (!EffectManager) return;
			AActor *Wearer = Spawn();
			EffectManager->ApplyEquipmentEffects(Wearer, {GearEffect(IDENTITY_TEST_ITEM_A), GearEffect(IDENTITY_TEST_ITEM_B)});
			TestEqual(TEXT("Two items, two copies"), EffectsOfType(Wearer, ESkillEffectType::SpellDamageBuff).Num(), 2);
		});

		It("refreshes the copy when the same item applies again", [this]()
		{
			if (!EffectManager) return;
			AActor *Wearer = Spawn();
			EffectManager->ApplyEquipmentEffects(Wearer, {GearEffect(IDENTITY_TEST_ITEM_A), GearEffect(IDENTITY_TEST_ITEM_A)});
			TestEqual(TEXT("One item, one copy"), EffectsOfType(Wearer, ESkillEffectType::SpellDamageBuff).Num(), 1);
		});

		It("gives each item its own source key when gathered", [this]()
		{
			if (!EffectManager) return;
			UEffectDefinition *Def = NewObject<UEffectDefinition>();
			Def->Effects.Add(GearEffect(0).Effect);
			URingData *RingA = NewObject<URingData>();
			URingData *RingB = NewObject<URingData>();
			RingA->ReferencedEffects.Add(Def);
			RingB->ReferencedEffects.Add(Def);
			const TArray<FGatheredEffect> FromA = RingA->GetStartingEffectsGathered();
			const TArray<FGatheredEffect> FromB = RingB->GetStartingEffectsGathered();
			if (!TestEqual(TEXT("Ring A gives one effect"), FromA.Num(), 1) || !TestEqual(TEXT("Ring B gives one effect"), FromB.Num(), 1)) return;
			TestNotEqual(TEXT("Ring A's key is stamped"), FromA[0].SourceKey, 0);
			TestNotEqual(TEXT("The two rings are different sources"), FromA[0].SourceKey, FromB[0].SourceKey);
			TestEqual(TEXT("Same definition, same slot"), FromA[0].DefID, FromB[0].DefID);
		});

		It("refreshes a doesn't-stack effect when the same item applies again", [this]()
		{
			if (!EffectManager) return;
			AActor *Wearer = Spawn();
			const FGatheredEffect Gear = GearEffect(IDENTITY_TEST_ITEM_A, false, /*bDoesNotStack*/ true);
			EffectManager->ApplyEquipmentEffects(Wearer, {Gear});
			FActiveSkillEffect Again = FActiveSkillEffect::CreateAllFromSkillEffect(TEXT("Gear"), IDENTITY_TEST_DEF_ID, Gear.Effect, 0)[0];
			Again.SourceKey = IDENTITY_TEST_ITEM_A;
			TestTrue(TEXT("The same item refreshes"),
					 EffectManager->ApplyEffect(Wearer, Again) == EEffectApplicationResult::DurationRefreshed);
			TestEqual(TEXT("One copy held"), EffectsOfType(Wearer, ESkillEffectType::SpellDamageBuff).Num(), 1);
		});

		It("adds a stack with the new, lower value from the same caster", [this]()
		{
			if (!EffectManager) return;
			AActor *Holder = Spawn();
			AActor *Caster = Spawn();
			FActiveSkillEffect Strong = Debuff(IDENTITY_TEST_STRONG_VALUE);
			Strong.bCanStack = true;
			FActiveSkillEffect Weak = Debuff(IDENTITY_TEST_WEAK_VALUE);
			Weak.bCanStack = true;
			EffectManager->ApplyEffect(Holder, Strong, Caster, TEXT("Test skill"));
			TestTrue(TEXT("A stack is added"),
					 EffectManager->ApplyEffect(Holder, Weak, Caster, TEXT("Test skill")) == EEffectApplicationResult::StackAdded);
			const TArray<FActiveSkillEffect> Held = EffectsOfType(Holder, ESkillEffectType::DefenseDebuff);
			if (!TestEqual(TEXT("One copy"), Held.Num(), 1)) return;
			TestEqual(TEXT("Two stacks"), Held[0].CurrentStacks, 2);
			TestEqual(TEXT("Per-stack value is the new one"), Held[0].EffectValue, IDENTITY_TEST_WEAK_VALUE);
		});

		It("adds nothing from a second item when the effect doesn't stack", [this]()
		{
			if (!EffectManager) return;
			AActor *Wearer = Spawn();
			EffectManager->ApplyEquipmentEffects(Wearer, {GearEffect(IDENTITY_TEST_ITEM_A, false, /*bDoesNotStack*/ true),
														  GearEffect(IDENTITY_TEST_ITEM_B, false, /*bDoesNotStack*/ true)});
			TestEqual(TEXT("One copy held"), EffectsOfType(Wearer, ESkillEffectType::SpellDamageBuff).Num(), 1);
		});

		It("adds a copy per caster of the same skill effect", [this]()
		{
			if (!EffectManager) return;
			AActor *Holder = Spawn();
			EffectManager->ApplyEffect(Holder, Debuff(IDENTITY_TEST_STRONG_VALUE), Spawn(), TEXT("Test skill"));
			EffectManager->ApplyEffect(Holder, Debuff(IDENTITY_TEST_STRONG_VALUE), Spawn(), TEXT("Test skill"));
			TestEqual(TEXT("Two casters, two copies"), EffectsOfType(Holder, ESkillEffectType::DefenseDebuff).Num(), 2);
		});

		It("replaces the value when the same caster re-applies, even a weaker one (Q-044 A)", [this]()
		{
			if (!EffectManager) return;
			AActor *Holder = Spawn();
			AActor *Caster = Spawn();
			EffectManager->ApplyEffect(Holder, Debuff(IDENTITY_TEST_STRONG_VALUE), Caster, TEXT("Test skill"));
			TestTrue(TEXT("Re-apply refreshes"),
					 EffectManager->ApplyEffect(Holder, Debuff(IDENTITY_TEST_WEAK_VALUE), Caster, TEXT("Test skill")) ==
						 EEffectApplicationResult::DurationRefreshed);
			const TArray<FActiveSkillEffect> Held = EffectsOfType(Holder, ESkillEffectType::DefenseDebuff);
			if (!TestEqual(TEXT("One copy"), Held.Num(), 1)) return;
			TestEqual(TEXT("It holds the new value"), Held[0].EffectValue, IDENTITY_TEST_WEAK_VALUE);
		});
	});
}

#endif
