// FieldMovementAttack.cpp
// B-214f (Design/FieldAbilities.md; Metaphor: ReFantazio's field strike): UFieldMovementComponent's field attack. A swing
// sweeps a sphere ahead; the first encounter it strikes engages with the party striking first and carries the hit's
// buildup into the fight (UEncounterComponent::StrikeFromField -> ABattleGameMode::ApplyFieldStrike). Held with Water
// it is Ice. Server side; a client's calls go by Server RPC.
// B-503 (D-215): the strike's element and physical type come from the entry in use, one stop of a cycle through the
// whole loadout (GetFieldAttackEntries); only a weapon entry has a physical type. TODO: the swing (B-503b) and the
// pulse (B-503c).

#include "Field/FieldMovementComponent.h"

#include "Character/ECharacterClass.h"
#include "CollisionQueryParams.h"
#include "Combat/Encounter/EncounterComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Skills/Definitions/AbilityData.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRFieldAttack"

namespace FieldAttackLog
{
	const TCHAR *const TAG = TEXT("[FieldAttack]");
}

namespace
{
	/** A socket's element as the strike reads it: None when empty or broken. */
	ESpellElement LiveSocketElement(const FRuntimeAttachedItem &Socket)
	{
		return !Socket.IsEmpty() && !Socket.IsBroken() ? Socket.GetElement() : ESpellElement::None;
	}

	FFieldAttackEntry MakeGearEntry(EFieldAttackKind Kind, const UEquipmentDataBase &Gear, const FRuntimeAttachedItem &Socket)
	{
		FFieldAttackEntry Entry;
		Entry.Kind = Kind;
		Entry.Name = FText::FromString(Gear.GetDisplayName());
		Entry.Element = LiveSocketElement(Socket);
		Entry.Icon = Gear.Icon;
		return Entry;
	}

	void AddEvolutionEntry(const FEvolutionAttachment &Attachment, TArray<FFieldAttackEntry> &Out)
	{
		const UEvolutionItemData *Item = Attachment.Item;
		if (!Item || Attachment.IsBroken())
		{
			return; // a broken evolution gives nothing (as on the character's own slot, B-214h)
		}
		FFieldAttackEntry &Entry = Out.AddDefaulted_GetRef();
		Entry.Kind = EFieldAttackKind::Evolution;
		Entry.Name = Item->ItemName.IsEmpty() ? WoRGameNames::GetCrystalTypeName(Item->CrystalType) : FText::FromString(Item->ItemName);
		Entry.Element = Item->GetAssociatedElement();
		Entry.Icon = Item->Icon;
	}

	void AddSocketEvolutionEntry(const FRuntimeAttachedItem &Socket, TArray<FFieldAttackEntry> &Out)
	{
		if (Socket.IsEvolution())
		{
			AddEvolutionEntry(Socket.Evolution, Out);
		}
	}
}

TArray<FFieldAttackEntry> UFieldMovementComponent::GetFieldAttackEntries() const
{
	TArray<FFieldAttackEntry> Entries;
	TArray<FFieldAttackEntry> Evolutions; // listed after the gear and the innate element
	const auto AddWeapon = [&Entries, &Evolutions](const FWeaponLoadoutEntry &Slot)
	{
		const UWeaponData *Weapon = Slot.WeaponEntry.Weapon;
		const FRuntimeAttachedItem &Socket = Slot.WeaponEntry.GetAttachedItem();
		const UAbilityData *Attack = Cast<UAbilityData>(Weapon->WeaponAttack);
		FFieldAttackEntry &Entry = Entries.Add_GetRef(MakeGearEntry(EFieldAttackKind::Weapon, *Weapon, Socket));
		Entry.Physical = Attack ? Attack->PhysicalDamageType : EPhysicalDamageType::None;
		AddSocketEvolutionEntry(Socket, Evolutions);
	};
	const auto AddRing = [&Entries, &Evolutions](const FRingLoadoutEntry &Slot)
	{
		const FRuntimeAttachedItem &Socket = Slot.RingEntry.GetAttachedItem();
		Entries.Add(MakeGearEntry(EFieldAttackKind::Ring, *Slot.RingEntry.Ring, Socket));
		AddSocketEvolutionEntry(Socket, Evolutions);
	};

	if (const ULoadoutComponent *LoadoutComponent = GetOwner() ? GetOwner()->FindComponentByClass<ULoadoutComponent>() : nullptr)
	{
		const FCombatLoadout &Loadout = LoadoutComponent->GetActiveLoadout();
		if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution)
		{
			AddEvolutionEntry(Loadout.PrimaryEvolution, Evolutions);
		}
		if (Loadout.HasPrimaryWeapon())
		{
			AddWeapon(Loadout.PrimaryWeapon);
		}
		// A Resonator's rings are its ring loadout only; a stray primary ring is never one in use (GetActiveSlotIn).
		if (Loadout.HasPrimaryRing() && LoadoutComponent->GetCharacterClass() != ECharacterClass::Resonator)
		{
			AddRing(Loadout.PrimaryRing);
		}
		if (Loadout.HasSecondaryWeapon())
		{
			AddWeapon(Loadout.SecondaryWeapon);
		}
		for (const FRingLoadoutEntry &Ring : Loadout.RingLoadout)
		{
			if (Ring.IsValid())
			{
				AddRing(Ring);
			}
		}
		for (const FApparelEntry &Piece : Loadout.Apparel)
		{
			if (Piece.Apparel)
			{
				AddSocketEvolutionEntry(Piece.AttachedItem, Evolutions);
			}
		}
	}

	const ESpellElement Innate = GetSourceElement(EFieldSkillSource::Innate);
	if (FieldSkillRules::IsElemental(Innate))
	{
		FFieldAttackEntry &Entry = Entries.AddDefaulted_GetRef();
		Entry.Kind = EFieldAttackKind::Innate;
		Entry.Name = LOCTEXT("Innate", "Innate"); // TODO: one shared name (also in the field skill picker and the infusion menu)
		Entry.Element = Innate;
	}
	Entries.Append(Evolutions);
	return Entries;
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
	return FieldSkillRules::MakeFieldStrike(Entry.Element, Entry.Physical, bHeld);
}

FFieldAttackEntry UFieldMovementComponent::CycleFieldAttack()
{
	const TArray<FFieldAttackEntry> Entries = GetFieldAttackEntries();
	const int32 Next = FieldSkillRules::NextFieldAttackIndex(GetKeptFieldAttackIndex(), Entries.Num());
	if (HasAuthority())
	{
		StoreFieldAttackIndex(Next);
	}
	else
	{
		FieldAttackIndex = Next; // shown at once; the host keeps it and its copy comes back the same
		ServerSetFieldAttackIndex(Next);
	}
	return Entries.IsValidIndex(Next) ? Entries[Next] : FFieldAttackEntry();
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
}

EFieldSkillRefusal UFieldMovementComponent::FieldAttack(bool bHeld)
{
	AActor *Owner = GetOwner();
	UWorld *World = GetWorld();
	if (!Owner || !World)
	{
		return EFieldSkillRefusal::NoMovement;
	}
	if (World->GetTimeSeconds() - LastFieldAttackTime < FieldSkillRules::FIELD_ATTACK_COOLDOWN_SECONDS)
	{
		return EFieldSkillRefusal::OnCooldown;
	}
	LastFieldAttackTime = World->GetTimeSeconds();
	if (!HasAuthority())
	{
		ServerFieldAttack(bHeld);
		return EFieldSkillRefusal::None;
	}

	const FVector Start = Owner->GetActorLocation();
	const FVector End = Start + Owner->GetActorForwardVector().GetSafeNormal2D() * FieldSkillRules::FIELD_ATTACK_RANGE;
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FieldAttack), false, Owner);
	World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Pawn,
							   FCollisionShape::MakeSphere(FieldSkillRules::FIELD_ATTACK_RADIUS), Params);
	const FFieldStrike Strike = GetFieldStrike(bHeld);
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
			return EFieldSkillRefusal::None;
		}
	}
	UE_LOG(LogWoR, Verbose, TEXT("%s %s: a swing, nothing struck"), FieldAttackLog::TAG, *GetNameSafe(Owner));
	return EFieldSkillRefusal::None;
}

void UFieldMovementComponent::ServerFieldAttack_Implementation(bool bHeld)
{
	FieldAttack(bHeld); // the server keeps its own cooldown
}

#undef LOCTEXT_NAMESPACE
