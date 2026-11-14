// FApparelEntry.cpp

#include "Loadout/Entries/FApparelEntry.h"

#include "Equipment/Apparel/ApparelData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
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
	return (AttachedItem.IsCrystal() || AttachedItem.IsEvolution()) && !AttachedItem.IsBroken() ? AttachedItem.GetElement() : ESpellElement::None;
}

const UEvolutionItemData *FApparelEntry::GetEvolution() const
{
	return AttachedItem.IsEvolution() ? AttachedItem.Evolution.Item : nullptr;
}

FEquipmentStatBonus FApparelEntry::GetStatLines() const
{
	// The asset's Base lines and this instance's roll; the obsolete GeneratedStatBonus layer (B-335) is not read.
	FEquipmentStatBonus Lines = Apparel ? Apparel->GetStatLines() : FEquipmentStatBonus();
	Lines.BonusMaxHP += StatBonus.BonusMaxHP;
	Lines.BonusDefense += StatBonus.BonusDefense;
	Lines.Amp.Accumulate(StatBonus.Amp); // B-386
	return Lines;
}

FEquipmentStatBonus FApparelEntry::GetBonusLineStats() const
{
	FEquipmentStatBonus Lines;
	ApparelBonusLine::AddTo(BonusLine, Lines);
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
	return Attachment.IsCrystal() || Attachment.IsEvolution(); // Crown, D-164: an evolution too
}

bool ApparelRules::AddApparel(FCombatLoadout &Loadout, const FApparelEntry &Entry)
{
	if (!Entry.Apparel || Loadout.Apparel.Num() >= MAX_APPAREL_SLOTS || (!Entry.AttachedItem.IsEmpty() && !CanSocket(Entry.AttachedItem)))
	{
		return false; // a piece with an augment stone in its socket is refused
	}
	if (Entry.PersistentID.IsValid() &&
		Loadout.Apparel.ContainsByPredicate([&Entry](const FApparelEntry &Worn) { return Worn.PersistentID == Entry.PersistentID; }))
	{
		return false;
	}
	Loadout.Apparel.Add(Entry);
	return true;
}

int32 ApparelRules::RefreshFromStore(TArray<FCombatLoadout> &Loadouts, const FApparelEntry &Owned)
{
	if (!Owned.PersistentID.IsValid())
	{
		return 0;
	}
	int32 Changed = 0;
	for (FCombatLoadout &Loadout : Loadouts)
	{
		for (FApparelEntry &Worn : Loadout.Apparel)
		{
			if (Worn.PersistentID == Owned.PersistentID)
			{
				Worn = Owned;
				++Changed;
			}
		}
	}
	return Changed;
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
		const FText Bonus = ApparelBonusLine::GetText(Entry.BonusLine); // B-475
		Pieces.Add(FString::Printf(TEXT("%s [%s] HP %+d Def %+d %s bonus %s"), *GetNameSafe(Entry.Apparel.Get()),
								   *UEnum::GetDisplayValueAsText(Entry.GetElement()).ToString(), Lines.BonusMaxHP, Lines.BonusDefense,
								   *Lines.Amp.ToString(), Bonus.IsEmpty() ? TEXT("none") : *Bonus.ToString()));
	}
	return FString::Printf(TEXT("apparel (%d of %d, no budget): %s"), Loadout.Apparel.Num(), MAX_APPAREL_SLOTS, *FString::Join(Pieces, TEXT("; ")));
}
