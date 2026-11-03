// ResistanceColumns.h
// B-272 (S7): the 12 resistance columns, read and written by damage type. Four structs carry the same
// named columns: ClassInnateResistanceTable::FResistanceRow, FResistanceProfileRow, FResistanceBonus and
// FResistanceProfileDisplay. Their fields stay as they are (authored and saved); copies, sums, compares
// and column reads go through here. Adding Ice = one field per struct and one case in Field.
// Debug: ToString; wor.CompareResistance; spec WoR.Resistance.Columns.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Resistance/EWoRDamageType.h"

namespace ResistanceColumns
{
	/** The damage types that have a column, in field order. None has no column; Ice has none yet (D-029, keyed Water). */
	inline const TArray<EWoRDamageType> &Types()
	{
		static const TArray<EWoRDamageType> ColumnTypes = {
			EWoRDamageType::Fire, EWoRDamageType::Water, EWoRDamageType::Earth, EWoRDamageType::Wind,
			EWoRDamageType::Light, EWoRDamageType::Darkness, EWoRDamageType::Lightning, EWoRDamageType::Void,
			EWoRDamageType::Reality, EWoRDamageType::Slash, EWoRDamageType::Pierce, EWoRDamageType::Impact};
		return ColumnTypes;
	}

	/** Row's field for Type, or null when no struct has that column. Const in, const out. */
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

	/** Row's value for Type; 0 for a type with no column. */
	template <class TRow>
	float Get(const TRow &Row, EWoRDamageType Type)
	{
		const float *Value = ResistanceColumns::Field(Row, Type);
		return Value ? *Value : 0.0f;
	}

	/** A TTo with every column copied from From. */
	template <class TTo, class TFrom>
	TTo Convert(const TFrom &From)
	{
		TTo To{};
		for (const EWoRDamageType Type : Types())
		{
			*ResistanceColumns::Field(To, Type) = ResistanceColumns::Get(From, Type);
		}
		return To;
	}

	/** Adds every column of Other onto Into (unclamped). */
	template <class TInto, class TOther>
	void Add(TInto &Into, const TOther &Other)
	{
		for (const EWoRDamageType Type : Types())
		{
			*ResistanceColumns::Field(Into, Type) += ResistanceColumns::Get(Other, Type);
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

	/** One line, percent: "Fi +15 Wa -15 ... | Sl +0 Pi +0 Im +0". */
	template <class TRow>
	FString ToString(const TRow &R)
	{
		return FString::Printf(TEXT("Fi %+.0f Wa %+.0f Ea %+.0f Wi %+.0f Li %+.0f Da %+.0f Lt %+.0f Vo %+.0f Re %+.0f | Sl %+.0f Pi %+.0f Im %+.0f"),
							   R.Fire, R.Water, R.Earth, R.Wind, R.Light, R.Darkness, R.Lightning, R.Void, R.Reality,
							   R.Slash, R.Pierce, R.Impact);
	}
}
