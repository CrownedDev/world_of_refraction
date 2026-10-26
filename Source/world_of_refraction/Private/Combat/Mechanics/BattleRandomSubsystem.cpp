// BattleRandomSubsystem.cpp

#include "Combat/Mechanics/BattleRandomSubsystem.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Guid.h"
#include "WoRLog.h"

namespace
{
	/** 0 means "pick a fresh seed", so a generated seed of 0 is bumped to this. */
	constexpr int32 NONZERO_SEED_FALLBACK = 1;

	TAutoConsoleVariable<int32> CVarBattleSeed(
		TEXT("wor.Battle.Seed"),
		0,
		TEXT("Seed for every following battle's random stream (0 = a fresh seed each battle). ")
		TEXT("Copy the seed from the [BattleRandom] log line to replay a fight."));

	/** FMath::Rand is only 15 bits on Windows; a GUID word gives the full positive int32 range. */
	int32 MakeFreshSeed()
	{
		const int32 NewSeed = static_cast<int32>(FGuid::NewGuid().A & static_cast<uint32>(MAX_int32));
		return NewSeed != 0 ? NewSeed : NONZERO_SEED_FALLBACK;
	}
}

void UBattleRandomSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	// Rolls taken before any battle (or in a world with no battle) still get a valid stream.
	SeedStream(MakeFreshSeed());
}

int32 UBattleRandomSubsystem::BeginBattle()
{
	const int32 ForcedSeed = CVarBattleSeed.GetValueOnGameThread();
	SeedStream(ForcedSeed != 0 ? ForcedSeed : MakeFreshSeed());
	UE_LOG(LogWoRCombat, Log, TEXT("[BattleRandom] Battle seed %d%s (replay with: wor.Battle.Seed %d)"),
		   Seed, ForcedSeed != 0 ? TEXT(" (from wor.Battle.Seed)") : TEXT(""), Seed);
	return Seed;
}

void UBattleRandomSubsystem::SeedStream(int32 NewSeed)
{
	Seed = NewSeed;
	Stream.Initialize(NewSeed);
	DrawCount = 0;
}

UBattleRandomSubsystem *UBattleRandomSubsystem::Get(const UObject *WorldContext)
{
	UWorld *World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBattleRandomSubsystem>() : nullptr;
}

float UBattleRandomSubsystem::FRand(const UObject *WorldContext)
{
	if (UBattleRandomSubsystem *Battle = Get(WorldContext))
	{
		++Battle->DrawCount;
		return Battle->Stream.FRand();
	}
	return FMath::FRand();
}

float UBattleRandomSubsystem::FRandRange(const UObject *WorldContext, float Min, float Max)
{
	if (UBattleRandomSubsystem *Battle = Get(WorldContext))
	{
		++Battle->DrawCount;
		return Battle->Stream.FRandRange(Min, Max);
	}
	return FMath::FRandRange(Min, Max);
}

int32 UBattleRandomSubsystem::RandRange(const UObject *WorldContext, int32 Min, int32 Max)
{
	if (UBattleRandomSubsystem *Battle = Get(WorldContext))
	{
		++Battle->DrawCount;
		return Battle->Stream.RandRange(Min, Max);
	}
	return FMath::RandRange(Min, Max);
}

bool UBattleRandomSubsystem::RandBool(const UObject *WorldContext)
{
	return RandRange(WorldContext, 0, 1) == 1;
}
