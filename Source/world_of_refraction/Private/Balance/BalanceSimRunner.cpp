// BalanceSimRunner.cpp
// B-292: see BalanceSimRunner.h.

#include "Balance/BalanceSimRunner.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/IConsoleManager.h"
#include "Balance/BalanceArchetypes.h"
#include "Balance/BalanceSimRecorder.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/CombatConstants.h"
#include "Combat/Encounter/EnemyZone.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Tests/WoRTestWorld.h"
#include "WoRLog.h"

namespace
{
	const TCHAR *SIM_CVAR_INSTANT_IMPACTS = TEXT("wor.Combat.InstantImpacts");
	const TCHAR *SIM_CVAR_THINK_DELAY = TEXT("wor.AI.ThinkDelayScale");
	const TCHAR *SIM_CVAR_BATTLE_SEED = TEXT("wor.Battle.Seed");

	/** Rank HP stand-in until B-224 puts the rank multiplier in the game (BalanceSimulator.md §2 row 6). */
	constexpr float SIM_RANK_HP_NORMAL = 1.0f;
	constexpr float SIM_RANK_HP_MINIBOSS = 3.0f;
	constexpr float SIM_RANK_HP_BOSS = 6.0f;
	constexpr int32 SIM_RANK_HP_EFFECT_ID = 292001;
	constexpr int32 SIM_ATTACK_HIT_COUNT = 1;
	/** Frames to let the end-of-fight timers settle before the next fight's actors spawn. */
	constexpr int32 SIM_SETTLE_FRAMES = 2;

	/** Set at console priority: a value typed in the editor console would otherwise outrank the runner's. */
	IConsoleVariable *SimCVar(const TCHAR *Name)
	{
		return IConsoleManager::Get().FindConsoleVariable(Name);
	}

	float RankHPMultiplier(EEncounterRank Rank)
	{
		switch (Rank)
		{
		case EEncounterRank::MiniBoss:
			return SIM_RANK_HP_MINIBOSS;
		case EEncounterRank::Boss:
			return SIM_RANK_HP_BOSS;
		default:
			return SIM_RANK_HP_NORMAL;
		}
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
		CVar->Set(1, ECVF_SetByConsole);
	}
	if (IConsoleVariable *CVar = SimCVar(SIM_CVAR_THINK_DELAY))
	{
		SavedThinkDelayScale = CVar->GetFloat();
		CVar->Set(0.0f, ECVF_SetByConsole);
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
		CVar->Set(SavedInstantImpacts, ECVF_SetByConsole);
	}
	if (IConsoleVariable *CVar = SimCVar(SIM_CVAR_THINK_DELAY))
	{
		CVar->Set(SavedThinkDelayScale, ECVF_SetByConsole);
	}
	if (IConsoleVariable *CVar = SimCVar(SIM_CVAR_BATTLE_SEED))
	{
		CVar->Set(SavedBattleSeed, ECVF_SetByConsole);
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
		CVar->Set(Seed, ECVF_SetByConsole); // StartCombat's BeginBattle seeds the stream from it
	}
	Recorder->Begin(Matchup.Id, Seed, Party);
	Orchestrator->SetEncounterRank(Matchup.Rank);
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

	// Montage-less skills: with the instant-impacts seam each hit lands at once, defended.
	UAbilityData *Attack = NewObject<UAbilityData>(GetTransientPackage());
	Attack->Name = FString::Printf(TEXT("%s attack"), *Data->Name);
	Attack->bIsAttack = true;
	Attack->BaseDamage = Archetype.AttackBase;
	Attack->HitCount = SIM_ATTACK_HIT_COUNT;
	Attack->Tier = Archetype.WeaponTier;
	Attack->PhysicalDamageType = EPhysicalDamageType::Slash;

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
	}
	UInventoryComponent *Inventory = Actor ? Actor->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Inventory || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
	{
		return Actor;
	}
	// Owned, or combat start clears it from the loadout.
	FWeaponInventoryEntry Owned;
	Owned.Weapon = Weapon;
	Inventory->Weapons.Add(Owned);
	if (Archetype.AbilityBase > 0)
	{
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->Name = FString::Printf(TEXT("%s ability"), *Data->Name);
		Ability->BaseDamage = Archetype.AbilityBase;
		Ability->HitCount = SIM_ATTACK_HIT_COUNT;
		Ability->Tier = Archetype.WeaponTier;
		Ability->PhysicalDamageType = EPhysicalDamageType::Slash;
		Ability->RequiredWeaponType = Weapon->WeaponType;
		FightAssets.Emplace(Ability);
		Inventory->LearnAbility(Ability);
		Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].PrimaryWeapon.AssignedAbilities.Add(Ability);
		Loadout->NotifyActiveWeaponChanged();
	}
	return Actor;
}

void FBalanceSimRunner::ApplyEnemyStrength(AActor *Enemy, const FBalanceMatchup &Matchup)
{
	// Same two multipliers as BattleGameMode's trial fights.
	const float Strength = TrialGate::GetEnemyStrengthMultiplier(Matchup.TrialDifficulty) *
						   AEnemyZone::GetTierStrengthMultiplier(Matchup.ZoneTier);
	TrialGate::ApplyEnemyStrength(Enemy, Strength);

	// TODO(B-224): delete with the game's own rank HP multiplier (a spec case then guards the double apply).
	// TODO(B-224): BalanceSimulator.md also scales by the enemy's tier power; the stand-in uses the rank alone.
	// MaxHP percents add up, so the rank's share is Strength x (Rank - 1): total HP = Strength x Rank.
	const float RankMultiplier = RankHPMultiplier(Matchup.Rank);
	UCharacterDataComponent *CharComp = Enemy->FindComponentByClass<UCharacterDataComponent>();
	USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
	if (!CharComp || !Effects || FMath::IsNearlyEqual(RankMultiplier, SIM_RANK_HP_NORMAL))
	{
		return;
	}
	FActiveSkillEffect RankHP = FActiveSkillEffect::CreatePersistent(TEXT("Balance sim rank HP"), SIM_RANK_HP_EFFECT_ID,
																	  ESkillEffectType::MaxHPBuff,
																	  Strength * (RankMultiplier - 1.0f) * CombatConstants::STAT_PERCENT_DIVISOR);
	RankHP.bFromGear = true; // part of the enemy, not a cleansable buff
	Effects->ApplyEffect(Enemy, RankHP);
	CharComp->RecomputeMaxPools();
	CharComp->ServerSetHP(CharComp->MaxHP);
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
