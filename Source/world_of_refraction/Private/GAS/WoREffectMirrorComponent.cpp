// WoREffectMirrorComponent.cpp

#include "GAS/WoREffectMirrorComponent.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "Character/CharacterDataComponent.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoREffectSpecBuilder.h"
#include "GAS/WoRTickExecution.h"
#include "GAS/WoRTurnEffectTracker.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Tags/WoRGameplayTags.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "WoRLog.h"

namespace
{
	/** Stacked values are copied, not recomputed, so anything above rounding noise is a change. */
	constexpr float MIRROR_MAGNITUDE_TOLERANCE = 0.001f;

	USkillEffectManager *MirrorEffectManager(const UObject *WorldContext)
	{
		const UWorld *World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return WoRFight::Get<USkillEffectManager>(GI);
	}

	/** Every stored effect of a ported type. Not IsInstant: a stored holder-turn span that counted
	 *  its landing turn reads 0 turns but is held until the holder's next turn starts. */
	bool IsMirrored(const FActiveSkillEffect &Effect)
	{
		return FWoREffectSpecBuilder::GetEffectClass(Effect.EffectType) != nullptr;
	}

	FWoRMirroredEffect ToMirrored(const FActiveSkillEffect &Effect)
	{
		FWoRMirroredEffect Entry;
		Entry.Identity = FWoREffectSpecBuilder::MakeIdentity(Effect);
		Entry.Magnitude = Effect.GetStackedValue();
		return Entry;
	}

	/** The stored effect's span as the tracker counts it (B-225: every timed effect counts holder turns). */
	FTurnSpan SpanOf(const FActiveSkillEffect &Effect)
	{
		return Effect.bCountsHolderTurns ? Effect.HolderTurns : FTurnSpan::OfHolderTurns(Effect.RemainingTurns);
	}
}

bool FWoRMirroredEffect::Matches(const FWoRMirroredEffect &Other) const
{
	return Identity.IsSameIdentity(Other.Identity) && Identity.EffectType == Other.Identity.EffectType &&
		   Identity.Element == Other.Identity.Element && Identity.DamageScope == Other.Identity.DamageScope && // B-385
		   Identity.bFromGear == Other.Identity.bFromGear &&
		   Identity.PhysicalType == Other.Identity.PhysicalType && // B-042g: the per-effect readers match on these
		   Identity.bKeyedIce == Other.Identity.bKeyedIce && // B-064c
		   FMath::IsNearlyEqual(Identity.EffectValue, Other.Identity.EffectValue, MIRROR_MAGNITUDE_TOLERANCE) &&
		   FMath::IsNearlyEqual(Magnitude, Other.Magnitude, MIRROR_MAGNITUDE_TOLERANCE);
}

UWoREffectMirrorComponent::UWoREffectMirrorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true); // B-318: the held-effects snapshot reaches co-op guests
}

void UWoREffectMirrorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWoREffectMirrorComponent, HeldSnapshot);
}

void UWoREffectMirrorComponent::OnRep_HeldSnapshot()
{
	OnHeldSnapshotChanged.Broadcast(GetOwner());
}

TArray<FWoRMirroredEffect> UWoREffectMirrorComponent::ReadEnumEffects(const AActor *Actor)
{
	TArray<FWoRMirroredEffect> Entries;
	if (const USkillEffectManager *SEM = MirrorEffectManager(Actor))
	{
		for (const FActiveSkillEffect &Effect : SEM->GetActiveEffects(const_cast<AActor *>(Actor)))
		{
			if (IsMirrored(Effect))
			{
				Entries.Add(ToMirrored(Effect));
			}
		}
	}
	return Entries;
}

void UWoREffectMirrorComponent::SyncMirror()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(WoR_SyncMirror); // B-266: Insights marker (PerfAudit_2026-10-28.md)
	AActor *Owner = GetOwner();
	UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	const USkillEffectManager *SEM = MirrorEffectManager(this);
	if (!ASC || !SEM || !Owner->HasAuthority())
	{
		return;
	}
	if (bSyncing)
	{
		bResyncRequested = true;
		return;
	}
	TGuardValue<bool> SyncingGuard(bSyncing, true);
	++SyncCount;

	UWoRTurnEffectTracker *Tracker = GetTracker(); // B-042h: with one, GAS owns the lifetime
	// Holder-turn spans only: an Immediate effect's teardown stays the enum path's (bPendingRemoval).
	const auto ShouldTrack = [Tracker](const FActiveSkillEffect &Effect)
	{
		return Tracker && !Effect.bPermanent && Effect.bCountsHolderTurns;
	};
	if (!Tracker)
	{
		EndedByGAS.Reset(); // the enum path owns the lifetime again
	}

	TArray<FActiveSkillEffect> Missing = SEM->GetActiveEffects(Owner).FilterByPredicate(IsMirrored);
	// A GE that GAS ended stays off until ReleaseEndedCopies drops its stored copy.
	EndedByGAS.RemoveAll([&Missing](const FWoREffectIdentity &Identity)
	{
		return !Missing.ContainsByPredicate([&Identity](const FActiveSkillEffect &Effect)
		{
			return Identity.IsSameIdentity(FWoREffectSpecBuilder::MakeIdentity(Effect));
		});
	});
	Missing.RemoveAll([this](const FActiveSkillEffect &Effect)
	{
		const FWoREffectIdentity Identity = FWoREffectSpecBuilder::MakeIdentity(Effect);
		return EndedByGAS.ContainsByPredicate([&Identity](const FWoREffectIdentity &Ended) { return Ended.IsSameIdentity(Identity); });
	});

	TArray<FWoRMirroredEffect> Kept;
	for (const FWoRMirroredEffect &Entry : Mirrored)
	{
		const int32 Index = Missing.IndexOfByPredicate([&Entry](const FActiveSkillEffect &Effect)
		{
			return Entry.Matches(ToMirrored(Effect));
		});
		const bool bLive = ASC->GetActiveGameplayEffect(Entry.Handle) != nullptr;
		if (Index != INDEX_NONE && !bLive && Entry.bTracked && Tracker)
		{
			EndedByGAS.Add(Entry.Identity); // GAS ended it (the tracker's span): the copy follows
			Missing.RemoveAt(Index);
		}
		else if (Index != INDEX_NONE && bLive && Entry.bTracked == ShouldTrack(Missing[Index]))
		{
			// A refresh in ApplyEffect restarts the stored span; the tracker takes it.
			const FTurnSpan Span = SpanOf(Missing[Index]);
			if (Entry.bTracked && Tracker->GetTurnsLeft(Entry.Handle) != Span.TurnsLeft)
			{
				Tracker->SetSpan(Entry.Handle, Span);
			}
			Kept.Add(Entry);
			Missing.RemoveAt(Index);
		}
		else if (!Entry.bTracked || !Tracker || !Tracker->RemoveTracked(Entry.Handle))
		{
			ASC->RemoveActiveGameplayEffect(Entry.Handle);
		}
	}

	for (const FActiveSkillEffect &Effect : Missing)
	{
		const FWoRBuiltEffect Built = FWoREffectSpecBuilder::Build(*ASC, Effect, Effect.SourceActor.Get(), true);
		if (!Built.IsValid())
		{
			continue;
		}
		// Untracked when the enum path owns the lifetime (no tracker) and for permanent
		// effects. The tracker doesn't pay the tick: the effect manager's turn pass calls PayTick (Q-042y).
		FWoRMirroredEffect Entry = ToMirrored(Effect);
		Entry.bTracked = ShouldTrack(Effect);
		Entry.TickSpec = Built.TickSpec;
		Entry.Handle = Entry.bTracked ? Tracker->ApplySpecForSpan(Built.Spec, SpanOf(Effect))
									  : ASC->ApplyGameplayEffectSpecToSelf(*Built.Spec.Data.Get());
		if (Entry.Handle.IsValid())
		{
			Kept.Add(Entry);
		}
	}
	Mirrored = MoveTemp(Kept);
	SyncedFingerprint = GetSyncFingerprint();
	// B-318: the read a client can't make itself, kept for replication.
	HeldSnapshot = BuildHeld(SEM->GetActiveEffects(Owner));

	if (bResyncRequested)
	{
		bResyncRequested = false;
		bSyncing = false;
		SyncMirror();
	}
}

uint32 UWoREffectMirrorComponent::GetSyncFingerprint() const
{
	AActor *Owner = GetOwner();
	const UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	const USkillEffectManager *SEM = MirrorEffectManager(this);
	uint32 Hash = PointerHash(GetTracker());
	Hash = HashCombineFast(Hash, GetTypeHash(ASC ? ASC->GetActiveGameplayEffects().GetNumGameplayEffects() : INDEX_NONE));
	// Which of our GEs are still live (one ending while another GE lands keeps the count).
	for (const FWoRMirroredEffect &Entry : Mirrored)
	{
		Hash = HashCombineFast(Hash, ASC && ASC->GetActiveGameplayEffect(Entry.Handle) ? 1u : 0u);
	}
	const TArray<FActiveSkillEffect> *Stored = SEM ? SEM->FindActiveEffects(Owner) : nullptr;
	if (!Stored)
	{
		return Hash;
	}
	for (const FActiveSkillEffect &Effect : *Stored)
	{
		// Every field the sync's identity, magnitude and span checks read.
		Hash = HashCombineFast(Hash, GetTypeHash(Effect.EffectID));
		Hash = HashCombineFast(Hash, GetTypeHash(Effect.SourceKey));
		Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Effect.EffectType)));
		Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Effect.Element)));
		Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Effect.DamageScope))); // B-385: Matches compares it
		Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Effect.PhysicalType))); // B-042g: Matches compares these
		Hash = HashCombineFast(Hash, GetTypeHash(Effect.EffectValue));
		Hash = HashCombineFast(Hash, GetTypeHash(Effect.GetStackedValue()));
		Hash = HashCombineFast(Hash, GetTypeHash(Effect.RemainingTurns));
		Hash = HashCombineFast(Hash, GetTypeHash(Effect.HolderTurns.TurnsLeft));
		const uint32 Flags = (Effect.bFromGear ? 1u : 0u) | (Effect.bPermanent ? 2u : 0u) | (Effect.bCountsHolderTurns ? 4u : 0u);
		Hash = HashCombineFast(Hash, Flags);
	}
	return Hash;
}

UWoRTurnEffectTracker *UWoREffectMirrorComponent::GetTracker() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UWoRTurnEffectTracker>() : nullptr;
}

int32 UWoREffectMirrorComponent::GetTrackedTurnsLeft(const FActiveSkillEffect &Effect) const
{
	const UWoRTurnEffectTracker *Tracker = GetTracker();
	const FWoREffectIdentity Identity = FWoREffectSpecBuilder::MakeIdentity(Effect);
	const FWoRMirroredEffect *Entry = Mirrored.FindByPredicate([&Identity](const FWoRMirroredEffect &Mirror)
	{
		return Mirror.bTracked && Mirror.Identity.IsSameIdentity(Identity);
	});
	return Entry && Tracker ? Tracker->GetTurnsLeft(Entry->Handle) : INDEX_NONE;
}

TArray<FActiveSkillEffect> UWoREffectMirrorComponent::ReadHeldEffects(const TArray<FActiveSkillEffect> &Stored)
{
	// B-318 (Q-318): a client reads the server's GAS read, replicated; its own stored list isn't the truth.
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		return HeldSnapshot;
	}
	UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	// Mid-sync, Mirrored is half-updated: the stored list is the safe answer.
	if (!ASC || bSyncing)
	{
		return Stored;
	}
	SyncMirror();
	return BuildHeld(Stored);
}

TArray<FActiveSkillEffect> UWoREffectMirrorComponent::BuildHeld(const TArray<FActiveSkillEffect> &Stored)
{
	UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (!ASC)
	{
		return Stored;
	}
	const UWoRTurnEffectTracker *Tracker = GetTracker();
	TArray<FActiveSkillEffect> Held;
	for (const FActiveSkillEffect &Effect : Stored)
	{
		const FWoREffectIdentity Identity = FWoREffectSpecBuilder::MakeIdentity(Effect);
		if (EndedByGAS.ContainsByPredicate([&Identity](const FWoREffectIdentity &Ended) { return Ended.IsSameIdentity(Identity); }))
		{
			continue; // GAS ended it; its stored copy goes at the next release (Q-042ye)
		}
		FActiveSkillEffect &Read = Held.Add_GetRef(Effect);
		if (!IsMirrored(Effect))
		{
			continue; // no family GE: GAS doesn't hold this type yet
		}
		const FWoRMirroredEffect Wanted = ToMirrored(Effect);
		const FWoRMirroredEffect *Entry = Mirrored.FindByPredicate([&Wanted](const FWoRMirroredEffect &Mirror) { return Mirror.Matches(Wanted); });
		// A GE that didn't build or apply keeps its stored copy as it is.
		const bool bLive = Entry && ASC->GetActiveGameplayEffect(Entry->Handle);
		const int32 TurnsLeft = bLive && Entry->bTracked && Tracker ? Tracker->GetTurnsLeft(Entry->Handle) : INDEX_NONE;
		if (TurnsLeft >= 0)
		{
			Read.RemainingTurns = TurnsLeft;
			Read.HolderTurns.TurnsLeft = TurnsLeft;
		}
	}
	return Held;
}

int32 UWoREffectMirrorComponent::PreviewDotTick(const FActiveSkillEffect &Effect)
{
	UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	// As PayTick: where GAS wouldn't pay the tick, the enum path's number is the one paid.
	const UCharacterDataComponent *Char = GetOwner() ? GetOwner()->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (!ASC || !GetOwner()->HasAuthority() || bSyncing || Effect.EffectType != ESkillEffectType::DOT ||
		!ASC->HasAttributeSetForAttribute(UWoRAttributeSet_Vitals::GetHealthAttribute()) || !Char || !Char->CharacterData)
	{
		return INDEX_NONE;
	}
	SyncMirror();
	const FWoRMirroredEffect Wanted = ToMirrored(Effect);
	const FWoRMirroredEffect *Entry = Mirrored.FindByPredicate([&Wanted](const FWoRMirroredEffect &Mirror) { return Mirror.Matches(Wanted); });
	const FWoREffectContext *TickContext =
		Entry && Entry->TickSpec.IsValid() && ASC->GetActiveGameplayEffect(Entry->Handle)
			? FWoREffectContext::Get(Entry->TickSpec.Data->GetContext())
			: nullptr;
	if (!TickContext || !TickContext->TickRequest)
	{
		return INDEX_NONE;
	}
	const FWoRTickRequest &Request = *TickContext->TickRequest;
	// The execution's Health change is -damage (a DOT only ever damages).
	return FMath::RoundToInt(-UWoRTickExecution::GetHealthChange(Request, UWoRTickExecution::ReadAttributes(Request, ASC)));
}

void UWoREffectMirrorComponent::ForgetEnded(const FActiveSkillEffect &Effect)
{
	const FWoREffectIdentity Identity = FWoREffectSpecBuilder::MakeIdentity(Effect);
	EndedByGAS.RemoveAll([&Identity](const FWoREffectIdentity &Ended) { return Ended.IsSameIdentity(Identity); });
	SyncedFingerprint.Reset(); // the sync reads EndedByGAS, which the fingerprint doesn't hash
}

bool UWoREffectMirrorComponent::PayTick(const FActiveSkillEffect &Effect)
{
	UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	// Without the Vitals set the tick execution has no Health / Energy to change, and without character data
	// the Vitals set has no HP / EP path to hand it to.
	const UCharacterDataComponent *Char = GetOwner()->FindComponentByClass<UCharacterDataComponent>();
	if (!ASC || !ASC->HasAttributeSetForAttribute(UWoRAttributeSet_Vitals::GetHealthAttribute()) || !Char ||
		!Char->CharacterData)
	{
		return false;
	}
	FGameplayEffectSpecHandle Tick;
	if (Effect.IsInstant())
	{
		// B-042yd: a one-shot is never stored, so never mirrored: its payout is a tick spec of its own.
		Tick = FWoREffectSpecBuilder::Build(*ASC, Effect, Effect.SourceActor.Get(), true).TickSpec;
	}
	else
	{
		SyncMirror(); // a payout on landing comes before the applied event's sync (Q-063)
		const FWoRMirroredEffect Wanted = ToMirrored(Effect);
		const FWoRMirroredEffect *Entry = Mirrored.FindByPredicate([&Wanted](const FWoRMirroredEffect &Mirror) { return Mirror.Matches(Wanted); });
		if (Entry && ASC->GetActiveGameplayEffect(Entry->Handle))
		{
			// A copy keeps the spec alive: a lethal tick's death path can resync (and rebuild) Mirrored mid-apply.
			Tick = Entry->TickSpec;
		}
	}
	if (!Tick.IsValid())
	{
		return false;
	}
	ASC->ApplyGameplayEffectSpecToSelf(*Tick.Data.Get());
	UE_LOG(LogWoREffects, Verbose, TEXT("[EffectMirror] %s: %s %s paid through GAS"), *GetNameSafe(GetOwner()),
		   *UEnum::GetValueAsString(Effect.EffectType), *FWoREffectSpecBuilder::MakeIdentity(Effect).ToString());
	return true;
}

void UWoREffectMirrorComponent::ReleaseEndedCopies()
{
	SyncMirror(); // notes the GEs the tracker just ended
	USkillEffectManager *SEM = MirrorEffectManager(this);
	// A copy: each removal's sync must still see the others as ended (not re-apply them); it prunes
	// each identity once its copy is gone.
	const TArray<FWoREffectIdentity> Ended = EndedByGAS;
	for (const FWoREffectIdentity &Identity : Ended)
	{
		UE_LOG(LogWoREffects, Log, TEXT("[EffectMirror] %s: GAS ended %s; its stored copy goes"), *GetNameSafe(GetOwner()),
			   *Identity.ToString());
		if (SEM)
		{
			SEM->RemoveEffectByIdentity(GetOwner(), Identity.EffectID, Identity.SourceKey);
		}
	}
}

const UAbilitySystemComponent *UWoREffectMirrorComponent::GetGASReadSource(const AActor *Actor, ESkillEffectType Type)
{
	if (!Actor || !FWoREffectSpecBuilder::GetEffectClass(Type))
	{
		return nullptr;
	}
	UWoREffectMirrorComponent *Mirror = Actor->FindComponentByClass<UWoREffectMirrorComponent>();
	if (!Mirror)
	{
		return nullptr;
	}
	// The read may come between a change and its event (NotifySpeedChanged, another listener), so it checks
	// what the sync reads rather than waiting for an event (B-261: it skips the sync when nothing changed).
	if (!Mirror->SyncedFingerprint.IsSet() || Mirror->GetSyncFingerprint() != Mirror->SyncedFingerprint.GetValue())
	{
		Mirror->SyncMirror();
	}
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
}

bool UWoREffectMirrorComponent::HasEffectOfType(const UAbilitySystemComponent &ASC, ESkillEffectType Type)
{
	const FGameplayTag Tag = WoRTags::EffectTypeToTag(Type);
	if (Tag.IsValid())
	{
		return ASC.HasMatchingGameplayTag(Tag);
	}
	const FGameplayEffectQuery OfType(FActiveGameplayEffectQueryCustomMatch::CreateLambda(
		[Type](const FActiveGameplayEffect &Active)
		{
			const FWoREffectContext *Context = FWoREffectContext::Get(Active.Spec.GetContext());
			return Context && Context->Identity.EffectType == Type;
		}));
	return ASC.GetActiveEffects(OfType).Num() > 0;
}

bool UWoREffectMirrorComponent::HasCleansableEffectOfType(const UAbilitySystemComponent &ASC, ESkillEffectType Type)
{
	const FGameplayEffectQuery Cleansable(FActiveGameplayEffectQueryCustomMatch::CreateLambda(
		[Type](const FActiveGameplayEffect &Active)
		{
			const FWoREffectContext *Context = FWoREffectContext::Get(Active.Spec.GetContext());
			FGameplayTagContainer AssetTags;
			Active.Spec.GetAllAssetTags(AssetTags);
			return Context && Context->Identity.EffectType == Type && !AssetTags.HasTagExact(WoRTags::Effect_Origin_Gear);
		}));
	return ASC.GetActiveEffects(Cleansable).Num() > 0;
}

FString UWoREffectMirrorComponent::GetDebugString() const
{
	const UWoRTurnEffectTracker *Tracker = GetTracker();
	TArray<FString> Lines;
	for (const FWoRMirroredEffect &Entry : Mirrored)
	{
		const FString Lifetime = Entry.bTracked && Tracker
									 ? FString::Printf(TEXT("tracked, %d turn(s) left"), Tracker->GetTurnsLeft(Entry.Handle))
									 : FString(TEXT("untracked"));
		Lines.Add(FString::Printf(TEXT("%s magnitude=%.2f handle=%s %s%s"), *Entry.Identity.ToString(), Entry.Magnitude,
								  *Entry.Handle.ToString(), *Lifetime, Entry.TickSpec.IsValid() ? TEXT(", pays ticks") : TEXT("")));
	}
	for (const FActiveSkillEffect &Held : HeldSnapshot)
	{
		Lines.Add(FString::Printf(TEXT("snapshot (B-318): %s id=%d, %d turn(s) left"), *Held.EffectName, Held.EffectID, Held.RemainingTurns));
	}
	for (const FWoREffectIdentity &Identity : EndedByGAS)
	{
		Lines.Add(FString::Printf(TEXT("%s ended by GAS, stored copy pending release"), *Identity.ToString()));
	}
	Lines.Add(FString::Printf(TEXT("%d sync(s); last fingerprint %s, now %u"), SyncCount,
							  SyncedFingerprint.IsSet() ? *FString::Printf(TEXT("%u"), SyncedFingerprint.GetValue()) : TEXT("<none>"),
							  GetSyncFingerprint()));
	return FString::Join(Lines, TEXT("\n"));
}

void UWoREffectMirrorComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		UE_LOG(LogWoREffects, Warning, TEXT("[EffectMirror] %s has no AbilitySystemComponent; effects not mirrored"), *GetOwner()->GetName());
		return;
	}
	BindSources();
	SyncMirror();
}

void UWoREffectMirrorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindSources();
	Super::EndPlay(EndPlayReason);
}

void UWoREffectMirrorComponent::HandleEffectChanged(AActor *Target, const FActiveSkillEffect &Effect)
{
	if (Target == GetOwner())
	{
		SyncMirror();
	}
}

void UWoREffectMirrorComponent::HandleEffectCountChanged(AActor *Target, const FActiveSkillEffect &Effect, int32 NewCount)
{
	if (Target == GetOwner())
	{
		SyncMirror();
	}
}

void UWoREffectMirrorComponent::BindSources()
{
	if (USkillEffectManager *SEM = MirrorEffectManager(this))
	{
		SEM->OnEffectApplied.AddUniqueDynamic(this, &UWoREffectMirrorComponent::HandleEffectChanged);
		SEM->OnEffectRemoved.AddUniqueDynamic(this, &UWoREffectMirrorComponent::HandleEffectChanged);
		// A same-source replace reports through the duration event, with the new value stored.
		SEM->OnEffectStacksChanged.AddUniqueDynamic(this, &UWoREffectMirrorComponent::HandleEffectCountChanged);
		SEM->OnEffectDurationChanged.AddUniqueDynamic(this, &UWoREffectMirrorComponent::HandleEffectCountChanged);
	}
}

void UWoREffectMirrorComponent::UnbindSources()
{
	if (USkillEffectManager *SEM = MirrorEffectManager(this))
	{
		SEM->OnEffectApplied.RemoveDynamic(this, &UWoREffectMirrorComponent::HandleEffectChanged);
		SEM->OnEffectRemoved.RemoveDynamic(this, &UWoREffectMirrorComponent::HandleEffectChanged);
		SEM->OnEffectStacksChanged.RemoveDynamic(this, &UWoREffectMirrorComponent::HandleEffectCountChanged);
		SEM->OnEffectDurationChanged.RemoveDynamic(this, &UWoREffectMirrorComponent::HandleEffectCountChanged);
	}
}
