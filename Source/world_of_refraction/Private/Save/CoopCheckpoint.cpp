// CoopCheckpoint.cpp

#include "Save/CoopCheckpoint.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Compression.h"
#include "Player/WoRPlayerState.h"
#include "WoRLog.h"

const TCHAR *WoRCheckpoint::SLOT_NAME = TEXT("WoRCoopCheckpoint");

namespace
{
	/** Each send gets a new id, so a client never mixes two checkpoints' chunks. */
	int32 GNextCheckpointId = 1;
}

TArray<TArray<uint8>> WoRCheckpoint::Split(const TArray<uint8> &Bytes, int32 ChunkBytes)
{
	TArray<TArray<uint8>> Chunks;
	const int32 Size = FMath::Max(ChunkBytes, 1);
	for (int32 Offset = 0; Offset < Bytes.Num(); Offset += Size)
	{
		Chunks.Emplace(Bytes.GetData() + Offset, FMath::Min(Size, Bytes.Num() - Offset));
	}
	if (Chunks.IsEmpty())
	{
		Chunks.AddDefaulted();
	}
	return Chunks;
}

int32 WoRCheckpoint::ChunkCountFor(int32 ByteCount, int32 ChunkBytes)
{
	const int32 Size = FMath::Max(ChunkBytes, 1);
	return FMath::Max(1, (ByteCount + Size - 1) / Size);
}

bool WoRCheckpoint::Compress(const TArray<uint8> &Bytes, TArray<uint8> &OutCompressed)
{
	int32 CompressedSize = FCompression::CompressMemoryBound(NAME_Zlib, Bytes.Num());
	OutCompressed.SetNumUninitialized(CompressedSize);
	if (!FCompression::CompressMemory(NAME_Zlib, OutCompressed.GetData(), CompressedSize, Bytes.GetData(), Bytes.Num()))
	{
		OutCompressed.Reset();
		return false;
	}
	OutCompressed.SetNum(CompressedSize);
	return true;
}

bool WoRCheckpoint::Decompress(const TArray<uint8> &Compressed, int32 UncompressedSize, TArray<uint8> &OutBytes)
{
	OutBytes.SetNumUninitialized(FMath::Max(UncompressedSize, 0));
	if (UncompressedSize <= 0
		|| !FCompression::UncompressMemory(NAME_Zlib, OutBytes.GetData(), UncompressedSize, Compressed.GetData(), Compressed.Num()))
	{
		OutBytes.Reset();
		return UncompressedSize == 0;
	}
	return true;
}

int32 WoRCheckpoint::SendToClients(UWorld *World, USaveGame *Save)
{
	if (!World || !Save || World->GetNetMode() != NM_ListenServer)
	{
		return 0; // solo has no one to send to; a client never sends (B-234b: a dedicated host would send too)
	}
	const AGameStateBase *GameState = World->GetGameState();
	if (!GameState)
	{
		return 0;
	}
	TArray<uint8> Bytes;
	TArray<uint8> Compressed;
	if (!UGameplayStatics::SaveGameToMemory(Save, Bytes) || !Compress(Bytes, Compressed))
	{
		UE_LOG(LogWoR, Warning, TEXT("[Checkpoint] Could not serialize or compress the save; no copy sent"));
		return 0;
	}
	const int32 ChunkCount = ChunkCountFor(Compressed.Num());
	if (ChunkCount > MAX_CHUNKS)
	{
		UE_LOG(LogWoR, Warning, TEXT("[Checkpoint] The save is too big to copy (%d bytes compressed, %d chunks, max %d); no copy sent"),
			   Compressed.Num(), ChunkCount, MAX_CHUNKS);
		return 0;
	}
	const int32 CheckpointId = GNextCheckpointId++;
	int32 Sent = 0;
	for (APlayerState *Player : GameState->PlayerArray)
	{
		AWoRPlayerState *WoRPlayer = Cast<AWoRPlayerState>(Player);
		const APlayerController *PC = WoRPlayer ? Cast<APlayerController>(WoRPlayer->GetOwner()) : nullptr;
		if (!PC || PC->IsLocalController())
		{
			continue; // the host keeps its own save
		}
		WoRPlayer->SendCheckpoint(CheckpointId, Compressed, Bytes.Num());
		++Sent;
	}
	UE_LOG(LogWoR, Log, TEXT("[Checkpoint] Copy %d (%d bytes, %d compressed, %d chunk(s)) sent to %d player(s)"), CheckpointId, Bytes.Num(),
		   Compressed.Num(), ChunkCount, Sent);
	return Sent;
}

bool FCoopCheckpointAssembler::Add(int32 InCheckpointId, int32 ChunkIndex, int32 ChunkCount, const TArray<uint8> &Chunk)
{
	if (ChunkCount <= 0 || ChunkIndex < 0 || ChunkIndex >= ChunkCount)
	{
		return false;
	}
	if (InCheckpointId != CheckpointId)
	{
		if (CheckpointId != INDEX_NONE && InCheckpointId < CheckpointId)
		{
			return false; // an older checkpoint's straggler
		}
		CheckpointId = InCheckpointId;
		Chunks.Reset();
		Chunks.SetNum(ChunkCount);
		Have.Init(false, ChunkCount);
		Received = 0;
		Assembled.Reset();
	}
	if (Chunks.Num() != ChunkCount || Have[ChunkIndex])
	{
		return false; // a count that disagrees, or a duplicate
	}
	Chunks[ChunkIndex] = Chunk;
	Have[ChunkIndex] = true;
	if (++Received < ChunkCount)
	{
		return false;
	}
	Assembled.Reset();
	for (const TArray<uint8> &Piece : Chunks)
	{
		Assembled.Append(Piece);
	}
	return true;
}
