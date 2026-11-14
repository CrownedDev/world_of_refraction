// LoadoutV2SkillsModel.cpp

#include "UI/Menus/LoadoutV2SkillsModel.h"

#include "Combat/TargetType.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Effects/EffectText.h"
#include "WoRGameNames.h"

#define LOCTEXT_NAMESPACE "WoRLoadoutV2Skills"

namespace
{
	ELoadoutScreenPlace ToScreenPlace(ELoadoutV2Place Place)
	{
		switch (Place)
		{
		case ELoadoutV2Place::Primary:
			return ELoadoutScreenPlace::Primary;
		case ELoadoutV2Place::Secondary:
			return ELoadoutScreenPlace::Secondary;
		case ELoadoutV2Place::Adapter:
			return ELoadoutScreenPlace::Adapter;
		case ELoadoutV2Place::Seat:
		default:
			return ELoadoutScreenPlace::None;
		}
	}

	/** The Skills page's tab for a Weapons tab entry: its weapon's or adapter's list. */
	int32 FindTab(const FLoadoutSkillsPage &Source, const FLoadoutV2Entry &Entry)
	{
		const ELoadoutScreenPlace Place = ToScreenPlace(Entry.Place);
		for (int32 Index = 0; Index < Source.Tabs.Num(); ++Index)
		{
			const FLoadoutSkillTab &Tab = Source.Tabs[Index];
			if (Tab.List == ELoadoutSkillList::Innate || Tab.Gear.Place != Place)
			{
				continue;
			}
			if (Place != ELoadoutScreenPlace::Adapter || Tab.Gear.AdapterIndex == Entry.AdapterIndex)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	void AddDetail(TArray<FLoadoutV2DetailLine> &Lines, ELoadoutV2DetailKind Kind, const FText &Label, const FText &Value, int32 Depth = 0)
	{
		FLoadoutV2DetailLine Line;
		Line.Kind = Kind;
		Line.Label = Label;
		Line.Value = Value;
		Line.Depth = Depth;
		Lines.Add(MoveTemp(Line));
	}

	FText GetTargetsText(const USkillDataBase &Skill)
	{
		switch (Skill.TargetCount)
		{
		case ETargetCount::All:
			return LOCTEXT("TargetsAll", "All");
		case ETargetCount::Double:
			return LOCTEXT("TargetsTwo", "Two");
		case ETargetCount::Single:
		default:
			return LOCTEXT("TargetsOne", "One");
		}
	}
}

FLoadoutV2SkillsPage WoRLoadoutV2Skills::Build(const FLoadoutSkillsPage &Source, const FLoadoutV2WeaponsPage &Weapons)
{
	FLoadoutV2SkillsPage Page;
	Page.Source = Source;
	for (const FLoadoutV2Entry &Weapon : Weapons.Entries)
	{
		if (Weapon.Kind == ELoadoutV2EntryKind::Evolution)
		{
			continue; // the character's evolution has no skill slots
		}
		const int32 Tab = FindTab(Source, Weapon);
		if (Tab == INDEX_NONE)
		{
			continue;
		}
		FLoadoutV2SkillEntry Entry;
		Entry.Name = Weapon.Name;
		Entry.Tabs.Add(Tab);
		Entry.Filled = Source.Tabs[Tab].Filled;
		Entry.Total = Source.Tabs[Tab].Rows.Num();
		Entry.Header = FText::Format(LOCTEXT("EntryHeader", "{0} \u00B7 {1}/{2} skills"), Weapon.Name, Entry.Filled, Entry.Total);
		Page.Entries.Add(MoveTemp(Entry));
	}
	FLoadoutV2SkillEntry Innate;
	Innate.bInnate = true;
	Innate.Name = LOCTEXT("InnateSpells", "Innate spells");
	for (int32 Index = 0; Index < Source.Tabs.Num(); ++Index)
	{
		if (Source.Tabs[Index].List == ELoadoutSkillList::Innate)
		{
			Innate.Tabs.Add(Index);
			Innate.Filled += Source.Tabs[Index].Filled;
			Innate.Total += Source.Tabs[Index].Rows.Num();
		}
	}
	if (!Innate.Tabs.IsEmpty())
	{
		Innate.Header = FText::Format(LOCTEXT("InnateHeader", "Innate spells \u00B7 {0}/{1} points"), Source.Points, Source.MaxPoints);
		Page.Entries.Add(MoveTemp(Innate));
	}
	return Page;
}

TArray<FLoadoutV2SkillRowRef> WoRLoadoutV2Skills::GetRows(const FLoadoutV2SkillsPage &Page, int32 Entry)
{
	TArray<FLoadoutV2SkillRowRef> Rows;
	if (!Page.Entries.IsValidIndex(Entry))
	{
		return Rows;
	}
	for (const int32 Tab : Page.Entries[Entry].Tabs)
	{
		if (!Page.Source.Tabs.IsValidIndex(Tab))
		{
			continue;
		}
		for (int32 Row = 0; Row < Page.Source.Tabs[Tab].Rows.Num(); ++Row)
		{
			Rows.Add({Tab, Row});
		}
	}
	return Rows;
}

const FLoadoutScreenRow *WoRLoadoutV2Skills::FindRow(const FLoadoutV2SkillsPage &Page, const FLoadoutV2SkillRowRef &Ref)
{
	return Page.Source.Tabs.IsValidIndex(Ref.Tab) && Page.Source.Tabs[Ref.Tab].Rows.IsValidIndex(Ref.Row) ? &Page.Source.Tabs[Ref.Tab].Rows[Ref.Row] : nullptr;
}

TArray<FLoadoutV2DetailLine> WoRLoadoutV2Skills::BuildSkillDetails(const USkillDataBase *Skill, bool bSimple)
{
	TArray<FLoadoutV2DetailLine> Lines;
	if (!Skill)
	{
		return Lines;
	}
	// TODO: B-127: the skill's Name becomes FText on the asset.
	AddDetail(Lines, ELoadoutV2DetailKind::Piece, FText::FromString(Skill->Name), WoRGameNames::GetTierLetter(Skill->Tier));
	AddDetail(Lines, ELoadoutV2DetailKind::Stat, LOCTEXT("EP", "EP"), FText::AsNumber(Skill->BaseEnergyCost));
	if (Skill->BaseDamage > 0)
	{
		AddDetail(Lines, ELoadoutV2DetailKind::Stat, LOCTEXT("Power", "Power"), FText::AsNumber(Skill->BaseDamage));
	}
	AddDetail(Lines, ELoadoutV2DetailKind::Stat, LOCTEXT("Targets", "Targets"), GetTargetsText(*Skill));
	const TArray<FSkillEffect> Effects = Skill->GetAllEffects();
	if (bSimple)
	{
		for (const FText &Short : EffectText::BuildShortForm(Effects, EEffectTextSource::Skill))
		{
			AddDetail(Lines, ELoadoutV2DetailKind::Effect, FText::GetEmpty(), Short);
		}
		return Lines;
	}
	for (const FEffectTextLine &Line : EffectText::BuildTree(Effects, EEffectTextSource::Skill))
	{
		AddDetail(Lines, ELoadoutV2DetailKind::Effect, FText::GetEmpty(), Line.Text, Line.Depth);
	}
	return Lines;
}

FString WoRLoadoutV2Skills::GetSkillsString(const FLoadoutV2SkillsPage &Page)
{
	TArray<FString> Lines;
	for (int32 Entry = 0; Entry < Page.Entries.Num(); ++Entry)
	{
		TArray<FString> Rows;
		for (const FLoadoutV2SkillRowRef &Ref : GetRows(Page, Entry))
		{
			const FLoadoutScreenRow *Row = FindRow(Page, Ref);
			Rows.Add(Row && Row->Skill ? Row->Name.ToString() : TEXT("(empty)"));
		}
		Lines.Add(FString::Printf(TEXT("%s: %s"), *Page.Entries[Entry].Header.ToString(), *FString::Join(Rows, TEXT(", "))));
	}
	return FString::Printf(TEXT("[LoadoutV2 Skills] %s"), Lines.IsEmpty() ? TEXT("no entries") : *FString::Join(Lines, TEXT(" | ")));
}

#undef LOCTEXT_NAMESPACE
