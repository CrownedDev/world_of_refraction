// TestLoadoutDebug.cpp

#include "Debug/Inventory/TestLoadoutDebug.h"

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Elements/ElementTable.h"
#include "Engine/World.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "UI/Menus/LoadoutV2Model.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

namespace
{
	const TCHAR *const GENERIC_CHARACTER_PATH = TEXT("/Game/Data/Characters/Data/Generic/DA_Character_GenericLord.DA_Character_GenericLord");
	const TCHAR *const REFRACTOR_CHARACTER_PATH = TEXT("/Game/Data/Characters/Data/Caster/Fire/DA_Character_FireLord.DA_Character_FireLord");
	const TCHAR *const RESONATOR_CHARACTER_PATH = TEXT("/Game/Data/Characters/Data/Resonator/DA_Character_ResonatorLord.DA_Character_ResonatorLord");
	const TCHAR *const BROKEN_DARKNESS_CHARACTER_PATH = TEXT("/Game/Data/Characters/Data/Caster/BrokenDarkness/DA_Character_BrokenDarknessLord.DA_Character_BrokenDarknessLord");
	const TCHAR *const REALITY_CHARACTER_PATH = TEXT("/Game/Data/Characters/Data/Caster/Reality/DA_Character_RealityLord.DA_Character_RealityLord");

	const TCHAR *const FIRST_WEAPON_PATH = TEXT("/Game/Data/Pool/Generic/Weapons/Sword/DA_Weapon_ArmingSword.DA_Weapon_ArmingSword");
	const TCHAR *const SECOND_WEAPON_PATH = TEXT("/Game/Data/Pool/Generic/Weapons/Greatsword/DA_Weapon_Longsword.DA_Weapon_Longsword");
	const TCHAR *const THIRD_WEAPON_PATH = TEXT("/Game/Data/Pool/Generic/Weapons/Axe/DA_Weapon_BattleAxe.DA_Weapon_BattleAxe");
	const TCHAR *const FIRST_ADAPTER_PATH = TEXT("/Game/Data/Pool/Generic/Rings/DA_Ring_Generic.DA_Ring_Generic");
	const TCHAR *const SECOND_ADAPTER_PATH = TEXT("/Game/Data/Pool/Fire/Rings/DA_Ring_Emberflame.DA_Ring_Emberflame");

	/** The crystals' elements, in order (the innate element and the evolution's are skipped). */
	const ESpellElement CRYSTAL_ELEMENT_ORDER[] = {ESpellElement::Fire, ESpellElement::Water, ESpellElement::Lightning, ESpellElement::Earth,
												   ESpellElement::Wind, ESpellElement::Light, ESpellElement::Void};
	const ESpellElement EVOLUTION_ELEMENT_ORDER[] = {ESpellElement::Water, ESpellElement::Lightning, ESpellElement::Wind, ESpellElement::Earth,
													 ESpellElement::Fire};
	/** A crystal for each piece that can take one: three weapons and two adapters, one of them holding the evolution. */
	constexpr int32 TEST_CRYSTAL_COUNT = 4;
	constexpr EItemTier TEST_CRYSTAL_TIER = EItemTier::F_Tier;

	FString GLastReport = TEXT("(none yet)");

	enum class EPieceSocket : uint8
	{
		Crystal,
		Evolution,
		/** The evolution if it isn't in yet, else a crystal. */
		EvolutionIfFree
	};

	struct FTestPiece
	{
		bool bWeapon = true;
		const TCHAR *Path = nullptr;
		EPieceSocket Socket = EPieceSocket::Crystal;
		int32 CrystalIndex = 0;
	};

	/** The order the pieces are tried in: a Refractor's (a Caster's) first entry is its adapter, so adapters go first. */
	TArray<FTestPiece> GetPieces(ECharacterClass Class)
	{
		TArray<FTestPiece> Weapons = {{true, FIRST_WEAPON_PATH, EPieceSocket::Crystal, 0},
									  {true, SECOND_WEAPON_PATH, EPieceSocket::Evolution, 0},
									  {true, THIRD_WEAPON_PATH, EPieceSocket::Crystal, 1}};
		TArray<FTestPiece> Adapters = {{false, FIRST_ADAPTER_PATH, EPieceSocket::Crystal, 2},
									   {false, SECOND_ADAPTER_PATH, EPieceSocket::EvolutionIfFree, 3}};
		if (Class == ECharacterClass::Caster)
		{
			Adapters.Append(Weapons);
			return Adapters;
		}
		Weapons.Append(Adapters);
		return Weapons;
	}

	/** Takes the active loadout's gear out (weapons, adapters, the evolution); innate spells, items and apparel stay.
	 *  TODO: B-309d: writes the slots once they are the stored truth. */
	void ClearGear(FCombatLoadout &Loadout)
	{
		Loadout.bShowPrimary = true;
		Loadout.PrimarySlotType = EPrimarySlotType::None;
		Loadout.PrimaryWeapon = FWeaponLoadoutEntry();
		Loadout.PrimaryRing = FRingLoadoutEntry();
		Loadout.PrimaryEvolution = FEvolutionAttachment();
		Loadout.PrimaryEvolutionInstance.Invalidate();
		Loadout.EvolutionSpells.Reset();
		Loadout.SecondarySlotType = ESecondarySlotType::None;
		Loadout.SecondaryWeapon = FWeaponLoadoutEntry();
		Loadout.RingLoadout.Reset();
		Loadout.ActiveRingIndex = 0;
	}

	/** The owned copy of Asset, given now when the character has none (one copy per character, D-115). */
	FGuid FindOrAddWeapon(UInventoryComponent &Inventory, UWeaponData *Weapon)
	{
		if (!Weapon)
		{
			return FGuid();
		}
		const FWeaponInventoryEntry *Owned = Inventory.Weapons.FindByPredicate([Weapon](const FWeaponInventoryEntry &Entry) { return Entry.Weapon == Weapon; });
		if (Owned)
		{
			return Owned->PersistentID;
		}
		return Inventory.AddWeapon(Weapon) && !Inventory.Weapons.IsEmpty() ? Inventory.Weapons.Last().PersistentID : FGuid();
	}

	FGuid FindOrAddRing(UInventoryComponent &Inventory, URingData *Ring)
	{
		if (!Ring)
		{
			return FGuid();
		}
		const FRingInventoryEntry *Owned = Inventory.Rings.FindByPredicate([Ring](const FRingInventoryEntry &Entry) { return Entry.Ring == Ring; });
		if (Owned)
		{
			return Owned->PersistentID;
		}
		return Inventory.AddRing(Ring) && !Inventory.Rings.IsEmpty() ? Inventory.Rings.Last().PersistentID : FGuid();
	}

	FGuid FindOrAddEvolution(UInventoryComponent &Inventory, UEvolutionInventoryComponent *Evolutions, UEvolutionItemData *Item)
	{
		if (!Evolutions || !Item)
		{
			return FGuid();
		}
		const FEvolutionInventoryEntry *Owned = Evolutions->Entries.FindByPredicate([Item](const FEvolutionInventoryEntry &Entry) { return Entry.Item == Item; });
		if (Owned)
		{
			return Owned->InstanceID;
		}
		return Inventory.AddEvolutionInstance(Item) && !Evolutions->Entries.IsEmpty() ? Evolutions->Entries.Last().InstanceID : FGuid();
	}

	/** Fills an empty socket 1 of the owned piece: the evolution or a crystal of Element (given first). True when the
	 *  evolution went in now. */
	bool FillSocket(UInventoryComponent &Inventory, bool bWeapon, const FGuid &GearID, bool bEvolution, const FGuid &EvolutionID, ESpellElement Element)
	{
		const FRuntimeAttachedItem *Socket = bWeapon ? (Inventory.FindOwnedWeapon(GearID) ? &Inventory.FindOwnedWeapon(GearID)->AttachedItem : nullptr)
													 : (Inventory.FindOwnedRing(GearID) ? &Inventory.FindOwnedRing(GearID)->AttachedItem : nullptr);
		if (!Socket || !Socket->IsEmpty())
		{
			return false; // already holds something (a run before this one): kept
		}
		if (bEvolution)
		{
			return bWeapon ? Inventory.AttachEvolutionToWeapon(GearID, EvolutionID) : Inventory.AttachEvolutionToRing(GearID, EvolutionID);
		}
		const FCrystalId Crystal(ElementTable::ForElement(Element).Gem, TEST_CRYSTAL_TIER);
		if (Inventory.AddCrystal(Crystal))
		{
			bWeapon ? Inventory.AttachCrystalToWeapon(GearID, Crystal) : Inventory.AttachCrystalToRing(GearID, Crystal);
		}
		return false;
	}

	/** Puts one draft in through the v2 screen's path; empty on success, else why not. */
	FText TryApply(ULoadoutComponent &Loadout, UInventoryComponent &Inventory, ECharacterClass Class, const FLoadoutV2Draft &Draft)
	{
		bool bApplied = false;
		FText ApplyRefusal;
		FText Refusal;
		const bool bDone = Loadout.TryChangeGear(Inventory.ActiveLoadoutIndex, [&Draft, Class, &bApplied, &ApplyRefusal](FCombatLoadout &Changed)
												 { bApplied = WoRLoadoutV2::ApplyDraft(Changed, Class, Draft, ApplyRefusal); },
												 Refusal);
		return !bDone ? Refusal : (bApplied ? FText::GetEmpty() : ApplyRefusal);
	}

	void GiveTestLoadoutCommand(const TArray<FString> &Args, UWorld *World)
	{
		ETestLoadoutVariant Variant = ETestLoadoutVariant::Played;
		if (!WoRTestLoadout::ParseVariant(Args.IsEmpty() ? FString() : Args[0], Variant))
		{
			UE_LOG(LogWoRLoadout, Warning, TEXT("[TestLoadout] wor.Debug.TestLoadout [Played|Generic|Refractor|Resonator|BrokenDarkness|Reality]: '%s' isn't one"), *Args[0]);
			return;
		}
		const APlayerController *Player = World ? World->GetFirstPlayerController() : nullptr;
		UE_LOG(LogWoRLoadout, Log, TEXT("%s"), *WoRTestLoadout::GiveTestLoadout(Player ? Player->GetPawn() : nullptr, Variant));
	}

	FAutoConsoleCommandWithWorldAndArgs GTestLoadoutCmd(
		TEXT("wor.Debug.TestLoadout"),
		TEXT("DEBUG: wor.Debug.TestLoadout [Played|Generic|Refractor|Resonator|BrokenDarkness|Reality] - give the played character a full v2 loadout (weapons with different crystals, an adapter, an evolution) so R / LB steps real sources; a variant swaps the pawn to that class's test character first (B-503n)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GiveTestLoadoutCommand));

	FAutoConsoleCommand GTestLoadoutPrintCmd(
		TEXT("wor.Debug.PrintTestLoadout"),
		TEXT("DEBUG: what the last wor.Debug.TestLoadout put in and what was refused (B-503n)."),
		FConsoleCommandDelegate::CreateLambda([]() { UE_LOG(LogWoRLoadout, Log, TEXT("%s"), *WoRTestLoadout::GetTestLoadoutString()); }));
}

bool WoRTestLoadout::ParseVariant(const FString &Text, ETestLoadoutVariant &OutVariant)
{
	static const TPair<const TCHAR *, ETestLoadoutVariant> NAMES[] = {
		{TEXT("Played"), ETestLoadoutVariant::Played},       {TEXT("Generic"), ETestLoadoutVariant::Generic},
		{TEXT("Refractor"), ETestLoadoutVariant::Refractor}, {TEXT("Resonator"), ETestLoadoutVariant::Resonator},
		{TEXT("BrokenDarkness"), ETestLoadoutVariant::BrokenDarkness}, {TEXT("Reality"), ETestLoadoutVariant::Reality}};
	if (Text.IsEmpty())
	{
		OutVariant = ETestLoadoutVariant::Played;
		return true;
	}
	for (const TPair<const TCHAR *, ETestLoadoutVariant> &Name : NAMES)
	{
		if (Text.Equals(Name.Key, ESearchCase::IgnoreCase))
		{
			OutVariant = Name.Value;
			return true;
		}
	}
	return false;
}

FString WoRTestLoadout::GetVariantCharacterPath(ETestLoadoutVariant Variant)
{
	switch (Variant)
	{
	case ETestLoadoutVariant::Generic:
		return GENERIC_CHARACTER_PATH;
	case ETestLoadoutVariant::Refractor:
		return REFRACTOR_CHARACTER_PATH;
	case ETestLoadoutVariant::Resonator:
		return RESONATOR_CHARACTER_PATH;
	case ETestLoadoutVariant::BrokenDarkness:
		return BROKEN_DARKNESS_CHARACTER_PATH;
	case ETestLoadoutVariant::Reality:
		return REALITY_CHARACTER_PATH;
	case ETestLoadoutVariant::Played:
	default:
		return FString();
	}
}

TArray<ESpellElement> WoRTestLoadout::PickCrystalElements(ESpellElement Innate, ESpellElement Skip, int32 Count)
{
	TArray<ESpellElement> Elements;
	for (const ESpellElement Element : CRYSTAL_ELEMENT_ORDER)
	{
		if (Elements.Num() >= Count)
		{
			break;
		}
		if (Element != Innate && Element != Skip)
		{
			Elements.Add(Element);
		}
	}
	return Elements;
}

ESpellElement WoRTestLoadout::PickEvolutionElement(ESpellElement Innate)
{
	for (const ESpellElement Element : EVOLUTION_ELEMENT_ORDER)
	{
		if (Element != Innate)
		{
			return Element;
		}
	}
	return EVOLUTION_ELEMENT_ORDER[0];
}

FString WoRTestLoadout::GetEvolutionPath(ESpellElement Element)
{
	switch (Element)
	{
	case ESpellElement::Fire:
		return TEXT("/Game/Data/Pool/Fire/Evolutions/DA_Evo_Fire_EmberVow.DA_Evo_Fire_EmberVow");
	case ESpellElement::Water:
		return TEXT("/Game/Data/Pool/Water/Evolutions/DA_Evo_Water_Drifting.DA_Evo_Water_Drifting");
	case ESpellElement::Earth:
		return TEXT("/Game/Data/Pool/Earth/Evolutions/DA_Evo_Earth_Pace.DA_Evo_Earth_Pace");
	case ESpellElement::Wind:
		return TEXT("/Game/Data/Pool/Wind/Evolutions/DA_Evo_Wind_WindRider.DA_Evo_Wind_WindRider");
	case ESpellElement::Lightning:
		return TEXT("/Game/Data/Pool/Lightning/Evolutions/DA_Evo_Lightning_GenerationalIdea.DA_Evo_Lightning_GenerationalIdea");
	default:
		return FString();
	}
}

FString WoRTestLoadout::GiveTestLoadout(APawn *Pawn, ETestLoadoutVariant Variant)
{
	UCharacterDataComponent *CharComp = Pawn ? Pawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	UInventoryComponent *Inventory = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	ULoadoutComponent *Loadout = Pawn ? Pawn->FindComponentByClass<ULoadoutComponent>() : nullptr;
	if (!CharComp || !Inventory || !Loadout || !Pawn->HasAuthority())
	{
		GLastReport = TEXT("[TestLoadout] no played character with a loadout here (or not the server)");
		return GLastReport;
	}
	TArray<FString> Lines;
	const FString CharacterPath = GetVariantCharacterPath(Variant);
	if (!CharacterPath.IsEmpty())
	{
		UCharacterData *Data = LoadObject<UCharacterData>(nullptr, *CharacterPath);
		if (!Data)
		{
			GLastReport = FString::Printf(TEXT("[TestLoadout] the test character %s didn't load"), *CharacterPath);
			return GLastReport;
		}
		CharComp->CharacterData = Data; // the pawn only, until the next map (not the party record)
		CharComp->ServerSetBrokenDarkness(Data->bBrokenDarknessInnate);
		Lines.Add(FString::Printf(TEXT("plays as %s (this pawn only, until the next map)"), *Data->GetName()));
	}
	const UCharacterData *Data = CharComp->CharacterData;
	if (!Data)
	{
		GLastReport = TEXT("[TestLoadout] the played character has no character data");
		return GLastReport;
	}
	const ECharacterClass Class = Data->CharacterClass;
	const ESpellElement Innate = Data->CharacterClass == ECharacterClass::Caster ? Data->InnateElement : ESpellElement::None;

	FText Refusal;
	if (!Loadout->TryChangeGear(Inventory->ActiveLoadoutIndex, [](FCombatLoadout &Changed) { ClearGear(Changed); }, Refusal))
	{
		GLastReport = FString::Printf(TEXT("[TestLoadout] the loadout can't change now: %s"), *Refusal.ToString());
		return GLastReport;
	}

	UEvolutionInventoryComponent *Evolutions = Pawn->FindComponentByClass<UEvolutionInventoryComponent>();
	const ESpellElement EvolutionElement = PickEvolutionElement(Innate);
	UEvolutionItemData *EvolutionItem = LoadObject<UEvolutionItemData>(nullptr, *GetEvolutionPath(EvolutionElement));
	const FGuid EvolutionID = FindOrAddEvolution(*Inventory, Evolutions, EvolutionItem);
	bool bEvolutionIn = false;
	const TArray<ESpellElement> CrystalElements = PickCrystalElements(Innate, EvolutionElement, TEST_CRYSTAL_COUNT);

	for (const FTestPiece &Piece : GetPieces(Class))
	{
		const bool bWantsEvolution = EvolutionID.IsValid() && !bEvolutionIn && Piece.Socket != EPieceSocket::Crystal;
		if (Piece.Socket == EPieceSocket::Evolution && !bWantsEvolution)
		{
			continue; // the evolution's piece, with the evolution already in elsewhere (or none owned)
		}
		const ESpellElement Element = CrystalElements.IsValidIndex(Piece.CrystalIndex) ? CrystalElements[Piece.CrystalIndex] : ESpellElement::Fire;
		FLoadoutV2Draft Draft;
		FGuid GearID;
		if (Piece.bWeapon)
		{
			GearID = FindOrAddWeapon(*Inventory, LoadObject<UWeaponData>(nullptr, Piece.Path));
		}
		else
		{
			Draft.Kind = ELoadoutV2DraftKind::Adapter;
			GearID = FindOrAddRing(*Inventory, LoadObject<URingData>(nullptr, Piece.Path));
		}
		if (!GearID.IsValid())
		{
			Lines.Add(FString::Printf(TEXT("%s: not given (didn't load, or the inventory is full)"), Piece.Path));
			continue;
		}
		const bool bAttachedEvolution = FillSocket(*Inventory, Piece.bWeapon, GearID, bWantsEvolution, EvolutionID, Element);
		const FRuntimeAttachedItem *Socket1 = nullptr;
		FString PieceName;
		if (Piece.bWeapon)
		{
			Draft.MainHand = *Inventory->FindOwnedWeapon(GearID);
			Draft.Socket1 = Draft.MainHand.AttachedItem;
			Draft.Socket2 = Draft.MainHand.StoneSocket;
			Socket1 = &Draft.MainHand.AttachedItem;
			PieceName = GetNameSafe(Draft.MainHand.Weapon);
		}
		else
		{
			Draft.Adapter = *Inventory->FindOwnedRing(GearID);
			Draft.Socket1 = Draft.Adapter.AttachedItem;
			Draft.Socket2 = Draft.Adapter.StoneSocket;
			Socket1 = &Draft.Adapter.AttachedItem;
			PieceName = GetNameSafe(Draft.Adapter.Ring);
		}
		const FString SocketName = Socket1->IsEvolution() ? FString::Printf(TEXT("evolution %s"), *GetNameSafe(EvolutionItem))
														  : FString::Printf(TEXT("crystal (%s)"), *WoRGameNames::GetElementName(Element).ToString());
		const FText Refused = TryApply(*Loadout, *Inventory, Class, Draft);
		if (Refused.IsEmpty())
		{
			bEvolutionIn = bEvolutionIn || Socket1->IsEvolution();
			Lines.Add(FString::Printf(TEXT("%s with %s: in"), *PieceName, *SocketName));
			continue;
		}
		if (bAttachedEvolution)
		{
			// Refused: the evolution comes back off so another piece can take it.
			Piece.bWeapon ? Inventory->RemoveEvolutionFromWeapon(GearID) : Inventory->RemoveEvolutionFromRing(GearID);
		}
		Lines.Add(FString::Printf(TEXT("%s with %s: refused (%s), kept in storage"), *PieceName, *SocketName, *Refused.ToString()));
	}

	if (EvolutionID.IsValid() && !bEvolutionIn)
	{
		// No piece took it in a socket: the evolution on its own, in the first entry when that is free.
		FLoadoutV2Draft Draft;
		Draft.Kind = ELoadoutV2DraftKind::Evolution;
		Draft.EvolutionInstance = EvolutionID;
		if (const FEvolutionInventoryEntry *Owned = Evolutions ? Evolutions->Entries.FindByPredicate([&EvolutionID](const FEvolutionInventoryEntry &Entry)
																									 { return Entry.InstanceID == EvolutionID; })
															   : nullptr)
		{
			Draft.Socket1 = WoRLoadoutV2::MakeEvolutionAttachment(*Owned);
		}
		const FText Refused = TryApply(*Loadout, *Inventory, Class, Draft);
		Lines.Add(FString::Printf(TEXT("evolution %s on its own: %s"), *GetNameSafe(EvolutionItem),
								  Refused.IsEmpty() ? TEXT("in") : *FString::Printf(TEXT("refused (%s), kept in storage"), *Refused.ToString())));
	}
	else if (!EvolutionID.IsValid())
	{
		Lines.Add(FString::Printf(TEXT("no evolution (%s didn't load, or the cap is reached)"), *GetEvolutionPath(EvolutionElement)));
	}

	GLastReport = FString::Printf(TEXT("[TestLoadout] %s (%s): %s"), *Data->GetName(), *UEnum::GetValueAsString(Class), *FString::Join(Lines, TEXT("; ")));
	return GLastReport;
}

FString WoRTestLoadout::GetTestLoadoutString()
{
	return GLastReport;
}
