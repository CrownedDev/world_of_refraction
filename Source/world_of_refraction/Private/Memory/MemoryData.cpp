// MemoryData.cpp

#include "Memory/MemoryData.h"

#include "Character/CharacterData.h"
#include "WoRLog.h"

FName UMemoryData::GetMemoryId() const
{
	return MemoryId.IsNone() ? GetFName() : MemoryId;
}

EMemoryDepth UMemoryData::GetDepthFor(const UCharacterData *Character) const
{
	for (const TPair<TObjectPtr<UCharacterData>, EMemoryDepth> &Entry : CharacterDepth)
	{
		if (Entry.Key == Character)
		{
			return Entry.Value;
		}
	}
	return EMemoryDepth::None;
}

FString UMemoryData::GetMemoryString() const
{
	TArray<FString> Depths;
	for (const TPair<TObjectPtr<UCharacterData>, EMemoryDepth> &Entry : CharacterDepth)
	{
		Depths.Add(FString::Printf(TEXT("%s for %s"), *UEnum::GetDisplayValueAsText(Entry.Value).ToString(),
								   Entry.Key ? *Entry.Key->Name : TEXT("<none>")));
	}
	TArray<FString> Modifiers;
	for (const FMemoryBossModifier &Modifier : BossModifiers)
	{
		Modifiers.Add(FString::Printf(TEXT("%s %s"), *UEnum::GetDisplayValueAsText(Modifier.Kind).ToString(),
									  Modifier.Boss ? *Modifier.Boss->Name : TEXT("<no boss>")));
	}
	return FString::Printf(TEXT("Memory %s '%s' | depth: %s | boss: %s"), *GetMemoryId().ToString(),
						   *Title.ToString(), Depths.IsEmpty() ? TEXT("none") : *FString::Join(Depths, TEXT(", ")),
						   Modifiers.IsEmpty() ? TEXT("none") : *FString::Join(Modifiers, TEXT(", ")));
}

void UMemoryData::PrintMemory() const
{
	UE_LOG(LogWoR, Log, TEXT("%s"), *GetMemoryString());
}
