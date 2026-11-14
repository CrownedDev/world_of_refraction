// WoRIconTable.cpp

#include "UI/WoRIconTable.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace WoRIconTableBuild
{
	// B-486 makes this asset; cooked by the Asset Manager's WoRIconTable rule (DefaultGame.ini, AlwaysCook).
	const TCHAR *ICON_TABLE_ASSET_PATH = TEXT("/Game/UI/Icons/DA_UIIcon_Table.DA_UIIcon_Table");

	/** The table a spec put in place of the asset. */
	TWeakObjectPtr<const UWoRIconTable> TestTable;

	/** The icon for Key, loaded now; null when the map has none (or an empty reference). */
	template <typename KeyType>
	UTexture2D *Find(const TMap<KeyType, TSoftObjectPtr<UTexture2D>> &Icons, KeyType Key)
	{
		const TSoftObjectPtr<UTexture2D> *Icon = Icons.Find(Key);
		return Icon && !Icon->IsNull() ? Icon->LoadSynchronous() : nullptr;
	}

	/** "Tiers: 5 of 7 set: F-Tier ...; missing: ..." for one map keyed by an enum. */
	template <typename KeyType>
	FString DescribeGroup(const TCHAR *Group, const TMap<KeyType, TSoftObjectPtr<UTexture2D>> &Icons)
	{
		const UEnum *Enum = StaticEnum<KeyType>();
		TArray<FString> Set;
		TArray<FString> Missing;
		for (int32 Index = 0; Enum && Index < Enum->NumEnums() - 1; ++Index) // the last entry is the generated _MAX
		{
			if (Enum->HasMetaData(TEXT("Hidden"), Index))
			{
				continue; // retired values aren't asked for
			}
			const KeyType Key = static_cast<KeyType>(Enum->GetValueByIndex(Index));
			const TSoftObjectPtr<UTexture2D> *Icon = Icons.Find(Key);
			(Icon && !Icon->IsNull() ? Set : Missing).Add(Enum->GetNameStringByIndex(Index));
		}
		return FString::Printf(TEXT("\n  %s: %d of %d set%s%s"), Group, Set.Num(), Set.Num() + Missing.Num(),
							   Set.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(": %s"), *FString::Join(Set, TEXT(" "))),
							   Missing.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("; missing: %s"), *FString::Join(Missing, TEXT(" "))));
	}

	FString DescribeOne(const TCHAR *Name, const TSoftObjectPtr<UTexture2D> &Icon)
	{
		return FString::Printf(TEXT("\n  %s: %s"), Name, Icon.IsNull() ? TEXT("missing") : *Icon.ToSoftObjectPath().ToString());
	}
}

const UWoRIconTable *UWoRIconTable::GetActive()
{
	using namespace WoRIconTableBuild;
	if (TestTable.IsValid())
	{
		return TestTable.Get();
	}
	// Same lookup as UCombatTuningData::GetActive: a miss is remembered until the next world starts; outside the
	// editor the asset stays loaded.
	static TWeakObjectPtr<const UWoRIconTable> Cached;
	static bool bMissing = false;
	static bool bResetBound = false;
	if (!bResetBound)
	{
		bResetBound = true;
		FWorldDelegates::OnPostWorldInitialization.AddLambda([](UWorld *, const UWorld::InitializationValues) { bMissing = false; });
	}
	if (Cached.IsValid())
	{
		return Cached.Get();
	}
	if (bMissing || !UObjectInitialized() || !IsInGameThread())
	{
		return nullptr;
	}
	const UWoRIconTable *Loaded = Cast<UWoRIconTable>(FSoftObjectPath(ICON_TABLE_ASSET_PATH).TryLoad());
	if (!Loaded)
	{
		bMissing = true;
		UE_LOG(LogWoRUI, Log, TEXT("[Icons] No icon table at %s; the screens keep their text"), ICON_TABLE_ASSET_PATH);
		return nullptr;
	}
	if (!GIsEditor)
	{
		const_cast<UWoRIconTable *>(Loaded)->AddToRoot();
	}
	Cached = Loaded;
	return Loaded;
}

UTexture2D *UWoRIconTable::GetElementBadge(ESpellElement Element) const
{
	return WoRIconTableBuild::Find(ElementBadges, Element);
}

UTexture2D *UWoRIconTable::GetEmptySocket() const
{
	return EmptySocket.IsNull() ? nullptr : EmptySocket.LoadSynchronous();
}

UTexture2D *UWoRIconTable::GetStoneSocket() const
{
	return StoneSocket.IsNull() ? nullptr : StoneSocket.LoadSynchronous();
}

UTexture2D *UWoRIconTable::GetTierBadge(EItemTier Tier) const
{
	return WoRIconTableBuild::Find(TierBadges, Tier);
}

UTexture2D *UWoRIconTable::GetWeaponType(EWeaponType Type) const
{
	return WoRIconTableBuild::Find(WeaponTypes, Type);
}

UTexture2D *UWoRIconTable::GetEffectKind(ESkillEffectType Type) const
{
	return WoRIconTableBuild::Find(EffectKinds, Type);
}

FString UWoRIconTable::GetIconTableString() const
{
	using namespace WoRIconTableBuild;
	FString Out = FString::Printf(TEXT("Icon table %s:"), *GetPathName());
	Out += DescribeGroup(TEXT("Element badges"), ElementBadges);
	Out += DescribeOne(TEXT("Empty socket"), EmptySocket);
	Out += DescribeOne(TEXT("Stone socket"), StoneSocket);
	Out += DescribeGroup(TEXT("Tier badges"), TierBadges);
	Out += DescribeGroup(TEXT("Weapon types"), WeaponTypes);
	Out += DescribeGroup(TEXT("Effect kinds"), EffectKinds);
	return Out;
}

void UWoRIconTable::PrintIcons() const
{
	UE_LOG(LogWoRUI, Display, TEXT("[Icons] %s"), *GetIconTableString());
}

UTexture2D *WoRIcons::GetElementBadge(ESpellElement Element)
{
	const UWoRIconTable *Table = UWoRIconTable::GetActive();
	return Table ? Table->GetElementBadge(Element) : nullptr;
}

UTexture2D *WoRIcons::GetEmptySocket()
{
	const UWoRIconTable *Table = UWoRIconTable::GetActive();
	return Table ? Table->GetEmptySocket() : nullptr;
}

UTexture2D *WoRIcons::GetStoneSocket()
{
	const UWoRIconTable *Table = UWoRIconTable::GetActive();
	return Table ? Table->GetStoneSocket() : nullptr;
}

UTexture2D *WoRIcons::GetTierBadge(EItemTier Tier)
{
	const UWoRIconTable *Table = UWoRIconTable::GetActive();
	return Table ? Table->GetTierBadge(Tier) : nullptr;
}

UTexture2D *WoRIcons::GetWeaponType(EWeaponType Type)
{
	const UWoRIconTable *Table = UWoRIconTable::GetActive();
	return Table ? Table->GetWeaponType(Type) : nullptr;
}

UTexture2D *WoRIcons::GetEffectKind(ESkillEffectType Type)
{
	const UWoRIconTable *Table = UWoRIconTable::GetActive();
	return Table ? Table->GetEffectKind(Type) : nullptr;
}

void WoRIcons::SetActiveForTests(const UWoRIconTable *Table)
{
	WoRIconTableBuild::TestTable = Table;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommand GIconTablePrintCommand(
	TEXT("wor.UI.Icons.Print"),
	TEXT("B-485: print which icons the active icon table has set (element badges, sockets, tiers, weapon types, effect kinds)."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		if (const UWoRIconTable *Table = UWoRIconTable::GetActive())
		{
			Table->PrintIcons();
		}
		else
		{
			UE_LOG(LogWoRUI, Display, TEXT("[Icons] No icon table (%s): every screen shows text"), WoRIconTableBuild::ICON_TABLE_ASSET_PATH);
		}
	}));
#endif
