// DefenseSystem.cpp
// Real-time defense system implementation

#include "Combat/Defense/DefenseSystem.h"
#include "Character/CharacterDataComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Combat/CombatAnimInstance.h"
#include "Character/CharacterData.h"
#include "Loadout/LoadoutComponent.h"

void UDefenseSystem::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[DefenseSystem] Initialized"));
}

void UDefenseSystem::Deinitialize()
{
	// Clear all timers
	if (UWorld *World = GetWorld())
	{
		for (auto &Pair : WindowTimerHandles)
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
		}
	}

	ActiveDefenseStates.Empty();
	WindowTimerHandles.Empty();
	Super::Deinitialize();
}

// ========================================
// DEFENSE WINDOW MANAGEMENT
// ========================================

void UDefenseSystem::OpenDefenseWindow(
	AActor *Attacker,
	AActor *Defender,
	float AttackSize,
	int32 BaseDamage,
	float WindowDuration,
	bool bManualClose)
{
	if (!Defender)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefenseSystem] Cannot open window - null defender"));
		return;
	}

	// Close existing window if any
	if (ActiveDefenseStates.Contains(Defender))
	{
		CloseDefenseWindow(Defender);
	}

	// Create new defense state
	FDefenseState State;
	State.Defender = Defender;
	State.Attacker = Attacker;
	State.bWindowOpen = true;
	State.AttackSize = AttackSize;
	State.BaseDamage = BaseDamage;
	State.DefenseChosen = EDefenseType::None;
	State.DodgeDirection = EDefenseDirection::None;
	State.WindowOpenTime = FPlatformTime::Seconds();
	State.WindowDuration = WindowDuration > 0.0f ? WindowDuration : DefaultWindowDuration;
	State.bInputReceived = false;
	State.bCountBasedClose = bManualClose;

	ActiveDefenseStates.Add(Defender, State);

	// Auto-close timer. For manual-close (count-based) windows the last landed hit
	// closes the window externally (ActionExecutor's Hit-notify counter); the timer is
	// armed at MaxWindowDuration as a FAILSAFE so a missing/miscounted hit can't hang
	// the window open forever. For normal windows it stays the closer at the requested
	// duration. State.WindowDuration is unchanged either way.
	const float CloseTimerDuration = bManualClose ? MaxWindowDuration : State.WindowDuration;

	FTimerHandle TimerHandle;
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &UDefenseSystem::OnWindowTimerExpired, Defender);

	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TimerHandle,
			TimerDelegate,
			CloseTimerDuration,
			false);

		WindowTimerHandles.Add(Defender, TimerHandle);
	}

	// Broadcast event for UI
	OnDefenseWindowOpened.Broadcast(Defender, AttackSize, State.WindowDuration);

	UE_LOG(LogTemp, Log, TEXT("[DefenseSystem] Defense window opened for %s (Size: %.1f, Damage: %d, Reaction: %.2fs, Close: %s %.2fs)"),
		   *Defender->GetName(), AttackSize, BaseDamage, State.WindowDuration,
		   bManualClose ? TEXT("count-based, failsafe") : TEXT("timer"), CloseTimerDuration);

	// AI defenders are synthesized per-impact at resolve time
	// (see ActionExecutor::ResolveImpactDefense) — nothing to schedule here.
}

FDefenseResult UDefenseSystem::CloseDefenseWindow(AActor *Defender)
{
	FDefenseResult Result;

	if (!Defender)
	{
		return Result;
	}

	// Get and remove state
	FDefenseState *StatePtr = ActiveDefenseStates.Find(Defender);
	if (!StatePtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefenseSystem] No active defense window for %s"),
			   *Defender->GetName());
		return Result;
	}

	FDefenseState State = *StatePtr;
	ActiveDefenseStates.Remove(Defender);

	// Clear timer
	if (FTimerHandle *TimerHandle = WindowTimerHandles.Find(Defender))
	{
		if (UWorld *World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(*TimerHandle);
		}
		WindowTimerHandles.Remove(Defender);
	}

	// Calculate result (dodge is timing-only — no attack-size/threshold gate)
	Result = CalculateDefenseResult(
		State.BaseDamage,
		State.DefenseChosen,
		State.bInputReceived);

	Result.bWasInWindow = State.bInputReceived;

	// Handle parry reflect — single-decision (non-melee) windows ONLY. Count-based (melee)
	// windows resolve reflect PER IMPACT in ActionExecutor::ResolveImpactDefense (Stage 3);
	// reflecting here too would double-reflect on the legacy lumped first-input decision.
	if (!State.bCountBasedClose && Result.bSuccess && State.DefenseChosen == EDefenseType::Parry && Result.ReflectedDamage > 0)
	{
		if (State.Attacker.IsValid())
		{
			ApplyReflectedDamage(State.Attacker.Get(), Result.ReflectedDamage);
			OnParryReflect.Broadcast(Defender, State.Attacker.Get(), Result.ReflectedDamage);
		}
	}

	// Broadcast event
	OnDefenseWindowClosed.Broadcast(Defender, Result);

	UE_LOG(LogTemp, Log, TEXT("[DefenseSystem] Defense window closed for %s - Type: %d, Success: %s, Damage: %d → %d"),
		   *Defender->GetName(),
		   static_cast<int32>(State.DefenseChosen),
		   Result.bSuccess ? TEXT("Yes") : TEXT("No"),
		   State.BaseDamage,
		   Result.FinalDamage);

	return Result;
}

void UDefenseSystem::SubmitDefenseInput(AActor *Defender, EDefenseType DefenseType, EDefenseDirection Direction, double InputTime)
{
	if (!Defender)
	{
		return;
	}

	FDefenseState *StatePtr = ActiveDefenseStates.Find(Defender);
	if (!StatePtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefenseSystem] No active defense window for %s - input ignored"),
			   *Defender->GetName());
		return;
	}

	if (!StatePtr->bWindowOpen)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefenseSystem] Defense window not open for %s - input ignored"),
			   *Defender->GetName());
		return;
	}

	// Stage 3: NO "already received" reject — multiple presses APPEND to the buffer so each
	// impact can be independently defended. Each press still plays its own anim (Expedition 33
	// feel: you see every input).

	// Validate dodge direction
	if (DefenseType == EDefenseType::Dodge)
	{
		if (Direction != EDefenseDirection::Left && Direction != EDefenseDirection::Right)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DefenseSystem] Dodge requires Left or Right direction"));
			Direction = EDefenseDirection::Left; // Default to left
		}
	}

	// Stage 3: append a timestamped entry — the per-impact match-and-consume reads this.
	FTimestampedDefenseInput Entry;
	Entry.Type = DefenseType;
	Entry.Direction = Direction;
	Entry.InputTime = (InputTime > 0.0) ? InputTime : FPlatformTime::Seconds();
	Entry.bConsumed = false;
	StatePtr->InputBuffer.Add(Entry);

	// Legacy single fields — still read by the non-melee/tail close path. First input wins
	// (mirrors the old reject-2nd behavior) so that path is byte-for-byte unchanged.
	if (!StatePtr->bInputReceived)
	{
		StatePtr->DefenseChosen = DefenseType;
		StatePtr->DodgeDirection = Direction;
		StatePtr->bInputReceived = true;
	}

	// Play defense animation
	PlayDefenseAnimation(Defender, DefenseType, Direction);

	// Broadcast event
	OnDefenseInputReceived.Broadcast(Defender, DefenseType, Direction);

	UE_LOG(LogTemp, Log, TEXT("[DefenseSystem] Defense input buffered: %s chose %d (Direction: %d, BufferSize: %d)"),
		   *Defender->GetName(),
		   static_cast<int32>(DefenseType),
		   static_cast<int32>(Direction),
		   StatePtr->InputBuffer.Num());
}

bool UDefenseSystem::IsDefenseWindowOpen(AActor *Defender) const
{
	if (!Defender)
	{
		return false;
	}

	const FDefenseState *StatePtr = ActiveDefenseStates.Find(Defender);
	return StatePtr && StatePtr->bWindowOpen;
}

AActor *UDefenseSystem::GetActiveDefenderForLocalPlayer() const
{
	// SINGLE-TARGET: first open-window defender that isn't AI-controlled. Multi-target
	// routing (solo sequential / MP simultaneous) is Stage 6 — see RealTimeDefenseRework.md §13.
	for (const TPair<TWeakObjectPtr<AActor>, FDefenseState> &Pair : ActiveDefenseStates)
	{
		if (!Pair.Value.bWindowOpen)
		{
			continue;
		}

		AActor *Defender = Pair.Key.Get();
		if (!Defender)
		{
			continue;
		}

		UCharacterDataComponent *Comp = GetCharacterDataComponent(Defender);
		if (Comp && Comp->CharacterData && !Comp->CharacterData->ShouldUseAI())
		{
			return Defender;
		}
	}

	return nullptr;
}

FDefenseInputMatch UDefenseSystem::MatchAndConsumeInput(AActor *Defender, double ImpactTime, EActionType AttackType,
														const FDefenseDifficultyTriple &Difficulty)
{
	FDefenseInputMatch Match;

	if (!Defender)
	{
		return Match;
	}

	FDefenseState *StatePtr = ActiveDefenseStates.Find(Defender);
	if (!StatePtr)
	{
		return Match;
	}

	// Latest (most recent press) unconsumed entry inside the TWO-SIDED window around the impact.
	// "Latest" = largest InputTime: the most recent press wins; older buffered presses stay
	// unconsumed for an earlier/later impact. EffectiveWindow = the attacker→defender duel (base +
	// defender Reflex − attacker speed, floored): attacker read from the live state, AttackType
	// selects the attacker's speed stat. Hoisted once.
	const float EffectiveWindow = GetEffectiveDefenseInputWindow(Defender, StatePtr->Attacker.Get(), AttackType);

	// Per-type difficulty multiplier (cluster 4): concrete tiers (caller-resolved); all-Inherit guard
	// triple → Easy ×1.0; None/unknown press type never tightens. Shared by the window test AND the
	// perfect band so difficulty scales every timing axis uniformly.
	auto TypeTier = [&Difficulty](EDefenseType Type) -> EDefenseDifficulty
	{
		return (Type == EDefenseType::Parry) ? Difficulty.Parry :
			   (Type == EDefenseType::Dodge) ? Difficulty.Dodge :
			   (Type == EDefenseType::Block) ? Difficulty.Block :
											   EDefenseDifficulty::Easy;
	};
	// TRIPWIRE (band-parity): the AI mirror UAIDecisionManager::CalculateDefenseDelta
	// (AIDecisionManager.cpp) reads the SAME per-type field and the SAME Inherit/None -> Easy x1.0
	// terminal as this lambda, so the AI aims at the band this matcher judges against. If real
	// Inherit resolution is ever added here, mirror it on the AI side too — otherwise the bands
	// diverge and Expert AI stops landing PERFECT.
	auto TypeMult = [&TypeTier](EDefenseType Type) -> float
	{
		return DefenseDifficultyMultiplier(TypeTier(Type));
	};

	int32 BestIndex = INDEX_NONE;
	double BestInputTime = TNumericLimits<double>::Lowest();
	for (int32 i = 0; i < StatePtr->InputBuffer.Num(); ++i)
	{
		const FTimestampedDefenseInput &Entry = StatePtr->InputBuffer[i];
		if (Entry.bConsumed)
		{
			continue;
		}

		// Two-sided window (Phase 1), BOTH sides scaled by the same per-type difficulty Mult.
		// Delta = ImpactTime − InputTime: Delta ≥ 0 = pressed BEFORE impact (anticipation, up to
		// BeforeWindow = the lead-in × Mult, re-floored at MINIMUM_DEFENSE_WINDOW); Delta < 0 = pressed
		// AFTER impact (late grace, up to AfterWindow = DEFENSE_AFTER_GRACE_SECONDS × Mult, smaller than
		// the lead-in so anticipation stays primary). Outside [−AfterWindow, +BeforeWindow] → whiff.
		const EDefenseDifficulty Tier = TypeTier(Entry.Type);
		const float Mult = DefenseDifficultyMultiplier(Tier);
		// Impossible floors at its OWN tiny floor (below the normal 0.1s min) so the window can go
		// tiny-but-nonzero; every other tier floors at MINIMUM_DEFENSE_WINDOW. After-grace + perfect
		// band stay unfloored — the ×0.1 Mult already shrinks them.
		const float Floor = (Tier == EDefenseDifficulty::Impossible)
								? CombatConstants::IMPOSSIBLE_WINDOW_FLOOR
								: CombatConstants::MINIMUM_DEFENSE_WINDOW;
		const float BeforeWindow = FMath::Max(Floor, EffectiveWindow * Mult);
		const float AfterWindow = CombatConstants::DEFENSE_AFTER_GRACE_SECONDS * Mult;

		const double Delta = ImpactTime - Entry.InputTime;
		if (Delta > BeforeWindow || Delta < -static_cast<double>(AfterWindow))
		{
			continue; // outside both the lead-in and the after-grace → whiff
		}

		if (Entry.InputTime > BestInputTime)
		{
			BestInputTime = Entry.InputTime;
			BestIndex = i;
		}
	}

	if (BestIndex == INDEX_NONE)
	{
		return Match; // bMatched stays false — this impact is undefended
	}

	FTimestampedDefenseInput &Hit = StatePtr->InputBuffer[BestIndex];
	Hit.bConsumed = true;

	const double Delta = ImpactTime - Hit.InputTime;
	Match.bMatched = true;
	Match.Type = Hit.Type;
	Match.Direction = Hit.Direction;

	// Perfect band (Phase 1): TWO-SIDED (dead-on either side of the impact, hence Abs(Delta)) and
	// scaled by the SAME per-type difficulty Mult as the success windows — Hard tightens the perfect
	// band exactly as it tightens the match windows. A press exactly at impact (Delta = 0) is always perfect.
	const float PerfectBand = PerfectThreshold * TypeMult(Hit.Type);
	Match.bPerfect = (FMath::Abs(Delta) <= static_cast<double>(PerfectBand));

	UE_LOG(LogTemp, Log, TEXT("[DefenseSystem] Impact matched for %s — Type: %d, Delta: %.3fs, %s"),
		   *Defender->GetName(), static_cast<int32>(Match.Type), Delta,
		   Match.bPerfect ? TEXT("PERFECT") : TEXT("normal"));

	return Match;
}

void UDefenseSystem::ApplyParryReflect(AActor *Attacker, AActor *Defender, int32 ReflectedDamage)
{
	if (!Attacker || ReflectedDamage <= 0)
	{
		return;
	}

	ApplyReflectedDamage(Attacker, ReflectedDamage);
	OnParryReflect.Broadcast(Defender, Attacker, ReflectedDamage);
}

FDefenseState UDefenseSystem::GetDefenseState(AActor *Defender) const
{
	if (!Defender)
	{
		return FDefenseState();
	}

	const FDefenseState *StatePtr = ActiveDefenseStates.Find(Defender);
	return StatePtr ? *StatePtr : FDefenseState();
}

float UDefenseSystem::GetRemainingWindowTime(AActor *Defender) const
{
	if (!Defender)
	{
		return 0.0f;
	}

	const FDefenseState *StatePtr = ActiveDefenseStates.Find(Defender);
	if (!StatePtr || !StatePtr->bWindowOpen)
	{
		return 0.0f;
	}

	double Elapsed = FPlatformTime::Seconds() - StatePtr->WindowOpenTime;
	float Remaining = StatePtr->WindowDuration - static_cast<float>(Elapsed);
	return FMath::Max(0.0f, Remaining);
}

float UDefenseSystem::GetEffectiveDefenseInputWindow(AActor *Defender, AActor *Attacker, EActionType AttackType) const
{
	// Attacker→defender duel: base lead-in window (tuned on the subsystem) WIDENED by the
	// defender's Reflex and NARROWED by the attacker's speed, floored at MINIMUM_DEFENSE_WINDOW.
	//   window = max(MINIMUM_DEFENSE_WINDOW, base + defenderReflexBonus − attackerSpeedPenalty)
	// Each per-character term is a STAT (raw, ≤0.25-capped) MULTIPLIED by that side's gear/buff factor
	// (pillar + matched Bonus substat + matched stone + transient), bounded by WINDOW_GEAR_CEILING_SECONDS
	// — matched gear pushes past the stat cap (defender widens, attacker narrows); inert (×1) with no gear.
	// Null-safe on BOTH sides: a missing component / CharacterData simply drops that side's term.
	float Window = DefenseInputWindow;

	// Defender side — Reflex widens. Pattern P (cluster B-5): the STAT bonus (raw, ≤0.25-capped) is
	// MULTIPLIED by the defender's Reflex-gear factor (Body pillar + BonusReflex + ReflexStone + transient
	// ReflexBuff/Debuff), so matched Reflex gear widens the window past the stat cap, bounded by
	// WINDOW_GEAR_CEILING_SECONDS. Inert (×1) with no Reflex gear — byte-identical to the pre-B-5 stat term.
	UCharacterDataComponent *DefComp = GetCharacterDataComponent(Defender);
	if (DefComp && DefComp->CharacterData)
	{
		float ReflexTerm = DefComp->CharacterData->CalculateReflexWindowBonus();
		ReflexTerm *= DefComp->ReflexWindowGearFactor();
		Window += FMath::Min(ReflexTerm, CombatConstants::WINDOW_GEAR_CEILING_SECONDS);
	}

	// Attacker side — speed narrows (type-aware: physical → ActionSpeed/Body, spell → SpellSpeed/Mind).
	// NOTE: the spell branch is DORMANT until Stage 6 — per-impact resolution is melee-only today.
	// Pattern P (cluster A1): the STAT penalty (raw, ±0.25-capped) is MULTIPLIED by the attacker's
	// speed-gear factor (pillar + Bonus{Action,Spell}Speed + stone + transient), so matched gear narrows
	// the window past the stat cap, bounded by WINDOW_GEAR_CEILING_SECONDS. Inert (×1) with no gear.
	UCharacterDataComponent *AtkComp = GetCharacterDataComponent(Attacker);
	if (AtkComp && AtkComp->CharacterData)
	{
		float SpeedTerm = AtkComp->CharacterData->CalculateSpeedWindowPenalty(AttackType);
		SpeedTerm *= AtkComp->SpeedWindowGearFactor(AttackType);
		Window -= FMath::Min(SpeedTerm, CombatConstants::WINDOW_GEAR_CEILING_SECONDS);
	}

	return FMath::Max(CombatConstants::MINIMUM_DEFENSE_WINDOW, Window);
}

// ========================================
// DAMAGE CALCULATION
// ========================================

FDefenseResult UDefenseSystem::CalculateDefenseResult(
	int32 BaseDamage,
	EDefenseType DefenseType,
	bool bDefenseSuccessful)
{
	FDefenseResult Result;
	Result.DefenseType = DefenseType;
	Result.FinalDamage = BaseDamage;
	Result.ReflectedDamage = 0;
	Result.bSuccess = false;

	// No defense = full damage
	if (DefenseType == EDefenseType::None || !bDefenseSuccessful)
	{
		Result.FailureReason = TEXT("No defense or missed timing");
		return Result;
	}

	switch (DefenseType)
	{
	case EDefenseType::Block:
		// Block: partial damage reduction (BlockReduction)
		Result.bSuccess = true;
		Result.FinalDamage = FMath::RoundToInt(BaseDamage * (1.0f - BlockReduction));
		break;

	case EDefenseType::Parry:
		// Parry: damage reduction (ParryReduction) + reflect (ParryReflect)
		Result.bSuccess = true;
		Result.FinalDamage = FMath::RoundToInt(BaseDamage * (1.0f - ParryReduction));
		Result.ReflectedDamage = FMath::RoundToInt(BaseDamage * ParryReflect);
		break;

	case EDefenseType::Dodge:
		// Dodge: 100% avoidance on TIMING alone — the attack-size gate was removed, so any
		// well-timed dodge fully avoids regardless of attack size (player + AI, uniform).
		Result.bSuccess = true;
		Result.FinalDamage = 0;
		break;

	default:
		break;
	}

	return Result;
}

void UDefenseSystem::PlayDefenseAnimation(AActor *Defender, EDefenseType DefenseType, EDefenseDirection Direction)
{
	if (!Defender)
	{
		return;
	}

	// Get LoadoutComponent for animation references
	ULoadoutComponent *LoadoutComp = Defender->FindComponentByClass<ULoadoutComponent>();
	if (!LoadoutComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefenseSystem] No LoadoutComponent for %s - cannot play defense animation"),
			   *Defender->GetName());
		return;
	}

	UAnimMontage *MontageToPlay = nullptr;
	float PlayRate = 1.0f;

	switch (DefenseType)
	{
	case EDefenseType::Block:
		MontageToPlay = LoadoutComp->GetBlockMontage();
		break;

	case EDefenseType::Parry:
		MontageToPlay = LoadoutComp->GetParryMontage();
		break;

	case EDefenseType::Dodge:
		if (Direction == EDefenseDirection::Left)
		{
			MontageToPlay = LoadoutComp->GetDodgeLeftMontage();
		}
		else
		{
			MontageToPlay = LoadoutComp->GetDodgeRightMontage();
		}
		break;

	default:
		return;
	}

	if (!MontageToPlay)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefenseSystem] No montage for %s defense type %d"),
			   *Defender->GetName(), static_cast<int32>(DefenseType));
		return;
	}

	// Get CombatAnimInstance and play
	ACharacter *Character = Cast<ACharacter>(Defender);
	if (!Character)
	{
		return;
	}

	UCombatAnimInstance *CombatAnim = Cast<UCombatAnimInstance>(Character->GetMesh()->GetAnimInstance());
	if (CombatAnim)
	{
		// Use PlayRate for direction (negative = reversed)
		CombatAnim->PlayActionMontage(MontageToPlay, PlayRate);
		UE_LOG(LogTemp, Log, TEXT("[DefenseSystem] Playing %s for %s (Rate: %.1f)"),
			   *MontageToPlay->GetName(), *Defender->GetName(), PlayRate);
	}
}

// ========================================
// INTERNAL METHODS
// ========================================

void UDefenseSystem::OnWindowTimerExpired(AActor *Defender)
{
	if (!Defender)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DefenseSystem] Defense window timer expired for %s"),
		   *Defender->GetName());

	// Close window and apply result
	CloseDefenseWindow(Defender);
}

void UDefenseSystem::ApplyReflectedDamage(AActor *Attacker, int32 Damage)
{
	if (!Attacker || Damage <= 0)
	{
		return;
	}

	UCharacterDataComponent *Comp = GetCharacterDataComponent(Attacker);
	if (Comp)
	{
		Comp->ServerTakeDamage(Damage);
		UE_LOG(LogTemp, Log, TEXT("[DefenseSystem] Applied %d reflected damage to %s"),
			   Damage, *Attacker->GetName());
	}
}

UCharacterDataComponent *UDefenseSystem::GetCharacterDataComponent(AActor *Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}

	return Actor->FindComponentByClass<UCharacterDataComponent>();
}
