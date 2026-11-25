// LoadoutSkillsModel.cpp

#include "UI/Menus/LoadoutSkillsModel.h"

#include "Character/CharacterDataComponent.h"
#include "Engine/World.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/SkillSlotList.h"
#include "Loadout/SpellPoolConstants.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "LoadoutSkills"

namespace LoadoutSkillsBuild
{
	FLoadoutScreenRow MakeEmptyRow(int32 Slot)
	{
		FLoadoutScreenRow Row;
		Row.Kind = ELoadoutScreenRowKind::EmptySkill;
		Row.Name = LOCTEXT("EmptySlot", "Empty slot");
		Row.SkillSlot = Slot;
		Row.bCountsToBudget = false;
		Row.Note = LOCTEXT("NoteEmptySlot", "Empty.");
		return Row;
	}

	/** Filled slot: name, tier, kind line, the EP, hits and (innate) points, the description as the note. InnatePoints
	 *  INDEX_NONE for a gear list. */
	FLoadoutScreenRow MakeSkillRow(USkillDataBase *Skill, int32 Slot, bool bPick, int32 InnatePoints, int32 Discount)
	{
		FLoadoutScreenRow Row;
		Row.Kind = ELoadoutScreenRowKind::Skill;
		Row.Skill = Skill;
		Row.SkillSlot = Slot;
		Row.bPick = bPick;
		Row.bCountsToBudget = false;
		Row.Name = FText::FromString(Skill->Name);
		Row.Tier = WoRGameNames::GetTierLetter(Skill->Tier);
		Row.Note = FText::FromString(Skill->Description);
		const USpellData *Spell = Cast<USpellData>(Skill);
		Row.KindLine = Spell ? FText::Format(LOCTEXT("SpellKind", "{0} · {1}"), WoRGameNames::GetElementName(Spell->Element), WoRGameNames::GetSpellSchoolName(Spell->GetSchool()))
							 : LOCTEXT("AbilityKind", "Ability");
		Row.Details.Add({LOCTEXT("DetailEP", "EP"), FText::AsNumber(Skill->BaseEnergyCost)});
		Row.Details.Add({LOCTEXT("DetailHits", "Hits"), FText::AsNumber(Skill->HitCount)});
		if (InnatePoints != INDEX_NONE)
		{
			Row.Cost = InnatePoints;
			Row.CostLine = FText::Format(LOCTEXT("PointsCost", "{0} pts"), InnatePoints);
			Row.Details.Add({LOCTEXT("DetailPoints", "Points"),
							 Discount > 0 ? FText::Format(LOCTEXT("PointsDiscounted", "{0}  (tier {1}, -{2} pillar discount)"), InnatePoints, Row.Tier,
														  SpellPoolConstants::SpellSlotBaseCost(Skill->Tier))
										  : FText::Format(LOCTEXT("PointsPlain", "{0}  (tier {1})"), InnatePoints, Row.Tier)});
		}
		else
		{
			Row.CostLine = bPick ? LOCTEXT("YourPick", "Your pick") : LOCTEXT("BuiltIn", "Built in");
		}
		return Row;
	}

	int32 InnateCost(const USpellData *Spell, int32 Discount)
	{
		return SpellPoolConstants::SpellSlotEffectiveCost(Spell->Tier, Discount);
	}

	/** A tab's slot rows: each slot's skill (null = empty). */
	template <typename TSkill>
	void AddSlotRows(FLoadoutSkillTab &Tab, const TArray<TSkill *> &Slots, const TArray<TSkill *> &Picks)
	{
		for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
		{
			Tab.Rows.Add(Slots[Slot] ? MakeSkillRow(Slots[Slot], Slot, SkillSlotList::IsReplaced(Picks, Slot), INDEX_NONE, 0) : MakeEmptyRow(Slot));
			Tab.Filled += Slots[Slot] ? 1 : 0;
		}
	}

	FText MemberNotHere(const FLoadoutScreenMember &Member)
	{
		return FText::Format(LOCTEXT("MemberNotHere", "{0} isn't here."), Member.Name);
	}
}

FLoadoutSkillsPage WoRLoadoutSkills::BuildSkillsPage(const FCombatLoadout &Loadout, ECharacterClass Class, int32 Discount, const TArray<USpellData *> &LearnedSpells)
{
	using namespace LoadoutSkillsBuild;
	FLoadoutSkillsPage Page;
	Page.Discount = Discount;
	if (Class == ECharacterClass::Caster)
	{
		// TODO: B-440d, a Broken Darkness Refractor's element pools (BDSpellPools, BD_SPELL_BUDGET); its rule check still applies.
		Page.MaxPoints = SpellPoolConstants::INNATE_SPELL_BUDGET;
		TSet<ESpellSchool> Held;
		for (const USpellData *Spell : Loadout.InnateSpells)
		{
			if (Spell)
			{
				Held.Add(Spell->GetSchool());
				Page.Points += InnateCost(Spell, Discount);
			}
		}
		for (const USpellData *Spell : LearnedSpells)
		{
			if (Spell)
			{
				Held.Add(Spell->GetSchool());
			}
		}
		for (uint8 SchoolIndex = 0; SchoolIndex <= static_cast<uint8>(ESpellSchool::Impairment); ++SchoolIndex)
		{
			const ESpellSchool School = static_cast<ESpellSchool>(SchoolIndex);
			if (!Held.Contains(School))
			{
				continue;
			}
			FLoadoutSkillTab &Tab = Page.Tabs.AddDefaulted_GetRef();
			Tab.List = ELoadoutSkillList::Innate;
			Tab.School = School;
			Tab.Title = WoRGameNames::GetSpellSchoolName(School);
			for (int32 Index = 0; Index < Loadout.InnateSpells.Num(); ++Index)
			{
				USpellData *Spell = Loadout.InnateSpells[Index];
				if (Spell && Spell->GetSchool() == School)
				{
					Tab.Rows.Add(MakeSkillRow(Spell, Index, true, InnateCost(Spell, Discount), Discount));
				}
			}
			Tab.Filled = Tab.Rows.Num();
			while (Tab.Rows.Num() < SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL)
			{
				Tab.Rows.Add(MakeEmptyRow(INDEX_NONE));
			}
		}
	}

	FCombatLoadout Copy = Loadout;
	for (const FLoadoutScreenRow &Gear : WoRLoadoutScreen::BuildGearPage(Loadout, Class).Rows)
	{
		FWeaponLoadoutEntry *Weapon = nullptr;
		FRingLoadoutEntry *Adapter = nullptr;
		if (Gear.GearID.IsValid())
		{
			WoRLoadoutScreen::FindRowGear(Copy, Gear, Weapon, Adapter);
		}
		if (!Weapon && !Adapter)
		{
			continue;
		}
		FLoadoutSkillTab &Tab = Page.Tabs.AddDefaulted_GetRef();
		Tab.Title = Gear.Name;
		Tab.Gear = Gear;
		if (Weapon)
		{
			Tab.List = ELoadoutSkillList::WeaponSkills;
			Tab.Host = FSkillGearHost::FromWeapon(Weapon->WeaponEntry);
			AddSlotRows(Tab, Weapon->GetSkillSlots(), Weapon->WeaponEntry.SkillPicks);
		}
		else
		{
			Tab.List = ELoadoutSkillList::AdapterSpells;
			Tab.Host = FSkillGearHost::FromAdapter(Adapter->RingEntry);
			AddSlotRows(Tab, Adapter->GetSpellSlots(), Adapter->RingEntry.SpellPicks);
		}
	}
	return Page;
}

FLoadoutSkillsPage WoRLoadoutSkills::BuildSkillsPage(UWorld *World, const FLoadoutScreenMember &Member)
{
	UInventoryComponent *Inventory = nullptr;
	if (!WoRLoadoutScreen::FindMemberLoadout(World, Member, Inventory))
	{
		FLoadoutSkillsPage Page;
		Page.Refusal = FText::Format(LOCTEXT("NotHere", "{0} isn't here."), Member.Name);
		return Page;
	}
	// The discount the rule check uses (ULoadoutComponent::BuildRuleContext).
	const UCharacterDataComponent *Character = Inventory->GetOwner()->FindComponentByClass<UCharacterDataComponent>();
	const int32 Discount = Character && Character->CharacterData
							   ? SpellPoolConstants::SpellSlotDiscount(Character->GetWorldMind(), Character->GetWorldBody(), Character->GetWorldSpirit())
							   : 0;
	TArray<USpellData *> Learned;
	for (const FSpellInstance &Instance : Inventory->Spells.LearnedSpells)
	{
		Learned.Add(Instance.Spell);
	}
	return BuildSkillsPage(Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex], Member.Class, Discount, Learned);
}

TArray<FLoadoutScreenChoice> WoRLoadoutSkills::BuildSkillChoices(const FLoadoutSkillsPage &Page, int32 TabIndex, const FLoadoutScreenRow &Row,
																 const TArray<UAbilityData *> &LearnedAbilities, const TArray<USpellData *> &LearnedSpells,
																 const TArray<UAbilityData *> &BuiltIns, bool bAllLearned)
{
	using namespace LoadoutSkillsBuild;
	TArray<FLoadoutScreenChoice> Choices;
	if (!Page.Tabs.IsValidIndex(TabIndex))
	{
		return Choices;
	}
	const FLoadoutSkillTab &Tab = Page.Tabs[TabIndex];
	TSet<const USkillDataBase *> InList;
	for (const FLoadoutSkillTab &Other : Page.Tabs)
	{
		// The innate spells are one list across the schools; a weapon's or adapter's is its own.
		if (&Other == &Tab || (Tab.List == ELoadoutSkillList::Innate && Other.List == ELoadoutSkillList::Innate))
		{
			for (const FLoadoutScreenRow &Slot : Other.Rows)
			{
				InList.Add(Slot.Skill);
			}
		}
	}
	TSet<const USkillDataBase *> Offered;
	const auto Offer = [&](USkillDataBase *Skill)
	{
		if (!Skill || Offered.Contains(Skill))
		{
			return;
		}
		// D-224 (§5.2): a piece offers only the skills whose gear it meets. B-629: All learned shows the rest too, each
		// refused with its reason (the ones in the list already ticked).
		const bool bSlotted = InList.Contains(Skill);
		const bool bFits = Tab.List == ELoadoutSkillList::Innate || SkillRequirementRules::IsMet(Skill, Tab.Host);
		if ((bSlotted || !bFits) && !bAllLearned)
		{
			return;
		}
		Offered.Add(Skill);
		FLoadoutScreenChoice &Choice = Choices.AddDefaulted_GetRef();
		Choice.bSlotted = bSlotted;
		if (Tab.List == ELoadoutSkillList::Innate)
		{
			const int32 Points = InnateCost(CastChecked<USpellData>(Skill), Page.Discount);
			Choice.Row = MakeSkillRow(Skill, Row.SkillSlot, true, Points, Page.Discount);
			Choice.NewSpent = Page.Points - (Row.Skill ? Row.Cost : 0) + Points;
			if (Choice.NewSpent > Page.MaxPoints)
			{
				Choice.Refusal = FText::Format(LOCTEXT("OverPoints", "Over the points: {0} of {1}."), Choice.NewSpent, Page.MaxPoints);
			}
		}
		else
		{
			// A pick unless it is the built-in this very slot holds.
			const bool bBuiltInHere = BuiltIns.IsValidIndex(Row.SkillSlot) && BuiltIns[Row.SkillSlot] == Skill;
			Choice.Row = MakeSkillRow(Skill, Row.SkillSlot, !bBuiltInHere, INDEX_NONE, 0);
		}
		if (bSlotted)
		{
			Choice.Refusal = LOCTEXT("SlottedAlready", "In this list already.");
		}
		else if (!bFits)
		{
			const FText Need = SkillRequirementRules::GetNeedText(Skill);
			Choice.Refusal = Need.IsEmpty() ? LOCTEXT("GearDoesNotFit", "This piece doesn't meet what it needs.") : Need;
		}
	};
	if (Tab.List == ELoadoutSkillList::WeaponSkills)
	{
		for (UAbilityData *Ability : BuiltIns)
		{
			Offer(Ability);
		}
		for (UAbilityData *Ability : LearnedAbilities)
		{
			Offer(Ability);
		}
		return Choices;
	}
	for (USpellData *Spell : LearnedSpells)
	{
		if (Spell && (Tab.List != ELoadoutSkillList::Innate || Spell->GetSchool() == Tab.School))
		{
			Offer(Spell);
		}
	}
	return Choices;
}

TArray<FLoadoutScreenChoice> WoRLoadoutSkills::BuildSkillChoices(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutSkillsPage &Page, int32 Tab,
																 const FLoadoutScreenRow &Row, bool bAllLearned)
{
	UInventoryComponent *Inventory = nullptr;
	if (!WoRLoadoutScreen::FindMemberLoadout(World, Member, Inventory) || !Page.Tabs.IsValidIndex(Tab))
	{
		return {};
	}
	TArray<UAbilityData *> Abilities;
	for (const FAbilityInstance &Instance : Inventory->Abilities.LearnedAbilities)
	{
		Abilities.Add(Instance.Ability);
	}
	TArray<USpellData *> Spells;
	for (const FSpellInstance &Instance : Inventory->Spells.LearnedSpells)
	{
		Spells.Add(Instance.Spell);
	}
	// B-518 (D-224): only the known skills are offered; gear has no built-ins.
	return BuildSkillChoices(Page, Tab, Row, Abilities, Spells, TArray<UAbilityData *>(), bAllLearned);
}

bool WoRLoadoutSkills::PutSkill(FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutSkillTab &Tab, const FLoadoutScreenRow &Row,
								USkillDataBase *Skill, FText &OutRefusal)
{
	const FText NothingToTakeOff = LOCTEXT("NothingToTakeOff", "This slot is empty.");
	if (Tab.List == ELoadoutSkillList::Innate)
	{
		USpellData *Spell = Cast<USpellData>(Skill);
		if (Skill && !Spell)
		{
			OutRefusal = LOCTEXT("NotASpell", "Only a spell goes here.");
			return false;
		}
		if (!Spell)
		{
			if (!Loadout.InnateSpells.IsValidIndex(Row.SkillSlot) || !Loadout.InnateSpells[Row.SkillSlot])
			{
				OutRefusal = NothingToTakeOff;
				return false;
			}
			Loadout.InnateSpells.RemoveAt(Row.SkillSlot);
			return true;
		}
		if (Loadout.InnateSpells.IsValidIndex(Row.SkillSlot))
		{
			Loadout.InnateSpells[Row.SkillSlot] = Spell;
		}
		else if (const int32 Gap = Loadout.InnateSpells.Find(nullptr); Gap != INDEX_NONE)
		{
			Loadout.InnateSpells[Gap] = Spell;
		}
		else
		{
			Loadout.InnateSpells.Add(Spell);
		}
		return true;
	}

	if (!Skill && !Row.bPick)
	{
		OutRefusal = Row.Skill ? LOCTEXT("BuiltInStays", "It's the piece's own skill: Change puts another in its place.") : NothingToTakeOff;
		return false;
	}
	bool bHeldRefused = false;
	const bool bDone = WoRLoadoutScreen::EditRowGear(Loadout, Class, Tab.Gear, [&](FWeaponLoadoutEntry *Weapon, FRingLoadoutEntry *Adapter)
	{
		if (Tab.List == ELoadoutSkillList::WeaponSkills && Weapon)
		{
			UAbilityData *Ability = Cast<UAbilityData>(Skill);
			if (Ability && !Weapon->CanHoldSkill(Row.SkillSlot, Ability, &OutRefusal)) // B-410: the gem's element
			{
				bHeldRefused = true;
				return false;
			}
			return Skill ? Ability && Weapon->ReplaceSkill(Row.SkillSlot, Ability) : Weapon->RestoreSkill(Row.SkillSlot);
		}
		if (Tab.List == ELoadoutSkillList::AdapterSpells && Adapter)
		{
			USpellData *Spell = Cast<USpellData>(Skill);
			return Skill ? Spell && Adapter->ReplaceSpell(Row.SkillSlot, Spell) : Adapter->RestoreSpell(Row.SkillSlot);
		}
		return false;
	});
	if (bHeldRefused)
	{
		return false;
	}
	if (!bDone)
	{
		OutRefusal = LOCTEXT("SlotRefused", "It can't go in this slot.");
	}
	return bDone;
}

FText WoRLoadoutSkills::ApplySkill(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutSkillTab &Tab, const FLoadoutScreenRow &Row, USkillDataBase *Skill)
{
	UInventoryComponent *Inventory = nullptr;
	ULoadoutComponent *Loadout = WoRLoadoutScreen::FindMemberLoadout(World, Member, Inventory);
	if (!Loadout)
	{
		return LoadoutSkillsBuild::MemberNotHere(Member);
	}
	const int32 Index = Inventory->ActiveLoadoutIndex;
	FText Refusal;
	FCombatLoadout Probe = Inventory->SavedLoadouts[Index];
	if (!PutSkill(Probe, Member.Class, Tab, Row, Skill, Refusal))
	{
		return Refusal;
	}
	const bool bChanged = Loadout->TryChangeGear(Index, [&](FCombatLoadout &Changed)
												 {
													 FText Ignored;
													 PutSkill(Changed, Member.Class, Tab, Row, Skill, Ignored);
												 },
												 Refusal);
	if (bChanged)
	{
		Inventory->WriteLoadoutToOwned(Index); // its sockets; the picks reach the worn entry (D-224, B-517)
		UE_LOG(LogWoRLoadout, Log, TEXT("%s: %s %s slot %d"), *Member.Key.ToString(), Skill ? TEXT("put") : TEXT("took off"), *Tab.Title.ToString(), Row.SkillSlot);
	}
	return Refusal;
}

FString WoRLoadoutSkills::GetSkillsPageString(UWorld *World)
{
	FString Out;
	for (const FLoadoutScreenMember &Member : WoRLoadoutScreen::BuildMembers(World))
	{
		const FLoadoutSkillsPage Page = BuildSkillsPage(World, Member);
		Out += FString::Printf(TEXT("%s: %s\n"), *Member.Name.ToString(), *Page.Refusal.ToString());
		if (Page.MaxPoints > 0)
		{
			Out += FString::Printf(TEXT("  innate points %d of %d (discount %d)\n"), Page.Points, Page.MaxPoints, Page.Discount);
		}
		for (const FLoadoutSkillTab &Tab : Page.Tabs)
		{
			Out += FString::Printf(TEXT("  [%s %d]\n"), *Tab.Title.ToString(), Tab.Filled);
			for (const FLoadoutScreenRow &Row : Tab.Rows)
			{
				Out += FString::Printf(TEXT("    %d %s %s %s\n"), Row.SkillSlot, *Row.Name.ToString(), *Row.Tier.ToString(), *Row.CostLine.ToString());
			}
		}
	}
	return Out;
}

namespace LoadoutSkillsDebug
{
	FAutoConsoleCommandWithWorld GPrintSkillsCmd(TEXT("wor.UI.PrintSkills"),
												 TEXT("DEBUG: wor.UI.PrintSkills - every party member's loadout Skills page: tabs, slots, innate points (B-440d)."),
												 FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
																								{ UE_LOG(LogWoRLoadout, Display, TEXT("\n%s"), *WoRLoadoutSkills::GetSkillsPageString(World)); }));
}

#undef LOCTEXT_NAMESPACE
