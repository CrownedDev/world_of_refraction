// MemoryDebugSpec.cpp
// B-375 (MemoryCatalogue §6): wor.Memory.Why's text: a memory's scope, each condition marked met / not yet, and who
// would receive it (standalone: the host).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryDebug.h"
#include "Memory/MemoryJournalSubsystem.h"

BEGIN_DEFINE_SPEC(FMemoryDebugSpec, "WoR.Memory.Debug", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UGameInstance *GameInstance = nullptr;
	UMemoryJournalSubsystem *Journal = nullptr;
END_DEFINE_SPEC(FMemoryDebugSpec)

void FMemoryDebugSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		GameInstance = TestWorld->GetWorld() ? TestWorld->GetWorld()->GetGameInstance() : nullptr;
		Journal = TestWorld->GetSubsystem<UMemoryJournalSubsystem>();
		TestTrue(TEXT("Subsystems"), GameInstance && Journal);
	});

	AfterEach([this]() { TestWorld.Reset(); });

	It("says who a memory is for", [this]()
	{
		UMemoryData *Everyone = NewObject<UMemoryData>(GetTransientPackage());
		UMemoryData *Personal = NewObject<UMemoryData>(GetTransientPackage());
		UCharacterData *Garnet = FWoRTestWorld::NewCharacterData();
		Personal->CharacterDepth.Add(Garnet, EMemoryDepth::Deep);
		TestEqual(TEXT("no character listed"), MemoryDebug::GetScopeString(*Everyone), FString(TEXT("for every character")));
		TestTrue(TEXT("names the character"), MemoryDebug::GetScopeString(*Personal).Contains(GetNameSafe(Garnet)));
	});

	It("marks each condition met or not yet, and who would get it", [this]()
	{
		UMemoryData *First = NewObject<UMemoryData>(GetTransientPackage());
		UMemoryData *After = NewObject<UMemoryData>(GetTransientPackage());
		FMemoryCondition Prior;
		Prior.Kind = EMemoryConditionKind::PriorMemory;
		Prior.PriorMemory = First;
		After->UnlockConditions = {Prior};
		FMemoryEvent Event;

		const FString Before = MemoryDebug::GetWhyString(GameInstance, *After, Event);
		TestTrue(TEXT("not yet"), Before.Contains(TEXT("[not yet]")) && Before.Contains(TEXT("-> not met")));
		Journal->RecordMemory(First, EMemorySource::Fragment);
		const FString Now = MemoryDebug::GetWhyString(GameInstance, *After, Event);
		TestTrue(TEXT("met"), Now.Contains(TEXT("[met]")) && Now.Contains(TEXT("-> MET"), ESearchCase::CaseSensitive));
		TestFalse(TEXT("no longer not met"), Now.Contains(TEXT("not met")));
		TestTrue(TEXT("standalone: the host would get it"), Now.Contains(TEXT("would go to: host")));
		TestEqual(TEXT("no conditions: its source"), MemoryDebug::GetRoutesString(*First, Event, Journal), FString(TEXT("by its source only")));
		TestNull(TEXT("an id nobody has"), MemoryDebug::FindMemory(TEXT("NoSuchMemory_DebugSpec")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
