// QAKitDebug.cpp

#include "Debug/Inventory/QAKitDebug.h"

#if !UE_BUILD_SHIPPING

#include "AssetRegistry/IAssetRegistry.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/SkillRequirementRules.h"
#include "Party/Party.h"
#include "Party/PartyInteractionComponent.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Skills/Definitions/AbilityData.h"
#include "UI/Menus/LoadoutSkillsModel.h"
#include "WoRLog.h"

namespace
{
	constexpr float QA_KIT_SCREEN_SECONDS = 10.0f;
	/** The most abilities the skill part learns while looking for one the weapon's list takes. */
	constexpr int32 MAX_SKILLS_LEARNED = 5;

	template <class TAsset>
	TArray<TAsset *> LoadAll()
	{
		TArray<FAssetData> Found;
		IAssetRegistry::GetChecked().GetAssetsByClass(TAsset::StaticClass()->GetClassPathName(), Found, /*bSearchSubClasses=*/true);
		Found.Sort([](const FAssetData &A, const FAssetData &B) { return A.AssetName.LexicalLess(B.AssetName); }); // the same pick every run
		TArray<TAsset *> Loaded;
		for (const FAssetData &Asset : Found)
		{
			if (TAsset *Object = Cast<TAsset>(Asset.GetAsset()))
			{
				Loaded.Add(Object);
			}
		}
		return Loaded;
	}

	/** The fight's weapon's skill list on Page: the active loadout's primary (Crown's QA pass: the fight holds it), else
	 *  the first weapon's; INDEX_NONE without one. */
	int32 FindWeaponTab(const FLoadoutSkillsPage &Page)
	{
		int32 First = INDEX_NONE;
		for (int32 Tab = 0; Tab < Page.Tabs.Num(); ++Tab)
		{
			if (Page.Tabs[Tab].List != ELoadoutSkillList::WeaponSkills)
			{
				continue;
			}
			if (Page.Tabs[Tab].Gear.Place == ELoadoutScreenPlace::Primary)
			{
				return Tab;
			}
			First = First == INDEX_NONE ? Tab : First;
		}
		return First;
	}

	/** The gear page's primary row; false with none. */
	bool FindPrimaryRow(UWorld &World, const FLoadoutScreenMember &Member, FLoadoutScreenPage &OutPage, int32 &OutRow)
	{
		OutPage = WoRLoadoutScreen::BuildGearPage(&World, Member);
		OutRow = OutPage.Rows.IndexOfByPredicate([](const FLoadoutScreenRow &Row) { return Row.Place == ELoadoutScreenPlace::Primary; });
		return OutRow != INDEX_NONE;
	}

	/** The primary has no skill that fits and plays: puts an owned weapon that has one in the primary place. Empty when
	 *  none is owned. */
	FString PutPrimaryWithSkill(UWorld &World, const FLoadoutScreenMember &Member, TFunctionRef<bool(const UWeaponData *)> HasSkill)
	{
		FLoadoutScreenPage Page;
		int32 PrimaryRow = INDEX_NONE;
		if (!FindPrimaryRow(World, Member, Page, PrimaryRow))
		{
			return FString();
		}
		const FLoadoutScreenRow &Primary = Page.Rows[PrimaryRow];
		for (const FLoadoutScreenChoice &Choice : WoRLoadoutScreen::BuildGearChoices(&World, Member, Primary))
		{
			const UWeaponData *Candidate = Choice.Weapon.WeaponEntry.Weapon;
			if (Choice.Refusal.IsEmpty() && HasSkill(Candidate))
			{
				const FText Refusal = WoRLoadoutScreen::ApplyGearChoice(&World, Member, Primary, Choice);
				return Refusal.IsEmpty() ? FString::Printf(TEXT("primary: %s put in place of %s (it has no skill with a montage)"), *Candidate->GetName(), *Primary.Name.ToString())
										 : FString::Printf(TEXT("primary: %s refused (%s)"), *Candidate->GetName(), *Refusal.ToString());
			}
		}
		return FString();
	}

	/** The primary is the weapon in hand, so the fight starts with it. Empty when it already is. */
	FString PutPrimaryInHand(UWorld &World, const FLoadoutScreenMember &Member)
	{
		FLoadoutScreenPage Page;
		int32 PrimaryRow = INDEX_NONE;
		const bool bOtherInHand = FindPrimaryRow(World, Member, Page, PrimaryRow) && !Page.Rows[PrimaryRow].bActive &&
								  Page.Rows.ContainsByPredicate([](const FLoadoutScreenRow &Row) { return Row.bActive; });
		if (!bOtherInHand)
		{
			return FString();
		}
		const FText Refusal = WoRLoadoutScreen::SetRowActive(&World, Member, Page.Rows[PrimaryRow]);
		return Refusal.IsEmpty() ? FString::Printf(TEXT("in hand: %s"), *Page.Rows[PrimaryRow].Name.ToString())
								 : FString::Printf(TEXT("in hand: %s refused (%s)"), *Page.Rows[PrimaryRow].Name.ToString(), *Refusal.ToString());
	}

	/** What the fight reads: the active loadout, the weapon in hand, the primary, and the primary's skill slots. */
	FString DescribeFightLoadout(UWorld &World, const ULoadoutComponent &Loadout, const FLoadoutScreenMember &Member)
	{
		const FLoadoutSkillsPage Page = WoRLoadoutSkills::BuildSkillsPage(&World, Member);
		const int32 Tab = FindWeaponTab(Page);
		TArray<FString> Slots;
		for (int32 Row = 0; Tab != INDEX_NONE && Row < Page.Tabs[Tab].Rows.Num(); ++Row)
		{
			const FLoadoutScreenRow &SlotRow = Page.Tabs[Tab].Rows[Row];
			if (SlotRow.SkillSlot != INDEX_NONE)
			{
				Slots.Add(FString::Printf(TEXT("[%d] %s"), SlotRow.SkillSlot, SlotRow.Skill ? *SlotRow.Skill->GetName() : TEXT("-")));
			}
		}
		return FString::Printf(TEXT("fight loadout: %s (%d of %d), in hand %s, primary %s; %s slots %s"), *Loadout.GetActiveLoadout().LoadoutName,
							   Loadout.GetActiveLoadoutIndex() + 1, Loadout.GetLoadoutCount(), *GetNameSafe(Loadout.GetActiveWeapon()),
							   *GetNameSafe(Loadout.GetPrimaryWeapon()), Tab == INDEX_NONE ? TEXT("(no weapon list)") : *Page.Tabs[Tab].Title.ToString(),
							   Slots.IsEmpty() ? TEXT("(none)") : *FString::Join(Slots, TEXT(" ")));
	}

	/** Puts an HP-cost skill the weapon's list offers in one of its slots (empty slots first). Empty when none is offered. */
	FString PutOfferedSkill(UWorld &World, const FLoadoutScreenMember &Member, const UWeaponData &Weapon)
	{
		const FLoadoutSkillsPage Page = WoRLoadoutSkills::BuildSkillsPage(&World, Member);
		const int32 Tab = FindWeaponTab(Page);
		if (Tab == INDEX_NONE)
		{
			return TEXT("skill: the loadout has no weapon skill list");
		}
		for (const FLoadoutScreenRow &Row : Page.Tabs[Tab].Rows)
		{
			if (Row.Skill && WoRQAKit::IsHPCostSkillFor(Cast<UAbilityData>(Row.Skill), &Weapon))
			{
				return FString::Printf(TEXT("skill: %s already in a slot of %s"), *Row.Skill->GetName(), *Weapon.GetName());
			}
		}
		for (const bool bEmptyOnly : {true, false})
		{
			for (const FLoadoutScreenRow &Row : Page.Tabs[Tab].Rows)
			{
				if (bEmptyOnly != (Row.Kind == ELoadoutScreenRowKind::EmptySkill))
				{
					continue;
				}
				for (const FLoadoutScreenChoice &Choice : WoRLoadoutSkills::BuildSkillChoices(&World, Member, Page, Tab, Row))
				{
					if (Choice.Refusal.IsEmpty() && WoRQAKit::IsHPCostSkillFor(Cast<UAbilityData>(Choice.Row.Skill), &Weapon))
					{
						const FString Replaced = Row.Skill ? FString::Printf(TEXT(", replacing %s"), *Row.Skill->GetName()) : FString();
						const FText Refusal = WoRLoadoutSkills::ApplySkill(&World, Member, Page.Tabs[Tab], Row, Choice.Row.Skill);
						return Refusal.IsEmpty() ? FString::Printf(TEXT("skill: %s put in slot %d of %s%s"), *Choice.Row.Skill->GetName(), Row.SkillSlot, *Weapon.GetName(), *Replaced)
												 : FString::Printf(TEXT("skill: %s refused (%s)"), *Choice.Row.Skill->GetName(), *Refusal.ToString());
					}
				}
			}
		}
		return FString();
	}

	/** Why each fitting ability wasn't put in a slot: on the member (the skills page's list), kept in the store, and the gear it needs. */
	FString DescribeFittingSkills(const TArray<UAbilityData *> &Abilities, const UWeaponData &Weapon, const UPoolSubsystem &Pool, FName CharacterKey,
								  const UInventoryComponent &Inventory)
	{
		TArray<FString> Parts;
		for (UAbilityData *Ability : Abilities)
		{
			if (WoRQAKit::IsHPCostSkillFor(Ability, &Weapon))
			{
				Parts.Add(FString::Printf(TEXT("%s (on member %s, kept %s, needs %s)"), *Ability->GetName(), Inventory.HasAbility(Ability) ? TEXT("yes") : TEXT("no"),
										  Pool.KnowsAbility(CharacterKey, Ability) ? TEXT("yes") : TEXT("no"), *SkillRequirementRules::GetRequirementString(Ability)));
			}
		}
		return Parts.IsEmpty() ? FString(TEXT("none fits")) : FString::Join(Parts, TEXT(", "));
	}

	FString GiveHPCostSkill(UWorld &World, const ULoadoutComponent &Loadout, UPoolSubsystem &Pool, FName CharacterKey, UInventoryComponent &Inventory,
							const FLoadoutScreenMember &Member)
	{
		const TArray<UAbilityData *> Abilities = LoadAll<UAbilityData>();
		const auto HasFittingSkill = [&Abilities](const UWeaponData *Weapon)
		{
			return Abilities.ContainsByPredicate([Weapon](const UAbilityData *Ability) { return WoRQAKit::IsHPCostSkillFor(Ability, Weapon); });
		};
		TArray<FString> Steps;
		if (Loadout.GetPrimaryWeapon() && !HasFittingSkill(Loadout.GetPrimaryWeapon())) // never swaps over an evolution or adapter primary
		{
			Steps.Add(PutPrimaryWithSkill(World, Member, HasFittingSkill));
		}
		Steps.Add(PutPrimaryInHand(World, Member));
		Steps.RemoveAll([](const FString &Step) { return Step.IsEmpty(); });
		const FString Prefix = Steps.IsEmpty() ? FString() : FString::Join(Steps, TEXT("; ")) + TEXT("; ");

		const UWeaponData *Weapon = Loadout.GetPrimaryWeapon();
		if (!Weapon)
		{
			return Prefix + TEXT("skill: the active loadout's primary is not a weapon");
		}
		FString Line = PutOfferedSkill(World, Member, *Weapon);
		TArray<FString> Learned;
		for (UAbilityData *Ability : Abilities)
		{
			if (!Line.IsEmpty() || Learned.Num() >= MAX_SKILLS_LEARNED)
			{
				break;
			}
			if (WoRQAKit::IsHPCostSkillFor(Ability, Weapon) && WoRQAKit::LearnForKit(Pool, CharacterKey, Inventory, Ability))
			{
				Learned.Add(Ability->GetName());
				Line = PutOfferedSkill(World, Member, *Weapon);
			}
		}
		if (Line.IsEmpty())
		{
			return Prefix + FString::Printf(TEXT("skill: %s's skill list offers no ability that costs EP and has a SkillMontage (%s, %s)%s; fitting: %s"), *Weapon->GetName(),
											*UEnum::GetValueAsString(Weapon->WeaponType), *UEnum::GetValueAsString(Weapon->WieldMode),
											Learned.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("; learned %s"), *FString::Join(Learned, TEXT(", "))),
											*DescribeFittingSkills(Abilities, *Weapon, Pool, CharacterKey, Inventory));
		}
		return Prefix + (Learned.IsEmpty() ? Line : FString::Printf(TEXT("%s (learned %s first)"), *Line, *FString::Join(Learned, TEXT(", "))));
	}

	FString AddCaster(UWorld &World, const APawn &Pawn, UPartySessionSubsystem &Session, ESpellElement Element)
	{
		const UParty *Party = Session.GetLocalParty();
		TArray<const UCharacterData *> InParty;
		for (int32 Slot = 0; Party && Slot < Party->Members.Num(); ++Slot)
		{
			const UCharacterData *Data = Party->Members[Slot].CharacterData;
			InParty.Add(Data);
			const bool bWanted = Data && Data->CharacterClass == ECharacterClass::Caster && (Element == ESpellElement::None || Data->InnateElement == Element);
			if (Slot > 0 && bWanted)
			{
				return FString::Printf(TEXT("caster: %s (%s) is already in the party, %s"), *Data->Name, *UEnum::GetValueAsString(Data->InnateElement),
									   Session.SetMemberAISeat(Slot, false) ? TEXT("as your seat") : TEXT("but its seat is still AI"));
			}
		}
		if (Party && Party->IsFull())
		{
			return FString::Printf(TEXT("caster: the party is full (%d)"), Party->GetMemberCount());
		}
		UCharacterData *Data = WoRQAKit::PickCaster(LoadAll<UCharacterData>(), Element, InParty);
		if (!Data)
		{
			return FString::Printf(TEXT("caster: no Caster CharacterData asset%s; wor.Debug.TestGear refractor (or bd for Darkness) makes the played character one for this PIE session"),
								   Element == ESpellElement::None ? TEXT("") : *FString::Printf(TEXT(" of %s"), *UEnum::GetValueAsString(Element)));
		}
		// Its own body when it stands here as a recruit; else the played character's, as GiveTestKit's member part.
		TSoftClassPtr<ACombatCharacter> Body;
		for (TActorIterator<ACombatCharacter> It(&World); It && Body.IsNull(); ++It)
		{
			const UCharacterDataComponent *DataComponent = It->FindComponentByClass<UCharacterDataComponent>();
			if (DataComponent && DataComponent->CharacterData == Data && It->FindComponentByClass<UPartyInteractionComponent>())
			{
				Body = TSoftClassPtr<ACombatCharacter>(It->GetClass()->GetPathName());
			}
		}
		if (Body.IsNull())
		{
			if (!Pawn.IsA<ACombatCharacter>())
			{
				return TEXT("caster: the played character can't lend its body and no recruit of that Caster stands here");
			}
			Body = TSoftClassPtr<ACombatCharacter>(Pawn.GetClass()->GetPathName());
		}
		if (!Session.InviteMemberWithData(Body, Data))
		{
			return FString::Printf(TEXT("caster: %s could not join"), *Data->Name);
		}
		const bool bPlayerSeat = Session.SetMemberAISeat(Session.GetMemberSlotByData(Data), false);
		return FString::Printf(TEXT("caster: %s (%s) joined, %s"), *Data->Name, *UEnum::GetValueAsString(Data->InnateElement),
							   bPlayerSeat ? TEXT("as your seat") : TEXT("but its seat is still AI"));
	}

	FAutoConsoleCommandWithWorldAndArgs GQAKitCmd(
		TEXT("wor.Debug.QAKit"),
		TEXT("DEBUG (QA setup): [skill] the active loadout's primary (put in hand, the weapon the fight holds) gets an ability that costs EP and has a "
			 "SkillMontage (B-592b's HP ghost), another owned weapon taking the primary place when it has none; ends on the fight loadout and its slots. "
			 "[caster [element]] a Caster joins as your seat (the selector colour). No argument: both. Hub only, through the real API."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				const FString Report = WoRQAKit::RunQAKit(World, Args);
				UE_LOG(LogWoR, Log, TEXT("[QAKit]\n%s"), *Report);
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, QA_KIT_SCREEN_SECONDS, FColor::Cyan, Report);
				}
			}));
}

bool WoRQAKit::IsHPCostSkillFor(const UAbilityData *Ability, const UWeaponData *Weapon)
{
	return Ability && Weapon && !Ability->bIsAttack && Ability->BaseEnergyCost > 0 && Ability->SkillMontage && Ability->RequiredWeaponType == Weapon->WeaponType &&
		   Ability->RequiredWieldMode == Weapon->WieldMode;
}

bool WoRQAKit::LearnForKit(UPoolSubsystem &Pool, FName CharacterKey, UInventoryComponent &Inventory, UAbilityData *Ability)
{
	if (!Pool.HasBankedSkills(CharacterKey))
	{
		// A brand-new character keeps its skills from its first draw that finds it in the party; bank what that draw would.
		Pool.BankSkills(CharacterKey, Inventory.Spells.LearnedSpells, Inventory.Abilities.LearnedAbilities);
	}
	const bool bKept = Pool.LearnSkillCopy(CharacterKey, Ability);
	const bool bOnMember = !Inventory.HasAbility(Ability) && Inventory.LearnAbility(Ability); // the list the skills page reads
	return bKept || bOnMember;
}

UCharacterData *WoRQAKit::PickCaster(const TArray<UCharacterData *> &Candidates, ESpellElement Element, const TArray<const UCharacterData *> &InParty)
{
	for (UCharacterData *Data : Candidates)
	{
		const bool bOwnElement = Data && Data->InnateElement != ESpellElement::Generic && Data->InnateElement != ESpellElement::None;
		if (Data && Data->CharacterClass == ECharacterClass::Caster && !InParty.Contains(Data) &&
			(Element == ESpellElement::None ? bOwnElement : Data->InnateElement == Element))
		{
			return Data;
		}
	}
	return nullptr;
}

FString WoRQAKit::RunQAKit(UWorld *World, const TArray<FString> &Args)
{
	const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn *Pawn = PC ? PC->GetPawn() : nullptr;
	const ULoadoutComponent *Loadout = Pawn ? Pawn->FindComponentByClass<ULoadoutComponent>() : nullptr;
	const UCharacterDataComponent *DataComponent = Pawn ? Pawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
	UPoolSubsystem *Pool = GameInstance ? GameInstance->GetSubsystem<UPoolSubsystem>() : nullptr;
	UPartySessionSubsystem *Session = GameInstance ? GameInstance->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	if (!Loadout || !DataComponent || !Pool || !Session)
	{
		return TEXT("no played character with a loadout, a pool and a party here");
	}
	if (World->GetNetMode() == NM_Client)
	{
		return TEXT("only the host can run the QA kit");
	}
	if (Pool->IsRunLayerActive())
	{
		return TEXT("a run is open: run it in the hub");
	}

	const bool bAll = Args.IsEmpty();
	const int32 CasterArg = Args.IndexOfByPredicate([](const FString &Arg) { return Arg.Equals(TEXT("caster"), ESearchCase::IgnoreCase); });
	ESpellElement Element = ESpellElement::None;
	if (CasterArg != INDEX_NONE && Args.IsValidIndex(CasterArg + 1) && !Args[CasterArg + 1].Equals(TEXT("skill"), ESearchCase::IgnoreCase))
	{
		const int64 Value = StaticEnum<ESpellElement>()->GetValueByNameString(Args[CasterArg + 1]);
		if (Value == INDEX_NONE || Value == static_cast<int64>(ESpellElement::Generic) || Value == static_cast<int64>(ESpellElement::None))
		{
			return FString::Printf(TEXT("caster: '%s' is not an element (fire, water, ..., darkness)"), *Args[CasterArg + 1]);
		}
		Element = static_cast<ESpellElement>(Value);
	}

	TArray<FString> Lines;
	const bool bSkill = bAll || Args.ContainsByPredicate([](const FString &Arg) { return Arg.Equals(TEXT("skill"), ESearchCase::IgnoreCase); });
	const FLoadoutScreenMember *Played = nullptr;
	UInventoryComponent *PlayedInventory = nullptr;
	const TArray<FLoadoutScreenMember> Members = WoRLoadoutScreen::BuildMembers(World);
	for (const FLoadoutScreenMember &Member : Members)
	{
		UInventoryComponent *Inventory = nullptr;
		if (WoRLoadoutScreen::FindMemberLoadout(World, Member, Inventory) == Loadout)
		{
			Played = &Member;
			PlayedInventory = Inventory;
		}
	}
	if (bSkill)
	{
		const FName CharacterKey = UPoolSubsystem::GetCharacterKey(DataComponent);
		Lines.Add(Played && PlayedInventory ? GiveHPCostSkill(*World, *Loadout, *Pool, CharacterKey, *PlayedInventory, *Played)
											: FString(TEXT("skill: the played character is not a party member here")));
	}
	if (bAll || CasterArg != INDEX_NONE)
	{
		Lines.Add(AddCaster(*World, *Pawn, *Session, Element));
	}
	if (bSkill && Played)
	{
		Lines.Add(DescribeFightLoadout(*World, *Loadout, *Played)); // last, so the report ends on what the fight will read
	}
	return Lines.IsEmpty() ? FString(TEXT("nothing asked: skill | caster [element] (or no argument for both)")) : FString::Join(Lines, TEXT("\n"));
}

#endif // !UE_BUILD_SHIPPING
