// MemoryFragmentSpec.cpp
// B-190 (RunStructure §5, Q-190): a memory runs deep for one character and minor for another; a
// fragment pickup adds its memory to the run and the journal once, and is gone afterwards.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryFragment.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	const FName MEMORY_TEST_ASSET_NAME(TEXT("Memory_SpecTest"));
	const FName MEMORY_TEST_FRAGMENT_PROPERTY(TEXT("Memory"));
}

BEGIN_DEFINE_SPEC(FMemoryFragmentSpec, "WoR.Memory.Fragment",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	static UMemoryData *NewMemory()
	{
		return NewObject<UMemoryData>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(),
															 UMemoryData::StaticClass(), MEMORY_TEST_ASSET_NAME));
	}

	/** A placed fragment holding Memory (its field is designer-set, so set it by reflection). */
	AMemoryFragment *SpawnFragment(UMemoryData *Memory)
	{
		AMemoryFragment *Fragment = TestWorld->GetWorld()->SpawnActor<AMemoryFragment>(FVector::ZeroVector, FRotator::ZeroRotator);
		const FObjectPropertyBase *Property = CastField<FObjectPropertyBase>(
			AMemoryFragment::StaticClass()->FindPropertyByName(MEMORY_TEST_FRAGMENT_PROPERTY));
		if (Fragment && Property)
		{
			Property->SetObjectPropertyValue_InContainer(Fragment, Memory);
		}
		return Fragment;
	}
END_DEFINE_SPEC(FMemoryFragmentSpec)

void FMemoryFragmentSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("runs deep for one character, minor for another, and means nothing to the rest", [this]()
	{
		UMemoryData *Memory = NewMemory();
		UCharacterData *Lord = FWoRTestWorld::NewCharacterData();
		UCharacterData *Other = FWoRTestWorld::NewCharacterData();
		UCharacterData *Stranger = FWoRTestWorld::NewCharacterData();
		Memory->CharacterDepth.Add(Lord, EMemoryDepth::Deep);
		Memory->CharacterDepth.Add(Other, EMemoryDepth::Minor);
		TestEqual(TEXT("Deep for its lord"), Memory->GetDepthFor(Lord), EMemoryDepth::Deep);
		TestEqual(TEXT("Minor for another"), Memory->GetDepthFor(Other), EMemoryDepth::Minor);
		TestEqual(TEXT("None for the rest"), Memory->GetDepthFor(Stranger), EMemoryDepth::None);
		TestEqual(TEXT("No id: the asset's name"), Memory->GetMemoryId(), Memory->GetFName());
		TestTrue(TEXT("Debug string lists the depth"), Memory->GetMemoryString().Contains(TEXT("Deep for")));
	});

	It("adds its memory to the run and the journal once, then is gone", [this]()
	{
		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		UMemoryJournalSubsystem *Journal = TestWorld->GetSubsystem<UMemoryJournalSubsystem>();
		UMemoryData *Memory = NewMemory();
		AMemoryFragment *Fragment = SpawnFragment(Memory);
		if (!TestTrue(TEXT("Spawned"), TrialRun && Journal && Fragment && Fragment->GetMemory() == Memory)) return;

		Fragment->Interact(nullptr);
		TestTrue(TEXT("In the run"), TrialRun->GetRunMemories().Contains(Memory));
		TestTrue(TEXT("In the journal"), Journal->HasMemory(Memory));
		TestTrue(TEXT("Gone from the map"), Fragment->IsHidden());
		TestFalse(TEXT("Out of reach"), Fragment->GetActorEnableCollision());

		Fragment->Interact(nullptr);
		TestEqual(TEXT("Once in the run"), TrialRun->GetRunMemories().Num(), 1);
		TestEqual(TEXT("Once in the journal"), Journal->GetMemories().Num(), 1);
		TestFalse(TEXT("The journal refuses a repeat"), Journal->RecordMemory(Memory, EMemorySource::Fragment));
	});

	It("stays gone when the level reloads after it was taken this run", [this]()
	{
		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		AMemoryFragment *Fragment = TestWorld->GetWorld()->SpawnActorDeferred<AMemoryFragment>(
			AMemoryFragment::StaticClass(), FTransform::Identity);
		if (!TestTrue(TEXT("Spawned"), TrialRun && Fragment)) return;
		TrialRun->RecordFound(Fragment->GetFeatureId(), FText::GetEmpty());
		Fragment->FinishSpawning(FTransform::Identity);
		TestTrue(TEXT("Hidden"), Fragment->IsHidden());
		TestFalse(TEXT("Out of reach"), Fragment->GetActorEnableCollision());
	});

	It("picks up nothing when no memory is set", [this]()
	{
		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		AMemoryFragment *Fragment = SpawnFragment(nullptr);
		if (!TestTrue(TEXT("Spawned"), TrialRun && Fragment)) return;
		Fragment->Interact(nullptr);
		TestEqual(TEXT("No memory collected"), TrialRun->GetRunMemories().Num(), 0);
		TestFalse(TEXT("Still on the map"), Fragment->IsHidden());
	});
}

#endif
