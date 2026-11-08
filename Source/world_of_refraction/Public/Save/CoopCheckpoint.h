// CoopCheckpoint.h
// B-234 part 1 (D-103): checkpoint copies. Whenever the host saves in a networked game (a fight's exit, a trial's
// exit, a rest spot once B-187 lands: every USaveSubsystem::SaveNow), it sends the whole save to every other player,
// who keeps it in its own slot (SLOT_NAME), so the run can go on from the last checkpoint if the host drops (the
// hand-over itself is B-234b). The save travels as its UGameplayStatics::SaveGameToMemory bytes, zlib-compressed, in
// chunks small enough for one RPC parameter (CHUNK_BYTES), by Client RPC on each player's AWoRPlayerState; a save
// over MAX_CHUNKS chunks is not sent (the reliable buffer would overflow and drop the client).
// Debug: wor.Coop.PrintCheckpoint (AWoRPlayerState::GetCheckpointString).

#pragma once

#include "CoreMinimal.h"
#include "Containers/BitArray.h"

class UWorld;
class USaveGame;

namespace WoRCheckpoint
{
	/** The client's own slot for the host's copy (never the player's own save). */
	WORLD_OF_REFRACTION_API extern const TCHAR *SLOT_NAME;

	/** Bytes per RPC chunk: under the engine's array limit for one replicated parameter (net.MaxRepArraySize 2048). */
	constexpr int32 CHUNK_BYTES = 2000;

	/** The most chunks one copy may take (about 80 KB compressed): each 2000-byte RPC splits into 2-3 partial bunches,
	 *  and the connection's reliable buffer holds 256, so more in one frame could overflow it and drop the client
	 *  (desktop review: was 128). TODO: spread a bigger copy over several ticks if saves ever grow that large. */
	constexpr int32 MAX_CHUNKS = 40;

	/** The largest save a copy may unpack to: a bad or hostile size is refused before any allocation. */
	constexpr int32 MAX_UNCOMPRESSED_BYTES = 16 * 1024 * 1024;

	/** Chunks Bytes needs (at least one). */
	WORLD_OF_REFRACTION_API int32 ChunkCountFor(int32 ByteCount, int32 ChunkBytes = CHUNK_BYTES);

	/** zlib, for the trip. False when the engine refuses. */
	WORLD_OF_REFRACTION_API bool Compress(const TArray<uint8> &Bytes, TArray<uint8> &OutCompressed);
	WORLD_OF_REFRACTION_API bool Decompress(const TArray<uint8> &Compressed, int32 UncompressedSize, TArray<uint8> &OutBytes);

	/** Bytes split into CHUNK_BYTES pieces (one empty chunk for empty bytes, so the receiver still completes). */
	WORLD_OF_REFRACTION_API TArray<TArray<uint8>> Split(const TArray<uint8> &Bytes, int32 ChunkBytes = CHUNK_BYTES);

	/** Host: sends Save to every remote player in World. Returns how many players it went to (0 off a server). */
	WORLD_OF_REFRACTION_API int32 SendToClients(UWorld *World, USaveGame *Save);
}

/** Client: puts one checkpoint's chunks back together, in any order; a newer checkpoint id starts over. */
struct WORLD_OF_REFRACTION_API FCoopCheckpointAssembler
{
	/** Adds a chunk. True when this chunk completed the checkpoint (then GetBytes holds it). A chunk of an older id,
	 *  a bad index or a count that disagrees is ignored. */
	bool Add(int32 InCheckpointId, int32 ChunkIndex, int32 ChunkCount, const TArray<uint8> &Chunk);

	const TArray<uint8> &GetBytes() const { return Assembled; }
	int32 GetCheckpointId() const { return CheckpointId; }
	int32 GetReceivedCount() const { return Received; }
	int32 GetChunkCount() const { return Chunks.Num(); }

private:
	int32 CheckpointId = INDEX_NONE;
	TArray<TArray<uint8>> Chunks;
	TBitArray<> Have;
	int32 Received = 0;
	TArray<uint8> Assembled;
};
