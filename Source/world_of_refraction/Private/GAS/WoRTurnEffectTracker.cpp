// WoRTurnEffectTracker.cpp

#include "GAS/WoRTurnEffectTracker.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "Character/CharacterDataComponent.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoREffectSpecBuilder.h"
#include "GAS/WoRGameplayEffects.h"
#include "Tags/WoRGameplayTags.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr int32 MIN_TRACKED_TURNS = 1;
	constexpr float TRACKED_EFFECT_LEVEL = 1.0f;

	FGameplayTag AbsorbVerbTag(EWoRAbsorbVerb Verb)
	{
		switch (Verb)
		{
		case EWoRAbsorbVerb::Ward: return WoRTags::Status_Absorb_Ward;
		case EWoRAbsorbVerb::Shield: return WoRTags::Status_Absorb_Shield;
		default: return WoRTags::Status_Absorb_Barrier;
		}
	}

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

namespace
{
	/** Arguments: name part, verb, magnitude, uses ("pool" for a pool), turns, then the Ward's element. */
	constexpr int32 ABSORB_ADD_NAME_ARG = 0;
	constexpr int32 ABSORB_ADD_VERB_ARG = 1;
	constexpr int32 ABSORB_ADD_MAGNITUDE_ARG = 2;
	constexpr int32 ABSORB_ADD_USES_ARG = 3;
	constexpr int32 ABSORB_ADD_TURNS_ARG = 4;
	constexpr int32 ABSORB_ADD_ELEMENT_ARG = 5;
	constexpr int32 ABSORB_ADD_MIN_ARGS = ABSORB_ADD_TURNS_ARG + 1;

	FAutoConsoleCommandWithWorldAndArgs GAddAbsorbCmd(
		TEXT("wor.Absorb.Add"),
		TEXT("DEBUG (B-042j): wor.Absorb.Add <NamePart> <Barrier|Shield|Ward> <Magnitude> <Uses (0 = unlimited) | pool> ")
		TEXT("<Turns (0 = until spent)> [Element]. Gives every combatant whose name contains NamePart an absorber."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
		{
			if (Args.Num() < ABSORB_ADD_MIN_ARGS)
			{
				UE_LOG(LogWoREffects, Warning, TEXT("[TurnEffectTracker] wor.Absorb.Add needs at least %d arguments"), ABSORB_ADD_MIN_ARGS);
				return;
			}
			FWoRAbsorbCharge Charge;
			const int64 Verb = StaticEnum<EWoRAbsorbVerb>()->GetValueByNameString(Args[ABSORB_ADD_VERB_ARG]);
			if (Verb == INDEX_NONE)
			{
				UE_LOG(LogWoREffects, Warning, TEXT("[TurnEffectTracker] wor.Absorb.Add: unknown verb %s"), *Args[ABSORB_ADD_VERB_ARG]);
				return;
			}
			Charge.Verb = static_cast<EWoRAbsorbVerb>(Verb);
			Charge.Magnitude = FCString::Atoi(*Args[ABSORB_ADD_MAGNITUDE_ARG]);
			const bool bPool = Args[ABSORB_ADD_USES_ARG].Equals(TEXT("pool"), ESearchCase::IgnoreCase);
			Charge.Mode = bPool ? EWoRChargeMode::Pool : EWoRChargeMode::Uses;
			Charge.MaxUses = bPool ? 0 : FCString::Atoi(*Args[ABSORB_ADD_USES_ARG]);
			const int32 Turns = FCString::Atoi(*Args[ABSORB_ADD_TURNS_ARG]);
			if (Turns < 0 || Charge.MaxUses < 0)
			{
				UE_LOG(LogWoREffects, Warning, TEXT("[TurnEffectTracker] wor.Absorb.Add: uses and turns can't be negative"));
				return;
			}
			if (Args.IsValidIndex(ABSORB_ADD_ELEMENT_ARG))
			{
				const int64 Element = StaticEnum<ESpellElement>()->GetValueByNameString(Args[ABSORB_ADD_ELEMENT_ARG]);
				Charge.Element = Element == INDEX_NONE ? ESpellElement::None : static_cast<ESpellElement>(Element);
			}
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				UWoRTurnEffectTracker *Tracker = It->FindComponentByClass<UWoRTurnEffectTracker>();
				if (Tracker && It->GetName().Contains(Args[ABSORB_ADD_NAME_ARG]))
				{
					Tracker->ApplyAbsorb(Charge, Turns, *It);
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
	Tracked.Reset();
	Absorbs.Reset();
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
	if (Turns < MIN_TRACKED_TURNS)
	{
		UE_LOG(LogWoREffects, Warning, TEXT("[TurnEffectTracker] %s: can't apply %s for %d turn(s)"), *GetNameSafe(GetOwner()),
			   Spec.IsValid() ? *GetNameSafe(Spec.Data->Def.Get()) : TEXT("None"), Turns);
		return FActiveGameplayEffectHandle();
	}
	return ApplySpecForSpan(Spec, FTurnSpan::OfHolderTurns(Turns), TickSpec);
}

FActiveGameplayEffectHandle UWoRTurnEffectTracker::ApplySpecForSpan(const FGameplayEffectSpecHandle &Spec, const FTurnSpan &Span,
																	const FGameplayEffectSpecHandle &TickSpec)
{
	UAbilitySystemComponent *ASC = GetOwnerASC();
	const UGameplayEffect *Def = Spec.IsValid() ? Spec.Data->Def.Get() : nullptr;
	if (!ASC || !Def)
	{
		UE_LOG(LogWoREffects, Warning, TEXT("[TurnEffectTracker] %s: can't apply %s (ASC %s)"),
			   *GetNameSafe(GetOwner()), *GetNameSafe(Def), ASC ? TEXT("ok") : TEXT("missing"));
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
		Tracked.Add(Handle, FTrackedEffect{Span, TickSpec, NextArrival++});
		UE_LOG(LogWoREffects, Log, TEXT("[TurnEffectTracker] %s gains %s for %d holder turn(s)%s"),
			   *GetNameSafe(GetOwner()), *Def->GetName(), Span.TurnsLeft, TickSpec.IsValid() ? TEXT(", ticking") : TEXT(""));
	}
	return Handle;
}

bool UWoRTurnEffectTracker::SetSpan(FActiveGameplayEffectHandle Handle, const FTurnSpan &Span)
{
	FTrackedEffect *Entry = Tracked.Find(Handle);
	if (!Entry)
	{
		return false;
	}
	Entry->Span = Span;
	return true;
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
	// B-042j, D-092 rule 4: a counted, timed absorber refills at the holder's turn start.
	for (TPair<FActiveGameplayEffectHandle, FWoRAbsorbCharge> &Absorb : Absorbs)
	{
		FWoRAbsorbCharge &Charge = Absorb.Value;
		if (Charge.bTimed && Charge.Mode == EWoRChargeMode::Uses && Charge.MaxUses > 0)
		{
			Charge.UsesLeft = Charge.MaxUses;
		}
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
	// B-042y: the game's HP and death state where the holder has them, and Health (a bare ASC, or a GAS
	// tick at UseGAS 0 that only moves the attribute).
	const UCharacterDataComponent *Char = GetOwner() ? GetOwner()->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (Char && Char->CharacterData && (!Char->bIsAlive || Char->CurrentHP <= 0))
	{
		return true;
	}
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
	Absorbs.Remove(Removed.Handle);
}

FActiveGameplayEffectHandle UWoRTurnEffectTracker::ApplyAbsorb(const FWoRAbsorbCharge &Charge, int32 Turns, AActor *Instigator)
{
	UAbilitySystemComponent *ASC = GetOwnerASC();
	if (!ASC || !WoRAbsorb::IsValidCharge(Charge))
	{
		UE_LOG(LogWoREffects, Warning, TEXT("[TurnEffectTracker] %s: absorber %s not applied (ASC %s)"), *GetNameSafe(GetOwner()),
			   *Charge.ToString(), ASC ? TEXT("ok; a Ward needs an element, a magnitude above 0") : TEXT("missing"));
		return FActiveGameplayEffectHandle();
	}
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(Instigator, Instigator);
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UWoRGE_Absorb::StaticClass(), TRACKED_EFFECT_LEVEL, Context);
	if (!Spec.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	Spec.Data->DynamicGrantedTags.AddTag(AbsorbVerbTag(Charge.Verb));

	FWoRAbsorbCharge Added = Charge;
	Added.Fill();
	Added.bTimed = Turns >= MIN_TRACKED_TURNS;
	Added.Arrival = NextAbsorbArrival++;
	BindRemovedEvent();
	// D-092 rule 3: with a duration it ends at whichever comes first; without one it stands until spent.
	const FActiveGameplayEffectHandle Handle =
		Added.bTimed ? ApplySpecForHolderTurns(Spec, Turns) : ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (Handle.IsValid())
	{
		Absorbs.Add(Handle, Added);
		UE_LOG(LogWoREffects, Log, TEXT("[TurnEffectTracker] %s gains %s"), *GetNameSafe(GetOwner()), *Added.ToString());
	}
	return Handle;
}

int32 UWoRTurnEffectTracker::AbsorbHit(int32 Damage, ESpellElement HitElement, EPhysicalDamageType HitPhysical)
{
	if (Damage <= 0 || Absorbs.Num() == 0)
	{
		return 0;
	}
	TArray<FWoRAbsorbCharge *> Charges;
	for (TPair<FActiveGameplayEffectHandle, FWoRAbsorbCharge> &Absorb : Absorbs)
	{
		Charges.Add(&Absorb.Value);
	}
	const int32 Negated = WoRAbsorb::Spend(Charges, Damage, HitElement, HitPhysical);

	TArray<FActiveGameplayEffectHandle> UsedUp;
	for (const TPair<FActiveGameplayEffectHandle, FWoRAbsorbCharge> &Absorb : Absorbs)
	{
		if (Absorb.Value.IsUsedUp())
		{
			UsedUp.Add(Absorb.Key);
		}
	}
	UAbilitySystemComponent *ASC = GetOwnerASC();
	for (const FActiveGameplayEffectHandle &Handle : UsedUp)
	{
		Absorbs.Remove(Handle);
		if (!RemoveTracked(Handle) && ASC)
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}
	if (Negated > 0)
	{
		UE_LOG(LogWoREffects, Log, TEXT("[TurnEffectTracker] %s's absorbers negate %d of %d damage (%d used up)"),
			   *GetNameSafe(GetOwner()), Negated, Damage, UsedUp.Num());
	}
	return Negated;
}

TArray<FWoRAbsorbCharge> UWoRTurnEffectTracker::GetAbsorbs() const
{
	TArray<FWoRAbsorbCharge> Out;
	Absorbs.GenerateValueArray(Out);
	return Out;
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
	for (const TPair<FActiveGameplayEffectHandle, FWoRAbsorbCharge> &Absorb : Absorbs)
	{
		Out += FString::Printf(TEXT("\n  absorber [%s]: %s"), *Absorb.Key.ToString(), *Absorb.Value.ToString());
	}
	return Out;
}

void UWoRTurnEffectTracker::PrintState() const
{
	UE_LOG(LogWoREffects, Log, TEXT("[TurnEffectTracker] %s"), *GetDebugString());
}
