// FieldSkillUnlockSubsystem.cpp

#include "Field/FieldSkillUnlockSubsystem.h"

#include "Pool/PoolSubsystem.h"
#include "Skills/Definitions/SpellData.h"
#include "WoRLog.h"

namespace FieldUnlockLog
{
	const TCHAR *const TAG = TEXT("[FieldUnlock]");
}

namespace
{
	FString ElementName(ESpellElement Element)
	{
		return StaticEnum<ESpellElement>()->GetNameStringByValue(static_cast<int64>(Element));
	}

	constexpr int32 ELEMENT_KEY_BITS = 8;
	constexpr uint16 ELEMENT_KEY_MASK = 0xFF;
}

void UFieldSkillUnlockSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	if (UPoolSubsystem *Pool = Collection.InitializeDependency<UPoolSubsystem>())
	{
		Pool->OnPoolSkillsChanged.AddDynamic(this, &UFieldSkillUnlockSubsystem::HandlePoolSkillsChanged);
	}
}

void UFieldSkillUnlockSubsystem::Deinitialize()
{
	if (UPoolSubsystem *Pool = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPoolSubsystem>() : nullptr)
	{
		Pool->OnPoolSkillsChanged.RemoveDynamic(this, &UFieldSkillUnlockSubsystem::HandlePoolSkillsChanged);
	}
	Super::Deinitialize();
}

void UFieldSkillUnlockSubsystem::BroadcastPendingUnlocks()
{
	// After the caller's loop: a listener may change the store's skills, which the scan iterates.
	const TArray<TPair<EFieldMovementType, ESpellElement>> Unlocked = MoveTemp(PendingUnlocks);
	PendingUnlocks.Reset();
	for (const TPair<EFieldMovementType, ESpellElement> &Unlock : Unlocked)
	{
		OnFieldSkillUnlocked.Broadcast(Unlock.Key, Unlock.Value);
	}
}

uint16 UFieldSkillUnlockSubsystem::MakeKey(EFieldAura Aura, ESpellElement Element)
{
	return static_cast<uint16>((static_cast<uint16>(Aura) << ELEMENT_KEY_BITS) | static_cast<uint16>(Element));
}

int32 UFieldSkillUnlockSubsystem::GetBestRank(EFieldAura Aura, ESpellElement Element) const
{
	const int32 *Rank = BestRanks.Find(MakeKey(Aura, Element));
	return Rank ? *Rank : 0;
}

bool UFieldSkillUnlockSubsystem::IsUnlocked(EFieldMovementType Type, ESpellElement Element) const
{
	return FieldSkillRules::IsUnlockedByRanks(Type, Element, [this, Element](EFieldAura Aura)
											  { return GetBestRank(Aura, Element); });
}

TArray<EFieldMovementType> UFieldSkillUnlockSubsystem::GetUnlockedTypes(ESpellElement Element) const
{
	TArray<EFieldMovementType> Types;
	const UEnum *TypeEnum = StaticEnum<EFieldMovementType>();
	for (int32 Index = 0; Index < TypeEnum->NumEnums() - 1; ++Index) // the last entry is the generated _MAX
	{
		const EFieldMovementType Type = static_cast<EFieldMovementType>(TypeEnum->GetValueByIndex(Index));
		if (IsUnlocked(Type, Element))
		{
			Types.Add(Type);
		}
	}
	return Types;
}

bool UFieldSkillUnlockSubsystem::RaiseRank(EFieldAura Aura, ESpellElement Element, int32 Rank)
{
	if (Aura == EFieldAura::None || !FieldSkillRules::IsElemental(Element) || Rank <= GetBestRank(Aura, Element))
	{
		return false;
	}
	const TArray<EFieldMovementType> Before = GetUnlockedTypes(Element);
	BestRanks.Add(MakeKey(Aura, Element), Rank);
	UE_LOG(LogWoR, Log, TEXT("%s %s %s aura reached rank %d"), FieldUnlockLog::TAG, *ElementName(Element),
		   *FieldSkillRules::AuraName(Aura), Rank);
	for (const EFieldMovementType Type : GetUnlockedTypes(Element))
	{
		if (!Before.Contains(Type))
		{
			UE_LOG(LogWoR, Log, TEXT("%s %s %s unlocked for every %s source"), FieldUnlockLog::TAG, *ElementName(Element),
				   *FieldSkillRules::TypeName(Type), *ElementName(Element));
			PendingUnlocks.Emplace(Type, Element);
		}
	}
	return true;
}

bool UFieldSkillUnlockSubsystem::RecordSpellTier(const USpellData *Spell, EItemTier Tier)
{
	const bool bRaised = Spell && RaiseRank(Spell->FieldAura, Spell->Element, FieldSkillRules::TierToRank(Tier));
	BroadcastPendingUnlocks();
	return bRaised;
}

int32 UFieldSkillUnlockSubsystem::RecordFromPool()
{
	const UPoolSubsystem *Pool = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPoolSubsystem>() : nullptr;
	if (!Pool)
	{
		return 0;
	}
	int32 Raised = 0;
	for (const TPair<FName, FPoolSkills> &Character : Pool->GetCharacterSkills())
	{
		for (const FSpellInstance &Instance : Character.Value.Spells)
		{
			Raised += Instance.Spell && RaiseRank(Instance.Spell->FieldAura, Instance.Spell->Element, FieldSkillRules::TierToRank(Instance.Tier)) ? 1 : 0;
		}
	}
	for (const FSpellInstance &Instance : Pool->GetOwnedSpells())
	{
		Raised += Instance.Spell && RaiseRank(Instance.Spell->FieldAura, Instance.Spell->Element, FieldSkillRules::TierToRank(Instance.Tier)) ? 1 : 0;
	}
	BroadcastPendingUnlocks();
	return Raised;
}

void UFieldSkillUnlockSubsystem::HandlePoolSkillsChanged(FName CharacterKey)
{
	RecordFromPool();
}

bool UFieldSkillUnlockSubsystem::UnlockForDebug(EFieldMovementType Type, ESpellElement Element)
{
	if (!FieldSkillRules::NeedsUnlock(Type))
	{
		return true;
	}
	if (!FieldSkillRules::CanElementUnlock(Type, Element))
	{
		return false;
	}
	for (const EFieldAura Aura : FieldSkillRules::GetUnlockAuras(Type))
	{
		RaiseRank(Aura, Element, FieldSkillRules::GetUnlockRank(Type));
	}
	BroadcastPendingUnlocks();
	return IsUnlocked(Type, Element);
}

FSavedFieldUnlocks UFieldSkillUnlockSubsystem::Capture() const
{
	FSavedFieldUnlocks Saved;
	for (const TPair<uint16, int32> &Entry : BestRanks)
	{
		FSavedFieldAuraRank &Row = Saved.Ranks.AddDefaulted_GetRef();
		Row.Aura = static_cast<uint8>(Entry.Key >> ELEMENT_KEY_BITS);
		Row.Element = static_cast<uint8>(Entry.Key & ELEMENT_KEY_MASK);
		Row.Rank = Entry.Value;
	}
	return Saved;
}

void UFieldSkillUnlockSubsystem::Restore(const FSavedFieldUnlocks &Saved)
{
	BestRanks.Reset();
	const UEnum *AuraEnum = StaticEnum<EFieldAura>();
	const UEnum *ElementEnum = StaticEnum<ESpellElement>();
	for (const FSavedFieldAuraRank &Row : Saved.Ranks)
	{
		if (!AuraEnum->IsValidEnumValue(Row.Aura) || !ElementEnum->IsValidEnumValue(Row.Element))
		{
			UE_LOG(LogWoR, Warning, TEXT("%s skipped a saved rank with an unknown aura %d or element %d"),
				   FieldUnlockLog::TAG, Row.Aura, Row.Element);
			continue;
		}
		BestRanks.Add(MakeKey(static_cast<EFieldAura>(Row.Aura), static_cast<ESpellElement>(Row.Element)), Row.Rank);
	}
	const int32 Raised = RecordFromPool();
	UE_LOG(LogWoR, Log, TEXT("%s restored %d aura rank(s), %d raised from the store"), FieldUnlockLog::TAG,
		   Saved.Ranks.Num(), Raised);
}

FString UFieldSkillUnlockSubsystem::GetUnlocksString() const
{
	TArray<FString> Ranks;
	TArray<FString> Unlocked;
	const UEnum *ElementEnum = StaticEnum<ESpellElement>();
	for (int32 Index = 0; Index < ElementEnum->NumEnums() - 1; ++Index)
	{
		const ESpellElement Element = static_cast<ESpellElement>(ElementEnum->GetValueByIndex(Index));
		if (!FieldSkillRules::IsElemental(Element))
		{
			continue;
		}
		for (const EFieldAura Aura : {EFieldAura::Elemental, EFieldAura::Enhancement, EFieldAura::Manipulation})
		{
			if (const int32 Rank = GetBestRank(Aura, Element))
			{
				Ranks.Add(FString::Printf(TEXT("%s %s rank %d"), *FieldSkillRules::AuraName(Aura), *ElementName(Element), Rank));
			}
		}
		for (const EFieldMovementType Type : GetUnlockedTypes(Element))
		{
			if (FieldSkillRules::NeedsUnlock(Type))
			{
				Unlocked.Add(FString::Printf(TEXT("%s %s"), *ElementName(Element), *FieldSkillRules::TypeName(Type)));
			}
		}
	}
	return FString::Printf(TEXT("ranks: %s || unlocked: %s"),
						   Ranks.Num() ? *FString::Join(Ranks, TEXT(" | ")) : TEXT("none"),
						   Unlocked.Num() ? *FString::Join(Unlocked, TEXT(", ")) : TEXT("none (the dash is everyone's)"));
}
