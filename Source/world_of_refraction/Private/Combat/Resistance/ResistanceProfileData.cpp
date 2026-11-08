// ResistanceProfileData.cpp

#include "Combat/Resistance/ResistanceProfileData.h"
#include "Combat/Resistance/ResistanceColumns.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/World.h"
#include "WoRLog.h"

namespace
{
	// B-278: cooked by the Asset Manager's ResistanceProfileData rule (DefaultGame.ini, AlwaysCook).
	const TCHAR *RESISTANCE_PROFILE_ASSET_PATH = TEXT("/Game/Data/Combat/DA_ResistanceProfile_Class.DA_ResistanceProfile_Class");

	/** Every ESpellElement a Caster row exists for in the C++ table. */
	const ESpellElement PROFILE_CASTER_ELEMENTS[] = {
		ESpellElement::Fire, ESpellElement::Water, ESpellElement::Earth, ESpellElement::Wind, ESpellElement::Light,
		ESpellElement::Darkness, ESpellElement::Lightning, ESpellElement::Void, ESpellElement::Reality};

	FString ProfileRowString(const ClassInnateResistanceTable::FResistanceRow &R)
	{
		return ResistanceColumns::ToString(R);
	}

	bool ProfileRowsEqual(const ClassInnateResistanceTable::FResistanceRow &A, const ClassInnateResistanceTable::FResistanceRow &B)
	{
		return ResistanceColumns::Equal(A, B);
	}

	FString ProfileElementName(ESpellElement Element)
	{
		FString Name = UEnum::GetValueAsString(Element);
		Name.RemoveFromStart(TEXT("ESpellElement::"));
		return Name;
	}
}

ClassInnateResistanceTable::FResistanceRow FResistanceProfileRow::ToTableRow() const
{
	return ResistanceColumns::Convert<ClassInnateResistanceTable::FResistanceRow>(*this);
}

FResistanceProfileRow FResistanceProfileRow::FromTableRow(const ClassInnateResistanceTable::FResistanceRow &Row)
{
	return ResistanceColumns::Convert<FResistanceProfileRow>(Row);
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
	// Qualified: the member ResolveRow would read these rows before they are filled.
	using ClassInnateResistanceTable::GetElementRow;
	BrokenDarkness = FResistanceProfileRow::FromTableRow(ClassInnateResistanceTable::ResolveRow(ECharacterClass::Caster, ESpellElement::None, true));
	GenericClass = FResistanceProfileRow::FromTableRow(ClassInnateResistanceTable::ResolveRow(ECharacterClass::Generic, ESpellElement::None, false));
	ResonatorClass = FResistanceProfileRow::FromTableRow(ClassInnateResistanceTable::ResolveRow(ECharacterClass::Resonator, ESpellElement::None, false));
	CasterByInnateElement.Reset();
	for (const ESpellElement Element : PROFILE_CASTER_ELEMENTS)
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
	// Missing is remembered until the next world starts (so an asset created in the editor counts
	// from the next PIE), and a project without the asset doesn't search the disk on every hit.
	static TWeakObjectPtr<const UResistanceProfileData> Cached;
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
	// Outside the editor keep it loaded, so a garbage collection never forces a load mid-fight.
	// In the editor it stays weak so the asset can still be renamed or deleted.
	if (!GIsEditor)
	{
		const_cast<UResistanceProfileData *>(Loaded)->AddToRoot();
	}
	Cached = Loaded;
	return Loaded;
}

FString UResistanceProfileData::GetResistanceProfileString() const
{
	FString Out = FString::Printf(TEXT("=== Resistance profiles (%s) ===\n"), *GetName());
	Out += FString::Printf(TEXT("  Broken Darkness  %s\n"), *ProfileRowString(BrokenDarkness.ToTableRow()));
	Out += FString::Printf(TEXT("  Generic          %s\n"), *ProfileRowString(GenericClass.ToTableRow()));
	Out += FString::Printf(TEXT("  Resonator        %s\n"), *ProfileRowString(ResonatorClass.ToTableRow()));
	for (const ESpellElement Element : PROFILE_CASTER_ELEMENTS)
	{
		Out += FString::Printf(TEXT("  Caster %-9s %s\n"), *ProfileElementName(Element),
							   *ProfileRowString(ResolveRow(ECharacterClass::Caster, Element, false)));
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
		if (!ProfileRowsEqual(Asset, Code))
		{
			++Differ;
			UE_LOG(LogWoRCombat, Warning, TEXT("[ResistanceProfile] %s differs\n  asset %s\n  code  %s"), *Label, *ProfileRowString(Asset), *ProfileRowString(Code));
		}
	};
	Check(TEXT("Broken Darkness"), ResolveRow(ECharacterClass::Caster, ESpellElement::None, true), ClassInnateResistanceTable::ResolveRow(ECharacterClass::Caster, ESpellElement::None, true));
	Check(TEXT("Generic"), ResolveRow(ECharacterClass::Generic, ESpellElement::None, false), ClassInnateResistanceTable::ResolveRow(ECharacterClass::Generic, ESpellElement::None, false));
	Check(TEXT("Resonator"), ResolveRow(ECharacterClass::Resonator, ESpellElement::None, false), ClassInnateResistanceTable::ResolveRow(ECharacterClass::Resonator, ESpellElement::None, false));
	for (const ESpellElement Element : PROFILE_CASTER_ELEMENTS)
	{
		Check(FString::Printf(TEXT("Caster %s"), *ProfileElementName(Element)), ResolveRow(ECharacterClass::Caster, Element, false), GetElementRow(Element));
	}
	UE_LOG(LogWoRCombat, Log, TEXT("[ResistanceProfile] %s: %d profile(s) differ from the C++ table"), *GetName(), Differ);
}
