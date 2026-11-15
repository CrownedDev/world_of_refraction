// FieldMovementAttack.cpp
// B-214f (Design/FieldAbilities.md; Metaphor: ReFantazio's field strike): UFieldMovementComponent's field attack. A swing
// sweeps a sphere ahead; the first encounter it strikes engages with the party striking first and carries the hit's
// buildup into the fight (UEncounterComponent::StrikeFromField -> ABattleGameMode::ApplyFieldStrike). Held with Water
// it is Ice. Server side; a client's calls go by Server RPC.
// B-503 (D-215): the strike's element and physical type come from the entry in use (GetFieldAttackEntries); only a
// swing has a physical type. B-503k (D-217): the stops are the attack's infusions, not the loadout's items.
// B-503b: a weapon entry plays its attack's FieldMontage (the battle swing without the dash) on every machine and the
// sweep waits for its Impact notify; with no montage the hit lands at once.
// B-503c: every other entry throws the pulse: the cast montage (FieldPulseMontage) the same way, then a thinner sweep
// 7 m ahead and the burst (FieldPulseVFX, or a plain line and ball) from the hand to where it stopped, in the strike's
// colour, on every machine.

#include "Field/FieldMovementComponent.h"

#include "Animation/AnimMontage.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/ECharacterClass.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/WoRFightSubsystem.h"
#include "CollisionQueryParams.h"
#include "Combat/Encounter/EncounterComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Character.h"
#include "Infusion/EInfusionSourceOption.h"
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
	constexpr uint8 PLACEHOLDER_DEPTH_PRIORITY = 0; // the world's own depth: hidden behind walls
}

namespace
{
	/** B-503k: a pulse stop's kind, from the infusion source it takes its element from (a weapon's crystal is
	 *  listed with the rings: a crystal in gear, thrown). */
	EFieldAttackKind PulseKindOf(EInfusionSourceOption Source)
	{
		switch (Source)
		{
		case EInfusionSourceOption::Innate:
			return EFieldAttackKind::Innate;
		case EInfusionSourceOption::Evolution:
			return EFieldAttackKind::Evolution;
		default:
			return EFieldAttackKind::Ring;
		}
	}

	/** D-217: "Fire Sword" for an infused weapon, "Fire" with no weapon line. */
	FText MakeInfusedLabel(const FText &ElementName, const FText &WeaponTypeName)
	{
		return WeaponTypeName.IsEmpty() ? ElementName
										: FText::Format(LOCTEXT("InfusedWeapon", "{0} {1}"), ElementName, WeaponTypeName);
	}

	/** Q-503l: which stops a hold cycles. Reality on the innate element or an evolution ("(R) Fire"; an adapter
	 *  or a weapon crystal cannot), and a Broken Darkness character's Darkness ("Dark Fire"). */
	enum class EFieldHoldCycle : uint8
	{
		None,
		Reality,
		BrokenDarkness
	};

	EFieldHoldCycle GetHoldCycle(ESpellElement Element, EInfusionSourceOption Source, bool bBrokenDarkness)
	{
		if (Element == ESpellElement::Reality && (Source == EInfusionSourceOption::Innate || Source == EInfusionSourceOption::Evolution))
		{
			return EFieldHoldCycle::Reality;
		}
		return bBrokenDarkness && Element == ESpellElement::Darkness ? EFieldHoldCycle::BrokenDarkness : EFieldHoldCycle::None;
	}

	FText MakeHoldElementName(EFieldHoldCycle Cycle, ESpellElement Element)
	{
		const FText ElementName = WoRGameNames::GetElementName(Element);
		return Cycle == EFieldHoldCycle::Reality ? FText::Format(LOCTEXT("RealityHoldElement", "(R) {0}"), ElementName)
												 : FText::Format(LOCTEXT("DarkHoldElement", "Dark {0}"), ElementName);
	}
}

TArray<FFieldAttackEntry> UFieldMovementComponent::GetFieldAttackEntries() const
{
	// B-503k (D-217): the attack is the loadout's active weapon (a swing) or, for a Refractor or Resonator, the pulse;
	// the stops are its infusions: plain first (a weapon user only), then each element of the combat menu's sources,
	// one stop per element (two sources of one element would read the same).
	TArray<FFieldAttackEntry> Entries;
	TArray<EInfusionSourceOption> Sources; // each stop's source, for its hold steps (B-503l)
	AActor *Owner = GetOwner();
	const UCharacterDataComponent *CharComp = GetCharacterData();
	const UCharacterData *Data = CharComp ? CharComp->CharacterData : nullptr;
	const bool bPulse = Data && Data->CharacterClass != ECharacterClass::Generic;
	FFieldAttackEntry Plain; // the Weapon kind: a swing
	if (!bPulse)
	{
		const ULoadoutComponent *Loadout = Owner ? Owner->FindComponentByClass<ULoadoutComponent>() : nullptr;
		const FWeaponLoadoutEntry *Slot = Loadout ? Loadout->GetActiveWeaponLoadout() : nullptr;
		if (const UWeaponData *Weapon = Slot ? Slot->WeaponEntry.Weapon : nullptr)
		{
			const UAbilityData *Attack = Cast<UAbilityData>(Weapon->WeaponAttack);
			Plain.Name = WoRGameNames::GetWeaponTypeName(Weapon->WeaponType);
			Plain.Physical = Attack ? Attack->PhysicalDamageType : EPhysicalDamageType::None;
			Plain.WeaponType = Weapon->WeaponType;
			Plain.Icon = Weapon->Icon;
			Plain.SwingMontage = Weapon->WeaponAttack ? Weapon->WeaponAttack->FieldMontage : nullptr;
		}
		Entries.Add(Plain); // unarmed: a plain swing with no name
		Sources.Add(EInfusionSourceOption::None);
	}

	const UActionExecutor *Executor = WoRFight::Get<UActionExecutor>(this);
	if (!Executor || !Owner)
	{
		return Entries;
	}
	for (const EInfusionSourceOption Source : Executor->GetAvailableInfusionSources(Owner))
	{
		const ESpellElement Element = InfusionSourceOptionHelpers::HasElement(Source) ? Executor->GetElementForSourceOption(Owner, Source)
																					  : ESpellElement::None;
		const bool bListed = Entries.ContainsByPredicate([Element](const FFieldAttackEntry &Listed)
		{ return Listed.Element == Element; });
		if (!FieldSkillRules::IsElemental(Element) || bListed)
		{
			continue;
		}
		FFieldAttackEntry &Entry = Entries.Add_GetRef(Plain);
		Entry.Element = Element;
		Entry.Name = MakeInfusedLabel(WoRGameNames::GetElementName(Element), Plain.Name);
		if (bPulse)
		{
			Entry.Kind = PulseKindOf(Source);
		}
		Sources.Add(Source);
	}

	// B-503l (Q-503l = B): a hold steps a Reality or Broken Darkness stop through the elements of the other stops,
	// listed right after it as hold steps (one saved index covers both keys; a press skips them).
	const bool bBrokenDarkness = CharComp && CharComp->IsBrokenDarkness();
	TArray<FFieldAttackEntry> WithHoldSteps;
	for (int32 Stop = 0; Stop < Entries.Num(); ++Stop)
	{
		WithHoldSteps.Add(Entries[Stop]);
		const EFieldHoldCycle Cycle = GetHoldCycle(Entries[Stop].Element, Sources[Stop], bBrokenDarkness);
		if (Cycle == EFieldHoldCycle::None)
		{
			continue;
		}
		for (const FFieldAttackEntry &Other : Entries)
		{
			// Each element once (the stops are), never the stop's own nor the hidden Reality.
			if (!FieldSkillRules::IsElemental(Other.Element) || Other.Element == Entries[Stop].Element || Other.Element == ESpellElement::Reality)
			{
				continue;
			}
			FFieldAttackEntry &Step = WithHoldSteps.Add_GetRef(Entries[Stop]);
			Step.bHoldStep = true;
			Step.Element = Other.Element;
			Step.Name = MakeInfusedLabel(MakeHoldElementName(Cycle, Other.Element), Plain.Name);
		}
	}
	return WithHoldSteps;
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
