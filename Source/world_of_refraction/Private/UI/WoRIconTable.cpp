// WoRIconTable.cpp

#include "UI/WoRIconTable.h"

#include "Engine/Texture2D.h"
#include "Equipment/Crystals/ItemIdentity.h"
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

	bool IsSet(const TSoftObjectPtr<UTexture2D> &Icon)
	{
		return !Icon.IsNull();
	}

	bool IsSet(const FWoREffectIcons &Icons)
	{
		if (!Icons.Default.IsNull())
		{
			return true;
		}
		for (const TPair<ESpellElement, TSoftObjectPtr<UTexture2D>> &Each : Icons.ByElement)
		{
			if (!Each.Value.IsNull())
			{
				return true;
			}
		}
		return false;
	}

	/** "Tiers: 5 of 7 set: F-Tier ...; missing: ..." for one map keyed by an enum. */
	template <typename KeyType, typename ValueType>
	FString DescribeGroup(const TCHAR *Group, const TMap<KeyType, ValueType> &Icons)
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
			const ValueType *Icon = Icons.Find(Key);
			(Icon && IsSet(*Icon) ? Set : Missing).Add(Enum->GetNameStringByIndex(Index));
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

UTexture2D *UWoRIconTable::GetEffectKind(ESkillEffectType Type, ESpellElement Element) const
{
	const FWoREffectIcons *Icons = EffectKinds.Find(Type);
	if (!Icons)
	{
		return nullptr;
	}
	// The element's own variant, else the one for any element.
	UTexture2D *Variant = WoRIconTableBuild::Find(Icons->ByElement, Element);
	return Variant ? Variant : Icons->Default.IsNull() ? nullptr : Icons->Default.LoadSynchronous();
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

UTexture2D *WoRIcons::GetEffectKind(ESkillEffectType Type, ESpellElement Element)
{
	const UWoRIconTable *Table = UWoRIconTable::GetActive();
	return Table ? Table->GetEffectKind(Type, Element) : nullptr;
}

UTexture2D *WoRIcons::Find(const FWoRIcon &Icon)
{
	switch (Icon.Kind)
	{
	case EWoRIconKind::WeaponType:
		return GetWeaponType(Icon.WeaponType);
	case EWoRIconKind::Element:
		return GetElementBadge(Icon.Element);
	case EWoRIconKind::EmptySocket:
	case EWoRIconKind::Adapter: // its frame; the crystal sits on it (FindCentre)
		return GetEmptySocket();
	case EWoRIconKind::Stone:
		return GetStoneSocket();
	case EWoRIconKind::Effect:
		return GetEffectKind(Icon.EffectType, Icon.Element);
	case EWoRIconKind::None:
	default:
		return nullptr;
	}
}

ESkillEffectType WoRIcons::GetItemEffectKind(EItemEffectType Type)
{
	switch (Type)
	{
	case EItemEffectType::Damage:
		return ESkillEffectType::DOT; // the damage crystal (Garnet) burns
	case EItemEffectType::Healing:
		return ESkillEffectType::Heal;
	case EItemEffectType::RestoreHealth:
		return ESkillEffectType::HealthRestore;
	case EItemEffectType::HealingBoost:
		return ESkillEffectType::HealingReceivedBuff; // the Healing Stone (D-252)
	case EItemEffectType::EnergyRestore:
		return ESkillEffectType::EnergyRestore;
	case EItemEffectType::DefyDeath:
		return ESkillEffectType::LastStand;
	case EItemEffectType::GrantBonusTurn:
		return ESkillEffectType::ExtraAction;
	case EItemEffectType::Silence:
		return ESkillEffectType::Silenced;
	case EItemEffectType::Cleanse:
	case EItemEffectType::StatusClear:
		return ESkillEffectType::Cleanse;
	case EItemEffectType::Gamble:
		return ESkillEffectType::RandomSkill;
	case EItemEffectType::BuffRawDamage:
		return ESkillEffectType::RawDamageBuff;
	case EItemEffectType::BuffDefense:
		return ESkillEffectType::DefenseBuff;
	case EItemEffectType::BuffCrit:
		return ESkillEffectType::CritChanceBuff;
	case EItemEffectType::BuffStatusMultiplier:
		return ESkillEffectType::StatusMultiplierBuff;
	case EItemEffectType::BuffEfficiency:
		return ESkillEffectType::EfficiencyBuff;
	case EItemEffectType::BuffMaxHP:
		return ESkillEffectType::MaxHPBuff;
	case EItemEffectType::BuffMaxEP:
		return ESkillEffectType::MaxEnergyBuff;
	case EItemEffectType::BuffSpellDamage:
		return ESkillEffectType::SpellDamageBuff;
	case EItemEffectType::BuffResistance:
		return ESkillEffectType::ResistanceBuff;
	case EItemEffectType::BuffSpellSpeed:
		return ESkillEffectType::SpellSpeedBuff;
	case EItemEffectType::BuffActionSpeed:
		return ESkillEffectType::ActionSpeedBuff;
	case EItemEffectType::BuffLuck:
		return ESkillEffectType::LuckBuff;
	case EItemEffectType::BuffReflex:
		return ESkillEffectType::ReflexBuff;
	default:
		return ESkillEffectType::None; // Repair, a crit-damage buff, the retired Turn Speed buff, None: no icon kind
	}
}

FWoRIcon WoRIcons::MakeItemEffectIcon(const FCrystalId &Item)
{
	FWoRIcon Icon;
	const ESkillEffectType Kind = GetItemEffectKind(ItemIdentity::GetItemEffectType(Item));
	if (Kind != ESkillEffectType::None)
	{
		Icon.Kind = EWoRIconKind::Effect;
		Icon.EffectType = Kind;
		Icon.Element = ItemIdentity::GetElement(Item);
		Icon.Name = StaticEnum<ESkillEffectType>()->GetDisplayNameTextByValue(static_cast<int64>(Kind));
	}
	return Icon;
}

UTexture2D *WoRIcons::FindCentre(const FWoRIcon &Icon)
{
	return Icon.Kind == EWoRIconKind::Adapter && Icon.Element != ESpellElement::None ? GetElementBadge(Icon.Element) : nullptr;
}

FWoRIcon WoRIcons::MakeAdapterIcon(ESpellElement SocketElement, const FText &Name)
{
	FWoRIcon Icon;
	Icon.Kind = EWoRIconKind::Adapter;
	Icon.Element = SocketElement;
	Icon.Name = Name;
	return Icon;
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
