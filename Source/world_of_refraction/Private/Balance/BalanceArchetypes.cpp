// BalanceArchetypes.cpp
// B-292 / B-293: archetypes and presets. The Resonator casts one Instant spell through an adapter, the Caster an
// innate one (B-293); everyone fights with weapon and one ability.

#include "Balance/BalanceArchetypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** BaselineNumbers.md: weapon Power by tier. */
	constexpr int32 SIM_POWER_F = 40;
	constexpr int32 SIM_POWER_C = 88;
	constexpr int32 SIM_POWER_S = 192;
	/** Normal fights are 3 v 2-3 (BalanceSimulator.md §3); the sim uses the upper end. */
	constexpr int32 SIM_NORMAL_ENEMY_COUNT = 3;
	/** An Elite fight is 3 v 2 (BalanceSimulator.md §4). */
	constexpr int32 SIM_ELITE_ENEMY_COUNT = 2;
	constexpr int32 SIM_PILLAR6_SIDE_SIZE = 3;
	constexpr int32 SIM_ZONE_TIER = 0;
	/** Fights per matchup: quick runs in the suite, nightly and full are reports (BalanceSimulator.md §3). */
	constexpr int32 SIM_QUICK_FIGHTS = 3;
	constexpr int32 SIM_NIGHTLY_FIGHTS = 50;
	constexpr int32 SIM_FULL_FIGHTS = 200;
	const FName SimPresetQuick(TEXT("quick"));
	const FName SimPresetNightly(TEXT("nightly"));
	const FName SimPresetFull(TEXT("full"));

	const FName SimEarlyGenericF(TEXT("P-EarlyGenericF"));
	const FName SimMidResonatorC(TEXT("P-MidResonatorC"));
	const FName SimLateCasterS(TEXT("P-LateCasterS"));
	const FName SimLowLevelSGear(TEXT("P-LowLevelSGear"));
	const FName SimHighLevelFGear(TEXT("P-HighLevelFGear"));
	const FName SimGenericC(TEXT("P-GenericC"));
	const FName SimGenericS(TEXT("P-GenericS"));
	const FName SimStatusBuilder(TEXT("P-StatusBuilder"));
	/** Per hit: two or three landed hits fill a 100-point bar (STATUS_EFFECT_THRESHOLD) before scaling. */
	constexpr int32 SIM_PARITY_STATUS_BUILDUP = 40;
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

		// The Mid and Late parties' Generic members (Q-293e): stat points grow with the tier like the same-tier
		// enemy's (E-C / E-S below) and the worked examples' damage points and skill base (BaselineNumbers.md §3,
		// examples 2 and 3). Was the early build with only the weapon swapped, so two of three members had early HP.
		FBalanceArchetype GenericC = MakeSimArchetype(SimGenericC, ECharacterClass::Generic, EItemTier::C_Tier, SIM_POWER_C);
		GenericC.Body = 5;
		GenericC.RawDamage = 8;
		GenericC.Defense = 15;
		GenericC.MaxHealth = 20;
		GenericC.AbilityBase = 30;
		Out.Add(GenericC);
		FBalanceArchetype GenericS = MakeSimArchetype(SimGenericS, ECharacterClass::Generic, EItemTier::S_Tier, SIM_POWER_S);
		GenericS.Body = 7; // the example's 14 is past the cap
		GenericS.RawDamage = 20;
		GenericS.Defense = 25;
		GenericS.MaxHealth = 35;
		GenericS.AbilityBase = 40;
		Out.Add(GenericS);

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
		Mid.SpellBase = SIM_POWER_C; // a C Destruction spell, Instant, Fire, through the adapter: hits like a C weapon (B-325 note)
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
		Late.SpellBase = SIM_POWER_S; // an S spell, Instant, innate: hits like an S weapon, or the AI never casts it (B-325 note)
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

		// B-293b: the Mid Resonator with status on every hit, for the parity matchup only.
		FBalanceArchetype StatusBuilder = Mid;
		StatusBuilder.Id = SimStatusBuilder;
		StatusBuilder.StatusBuildup = SIM_PARITY_STATUS_BUILDUP;
		Out.Add(StatusBuilder);

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
		M.EnemyCount = Rank == EEncounterRank::Normal ? SIM_NORMAL_ENEMY_COUNT : Rank == EEncounterRank::Elite ? SIM_ELITE_ENEMY_COUNT : 1;
		M.Difficulty = Difficulty;
		// D-136 (B-344): the door's difficulty and the AI tier are one knob, so a Hard row is a Hard door (damage x1.2, Hard AI).
		M.TrialDifficulty = Difficulty == EAIDifficulty::Hard ? ETrialDifficulty::Hard
						  : Difficulty == EAIDifficulty::Easy ? ETrialDifficulty::Easy
															  : ETrialDifficulty::Medium;
		M.ZoneTier = SIM_ZONE_TIER;
		M.Id = FName(FString::Printf(TEXT("%s-v-%s-%s-%s"), Party, *Enemy.ToString(),
									 *StaticEnum<EEncounterRank>()->GetNameStringByValue(static_cast<int64>(Rank)),
									 *StaticEnum<EAIDifficulty>()->GetNameStringByValue(static_cast<int64>(Difficulty))));
		return M;
	}

	TArray<FName> SimEarlyParty() { return {SimEarlyGenericF, SimEarlyGenericF, SimEarlyGenericF}; }
	TArray<FName> SimMidParty() { return {SimMidResonatorC, SimGenericC, SimGenericC}; }
	TArray<FName> SimLateParty() { return {SimLateCasterS, SimGenericS, SimGenericS}; }
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
	if (Name == SimPresetQuick)
	{
		Out.Add(MakeSimMatchup(TEXT("Early"), SimEarlyParty(), SimEnemyF, EEncounterRank::Normal, EAIDifficulty::Medium));
		Out.Add(MakeSimMatchup(TEXT("Mid"), SimMidParty(), SimEnemyC, EEncounterRank::MiniBoss, EAIDifficulty::Medium));
		Out.Add(MakeSimMatchup(TEXT("Late"), SimLateParty(), SimEnemyS, EEncounterRank::Boss, EAIDifficulty::Medium));
	}
	else if (Name == SimPresetNightly || Name == SimPresetFull)
	{
		// full adds the Elite rank (B-224) to the grid.
		const TArray<EEncounterRank> Ranks = Name == SimPresetFull
												 ? TArray<EEncounterRank>{EEncounterRank::Normal, EEncounterRank::Elite, EEncounterRank::MiniBoss, EEncounterRank::Boss}
												 : TArray<EEncounterRank>{EEncounterRank::Normal, EEncounterRank::MiniBoss, EEncounterRank::Boss};
		for (const EAIDifficulty Difficulty : {EAIDifficulty::Medium, EAIDifficulty::Hard})
		{
			for (const EEncounterRank Rank : Ranks)
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

FBalanceMatchup BalanceArchetypes::ParityStatusMatchup()
{
	return MakeSimMatchup(TEXT("ParityStatus"), {SimStatusBuilder, SimStatusBuilder, SimStatusBuilder}, SimEnemyC, EEncounterRank::Normal,
						  EAIDifficulty::Medium);
}

int32 BalanceArchetypes::PresetFights(FName Name)
{
	if (Name == SimPresetQuick)
	{
		return SIM_QUICK_FIGHTS;
	}
	if (Name == SimPresetNightly)
	{
		return SIM_NIGHTLY_FIGHTS;
	}
	if (Name == SimPresetFull)
	{
		return SIM_FULL_FIGHTS;
	}
	return 0;
}

const TArray<FName> &BalanceArchetypes::PresetNames()
{
	static const TArray<FName> Names = {SimPresetQuick, SimPresetNightly, SimPresetFull};
	return Names;
}

#endif // WITH_DEV_AUTOMATION_TESTS
