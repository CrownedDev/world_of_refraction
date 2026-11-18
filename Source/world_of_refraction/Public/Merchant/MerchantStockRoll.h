// MerchantStockRoll.h
// Rolled in-run vendor stock (Crown 2026-10-26, rules page 05; B-179): a vendor met in a trial
// shows a random part of its authored stock, rolled once per run from the run seed. Home shops
// keep their full, fixed list.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Misc/Crc.h"
#include "Merchant/MerchantData.h"

namespace MerchantStockRoll
{
	/** Lines a trial vendor shows per run. */
	constexpr int32 TRIAL_VENDOR_STOCK_SIZE = 8; // TODO: tune (Q-179)

	/** Each vendor rolls its own stock from the shared run seed. Hashes the path text (stable across
	 *  launches, unlike an FName hash) so wor.Trial.Seed replays the same stock after a restart. */
	inline int32 SeedFor(int32 RunSeed, const UMerchantData &Merchant)
	{
		return static_cast<int32>(HashCombine(static_cast<uint32>(RunSeed), FCrc::StrCrc32(*Merchant.GetPathName())));
	}

	/** Size lines of Pool picked at random by Seed (all of it when it is smaller), in authored order. */
	inline TArray<FMerchantStockEntry> Roll(const TArray<FMerchantStockEntry> &Pool, int32 Seed,
											int32 Size = TRIAL_VENDOR_STOCK_SIZE)
	{
		if (Pool.Num() <= Size)
		{
			return Pool;
		}

		TArray<int32> Indices;
		for (int32 i = 0; i < Pool.Num(); ++i)
		{
			Indices.Add(i);
		}
		FRandomStream Stream(Seed);
		for (int32 i = 0; i < Size; ++i)
		{
			Indices.Swap(i, Stream.RandRange(i, Indices.Num() - 1));
		}
		Indices.SetNum(Size);
		Indices.Sort();

		TArray<FMerchantStockEntry> Rolled;
		for (const int32 Index : Indices)
		{
			Rolled.Add(Pool[Index]);
		}
		return Rolled;
	}

	/** B-571 (D-225, TrialPoolDesign §2): the share of a trial vendor's shelf drawn from the trial's pool; the rest is
	 *  its own list (things that never drop). */
	constexpr float TRIAL_POOL_SHARE = 0.5f; // TODO: tune (Q-571)
	/** The pool half rolls from its own stream, so a change to one half's list leaves the other half's picks alone. */
	constexpr uint32 TRIAL_POOL_SEED_SALT = 0x571;

	inline bool IsSameLine(const FMerchantStockEntry &A, const FMerchantStockEntry &B)
	{
		return A.IsAssetEntry() ? A.Asset == B.Asset : (B.IsCrystalEntry() && A.Crystal == B.Crystal);
	}

	/** B-571: a trial vendor's shelf, half and half: TRIAL_POOL_SHARE of Size from TrialPool (the lines Own doesn't
	 *  list already), the rest from Own. A side with too few lines leaves its places to the other. Own's lines first
	 *  (in authored order), then the pool's. */
	inline TArray<FMerchantStockEntry> RollWithPool(const TArray<FMerchantStockEntry> &Own, const TArray<FMerchantStockEntry> &TrialPool,
													int32 Seed, int32 Size = TRIAL_VENDOR_STOCK_SIZE)
	{
		TArray<FMerchantStockEntry> Offered;
		for (const FMerchantStockEntry &Line : TrialPool)
		{
			const auto Same = [&Line](const FMerchantStockEntry &Other) { return IsSameLine(Line, Other); };
			if (!Own.ContainsByPredicate(Same) && !Offered.ContainsByPredicate(Same))
			{
				Offered.Add(Line);
			}
		}
		const int32 Wanted = FMath::RoundToInt(static_cast<float>(Size) * TRIAL_POOL_SHARE);
		const int32 PoolLines = FMath::Min(Offered.Num(), FMath::Max(Wanted, Size - Own.Num()));
		const int32 OwnLines = FMath::Min(Own.Num(), Size - PoolLines);
		TArray<FMerchantStockEntry> Shelf = Roll(Own, Seed, OwnLines);
		Shelf.Append(Roll(Offered, static_cast<int32>(HashCombine(static_cast<uint32>(Seed), TRIAL_POOL_SEED_SALT)), PoolLines));
		return Shelf;
	}

	/** What a merchant shows: its rolled stock in a trial, its full authored stock at home. OBSOLETE in a trial (B-571):
	 *  UTrialRunSubsystem::GetMerchantStock rolls with the trial's pool (RollWithPool). */
	inline TArray<FMerchantStockEntry> StockFor(const UMerchantData &Merchant, bool bInTrial, int32 RunSeed)
	{
		return bInTrial ? Roll(Merchant.Stock, SeedFor(RunSeed, Merchant)) : Merchant.Stock;
	}
}
