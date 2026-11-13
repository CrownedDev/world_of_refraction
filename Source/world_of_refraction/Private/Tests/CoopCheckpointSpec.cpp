// CoopCheckpointSpec.cpp
// B-234 part 1: the host's save survives the trip in chunks: split, reassembled in any order, duplicates and older
// checkpoints ignored, a newer checkpoint starting over. The send itself is the 2-player PIE sheet.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Save/CoopCheckpoint.h"

namespace
{
	constexpr int32 CHECKPOINT_TEST_CHUNK = 4;

	TArray<uint8> CheckpointTestBytes(int32 Count)
	{
		TArray<uint8> Bytes;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Bytes.Add(static_cast<uint8>(Index % 251));
		}
		return Bytes;
	}
}

BEGIN_DEFINE_SPEC(FCoopCheckpointSpec, "WoR.Coop.Checkpoint", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FCoopCheckpointSpec)

void FCoopCheckpointSpec::Define()
{
	It("splits into chunks no larger than the chunk size, and an empty save into one chunk", [this]()
	{
		const TArray<TArray<uint8>> Chunks = WoRCheckpoint::Split(CheckpointTestBytes(10), CHECKPOINT_TEST_CHUNK);
		TestEqual(TEXT("three chunks"), Chunks.Num(), 3);
		TestEqual(TEXT("the last holds the rest"), Chunks.Last().Num(), 2);
		TestEqual(TEXT("empty: one chunk"), WoRCheckpoint::Split(TArray<uint8>(), CHECKPOINT_TEST_CHUNK).Num(), 1);
		TestTrue(TEXT("the RPC chunk fits one parameter"), WoRCheckpoint::CHUNK_BYTES <= 2048);
	});

	It("puts the chunks back together in any order", [this]()
	{
		const TArray<uint8> Bytes = CheckpointTestBytes(10);
		const TArray<TArray<uint8>> Chunks = WoRCheckpoint::Split(Bytes, CHECKPOINT_TEST_CHUNK);
		FCoopCheckpointAssembler Assembler;
		TestFalse(TEXT("chunk 2"), Assembler.Add(1, 2, Chunks.Num(), Chunks[2]));
		TestFalse(TEXT("chunk 0"), Assembler.Add(1, 0, Chunks.Num(), Chunks[0]));
		TestFalse(TEXT("chunk 0 again: ignored"), Assembler.Add(1, 0, Chunks.Num(), Chunks[0]));
		TestTrue(TEXT("chunk 1 completes it"), Assembler.Add(1, 1, Chunks.Num(), Chunks[1]));
		TestTrue(TEXT("the same bytes"), Assembler.GetBytes() == Bytes);
	});

	It("starts over on a newer checkpoint and ignores an older one's stragglers", [this]()
	{
		const TArray<TArray<uint8>> Old = WoRCheckpoint::Split(CheckpointTestBytes(8), CHECKPOINT_TEST_CHUNK);
		const TArray<uint8> NewBytes = CheckpointTestBytes(3);
		FCoopCheckpointAssembler Assembler;
		Assembler.Add(5, 0, Old.Num(), Old[0]);
		TestTrue(TEXT("checkpoint 6 in one chunk"), Assembler.Add(6, 0, 1, NewBytes));
		TestEqual(TEXT("now 6"), Assembler.GetCheckpointId(), 6);
		TestFalse(TEXT("5's last chunk is ignored"), Assembler.Add(5, 1, Old.Num(), Old[1]));
		TestTrue(TEXT("still 6's bytes"), Assembler.GetBytes() == NewBytes);
	});

	It("compresses for the trip and back, and counts the chunks (review)", [this]()
	{
		const TArray<uint8> Bytes = CheckpointTestBytes(5000);
		TArray<uint8> Compressed;
		TArray<uint8> Back;
		TestTrue(TEXT("compressed"), WoRCheckpoint::Compress(Bytes, Compressed));
		TestTrue(TEXT("decompressed"), WoRCheckpoint::Decompress(Compressed, Bytes.Num(), Back));
		TestTrue(TEXT("the same bytes"), Back == Bytes);
		AddExpectedError(TEXT("Failed to uncompress memory"), EAutomationExpectedErrorFlags::Contains, 1); // the engine logs the refusal
		TestFalse(TEXT("a wrong size fails"), WoRCheckpoint::Decompress(Compressed, Bytes.Num() / 2, Back));
		TestEqual(TEXT("chunks: 10 bytes by 4"), WoRCheckpoint::ChunkCountFor(10, CHECKPOINT_TEST_CHUNK), 3);
		TestEqual(TEXT("chunks: empty still one"), WoRCheckpoint::ChunkCountFor(0, CHECKPOINT_TEST_CHUNK), 1);
		TestEqual(TEXT("ChunkCountFor matches Split"), WoRCheckpoint::ChunkCountFor(Bytes.Num()), WoRCheckpoint::Split(Bytes).Num());
	});

	It("ignores a bad index or a count that disagrees", [this]()
	{
		FCoopCheckpointAssembler Assembler;
		TestFalse(TEXT("index past the count"), Assembler.Add(1, 3, 2, CheckpointTestBytes(1)));
		TestFalse(TEXT("no chunks"), Assembler.Add(1, 0, 0, CheckpointTestBytes(1)));
		TestFalse(TEXT("a count over MAX_CHUNKS (B-434)"), Assembler.Add(1, 0, WoRCheckpoint::MAX_CHUNKS + 1, CheckpointTestBytes(1)));
		TestEqual(TEXT("and nothing was started for it"), Assembler.GetCheckpointId(), static_cast<int32>(INDEX_NONE));
		Assembler.Add(2, 0, 3, CheckpointTestBytes(1));
		TestFalse(TEXT("same id, another count"), Assembler.Add(2, 1, 2, CheckpointTestBytes(1)));
		TestEqual(TEXT("one chunk in"), Assembler.GetReceivedCount(), 1);
	});
}

#endif
