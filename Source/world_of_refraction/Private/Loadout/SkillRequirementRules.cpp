// SkillRequirementRules.cpp

#include "Loadout/SkillRequirementRules.h"
#include "Loadout/Entries/FWeaponInventoryEntry.h"
#include "Loadout/Entries/FRingInventoryEntry.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "WoRGameNames.h"

#define LOCTEXT_NAMESPACE "WoRSkillRequirement"

namespace
{
	void ReadSocket(const FRuntimeAttachedItem &Socket, FSkillGearHost &Host)
	{
		Host.Element = Socket.IsEmpty() ? ESpellElement::None : Socket.GetElement();
		Host.Evolution = Socket.IsEvolution() ? Socket.Evolution.Item : nullptr;
	}
}

FSkillGearHost FSkillGearHost::FromWeapon(const FWeaponInventoryEntry &Entry)
{
	FSkillGearHost Host;
	if (const UWeaponData *Weapon = Entry.Weapon)
	{
		Host.Kind = ESkillGearKind::Weapon;
		Host.WeaponType = Weapon->WeaponType;
		ReadSocket(Entry.AttachedItem, Host);
	}
	return Host;
}

FSkillGearHost FSkillGearHost::FromAdapter(const FRingInventoryEntry &Entry)
{
	FSkillGearHost Host;
	if (const URingData *Ring = Entry.Ring)
	{
		Host.Kind = ESkillGearKind::Adapter;
		Host.Form = Ring->Form;
		ReadSocket(Entry.AttachedItem, Host);
	}
	return Host;
}

FSkillGearHost FSkillGearHost::FromEvolutionSeat(const UEvolutionItemData *Evolution)
{
	FSkillGearHost Host;
	if (Evolution)
	{
		Host.Kind = ESkillGearKind::Evolution;
		Host.Element = Evolution->GetAssociatedElement();
		Host.Evolution = Evolution;
	}
	return Host;
}

bool SkillRequirementRules::IsMet(const USkillDataBase *Skill, const FSkillGearHost &Host)
{
	if (!Skill)
	{
		return false;
	}
	const FSkillGearRequirement Need = Skill->GetGearRequirement();
	if (Need.Element != ESpellElement::None && Host.Element != Need.Element)
	{
		return false;
	}
	switch (Need.Kind)
	{
	case ESkillGearKind::Weapon:
		return Host.Kind == ESkillGearKind::Weapon && (!Need.bNeedsWeaponType || Host.WeaponType == Need.WeaponType);
	case ESkillGearKind::Adapter:
		return Host.Kind == ESkillGearKind::Adapter && (!Need.bNeedsForm || Host.Form == Need.Form);
	case ESkillGearKind::Evolution:
		// The seat, or an adapter carrying one (§5.1: an evolution in an adapter uses the adapter's list).
		return Host.Evolution && (!Need.Evolution || Host.Evolution == Need.Evolution);
	default:
		return true; // a spell with nothing set: any spell source, which the host's own list already checks
	}
}

FText SkillRequirementRules::GetNeedText(const USkillDataBase *Skill)
{
	if (!Skill)
	{
		return FText::GetEmpty();
	}
	const FSkillGearRequirement Need = Skill->GetGearRequirement();
	TArray<FText> Parts;
	switch (Need.Kind)
	{
	case ESkillGearKind::Weapon:
		Parts.Add(Need.bNeedsWeaponType ? WoRGameNames::GetWeaponTypeName(Need.WeaponType) : LOCTEXT("AnyWeapon", "a weapon"));
		break;
	case ESkillGearKind::Adapter:
		Parts.Add(Need.bNeedsForm ? WoRGameNames::GetAdapterFormName(Need.Form) : LOCTEXT("AnyAdapter", "an adapter"));
		break;
	case ESkillGearKind::Evolution:
		Parts.Add(Need.Evolution ? FText::FromString(Need.Evolution->ItemName) : LOCTEXT("AnyEvolution", "an evolution"));
		break;
	default:
		break;
	}
	if (Need.Element != ESpellElement::None)
	{
		Parts.Add(FText::Format(LOCTEXT("ElementSocket", "{0} socket"), WoRGameNames::GetElementName(Need.Element)));
	}
	return Parts.IsEmpty() ? FText::GetEmpty()
						   : FText::Format(LOCTEXT("Needs", "Needs: {0}."), FText::Join(LOCTEXT("PartSeparator", ", "), Parts));
}

FString SkillRequirementRules::GetRequirementString(const USkillDataBase *Skill)
{
	if (!Skill)
	{
		return TEXT("(no skill)");
	}
	const FSkillGearRequirement Need = Skill->GetGearRequirement();
	FString Out = UEnum::GetDisplayValueAsText(Need.Kind).ToString();
	if (Need.Kind == ESkillGearKind::Weapon && Need.bNeedsWeaponType)
	{
		Out += TEXT(" ") + UEnum::GetValueAsString(Need.WeaponType);
	}
	if (Need.Kind == ESkillGearKind::Adapter && Need.bNeedsForm)
	{
		Out += TEXT(" ") + UEnum::GetValueAsString(Need.Form);
	}
	if (Need.Kind == ESkillGearKind::Evolution && Need.Evolution)
	{
		Out += TEXT(" ") + Need.Evolution->ItemName;
	}
	if (Need.Element != ESpellElement::None)
	{
		Out += TEXT(", element ") + UEnum::GetValueAsString(Need.Element);
	}
	return Skill->GearRequirement.Kind == ESkillGearKind::None ? Out + TEXT(" (from old fields)") : Out;
}

#undef LOCTEXT_NAMESPACE
