// FieldMovementAttack.cpp
// B-214f (Design/FieldAbilities.md; Metaphor: ReFantazio's field strike): UFieldMovementComponent's field attack. A swing
// sweeps a sphere ahead; the first encounter it strikes engages with the party striking first and carries the hit's
// buildup into the fight (UEncounterComponent::StrikeFromField -> ABattleGameMode::ApplyFieldStrike). Held with Water
// it is Ice. Server side; a client's calls go by Server RPC.
// B-503 (D-215): the strike's element and physical type come from the entry in use, one stop of a cycle through the
// whole loadout (GetFieldAttackEntries); only a weapon entry has a physical type.
// B-503b: a weapon entry plays its attack's FieldMontage (the battle swing without the dash) on every machine and the
// sweep waits for its Impact notify; with no montage the hit lands at once.
// B-503c: every other entry throws the pulse: the cast montage (FieldPulseMontage) the same way, then a thinner sweep
// 7 m ahead and the burst (FieldPulseVFX, or a plain line and ball) from the hand to where it stopped, in the strike's
// colour, on every machine.

#include "Field/FieldMovementComponent.h"

#include "Animation/AnimMontage.h"
#include "Character/ECharacterClass.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Combat/Encounter/EncounterComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Character.h"
#include "Infusion/ElementColors.h"
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
		Entry.SwingMontage = Weapon->WeaponAttack ? Weapon->WeaponAttack->FieldMontage : nullptr;
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
	const FFieldStrike Strike = FieldSkillRules::MakeFieldStrike(Entry.Element, Entry.Physical, bHeld);
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
	DrawDebugLine(World, From, End, Drawn, /*bPersistentLines=*/false, FieldPulse::PLACEHOLDER_SECONDS, 0, FieldPulse::PLACEHOLDER_THICKNESS);
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
