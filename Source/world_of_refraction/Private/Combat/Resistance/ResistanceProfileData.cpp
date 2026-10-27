// ResistanceProfileData.cpp

#include "Combat/Resistance/ResistanceProfileData.h"
#include "UObject/SoftObjectPath.h"
#include "WoRLog.h"

namespace
{
	// TODO: cook rule for this path (Asset Manager primary asset type) before the first packaged build.
	const TCHAR *RESISTANCE_PROFILE_ASSET_PATH = TEXT("/Game/Data/Combat/DA_ResistanceProfile_Class.DA_ResistanceProfile_Class");

	/** Every ESpellElement a Caster row exists for in the C++ table. */
	const ESpellElement CASTER_ELEMENTS[] = {
		ESpellElement::Fire, ESpellElement::Water, ESpellElement::Earth, ESpellElement::Wind, ESpellElement::Light,
		ESpellElement::Darkness, ESpellElement::Lightning, ESpellElement::Void, ESpellElement::Reality};

	FString RowString(const ClassInnateResistanceTable::FResistanceRow &R)
	{
		return FString::Printf(TEXT("Fi %+.0f Wa %+.0f Ea %+.0f Wi %+.0f Li %+.0f Da %+.0f Lt %+.0f Vo %+.0f Re %+.0f | Sl %+.0f Pi %+.0f Im %+.0f"),
							   R.Fire, R.Water, R.Earth, R.Wind, R.Light, R.Darkness, R.Lightning, R.Void, R.Reality,
							   R.Slash, R.Pierce, R.Impact);
	}

	bool RowsEqual(const ClassInnateResistanceTable::FResistanceRow &A, const ClassInnateResistanceTable::FResistanceRow &B)
	{
		return A.Fire == B.Fire && A.Water == B.Water && A.Earth == B.Earth && A.Wind == B.Wind && A.Light == B.Light &&
			   A.Darkness == B.Darkness && A.Lightning == B.Lightning && A.Void == B.Void && A.Reality == B.Reality &&
			   A.Slash == B.Slash && A.Pierce == B.Pierce && A.Impact == B.Impact;
	}

	FString ElementName(ESpellElement Element)
	{
		FString Name = UEnum::GetValueAsString(Element);
		Name.RemoveFromStart(TEXT("ESpellElement::"));
		return Name;
	}
}

ClassInnateResistanceTable::FResistanceRow FResistanceProfileRow::ToTableRow() const
{
	return {Fire, Water, Earth, Wind, Light, Darkness, Lightning, Void, Reality, Slash, Pierce, Impact};
}

FResistanceProfileRow FResistanceProfileRow::FromTableRow(const ClassInnateResistanceTable::FResistanceRow &Row)
{
	FResistanceProfileRow Out;
	Out.Fire = Row.Fire;
	Out.Water = Row.Water;
	Out.Earth = Row.Earth;
	Out.Wind = Row.Wind;
	Out.Light = Row.Light;
	Out.Darkness = Row.Darkness;
	Out.Lightning = Row.Lightning;
	Out.Void = Row.Void;
	Out.Reality = Row.Reality;
	Out.Slash = Row.Slash;
	Out.Pierce = Row.Pierce;
	Out.Impact = Row.Impact;
	return Out;
}

UResistanceProfileData::UResistanceProfileData()
{
	FillFromCodeTable();
}

void UResistanceProfileData::ResetToCodeTable()
{
	Modify();
	FillFromCodeTable();
}

void UResistanceProfileData::FillFromCodeTable()
{
	using namespace ClassInnateResistanceTable;
	BrokenDarkness = FResistanceProfileRow::FromTableRow(ResolveRow(ECharacterClass::Caster, ESpellElement::None, true));
	GenericClass = FResistanceProfileRow::FromTableRow(ResolveRow(ECharacterClass::Generic, ESpellElement::None, false));
	ResonatorClass = FResistanceProfileRow::FromTableRow(ResolveRow(ECharacterClass::Resonator, ESpellElement::None, false));
	CasterByInnateElement.Reset();
	for (const ESpellElement Element : CASTER_ELEMENTS)
	{
		CasterByInnateElement.Add(Element, FResistanceProfileRow::FromTableRow(GetElementRow(Element)));
	}
}

ClassInnateResistanceTable::FResistanceRow UResistanceProfileData::ResolveRow(ECharacterClass Class, ESpellElement InnateElement, bool bIsBrokenDarkness) const
{
	if (bIsBrokenDarkness)
	{
		return BrokenDarkness.ToTableRow();
	}
	if (Class == ECharacterClass::Generic)
	{
		return GenericClass.ToTableRow();
	}
	if (Class == ECharacterClass::Resonator)
	{
		return ResonatorClass.ToTableRow();
	}
	const FResistanceProfileRow *Row = CasterByInnateElement.Find(InnateElement);
	return Row ? Row->ToTableRow() : FResistanceProfileRow().ToTableRow();
}

const UResistanceProfileData *UResistanceProfileData::GetActive()
{
	// Weak: a collected asset is simply loaded again. Missing is remembered for the session, so a
	// project without the asset doesn't search the disk on every hit.
	static TWeakObjectPtr<const UResistanceProfileData> Cached;
	static bool bMissing = false;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}
	if (bMissing)
	{
		return nullptr;
	}
	const UResistanceProfileData *Loaded = Cast<UResistanceProfileData>(FSoftObjectPath(RESISTANCE_PROFILE_ASSET_PATH).TryLoad());
	if (!Loaded)
	{
		bMissing = true;
		UE_LOG(LogWoRCombat, Log, TEXT("[ResistanceProfile] No asset at %s; using the C++ class table"), RESISTANCE_PROFILE_ASSET_PATH);
		return nullptr;
	}
	Cached = Loaded;
	return Loaded;
}

FString UResistanceProfileData::GetResistanceProfileString() const
{
	FString Out = FString::Printf(TEXT("=== Resistance profiles (%s) ===\n"), *GetName());
	Out += FString::Printf(TEXT("  Broken Darkness  %s\n"), *RowString(BrokenDarkness.ToTableRow()));
	Out += FString::Printf(TEXT("  Generic          %s\n"), *RowString(GenericClass.ToTableRow()));
	Out += FString::Printf(TEXT("  Resonator        %s\n"), *RowString(ResonatorClass.ToTableRow()));
	for (const ESpellElement Element : CASTER_ELEMENTS)
	{
		Out += FString::Printf(TEXT("  Caster %-9s %s\n"), *ElementName(Element),
							   *RowString(ResolveRow(ECharacterClass::Caster, Element, false)));
	}
	return Out;
}

void UResistanceProfileData::PrintProfiles() const
{
	UE_LOG(LogWoRCombat, Log, TEXT("%s"), *GetResistanceProfileString());
}

void UResistanceProfileData::CompareWithCodeTable() const
{
	using namespace ClassInnateResistanceTable;
	int32 Differ = 0;
	auto Check = [&Differ](const FString &Label, const FResistanceRow &Asset, const FResistanceRow &Code)
	{
		if (!RowsEqual(Asset, Code))
		{
			++Differ;
			UE_LOG(LogWoRCombat, Warning, TEXT("[ResistanceProfile] %s differs\n  asset %s\n  code  %s"), *Label, *RowString(Asset), *RowString(Code));
		}
	};
	Check(TEXT("Broken Darkness"), ResolveRow(ECharacterClass::Caster, ESpellElement::None, true), ClassInnateResistanceTable::ResolveRow(ECharacterClass::Caster, ESpellElement::None, true));
	Check(TEXT("Generic"), ResolveRow(ECharacterClass::Generic, ESpellElement::None, false), ClassInnateResistanceTable::ResolveRow(ECharacterClass::Generic, ESpellElement::None, false));
	Check(TEXT("Resonator"), ResolveRow(ECharacterClass::Resonator, ESpellElement::None, false), ClassInnateResistanceTable::ResolveRow(ECharacterClass::Resonator, ESpellElement::None, false));
	for (const ESpellElement Element : CASTER_ELEMENTS)
	{
		Check(FString::Printf(TEXT("Caster %s"), *ElementName(Element)), ResolveRow(ECharacterClass::Caster, Element, false), GetElementRow(Element));
	}
	UE_LOG(LogWoRCombat, Log, TEXT("[ResistanceProfile] %s: %d profile(s) differ from the C++ table"), *GetName(), Differ);
}
