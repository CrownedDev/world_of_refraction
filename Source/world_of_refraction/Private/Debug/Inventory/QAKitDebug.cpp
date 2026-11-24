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

	/** The weapon in hand's skill list on Page (its tab), else the first weapon's; INDEX_NONE without one. */
	int32 FindWeaponTab(const FLoadoutSkillsPage &Page)
	{
		int32 First = INDEX_NONE;
		for (int32 Tab = 0; Tab < Page.Tabs.Num(); ++Tab)
		{
			if (Page.Tabs[Tab].List != ELoadoutSkillList::WeaponSkills)
			{
				continue;
			}
			if (Page.Tabs[Tab].Gear.bActive)
			{
				return Tab;
			}
			First = First == INDEX_NONE ? Tab : First;
		}
		return First;
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

	FString GiveHPCostSkill(UWorld &World, const ULoadoutComponent &Loadout, UPoolSubsystem &Pool, FName CharacterKey, const FLoadoutScreenMember &Member)
	{
		const UWeaponData *Weapon = Loadout.GetActiveWeapon();
		if (!Weapon)
		{
			return TEXT("skill: no weapon in hand");
		}
		FString Line = PutOfferedSkill(World, Member, *Weapon);
		TArray<FString> Learned;
		for (const UAbilityData *Ability : LoadAll<UAbilityData>())
		{
			if (!Line.IsEmpty() || Learned.Num() >= MAX_SKILLS_LEARNED)
			{
				break;
			}
			if (WoRQAKit::IsHPCostSkillFor(Ability, Weapon) && Pool.LearnSkillCopy(CharacterKey, Ability))
			{
				Learned.Add(Ability->GetName());
				Line = PutOfferedSkill(World, Member, *Weapon);
			}
		}
		if (Line.IsEmpty())
		{
			return FString::Printf(TEXT("skill: no ability that costs EP fits %s (%s, %s)%s"), *Weapon->GetName(), *UEnum::GetValueAsString(Weapon->WeaponType),
								   *UEnum::GetValueAsString(Weapon->WieldMode), Learned.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("; learned %s"), *FString::Join(Learned, TEXT(", "))));
		}
		return Learned.IsEmpty() ? Line : FString::Printf(TEXT("%s (learned %s first)"), *Line, *FString::Join(Learned, TEXT(", ")));
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
		TEXT("DEBUG (QA setup): [skill] the weapon in hand gets an ability that costs EP (B-592b's HP ghost); [caster [element]] a Caster joins as your seat "
			 "(the selector colour). No argument: both. Hub only, through the real API."),
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
	return Ability && Weapon && !Ability->bIsAttack && Ability->BaseEnergyCost > 0 && Ability->RequiredWeaponType == Weapon->WeaponType &&
		   Ability->RequiredWieldMode == Weapon->WieldMode;
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
	if (bAll || Args.ContainsByPredicate([](const FString &Arg) { return Arg.Equals(TEXT("skill"), ESearchCase::IgnoreCase); }))
	{
		const FName CharacterKey = UPoolSubsystem::GetCharacterKey(DataComponent);
		const FLoadoutScreenMember *Played = nullptr;
		const TArray<FLoadoutScreenMember> Members = WoRLoadoutScreen::BuildMembers(World);
		for (const FLoadoutScreenMember &Member : Members)
		{
			UInventoryComponent *Inventory = nullptr;
			if (WoRLoadoutScreen::FindMemberLoadout(World, Member, Inventory) == Loadout)
			{
				Played = &Member;
			}
		}
		Lines.Add(Played ? GiveHPCostSkill(*World, *Loadout, *Pool, CharacterKey, *Played) : FString(TEXT("skill: the played character is not a party member here")));
	}
	if (bAll || CasterArg != INDEX_NONE)
	{
		Lines.Add(AddCaster(*World, *Pawn, *Session, Element));
	}
	return Lines.IsEmpty() ? FString(TEXT("nothing asked: skill | caster [element] (or no argument for both)")) : FString::Join(Lines, TEXT("\n"));
}

#endif // !UE_BUILD_SHIPPING
