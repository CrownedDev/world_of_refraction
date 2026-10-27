// WoREffectMirrorComponent.cpp

#include "GAS/WoREffectMirrorComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "GAS/WoREffectSpecBuilder.h"
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

	bool IsMirrored(const FActiveSkillEffect &Effect)
	{
		return !Effect.IsInstant() && FWoREffectSpecBuilder::GetEffectClass(Effect.EffectType) != nullptr;
	}

	FWoRMirroredEffect ToMirrored(const FActiveSkillEffect &Effect)
	{
		FWoRMirroredEffect Entry;
		Entry.Identity = FWoREffectSpecBuilder::MakeIdentity(Effect);
		Entry.Magnitude = Effect.GetStackedValue();
		return Entry;
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

	TArray<FActiveSkillEffect> Missing = SEM->GetActiveEffects(Owner).FilterByPredicate(IsMirrored);
	TArray<FWoRMirroredEffect> Kept;
	for (const FWoRMirroredEffect &Entry : Mirrored)
	{
		const int32 Index = Missing.IndexOfByPredicate([&Entry](const FActiveSkillEffect &Effect)
		{
			return Entry.Matches(ToMirrored(Effect));
		});
		if (Index != INDEX_NONE && ASC->GetActiveGameplayEffect(Entry.Handle))
		{
			Kept.Add(Entry);
			Missing.RemoveAt(Index);
		}
		else
		{
			ASC->RemoveActiveGameplayEffect(Entry.Handle);
		}
	}

	for (const FActiveSkillEffect &Effect : Missing)
	{
		const FWoRBuiltEffect Built = FWoREffectSpecBuilder::Build(*ASC, Effect, Effect.SourceActor.Get());
		if (!Built.IsValid())
		{
			continue;
		}
		// Untracked and tick-less: the enum path counts the turns and pays the ticks.
		FWoRMirroredEffect Entry = ToMirrored(Effect);
		Entry.Handle = ASC->ApplyGameplayEffectSpecToSelf(*Built.Spec.Data.Get());
		if (Entry.Handle.IsValid())
		{
			Kept.Add(Entry);
		}
	}
	Mirrored = MoveTemp(Kept);
}

const UAbilitySystemComponent *UWoREffectMirrorComponent::GetGASReadSource(const AActor *Actor, ESkillEffectType Type)
{
	if (!ReadsFromGAS() || !Actor || !FWoREffectSpecBuilder::GetEffectClass(Type) ||
		!Actor->FindComponentByClass<UWoREffectMirrorComponent>())
	{
		return nullptr;
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

FString UWoREffectMirrorComponent::GetDebugString() const
{
	TArray<FString> Lines;
	for (const FWoRMirroredEffect &Entry : Mirrored)
	{
		Lines.Add(FString::Printf(TEXT("%s magnitude=%.2f handle=%s"), *Entry.Identity.ToString(), Entry.Magnitude,
								  *Entry.Handle.ToString()));
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
