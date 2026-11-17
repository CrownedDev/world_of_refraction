// NewGameScreenModel.cpp

#include "UI/Menus/NewGameScreenModel.h"

#include "Character/CharacterData.h"
#include "Character/PlayerBuild.h"
#include "Character/CombatCharacter.h"
#include "Engine/GameInstance.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "GameFramework/PlayerController.h"
#include "Loadout/ClassDefaultLoadoutData.h"
#include "Loadout/LoadoutRules.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Menus/TitleScreenModel.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRNewGame"

const TArray<ECharacterClass> &WoRNewGame::GetClasses()
{
	static const TArray<ECharacterClass> Classes = {ECharacterClass::Generic, ECharacterClass::Caster, ECharacterClass::Resonator};
	return Classes;
}

const TArray<ESpellElement> &WoRNewGame::GetElements()
{
	static const TArray<ESpellElement> Elements = {ESpellElement::Fire, ESpellElement::Water, ESpellElement::Earth,
												   ESpellElement::Wind, ESpellElement::Light, ESpellElement::Darkness,
												   ESpellElement::Lightning, ESpellElement::Void, ESpellElement::Reality};
	return Elements;
}

const TArray<FNewGameRefractorChoice> &WoRNewGame::GetRefractorChoices()
{
	static const TArray<FNewGameRefractorChoice> Choices = []()
	{
		TArray<FNewGameRefractorChoice> Out;
		for (const ESpellElement Element : GetElements())
		{
			Out.Add({Element, false});
			if (Element == ESpellElement::Darkness)
			{
				Out.Add({ESpellElement::Darkness, true}); // D-228: Broken Darkness comes under the Refractor
			}
		}
		return Out;
	}();
	return Choices;
}

TArray<EWeaponType> WoRNewGame::GetWeaponChoicesFor(const UClassDefaultLoadoutData *Table)
{
	TArray<EWeaponType> All;
	TArray<EWeaponType> Stocked;
	for (int32 Value = 0; Value <= static_cast<int32>(EWeaponType::Shield); ++Value)
	{
		const EWeaponType Type = static_cast<EWeaponType>(Value);
		All.Add(Type);
		if (Table && Table->PickWeapons.FindRef(Type))
		{
			Stocked.Add(Type);
		}
	}
	// Nothing stocked (no table, no rows, only empty rows): every type, so a Generic can still finish the screen.
	return Stocked.IsEmpty() ? All : Stocked;
}

TArray<EWeaponType> WoRNewGame::GetWeaponChoices()
{
	const TSoftObjectPtr<UClassDefaultLoadoutData> &Table = GetDefault<UTrialRunSubsystem>()->GetClassDefaultLoadouts();
	return GetWeaponChoicesFor(Table.IsNull() ? nullptr : Table.LoadSynchronous());
}

const TArray<ECrystalType> &WoRNewGame::GetCrystalChoices()
{
	static const TArray<ECrystalType> Crystals = {ECrystalType::Garnet, ECrystalType::Sapphire, ECrystalType::Citrine,
												  ECrystalType::Emerald, ECrystalType::Amber, ECrystalType::Opal,
												  ECrystalType::Onyx, ECrystalType::Amethyst, ECrystalType::Iolite};
	return Crystals;
}

int32 WoRNewGame::GetClassChoiceCount()
{
	return GetClasses().Num() - 1 + GetRefractorChoices().Num(); // the Refractor counts once per choice
}

int32 WoRNewGame::GetPickChoiceCount(ECharacterClass Class)
{
	switch (Class)
	{
	case ECharacterClass::Generic:
		return GetWeaponChoices().Num();
	case ECharacterClass::Caster:
		return GetRefractorChoices().Num();
	case ECharacterClass::Resonator:
		return GetCrystalChoices().Num();
	}
	return 0;
}

FText WoRNewGame::GetPickChoiceName(ECharacterClass Class, int32 Index)
{
	if (Index < 0 || Index >= GetPickChoiceCount(Class))
	{
		return FText::GetEmpty();
	}
	switch (Class)
	{
	case ECharacterClass::Generic:
		return WoRGameNames::GetWeaponTypeName(GetWeaponChoices()[Index]);
	case ECharacterClass::Caster:
		return GetRefractorChoiceName(GetRefractorChoices()[Index]);
	case ECharacterClass::Resonator:
		return WoRGameNames::GetCrystalTypeName(GetCrystalChoices()[Index]);
	}
	return FText::GetEmpty();
}

bool WoRNewGame::GetPickChoiceElement(ECharacterClass Class, int32 Index, ESpellElement &OutElement)
{
	if (Index < 0 || Index >= GetPickChoiceCount(Class) || Class == ECharacterClass::Generic)
	{
		return false;
	}
	OutElement = Class == ECharacterClass::Caster ? GetRefractorChoices()[Index].Element : CrystalTypeHelpers::GetElement(GetCrystalChoices()[Index]);
	return true;
}

int32 WoRNewGame::GetPickCount(ECharacterClass Class)
{
	return Class == ECharacterClass::Caster ? 1 : MULTI_PICK_COUNT;
}

bool WoRNewGame::IsPicked(const FNewGamePick &Pick, int32 Index)
{
	if (Index < 0 || Index >= GetPickChoiceCount(Pick.Class))
	{
		return false;
	}
	switch (Pick.Class)
	{
	case ECharacterClass::Generic:
		return Pick.Weapons.Contains(GetWeaponChoices()[Index]);
	case ECharacterClass::Caster:
	{
		const FNewGameRefractorChoice &Choice = GetRefractorChoices()[Index];
		return Choice.Element == Pick.Element && Choice.bBrokenDarkness == Pick.bBrokenDarkness;
	}
	case ECharacterClass::Resonator:
		return Pick.Crystals.Contains(GetCrystalChoices()[Index]);
	}
	return false;
}

namespace
{
	template <typename T>
	void ToggleIn(TArray<T> &Picked, const T &Entry)
	{
		if (Picked.Remove(Entry) > 0)
		{
			return;
		}
		Picked.Add(Entry);
		while (Picked.Num() > WoRNewGame::MULTI_PICK_COUNT)
		{
			Picked.RemoveAt(0); // the oldest pick gives way
		}
	}
}

void WoRNewGame::TogglePick(FNewGamePick &Pick, int32 Index)
{
	if (Index < 0 || Index >= GetPickChoiceCount(Pick.Class))
	{
		return;
	}
	switch (Pick.Class)
	{
	case ECharacterClass::Generic:
		ToggleIn(Pick.Weapons, GetWeaponChoices()[Index]);
		break;
	case ECharacterClass::Caster:
		Pick.Element = GetRefractorChoices()[Index].Element;
		Pick.bBrokenDarkness = GetRefractorChoices()[Index].bBrokenDarkness;
		break;
	case ECharacterClass::Resonator:
		ToggleIn(Pick.Crystals, GetCrystalChoices()[Index]);
		break;
	}
}

FText WoRNewGame::GetPicksLabel(ECharacterClass Class)
{
	switch (Class)
	{
	case ECharacterClass::Generic:
		return LOCTEXT("PicksWeapons", "Two weapons");
	case ECharacterClass::Resonator:
		return LOCTEXT("PicksCrystals", "Two crystal types");
	default:
		return LOCTEXT("Element", "Element");
	}
}

FText WoRNewGame::GetPicksLine(const FNewGamePick &Pick)
{
	TArray<FText> Names;
	if (Pick.Class == ECharacterClass::Generic)
	{
		for (const EWeaponType Weapon : Pick.Weapons)
		{
			Names.Add(WoRGameNames::GetWeaponTypeName(Weapon));
		}
		return Names.IsEmpty() ? LOCTEXT("NoWeapons", "Pick two weapons. A sword and a shield are two picks.")
							   : FText::Format(LOCTEXT("WeaponsLine", "{0} (F tier)"), FText::Join(LOCTEXT("PickSeparator", ", "), Names));
	}
	if (Pick.Class == ECharacterClass::Resonator)
	{
		for (const ECrystalType Crystal : Pick.Crystals)
		{
			Names.Add(WoRGameNames::GetCrystalTypeName(Crystal));
		}
		return Names.IsEmpty() ? FText::Format(LOCTEXT("NoCrystals", "Pick two crystal types: {0} of each, for two empty adapters."), STARTING_CRYSTALS_PER_TYPE)
							   : FText::Format(LOCTEXT("CrystalsLine", "{0}: {1} of each (F tier)"), FText::Join(LOCTEXT("PickSeparator", ", "), Names), STARTING_CRYSTALS_PER_TYPE);
	}
	return FText::GetEmpty();
}

FText WoRNewGame::GetRefractorChoiceName(const FNewGameRefractorChoice &Choice)
{
	return Choice.bBrokenDarkness ? LOCTEXT("BrokenDarkness", "Broken Darkness") : WoRGameNames::GetElementName(Choice.Element);
}

FText WoRNewGame::GetClassKind(ECharacterClass Class)
{
	switch (Class)
	{
	case ECharacterClass::Generic:
		return LOCTEXT("GenericKind", "Weapons");
	case ECharacterClass::Caster:
		return LOCTEXT("RefractorKind", "Innate magic");
	case ECharacterClass::Resonator:
		return WoRGameNames::GetAdapterWordPlural();
	}
	return FText::GetEmpty();
}

FText WoRNewGame::GetClassLine(ECharacterClass Class)
{
	const int32 Budget = LoadoutClassTable::Get(Class).Budget;
	switch (Class)
	{
	case ECharacterClass::Generic:
		return FText::Format(LOCTEXT("GenericLine", "Two weapons, switch between them freely. Budget {0}."), Budget);
	case ECharacterClass::Caster:
		return FText::Format(LOCTEXT("RefractorLine", "Your own element and spell pool; one item. Budget {0}."), Budget);
	case ECharacterClass::Resonator:
		return FText::Format(LOCTEXT("ResonatorLine", "Up to {1} adapters and one weapon. Budget {0}."), Budget, LoadoutConstants::MAX_RESONATOR_ADAPTERS);
	}
	return FText::GetEmpty();
}

FText WoRNewGame::GetCharacterName(const FPlayerPicks &Picks)
{
	return Picks.bRecruit ? FText::FromString(Picks.Name) : GetPanelTitle(Picks);
}

FText WoRNewGame::GetPanelTitle(const FNewGamePick &Pick)
{
	const FText ClassName = WoRGameNames::GetCharacterClassName(Pick.Class);
	return Pick.Class == ECharacterClass::Caster
		? FText::Format(LOCTEXT("RefractorTitle", "{0} {1}"), GetRefractorChoiceName({Pick.Element, Pick.bBrokenDarkness}), ClassName)
		: ClassName;
}

FText WoRNewGame::GetAbout(const FNewGamePick &Pick)
{
	switch (Pick.Class)
	{
	case ECharacterClass::Generic:
		return LOCTEXT("GenericAbout", "Fights with what it holds. Abilities come from the weapon; elements from its crystal.");
	case ECharacterClass::Caster:
		return FText::Format(LOCTEXT("RefractorAbout", "Casts {0} from within. The only class with an innate element and spells of its own."),
							 GetRefractorChoiceName({Pick.Element, Pick.bBrokenDarkness}));
	case ECharacterClass::Resonator:
		return LOCTEXT("ResonatorAbout", "Channels through adapters, each with its own element and spells; switch between them in a fight.");
	}
	return FText::GetEmpty();
}

FText WoRNewGame::GetElementLine(ESpellElement Element)
{
	// Plain words for each element's full bar (Rules 03-status-bars SB-19 to SB-28).
	switch (Element)
	{
	case ESpellElement::Fire:
		return LOCTEXT("FireLine", "Burns: a full bar deals 8% of the target's max HP at its turn.");
	case ESpellElement::Water:
		return LOCTEXT("WaterLine", "Slows: a full bar cuts the target's Spell Speed.");
	case ESpellElement::Earth:
		return LOCTEXT("EarthLine", "Cracks armour: a full bar lowers the target's defence by 30%.");
	case ESpellElement::Wind:
		return LOCTEXT("WindLine", "Staggers: a full bar slows Action Speed and takes an action away.");
	case ESpellElement::Light:
		return LOCTEXT("LightLine", "Dazzles: a full bar stops the target landing critical hits.");
	case ESpellElement::Darkness:
		return LOCTEXT("DarknessLine", "Silences: a full bar seals half the target's EP.");
	case ESpellElement::Lightning:
		return LOCTEXT("LightningLine", "Stuns: a full bar leaves the target only Attack and Defend.");
	case ESpellElement::Void:
		return LOCTEXT("VoidLine", "Unravels: a full bar makes the target act at random for a turn.");
	case ESpellElement::Reality:
		return LOCTEXT("RealityLine", "Breaks: a full bar deals 25% of the target's max HP at once.");
	default:
		return FText::GetEmpty();
	}
}

FText WoRNewGame::GetPanelFootnote(const FNewGamePick &Pick)
{
	return Pick.Class == ECharacterClass::Caster
		? LOCTEXT("RefractorFootnote", "Your spells, your infusions and your status bar all use this element.")
		: Pick.Class == ECharacterClass::Resonator
		? LOCTEXT("ResonatorFootnote", "You start with two empty adapters and socket the crystals yourself. Every pick is F tier.")
		: LOCTEXT("GenericFootnote", "You start holding both weapons. Every pick is F tier.");
}

FText WoRNewGame::GetBeginRefusal(const FNewGamePick &Pick)
{
	const int32 Picked = Pick.Class == ECharacterClass::Generic ? Pick.Weapons.Num() : Pick.Class == ECharacterClass::Resonator ? Pick.Crystals.Num() : 1;
	if (Picked < GetPickCount(Pick.Class))
	{
		return FText::Format(LOCTEXT("PickMore", "{0}: pick {1}."), GetPicksLabel(Pick.Class), GetPickCount(Pick.Class));
	}
	return FText::GetEmpty();
}

FText WoRNewGame::GetBeginLabel(const FNewGamePick &Pick)
{
	return Pick.Class == ECharacterClass::Caster
		? FText::Format(LOCTEXT("BeginAsRefractor", "Begin as {0}"), GetPanelTitle(Pick))
		: FText::Format(LOCTEXT("BeginAsClass", "Begin as a {0}"), WoRGameNames::GetCharacterClassName(Pick.Class));
}

UCharacterData *WoRNewGame::FindStartCharacter(const TArray<TSoftObjectPtr<UCharacterData>> &Choices, const FNewGamePick &Pick)
{
	for (const TSoftObjectPtr<UCharacterData> &Choice : Choices)
	{
		UCharacterData *Data = Choice.LoadSynchronous();
		const bool bWantBrokenDarkness = Pick.Class == ECharacterClass::Caster && Pick.bBrokenDarkness;
		if (!Data || Data->CharacterClass != Pick.Class || Data->bBrokenDarknessInnate != bWantBrokenDarkness)
		{
			continue;
		}
		if (Pick.Class != ECharacterClass::Caster || Data->InnateElement == Pick.Element)
		{
			return Data;
		}
	}
	return nullptr;
}

FText WoRNewGame::Begin(APlayerController *PC, const FNewGamePick &Pick)
{
	UGameInstance *GI = PC ? PC->GetGameInstance() : nullptr;
	UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	USaveSubsystem *Saves = GI ? GI->GetSubsystem<USaveSubsystem>() : nullptr;
	UTrialRunSubsystem *TrialRun = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	if (!PartySession || !Saves || !TrialRun)
	{
		return LOCTEXT("NoGame", "The game isn't ready to start yet.");
	}
	const FText Refusal = GetBeginRefusal(Pick);
	if (!Refusal.IsEmpty())
	{
		return Refusal;
	}
	if (Saves->IsPlayingHostCopy())
	{
		return LOCTEXT("FriendsRun", "A new game can't start during a friend's run.");
	}

	// B-554 (D-229): the new character takes the first free slot; the others keep theirs.
	const int32 Slot = Saves->FindFreeSlot(WoRTitleScreen::GetMaxSlots());
	if (Slot == INDEX_NONE)
	{
		return LOCTEXT("SlotsFull", "Every character slot is in use: delete one on the title first.");
	}

	// B-554 (D-228): the player is built from the picks (the shared player asset, the class table, the picks), after the
	// trial's exit below: a build renames the last player's copy away, and the run's end still looks it up by name.
	FPartyMember Leader;
	Leader.Picks = Pick;
	Leader.Picks.Name = GetCharacterName(Pick).ToString(); // D-228: no typed name; a set Name marks the picks as made
	const UCharacterData *PlayerAsset = PartySession->PlayerCharacter.IsNull() ? nullptr : PartySession->PlayerCharacter.LoadSynchronous();
	// The player's pawn class, as EnsureLocalParty resolves it: the pawn standing in the world, else the config's.
	const APawn *Pawn = PC->GetPawn();
	Leader.PawnClass = Pawn && Pawn->IsA<ACombatCharacter>() ? TSoftClassPtr<ACombatCharacter>(Pawn->GetClass()->GetPathName())
															  : PartySession->DefaultSoloPawnClass;
	if (!PlayerAsset)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[NewGame] %s not built (UPartySessionSubsystem::PlayerCharacter)"), *GetPanelTitle(Pick).ToString());
		return FText::Format(LOCTEXT("NoCharacter", "{0} can't be started yet: no character is set up for it."), GetPanelTitle(Pick));
	}
	if (Leader.PawnClass.IsNull())
	{
		return LOCTEXT("NoPawn", "The game isn't ready to start yet: no player character class is set up.");
	}

	if (!TrialRun->HasHubLevel())
	{
		return LOCTEXT("NoHub", "The game isn't ready to start yet: no hub level is set up.");
	}

	// The old game's saves stop first: the trial's exit below saves it (async), and that write must not land after the
	// new game's. StartFreshGame resumes writes. B-554: the old character's file stays (its own slot); it was written
	// when the game menu's quit came back to the title (WoRPauseMenu::Quit), so the stop only keeps the title world's
	// capture off it.
	Saves->StopWrites();
	// The trial's exit ends a run under way, then travels to the hub on the next tick, where the player's pawn comes
	// back as the picked character.
	TrialRun->ExitTrial();
	Leader.CharacterData = WoRPlayerBuild::Build(Leader.Picks, GI);
	if (!Leader.CharacterData || !Saves->StartFreshGame(Leader, Pick.Body, Slot))
	{
		return LOCTEXT("NotStarted", "The new game didn't start."); // only if the build or the new slot failed (logged)
	}
	UE_LOG(LogWoRUI, Display, TEXT("[NewGame] Begin as %s (%s), %s"), *GetPanelTitle(Pick).ToString(),
		   *Leader.CharacterData->GetName(), *UEnum::GetValueAsString(Pick.Body));
	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
