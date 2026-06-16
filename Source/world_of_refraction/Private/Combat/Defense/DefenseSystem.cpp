// DefenseSystem.cpp
// Real-time defense system implementation

#include "Combat/Defense/DefenseSystem.h"
#include "Character/CharacterDataComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Combat/CombatAnimInstance.h"
#include "AI/AIDecisionManager.h"
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
	// duration. State.WindowDuration is unchanged either way — it remains the AI
	// reaction-delay seed passed to ScheduleDefenseDecision below.
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

	// Check if AI-controlled and schedule defense
	UCharacterDataComponent *CharComp = Defender->FindComponentByClass<UCharacterDataComponent>();
	if (!CharComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefenseSystem] No CharacterDataComponent on %s"), *Defender->GetName());
	}
	else if (!CharComp->CharacterData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefenseSystem] CharacterData is null on %s"), *Defender->GetName());
	}
	else if (!CharComp->CharacterData->ShouldUseAI())
	{
		UE_LOG(LogTemp, Log, TEXT("[DefenseSystem] %s is not AI-controlled, skipping AI defense"), *Defender->GetName());
	}
	else
	{
		UAIDecisionManager *AIManager = GetGameInstance()->GetSubsystem<UAIDecisionManager>();
		if (!AIManager)
		{
			UE_LOG(LogTemp, Error, TEXT("[DefenseSystem] Failed to get AIDecisionManager!"));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[DefenseSystem] Scheduling AI defense for %s"), *Defender->GetName());
			AIManager->ScheduleDefenseDecision(Defender, AttackSize, BaseDamage, State.WindowDuration);
		}
	}
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

	// Calculate result
	float DodgeThreshold = GetDodgeThreshold(Defender);
	Result = CalculateDefenseResult(
		State.BaseDamage,
		State.DefenseChosen,
		State.bInputReceived,
		State.AttackSize,
		DodgeThreshold);

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

void UDefenseSystem::SubmitDefenseInput(AActor *Defender, EDefenseType DefenseType, EDefenseDirection Direction)
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
	Entry.InputTime = FPlatformTime::Seconds();
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

	// Latest (most recent press) unconsumed entry with delta in [0, scaledWindow].
	// "Latest" = largest InputTime: a press right before the impact beats an older buffered
	// press, which stays unconsumed for an earlier/later impact. EffectiveWindow = the
	// attacker→defender duel (base + defender Reflex − attacker speed, floored): attacker read
	// from the live state, AttackType selects the attacker's speed stat. Hoisted once.
	const float EffectiveWindow = GetEffectiveDefenseInputWindow(Defender, StatePtr->Attacker.Get(), AttackType);
	int32 BestIndex = INDEX_NONE;
	double BestInputTime = TNumericLimits<double>::Lowest();
	for (int32 i = 0; i < StatePtr->InputBuffer.Num(); ++i)
	{
		const FTimestampedDefenseInput &Entry = StatePtr->InputBuffer[i];
		if (Entry.bConsumed)
		{
			continue;
		}

		// Window = duel × DIFFICULTY for THIS press's DefenseType (cluster 4). Easy ×1.0 = unchanged;
		// Medium/Hard tighten. The triple's tiers are concrete (caller-resolved); an all-Inherit guard
		// triple → Easy ×1.0. None/unknown press type never tightens. Re-floor at MINIMUM_DEFENSE_WINDOW
		// AFTER the scale, so a Hard window tightens toward but not below the floor.
		const EDefenseDifficulty Tier =
			(Entry.Type == EDefenseType::Parry) ? Difficulty.Parry :
			(Entry.Type == EDefenseType::Dodge) ? Difficulty.Dodge :
			(Entry.Type == EDefenseType::Block) ? Difficulty.Block :
												  EDefenseDifficulty::Easy;
		const float ScaledWindow = FMath::Max(
			CombatConstants::MINIMUM_DEFENSE_WINDOW,
			EffectiveWindow * DefenseDifficultyMultiplier(Tier));

		const double Delta = ImpactTime - Entry.InputTime;
		if (Delta < 0.0 || Delta > ScaledWindow)
		{
			continue; // pressed after this impact, or too early / outside the difficulty-scaled window
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
	Match.bPerfect = (Delta <= PerfectThreshold);

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

// ========================================
// DODGE CALCULATIONS
// ========================================

bool UDefenseSystem::CanDodgeAttack(AActor *Defender, float AttackSize) const
{
	float Threshold = GetDodgeThreshold(Defender);
	return AttackSize < Threshold;
}

float UDefenseSystem::GetDodgeThreshold(AActor *Defender) const
{
	// Base threshold
	float Threshold = BaseDodgeThreshold;

	// TODO: Could be modified by:
	// - Character stats (agility/speed)
	// - Equipment bonuses
	// - Status effects (slowed reduces threshold)
	// - Character size

	return Threshold;
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
	bool bDefenseSuccessful,
	float AttackSize,
	float DodgeThreshold)
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
		// Dodge: 100% avoidance IF attack is small enough
		if (AttackSize < DodgeThreshold)
		{
			Result.bSuccess = true;
			Result.FinalDamage = 0;
		}
		else
		{
			// Attack too big, dodge fails completely
			Result.bSuccess = false;
			Result.FinalDamage = BaseDamage;
			Result.FailureReason = FString::Printf(
				TEXT("Attack too large (%.1f >= %.1f threshold)"),
				AttackSize, DodgeThreshold);
		}
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
