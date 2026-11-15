// TestGearDebug.cpp

#include "Debug/Inventory/TestGearDebug.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Debug/Field/FieldMovementDebug.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Field/FieldMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/GearGive.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutComponent.h"
#include "Pool/PoolSubsystem.h"
#include "UI/Menus/LoadoutV2Model.h"
#include "WoRLog.h"

namespace
{
	// TODO: test assets; an editor step clears a crystal baked on them if Crown sees one arrive socketed (PIE sheet).
	const TCHAR *TEST_SWORD_PATH = TEXT("/Game/Data/Pool/Generic/Weapons/Sword/DA_Weapon_ArmingSword.DA_Weapon_ArmingSword");
	const TCHAR *TEST_RING_PATH = TEXT("/Game/Data/Pool/Generic/Rings/DA_Ring_Generic.DA_Ring_Generic");
	const ECrystalType TEST_CRYSTALS[] = {ECrystalType::Garnet, ECrystalType::Sapphire, ECrystalType::Citrine};
	constexpr EItemTier TEST_CRYSTAL_TIER = EItemTier::F_Tier;
	constexpr int32 TEST_CRYSTAL_COUNT = 1;
	constexpr float TEST_GEAR_SCREEN_SECONDS = 10.0f;

	struct FSavedClass
	{
		ECharacterClass Class = ECharacterClass::Generic;
		ESpellElement Innate = ESpellElement::Generic;
	};

	/** The CharacterData a variant changed, with what it had; put back when the PIE session ends. */
	TMap<TWeakObjectPtr<UCharacterData>, FSavedClass> GChangedClasses;

	void RestoreChangedClasses(UWorld *World, bool bSessionEnded, bool /*bCleanupResources*/)
	{
		// Map travel cleans up a world too: only the end of the session puts the classes back.
		if (!bSessionEnded || GChangedClasses.IsEmpty())
		{
			return;
		}
		for (const TPair<TWeakObjectPtr<UCharacterData>, FSavedClass> &Changed : GChangedClasses)
		{
			if (UCharacterData *Data = Changed.Key.Get())
			{
				Data->CharacterClass = Changed.Value.Class;
				Data->InnateElement = Changed.Value.Innate;
				UE_LOG(LogWoR, Log, TEXT("[TestGear] %s: class and innate put back"), *Data->GetName());
			}
		}
		GChangedClasses.Reset();
	}

	bool FindPlayedMember(UWorld *World, const ULoadoutComponent *PawnLoadout, FLoadoutScreenMember &OutMember)
	{
		for (const FLoadoutScreenMember &Member : WoRLoadoutScreen::BuildMembers(World))
		{
			UInventoryComponent *Inventory = nullptr;
			if (WoRLoadoutScreen::FindMemberLoadout(World, Member, Inventory) == PawnLoadout)
			{
				OutMember = Member;
				return true;
			}
		}
		return false;
	}

	template <class TEntry, class TAsset>
	FGuid FindOwned(const TArray<TEntry> &Owned, const TAsset *Asset, TAsset *TEntry::*Field)
	{
		const TEntry *Found = Owned.FindByPredicate([Asset, Field](const TEntry &Entry) { return Entry.*Field == Asset; });
		return Found ? Found->PersistentID : FGuid();
	}

	/** Changes the loadout through the screen's door (TryChangeGear); false with the reason in Lines when refused. */
	bool ChangeGear(ULoadoutComponent &Loadout, const UInventoryComponent &Inventory, TFunctionRef<bool(FCombatLoadout &, FText &)> Change, const TCHAR *What,
					TArray<FString> &Lines)
	{
		bool bApplied = false;
		FText ApplyRefusal;
		FText Refusal;
		const bool bDone = Loadout.TryChangeGear(Inventory.ActiveLoadoutIndex, [&](FCombatLoadout &Changed) { bApplied = Change(Changed, ApplyRefusal); }, Refusal);
		if (!bDone || !bApplied)
		{
			Lines.Add(FString::Printf(TEXT("%s: refused (%s)"), What, *(bDone ? ApplyRefusal : Refusal).ToString()));
			return false;
		}
		Lines.Add(FString::Printf(TEXT("%s: in"), What));
		return true;
	}

	void ApplyVariant(UWorld *World, const FString &Variant, ULoadoutComponent &Loadout, UInventoryComponent &Inventory, UCharacterDataComponent &DataComponent,
					  TArray<FString> &Lines)
	{
		UCharacterData *Data = DataComponent.CharacterData;
		if (!Data)
		{
			Lines.Add(TEXT("variant: the character has no CharacterData"));
			return;
		}
		ECharacterClass Class = ECharacterClass::Caster;
		ESpellElement Innate = Data->InnateElement;
		if (Variant == TEXT("refractor"))
		{
			if (Innate == ESpellElement::Generic || Innate == ESpellElement::None)
			{
				Innate = ESpellElement::Fire; // a Refractor needs an element of its own
			}
		}
		else if (Variant == TEXT("reality"))
		{
			Innate = ESpellElement::Reality;
		}
		else if (Variant == TEXT("bd"))
		{
			Innate = ESpellElement::Darkness;
		}
		else if (Variant == TEXT("resonator"))
		{
			Class = ECharacterClass::Resonator;
		}
		else
		{
			Lines.Add(FString::Printf(TEXT("variant '%s' unknown (refractor | resonator | bd | reality)"), *Variant));
			return;
		}

		// Out under the old class, so its own entries are the ones counted; the first stays (D-116).
		const ECharacterClass OldClass = Data->CharacterClass;
		ChangeGear(Loadout, Inventory, [OldClass](FCombatLoadout &Changed, FText &) { WoRLoadoutV2::RemoveAllGear(Changed, OldClass); return true; },
				   TEXT("every entry but the first out"), Lines);

		static bool bRestoreBound = false;
		if (!bRestoreBound)
		{
			FWorldDelegates::OnWorldCleanup.AddStatic(&RestoreChangedClasses);
			bRestoreBound = true;
		}
		if (!GChangedClasses.Contains(Data))
		{
			GChangedClasses.Add(Data, FSavedClass{Data->CharacterClass, Data->InnateElement}); // the first change keeps the original
		}
		Data->CharacterClass = Class;
		Data->InnateElement = Innate;
		if (Variant == TEXT("bd"))
		{
			DataComponent.ServerSetBrokenDarkness(true);
		}
		// The gear the class fights with is read again (TryChangeGear re-reads after any change).
		ChangeGear(Loadout, Inventory, [](FCombatLoadout &, FText &) { return true; }, TEXT("re-read for the new class"), Lines);
		Lines.Add(FString::Printf(TEXT("variant %s: class %s, innate %s (put back when PIE ends)"), *Variant, *UEnum::GetValueAsString(Class),
								  *UEnum::GetValueAsString(Innate)));
	}

	void Fill(UWorld *World, const FLoadoutScreenMember &Member, ULoadoutComponent &Loadout, UInventoryComponent &Inventory, const FGuid &SwordID, const FGuid &RingID,
			  TArray<FString> &Lines)
	{
		const FCombatLoadout &Active = Inventory.SavedLoadouts[Inventory.ActiveLoadoutIndex];
		for (const FLoadoutV2MainHandChoice &Choice : WoRLoadoutV2::BuildMainHandChoices(Active, Member.Class, Inventory.Weapons))
		{
			if (Choice.Weapon.PersistentID != SwordID)
			{
				continue;
			}
			if (!Choice.Refusal.IsEmpty())
			{
				Lines.Add(FString::Printf(TEXT("sword entry: refused (%s)"), *Choice.Refusal.ToString()));
				break;
			}
			FLoadoutV2Draft Draft;
			Draft.Kind = ELoadoutV2DraftKind::Weapon;
			Draft.MainHand = Choice.Weapon;
			Draft.Socket1 = Choice.Weapon.AttachedItem; // the sockets stay on their piece (Q-476c)
			Draft.Socket2 = Choice.Weapon.StoneSocket;
			const ECharacterClass Class = Member.Class;
			ChangeGear(Loadout, Inventory, [&Draft, Class](FCombatLoadout &Changed, FText &Refusal) { return WoRLoadoutV2::ApplyDraft(Changed, Class, Draft, Refusal); },
					   TEXT("sword entry"), Lines);
			break;
		}
		for (const FLoadoutV2PieceChoice &Choice : WoRLoadoutV2::BuildAdapterChoices(Inventory.SavedLoadouts[Inventory.ActiveLoadoutIndex], Member.Class, Inventory.Rings))
		{
			if (Choice.Adapter.PersistentID != RingID)
			{
				continue;
			}
			if (!Choice.Refusal.IsEmpty())
			{
				Lines.Add(FString::Printf(TEXT("ring entry: refused (%s)"), *Choice.Refusal.ToString()));
				break;
			}
			FLoadoutV2Draft Draft;
			Draft.Kind = ELoadoutV2DraftKind::Adapter;
			Draft.Adapter = Choice.Adapter;
			Draft.Socket1 = Choice.Adapter.AttachedItem;
			Draft.Socket2 = Choice.Adapter.StoneSocket;
			const ECharacterClass Class = Member.Class;
			ChangeGear(Loadout, Inventory, [&Draft, Class](FCombatLoadout &Changed, FText &Refusal) { return WoRLoadoutV2::ApplyDraft(Changed, Class, Draft, Refusal); },
					   TEXT("ring entry"), Lines);
			break;
		}

		// The crystals, row by row, as the screen's socket list offers them (the pouch pays).
		const FLoadoutScreenPage Page = WoRLoadoutScreen::BuildGearPage(World, Member);
		TSet<ESpellElement> Taken;
		for (const FLoadoutScreenRow &Row : Page.Rows)
		{
			if (Row.Sockets.IsValidIndex(LoadoutScreenSockets::CRYSTAL) && !Row.Sockets[LoadoutScreenSockets::CRYSTAL].bEmpty)
			{
				Taken.Add(Row.Sockets[LoadoutScreenSockets::CRYSTAL].Element);
			}
		}
		for (const FLoadoutScreenRow &Row : Page.Rows)
		{
			const bool bGear = Row.Kind == ELoadoutScreenRowKind::Weapon || Row.Kind == ELoadoutScreenRowKind::Adapter;
			if (!bGear || !Row.Sockets.IsValidIndex(LoadoutScreenSockets::CRYSTAL) || !Row.Sockets[LoadoutScreenSockets::CRYSTAL].bEmpty)
			{
				continue;
			}
			const TArray<FLoadoutScreenChoice> Choices = WoRLoadoutScreen::BuildSocketChoices(World, Member, Row, LoadoutScreenSockets::CRYSTAL);
			TArray<ECrystalType> Offered;
			for (const FLoadoutScreenChoice &Choice : Choices)
			{
				if (Choice.Refusal.IsEmpty() && !Choice.EvolutionID.IsValid())
				{
					Offered.Add(Choice.Crystal.Type);
				}
			}
			const ECrystalType Pick = WoRTestGear::PickTestCrystal(Row.Kind, Offered, Taken);
			const FLoadoutScreenChoice *Chosen =
				Choices.FindByPredicate([Pick](const FLoadoutScreenChoice &Choice) { return Choice.Refusal.IsEmpty() && !Choice.EvolutionID.IsValid() && Choice.Crystal.Type == Pick; });
			if (!Chosen)
			{
				Lines.Add(FString::Printf(TEXT("%s socket: no test crystal fits"), *Row.Name.ToString()));
				continue;
			}
			const FText Refusal = WoRLoadoutScreen::ApplySocketChoice(World, Member, Row, LoadoutScreenSockets::CRYSTAL, *Chosen);
			Lines.Add(FString::Printf(TEXT("%s socket: %s %s"), *Row.Name.ToString(), *UEnum::GetValueAsString(Pick),
									  Refusal.IsEmpty() ? TEXT("in") : *FString::Printf(TEXT("refused (%s)"), *Refusal.ToString())));
			if (Refusal.IsEmpty())
			{
				Taken.Add(CrystalTypeHelpers::GetElement(Pick));
			}
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GTestGearCmd(
		TEXT("wor.Debug.TestGear"),
		TEXT("DEBUG (B-503n): gives the played character a plain sword, a ring and Fire / Water / Lightning crystals through the real API. "
			 "[fill] builds the loadout as the v2 screen does; [refractor|resonator|bd|reality] sets the class (put back when PIE ends)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				bool bFill = false;
				FString Variant;
				for (const FString &Arg : Args)
				{
					if (Arg.Equals(TEXT("fill"), ESearchCase::IgnoreCase))
					{
						bFill = true;
					}
					else
					{
						Variant = Arg.ToLower();
					}
				}
				const FString Report = WoRTestGear::GrantTestGear(World, bFill, Variant);
				UE_LOG(LogWoR, Log, TEXT("[TestGear]\n%s"), *Report);
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, TEST_GEAR_SCREEN_SECONDS, FColor::Cyan, Report);
				}
			}));
}

ECrystalType WoRTestGear::PickTestCrystal(ELoadoutScreenRowKind Kind, const TArray<ECrystalType> &Offered, const TSet<ESpellElement> &Taken)
{
	static const ECrystalType WeaponOrder[] = {ECrystalType::Garnet, ECrystalType::Citrine, ECrystalType::Sapphire};
	static const ECrystalType AdapterOrder[] = {ECrystalType::Sapphire, ECrystalType::Garnet, ECrystalType::Citrine};
	for (const ECrystalType Type : Kind == ELoadoutScreenRowKind::Adapter ? AdapterOrder : WeaponOrder)
	{
		if (Offered.Contains(Type) && !Taken.Contains(CrystalTypeHelpers::GetElement(Type)))
		{
			return Type;
		}
	}
	return ECrystalType::None;
}

FString WoRTestGear::GrantTestGear(UWorld *World, bool bFill, const FString &Variant)
{
	const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn *Pawn = PC ? PC->GetPawn() : nullptr;
	ULoadoutComponent *Loadout = Pawn ? Pawn->FindComponentByClass<ULoadoutComponent>() : nullptr;
	UInventoryComponent *Inventory = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	UCharacterDataComponent *DataComponent = Pawn ? Pawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	UPoolSubsystem *Pool = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UPoolSubsystem>() : nullptr;
	if (!Loadout || !Inventory || !DataComponent || !Pool || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
	{
		return TEXT("no played character with a loadout, an inventory and a pool here");
	}
	TArray<FString> Lines;
	if (!Variant.IsEmpty())
	{
		ApplyVariant(World, Variant, *Loadout, *Inventory, *DataComponent, Lines);
	}
	FLoadoutScreenMember Member;
	if (!FindPlayedMember(World, Loadout, Member))
	{
		Lines.Add(TEXT("the played character is not a party member here"));
		return FString::Join(Lines, TEXT("\n"));
	}

	UWeaponData *Sword = LoadObject<UWeaponData>(nullptr, TEST_SWORD_PATH);
	URingData *Ring = LoadObject<URingData>(nullptr, TEST_RING_PATH);
	FGuid SwordID = FindOwned(Inventory->Weapons, Sword, &FWeaponInventoryEntry::Weapon);
	if (!Sword)
	{
		Lines.Add(FString::Printf(TEXT("sword: %s not found"), TEST_SWORD_PATH));
	}
	else if (!SwordID.IsValid() && Inventory->AddWeapon(Sword))
	{
		SwordID = FindOwned(Inventory->Weapons, Sword, &FWeaponInventoryEntry::Weapon);
		const FWeaponInventoryEntry *Owned = Inventory->FindOwnedWeapon(SwordID);
		Lines.Add(FString::Printf(TEXT("sword: %s given%s"), *Sword->GetName(), Owned && !Owned->AttachedItem.IsEmpty() ? TEXT(" (it came with a socketed piece)") : TEXT("")));
	}
	else
	{
		Lines.Add(FString::Printf(TEXT("sword: %s %s"), *Sword->GetName(), SwordID.IsValid() ? TEXT("already owned") : TEXT("refused (no room)")));
	}
	FGuid RingID = FindOwned(Inventory->Rings, Ring, &FRingInventoryEntry::Ring);
	if (!Ring)
	{
		Lines.Add(FString::Printf(TEXT("ring: %s not found"), TEST_RING_PATH));
	}
	else if (!RingID.IsValid() && Inventory->AddRing(Ring))
	{
		RingID = FindOwned(Inventory->Rings, Ring, &FRingInventoryEntry::Ring);
		const FRingInventoryEntry *Owned = Inventory->FindOwnedRing(RingID);
		Lines.Add(FString::Printf(TEXT("ring: %s given%s"), *Ring->GetName(), Owned && !Owned->AttachedItem.IsEmpty() ? TEXT(" (it came with a socketed piece)") : TEXT("")));
	}
	else
	{
		Lines.Add(FString::Printf(TEXT("ring: %s %s"), *Ring->GetName(), RingID.IsValid() ? TEXT("already owned") : TEXT("refused (no room)")));
	}
	const FName PouchKey = WoRGive::GetPouchKey(World, Member.Key);
	for (const ECrystalType Type : TEST_CRYSTALS)
	{
		const FCrystalId Id(Type, TEST_CRYSTAL_TIER);
		if (Pool->GetCarriedItems(PouchKey).Contains(Id))
		{
			Lines.Add(FString::Printf(TEXT("crystal %s: already carried"), *UEnum::GetValueAsString(Type)));
			continue;
		}
		const int32 Stored = Pool->CarryOrStore(PouchKey, Id, TEST_CRYSTAL_COUNT);
		Lines.Add(FString::Printf(TEXT("crystal %s: %s"), *UEnum::GetValueAsString(Type), Stored > 0 ? TEXT("pouch full, went to storage") : TEXT("in the pouch")));
	}

	if (bFill)
	{
		Fill(World, Member, *Loadout, *Inventory, SwordID, RingID, Lines);
	}
	if (const UFieldMovementComponent *FieldMovement = Pawn->FindComponentByClass<UFieldMovementComponent>())
	{
		Lines.Add(TEXT("field attack stops:"));
		Lines.Add(WoRFieldMovementDebug::GetFieldAttackCycleString(*FieldMovement));
	}
	return FString::Join(Lines, TEXT("\n"));
}
