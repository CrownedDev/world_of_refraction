// ElementTable.cpp

#include "Combat/Elements/ElementTable.h"

#include "Currency/CurrencyTypes.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Infusion/ElementColors.h"
#include "Skills/Effects/ESkillEffectType.h"

namespace
{
	/** Physical damage reads white (B-105). */
	const FLinearColor PHYSICAL_COLOUR = FLinearColor::White;
	/** Ice has no colour of its own yet; it is keyed Water (D-029). // TODO: an Ice colour */
	const FLinearColor ICE_COLOUR = ElementColors::Water;

	using T = EWoRDamageType;
	using E = ESpellElement;
	using P = EPhysicalDamageType;
	using G = ECrystalType;
	using Ess = EEssenceType;
	using S = ESkillEffectType;

	TArray<FElementRow> BuildRows()
	{
		// Type, Element, Physical, AlsoResistedBy, Gem, Essence, BreakTrigger, Immunity, Colour, bBDAbsorbable, bBDForbidden
		return {
			{T::None,      E::None,      P::None,   T::None, G::Quartz,   Ess::Generic,   S::None,              S::None,                   ElementColors::Generic,  false, false},
			{T::Fire,      E::Fire,      P::None,   T::None, G::Garnet,   Ess::Fire,      S::DOT,               S::GrantFireImmunity,      ElementColors::Fire,      true,  false},
			{T::Water,     E::Water,     P::None,   T::Ice,  G::Sapphire, Ess::Water,     S::ActionSpeedDebuff, S::GrantWaterImmunity,     ElementColors::Water,     true,  false},
			{T::Earth,     E::Earth,     P::None,   T::None, G::Amber,    Ess::Earth,     S::DefenseDebuff,     S::GrantEarthImmunity,     ElementColors::Earth,     true,  false},
			{T::Wind,      E::Wind,      P::None,   T::None, G::Emerald,  Ess::Wind,      S::SkipTurn,          S::GrantWindImmunity,      ElementColors::Wind,      true,  false},
			{T::Light,     E::Light,     P::None,   T::None, G::Opal,     Ess::Light,     S::CritChanceDebuff,  S::GrantLightImmunity,     ElementColors::Light,     true,  true},
			{T::Darkness,  E::Darkness,  P::None,   T::None, G::Onyx,     Ess::Darkness,  S::Silenced,          S::GrantDarknessImmunity,  ElementColors::Darkness,  true,  false},
			{T::Lightning, E::Lightning, P::None,   T::None, G::Citrine,  Ess::Lightning, S::Stun,              S::GrantLightningImmunity, ElementColors::Lightning, true,  false},
			{T::Void,      E::Void,      P::None,   T::None, G::Amethyst, Ess::Void,      S::RandomSkill,       S::GrantVoidImmunity,      ElementColors::Void,      true,  true},
			{T::Reality,   E::Reality,   P::None,   T::None, G::Iolite,   Ess::Reality,   S::BurstDamage,       S::GrantRealityImmunity,   ElementColors::Reality,   false, false},
			{T::Slash,     E::None,      P::Slash,  T::None, G::None,     Ess::Generic,   S::DOT,               S::None,                   PHYSICAL_COLOUR,          false, false},
			{T::Pierce,    E::None,      P::Pierce, T::None, G::None,     Ess::Generic,   S::DefenseDebuff,     S::None,                   PHYSICAL_COLOUR,          false, false},
			{T::Impact,    E::None,      P::Impact, T::None, G::None,     Ess::Generic,   S::Stun,              S::None,                   PHYSICAL_COLOUR,          false, false},
			{T::Ice,       E::None,      P::None,   T::None, G::None,     Ess::Generic,   S::None,              S::None,                   ICE_COLOUR,               false, false},
		};
	}
}

namespace ElementTable
{
	const TArray<FElementRow> &Rows()
	{
		static const TArray<FElementRow> Table = BuildRows();
		return Table;
	}

	const FElementRow &For(EWoRDamageType Type)
	{
		const TArray<FElementRow> &Table = Rows();
		const int32 Index = static_cast<int32>(Type);
		return Table.IsValidIndex(Index) ? Table[Index] : Table[0];
	}

	const FElementRow &ForElement(ESpellElement Element)
	{
		return For(WoRDamageType::ElementToDamageType(Element));
	}

	const FElementRow &ForPhysical(EPhysicalDamageType Physical)
	{
		return For(WoRDamageType::PhysicalToDamageType(Physical));
	}
}
