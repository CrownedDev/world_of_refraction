// BalanceArchetypes.cpp
// B-292: archetypes and presets. Spells are not modelled yet: the Resonator and Caster archetypes fight with their
// weapon and one ability (BalanceSimulator.md §6, fidelity gaps). TODO(B-293): spells through an adapter.

#include "Balance/BalanceArchetypes.h"

namespace
{
	/** BaselineNumbers.md: weapon Power by tier. */
	constexpr int32 SIM_POWER_F = 40;
	constexpr int32 SIM_POWER_C = 88;
	constexpr int32 SIM_POWER_S = 192;
	/** Normal fights are 3 v 2-3 (BalanceSimulator.md §3); the sim uses the upper end. */
	constexpr int32 SIM_NORMAL_ENEMY_COUNT = 3;
	constexpr int32 SIM_PILLAR6_SIDE_SIZE = 3;
	constexpr int32 SIM_ZONE_TIER = 0;

	const FName SimEarlyGenericF(TEXT("P-EarlyGenericF"));
	const FName SimMidResonatorC(TEXT("P-MidResonatorC"));
	const FName SimLateCasterS(TEXT("P-LateCasterS"));
	const FName SimLowLevelSGear(TEXT("P-LowLevelSGear"));
	const FName SimHighLevelFGear(TEXT("P-HighLevelFGear"));
	const FName SimEnemyF(TEXT("E-F"));
	const FName SimEnemyC(TEXT("E-C"));
	const FName SimEnemyS(TEXT("E-S"));

	FBalanceArchetype MakeSimArchetype(FName Id, ECharacterClass Class, EItemTier Tier, int32 Power)
	{
		FBalanceArchetype A;
		A.Id = Id;
		A.Class = Class;
		A.WeaponTier = Tier;
		A.WeaponPower = Power;
		return A;
	}

	TArray<FBalanceArchetype> BuildSimArchetypes()
	{
		TArray<FBalanceArchetype> Out;

		FBalanceArchetype Early = MakeSimArchetype(SimEarlyGenericF, ECharacterClass::Generic, EItemTier::F_Tier, SIM_POWER_F);
		Early.Body = 2;
		Early.RawDamage = 6;
		Early.Defense = 4;
		Early.MaxHealth = 7;
		Early.AbilityBase = 20;
		Out.Add(Early);

		FBalanceArchetype Mid = MakeSimArchetype(SimMidResonatorC, ECharacterClass::Resonator, EItemTier::C_Tier, SIM_POWER_C);
		Mid.Mind = 7; // the table's 8 is past the cap of 7 (B-222)
		Mid.Spirit = 5;
		Mid.Body = 3;
		Mid.RawDamage = 8;
		Mid.Defense = 10;
		Mid.MaxHealth = 15;
		Mid.ScalingPillar = EWorldPillar::Mind;
		Mid.ScalingTier = EScalingTier::B;
		Mid.RequiredLevel = 3;
		Mid.AbilityBase = 30;
		Out.Add(Mid);

		FBalanceArchetype Late = MakeSimArchetype(SimLateCasterS, ECharacterClass::Caster, EItemTier::S_Tier, SIM_POWER_S);
		Late.Mind = 7; // the table's 14 is past the cap
		Late.Spirit = 6;
		Late.SpellDamage = 20;
		Late.MaxHealth = 20;
		Late.Luck = 10;
		Late.ScalingPillar = EWorldPillar::Mind;
		Late.ScalingTier = EScalingTier::S;
		Late.RequiredLevel = 7;
		Late.AbilityBase = 40;
		Out.Add(Late);

		FBalanceArchetype LowS = MakeSimArchetype(SimLowLevelSGear, ECharacterClass::Generic, EItemTier::S_Tier, SIM_POWER_S);
		LowS.Body = 4;
		LowS.Spirit = 2;
		LowS.RawDamage = 6;
		LowS.RequiredLevel = 7; // under-levelled: the requirement factor floors (BaselineNumbers.md)
		LowS.AbilityBase = 20;
		Out.Add(LowS);

		FBalanceArchetype HighF = MakeSimArchetype(SimHighLevelFGear, ECharacterClass::Generic, EItemTier::F_Tier, SIM_POWER_F);
		HighF.Body = 7; // the table's 14 is past the cap
		HighF.RawDamage = 20;
		HighF.AbilityBase = 20;
		Out.Add(HighF);

		FBalanceArchetype EF = MakeSimArchetype(SimEnemyF, ECharacterClass::Generic, EItemTier::F_Tier, SIM_POWER_F);
		EF.Body = 1;
		EF.Defense = 4;
		EF.MaxHealth = 7;
		Out.Add(EF);

		FBalanceArchetype EC = MakeSimArchetype(SimEnemyC, ECharacterClass::Generic, EItemTier::C_Tier, SIM_POWER_C);
		EC.Body = 5;
		EC.Defense = 15;
		EC.MaxHealth = 20;
		Out.Add(EC);

		FBalanceArchetype ES = MakeSimArchetype(SimEnemyS, ECharacterClass::Generic, EItemTier::S_Tier, SIM_POWER_S);
		ES.Body = 7;
		ES.Defense = 25;
		ES.MaxHealth = 35;
		Out.Add(ES);

		return Out;
	}

	FBalanceMatchup MakeSimMatchup(const TCHAR *Party, TArray<FName> Members, FName Enemy, EEncounterRank Rank, EAIDifficulty Difficulty)
	{
		FBalanceMatchup M;
		M.Party = MoveTemp(Members);
		M.Enemy = Enemy;
		M.Rank = Rank;
		M.EnemyCount = Rank == EEncounterRank::Normal ? SIM_NORMAL_ENEMY_COUNT : 1;
		M.Difficulty = Difficulty;
		M.ZoneTier = SIM_ZONE_TIER;
		M.Id = FName(FString::Printf(TEXT("%s-v-%s-%s-%s"), Party, *Enemy.ToString(),
									 *StaticEnum<EEncounterRank>()->GetNameStringByValue(static_cast<int64>(Rank)),
									 *StaticEnum<EAIDifficulty>()->GetNameStringByValue(static_cast<int64>(Difficulty))));
		return M;
	}

	TArray<FName> SimEarlyParty() { return {SimEarlyGenericF, SimEarlyGenericF, SimEarlyGenericF}; }
	TArray<FName> SimMidParty() { return {SimMidResonatorC, SimEarlyGenericF, SimEarlyGenericF}; } // TODO: the two Generics at C gear
	TArray<FName> SimLateParty() { return {SimLateCasterS, SimEarlyGenericF, SimEarlyGenericF}; }  // TODO: the two Generics at S gear
}

const TArray<FBalanceArchetype> &BalanceArchetypes::All()
{
	static const TArray<FBalanceArchetype> Table = BuildSimArchetypes();
	return Table;
}

const FBalanceArchetype *BalanceArchetypes::Find(FName Id)
{
	return All().FindByPredicate([Id](const FBalanceArchetype &A) { return A.Id == Id; });
}

TArray<FBalanceMatchup> BalanceArchetypes::Preset(FName Name)
{
	TArray<FBalanceMatchup> Out;
	if (Name == TEXT("quick"))
	{
		Out.Add(MakeSimMatchup(TEXT("Early"), SimEarlyParty(), SimEnemyF, EEncounterRank::Normal, EAIDifficulty::Medium));
		Out.Add(MakeSimMatchup(TEXT("Mid"), SimMidParty(), SimEnemyC, EEncounterRank::MiniBoss, EAIDifficulty::Medium));
		Out.Add(MakeSimMatchup(TEXT("Late"), SimLateParty(), SimEnemyS, EEncounterRank::Boss, EAIDifficulty::Medium));
	}
	else if (Name == TEXT("nightly"))
	{
		for (const EAIDifficulty Difficulty : {EAIDifficulty::Medium, EAIDifficulty::Hard})
		{
			for (const EEncounterRank Rank : {EEncounterRank::Normal, EEncounterRank::MiniBoss, EEncounterRank::Boss})
			{
				Out.Add(MakeSimMatchup(TEXT("Early"), SimEarlyParty(), SimEnemyF, Rank, Difficulty));
				Out.Add(MakeSimMatchup(TEXT("Mid"), SimMidParty(), SimEnemyC, Rank, Difficulty));
				Out.Add(MakeSimMatchup(TEXT("Late"), SimLateParty(), SimEnemyS, Rank, Difficulty));
			}
		}
		// Pillar 6: low level + S gear against high level + F gear, both AI; and high level + F gear against
		// same-tier enemies.
		FBalanceMatchup Pillar6 = MakeSimMatchup(TEXT("Pillar6"), {SimLowLevelSGear, SimLowLevelSGear, SimLowLevelSGear}, SimHighLevelFGear,
										  EEncounterRank::Normal, EAIDifficulty::Medium);
		Pillar6.EnemyCount = SIM_PILLAR6_SIDE_SIZE;
		Out.Add(Pillar6);
		Out.Add(MakeSimMatchup(TEXT("Pillar6Reverse"), {SimHighLevelFGear, SimHighLevelFGear, SimHighLevelFGear}, SimEnemyC,
						EEncounterRank::Normal, EAIDifficulty::Medium));
	}
	return Out;
}
