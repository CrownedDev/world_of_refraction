// NewGameScreenModel.cpp

#include "UI/Menus/NewGameScreenModel.h"

#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Loadout/LoadoutRules.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Trial/TrialRunSubsystem.h"
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
		return FText::Format(LOCTEXT("ResonatorLine", "Up to five adapters and one weapon. Budget {0}."), Budget);
	}
	return FText::GetEmpty();
}

FText WoRNewGame::GetPanelTitle(const FNewGamePick &Pick)
{
	const FText ClassName = WoRGameNames::GetCharacterClassName(Pick.Class);
	return Pick.Class == ECharacterClass::Caster
		? FText::Format(LOCTEXT("RefractorTitle", "{0} {1}"), WoRGameNames::GetElementName(Pick.Element), ClassName)
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
							 WoRGameNames::GetElementName(Pick.Element));
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
		: LOCTEXT("OtherFootnote", "Gear, skills and forms are earned by playing.");
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
		if (!Data || Data->CharacterClass != Pick.Class || Data->bBrokenDarknessInnate)
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
	if (Saves->IsPlayingHostCopy())
	{
		return LOCTEXT("FriendsRun", "A new game can't start during a friend's run.");
	}

	FPartyMember Leader;
	Leader.CharacterData = FindStartCharacter(PartySession->NewGameCharacters, Pick);
	// The player's pawn class, as EnsureLocalParty resolves it: the pawn standing in the world, else the config's.
	const APawn *Pawn = PC->GetPawn();
	Leader.PawnClass = Pawn && Pawn->IsA<ACombatCharacter>() ? TSoftClassPtr<ACombatCharacter>(Pawn->GetClass()->GetPathName())
															  : PartySession->DefaultSoloPawnClass;
	if (!Leader.CharacterData)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[NewGame] No start character for %s (UPartySessionSubsystem::NewGameCharacters)"),
			   *GetPanelTitle(Pick).ToString());
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
	// new game's. StartFreshGame resumes writes.
	if (Saves->IsBoundToPlayer())
	{
		Saves->DeleteSave();
	}
	// The trial's exit ends a run under way, then travels to the hub on the next tick, where the player's pawn comes
	// back as the picked character.
	TrialRun->ExitTrial();
	if (!Saves->StartFreshGame(Leader, Pick.Body))
	{
		return LOCTEXT("NotStarted", "The new game didn't start."); // not reached: Begin checked what StartFreshGame refuses
	}
	UE_LOG(LogWoRUI, Display, TEXT("[NewGame] Begin as %s (%s), %s"), *GetPanelTitle(Pick).ToString(),
		   *Leader.CharacterData->GetName(), *UEnum::GetValueAsString(Pick.Body));
	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
