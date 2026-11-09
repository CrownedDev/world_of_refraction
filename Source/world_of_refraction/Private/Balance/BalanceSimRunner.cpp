// BalanceSimRunner.cpp
// B-292: see BalanceSimRunner.h.

#include "Balance/BalanceSimRunner.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/IConsoleManager.h"
#include "Balance/BalanceArchetypes.h"
#include "Balance/BalanceSimRecorder.h"
#include "Combat/CombatControl.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Encounter/EnemyZone.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SkillCastEntry.h"
#include "Skills/Definitions/SpellData.h"
#include "Tests/WoRTestWorld.h"
#include "Trial/TrialGate.h"
#include "WoRLog.h"

namespace
{
	const TCHAR *SIM_CVAR_INSTANT_IMPACTS = TEXT("wor.Combat.InstantImpacts");
	const TCHAR *SIM_CVAR_THINK_DELAY = TEXT("wor.AI.ThinkDelayScale");
	const TCHAR *SIM_CVAR_BATTLE_SEED = TEXT("wor.Battle.Seed");

	constexpr int32 SIM_ATTACK_HIT_COUNT = 1;
	/** B-346: the sim party's AI seat setting; Hard = the fixed level the party played at before B-346. */
	constexpr EAIDifficulty SIM_PARTY_AI_DIFFICULTY = EAIDifficulty::Hard;
	/** The adapter's gem (B-293): a Garnet at the weapon's tier, intact, so the Resonator's spell has an element. */
	constexpr ECrystalType SIM_ADAPTER_GEM_TYPE = ECrystalType::Garnet;
	constexpr int32 SIM_ADAPTER_GEM_DURABILITY = 5;
	/** Frames to let the end-of-fight timers settle before the next fight's actors spawn. */
	constexpr int32 SIM_SETTLE_FRAMES = 2;

	IConsoleVariable *SimCVar(const TCHAR *Name)
	{
		return IConsoleManager::Get().FindConsoleVariable(Name);
	}

	/** Writes at the variable's own priority (at least code): a value typed in the editor console doesn't outrank
	 *  the runner, and the runner doesn't lift it above code, so specs that set it afterwards still can. */
	EConsoleVariableFlags SimPriority(const IConsoleVariable *CVar)
	{
		const uint32 Current = static_cast<uint32>(CVar->GetFlags()) & ECVF_SetByMask;
		return static_cast<EConsoleVariableFlags>(FMath::Max<uint32>(Current, ECVF_SetByCode));
	}

	EBalanceFightOutcome OutcomeOf(ECombatState State)
	{
		switch (State)
		{
		case ECombatState::Victory:
			return EBalanceFightOutcome::PartyWon;
		case ECombatState::Defeat:
			return EBalanceFightOutcome::EnemiesWon;
		case ECombatState::Draw:
			return EBalanceFightOutcome::Draw;
		default:
			return EBalanceFightOutcome::Stalled;
		}
	}
}

FBalanceSimRunner::FBalanceSimRunner() = default;

FBalanceSimRunner::~FBalanceSimRunner()
{
	End();
}

bool FBalanceSimRunner::Begin()
{
	if (bRunning)
	{
		return true;
	}
	TestWorld = MakeUnique<FWoRTestWorld>();
	if (!TestWorld->Create())
	{
		TestWorld.Reset();
		return false;
	}
	Orchestrator = TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>();
	// Every combatant is AI: when a party member falls, D-045 would hand the rest to a player who isn't there.
	if (UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>())
	{
		PartySession->RestoreLocalParty(FText::FromString(TEXT("Balance sim")), /*bLetAIFinish=*/true, {FPartyMember()});
	}
	Recorder.Reset(NewObject<UBalanceSimRecorder>());
	Recorder->Listen(TestWorld->GetSubsystem<UActionExecutor>(), Orchestrator, TestWorld->GetSubsystem<UTurnManager>(),
					 TestWorld->GetSubsystem<UStatusBuildupManager>());

	if (IConsoleVariable *CVar = SimCVar(SIM_CVAR_INSTANT_IMPACTS))
	{
		SavedInstantImpacts = CVar->GetInt();
		CVar->Set(1, SimPriority(CVar));
	}
	if (IConsoleVariable *CVar = SimCVar(SIM_CVAR_THINK_DELAY))
	{
		SavedThinkDelayScale = CVar->GetFloat();
		CVar->Set(0.0f, SimPriority(CVar));
	}
	if (IConsoleVariable *CVar = SimCVar(SIM_CVAR_BATTLE_SEED))
	{
		SavedBattleSeed = CVar->GetInt();
	}
	// The executor and AI log every action; thousands of fights would flood the log (BalanceSimulator.md §1).
	SavedLogTemp = LogTemp.GetVerbosity();
	SavedLogCombat = LogWoRCombat.GetVerbosity();
	SavedLogEffects = LogWoREffects.GetVerbosity();
	SavedLogAI = LogWoRAI.GetVerbosity();
	LogTemp.SetVerbosity(ELogVerbosity::Warning);
	LogWoRCombat.SetVerbosity(ELogVerbosity::Warning);
	LogWoREffects.SetVerbosity(ELogVerbosity::Warning);
	LogWoRAI.SetVerbosity(ELogVerbosity::Warning);
	bRunning = true;
	return true;
}

void FBalanceSimRunner::End()
{
	if (!bRunning)
	{
		return;
	}
	if (Orchestrator && Orchestrator->GetCombatState() == ECombatState::InProgress)
	{
		Orchestrator->ForceEndCombat();
	}
	ClearFight();
	if (IConsoleVariable *CVar = SimCVar(SIM_CVAR_INSTANT_IMPACTS))
	{
		CVar->Set(SavedInstantImpacts, SimPriority(CVar));
	}
	if (IConsoleVariable *CVar = SimCVar(SIM_CVAR_THINK_DELAY))
	{
		CVar->Set(SavedThinkDelayScale, SimPriority(CVar));
	}
	if (IConsoleVariable *CVar = SimCVar(SIM_CVAR_BATTLE_SEED))
	{
		CVar->Set(SavedBattleSeed, SimPriority(CVar));
	}
	LogTemp.SetVerbosity(SavedLogTemp);
	LogWoRCombat.SetVerbosity(SavedLogCombat);
	LogWoREffects.SetVerbosity(SavedLogEffects);
	LogWoRAI.SetVerbosity(SavedLogAI);
	Recorder.Reset();
	Orchestrator = nullptr;
	TestWorld.Reset();
	bRunning = false;
}

FBalanceFightRecord FBalanceSimRunner::RunFight(const FBalanceMatchup &Matchup, int32 Seed)
{
	FBalanceFightRecord Failed;
	Failed.MatchupId = Matchup.Id;
	Failed.Seed = Seed;
	const FBalanceArchetype *EnemyArchetype = BalanceArchetypes::Find(Matchup.Enemy);
	if (!bRunning || !Orchestrator || !EnemyArchetype)
	{
		UE_LOG(LogWoR, Error, TEXT("[BalanceSim] %s: not running or unknown enemy archetype %s"), *Matchup.Id.ToString(),
			   *Matchup.Enemy.ToString());
		return Failed;
	}

	TArray<AActor *> Party;
	for (const FName Id : Matchup.Party)
	{
		const FBalanceArchetype *Archetype = BalanceArchetypes::Find(Id);
		if (AActor *Member = Archetype ? SpawnArchetype(*Archetype) : nullptr)
		{
			Party.Add(Member);
		}
	}
	TArray<AActor *> Enemies;
	for (int32 Index = 0; Index < Matchup.EnemyCount; ++Index)
	{
		if (AActor *Enemy = SpawnArchetype(*EnemyArchetype))
		{
			ApplyEnemyStrength(Enemy, Matchup);
			Enemies.Add(Enemy);
		}
	}
	if (Party.Num() != Matchup.Party.Num() || Enemies.Num() != Matchup.EnemyCount)
	{
		UE_LOG(LogWoR, Error, TEXT("[BalanceSim] %s: combatants failed to spawn"), *Matchup.Id.ToString());
		ClearFight();
		return Failed;
	}

	if (IConsoleVariable *CVar = SimCVar(SIM_CVAR_BATTLE_SEED))
	{
		CVar->Set(Seed, SimPriority(CVar)); // StartCombat's BeginBattle seeds the stream from it
	}
	Recorder->Begin(Matchup.Id, Seed, Party);
	Orchestrator->SetEncounterRank(Matchup.Rank);
	// B-346: the sim's party plays at the AI seat setting; Hard keeps the pre-B-346 numbers
	// (the old fixed ALLY_AI_DIFFICULTY), so the baselines stay comparable.
	for (AActor *Member : Party)
	{
		CombatControl::SetAISeatDifficulty(Member, SIM_PARTY_AI_DIFFICULTY);
	}
	Orchestrator->StartCombat(Party, Enemies, Matchup.Difficulty);

	int32 Ticks = 0;
	while (!Recorder->FinalState.IsSet() && Orchestrator->GetCombatState() == ECombatState::InProgress && Ticks < FIGHT_TICK_CAP)
	{
		TestWorld->Tick();
		++Ticks;
	}
	FBalanceFightRecord Record = Recorder->Record;
	Record.Ticks = Ticks;
	Record.Outcome = OutcomeOf(Recorder->FinalState.Get(Orchestrator->GetCombatState()));
	if (Record.Outcome == EBalanceFightOutcome::Stalled && Orchestrator->GetCombatState() == ECombatState::InProgress)
	{
		UE_LOG(LogWoR, Error, TEXT("[BalanceSim] stalled: %s (replay with wor.Battle.Seed %d)"), *Record.ToString(), Seed);
		Orchestrator->ForceEndCombat();
	}
	TestWorld->Tick(SIM_SETTLE_FRAMES);
	ClearFight();
	return Record;
}

TArray<FBalanceFightRecord> FBalanceSimRunner::RunMatchup(const FBalanceMatchup &Matchup, int32 Fights, int32 BaseSeed)
{
	TArray<FBalanceFightRecord> Records;
	for (int32 Index = 0; Index < Fights; ++Index)
	{
		Records.Add(RunFight(Matchup, BaseSeed + Index));
	}
	UE_LOG(LogWoR, Display, TEXT("[BalanceSim] %s"), *FBalanceAggregate::From(Records).ToString());
	return Records;
}

AActor *FBalanceSimRunner::SpawnArchetype(const FBalanceArchetype &Archetype)
{
	UCharacterData *Data = FWoRTestWorld::NewCharacterData();
	Data->Name = Archetype.Id.ToString();
	Data->CharacterClass = Archetype.Class;
	Data->WorldMindLevel = Archetype.Mind;
	Data->WorldBodyLevel = Archetype.Body;
	Data->WorldSpiritLevel = Archetype.Spirit;
	Data->RawDamage = Archetype.RawDamage;
	Data->Defense = Archetype.Defense;
	Data->MaxHealth = Archetype.MaxHealth;
	Data->Luck = Archetype.Luck;
	Data->SpellDamage = Archetype.SpellDamage;
	Data->EnemyTier = Archetype.WeaponTier; // B-224b: read only for enemies (rank health), so a party member is unaffected
	// A Caster casts its own element (BalanceSimulator.md §4: "Caster (Fire)"); left Generic, combat start clears
	// its innate spell as not castable.
	if (Archetype.Class == ECharacterClass::Caster)
	{
		Data->InnateElement = Archetype.SpellElement;
	}

	// Montage-less skills: with the instant-impacts seam each hit lands at once, defended.
	UAbilityData *Attack = NewObject<UAbilityData>(GetTransientPackage());
	Attack->Name = FString::Printf(TEXT("%s attack"), *Data->Name);
	Attack->bIsAttack = true;
	Attack->BaseDamage = Archetype.AttackBase;
	Attack->HitCount = SIM_ATTACK_HIT_COUNT;
	Attack->Tier = Archetype.WeaponTier;
	Attack->PhysicalDamageType = EPhysicalDamageType::Slash;
	Attack->StatusBuildup = Archetype.StatusBuildup;

	UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
	Weapon->Tier = Archetype.WeaponTier;
	Weapon->Power = Archetype.WeaponPower;
	FPillarScaling Scaling;
	Scaling.Pillar = Archetype.ScalingPillar;
	Scaling.Tier = Archetype.ScalingTier;
	Weapon->StatScaling.Add(Scaling);
	switch (Archetype.ScalingPillar)
	{
	case EWorldPillar::Mind:
		Weapon->Requirements.RequiredWorldMind = Archetype.RequiredLevel;
		break;
	case EWorldPillar::Body:
		Weapon->Requirements.RequiredWorldBody = Archetype.RequiredLevel;
		break;
	case EWorldPillar::Spirit:
		Weapon->Requirements.RequiredWorldSpirit = Archetype.RequiredLevel;
		break;
	}
	Weapon->WeaponAttack = Attack;
	FightAssets.Emplace(Data);
	FightAssets.Emplace(Attack);
	FightAssets.Emplace(Weapon);

	ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, Weapon, /*bAIControlled=*/true);
	AActor *Actor = Loadout ? Loadout->GetOwner() : nullptr;
	if (Actor)
	{
		FightActors.Add(Actor);
		// B-293: the ability system, turn tracker and effect mirror an ACombatCharacter has; without them every GAS
		// read falls back to the enum path and the parity check would compare the enum path with itself.
		FWoRTestWorld::AddEffectMirror(Actor);
	}
	UInventoryComponent *Inventory = Actor ? Actor->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Inventory || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
	{
		return Actor;
	}
	// The weapon's instance tier is its archetype's (B-293d / B-325): left at F, an S weapon's Power is rescaled to
	// F (WoRWeaponPower::GetScaledPower, 192 -> 40), the tier gap reads F, and an enemy's Combat Tier drops.
	Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].PrimaryWeapon.WeaponEntry.Tier = Archetype.WeaponTier;
	// Owned, or combat start clears it from the loadout.
	FWeaponInventoryEntry Owned;
	Owned.Weapon = Weapon;
	Owned.Tier = Archetype.WeaponTier;
	Inventory->Weapons.Add(Owned);
	if (Archetype.AbilityBase > 0)
	{
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->Name = FString::Printf(TEXT("%s ability"), *Data->Name);
		Ability->BaseDamage = Archetype.AbilityBase;
		Ability->HitCount = SIM_ATTACK_HIT_COUNT;
		Ability->Tier = Archetype.WeaponTier;
		Ability->PhysicalDamageType = EPhysicalDamageType::Slash;
		Ability->StatusBuildup = Archetype.StatusBuildup;
		Ability->RequiredWeaponType = Weapon->WeaponType;
		FightAssets.Emplace(Ability);
		Inventory->LearnAbility(Ability);
		Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].PrimaryWeapon.AssignedAbilities.Add(Ability);
		Loadout->NotifyActiveWeaponChanged();
	}
	if (Archetype.SpellBase > 0)
	{
		// B-293: one Instant spell (montage-less: B-291 dispatches CastArray[0] at release). A Caster has it innate;
		// anyone else casts it through an adapter holding an intact gem of the spell's element.
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Name = FString::Printf(TEXT("%s spell"), *Data->Name);
		Spell->BaseDamage = Archetype.SpellBase;
		Spell->Tier = Archetype.WeaponTier;
		Spell->Element = Archetype.SpellElement;
		Spell->DeliveryType = ESpellDeliveryType::Instant;
		Spell->StatusBuildup = Archetype.StatusBuildup;
		FSkillCastEntry &Cast = Spell->CastArray.AddDefaulted_GetRef();
		Cast.DeliveryType = ESpellDeliveryType::Instant;
		FightAssets.Emplace(Spell);
		Inventory->LearnSpell(Spell);
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		if (Archetype.Class == ECharacterClass::Caster)
		{
			Active.InnateSpells.Add(Spell);
		}
		else
		{
			URingData *Adapter = NewObject<URingData>(GetTransientPackage());
			Adapter->Tier = Archetype.WeaponTier;
			FightAssets.Emplace(Adapter);
			FRingInventoryEntry OwnedAdapter = FRingInventoryEntry::CreateFromRing(Adapter);
			OwnedAdapter.AttachedItem.Kind = EAttachedItemKind::Crystal;
			OwnedAdapter.AttachedItem.Crystal = FCrystalAttachment(FCrystalId(SIM_ADAPTER_GEM_TYPE, Archetype.WeaponTier), SIM_ADAPTER_GEM_DURABILITY);
			OwnedAdapter.AssignedSpells.Add(Spell);
			Inventory->Rings.Add(OwnedAdapter);
			FRingLoadoutEntry Slot;
			Slot.RingEntry = OwnedAdapter;
			Active.RingLoadout.Add(Slot);
			Active.ActiveRingIndex = 0;
		}
		Loadout->NotifyActiveWeaponChanged();
	}
	return Actor;
}

void FBalanceSimRunner::ApplyEnemyStrength(AActor *Enemy, const FBalanceMatchup &Matchup)
{
	// The same multipliers as BattleGameMode's trial fights: the door's difficulty (damage only, B-293c), then rank x
	// tier power (B-224 / B-224b). D-120: the zone scales nothing (ZoneTier stays in the matchup for its placement role).
	const float Strength = TrialGate::GetEnemyStrengthMultiplier(Matchup.TrialDifficulty);
	const float Health = TrialGate::GetEnemyHealthMultiplier(Matchup.TrialDifficulty);
	TrialGate::ApplyEnemyStrength(Enemy, Health, Strength);
	TrialGate::ApplyEnemyRankHealth(Enemy, Health, Matchup.Rank);
}

void FBalanceSimRunner::ClearFight()
{
	for (AActor *Actor : FightActors)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	FightActors.Reset();
	FightAssets.Reset();
}

#endif // WITH_DEV_AUTOMATION_TESTS
