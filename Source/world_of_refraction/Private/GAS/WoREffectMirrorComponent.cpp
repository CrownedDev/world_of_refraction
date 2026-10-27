// WoREffectMirrorComponent.cpp

#include "GAS/WoREffectMirrorComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "Character/CharacterDataComponent.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoREffectSpecBuilder.h"
#include "GAS/WoRTurnEffectTracker.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Tags/WoRGameplayTags.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	/** Stacked values are copied, not recomputed, so anything above rounding noise is a change. */
	constexpr float MIRROR_MAGNITUDE_TOLERANCE = 0.001f;

	int32 GWoREffectsUseGAS = 0;
	FAutoConsoleVariableRef CVarWoREffectsUseGAS(
		TEXT("wor.Effects.UseGAS"), GWoREffectsUseGAS,
		TEXT("1: the effect manager's type and total reads use the mirrored GameplayEffects for ported types (B-042c, Q-042c). 0: the enum path (default)."),
		ECVF_Default);

	USkillEffectManager *MirrorEffectManager(const UObject *WorldContext)
	{
		const UWorld *World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<USkillEffectManager>() : nullptr;
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
		   Identity.Element == Other.Identity.Element && Identity.bFromGear == Other.Identity.bFromGear &&
		   FMath::IsNearlyEqual(Magnitude, Other.Magnitude, MIRROR_MAGNITUDE_TOLERANCE);
}

UWoREffectMirrorComponent::UWoREffectMirrorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UWoREffectMirrorComponent::ReadsFromGAS()
{
	return GWoREffectsUseGAS != 0;
}

void UWoREffectMirrorComponent::SetReadsFromGAS(bool bEnabled)
{
	GWoREffectsUseGAS = bEnabled ? 1 : 0;
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

	UWoRTurnEffectTracker *Tracker = OwnsLifetime(Owner) ? GetTracker() : nullptr;
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
		// Untracked when the enum path owns the lifetime (UseGAS 0, or no tracker) and for permanent
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

	if (bResyncRequested)
	{
		bResyncRequested = false;
		bSyncing = false;
		SyncMirror();
	}
}

UWoRTurnEffectTracker *UWoREffectMirrorComponent::GetTracker() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UWoRTurnEffectTracker>() : nullptr;
}

bool UWoREffectMirrorComponent::OwnsLifetime(const AActor *Actor)
{
	return ReadsFromGAS() && Actor && Actor->FindComponentByClass<UWoREffectMirrorComponent>() &&
		   Actor->FindComponentByClass<UWoRTurnEffectTracker>();
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

void UWoREffectMirrorComponent::ForgetEnded(const FActiveSkillEffect &Effect)
{
	const FWoREffectIdentity Identity = FWoREffectSpecBuilder::MakeIdentity(Effect);
	EndedByGAS.RemoveAll([&Identity](const FWoREffectIdentity &Ended) { return Ended.IsSameIdentity(Identity); });
}

bool UWoREffectMirrorComponent::PayTick(const FActiveSkillEffect &Effect)
{
	SyncMirror(); // a payout on landing comes before the applied event's sync (Q-063)
	UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	const FWoRMirroredEffect Wanted = ToMirrored(Effect);
	const FWoRMirroredEffect *Entry = Mirrored.FindByPredicate([&Wanted](const FWoRMirroredEffect &Mirror) { return Mirror.Matches(Wanted); });
	// Without the Vitals set the tick execution has no Health to change, and without character data the
	// Vitals set has no HP path to hand it to.
	const UCharacterDataComponent *Char = GetOwner()->FindComponentByClass<UCharacterDataComponent>();
	if (!ASC || !ASC->HasAttributeSetForAttribute(UWoRAttributeSet_Vitals::GetHealthAttribute()) || !Char ||
		!Char->CharacterData || !Entry ||
		!Entry->TickSpec.IsValid() || !ASC->GetActiveGameplayEffect(Entry->Handle))
	{
		return false;
	}
	// A copy keeps the spec alive: a lethal tick's death path can resync (and rebuild) Mirrored mid-apply.
	const FGameplayEffectSpecHandle Tick = Entry->TickSpec;
	ASC->ApplyGameplayEffectSpecToSelf(*Tick.Data.Get());
	UE_LOG(LogWoREffects, Verbose, TEXT("[EffectMirror] %s: %s tick paid through GAS"), *GetNameSafe(GetOwner()),
		   *Wanted.Identity.ToString());
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
	if (!ReadsFromGAS() || !Actor || !FWoREffectSpecBuilder::GetEffectClass(Type))
	{
		return nullptr;
	}
	UWoREffectMirrorComponent *Mirror = Actor->FindComponentByClass<UWoREffectMirrorComponent>();
	if (!Mirror)
	{
		return nullptr;
	}
	// The read may come between a change and its event (NotifySpeedChanged, another listener).
	Mirror->SyncMirror();
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
	for (const FWoREffectIdentity &Identity : EndedByGAS)
	{
		Lines.Add(FString::Printf(TEXT("%s ended by GAS, stored copy pending release"), *Identity.ToString()));
	}
	return Lines.Num() > 0 ? FString::Join(Lines, TEXT("\n")) : TEXT("nothing mirrored");
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
