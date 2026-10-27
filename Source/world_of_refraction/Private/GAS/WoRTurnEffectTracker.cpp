// WoRTurnEffectTracker.cpp

#include "GAS/WoRTurnEffectTracker.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoREffectSpecBuilder.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr int32 MIN_TRACKED_TURNS = 1;
	constexpr float TRACKED_EFFECT_LEVEL = 1.0f;

	FAutoConsoleCommandWithWorld GDumpTurnEffectsCmd(
		TEXT("wor.DumpTurnEffects"),
		TEXT("DEBUG: print every combatant's turn-counted GameplayEffects (B-042)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (const UWoRTurnEffectTracker *Tracker = It->FindComponentByClass<UWoRTurnEffectTracker>())
				{
					Tracker->PrintState();
					if (const UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(*It))
					{
						UE_LOG(LogWoREffects, Log, TEXT("[TurnEffectTracker] %s GEs:\n%s"), *It->GetName(),
							   *FWoREffectSpecBuilder::GetDebugString(*ASC));
					}
				}
			}
		}));
}

UWoRTurnEffectTracker::UWoRTurnEffectTracker()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWoRTurnEffectTracker::BeginPlay()
{
	Super::BeginPlay();
	BindRemovedEvent();
}

void UWoRTurnEffectTracker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent *ASC = BoundASC.Get())
	{
		ASC->OnAnyGameplayEffectRemovedDelegate().Remove(RemovedHandle);
	}
	BoundASC.Reset();
	RemovedHandle.Reset();
	// TODO: co-op (D-065) - battle pawns die with the fight today, so the GEs go with the ASC; a
	// pawn that outlives a fight should remove its tracked GEs here.
	Tracked.Reset();
	Super::EndPlay(EndPlayReason);
}

UAbilitySystemComponent *UWoRTurnEffectTracker::GetOwnerASC() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

void UWoRTurnEffectTracker::BindRemovedEvent()
{
	UAbilitySystemComponent *ASC = GetOwnerASC();
	if (!ASC || BoundASC.Get() == ASC)
	{
		return;
	}
	RemovedHandle = ASC->OnAnyGameplayEffectRemovedDelegate().AddUObject(this, &UWoRTurnEffectTracker::HandleEffectRemoved);
	BoundASC = ASC;
}

FActiveGameplayEffectHandle UWoRTurnEffectTracker::ApplyForHolderTurns(TSubclassOf<UGameplayEffect> EffectClass, int32 Turns, AActor *Instigator)
{
	UAbilitySystemComponent *ASC = GetOwnerASC();
	if (!ASC || !EffectClass)
	{
		UE_LOG(LogWoREffects, Warning, TEXT("[TurnEffectTracker] %s: can't apply %s (ASC %s)"),
			   *GetNameSafe(GetOwner()), *GetNameSafe(EffectClass.Get()), ASC ? TEXT("ok") : TEXT("missing"));
		return FActiveGameplayEffectHandle();
	}
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(Instigator, Instigator);
	return ApplySpecForHolderTurns(ASC->MakeOutgoingSpec(EffectClass, TRACKED_EFFECT_LEVEL, Context), Turns);
}

FActiveGameplayEffectHandle UWoRTurnEffectTracker::ApplySpecForHolderTurns(const FGameplayEffectSpecHandle &Spec, int32 Turns,
																		   const FGameplayEffectSpecHandle &TickSpec)
{
	UAbilitySystemComponent *ASC = GetOwnerASC();
	const UGameplayEffect *Def = Spec.IsValid() ? Spec.Data->Def.Get() : nullptr;
	if (!ASC || !Def || Turns < MIN_TRACKED_TURNS)
	{
		UE_LOG(LogWoREffects, Warning, TEXT("[TurnEffectTracker] %s: can't apply %s for %d turn(s) (ASC %s)"),
			   *GetNameSafe(GetOwner()), *GetNameSafe(Def), Turns, ASC ? TEXT("ok") : TEXT("missing"));
		return FActiveGameplayEffectHandle();
	}
	if (Def->DurationPolicy != EGameplayEffectDurationType::Infinite)
	{
		UE_LOG(LogWoREffects, Warning, TEXT("[TurnEffectTracker] %s is not Infinite; only Infinite GEs are turn-counted"),
			   *Def->GetName());
		return FActiveGameplayEffectHandle();
	}
	BindRemovedEvent();

	const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (Handle.IsValid())
	{
		Tracked.Add(Handle, FTrackedEffect{FTurnSpan::OfHolderTurns(Turns), TickSpec, NextArrival++});
		UE_LOG(LogWoREffects, Log, TEXT("[TurnEffectTracker] %s gains %s for %d holder turn(s)%s"),
			   *GetNameSafe(GetOwner()), *Def->GetName(), Turns, TickSpec.IsValid() ? TEXT(", ticking") : TEXT(""));
	}
	return Handle;
}

void UWoRTurnEffectTracker::OnHolderTurnStart()
{
	TArray<FActiveGameplayEffectHandle> Finished;
	for (const TPair<FActiveGameplayEffectHandle, FTrackedEffect> &Entry : Tracked)
	{
		if (Entry.Value.Span.EndsAtHolderTurnStart())
		{
			Finished.Add(Entry.Key);
		}
	}
	for (const FActiveGameplayEffectHandle &Handle : Finished)
	{
		UE_LOG(LogWoREffects, Log, TEXT("[TurnEffectTracker] %s: a span ended at turn start"), *GetNameSafe(GetOwner()));
		RemoveTracked(Handle);
	}

	// Copied first: a payout can kill the holder, and its removals reach Tracked through the ASC.
	TArray<const FTrackedEffect *> Ticking;
	for (const TPair<FActiveGameplayEffectHandle, FTrackedEffect> &Entry : Tracked)
	{
		if (Entry.Value.TickSpec.IsValid())
		{
			Ticking.Add(&Entry.Value);
		}
	}
	Ticking.Sort([](const FTrackedEffect &A, const FTrackedEffect &B) { return A.Arrival < B.Arrival; });
	TArray<FGameplayEffectSpecHandle> Ticks;
	for (const FTrackedEffect *Entry : Ticking)
	{
		Ticks.Add(Entry->TickSpec);
	}

	UAbilitySystemComponent *ASC = GetOwnerASC();
	for (const FGameplayEffectSpecHandle &Tick : Ticks)
	{
		if (!ASC || IsHolderDown())
		{
			break;
		}
		ASC->ApplyGameplayEffectSpecToSelf(*Tick.Data.Get());
	}
}

bool UWoRTurnEffectTracker::IsHolderDown() const
{
	const UAbilitySystemComponent *ASC = GetOwnerASC();
	const FGameplayAttribute Health = UWoRAttributeSet_Vitals::GetHealthAttribute();
	return ASC && ASC->HasAttributeSetForAttribute(Health) && ASC->GetNumericAttribute(Health) <= 0.0f;
}

void UWoRTurnEffectTracker::OnHolderTurnEnd()
{
	for (TPair<FActiveGameplayEffectHandle, FTrackedEffect> &Entry : Tracked)
	{
		Entry.Value.Span.CountHolderTurn();
	}
}

bool UWoRTurnEffectTracker::RemoveTracked(FActiveGameplayEffectHandle Handle)
{
	if (Tracked.Remove(Handle) == 0)
	{
		return false;
	}
	if (UAbilitySystemComponent *ASC = GetOwnerASC())
	{
		ASC->RemoveActiveGameplayEffect(Handle);
	}
	return true;
}

int32 UWoRTurnEffectTracker::GetTurnsLeft(FActiveGameplayEffectHandle Handle) const
{
	const FTrackedEffect *Entry = Tracked.Find(Handle);
	return Entry ? Entry->Span.TurnsLeft : -1;
}

void UWoRTurnEffectTracker::HandleEffectRemoved(const FActiveGameplayEffect &Removed)
{
	Tracked.Remove(Removed.Handle);
}

FString UWoRTurnEffectTracker::GetDebugString() const
{
	const UAbilitySystemComponent *ASC = GetOwnerASC();
	FString Out = FString::Printf(TEXT("%s: %d turn-counted effect(s)"), *GetNameSafe(GetOwner()), Tracked.Num());
	for (const TPair<FActiveGameplayEffectHandle, FTrackedEffect> &Entry : Tracked)
	{
		const FActiveGameplayEffect *Active = ASC ? ASC->GetActiveGameplayEffect(Entry.Key) : nullptr;
		const UGameplayEffect *Def = Active ? Active->Spec.Def.Get() : nullptr;
		Out += FString::Printf(TEXT("\n  %s [%s]: %s"), *GetNameSafe(Def), *Entry.Key.ToString(), *Entry.Value.Span.GetDebugString());
	}
	return Out;
}

void UWoRTurnEffectTracker::PrintState() const
{
	UE_LOG(LogWoREffects, Log, TEXT("[TurnEffectTracker] %s"), *GetDebugString());
}
