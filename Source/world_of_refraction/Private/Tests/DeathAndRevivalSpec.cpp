// DeathAndRevivalSpec.cpp
// B-580 (D-253): death takes buffs and debuffs and empties the status bar, and leaves what is part of the
// character (permanent gear effects). A dead ally can be targeted by a Sapphire only, and is revived through a
// submitted action at 30% of max HP, raised by the Healing Boost it held when it died. Sapphire's death-save is
// raised the same way and then takes the holder's effects; every other Last Stand is unchanged.
// The uses go through UActionExecutor::ExecuteAction (validation, then the item), not the item executor directly.
// Unproven here: the fight menu's target list, the card redrawn on death, co-op (docs/PIE/B-580.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "WoRTestWorld.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/TurnManager.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "GAS/WoRAbsorb.h"
#include "GAS/WoRTurnEffectTracker.h"
#include "Inventory/ItemConstants.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"

namespace
{
	constexpr int32 DEATH_TEST_TURNS = 3;
	constexpr float DEATH_TEST_VALUE = 20.0f;
	constexpr float DEATH_TEST_BUILDUP = 10.0f;
	constexpr int32 DEATH_TEST_HEAL = 20;
	constexpr int32 DEATH_TEST_OTHER_SAVE_CHARGES = 2;
	// Effect IDs: each a different effect.
	constexpr int32 DEATH_TEST_BUFF_ID = 9601;
	constexpr int32 DEATH_TEST_DEBUFF_ID = 9602;
	constexpr int32 DEATH_TEST_GEAR_ID = 9603;
	constexpr int32 DEATH_TEST_GEAR_TIMED_ID = 9604;
	constexpr int32 DEATH_TEST_SPELL_PERMANENT_ID = 9605;
	constexpr int32 DEATH_TEST_OTHER_SAVE_ID = 9606;
	constexpr int32 DEATH_TEST_DOT_ID = 9607;
	constexpr int32 DEATH_TEST_ON_HIT_ID = 9608;
	constexpr int32 DEATH_TEST_SHIELD_ID = 9609;
	constexpr int32 DEATH_TEST_GEAR_SHIELD_ID = 9610;
	constexpr int32 DEATH_TEST_WAITING_ID = 9611;
	/** Times the target's max HP: a hit or a tick that kills whatever reduces it. */
	constexpr int32 DEATH_TEST_OVERKILL = 10;
	const FCrystalId DEATH_TEST_SAPPHIRE(ECrystalType::Sapphire, EItemTier::F_Tier);
	const FCrystalId DEATH_TEST_HEALING_STONE(ECrystalType::HealingStone, EItemTier::F_Tier);
	const FCrystalId DEATH_TEST_GARNET(ECrystalType::Garnet, EItemTier::F_Tier);
	const FCrystalId DEATH_TEST_CITRINE(ECrystalType::Citrine, EItemTier::F_Tier);
}

BEGIN_DEFINE_SPEC(FDeathAndRevivalSpec, "WoR.Combat.DeathAndRevival", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *Effects = nullptr;
	UActionExecutor *Executor = nullptr;
	UTurnManager *TurnManager = nullptr;
	AActor *Hero = nullptr;
	AActor *Ally = nullptr;
	AActor *Foe = nullptr;
	UCharacterDataComponent *AllyChar = nullptr;
	UCharacterDataComponent *FoeChar = nullptr;

	static FAction ItemOn(const FCrystalId &Item, AActor *Target)
	{
		FAction Action;
		Action.ActionType = EActionType::Item;
		Action.ItemData = Item;
		Action.Targets.Add(Target);
		return Action;
	}

	bool Has(AActor *Actor, int32 EffectID) const
	{
		return Effects->GetActiveEffects(Actor).ContainsByPredicate([EffectID](const FActiveSkillEffect &Effect) { return Effect.EffectID == EffectID; });
	}

	void Apply(AActor *Target, int32 EffectID, ESkillEffectType Type, bool bFromGear, bool bPermanent, AActor *Source) const
	{
		FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(FString::Printf(TEXT("Death test %d"), EffectID), EffectID, Type, DEATH_TEST_VALUE, DEATH_TEST_TURNS);
		Effect.bFromGear = bFromGear;
		Effect.bPermanent = bPermanent;
		Effects->ApplyEffect(Target, Effect, Source);
	}

	void Kill(UCharacterDataComponent *Char) const { Char->ServerTakeDamage(Char->MaxHP); }
END_DEFINE_SPEC(FDeathAndRevivalSpec)

void FDeathAndRevivalSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		Executor = TestWorld->GetSubsystem<UActionExecutor>();
		TurnManager = TestWorld->GetSubsystem<UTurnManager>();
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Ally = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AllyChar = Ally ? Ally->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		FoeChar = Foe ? Foe->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("Effects"), Effects) || !TestNotNull(TEXT("Executor"), Executor) || !TestNotNull(TEXT("TurnManager"), TurnManager) ||
			!TestNotNull(TEXT("Hero"), Hero) || !TestNotNull(TEXT("AllyChar"), AllyChar) || !TestNotNull(TEXT("FoeChar"), FoeChar))
		{
			Effects = nullptr; // each case returns early
			return;
		}
		TurnManager->InitializeCombat({Hero, Ally}, {Foe});
	});

	AfterEach([this]()
	{
		if (TurnManager && TurnManager->IsCombatActive())
		{
			TurnManager->EndCombat();
		}
		TestWorld.Reset();
	});

	It("takes buffs, debuffs and the status bar at death, and leaves permanent gear effects", [this]()
	{
		if (!Effects) return;
		Apply(Ally, DEATH_TEST_BUFF_ID, ESkillEffectType::DefenseBuff, /*bFromGear=*/false, /*bPermanent=*/false, Hero);
		Apply(Ally, DEATH_TEST_DEBUFF_ID, ESkillEffectType::DefenseDebuff, false, false, Foe);
		Apply(Ally, DEATH_TEST_SPELL_PERMANENT_ID, ESkillEffectType::DamageBuff, false, /*bPermanent=*/true, Hero); // a buff that lasts until spent
		Apply(Ally, DEATH_TEST_GEAR_ID, ESkillEffectType::DamageBuff, /*bFromGear=*/true, /*bPermanent=*/true, nullptr);
		Apply(Ally, DEATH_TEST_GEAR_TIMED_ID, ESkillEffectType::DamageBuff, /*bFromGear=*/true, /*bPermanent=*/false, nullptr); // a timed buff its gear gave
		UStatusBuildupManager *Bars = TestWorld->GetSubsystem<UStatusBuildupManager>();
		if (!TestNotNull(TEXT("Bars"), Bars)) return;
		Bars->AddStatusBuildup(Foe, Ally, DEATH_TEST_BUILDUP, ESpellElement::Fire, EPhysicalDamageType::None);
		if (!TestTrue(TEXT("the bar has buildup"), Bars->GetStatusBarBuildup(Ally) > 0.0f)) return;
		TestTrue(TEXT("all five effects are on"), Has(Ally, DEATH_TEST_BUFF_ID) && Has(Ally, DEATH_TEST_DEBUFF_ID) && Has(Ally, DEATH_TEST_SPELL_PERMANENT_ID) &&
													  Has(Ally, DEATH_TEST_GEAR_ID) && Has(Ally, DEATH_TEST_GEAR_TIMED_ID));

		Kill(AllyChar);
		TestFalse(TEXT("dead"), AllyChar->bIsAlive);
		TestFalse(TEXT("the buff is gone"), Has(Ally, DEATH_TEST_BUFF_ID));
		TestFalse(TEXT("the debuff is gone"), Has(Ally, DEATH_TEST_DEBUFF_ID));
		TestFalse(TEXT("a permanent buff from a skill is gone"), Has(Ally, DEATH_TEST_SPELL_PERMANENT_ID));
		TestFalse(TEXT("a timed buff its gear gave is gone"), Has(Ally, DEATH_TEST_GEAR_TIMED_ID));
		TestTrue(TEXT("the permanent gear effect stays"), Has(Ally, DEATH_TEST_GEAR_ID));
		TestEqual(TEXT("the status bar is empty"), Bars->GetStatusBarBuildup(Ally), 0.0f);
	});

	It("lets a Sapphire, and nothing else, target a dead ally; a dead enemy is refused", [this]()
	{
		if (!Effects) return;
		Kill(AllyChar);
		Kill(FoeChar);
		if (!TestFalse(TEXT("the ally is dead"), AllyChar->bIsAlive) || !TestFalse(TEXT("the enemy is dead"), FoeChar->bIsAlive)) return;

		TestTrue(TEXT("a Sapphire on the dead ally is a valid action"), Executor->ValidateAction(Hero, ItemOn(DEATH_TEST_SAPPHIRE, Ally)).bIsValid);
		TestFalse(TEXT("a Sapphire on a dead enemy is not"), Executor->ValidateAction(Hero, ItemOn(DEATH_TEST_SAPPHIRE, Foe)).bIsValid);
		for (const FCrystalId &Other : {DEATH_TEST_HEALING_STONE, DEATH_TEST_GARNET, DEATH_TEST_CITRINE})
		{
			TestFalse(TEXT("another item on the dead ally is not valid"), Executor->ValidateAction(Hero, ItemOn(Other, Ally)).bIsValid);
			const FActionResult Refused = Executor->ExecuteItem(Hero, Other, {Ally});
			TestFalse(TEXT("and its use is refused"), Refused.bSuccess);
			TestEqual(TEXT("because the target is dead"), Refused.ErrorMessage, FString(TEXT("Target is dead")));
		}
		FAction Skill; // an attack or an ability: one action type since the merge
		Skill.ActionType = EActionType::Ability;
		Skill.SkillData = NewObject<UAbilityData>(GetTransientPackage());
		Skill.Targets.Add(Ally);
		const FActionValidationResult SkillOnDead = Executor->ValidateAction(Hero, Skill);
		TestFalse(TEXT("nor is a skill on the dead"), SkillOnDead.bIsValid);
		TestEqual(TEXT("and the reason is the dead target"), SkillOnDead.ErrorMessage, FString(TEXT("Target is dead")));
		TestFalse(TEXT("the refused uses revived nobody"), AllyChar->bIsAlive);
	});

	It("revives a dead ally through a submitted Sapphire at 30% of max HP", [this]()
	{
		if (!Effects) return;
		Kill(AllyChar);
		const FActionResult Used = Executor->ExecuteAction(Hero, ItemOn(DEATH_TEST_SAPPHIRE, Ally));
		TestTrue(TEXT("the action lands"), Used.bSuccess);
		TestTrue(TEXT("alive again"), AllyChar->bIsAlive);
		TestEqual(TEXT("at 30% of max HP"), AllyChar->CurrentHP, FMath::RoundToInt(AllyChar->MaxHP * ItemConstants::REVIVE_HP_PERCENT));
	});

	It("counts the Healing Boost held at death in the revive (30% becomes 34.5% with an F stone), then it is gone", [this]()
	{
		if (!Effects) return;
		const float BoostPercent = CrystalEffectTable::GetHealingBoostPercent(DEATH_TEST_HEALING_STONE);
		TestTrue(TEXT("the stone lands"), Executor->ExecuteAction(Hero, ItemOn(DEATH_TEST_HEALING_STONE, Ally)).bSuccess);
		Kill(AllyChar);
		TestEqual(TEXT("death took the boost itself"), Effects->GetTotalStatModifier(Ally, ESkillEffectType::HealingReceivedBuff), 0.0f);
		TestEqual(TEXT("its number is remembered"), AllyChar->GetHealingBoostAtDeath(), BoostPercent);

		TestTrue(TEXT("the revive lands"), Executor->ExecuteAction(Hero, ItemOn(DEATH_TEST_SAPPHIRE, Ally)).bSuccess);
		const int32 Boosted = FMath::RoundToInt(AllyChar->MaxHP * ItemConstants::REVIVE_HP_PERCENT * (1.0f + BoostPercent / 100.0f));
		TestEqual(TEXT("revived at 34.5% of max HP"), AllyChar->CurrentHP, Boosted);
		TestTrue(TEXT("more than the plain 30%"), AllyChar->CurrentHP > FMath::RoundToInt(AllyChar->MaxHP * ItemConstants::REVIVE_HP_PERCENT));
		TestEqual(TEXT("the remembered number is used up"), AllyChar->GetHealingBoostAtDeath(), 0.0f);
		TestEqual(TEXT("the revived have no boost: a heal is plain"), Effects->HealActor(Ally, DEATH_TEST_HEAL, Hero), DEATH_TEST_HEAL);

		Kill(AllyChar);
		TestTrue(TEXT("a second revive lands"), Executor->ExecuteAction(Hero, ItemOn(DEATH_TEST_SAPPHIRE, Ally)).bSuccess);
		TestEqual(TEXT("with no boost held at that death, 30% again"), AllyChar->CurrentHP, FMath::RoundToInt(AllyChar->MaxHP * ItemConstants::REVIVE_HP_PERCENT));
	});

	It("raises Sapphire's death-save by the Healing Boost held, then takes the holder's buffs and debuffs", [this]()
	{
		if (!Effects) return;
		const float BoostPercent = CrystalEffectTable::GetHealingBoostPercent(DEATH_TEST_HEALING_STONE);
		TestTrue(TEXT("the ward lands on the living ally"), Executor->ExecuteAction(Hero, ItemOn(DEATH_TEST_SAPPHIRE, Ally)).bSuccess);
		TestTrue(TEXT("the stone lands"), Executor->ExecuteAction(Hero, ItemOn(DEATH_TEST_HEALING_STONE, Ally)).bSuccess);
		Apply(Ally, DEATH_TEST_BUFF_ID, ESkillEffectType::DefenseBuff, false, false, Hero);
		Apply(Ally, DEATH_TEST_DEBUFF_ID, ESkillEffectType::DefenseDebuff, false, false, Foe);
		Apply(Ally, DEATH_TEST_GEAR_ID, ESkillEffectType::DamageBuff, /*bFromGear=*/true, /*bPermanent=*/true, nullptr);

		Kill(AllyChar);
		TestTrue(TEXT("saved, not dead"), AllyChar->bIsAlive);
		const int32 Boosted = FMath::RoundToInt(AllyChar->MaxHP * (ItemConstants::LAST_STAND_HP_PERCENT / 100.0f) * (1.0f + BoostPercent / 100.0f));
		TestEqual(TEXT("at 50% of max HP raised by the boost"), AllyChar->CurrentHP, Boosted);
		TestFalse(TEXT("the buff is gone"), Has(Ally, DEATH_TEST_BUFF_ID));
		TestFalse(TEXT("the debuff is gone"), Has(Ally, DEATH_TEST_DEBUFF_ID));
		TestEqual(TEXT("the Healing Boost is gone"), Effects->GetTotalStatModifier(Ally, ESkillEffectType::HealingReceivedBuff), 0.0f);
		TestFalse(TEXT("the ward is used up"), Effects->HasEffectOfType(Ally, ESkillEffectType::LastStand));
		TestTrue(TEXT("the permanent gear effect stays"), Has(Ally, DEATH_TEST_GEAR_ID));
	});

	// ---- Effects that arrive at, or during, a death (the brain's read of the code, B-580 fourth attempt) ----

	It("survives a newly applied DOT whose first tick kills its holder, and stores nothing on the dead", [this]()
	{
		if (!Effects) return;
		// A DOT pays on landing only on its holder's own turn (Q-063), so the holder is whoever's turn it is.
		AActor *Holder = TurnManager->GetCurrentActor();
		UCharacterDataComponent *HolderChar = Holder ? Holder->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("the actor whose turn it is"), HolderChar)) return;
		Apply(Holder, DEATH_TEST_BUFF_ID, ESkillEffectType::DefenseBuff, false, false, Holder);
		const FActiveSkillEffect Lethal = FActiveSkillEffect::CreateDOT(TEXT("Lethal DOT"), DEATH_TEST_DOT_ID, static_cast<float>(HolderChar->MaxHP) * DEATH_TEST_OVERKILL,
																	 DEATH_TEST_TURNS, ESpellElement::Fire);
		Effects->ApplyEffect(Holder, Lethal, Holder); // the first tick fires inside this call and takes the holder's effects
		TestFalse(TEXT("the first tick killed"), HolderChar->bIsAlive);
		TestFalse(TEXT("the DOT is not left on the dead"), Has(Holder, DEATH_TEST_DOT_ID));
		TestFalse(TEXT("nor the buff"), Has(Holder, DEATH_TEST_BUFF_ID));
		TestEqual(TEXT("the dead hold no effect at all"), Effects->GetActiveEffects(Holder).Num(), 0);
	});

	It("survives a newly applied DOT whose first tick triggers Sapphire's save: the save takes the DOT too", [this]()
	{
		if (!Effects) return;
		AActor *Holder = TurnManager->GetCurrentActor();
		UCharacterDataComponent *HolderChar = Holder ? Holder->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("the actor whose turn it is"), HolderChar)) return;
		TestTrue(TEXT("the ward lands"), Executor->ExecuteAction(Holder, ItemOn(DEATH_TEST_SAPPHIRE, Holder)).bSuccess);
		if (!TestTrue(TEXT("the ward is on"), Effects->HasEffectOfType(Holder, ESkillEffectType::LastStand))) return;
		const FActiveSkillEffect Lethal = FActiveSkillEffect::CreateDOT(TEXT("Lethal DOT"), DEATH_TEST_DOT_ID, static_cast<float>(HolderChar->MaxHP) * DEATH_TEST_OVERKILL,
																	 DEATH_TEST_TURNS, ESpellElement::Fire);
		Effects->ApplyEffect(Holder, Lethal, Holder);
		TestTrue(TEXT("saved"), HolderChar->bIsAlive);
		TestEqual(TEXT("at 50% of max HP"), HolderChar->CurrentHP, FMath::RoundToInt(HolderChar->MaxHP * (ItemConstants::LAST_STAND_HP_PERCENT / 100.0f)));
		TestFalse(TEXT("the DOT went with the rest"), Has(Holder, DEATH_TEST_DOT_ID));
		TestFalse(TEXT("the ward is used up"), Effects->HasEffectOfType(Holder, ESkillEffectType::LastStand));
	});

	It("puts nothing back on the dead: a killing hit's on-hit DOT, its buildup, and a debuff applied after", [this]()
	{
		if (!Effects) return;
		UStatusBuildupManager *Bars = TestWorld->GetSubsystem<UStatusBuildupManager>();
		if (!TestNotNull(TEXT("Bars"), Bars)) return;
		// The attacker burns what it hits: applied to the target after the hit's damage.
		Effects->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Burns on hit"), DEATH_TEST_ON_HIT_ID, ESkillEffectType::ApplyBurnToTarget, DEATH_TEST_VALUE,
																 DEATH_TEST_TURNS), Hero);
		FActionHitInput Hit;
		Hit.Attacker = Hero;
		Hit.Target = Foe;
		Hit.ActionType = EActionType::None;
		Hit.BaseDamage = FoeChar->MaxHP * DEATH_TEST_OVERKILL;
		Hit.bCanCrit = false;
		Hit.Element = ESpellElement::Fire;
		Hit.BaseStatusBuildup = FMath::RoundToInt(DEATH_TEST_BUILDUP);
		const FCombatHitResult Result = Executor->ApplyHit(Hit);
		if (!TestTrue(TEXT("the hit killed"), Result.bTargetDied)) return;
		TestEqual(TEXT("the killing hit's on-hit DOT is not on the dead"), Effects->GetActiveEffects(Foe).Num(), 0);
		TestEqual(TEXT("its buildup did not fill the emptied bar"), Bars->GetStatusBarBuildup(Foe), 0.0f);

		Apply(Foe, DEATH_TEST_DEBUFF_ID, ESkillEffectType::DefenseDebuff, false, false, Hero);
		TestFalse(TEXT("a debuff applied after the death is refused"), Has(Foe, DEATH_TEST_DEBUFF_ID));
		TestFalse(TEXT("so is more buildup"), Bars->AddStatusBuildup(Hero, Foe, DEATH_TEST_BUILDUP, ESpellElement::Fire, EPhysicalDamageType::None));
		Apply(Foe, DEATH_TEST_GEAR_ID, ESkillEffectType::DamageBuff, /*bFromGear=*/true, /*bPermanent=*/true, nullptr);
		TestTrue(TEXT("a permanent gear effect still applies to the dead (battle prep)"), Has(Foe, DEATH_TEST_GEAR_ID));
	});

	It("takes shields and waiting action changes at death and at Sapphire's save, and leaves a permanent gear shield", [this]()
	{
		if (!Effects) return;
		// As a fighter in the game has: an ability system, the absorber store and the effect mirror (the test world's
		// bare combatant has none, so this case also covers a death with the effects mirrored onto GAS).
		FWoRTestWorld::AddEffectMirror(Ally);
		UWoRTurnEffectTracker *Absorbers = Ally->FindComponentByClass<UWoRTurnEffectTracker>();
		if (!TestNotNull(TEXT("the ally's absorber store"), Absorbers)) return;
		const auto GiveShields = [this]()
		{
			Apply(Ally, DEATH_TEST_SHIELD_ID, ESkillEffectType::NegatePhysical, /*bFromGear=*/false, /*bPermanent=*/false, Hero);
			Apply(Ally, DEATH_TEST_GEAR_SHIELD_ID, ESkillEffectType::NegatePhysical, /*bFromGear=*/true, /*bPermanent=*/true, nullptr);
			FActiveSkillEffect Waiting = FActiveSkillEffect::CreateBuff(TEXT("Action next turn"), DEATH_TEST_WAITING_ID, ESkillEffectType::GainActions, 1.0f, DEATH_TEST_TURNS);
			Effects->ApplyEffect(Ally, Waiting, Hero);
		};
		const auto HasShieldFrom = [Absorbers](int32 EffectID)
		{
			return Absorbers->GetAbsorbs().ContainsByPredicate([EffectID](const FWoRAbsorbCharge &Charge) { return Charge.EffectID == EffectID; });
		};

		GiveShields();
		if (!TestTrue(TEXT("both shields and the waiting change are on"), HasShieldFrom(DEATH_TEST_SHIELD_ID) && HasShieldFrom(DEATH_TEST_GEAR_SHIELD_ID) &&
																		  AllyChar->GetWaitingActionChanges().Num() == 1)) return;
		TestTrue(TEXT("the ward lands"), Executor->ExecuteAction(Hero, ItemOn(DEATH_TEST_SAPPHIRE, Ally)).bSuccess);
		Kill(AllyChar);
		TestTrue(TEXT("saved"), AllyChar->bIsAlive);
		TestFalse(TEXT("the save took the shield a skill gave"), HasShieldFrom(DEATH_TEST_SHIELD_ID));
		TestTrue(TEXT("and left the permanent gear shield"), HasShieldFrom(DEATH_TEST_GEAR_SHIELD_ID));
		TestEqual(TEXT("and took the waiting action change"), AllyChar->GetWaitingActionChanges().Num(), 0);

		GiveShields();
		Kill(AllyChar);
		TestFalse(TEXT("dead this time"), AllyChar->bIsAlive);
		TestFalse(TEXT("death took the shield a skill gave"), HasShieldFrom(DEATH_TEST_SHIELD_ID));
		TestTrue(TEXT("and left the permanent gear shield"), HasShieldFrom(DEATH_TEST_GEAR_SHIELD_ID));
		TestEqual(TEXT("and took the waiting action change"), AllyChar->GetWaitingActionChanges().Num(), 0);

		Apply(Ally, DEATH_TEST_SHIELD_ID, ESkillEffectType::NegatePhysical, false, false, Hero);
		TestFalse(TEXT("a shield is not given to the dead"), HasShieldFrom(DEATH_TEST_SHIELD_ID));
	});

	It("clears at a death from a DOT tick paid through GAS (wor.Effects.UseGAS 1, the effects mirrored)", [this]()
	{
		if (!Effects) return;
		FWoRTestWorld::AddEffectMirror(Ally);
		UWoREffectMirrorComponent::SetReadsFromGAS(true);
		ON_SCOPE_EXIT { UWoREffectMirrorComponent::SetReadsFromGAS(false); };
		Apply(Ally, DEATH_TEST_BUFF_ID, ESkillEffectType::DefenseBuff, false, false, Hero);
		Apply(Ally, DEATH_TEST_GEAR_ID, ESkillEffectType::DamageBuff, /*bFromGear=*/true, /*bPermanent=*/true, nullptr);
		const FActiveSkillEffect Lethal = FActiveSkillEffect::CreateDOT(TEXT("Lethal DOT"), DEATH_TEST_DOT_ID, static_cast<float>(AllyChar->MaxHP) * DEATH_TEST_OVERKILL,
																	 DEATH_TEST_TURNS, ESpellElement::Fire);
		Effects->ApplyEffect(Ally, Lethal, Foe);
		if (AllyChar->bIsAlive)
		{
			Effects->ProcessStartOfTurnEffects(Ally); // its turn start pays the tick (landed off its turn)
		}
		TestFalse(TEXT("the tick killed"), AllyChar->bIsAlive);
		TestFalse(TEXT("the buff is gone"), Has(Ally, DEATH_TEST_BUFF_ID));
		TestFalse(TEXT("the DOT is gone"), Has(Ally, DEATH_TEST_DOT_ID));
		TestTrue(TEXT("the permanent gear effect stays"), Has(Ally, DEATH_TEST_GEAR_ID));
	});

	It("leaves every other Last Stand as it was: no boost, effects and remaining charges kept", [this]()
	{
		if (!Effects) return;
		FActiveSkillEffect OtherSave = FActiveSkillEffect::CreateBuff(TEXT("Another Last Stand"), DEATH_TEST_OTHER_SAVE_ID, ESkillEffectType::LastStand,
																	 ItemConstants::LAST_STAND_HP_PERCENT, DEATH_TEST_TURNS);
		OtherSave.Charges = DEATH_TEST_OTHER_SAVE_CHARGES;
		Effects->ApplyEffect(Ally, OtherSave, Hero);
		TestTrue(TEXT("the stone lands"), Executor->ExecuteAction(Hero, ItemOn(DEATH_TEST_HEALING_STONE, Ally)).bSuccess);
		Apply(Ally, DEATH_TEST_BUFF_ID, ESkillEffectType::DefenseBuff, false, false, Hero);

		Kill(AllyChar);
		TestTrue(TEXT("saved"), AllyChar->bIsAlive);
		TestEqual(TEXT("at a plain 50% of max HP"), AllyChar->CurrentHP, FMath::RoundToInt(AllyChar->MaxHP * (ItemConstants::LAST_STAND_HP_PERCENT / 100.0f)));
		TestTrue(TEXT("the buff is kept"), Has(Ally, DEATH_TEST_BUFF_ID));
		TestTrue(TEXT("the Healing Boost is kept"), Effects->GetTotalStatModifier(Ally, ESkillEffectType::HealingReceivedBuff) > 0.0f);
		TestTrue(TEXT("it has a charge left"), Effects->HasEffectOfType(Ally, ESkillEffectType::LastStand));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
