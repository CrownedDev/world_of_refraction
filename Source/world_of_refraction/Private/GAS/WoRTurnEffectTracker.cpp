// WoRTurnEffectTracker.cpp

#include "GAS/WoRTurnEffectTracker.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
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
	Spans.Reset();
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
	const UGameplayEffect *EffectCDO = EffectClass ? EffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;
	if (!ASC || !EffectCDO || Turns < MIN_TRACKED_TURNS)
	{
		UE_LOG(LogWoREffects, Warning, TEXT("[TurnEffectTracker] %s: can't apply %s for %d turn(s) (ASC %s)"),
			   *GetNameSafe(GetOwner()), *GetNameSafe(EffectClass.Get()), Turns, ASC ? TEXT("ok") : TEXT("missing"));
		return FActiveGameplayEffectHandle();
	}
	if (EffectCDO->DurationPolicy != EGameplayEffectDurationType::Infinite)
	{
		UE_LOG(LogWoREffects, Warning, TEXT("[TurnEffectTracker] %s is not Infinite; only Infinite GEs are turn-counted"),
			   *EffectClass->GetName());
		return FActiveGameplayEffectHandle();
	}
	BindRemovedEvent();

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(Instigator, Instigator);
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, TRACKED_EFFECT_LEVEL, Context);
	if (!Spec.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (Handle.IsValid())
	{
		Spans.Add(Handle, FTurnSpan::OfHolderTurns(Turns));
		UE_LOG(LogWoREffects, Log, TEXT("[TurnEffectTracker] %s gains %s for %d holder turn(s)"),
			   *GetNameSafe(GetOwner()), *EffectClass->GetName(), Turns);
	}
	return Handle;
}

void UWoRTurnEffectTracker::OnHolderTurnStart()
{
	TArray<FActiveGameplayEffectHandle> Finished;
	for (const TPair<FActiveGameplayEffectHandle, FTurnSpan> &Entry : Spans)
	{
		if (Entry.Value.EndsAtHolderTurnStart())
		{
			Finished.Add(Entry.Key);
		}
	}
	for (const FActiveGameplayEffectHandle &Handle : Finished)
	{
		UE_LOG(LogWoREffects, Log, TEXT("[TurnEffectTracker] %s: a span ended at turn start"), *GetNameSafe(GetOwner()));
		RemoveTracked(Handle);
	}
}

void UWoRTurnEffectTracker::OnHolderTurnEnd()
{
	for (TPair<FActiveGameplayEffectHandle, FTurnSpan> &Entry : Spans)
	{
		Entry.Value.CountHolderTurn();
	}
}

bool UWoRTurnEffectTracker::RemoveTracked(FActiveGameplayEffectHandle Handle)
{
	if (Spans.Remove(Handle) == 0)
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
	const FTurnSpan *Span = Spans.Find(Handle);
	return Span ? Span->TurnsLeft : -1;
}

void UWoRTurnEffectTracker::HandleEffectRemoved(const FActiveGameplayEffect &Removed)
{
	Spans.Remove(Removed.Handle);
}

FString UWoRTurnEffectTracker::GetDebugString() const
{
	const UAbilitySystemComponent *ASC = GetOwnerASC();
	FString Out = FString::Printf(TEXT("%s: %d turn-counted effect(s)"), *GetNameSafe(GetOwner()), Spans.Num());
	for (const TPair<FActiveGameplayEffectHandle, FTurnSpan> &Entry : Spans)
	{
		const FActiveGameplayEffect *Active = ASC ? ASC->GetActiveGameplayEffect(Entry.Key) : nullptr;
		const UGameplayEffect *Def = Active ? Active->Spec.Def.Get() : nullptr;
		Out += FString::Printf(TEXT("\n  %s [%s]: %s"), *GetNameSafe(Def), *Entry.Key.ToString(), *Entry.Value.GetDebugString());
	}
	return Out;
}

void UWoRTurnEffectTracker::PrintState() const
{
	UE_LOG(LogWoREffects, Log, TEXT("[TurnEffectTracker] %s"), *GetDebugString());
}
