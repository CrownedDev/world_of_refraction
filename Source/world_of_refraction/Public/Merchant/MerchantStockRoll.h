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

	/** What a merchant shows: its rolled stock in a trial, its full authored stock at home. */
	inline TArray<FMerchantStockEntry> StockFor(const UMerchantData &Merchant, bool bInTrial, int32 RunSeed)
	{
		return bInTrial ? Roll(Merchant.Stock, SeedFor(RunSeed, Merchant)) : Merchant.Stock;
	}
}
