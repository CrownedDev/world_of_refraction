// FApparelEntry.cpp

#include "Loadout/Entries/FApparelEntry.h"

#include "Equipment/Apparel/ApparelData.h"
#include "Loadout/FCombatLoadout.h"
#include "Combat/Resistance/EWoRDamageType.h"
#include "Combat/Resistance/ResistanceColumns.h"

FApparelEntry FApparelEntry::CreateFromApparel(UApparelData *InApparel)
{
	FApparelEntry Entry;
	Entry.Apparel = InApparel;
	Entry.Tier = InApparel ? InApparel->Tier : EItemTier::F_Tier;
	return Entry;
}

ESpellElement FApparelEntry::GetElement() const
{
	return AttachedItem.IsCrystal() && !AttachedItem.IsBroken() ? AttachedItem.GetElement() : ESpellElement::None;
}

FEquipmentStatBonus FApparelEntry::GetStatLines() const
{
	// The asset's Base lines and this instance's roll; the obsolete GeneratedStatBonus layer (B-335) is not read.
	FEquipmentStatBonus Lines = Apparel ? Apparel->GetStatLines() : FEquipmentStatBonus();
	Lines.BonusMaxHP += StatBonus.BonusMaxHP;
	Lines.BonusDefense += StatBonus.BonusDefense;
	return Lines;
}

FResistanceBonus FApparelEntry::GetResistance() const
{
	FResistanceBonus Resistance = Apparel ? Apparel->BaseResistance : FResistanceBonus();
	const ESpellElement Element = GetElement();
	if (Element != ESpellElement::None && Element != ESpellElement::Generic)
	{
		const EWoRDamageType Type = WoRDamageType::ElementToDamageType(Element);
		ResistanceColumns::Set(Resistance, Type, ResistanceColumns::Get(Resistance, Type) + ApparelRules::CRYSTAL_ELEMENT_RESISTANCE);
	}
	return Resistance;
}

bool ApparelRules::CanSocket(const FRuntimeAttachedItem &Attachment)
{
	return Attachment.IsCrystal();
}

bool ApparelRules::AddApparel(FCombatLoadout &Loadout, const FApparelEntry &Entry)
{
	if (!Entry.Apparel || Loadout.Apparel.Num() >= MAX_APPAREL_SLOTS || (!Entry.AttachedItem.IsEmpty() && !CanSocket(Entry.AttachedItem)))
	{
		return false; // review: a piece with an evolution or stone in its socket is refused (Q-380)
	}
	if (Entry.PersistentID.IsValid() &&
		Loadout.Apparel.ContainsByPredicate([&Entry](const FApparelEntry &Worn) { return Worn.PersistentID == Entry.PersistentID; }))
	{
		return false;
	}
	Loadout.Apparel.Add(Entry);
	return true;
}

FString ApparelRules::GetApparelString(const FCombatLoadout &Loadout)
{
	if (Loadout.Apparel.IsEmpty())
	{
		return TEXT("apparel: none");
	}
	TArray<FString> Pieces;
	for (const FApparelEntry &Entry : Loadout.Apparel)
	{
		const FEquipmentStatBonus Lines = Entry.GetStatLines();
		Pieces.Add(FString::Printf(TEXT("%s [%s] HP %+d Def %+d"), *GetNameSafe(Entry.Apparel.Get()),
								   *UEnum::GetDisplayValueAsText(Entry.GetElement()).ToString(), Lines.BonusMaxHP, Lines.BonusDefense));
	}
	return FString::Printf(TEXT("apparel (%d of %d, no budget): %s"), Loadout.Apparel.Num(), MAX_APPAREL_SLOTS, *FString::Join(Pieces, TEXT("; ")));
}
