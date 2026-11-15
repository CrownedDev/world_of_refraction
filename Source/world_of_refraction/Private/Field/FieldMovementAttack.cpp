// FieldMovementAttack.cpp
// B-214f (Design/FieldAbilities.md; Metaphor: ReFantazio's field strike): UFieldMovementComponent's field attack. A swing
// sweeps a sphere ahead; the first encounter it strikes engages with the party striking first and carries the hit's
// buildup into the fight (UEncounterComponent::StrikeFromField -> ABattleGameMode::ApplyFieldStrike). Held with Water
// it is Ice. Server side; a client's calls go by Server RPC.
// B-503 (D-215): the strike's element and physical type come from the entry in use (GetFieldAttackEntries); only a
// swing has a physical type. B-503k (D-217): the stops are the attack's infusions, not the loadout's items.
// B-503m4 (D-217 "The wheel on RB"): one slice per loadout entry again, its element options inside (Ice on Water).
// B-503b: a weapon entry plays its attack's FieldMontage (the battle swing without the dash) on every machine and the
// sweep waits for its Impact notify; with no montage the hit lands at once.
// B-503c: every other entry throws the pulse: the cast montage (FieldPulseMontage) the same way, then a thinner sweep
// 7 m ahead and the burst (FieldPulseVFX, or a plain line and ball) from the hand to where it stopped, in the strike's
// colour, on every machine.
// B-503s (D-218): picking a weapon's slice puts that weapon in hand (the loadout's ACTIVE); a pulse pick puts it away.
// A weapon user's ring infuses the swing; an evolution or a Pistol / Trainer adapter throws the pulse.

#include "Field/FieldMovementComponent.h"

#include "Animation/AnimMontage.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/ECharacterClass.h"
#include "CollisionQueryParams.h"
#include "Combat/Encounter/EncounterComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Weapons/WeaponMeshComponent.h"
#include "GameFramework/Character.h"
#include "Infusion/ElementColors.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Skills/Definitions/AbilityData.h"
#include "TimerManager.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRFieldAttack"

namespace FieldAttackLog
{
	const TCHAR *const TAG = TEXT("[FieldAttack]");
}

namespace FieldPulse
{
	/** The burst's user parameters (the editor step names them the same in the Niagara system). */
	const FName COLOUR_PARAMETER(TEXT("PulseColor"));
	const FName END_PARAMETER(TEXT("PulseEnd"));
	/** The pulse leaves the right hand; a mesh without the bone throws it from the body's centre. */
	const FName HAND_BONE(TEXT("hand_r"));
	constexpr float PLACEHOLDER_SECONDS = 0.4f;
	constexpr float PLACEHOLDER_THICKNESS = 6.0f;
	constexpr int32 PLACEHOLDER_SEGMENTS = 12;
	constexpr uint8 PLACEHOLDER_DEPTH_PRIORITY = 0; // the world's own depth: hidden behind walls
}

namespace
{
	/** CL-24: a socket's element as the strike reads it (as UActionExecutor::ResolveInfusionSource): None when empty or
	 *  broken. An evolution in the socket gives its own element. */
	ESpellElement LiveSocketElement(const FRuntimeAttachedItem &Socket)
	{
		return !Socket.IsEmpty() && !Socket.IsBroken() ? Socket.GetElement() : ESpellElement::None;
	}

	/** B-503m4 (D-217): one loadout entry, a slice of the wheel, before its element options are made. */
	struct FFieldSlice
	{
		FText Name;
		ESpellElement Element = ESpellElement::None;
		EFieldAttackKind Kind = EFieldAttackKind::Ring;
		/** Q-503l: Reality cycles on the innate element or an evolution; an adapter or a weapon crystal cannot. */
		bool bRealityCycles = false;
		/** A weapon slice: the weapon it swings (a weapon user); a pulse class throws its crystal. */
		const FWeaponLoadoutEntry *Weapon = nullptr;
		/** B-514 (D-222): the quick strip's tile: the piece's icon, a pair's off hand beside it. */
		UTexture2D *Icon = nullptr;
		UTexture2D *OffHandIcon = nullptr;
		/** B-503s (D-218): a weapon user throws the pulse from this slice too: the innate, an evolution, an adapter
		 *  that isn't a ring (a Pistol or a Trainer). A ring infuses the weapon in hand. */
		bool bPulse = false;
	};

	/** A weapon's swing, from its loadout entry: the entry's own attack override first (as
	 *  ULoadoutComponent::GetCurrentAttack), else the weapon's attack. Unarmed: a plain swing with no name. */
	FFieldAttackEntry MakeSwing(const FWeaponLoadoutEntry *Slot)
	{
		FFieldAttackEntry Swing;
		const UWeaponData *Weapon = Slot ? Slot->WeaponEntry.Weapon : nullptr;
		if (!Weapon)
		{
			return Swing;
		}
		const USkillDataBase *Attack = Slot->OverrideAttack ? Slot->OverrideAttack : Weapon->WeaponAttack;
		const UAbilityData *Ability = Cast<UAbilityData>(Attack);
		Swing.Name = WoRGameNames::GetHeldWeaponTypeName(Weapon, Slot->HasOffHand()); // B-513: "Sword and Shield"
		Swing.Physical = Ability ? Ability->PhysicalDamageType : EPhysicalDamageType::None;
		Swing.WeaponType = Weapon->WeaponType;
		Swing.Icon = Weapon->Icon;
		Swing.SwingMontage = Attack ? Attack->FieldMontage : nullptr;
		return Swing;
	}

	/** D-217: "Fire Sword" for an infused weapon, "Fire" with no weapon line. */
	FText MakeInfusedLabel(const FText &ElementName, const FText &WeaponTypeName)
	{
		return WeaponTypeName.IsEmpty() ? ElementName
										: FText::Format(LOCTEXT("InfusedWeapon", "{0} {1}"), ElementName, WeaponTypeName);
	}

	/** Q-503l: which slices step through the other entries' elements. Reality on the innate element or an evolution
	 *  ("(R) Fire"), and a Broken Darkness character's Darkness ("Dark Fire"). */
	enum class EFieldHoldCycle : uint8
	{
		None,
		Reality,
		BrokenDarkness
	};

	EFieldHoldCycle GetHoldCycle(const FFieldSlice &Slice, bool bBrokenDarkness)
	{
		if (Slice.Element == ESpellElement::Reality && Slice.bRealityCycles)
		{
			return EFieldHoldCycle::Reality;
		}
		return bBrokenDarkness && Slice.Element == ESpellElement::Darkness ? EFieldHoldCycle::BrokenDarkness : EFieldHoldCycle::None;
	}

	FText MakeHoldElementName(EFieldHoldCycle Cycle, ESpellElement Element)
	{
		const FText ElementName = WoRGameNames::GetElementName(Element);
		return Cycle == EFieldHoldCycle::Reality ? FText::Format(LOCTEXT("RealityHoldElement", "(R) {0}"), ElementName)
												 : FText::Format(LOCTEXT("DarkHoldElement", "Dark {0}"), ElementName);
	}

	/** The loadout's entries in loadout order (Q-503): the primary weapon or adapter, the secondary weapon, the
	 *  adapters of the ring loadout, the innate element (a Refractor), the character's evolution. */
	TArray<FFieldSlice> GetFieldSlices(const ULoadoutComponent *LoadoutComponent, const UCharacterData *Data)
	{
		TArray<FFieldSlice> Slices;
		if (LoadoutComponent)
		{
			const FCombatLoadout &Loadout = LoadoutComponent->GetActiveLoadout();
			const auto AddWeapon = [&Slices](const FWeaponLoadoutEntry &Slot)
			{
				FFieldSlice &Slice = Slices.AddDefaulted_GetRef();
				// B-513 / B-513b: the pieces' own names ("Flame Sword and Void Shield"); the centre keeps the type.
				Slice.Name = WoRGameNames::GetHeldPiecesName(Slot.WeaponEntry.Weapon, Slot.HasOffHand() ? Slot.OffHandEntry.Weapon : nullptr);
				Slice.Element = LiveSocketElement(Slot.WeaponEntry.GetAttachedItem());
				Slice.Kind = EFieldAttackKind::Weapon;
				Slice.Weapon = &Slot;
				Slice.Icon = Slot.WeaponEntry.Weapon ? Slot.WeaponEntry.Weapon->Icon : nullptr;
				Slice.OffHandIcon = Slot.HasOffHand() && Slot.OffHandEntry.Weapon ? Slot.OffHandEntry.Weapon->Icon : nullptr;
			};
			const auto AddRing = [&Slices](const FRingLoadoutEntry &Slot)
			{
				FFieldSlice &Slice = Slices.AddDefaulted_GetRef();
				Slice.Name = FText::FromString(Slot.RingEntry.Ring->GetDisplayName());
				Slice.Element = LiveSocketElement(Slot.RingEntry.GetAttachedItem());
				Slice.bPulse = Slot.RingEntry.Ring->Form != EAdapterForm::Ring;
				Slice.Icon = Slot.RingEntry.Ring->Icon;
			};
			// The fight's own walk (FCombatLoadout.cpp ForEachReadLegacySlot): a primary weapon with a Weapon or None type
			// (AutoPopulateLoadout leaves None), else a primary adapter; a Generic's secondary weapon; the adapter loadout
			// of a Resonator or a Generic. TODO: B-503q shares one walker instead of this copy.
			const ECharacterClass Class = LoadoutComponent->GetCharacterClass();
			if ((Loadout.PrimarySlotType == EPrimarySlotType::Weapon || Loadout.PrimarySlotType == EPrimarySlotType::None) &&
				Loadout.PrimaryWeapon.IsValid())
			{
				AddWeapon(Loadout.PrimaryWeapon);
			}
			else if (Loadout.HasPrimaryRing() && Class != ECharacterClass::Resonator)
			{
				AddRing(Loadout.PrimaryRing); // a Resonator's stray primary ring is never one in use (GetActiveSlotIn)
			}
			if (Class == ECharacterClass::Generic && Loadout.HasSecondaryWeapon())
			{
				AddWeapon(Loadout.SecondaryWeapon);
			}
			for (const FRingLoadoutEntry &Ring : Class == ECharacterClass::Resonator || Class == ECharacterClass::Generic ? Loadout.RingLoadout : TArray<FRingLoadoutEntry>())
			{
				if (Ring.IsValid())
				{
					AddRing(Ring);
				}
			}
		}
		if (Data && Data->IsCaster())
		{
			FFieldSlice &Slice = Slices.AddDefaulted_GetRef();
			Slice.Name = LOCTEXT("Innate", "Innate"); // TODO: one shared name (also in the field skill picker and the infusion menu)
			Slice.Element = Data->InnateElement;
			Slice.Kind = EFieldAttackKind::Innate;
			Slice.bRealityCycles = true;
			Slice.bPulse = true;
		}
		// The combat menu's Evolution source: the character's own slot (one in a gear socket gives that gear's element).
		if (LoadoutComponent && LoadoutComponent->IsEvolved())
		{
			const FEvolutionAttachment Seat = LoadoutComponent->GetCharacterSlotEvolution();
			const UEvolutionItemData *Item = Seat.Item;
			if (Item && !Seat.IsBroken())
			{
				FFieldSlice &Slice = Slices.AddDefaulted_GetRef();
				Slice.Name = Item->ItemName.IsEmpty() ? WoRGameNames::GetCrystalTypeName(Item->CrystalType) : FText::FromString(Item->ItemName);
				Slice.Element = Item->GetAssociatedElement();
				Slice.Kind = EFieldAttackKind::Evolution;
				Slice.bRealityCycles = true;
				Slice.bPulse = true;
			}
		}
		return Slices;
	}
}

FText FieldSkillRules::GetFieldStripName(const FFieldAttackEntry &Entry)
{
	const bool bPlain = Entry.Element == ESpellElement::None && !Entry.bIce;
	return bPlain || Entry.OptionName.IsEmpty() ? Entry.SliceName
												: FText::Format(LOCTEXT("StripPieceSource", "{0} ({1})"), Entry.SliceName, Entry.OptionName);
}

TArray<FFieldAttackEntry> FieldSkillRules::BuildFieldAttackEntries(const AActor *Character)
{
	// B-503m4 (D-217 "The wheel on RB"): one slice per loadout entry, each followed by its element options (bHoldStep).
	// A weapon user swings: a weapon's slice swings that weapon, plain then its crystal's element; a ring infuses the
	// weapon in hand ("Lightning Sword"). B-503s (D-218): an evolution or a Pistol / Trainer adapter throws the pulse
	// even for a weapon user. A Refractor or Resonator throws the pulse in the slice's element ("Fire"). A slice with
	// nothing to add (no element, no weapon of its own) is left out (Q-503 (2)); two slices of one element both show
	// (each entry its slice, Q-503m4).
	const UCharacterDataComponent *CharComp = Character ? Character->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	const UCharacterData *Data = CharComp ? CharComp->CharacterData : nullptr;
	const ULoadoutComponent *LoadoutComponent = Character ? Character->FindComponentByClass<ULoadoutComponent>() : nullptr;
	const TArray<FFieldSlice> Slices = GetFieldSlices(LoadoutComponent, Data);
	const bool bPulseClass = Data && Data->CharacterClass != ECharacterClass::Generic;
	const FFieldAttackEntry ActiveSwing = MakeSwing(LoadoutComponent ? LoadoutComponent->GetActiveWeaponLoadout() : nullptr);
	const bool bBrokenDarkness = CharComp && CharComp->IsBrokenDarkness();

	// Q-503l: Reality's and Broken Darkness's options are the other entries' elements, each once, never the hidden Reality.
	TArray<ESpellElement> GearElements;
	for (const FFieldSlice &Slice : Slices)
	{
		if (FieldSkillRules::IsElemental(Slice.Element) && Slice.Element != ESpellElement::Reality)
		{
			GearElements.AddUnique(Slice.Element);
		}
	}

	TArray<FFieldAttackEntry> Entries;
	for (const FFieldSlice &Slice : Slices)
	{
		const bool bPulse = bPulseClass || Slice.bPulse;
		const bool bOwnSwing = !bPulse && Slice.Weapon;
		if (!FieldSkillRules::IsElemental(Slice.Element) && !bOwnSwing)
		{
			continue;
		}
		FFieldAttackEntry Base = bPulse ? FFieldAttackEntry() : (bOwnSwing ? MakeSwing(Slice.Weapon) : ActiveSwing);
		if (bPulse)
		{
			Base.Kind = Slice.Kind == EFieldAttackKind::Weapon ? EFieldAttackKind::Ring : Slice.Kind; // a crystal in gear, thrown
		}
		else if (bOwnSwing)
		{
			Base.WeaponId = Slice.Weapon->WeaponEntry.PersistentID; // picking it puts it in hand (D-218)
		}
		Base.SliceName = Slice.Name;
		Base.SliceIcon = Slice.Icon;
		Base.SliceOffHandIcon = Slice.OffHandIcon;
		const FText WeaponTypeName = Base.Name; // empty for a pulse or unarmed
		const int32 First = Entries.Num();
		const auto AddOption = [&Entries, &Base, &WeaponTypeName, First](ESpellElement Element, const FText &ElementName, bool bIce)
		{
			FFieldAttackEntry &Option = Entries.Add_GetRef(Base);
			Option.Element = Element;
			Option.bIce = bIce;
			Option.OptionName = ElementName;
			Option.Name = MakeInfusedLabel(ElementName, WeaponTypeName);
			Option.bHoldStep = Entries.Num() - 1 > First;
		};
		if (bOwnSwing)
		{
			Entries.Add_GetRef(Base).OptionName = LOCTEXT("PlainOption", "Plain"); // plain first
		}
		if (!FieldSkillRules::IsElemental(Slice.Element))
		{
			continue;
		}
		AddOption(Slice.Element, WoRGameNames::GetElementName(Slice.Element), /*bIce=*/false);
		if (Slice.Element == ESpellElement::Water)
		{
			AddOption(ESpellElement::Water, WoRGameNames::GetIceName(), /*bIce=*/true); // D-217 "Ice"
		}
		const EFieldHoldCycle Cycle = GetHoldCycle(Slice, bBrokenDarkness);
		if (Cycle == EFieldHoldCycle::None)
		{
			continue;
		}
		for (const ESpellElement Other : GearElements)
		{
			if (Other != Slice.Element)
			{
				AddOption(Other, MakeHoldElementName(Cycle, Other), /*bIce=*/false);
			}
		}
	}
	return Entries;
}

TArray<FFieldAttackEntry> UFieldMovementComponent::GetFieldAttackEntries() const
{
	return FieldSkillRules::BuildFieldAttackEntries(GetOwner());
}

int32 UFieldMovementComponent::GetKeptFieldAttackIndex() const
{
	const FPartyMember *Record = HasAuthority() ? FindRecord() : nullptr;
	return Record ? Record->FieldAttackIndex : FieldAttackIndex;
}

int32 UFieldMovementComponent::GetFieldAttackIndex() const
{
	return FieldSkillRules::ClampFieldAttackIndex(GetKeptFieldAttackIndex(), GetFieldAttackEntries().Num());
}

FFieldAttackEntry UFieldMovementComponent::GetFieldAttackEntry() const
{
	const TArray<FFieldAttackEntry> Entries = GetFieldAttackEntries();
	const int32 Index = FieldSkillRules::ClampFieldAttackIndex(GetKeptFieldAttackIndex(), Entries.Num());
	return Entries.IsValidIndex(Index) ? Entries[Index] : FFieldAttackEntry();
}

FFieldStrike UFieldMovementComponent::GetFieldStrike(bool bHeld) const
{
	const FFieldAttackEntry Entry = GetFieldAttackEntry();
	return FieldSkillRules::MakeFieldStrike(Entry.Element, Entry.Physical, bHeld || Entry.bIce); // B-503m4: Ice is the option
}

FFieldAttackEntry UFieldMovementComponent::CycleFieldAttack(bool bHold)
{
	const TArray<FFieldAttackEntry> Entries = GetFieldAttackEntries();
	const int32 Kept = GetKeptFieldAttackIndex();
	const int32 Next = bHold ? FieldSkillRules::NextFieldAttackHoldIndex(Entries, Kept) : FieldSkillRules::NextFieldAttackSourceIndex(Entries, Kept);
	return SelectFieldAttack(Next);
}

FFieldAttackEntry UFieldMovementComponent::SelectFieldAttack(int32 Index)
{
	const TArray<FFieldAttackEntry> Entries = GetFieldAttackEntries();
	const int32 Picked = FieldSkillRules::ClampFieldAttackIndex(Index, Entries.Num());
	if (HasAuthority())
	{
		StoreFieldAttackIndex(Picked);
	}
	else
	{
		FieldAttackIndex = Picked; // shown at once; the host keeps it and its copy comes back the same
		ServerSetFieldAttackIndex(Picked);
		ShowWeaponForFieldAttack();
	}
	return Entries.IsValidIndex(Picked) ? Entries[Picked] : FFieldAttackEntry();
}

void UFieldMovementComponent::StoreFieldAttackIndex(int32 Index)
{
	const TArray<FFieldAttackEntry> Entries = GetFieldAttackEntries();
	FieldAttackIndex = FieldSkillRules::ClampFieldAttackIndex(Index, Entries.Num());
	const UGameInstance *GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	if (PartySession && FindRecord())
	{
		PartySession->SetMemberFieldAttackIndex(PartySession->GetMemberSlotOf(GetCharacterData()), FieldAttackIndex);
	}
	UE_LOG(LogWoR, Log, TEXT("%s %s: field attack %d of %d: %s"), FieldAttackLog::TAG, *GetNameSafe(GetOwner()),
		   FieldAttackIndex + 1, Entries.Num(),
		   *FieldSkillRules::FieldAttackEntryString(Entries.IsValidIndex(FieldAttackIndex) ? Entries[FieldAttackIndex] : FFieldAttackEntry()));
	// B-503s (D-218, Q-503s A): a weapon's own slice puts that weapon in hand, saved with the loadout (the loadout
	// screen's ACTIVE); the mesh follows OnActiveWeaponChanged. Slices keep loadout order, so the index stays valid.
	// shortcut: the host's pawn only; a co-op guest's own copy of the loadout catches up on its next loadout sync.
	const FGuid PickedWeapon = Entries.IsValidIndex(FieldAttackIndex) ? Entries[FieldAttackIndex].WeaponId : FGuid();
	ULoadoutComponent *Loadout = PickedWeapon.IsValid() && GetOwner() ? GetOwner()->FindComponentByClass<ULoadoutComponent>() : nullptr;
	const FWeaponLoadoutEntry *InHand = Loadout ? Loadout->GetActiveWeaponLoadout() : nullptr;
	const UInventoryComponent *Inventory = Loadout ? Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (Inventory && (!InHand || InHand->WeaponEntry.PersistentID != PickedWeapon))
	{
		FText Refusal;
		const auto PutInHand = [&PickedWeapon](FCombatLoadout &Changed)
		{
			if (Changed.PrimaryWeapon.WeaponEntry.PersistentID == PickedWeapon || Changed.SecondaryWeapon.WeaponEntry.PersistentID == PickedWeapon)
			{
				Changed.bShowPrimary = Changed.PrimaryWeapon.WeaponEntry.PersistentID == PickedWeapon;
			}
		};
		if (!Loadout->TryChangeGear(Inventory->ActiveLoadoutIndex, PutInHand, Refusal))
		{
			UE_LOG(LogWoR, Log, TEXT("%s %s: the picked weapon stays out of hand: %s"), FieldAttackLog::TAG, *GetNameSafe(GetOwner()), *Refusal.ToString());
		}
	}
	ShowWeaponForFieldAttack();
}

void UFieldMovementComponent::ServerSetFieldAttackIndex_Implementation(int32 Index)
{
	StoreFieldAttackIndex(Index); // clamped to the host's own reading of the loadout
	if (FieldAttackIndex != Index)
	{
		// The host read a shorter loadout. If its value didn't change, nothing replicates back: tell the client.
		ClientSetFieldAttackIndex(FieldAttackIndex);
	}
}

void UFieldMovementComponent::ClientSetFieldAttackIndex_Implementation(int32 Index)
{
	FieldAttackIndex = Index;
	ShowWeaponForFieldAttack();
}

void UFieldMovementComponent::ShowWeaponForFieldAttack() const
{
	// shortcut: the owner's machine and the host only (the index replicates to its owner); other co-op players
	// still see the weapon. Upgrade with the real HUD's co-op pass if the put-away should show to everyone.
	if (UWeaponMeshComponent *Weapon = GetOwner() ? GetOwner()->FindComponentByClass<UWeaponMeshComponent>() : nullptr)
	{
		const TArray<FFieldAttackEntry> Entries = GetFieldAttackEntries();
		const int32 Index = FieldSkillRules::ClampFieldAttackIndex(GetKeptFieldAttackIndex(), Entries.Num());
		Weapon->SetWeaponHidden(Entries.IsValidIndex(Index) && !Entries[Index].IsSwing());
	}
}

EFieldSkillRefusal UFieldMovementComponent::FieldAttack(bool bHeld)
{
	AActor *Owner = GetOwner();
	UWorld *World = GetWorld();
	if (!Owner || !World)
	{
		return EFieldSkillRefusal::NoMovement;
	}
	if (World->GetTimeSeconds() - LastFieldAttackTime < FieldSkillRules::FIELD_ATTACK_COOLDOWN_SECONDS ||
		World->GetTimerManager().IsTimerActive(FieldSwingTimer))
	{
		return EFieldSkillRefusal::OnCooldown;
	}
	LastFieldAttackTime = World->GetTimeSeconds();
	if (!HasAuthority())
	{
		ServerFieldAttack(bHeld);
		return EFieldSkillRefusal::None;
	}

	// The strike is the entry's at the press: cycling during the swing doesn't change the hit on its way.
	const FFieldAttackEntry Entry = GetFieldAttackEntry();
	const FFieldStrike Strike = FieldSkillRules::MakeFieldStrike(Entry.Element, Entry.Physical, bHeld || Entry.bIce);
	const bool bSwing = Entry.IsSwing();
	// shortcut: a blocking load of one small montage on the first attack, async-load with the loadout if it hitches
	UAnimMontage *Montage = (bSwing ? Entry.SwingMontage : FieldPulseMontage).LoadSynchronous();
	if (Montage)
	{
		MulticastPlayFieldSwing(Montage);
	}
	const float ImpactSeconds = FieldSkillRules::GetSwingImpactSeconds(Montage);
	if (ImpactSeconds > FieldSkillRules::NO_SWING_DELAY)
	{
		World->GetTimerManager().SetTimer(FieldSwingTimer, FTimerDelegate::CreateUObject(this, &UFieldMovementComponent::ResolveFieldAttack, Strike, bSwing),
										  ImpactSeconds, /*bLoop=*/false);
	}
	else
	{
		ResolveFieldAttack(Strike, bSwing);
	}
	return EFieldSkillRefusal::None;
}

void UFieldMovementComponent::MulticastPlayFieldSwing_Implementation(UAnimMontage *Montage)
{
	if (ACharacter *Character = Cast<ACharacter>(GetOwner()))
	{
		Character->PlayAnimMontage(Montage); // in place: the strike clip carries no root motion
	}
}

void UFieldMovementComponent::MulticastPlayFieldPulse_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End, FLinearColor Colour)
{
	UWorld *World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const ACharacter *Character = Cast<ACharacter>(GetOwner());
	const USkeletalMeshComponent *Mesh = Character ? Character->GetMesh() : nullptr;
	const FVector From = Mesh && Mesh->DoesSocketExist(FieldPulse::HAND_BONE) ? Mesh->GetSocketLocation(FieldPulse::HAND_BONE) : FVector(Start);
	// shortcut: a blocking load of one small effect on the first pulse, async-load at BeginPlay if it hitches
	if (UNiagaraSystem *Burst = FieldPulseVFX.LoadSynchronous())
	{
		if (UNiagaraComponent *Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Burst, From, (End - From).Rotation()))
		{
			Spawned->SetVariableLinearColor(FieldPulse::COLOUR_PARAMETER, Colour);
			Spawned->SetVariableVec3(FieldPulse::END_PARAMETER, End);
		}
		return;
	}
	// The placeholder until the burst is authored (not drawn in a shipping build).
	const FColor Drawn = Colour.ToFColor(/*bSRGB=*/true);
	DrawDebugLine(World, From, End, Drawn, /*bPersistentLines=*/false, FieldPulse::PLACEHOLDER_SECONDS,
				  FieldPulse::PLACEHOLDER_DEPTH_PRIORITY, FieldPulse::PLACEHOLDER_THICKNESS);
	DrawDebugSphere(World, End, FieldSkillRules::FIELD_PULSE_RADIUS, FieldPulse::PLACEHOLDER_SEGMENTS, Drawn, false, FieldPulse::PLACEHOLDER_SECONDS);
}

void UFieldMovementComponent::ResolveFieldAttack(FFieldStrike Strike, bool bSwing)
{
	AActor *Owner = GetOwner();
	UWorld *World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}
	const FVector Start = Owner->GetActorLocation();
	const FVector End = Start + Owner->GetActorForwardVector().GetSafeNormal2D() * FieldSkillRules::GetFieldAttackRange(bSwing);
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FieldAttack), false, Owner);
	World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Pawn,
							   FCollisionShape::MakeSphere(FieldSkillRules::GetFieldAttackRadius(bSwing)), Params);
	if (!bSwing)
	{
		// A multi sweep ends at its first blocking hit, listed last: the pulse stops at that body or wall.
		const FVector Stop = Hits.Num() > 0 && Hits.Last().bBlockingHit ? Hits.Last().Location : End;
		MulticastPlayFieldPulse(Start, Stop, Strike.bIce ? ElementColors::Ice : ElementColors::GetColorForElement(Strike.Element));
	}
	for (const FHitResult &Hit : Hits)
	{
		if (!Hit.bBlockingHit)
		{
			continue; // an overlap (an encounter's trigger sphere reaches far past the body): only bodies count
		}
		AActor *Struck = Hit.GetActor();
		UEncounterComponent *Encounter = Struck ? Struck->FindComponentByClass<UEncounterComponent>() : nullptr;
		if (Encounter && Encounter->StrikeFromField(Owner, Strike))
		{
			UE_LOG(LogWoR, Log, TEXT("%s %s struck %s"), FieldAttackLog::TAG, *GetNameSafe(Owner), *GetNameSafe(Struck));
			return;
		}
	}
	UE_LOG(LogWoR, Verbose, TEXT("%s %s: a %s, nothing struck"), FieldAttackLog::TAG, *GetNameSafe(Owner), bSwing ? TEXT("swing") : TEXT("pulse"));
}

void UFieldMovementComponent::ServerFieldAttack_Implementation(bool bHeld)
{
	FieldAttack(bHeld); // the server keeps its own cooldown
}

#undef LOCTEXT_NAMESPACE
