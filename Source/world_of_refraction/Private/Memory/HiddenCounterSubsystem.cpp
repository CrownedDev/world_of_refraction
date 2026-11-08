// HiddenCounterSubsystem.cpp

#include "Memory/HiddenCounterSubsystem.h"

#include "Character/CharacterData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

FName UHiddenCounterSubsystem::KeyFor(const UCharacterData *Character)
{
	return Character ? Character->GetFName() : NAME_None;
}

void UHiddenCounterSubsystem::Add(FName Counter, const UCharacterData *Character, int32 Amount)
{
	if (Counter.IsNone() || Amount <= 0)
	{
		return;
	}
	TMap<FName, int32> &ByCharacter = Counts.FindOrAdd(Counter);
	ByCharacter.FindOrAdd(NAME_None) += Amount;
	if (Character)
	{
		ByCharacter.FindOrAdd(KeyFor(Character)) += Amount;
	}
	UE_LOG(LogWoR, Verbose, TEXT("[HiddenCounter] %s +%d for %s"), *Counter.ToString(), Amount, *GetNameSafe(Character));
}

int32 UHiddenCounterSubsystem::Get(FName Counter, const UCharacterData *Character) const
{
	const TMap<FName, int32> *ByCharacter = Counts.Find(Counter);
	const int32 *Count = ByCharacter ? ByCharacter->Find(KeyFor(Character)) : nullptr;
	return Count ? *Count : 0;
}

FSavedCounters UHiddenCounterSubsystem::Capture() const
{
	FSavedCounters Saved;
	for (const TPair<FName, TMap<FName, int32>> &Counter : Counts)
	{
		for (const TPair<FName, int32> &Entry : Counter.Value)
		{
			FSavedCounter &Out = Saved.Counts.AddDefaulted_GetRef();
			Out.Counter = Counter.Key;
			Out.Character = Entry.Key;
			Out.Count = Entry.Value;
		}
	}
	return Saved;
}

void UHiddenCounterSubsystem::Restore(const FSavedCounters &Saved)
{
	Counts.Reset();
	for (const FSavedCounter &In : Saved.Counts)
	{
		if (!In.Counter.IsNone() && In.Count > 0)
		{
			Counts.FindOrAdd(In.Counter).Add(In.Character, In.Count);
		}
	}
}

FString UHiddenCounterSubsystem::GetCountersString() const
{
	if (Counts.IsEmpty())
	{
		return TEXT("no hidden counters yet");
	}
	TArray<FString> Lines;
	for (const TPair<FName, TMap<FName, int32>> &Counter : Counts)
	{
		TArray<FString> Parts;
		for (const TPair<FName, int32> &Entry : Counter.Value)
		{
			Parts.Add(FString::Printf(TEXT("%s %d"), Entry.Key.IsNone() ? TEXT("all") : *Entry.Key.ToString(), Entry.Value));
		}
		Parts.Sort();
		Lines.Add(FString::Printf(TEXT("%s: %s"), *Counter.Key.ToString(), *FString::Join(Parts, TEXT(", "))));
	}
	Lines.Sort();
	return FString::Join(Lines, TEXT("\n"));
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GCounterPrintCommand(
	TEXT("wor.Counter.Print"),
	TEXT("B-379: this player's hidden counters (each counter per character and for all)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		const UHiddenCounterSubsystem *Counters = GameInstance ? GameInstance->GetSubsystem<UHiddenCounterSubsystem>() : nullptr;
		UE_LOG(LogWoR, Display, TEXT("[HiddenCounter]\n%s"), Counters ? *Counters->GetCountersString() : TEXT("no counter subsystem"));
	}));

static FAutoConsoleCommandWithWorldAndArgs GCounterAddCommand(
	TEXT("wor.Counter.Add"),
	TEXT("B-379: wor.Counter.Add <Counter> <Amount> - add to a hidden counter's all-characters total (to test a milestone)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
	{
		const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		UHiddenCounterSubsystem *Counters = GameInstance ? GameInstance->GetSubsystem<UHiddenCounterSubsystem>() : nullptr;
		if (!Counters || Args.Num() < 2)
		{
			UE_LOG(LogWoR, Display, TEXT("[HiddenCounter] usage: wor.Counter.Add <Counter> <Amount>"));
			return;
		}
		Counters->Add(FName(*Args[0]), nullptr, FCString::Atoi(*Args[1]));
		UE_LOG(LogWoR, Display, TEXT("[HiddenCounter]\n%s"), *Counters->GetCountersString());
	}));
#endif
