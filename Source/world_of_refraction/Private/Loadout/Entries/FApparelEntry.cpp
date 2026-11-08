// FApparelEntry.cpp

#include "Loadout/Entries/FApparelEntry.h"

#include "Equipment/Apparel/ApparelData.h"
#include "Loadout/FCombatLoadout.h"

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
	FEquipmentStatBonus Lines = Apparel ? Apparel->GetStatLines() : FEquipmentStatBonus();
	Lines.BonusMaxHP += StatBonus.BonusMaxHP;
	Lines.BonusDefense += StatBonus.BonusDefense;
	return Lines;
}

bool ApparelRules::CanSocket(const FRuntimeAttachedItem &Attachment)
{
	return Attachment.IsCrystal();
}

bool ApparelRules::AddApparel(FCombatLoadout &Loadout, const FApparelEntry &Entry)
{
	if (!Entry.Apparel || Loadout.Apparel.Num() >= MAX_APPAREL_SLOTS)
	{
		return false;
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
