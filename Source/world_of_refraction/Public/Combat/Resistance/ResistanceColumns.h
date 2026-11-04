// ResistanceColumns.h
// B-272 (S7): the 12 resistance columns, read and written by damage type. Four structs carry the same
// named columns: ClassInnateResistanceTable::FResistanceRow, FResistanceProfileRow, FResistanceBonus and
// FResistanceProfileDisplay. Their fields stay as they are (authored and saved); copies, sums, compares
// and column reads go through here. Adding Ice = one field per struct and one case in Field.
// B-358 (D-148, Q-358): one Physical column. The structs keep their Slash / Pierce / Impact fields (authored and saved,
// append-only), but every read of a physical type gives the one Physical value, their average (old values fold in as
// they load), and every write sets all three to it. Slash stands for the Physical column in Types(). The subtypes stay
// on the hit and only pick its break (SB-19 to SB-21).
// Debug: ToString; wor.CompareResistance; spec WoR.Resistance.Columns, WoR.Resistance.OnePhysical.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Resistance/EWoRDamageType.h"

namespace ResistanceColumns
{
	/** B-358: the type that stands for the one Physical column in Types(). */
	constexpr EWoRDamageType PHYSICAL_COLUMN = EWoRDamageType::Slash;

	/** B-358: how many stored fields the Physical column folds (Slash, Pierce, Impact). */
	constexpr float PHYSICAL_FIELD_COUNT = 3.0f;

	/** The columns, in field order: the nine elements and Physical (B-358). None has no column; Ice has none yet
	 *  (D-029, keyed Water). */
	inline const TArray<EWoRDamageType> &Types()
	{
		static const TArray<EWoRDamageType> ColumnTypes = {
			EWoRDamageType::Fire, EWoRDamageType::Water, EWoRDamageType::Earth, EWoRDamageType::Wind,
			EWoRDamageType::Light, EWoRDamageType::Darkness, EWoRDamageType::Lightning, EWoRDamageType::Void,
			EWoRDamageType::Reality, PHYSICAL_COLUMN};
		return ColumnTypes;
	}

	/** Row's stored field for Type, or null when no struct has that column. Const in, const out. Raw: a physical type
	 *  names its own stored field; read and write columns through Get / Set. */
	template <class TRow>
	auto Field(TRow &Row, EWoRDamageType Type) -> decltype(&Row.Fire)
	{
		switch (Type)
		{
		case EWoRDamageType::Fire:      return &Row.Fire;
		case EWoRDamageType::Water:     return &Row.Water;
		case EWoRDamageType::Earth:     return &Row.Earth;
		case EWoRDamageType::Wind:      return &Row.Wind;
		case EWoRDamageType::Light:     return &Row.Light;
		case EWoRDamageType::Darkness:  return &Row.Darkness;
		case EWoRDamageType::Lightning: return &Row.Lightning;
		case EWoRDamageType::Void:      return &Row.Void;
		case EWoRDamageType::Reality:   return &Row.Reality;
		case EWoRDamageType::Slash:     return &Row.Slash;
		case EWoRDamageType::Pierce:    return &Row.Pierce;
		case EWoRDamageType::Impact:    return &Row.Impact;
		case EWoRDamageType::None:
		case EWoRDamageType::Ice:
		default:
			return nullptr;
		}
	}

	/** Row's value for Type; 0 for a type with no column. Any physical type reads the one Physical column: the
	 *  average of the three stored fields (B-358, Q-358). */
	template <class TRow>
	float Get(const TRow &Row, EWoRDamageType Type)
	{
		if (WoRDamageType::IsPhysical(Type))
		{
			return (Row.Slash + Row.Pierce + Row.Impact) / PHYSICAL_FIELD_COUNT;
		}
		const float *Value = ResistanceColumns::Field(Row, Type);
		return Value ? *Value : 0.0f;
	}

	/** Sets Row's column for Type to Value; a physical type sets all three stored fields (B-358). No-op for a type with
	 *  no column. */
	template <class TRow>
	void Set(TRow &Row, EWoRDamageType Type, float Value)
	{
		if (WoRDamageType::IsPhysical(Type))
		{
			Row.Slash = Value;
			Row.Pierce = Value;
			Row.Impact = Value;
			return;
		}
		if (float *Stored = ResistanceColumns::Field(Row, Type))
		{
			*Stored = Value;
		}
	}

	/** A TTo with every column copied from From. */
	template <class TTo, class TFrom>
	TTo Convert(const TFrom &From)
	{
		TTo To{};
		for (const EWoRDamageType Type : Types())
		{
			ResistanceColumns::Set(To, Type, ResistanceColumns::Get(From, Type));
		}
		return To;
	}

	/** Adds every column of Other onto Into (unclamped). */
	template <class TInto, class TOther>
	void Add(TInto &Into, const TOther &Other)
	{
		for (const EWoRDamageType Type : Types())
		{
			ResistanceColumns::Set(Into, Type, ResistanceColumns::Get(Into, Type) + ResistanceColumns::Get(Other, Type));
		}
	}

	template <class TA, class TB>
	bool Equal(const TA &A, const TB &B)
	{
		for (const EWoRDamageType Type : Types())
		{
			if (ResistanceColumns::Get(A, Type) != ResistanceColumns::Get(B, Type))
			{
				return false;
			}
		}
		return true;
	}

	/** One line, percent: "Fi +15 Wa -15 ... | Ph +0" (B-358: the one Physical column). */
	template <class TRow>
	FString ToString(const TRow &R)
	{
		return FString::Printf(TEXT("Fi %+.0f Wa %+.0f Ea %+.0f Wi %+.0f Li %+.0f Da %+.0f Lt %+.0f Vo %+.0f Re %+.0f | Ph %+.0f"),
							   R.Fire, R.Water, R.Earth, R.Wind, R.Light, R.Darkness, R.Lightning, R.Void, R.Reality,
							   ResistanceColumns::Get(R, PHYSICAL_COLUMN));
	}
}
