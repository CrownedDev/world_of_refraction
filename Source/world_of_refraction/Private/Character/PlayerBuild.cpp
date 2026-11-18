// PlayerBuild.cpp

#include "Character/PlayerBuild.h"

#include "Algo/Count.h"

#include "Character/CharacterData.h"
#include "Character/Forms/CharacterFormData.h"
#include "Character/Forms/CharacterFormSubsystem.h"
#include "Character/PlayerPicks.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryData.h"
#include "Loadout/ClassDefaultLoadoutData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Loadout/FSavedLoadout.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/WoRPlayerState.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Menus/NewGameScreenModel.h"
#include "UObject/Package.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRPlayerBuild"

const TCHAR *WoRPlayerBuild::BUILT_NAME = TEXT("WoRPlayer");

namespace
{
	const TCHAR *const RECRUIT_NAME_PART = TEXT("_Recruit_"); // GetBuiltName's recruit part
	constexpr int32 GUEST_NAME_MAX_LETTERS = 32; // a guest's player name in its object name: the guest's word, so capped
}

UCharacterData *WoRPlayerBuild::BuildFrom(const FPlayerPicks &Picks, const UCharacterData *Template,
										  const FClassElementNumbers *Numbers, int32 ActionSpeed, UInventoryData *Inventory,
										  FName ObjectName)
{
	if (!Template || !Picks.IsSet() || ObjectName.IsNone())
	{
		return nullptr;
	}
	UPackage *Package = GetTransientPackage();
	if (UObject *Older = StaticFindObjectFast(UCharacterData::StaticClass(), Package, ObjectName))
	{
		// The older build (the last game's player) gives up the name; whatever still holds it keeps a working object.
		const FName Away = MakeUniqueObjectName(Package, UCharacterData::StaticClass(), *(ObjectName.ToString() + TEXT("_Old")));
		Older->Rename(*Away.ToString(), Package, REN_DontCreateRedirectors | REN_NonTransactional);
	}
	UCharacterData *Built = DuplicateObject<UCharacterData>(Template, Package, ObjectName);
	Built->Name = WoRNewGame::GetCharacterName(Picks).ToString(); // D-228: named by what it is
	Built->CharacterClass = Picks.Class;
	const bool bCaster = Picks.Class == ECharacterClass::Caster;
	Built->InnateElement = bCaster ? Picks.Element : (Numbers ? Numbers->Element : Template->InnateElement);
	Built->bBrokenDarknessInnate = bCaster && Picks.bBrokenDarkness && Picks.Element == ESpellElement::Darkness;
	Built->ActionSpeed = FMath::Max(0, ActionSpeed); // the class table's 0 is the normal speed (no points)
	if (Numbers)
	{
		Built->WorldMindLevel = Numbers->WorldMindLevel;
		Built->WorldBodyLevel = Numbers->WorldBodyLevel;
		Built->WorldSpiritLevel = Numbers->WorldSpiritLevel;
		if (UTexture2D *Portrait = Numbers->Portrait.LoadSynchronous())
		{
			Built->Portrait = Portrait;
		}
	}
	Built->Inventory = Inventory;
	return Built;
}

UCharacterData *WoRPlayerBuild::Build(const FPlayerPicks &Picks, const UGameInstance *GameInstance)
{
	// B-557: a recruit's name stands for its fixed row, so one already built (the hub's, before a load) is the same one.
	// B-561: so does a guest's (its picks are its name), so a guest that leaves and comes back keeps its pool key.
	if (UCharacterData *Recruit = (Picks.bRecruit || Picks.bGuest) && Picks.IsSet() ? FindObjectFast<UCharacterData>(GetTransientPackage(), GetBuiltName(Picks)) : nullptr)
	{
		return Recruit;
	}
	const UPartySessionSubsystem *PartySession = GameInstance ? GameInstance->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	const UTrialRunSubsystem *TrialRun = GameInstance ? GameInstance->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	const UCharacterData *Template = PartySession && !PartySession->PlayerCharacter.IsNull() ? PartySession->PlayerCharacter.LoadSynchronous() : nullptr;
	if (!Picks.IsSet() || !Template)
	{
		UE_LOG(LogWoR, Warning, TEXT("[PlayerBuild] Not built: %s"), !Picks.IsSet() ? TEXT("no picks") : TEXT("no player asset (PartySessionSubsystem PlayerCharacter, DefaultGame.ini)"));
		return nullptr;
	}
	const UClassDefaultLoadoutData *Table = TrialRun && !TrialRun->GetClassDefaultLoadouts().IsNull() ? TrialRun->GetClassDefaultLoadouts().LoadSynchronous() : nullptr;
	FClassElementNumbers Numbers;
	int32 ActionSpeed = 0;
	const bool bNumbers = Table && Table->FindNumbers(Picks.Class, Picks.Element, Picks.bBrokenDarkness, Numbers, ActionSpeed);
	UE_CLOG(!bNumbers, LogWoR, Warning, TEXT("[PlayerBuild] No class table row for %s: the player asset's numbers stay"),
			*UEnum::GetValueAsString(Picks.Class));
	// B-556 (stage 4): the player's own inventory from the picks and the table (no table: the shared asset's).
	UInventoryData *Inventory = Table ? MakeInventory(Picks, *Table, bNumbers ? &Numbers : nullptr) : Template->Inventory;
	UCharacterData *Built = BuildFrom(Picks, Template, bNumbers ? &Numbers : nullptr, bNumbers ? ActionSpeed : Template->ActionSpeed,
									  Inventory, GetBuiltName(Picks));
	UE_LOG(LogWoR, Log, TEXT("[PlayerBuild] %s"), *GetBuiltString(Built));
	return Built;
}

UInventoryData *WoRPlayerBuild::MakeInventory(const FPlayerPicks &Picks, const UClassDefaultLoadoutData &Table, const FClassElementNumbers *Numbers)
{
	UPackage *Package = GetTransientPackage();
	UInventoryData *Inventory = NewObject<UInventoryData>(Package, MakeUniqueObjectName(Package, UInventoryData::StaticClass(), TEXT("WoRPlayerInventory")));
	// Minted once, on the first draw or trial (UPoolSubsystem::SeedStartingGear); the crystals banked once (BankStartingItems).
	Inventory->StartingGear = Table.MakePickedGear(Picks, /*Base=*/nullptr);
	Inventory->CrystalStock = UClassDefaultLoadoutData::GetPickedCrystals(Picks);
	// D-245: the class's starting skills are the new character's own from the first moment in the hub (known, not put
	// in a slot: D-224, Q9). A run still starts its list from the kit's (UTrialRunSubsystem, BeginKitRunSkills).
	// The player's own character only (Q-245): a guest's skills are its own save's and are handed back when it leaves
	// (B-564), and a hub recruit keeps its fixed row.
	if (const FClassDefaultLoadout *ClassKit = Picks.bGuest || Picks.bRecruit ? nullptr : Table.Defaults.Find(Picks.Class))
	{
		for (USkillDataBase *Skill : ClassKit->StartingSkills)
		{
			if (USpellData *Spell = Cast<USpellData>(Skill))
			{
				Inventory->Spells.AddUnique(Spell);
			}
			else if (UAbilityData *Ability = Cast<UAbilityData>(Skill))
			{
				Inventory->Abilities.AddUnique(Ability);
			}
		}
	}
	// The innate list and pools a pawn's first draw reads: the inventory's active authored loadout.
	FSavedLoadout Loadout;
	Loadout.LoadoutName = TEXT("Innate");
	for (USpellData *Spell : Numbers ? Numbers->InnateSpells : TArray<TObjectPtr<USpellData>>())
	{
		Loadout.InnateSpells.Add(FSpellRef(Spell));
	}
	for (const FBDElementSpellPool &Pool : Numbers ? Numbers->BDSpellPools : TArray<FBDElementSpellPool>())
	{
		FSavedBDElementSpellPool &Saved = Loadout.BDSpellPools.AddDefaulted_GetRef();
		Saved.Element = Pool.Element;
		for (USpellData *Spell : Pool.Spells)
		{
			Saved.Spells.Add(FSpellRef(Spell));
		}
	}
	Inventory->SavedLoadouts.Add(Loadout);
	Inventory->DefaultActiveLoadoutIndex = 0;
	return Inventory;
}

FName WoRPlayerBuild::GetBuiltName(const FPlayerPicks &Picks)
{
	if (Picks.bGuest)
	{
		// shortcut: two guests with the same player name, class and element share a name (and gear), and a guest that
		// comes back with other weapons or crystals keeps its first build; key it on an online id when that matters.
		FString Name;
		for (const TCHAR Letter : (Picks.GuestKey.IsEmpty() ? Picks.Name : Picks.GuestKey).Left(GUEST_NAME_MAX_LETTERS))
		{
			Name.AppendChar(FChar::IsAlnum(Letter) ? Letter : TEXT('_'));
		}
		return FName(FString::Printf(TEXT("%s_Guest_%s_%s_%s%s"), BUILT_NAME, *Name, *StaticEnum<ECharacterClass>()->GetNameStringByValue(static_cast<int64>(Picks.Class)),
									 *StaticEnum<ESpellElement>()->GetNameStringByValue(static_cast<int64>(Picks.Element)), Picks.bBrokenDarkness ? TEXT("_BD") : TEXT("")));
	}
	if (!Picks.bRecruit)
	{
		return FName(BUILT_NAME);
	}
	return FName(FString::Printf(TEXT("%s_Recruit_%s_%s%s"), BUILT_NAME, *StaticEnum<ECharacterClass>()->GetNameStringByValue(static_cast<int64>(Picks.Class)),
								 *StaticEnum<ESpellElement>()->GetNameStringByValue(static_cast<int64>(Picks.Element)),
								 Picks.bBrokenDarkness ? TEXT("_BD") : TEXT("")));
}

const FPlayerPicks *WoRPlayerBuild::FindRecruitPicks(const UCharacterData &Character, const UGameInstance *GameInstance)
{
	const UTrialRunSubsystem *TrialRun = GameInstance ? GameInstance->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	const UClassDefaultLoadoutData *Table = TrialRun && !TrialRun->GetClassDefaultLoadouts().IsNull() ? TrialRun->GetClassDefaultLoadouts().LoadSynchronous() : nullptr;
	return Table ? Table->FindRecruit(Character.CharacterClass, Character.InnateElement, Character.bBrokenDarknessInnate) : nullptr;
}

bool WoRPlayerBuild::RetagSameNames(TArrayView<FPartyMember> Members, const FString &HostTag)
{
	const auto IsPlayerMember = [](const FPartyMember &Member) { return IsBuilt(Member.CharacterData) && Member.Picks.IsSet() && !Member.Picks.bRecruit; };
	bool bChanged = false;
	for (const FPartyMember &Member : Members)
	{
		if (!IsPlayerMember(Member))
		{
			continue;
		}
		const FText KindName = WoRNewGame::GetCharacterName(Member.Picks);
		const bool bClash = Algo::CountIf(Members, [&](const FPartyMember &Other)
		{
			return IsPlayerMember(Other) && WoRNewGame::GetCharacterName(Other.Picks).EqualTo(KindName);
		}) > 1;
		const FString &Tag = Member.Picks.bGuest ? Member.Picks.GuestKey : HostTag;
		const FText TagText = Tag.IsEmpty() ? (Member.Picks.bGuest ? LOCTEXT("GuestTag", "Guest") : LOCTEXT("HostTag", "Host")) : FText::FromString(Tag);
		const FString Shown = bClash ? FText::Format(LOCTEXT("TaggedName", "{0} ({1})"), KindName, TagText).ToString() : KindName.ToString();
		if (Member.CharacterData->Name != Shown)
		{
			Member.CharacterData->Name = Shown;
			bChanged = true;
		}
	}
	return bChanged;
}

bool WoRPlayerBuild::IsPlayerCharacter(const UCharacterData *Character)
{
	return IsBuilt(Character) && !Character->GetName().StartsWith(FString(BUILT_NAME) + RECRUIT_NAME_PART);
}

FText WoRPlayerBuild::MakeCardName(const UCharacterData &Character, bool bPlayerCharacter, bool bViewersOwn, const FString &OwnerTag)
{
	if (!bPlayerCharacter)
	{
		return FText::FromString(Character.Name);
	}
	if (bViewersOwn)
	{
		return WoRGameNames::GetYouWord();
	}
	return OwnerTag.IsEmpty() ? LOCTEXT("NoTag", "Player") : FText::FromString(OwnerTag);
}

namespace
{
	/** D-229: the player claiming this character in World (null: nobody yet), and whether it is World's viewer's own. */
	const AWoRPlayerState *FindClaimingPlayer(const UCharacterData *Character, const FGuid &MemberId, const UWorld *World, bool &bOutViewersOwn)
	{
		const AGameStateBase *GameState = World ? World->GetGameState() : nullptr;
		const AWoRPlayerState *Owner = nullptr;
		for (int32 Index = 0; GameState && !Owner && Index < GameState->PlayerArray.Num(); ++Index)
		{
			const AWoRPlayerState *Player = Cast<AWoRPlayerState>(GameState->PlayerArray[Index]);
			if (Player && ((MemberId.IsValid() && Player->ClaimsMember(MemberId)) || Player->ClaimsCharacter(Character)))
			{
				Owner = Player;
			}
		}
		const APlayerController *Viewer = World ? World->GetFirstPlayerController() : nullptr;
		// Unclaimed: the viewer's own only when alone (solo); with other players it may be theirs, so neutral until a claim arrives.
		const bool bAlone = !GameState || GameState->PlayerArray.Num() <= 1;
		bOutViewersOwn = Owner ? Viewer && Viewer->PlayerState == Owner : bAlone;
		return Owner;
	}
}

FText WoRPlayerBuild::GetCardName(const UCharacterData *Character, const FGuid &MemberId, const UWorld *World)
{
	if (!Character)
	{
		return FText::GetEmpty();
	}
	bool bViewersOwn = false;
	const AWoRPlayerState *Owner = FindClaimingPlayer(Character, MemberId, World, bViewersOwn);
	return MakeCardName(*Character, IsPlayerCharacter(Character), bViewersOwn, Owner ? Owner->GetPlayerName() : FString());
}

FText WoRPlayerBuild::GetSpiritName(ECharacterClass Class, ESpellElement Element, bool bBrokenDarkness)
{
	return Class == ECharacterClass::Caster ? WoRNewGame::GetRefractorChoiceName({Element, bBrokenDarkness}) : WoRGameNames::GetCharacterClassName(Class);
}

FText WoRPlayerBuild::MakeIdentityLines(const FText &SpiritName, const FText &BodyName)
{
	const FText Spirit = WoRGameNames::FormatSpiritOf(SpiritName);
	return BodyName.IsEmpty() ? Spirit : FText::Format(LOCTEXT("IdentityLines", "{0}\n{1}"), Spirit, WoRGameNames::FormatBodyOf(BodyName));
}

FText WoRPlayerBuild::GetLocalBodyName(const UGameInstance *GameInstance)
{
	const UCharacterFormSubsystem *Forms = GameInstance ? GameInstance->GetSubsystem<UCharacterFormSubsystem>() : nullptr;
	if (!Forms)
	{
		return FText::GetEmpty();
	}
	const UCharacterFormData *Worn = Forms->GetWornForm();
	return Worn ? Worn->FormName : WoRGameNames::GetBaseFormName(Forms->GetBaseForm());
}

FText WoRPlayerBuild::GetDetailLine(const UCharacterData *Character, const FGuid &MemberId, const UWorld *World)
{
	if (!IsPlayerCharacter(Character))
	{
		return FText::GetEmpty();
	}
	bool bViewersOwn = false;
	FindClaimingPlayer(Character, MemberId, World, bViewersOwn);
	// shortcut: another player's body isn't known on this machine (its forms live in its own game): its Spirit line only.
	const FText Body = bViewersOwn && World ? GetLocalBodyName(World->GetGameInstance()) : FText::GetEmpty();
	return MakeIdentityLines(GetSpiritName(Character->CharacterClass, Character->InnateElement, Character->bBrokenDarknessInnate), Body);
}

FText WoRPlayerBuild::GetMemberListName(const FPartyMember &Member, TConstArrayView<FPartyMember> Members, const UWorld *World)
{
	const FText Name = GetCardName(Member.CharacterData, Member.MemberId, World);
	if (!IsPlayerCharacter(Member.CharacterData))
	{
		return Name;
	}
	bool bViewersOwn = false;
	const AWoRPlayerState *Owner = FindClaimingPlayer(Member.CharacterData, Member.MemberId, World, bViewersOwn);
	const int32 SameOwner = Algo::CountIf(Members, [Owner, World](const FPartyMember &Other)
	{
		bool bOtherOwn = false;
		return IsPlayerCharacter(Other.CharacterData) && FindClaimingPlayer(Other.CharacterData, Other.MemberId, World, bOtherOwn) == Owner;
	});
	// D-241: one player's two characters read alike by name alone; their identity lines tell them apart.
	return SameOwner > 1 ? FText::Format(LOCTEXT("NameAndIdentity", "{0}\n{1}"), Name, GetDetailLine(Member.CharacterData, Member.MemberId, World)) : Name;
}


bool WoRPlayerBuild::IsBuilt(const UCharacterData *Character)
{
	return Character && Character->GetOuter() == GetTransientPackage() && Character->GetName().StartsWith(BUILT_NAME);
}

FString WoRPlayerBuild::GetBuiltString(const UCharacterData *Character)
{
	if (!Character)
	{
		return TEXT("no character");
	}
	return FString::Printf(TEXT("%s '%s': %s, element %s%s, Mind / Body / Spirit %d / %d / %d, action speed %d, inventory %s%s"),
						   *Character->GetName(), *Character->Name, *UEnum::GetValueAsString(Character->CharacterClass),
						   *UEnum::GetValueAsString(Character->InnateElement), Character->bBrokenDarknessInnate ? TEXT(" (Broken Darkness)") : TEXT(""),
						   Character->WorldMindLevel, Character->WorldBodyLevel, Character->WorldSpiritLevel, Character->ActionSpeed,
						   *GetNameSafe(Character->Inventory), IsBuilt(Character) ? TEXT("") : TEXT(" (not built: an authored asset)"));
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GPlayerBuildPrintCommand(
	TEXT("wor.Player.PrintBuilt"),
	TEXT("B-554: log the party leader's character data: built from the picks (name, class, element, levels) or an authored asset."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
		const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
		const FPartyMember *Leader = Party && Party->Members.IsValidIndex(PartyConstants::LEADER_SLOT) ? &Party->Members[PartyConstants::LEADER_SLOT] : nullptr;
		UE_LOG(LogWoR, Log, TEXT("[PlayerBuild] Leader: %s"), Leader ? *WoRPlayerBuild::GetBuiltString(Leader->CharacterData) : TEXT("no party"));
	}));
#endif

#undef LOCTEXT_NAMESPACE
