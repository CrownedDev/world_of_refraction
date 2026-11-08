// CoopCheckpoint.h
// B-234 part 1 (D-103): checkpoint copies. Whenever the host saves in a networked game (a fight's exit, a trial's
// exit, a rest spot once B-187 lands: every USaveSubsystem::SaveNow), it sends the whole save to every other player,
// who keeps it in its own slot (SLOT_NAME), so the run can go on from the last checkpoint if the host drops (the
// hand-over itself is B-234b). The save travels as its UGameplayStatics::SaveGameToMemory bytes, in chunks small
// enough for one RPC parameter (CHUNK_BYTES), by Client RPC on each player's AWoRPlayerState.
// Debug: wor.Coop.PrintCheckpoint (AWoRPlayerState::GetCheckpointString).

#pragma once

#include "CoreMinimal.h"

class UWorld;
class USaveGame;

namespace WoRCheckpoint
{
	/** The client's own slot for the host's copy (never the player's own save). */
	WORLD_OF_REFRACTION_API extern const TCHAR *SLOT_NAME;

	/** Bytes per RPC chunk: under the engine's array limit for one replicated parameter (net.MaxRepArraySize). */
	constexpr int32 CHUNK_BYTES = 1024;

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
	bool Add(int32 CheckpointId, int32 ChunkIndex, int32 ChunkCount, const TArray<uint8> &Chunk);

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
